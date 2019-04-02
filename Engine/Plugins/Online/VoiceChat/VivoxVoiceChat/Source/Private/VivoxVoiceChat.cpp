// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VivoxVoiceChat.h"

#include "Async/Async.h"
#include "Logging/LogMacros.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"

#include "Vxc.h"
#include "VxcErrors.h"

DEFINE_LOG_CATEGORY(LogVivoxVoiceChat);

namespace
{
static const FVoiceChatResult ResultSuccess = { true, 0, TEXT("") };

FString LexToString(VivoxClientApi::IClientApiEventHandler::ParticipantLeftReason Reason)
{
	switch (Reason)
	{
	case VivoxClientApi::IClientApiEventHandler::ReasonLeft:	return TEXT("Left");
	case VivoxClientApi::IClientApiEventHandler::ReasonNetwork:	return TEXT("Network");
	case VivoxClientApi::IClientApiEventHandler::ReasonKicked:	return TEXT("Kicked");
	case VivoxClientApi::IClientApiEventHandler::ReasonBanned:	return TEXT("Banned");
	default:													return TEXT("Unknown");
	}
}

FString LexToString(const FVoiceChatResult& Result)
{
	if (Result.bSuccess)
	{
		return TEXT("Success");
	}
	else if (!Result.Error.IsEmpty())
	{
		return FString::Printf(TEXT("Failed: %s"), *Result.Error);
	}
	else
	{
		return FString::Printf(TEXT("Failed: Error %i"), Result.ErrorCode);
	}
}

FVoiceChatResult ResultFromVivoxStatus(const VivoxClientApi::VCSStatus& Status)
{
	FString ErrorString = ANSI_TO_TCHAR(Status.ToString());
	if (ErrorString.IsEmpty() && Status.IsError())
	{
		ErrorString = FString::Printf(TEXT("Error %i"), Status.GetStatusCode());
	}
	return { !Status.IsError(), Status.GetStatusCode(), ErrorString };
}

FVoiceChatResult ResultFromErrorString(const FString& Error, int ErrorCode = -1)
{
	return { false, ErrorCode, Error };
}

template<class TDelegate, class... TArgs>
void TriggerCompletionDelegates(TArray<TDelegate>& InOutDelegates, const TArgs&... Args)
{
	TArray<TDelegate> Delegates = MoveTemp(InOutDelegates);
	InOutDelegates.Reset();

	for (TDelegate& Delegate : Delegates)
	{
		Delegate.ExecuteIfBound(Args...);
	}
}

template<class TDelegate, class... TArgs>
void TriggerCompletionDelegate(TDelegate& InOutDelegate, const TArgs&... Args)
{
	TDelegate Delegate = InOutDelegate;
	InOutDelegate.Unbind();
	Delegate.ExecuteIfBound(Args...);
}

VivoxClientApi::Vector ToVivoxVector(const FVector& Vec)
{
	return { Vec.Y, Vec.Z, -Vec.X };
}

bool VivoxNameContainsValidCharacters(const FString& Name)
{
	// Must contain characters chosen only from letters a-z and A-Z, numbers 0-9, and the following characters: =+-_.!~()%
	static const FString AdditionalValidCharacters = TEXT("=+-_.!~()%");
	for (const TCHAR& Char : Name)
	{
		int32 Index;
		if (FChar::IsAlnum(Char) || AdditionalValidCharacters.FindChar(Char, Index))
		{
			continue;
		}

		return false;
	}

	return true;
}

}

FVivoxVoiceChat::FVivoxVoiceChat()
	: bInitialized(false)
	, ConnectionState(EConnectionState::Disconnected)
{
	SetClientConnection(&VivoxClientConnection);
	SetAbortEnabled(false);
}

FVivoxVoiceChat::~FVivoxVoiceChat()
{
}

static void* VivoxMalloc(size_t bytes)
{
	return FMemory::Malloc(bytes);
}

static void VivoxFree(void* ptr)
{
	FMemory::Free(ptr);
}

static void* VivoxRealloc(void* ptr, size_t bytes)
{
	return FMemory::Realloc(ptr, bytes);
}

static void* VivoxCalloc(size_t num, size_t bytes)
{
	return FMemory::Malloc(bytes*num);
}

static void* VivoxMallocAligned(size_t alignment, size_t bytes)
{
	return FMemory::Malloc(bytes, alignment);
}

static void VivoxFreeAligned(void* ptr)
{
	FMemory::Free(ptr);
}

bool FVivoxVoiceChat::Initialize()
{
	if (!IsInitialized())
	{
		bool bEnabled = true;
		GConfig->GetBool(TEXT("VoiceChat.Vivox"), TEXT("bEnabled"), bEnabled, GEngineIni);
		if (bEnabled)
		{
			GConfig->GetString(TEXT("VoiceChat.Vivox"), TEXT("ServerUrl"), VivoxServerUrl, GEngineIni);
			GConfig->GetString(TEXT("VoiceChat.Vivox"), TEXT("Domain"), VivoxDomain, GEngineIni);
			GConfig->GetString(TEXT("VoiceChat.Vivox"), TEXT("Issuer"), VivoxIssuer, GEngineIni);
			GConfig->GetString(TEXT("VoiceChat.Vivox"), TEXT("Namespace"), VivoxNamespace, GEngineIni);
			GConfig->GetString(TEXT("VoiceChat.Vivox"), TEXT("InsecureSecret"), VivoxInsecureSecret, GEngineIni);

			if (VivoxNamespace.IsEmpty())
			{
				VivoxNamespace = VivoxIssuer;
			}

			// positional audio settings
			AttenuationModel = EVoiceChatAttenuationModel::InverseByDistance;
			MinDistance = 100;
			MaxDistance = 3000;
			Rolloff = 1.0f;
			FString AttenuationModelString;
			if (GConfig->GetString(TEXT("VoiceChat.Vivox"), TEXT("AttenuationModel"), AttenuationModelString, GEngineIni) && !AttenuationModelString.IsEmpty())
			{
				if (AttenuationModelString == TEXT("None"))
				{
					AttenuationModel = EVoiceChatAttenuationModel::None;
				}
				else if (AttenuationModelString == TEXT("InverseByDistance"))
				{
					AttenuationModel = EVoiceChatAttenuationModel::InverseByDistance;
				}
				else if (AttenuationModelString == TEXT("LinearByDistance"))
				{
					AttenuationModel = EVoiceChatAttenuationModel::LinearByDistance;
				}
				else if (AttenuationModelString == TEXT("ExponentialByDistance"))
				{
					AttenuationModel = EVoiceChatAttenuationModel::ExponentialByDistance;
				}
				else
				{
					UE_LOG(LogVivoxVoiceChat, Warning, TEXT("Unknown AttenuationModel: %s"), *AttenuationModelString);
				}
			}
			GConfig->GetInt(TEXT("VoiceChat.Vivox"), TEXT("MinDistance"), MinDistance, GEngineIni);
			GConfig->GetInt(TEXT("VoiceChat.Vivox"), TEXT("MaxDistance"), MaxDistance, GEngineIni);
			GConfig->GetFloat(TEXT("VoiceChat.Vivox"), TEXT("Rolloff"), Rolloff, GEngineIni);

			const char* VivoxVersionInfo = vx_get_sdk_version_info();
			UE_LOG(LogVivoxVoiceChat, Log, TEXT("Initializing Vivox %s"), ANSI_TO_TCHAR(VivoxVersionInfo));

			vx_sdk_config_t ConfigHints;
			int Result = vx_get_default_config3(&ConfigHints, sizeof(ConfigHints));
			if (Result != 0)
			{
				UE_LOG(LogVivoxVoiceChat, Warning, TEXT("Failed to get default config: error:%s (%i)"), ANSI_TO_TCHAR(VivoxClientApi::GetErrorString(Result)), Result);
			}
			else
			{
				SetVivoxSdkConfigHints(ConfigHints);

				LogLevel VivoxLogLevel = LogLevelWarning;
				FString LogLevelString;
				if (GConfig->GetString(TEXT("VoiceChat.Vivox"), TEXT("LogLevel"), LogLevelString, GEngineIni))
				{
					if (LogLevelString == TEXT("None"))
					{
						VivoxLogLevel = LogLevelNone;
					}
					else if (LogLevelString == TEXT("Error"))
					{
						VivoxLogLevel = LogLevelError;
					}
					else if (LogLevelString == TEXT("Warning"))
					{
						VivoxLogLevel = LogLevelWarning;
					}
					else if (LogLevelString == TEXT("Info"))
					{
						VivoxLogLevel = LogLevelInfo;
					}
					else if (LogLevelString == TEXT("Debug"))
					{
						VivoxLogLevel = LogLevelDebug;
					}
					else if (LogLevelString == TEXT("Trace"))
					{
						VivoxLogLevel = LogLevelTrace;
					}
				}

				VivoxClientApi::VCSStatus Status = VivoxClientConnection.Initialize(this, VivoxLogLevel, true, false, &ConfigHints, sizeof(ConfigHints));
				if (Status.IsError())
				{
					UE_LOG(LogVivoxVoiceChat, Warning, TEXT("Initialize failed: error:%s (%i)"), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
				}
				else
				{
					bInitialized = true;

					bool bVADAutomaticParameterSelection = true;
					GConfig->GetBool(TEXT("VoiceChat.Vivox"), TEXT("bVADAutomaticParameterSelection"), bVADAutomaticParameterSelection, GEngineIni);
					VivoxClientConnection.SetVADAutomaticParameterSelection(bVADAutomaticParameterSelection);
				}
			}
		}
	}

	return IsInitialized();
}

bool FVivoxVoiceChat::Uninitialize()
{
	if (IsInitialized())
	{
		VivoxClientConnection.Uninitialize();
		bInitialized = false;
	}

	return true;
}

bool FVivoxVoiceChat::IsInitialized() const
{
	return bInitialized;
}

void FVivoxVoiceChat::SetAudioInputVolume(float InVolume)
{
	UE_LOG(LogVivoxVoiceChat, Verbose, TEXT("SetAudioInputVolume %f"), InVolume);

	VivoxClientConnection.SetAudioInputDeviceMuted(InVolume <= SMALL_NUMBER);
	VivoxClientConnection.SetMasterAudioInputDeviceVolume(FMath::Lerp(VIVOX_MIN_VOL, VIVOX_MAX_VOL, InVolume));
}

void FVivoxVoiceChat::SetAudioOutputVolume(float InVolume)
{
	UE_LOG(LogVivoxVoiceChat, Verbose, TEXT("SetAudioOutputVolume %f"), InVolume);

	VivoxClientConnection.SetAudioOutputDeviceMuted(InVolume <= SMALL_NUMBER);
	VivoxClientConnection.SetMasterAudioOutputDeviceVolume(FMath::Lerp(VIVOX_MIN_VOL, VIVOX_MAX_VOL, InVolume));
}

float FVivoxVoiceChat::GetAudioInputVolume() const
{
	return VivoxClientConnection.GetMasterAudioInputDeviceVolume();
}

float FVivoxVoiceChat::GetAudioOutputVolume() const
{
	return VivoxClientConnection.GetMasterAudioOutputDeviceVolume();
}

void FVivoxVoiceChat::SetAudioInputDeviceMuted(bool bIsMuted)
{
	UE_LOG(LogVivoxVoiceChat, Verbose, TEXT("SetAudioInputDeviceMuted %s"), *LexToString(bIsMuted));

	VivoxClientConnection.SetAudioInputDeviceMuted(bIsMuted);
}

void FVivoxVoiceChat::SetAudioOutputDeviceMuted(bool bIsMuted)
{
	UE_LOG(LogVivoxVoiceChat, Verbose, TEXT("SetAudioOutputDeviceMuted %s"), *LexToString(bIsMuted));

	VivoxClientConnection.SetAudioOutputDeviceMuted(bIsMuted);
}

bool FVivoxVoiceChat::GetAudioInputDeviceMuted() const
{
	return VivoxClientConnection.GetAudioInputDeviceMuted();
}

bool FVivoxVoiceChat::GetAudioOutputDeviceMuted() const
{
	return VivoxClientConnection.GetAudioOutputDeviceMuted();
}

TArray<FString> FVivoxVoiceChat::GetAvailableInputDevices() const
{
	TArray<FString> InputDevices;
	const std::vector<VivoxClientApi::AudioDeviceId>& AudioInputDevices = VivoxClientConnection.GetAvailableAudioInputDevices();
	InputDevices.Reserve(AudioInputDevices.size());
	for (const VivoxClientApi::AudioDeviceId& DeviceId : AudioInputDevices)
	{
		InputDevices.Add(UTF8_TO_TCHAR(DeviceId.GetAudioDeviceDisplayName().c_str()));
	}
	return InputDevices;
}

TArray<FString> FVivoxVoiceChat::GetAvailableOutputDevices() const
{
	TArray<FString> OutputDevices;
	const std::vector<VivoxClientApi::AudioDeviceId>& AudioOutputDevices = VivoxClientConnection.GetAvailableAudioOutputDevices();
	OutputDevices.Reserve(AudioOutputDevices.size());
	for (const VivoxClientApi::AudioDeviceId& DeviceId : AudioOutputDevices)
	{
		OutputDevices.Add(UTF8_TO_TCHAR(DeviceId.GetAudioDeviceDisplayName().c_str()));
	}
	return OutputDevices;
}

void FVivoxVoiceChat::SetInputDevice(const FString& InputDevice)
{
	UE_LOG(LogVivoxVoiceChat, Verbose, TEXT("SetInputDevice %s"), *InputDevice);

	if (!InputDevice.IsEmpty())
	{
		const std::vector<VivoxClientApi::AudioDeviceId>& AudioInputDevices = VivoxClientConnection.GetAvailableAudioInputDevices();
		for (const VivoxClientApi::AudioDeviceId& DeviceId : AudioInputDevices)
		{
			if (InputDevice == UTF8_TO_TCHAR(DeviceId.GetAudioDeviceDisplayName().c_str()))
			{
				VivoxClientConnection.SetApplicationChosenAudioInputDevice(DeviceId);
				return;
			}
		}
	}

	VivoxClientConnection.UseOperatingSystemChosenAudioInputDevice();
}

void FVivoxVoiceChat::SetOutputDevice(const FString& OutputDevice)
{
	UE_LOG(LogVivoxVoiceChat, Verbose, TEXT("SetOutputDevice %s"), *OutputDevice);

	if (!OutputDevice.IsEmpty())
	{
		const std::vector<VivoxClientApi::AudioDeviceId>& AudioOutputDevices = VivoxClientConnection.GetAvailableAudioOutputDevices();
		for (const VivoxClientApi::AudioDeviceId& DeviceId : AudioOutputDevices)
		{
			if (OutputDevice == UTF8_TO_TCHAR(DeviceId.GetAudioDeviceDisplayName().c_str()))
			{
				VivoxClientConnection.SetApplicationChosenAudioOutputDevice(DeviceId);
				return;
			}
		}
	}

	VivoxClientConnection.UseOperatingSystemChosenAudioOutputDevice();
}

FString FVivoxVoiceChat::GetInputDevice() const
{
	if (VivoxClientConnection.IsUsingOperatingSystemChosenAudioInputDevice())
	{
		VivoxClientApi::AudioDeviceId DeviceId = VivoxClientConnection.GetOperatingSystemChosenAudioInputDevice();
		return UTF8_TO_TCHAR(DeviceId.GetAudioDeviceDisplayName().c_str());
	}
	else
	{
		VivoxClientApi::AudioDeviceId DeviceId = VivoxClientConnection.GetApplicationChosenAudioInputDevice();
		return UTF8_TO_TCHAR(DeviceId.GetAudioDeviceDisplayName().c_str());
	}
}

FString FVivoxVoiceChat::GetOutputDevice() const
{
	if (VivoxClientConnection.IsUsingOperatingSystemChosenAudioOutputDevice())
	{
		VivoxClientApi::AudioDeviceId DeviceId = VivoxClientConnection.GetOperatingSystemChosenAudioOutputDevice();
		return UTF8_TO_TCHAR(DeviceId.GetAudioDeviceDisplayName().c_str());
	}
	else
	{
		VivoxClientApi::AudioDeviceId DeviceId = VivoxClientConnection.GetApplicationChosenAudioOutputDevice();
		return UTF8_TO_TCHAR(DeviceId.GetAudioDeviceDisplayName().c_str());
	}
}

FString FVivoxVoiceChat::GetDefaultInputDevice() const
{
	VivoxClientApi::AudioDeviceId DeviceId = VivoxClientConnection.GetOperatingSystemChosenAudioInputDevice();
	return UTF8_TO_TCHAR(DeviceId.GetAudioDeviceDisplayName().c_str());
}

FString FVivoxVoiceChat::GetDefaultOutputDevice() const
{
	VivoxClientApi::AudioDeviceId DeviceId = VivoxClientConnection.GetOperatingSystemChosenAudioOutputDevice();
	return UTF8_TO_TCHAR(DeviceId.GetAudioDeviceDisplayName().c_str());
}

void FVivoxVoiceChat::Connect(const FOnVoiceChatConnectCompleteDelegate& Delegate)
{
	FVoiceChatResult Result = ResultSuccess;

	if (!IsInitialized())
	{
		Result = ResultFromErrorString(TEXT("Not initialized"), VX_E_NOT_INITIALIZED);
	}
	else if (ConnectionState == EConnectionState::Disconnecting)
	{
		Result = ResultFromErrorString("Disconnect in progress");
	}

	if (!Result.bSuccess)
	{
		UE_LOG(LogVivoxVoiceChat, Warning, TEXT("Connect Failed: %s"), *Result.Error);

		Delegate.ExecuteIfBound(Result);
	}
	else if (IsConnected())
	{
		Delegate.ExecuteIfBound(ResultSuccess);
	}
	else
	{
		OnVoiceChatConnectCompleteDelegates.Add(Delegate);

		if (!IsConnecting())
		{
			if (VivoxServerUrl.IsEmpty() || VivoxDomain.IsEmpty() || VivoxNamespace.IsEmpty())
			{
				UE_LOG(LogVivoxVoiceChat, Warning, TEXT("[VoiceChat.Vivox] ServerUrl, Domain, or Issuer is not set. Vivox voice chat will not work"));
				Result = ResultFromErrorString(TEXT("Vivox config missing"));
			}
			else
			{
				VivoxClientApi::Uri BackendUri(TCHAR_TO_ANSI(*VivoxServerUrl));
				VivoxClientApi::VCSStatus Status = VivoxClientConnection.Connect(BackendUri);

				if (Status.IsError())
				{
					UE_LOG(LogVivoxVoiceChat, Warning, TEXT("Connect failed: server:%s error:%s (%i)"), ANSI_TO_TCHAR(BackendUri.ToString()), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
					Result = ResultFromVivoxStatus(Status);
				}
				else
				{
					ConnectionState = EConnectionState::Connecting;
				}
			}

			if (!Result.bSuccess)
			{
				ConnectionState = EConnectionState::Disconnected;

				TriggerCompletionDelegates(OnVoiceChatConnectCompleteDelegates, Result);
			}
		}
	}
}

void FVivoxVoiceChat::Disconnect(const FOnVoiceChatDisconnectCompleteDelegate& Delegate)
{
	if (IsConnected())
	{
		VivoxClientApi::Uri BackendUri(TCHAR_TO_ANSI(*VivoxServerUrl));
		VivoxClientConnection.Disconnect(BackendUri);

		ConnectionState = EConnectionState::Disconnecting;
		OnVoiceChatDisconnectCompleteDelegates.Add(Delegate);
	}
	else
	{
		Delegate.ExecuteIfBound(ResultSuccess);
	}
}

bool FVivoxVoiceChat::IsConnecting() const
{
	return ConnectionState == EConnectionState::Connecting;
}

bool FVivoxVoiceChat::IsConnected() const
{
	return ConnectionState == EConnectionState::Connected;
}

void FVivoxVoiceChat::Login(FPlatformUserId PlatformId, const FString& PlayerName, const FString& Credentials, const FOnVoiceChatLoginCompleteDelegate& Delegate)
{
	FVoiceChatResult Result = ResultSuccess;

	if (!IsInitialized())
	{
		Result = ResultFromErrorString(TEXT("Not Initialized"), VX_E_NOT_INITIALIZED);
	}
	else if (!IsConnected())
	{
		Result = ResultFromErrorString(TEXT("Not Connected"));
	}
	else if (IsLoggedIn())
	{
		if (PlayerName == GetLoggedInPlayerName())
		{
			Delegate.ExecuteIfBound(PlayerName, ResultSuccess);
			return;
		}
		else
		{
			Result = ResultFromErrorString(TEXT("Other user logged in"), VX_E_ALREADY_LOGGED_IN);
		}
	}
	else if (PlayerName.IsEmpty())
	{
		Result = ResultFromErrorString(TEXT("Player name is empty"), VX_E_INVALID_ARGUMENT);
	}
	else if (!VivoxNameContainsValidCharacters(PlayerName))
	{
		Result = ResultFromErrorString(TEXT("Invalid PlayerName"), VX_E_INVALID_ARGUMENT);
	}
	else if (PlayerName.Len() > 60 - VivoxNamespace.Len())
	{
		// Name must be between 3-63 characters long and start and end with a '.'. It also must contain the issuer and another '.' separating issuer and name
		Result = ResultFromErrorString(TEXT("PlayerName is too long"));
	}

	if (!Result.bSuccess)
	{
		UE_LOG(LogVivoxVoiceChat, Warning, TEXT("Login failed: PlayerName:%s error:%s"), *PlayerName, *Result.Error);
		Delegate.ExecuteIfBound(PlayerName, Result);
		return;
	}

	const VivoxClientApi::AccountName AccountName = CreateAccountName(PlayerName);
	const VivoxClientApi::Uri UserUri = CreateUserUri(PlayerName);

	if (!AccountName.IsValid() || !UserUri.IsValid())
	{
		VivoxClientApi::VCSStatus Status(VX_E_INVALID_ARGUMENT);
		UE_LOG(LogVivoxVoiceChat, Warning, TEXT("Login failed: PlayerName:%s error:%s (%i)"), *PlayerName, ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
		Delegate.ExecuteIfBound(PlayerName, ResultFromVivoxStatus(Status));
		return;
	}

	LoginSession.PlatformId = PlatformId;
	LoginSession.PlayerName = PlayerName;
	LoginSession.AccountName = AccountName;
	LoginSession.UserUri = UserUri;
	LoginSession.State = FLoginSession::EState::LoggedOut;

	VivoxClientApi::VCSStatus Status = VivoxClientConnection.Login(LoginSession.AccountName, TCHAR_TO_ANSI(*Credentials));
	if (Status.IsError())
	{
		UE_LOG(LogVivoxVoiceChat, Warning, TEXT("Login failed: account:%s error:%s (%i)"), ANSI_TO_TCHAR(LoginSession.AccountName.ToString()), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
		Delegate.ExecuteIfBound(PlayerName, ResultFromVivoxStatus(Status));
		return;
	}

	LoginSession.State = FLoginSession::EState::LoggingIn;

	OnVoiceChatLoginCompleteDelegate = Delegate;
}

void FVivoxVoiceChat::Logout(const FOnVoiceChatLogoutCompleteDelegate& Delegate)
{
	FVoiceChatResult Result = ResultSuccess;

	if (!IsInitialized())
	{
		Result = ResultFromErrorString(TEXT("Not Initialized"), VX_E_NOT_INITIALIZED);
	}
	else if (!IsConnected())
	{
		Result = ResultFromErrorString(TEXT("Not Connected"));
	}
	else if (!IsLoggedIn())
	{
		Result = ResultFromErrorString(TEXT("Not Logged In"), VX_E_NOT_LOGGED_IN);
	}
	// TODO: handle IsLoggingIn case

	if (!Result.bSuccess)
	{
		UE_LOG(LogVivoxVoiceChat, Warning, TEXT("Logout failed: error:%s"), *Result.Error);
		Delegate.ExecuteIfBound(FString(), Result);
		return;
	}

	VivoxClientApi::AccountName AccountName = CreateAccountName(LoginSession.PlayerName);
	VivoxClientApi::VCSStatus Status = VivoxClientConnection.Logout(AccountName);
	if (Status.IsError())
	{
		UE_LOG(LogVivoxVoiceChat, Warning, TEXT("Logout failed: error:%s (%i)"), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
		Delegate.ExecuteIfBound(LoginSession.PlayerName, ResultFromVivoxStatus(Status));
		return;
	}

	OnVoiceChatLogoutCompleteDelegate = Delegate;
	LoginSession.State = FLoginSession::EState::LoggingOut;
}

bool FVivoxVoiceChat::IsLoggingIn() const
{
	return LoginSession.State == FLoginSession::EState::LoggingIn;
}

bool FVivoxVoiceChat::IsLoggedIn() const
{
	return LoginSession.State == FLoginSession::EState::LoggedIn;
}

FString FVivoxVoiceChat::GetLoggedInPlayerName() const
{
	return IsLoggedIn() ? LoginSession.PlayerName : FString();
}

void FVivoxVoiceChat::BlockPlayers(const TArray<FString>& PlayerNames)
{
	std::set<VivoxClientApi::Uri> UsersToBlock;
	for (const FString& PlayerName : PlayerNames)
	{
		UsersToBlock.insert(CreateUserUri(PlayerName));
	}

	VivoxClientApi::VCSStatus Status = VivoxClientConnection.BlockUsers(LoginSession.AccountName, UsersToBlock);
	if (Status.IsError())
	{
		UE_LOG(LogVivoxVoiceChat, Warning, TEXT("BlockPlayers failed: error:%s (%i)"), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
	}
}

void FVivoxVoiceChat::UnblockPlayers(const TArray<FString>& PlayerNames)
{
	std::set<VivoxClientApi::Uri> UsersToUnblock;
	for (const FString& PlayerName : PlayerNames)
	{
		UsersToUnblock.insert(CreateUserUri(PlayerName));
	}

	VivoxClientApi::VCSStatus Status = VivoxClientConnection.UnblockUsers(LoginSession.AccountName, UsersToUnblock);
	if (Status.IsError())
	{
		UE_LOG(LogVivoxVoiceChat, Warning, TEXT("UnblockPlayers failed: error:%s (%i)"), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
	}
}

void FVivoxVoiceChat::JoinChannel(const FString& ChannelName, const FString& ChannelCredentials, EVoiceChatChannelType ChannelType, const FOnVoiceChatChannelJoinCompleteDelegate& Delegate, TOptional<FVoiceChatChannel3dProperties> Channel3dProperties)
{
	FVoiceChatResult Result = ResultSuccess;

	if (!IsInitialized())
	{
		Result = ResultFromErrorString(TEXT("Not Initialized"), VX_E_NOT_INITIALIZED);
	}
	else if (!IsConnected())
	{
		Result = ResultFromErrorString(TEXT("Not Connected"));
	}
	else if (!IsLoggedIn())
	{
		Result = ResultFromErrorString(TEXT("Not Logged In"), VX_E_NOT_LOGGED_IN);
	}
	else if (ChannelName.IsEmpty())
	{
		Result = ResultFromErrorString(TEXT("ChannelName is empty"), VX_E_INVALID_ARGUMENT);
	}
	else if (!VivoxNameContainsValidCharacters(ChannelName))
	{
		Result = ResultFromErrorString(TEXT("Invalid ChannelName"), VX_E_INVALID_ARGUMENT);
	}
	else if (ChannelName.Len() > 189 - VivoxNamespace.Len())
	{
		// channel name length must not exceed 200 characters, including the confctl-?- prefix and the issuer and separator
		Result = ResultFromErrorString(TEXT("ChannelName is too long"), VX_E_INVALID_ARGUMENT);
	}

	if (!Result.bSuccess)
	{
		UE_LOG(LogVivoxVoiceChat, Warning, TEXT("JoinChannel failed: ChannelName:%s error:%s"), *ChannelName, *Result.Error);
		Delegate.ExecuteIfBound(ChannelName, Result);
		return;
	}

	FChannelSession& ChannelSession = GetChannelSession(ChannelName);
	if (ChannelSession.State == FChannelSession::EState::Connected)
	{
		Delegate.ExecuteIfBound(ChannelName, ResultSuccess);
		return;
	}
	else if (ChannelSession.State == FChannelSession::EState::Connecting)
	{
		Delegate.ExecuteIfBound(ChannelName, ResultFromErrorString("Join in progress"));
		return;
	}
	else if (ChannelSession.State == FChannelSession::EState::Disconnecting)
	{
		Delegate.ExecuteIfBound(ChannelName, ResultFromErrorString("Leave in progress"));
		return;
	}

	ChannelSession.ChannelName = ChannelName;
	ChannelSession.ChannelType = ChannelType;
	ChannelSession.ChannelUri = CreateChannelUri(ChannelName, ChannelType, Channel3dProperties);

	VivoxClientApi::VCSStatus Status = VivoxClientConnection.JoinChannel(LoginSession.AccountName, ChannelSession.ChannelUri, TCHAR_TO_ANSI(*ChannelCredentials));
	if (Status.IsError())
	{
		UE_LOG(LogVivoxVoiceChat, Warning, TEXT("JoinChannel failed: channel:%s error:%s (%i)"), ANSI_TO_TCHAR(ChannelSession.ChannelUri.ToString()), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
		Delegate.ExecuteIfBound(ChannelName, ResultFromVivoxStatus(Status));
		return;
	}

	ChannelSession.State = FChannelSession::EState::Connecting;
	ChannelSession.JoinDelegate = Delegate;
}

void FVivoxVoiceChat::LeaveChannel(const FString& ChannelName, const FOnVoiceChatChannelLeaveCompleteDelegate& Delegate)
{
	FVoiceChatResult Result = ResultSuccess;

	if (!IsInitialized())
	{
		Result = ResultFromErrorString(TEXT("Not Initialized"), VX_E_NOT_INITIALIZED);
	}
	else if (!IsConnected())
	{
		Result = ResultFromErrorString(TEXT("Not Connected"));
	}
	else if (!IsLoggedIn())
	{
		Result = ResultFromErrorString(TEXT("Not Logged In"), VX_E_NOT_LOGGED_IN);
	}
	else if (ChannelName.IsEmpty())
	{
		Result = ResultFromErrorString(TEXT("ChannelName is empty"), VX_E_INVALID_ARGUMENT);
	}

	FChannelSession& ChannelSession = GetChannelSession(ChannelName);
	if (ChannelSession.State != FChannelSession::EState::Connected)
	{
		Result = ResultFromErrorString("Not in channel");
	}

	if (!Result.bSuccess)
	{
		UE_LOG(LogVivoxVoiceChat, Warning, TEXT("LeaveChannel failed: ChannelName:%s error:%s"), *ChannelName, *Result.Error);
		Delegate.ExecuteIfBound(ChannelName, Result);
		return;
	}

	VivoxClientApi::VCSStatus Status = VivoxClientConnection.LeaveChannel(LoginSession.AccountName, ChannelSession.ChannelUri);
	if (Status.IsError())
	{
		UE_LOG(LogVivoxVoiceChat, Warning, TEXT("LeaveChannel failed: channel:%s error:%s (%i)"), ANSI_TO_TCHAR(ChannelSession.ChannelUri.ToString()), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
		Delegate.ExecuteIfBound(ChannelName, ResultFromVivoxStatus(Status));
		return;
	}

	ChannelSession.State = FChannelSession::EState::Disconnecting;
	ChannelSession.LeaveDelegate = Delegate;
}

void FVivoxVoiceChat::Set3DPosition(const FString& ChannelName, const FVector& SpeakerPosition, const FVector& ListenerPosition, const FVector& ListenerForwardDirection, const FVector& ListenerUpDirection)
{
	FChannelSession& ChannelSession = GetChannelSession(ChannelName);

	// Transform Pos and Direction to up -> (0,1,0) and left -> (-1, 0, 0)
	FVector RotatedPos(ListenerPosition.Y, ListenerPosition.Z, -ListenerPosition.X);
	FVector RotatedForwardDirection(ListenerForwardDirection.Y, ListenerForwardDirection.Z, -ListenerForwardDirection.X);
	FVector RotatedUpDirection(ListenerUpDirection.Y, ListenerUpDirection.Z, -ListenerUpDirection.X);

	VivoxClientApi::VCSStatus Status = VivoxClientConnection.Set3DPosition(LoginSession.AccountName, ChannelSession.ChannelUri, ToVivoxVector(SpeakerPosition), ToVivoxVector(ListenerPosition), ToVivoxVector(ListenerForwardDirection), ToVivoxVector(ListenerUpDirection));
	if (Status.IsError())
	{
		UE_LOG(LogVivoxVoiceChat, Warning, TEXT("Set3DPosition failed: channel:%s error:%s (%i)"), ANSI_TO_TCHAR(ChannelSession.ChannelUri.ToString()), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
	}
}

TArray<FString> FVivoxVoiceChat::GetChannels() const
{
	TArray<FString> ChannelNames;
	for (const TPair<FString, FChannelSession>& ChannelSessionPair : LoginSession.ChannelSessions)
	{
		const FString& ChannelName = ChannelSessionPair.Key;
		const FChannelSession& ChannelSession = ChannelSessionPair.Value;
		if (ChannelSession.State == FChannelSession::EState::Connected)
		{
			ChannelNames.Add(ChannelName);
		}
	}
	return ChannelNames;
}

TArray<FString> FVivoxVoiceChat::GetPlayersInChannel(const FString& ChannelName) const
{
	TArray<FString> PlayerNames;
	GetChannelSession(ChannelName).Participants.GenerateKeyArray(PlayerNames);
	return PlayerNames;
}

EVoiceChatChannelType FVivoxVoiceChat::GetChannelType(const FString& ChannelName) const
{
	return GetChannelSession(ChannelName).ChannelType;
}

bool FVivoxVoiceChat::IsPlayerTalking(const FString& PlayerName) const
{
	return GetParticipant(PlayerName).bTalking;
}

void FVivoxVoiceChat::SetPlayerMuted(const FString& PlayerName, bool bMuted)
{
	FParticipant& Participant = GetParticipant(PlayerName);
	Participant.bMuted = bMuted;

	for (TPair<FString, FChannelSession>& ChannelSessionPair : LoginSession.ChannelSessions)
	{
		FChannelSession& ChannelSession = ChannelSessionPair.Value;
		if (ChannelSession.State == FChannelSession::EState::Connected)
		{
			if (FParticipant* ChannelParticipant = ChannelSession.Participants.Find(PlayerName))
			{
				const bool bShouldMute = Participant.bMuted || Participant.Volume < SMALL_NUMBER;
				VivoxClientApi::VCSStatus Status = VivoxClientConnection.SetParticipantMutedForMe(LoginSession.AccountName, ChannelParticipant->UserUri, ChannelSession.ChannelUri, bShouldMute);
				if (Status.IsError())
				{
					UE_LOG(LogVivoxVoiceChat, Warning, TEXT("SetParticipantMutedForMe failed: channel:%s user:%s muted:%s error:%s (%i)"), ANSI_TO_TCHAR(ChannelSession.ChannelUri.ToString()), ANSI_TO_TCHAR(ChannelParticipant->UserUri.ToString()), *LexToString(bShouldMute), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
					// TODO: This will fail only when Account/participant/channel is not found -> fixup our state
				}
			}
		}
	}
}

bool FVivoxVoiceChat::IsPlayerMuted(const FString& PlayerName) const
{
	return GetParticipant(PlayerName).bMuted;
}

void FVivoxVoiceChat::SetPlayerVolume(const FString& PlayerName, float Volume)
{
	FParticipant& Participant = GetParticipant(PlayerName);
	Participant.Volume = FMath::Clamp(Volume, 0.0f, 1.0f);
	Participant.IntVolume = FMath::Lerp(VIVOX_MIN_VOL, VIVOX_MAX_VOL, Participant.Volume);

	for (TPair<FString, FChannelSession>& ChannelSessionPair : LoginSession.ChannelSessions)
	{
		FChannelSession& ChannelSession = ChannelSessionPair.Value;
		if (ChannelSession.State == FChannelSession::EState::Connected)
		{
			if (FParticipant* ChannelParticipant = ChannelSession.Participants.Find(PlayerName))
			{
				const bool bShouldMute = Participant.bMuted || Participant.Volume < SMALL_NUMBER;
				VivoxClientApi::VCSStatus Status = VivoxClientConnection.SetParticipantMutedForMe(LoginSession.AccountName, ChannelParticipant->UserUri, ChannelSession.ChannelUri, bShouldMute);
				if (Status.IsError())
				{
					UE_LOG(LogVivoxVoiceChat, Warning, TEXT("SetParticipantMutedForMe failed: channel:%s user:%s muted:%s error:%s (%i)"), ANSI_TO_TCHAR(ChannelSession.ChannelUri.ToString()), ANSI_TO_TCHAR(ChannelParticipant->UserUri.ToString()), *LexToString(bShouldMute), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
					// TODO: This will fail only when Account/participant/channel is not found -> fixup our state
				}

				Status = VivoxClientConnection.SetParticipantAudioOutputDeviceVolumeForMe(LoginSession.AccountName, ChannelParticipant->UserUri, ChannelSession.ChannelUri, Participant.IntVolume);
				if (Status.IsError())
				{
					UE_LOG(LogVivoxVoiceChat, Warning, TEXT("SetParticipantAudioOutputDeviceVolumeForMe failed: channel:%s user:%s volume:%i error:%s (%i)"), ANSI_TO_TCHAR(ChannelSession.ChannelUri.ToString()), ANSI_TO_TCHAR(ChannelParticipant->UserUri.ToString()), Participant.IntVolume, ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
					// TODO: This will fail only when Account/participant/channel is not found -> fixup our state
				}
			}
		}
	}
}

float FVivoxVoiceChat::GetPlayerVolume(const FString& PlayerName) const
{
	return GetParticipant(PlayerName).Volume;
}

void FVivoxVoiceChat::TransmitToNoChannels()
{
	UE_LOG(LogVivoxVoiceChat, Log, TEXT("TransmitToNoChannels"));

	VivoxClientApi::VCSStatus Status = VivoxClientConnection.SetTransmissionToNone(LoginSession.AccountName);
	if (Status.IsError())
	{
		UE_LOG(LogVivoxVoiceChat, Warning, TEXT("SetTransmissionToNone failed: error:%s (%i)"), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
	}
}

void FVivoxVoiceChat::TransmitToAllChannels()
{
	UE_LOG(LogVivoxVoiceChat, Log, TEXT("TransmitToAllChannels"));

	VivoxClientApi::VCSStatus Status = VivoxClientConnection.SetTransmissionToAll(LoginSession.AccountName);
	if (Status.IsError())
	{
		UE_LOG(LogVivoxVoiceChat, Warning, TEXT("SetTransmissionToAll failed: error:%s (%i)"), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
	}
}

void FVivoxVoiceChat::TransmitToSpecificChannel(const FString& Channel)
{
	UE_LOG(LogVivoxVoiceChat, Log, TEXT("TransmitToSpecificChannel %s"), *Channel);

	FChannelSession& ChannelSession = GetChannelSession(Channel);
	if (ChannelSession.State == FChannelSession::EState::Connected)
	{
		VivoxClientApi::VCSStatus Status = VivoxClientConnection.SetTransmissionToSpecificChannel(LoginSession.AccountName, ChannelSession.ChannelUri);
		if (Status.IsError())
		{
			UE_LOG(LogVivoxVoiceChat, Warning, TEXT("TransmitToSpecificChannel failed: channel:%s error:%s (%i)"), ANSI_TO_TCHAR(ChannelSession.ChannelUri.ToString()), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
		}
	}
}

EVoiceChatTransmitMode FVivoxVoiceChat::GetTransmitMode() const
{
	VivoxClientApi::ChannelTransmissionPolicy TransmissionPolicy = VivoxClientConnection.GetChannelTransmissionPolicy(LoginSession.AccountName);
	switch (TransmissionPolicy.GetChannelTransmissionPolicy())
	{
	default:
	case VivoxClientApi::ChannelTransmissionPolicy::vx_channel_transmission_policy_all:
		return EVoiceChatTransmitMode::All;
	case VivoxClientApi::ChannelTransmissionPolicy::vx_channel_transmission_policy_none:
		return EVoiceChatTransmitMode::None;
	case VivoxClientApi::ChannelTransmissionPolicy::vx_channel_transmission_policy_specific_channel:
		return EVoiceChatTransmitMode::Channel;
	}
}

FString FVivoxVoiceChat::GetTransmitChannel() const
{
	VivoxClientApi::ChannelTransmissionPolicy TransmissionPolicy = VivoxClientConnection.GetChannelTransmissionPolicy(LoginSession.AccountName);
	if (TransmissionPolicy.GetChannelTransmissionPolicy() == VivoxClientApi::ChannelTransmissionPolicy::vx_channel_transmission_policy_specific_channel)
	{
		return ANSI_TO_TCHAR(TransmissionPolicy.GetSpecificTransmissionChannel().ToString());
	}
	else
	{
		return FString();
	}
}

FDelegateHandle FVivoxVoiceChat::StartRecording(const FOnVoiceChatRecordSamplesAvailableDelegate::FDelegate& Delegate)
{
	if (!VivoxClientConnection.AudioInputDeviceTestIsRecording())
	{
		VivoxClientApi::VCSStatus Status = VivoxClientConnection.StartAudioInputDeviceTestRecord();
		if (Status.IsError())
		{
			UE_LOG(LogVivoxVoiceChat, Warning, TEXT("StartRecording failed: error:%s (%i)"), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
			return FDelegateHandle();
		}
	}

	FScopeLock Lock(&AudioRecordLock);

	return OnVoiceChatRecordSamplesAvailableDelegate.Add(Delegate);
}

void FVivoxVoiceChat::StopRecording(FDelegateHandle Handle)
{
	FScopeLock Lock(&AudioRecordLock);

	OnVoiceChatRecordSamplesAvailableDelegate.Remove(Handle);

	if (!OnVoiceChatRecordSamplesAvailableDelegate.IsBound())
	{
		VivoxClientConnection.StopAudioInputDeviceTestRecord();
	}
}

FDelegateHandle FVivoxVoiceChat::RegisterOnVoiceChatAfterCaptureAudioReadDelegate(const FOnVoiceChatAfterCaptureAudioReadDelegate::FDelegate& Delegate)
{
	FScopeLock Lock(&AfterCaptureAudioReadLock);

	return OnVoiceChatAfterCaptureAudioReadDelegate.Add(Delegate);
}

void FVivoxVoiceChat::UnregisterOnVoiceChatAfterCaptureAudioReadDelegate(FDelegateHandle Handle)
{
	FScopeLock Lock(&AfterCaptureAudioReadLock);

	OnVoiceChatAfterCaptureAudioReadDelegate.Remove(Handle);
}

FDelegateHandle FVivoxVoiceChat::RegisterOnVoiceChatBeforeCaptureAudioSentDelegate(const FOnVoiceChatBeforeCaptureAudioSentDelegate::FDelegate& Delegate)
{
	FScopeLock Lock(&BeforeCaptureAudioSentLock);

	return OnVoiceChatBeforeCaptureAudioSentDelegate.Add(Delegate);
}

void FVivoxVoiceChat::UnregisterOnVoiceChatBeforeCaptureAudioSentDelegate(FDelegateHandle Handle)
{
	FScopeLock Lock(&BeforeCaptureAudioSentLock);

	OnVoiceChatBeforeCaptureAudioSentDelegate.Remove(Handle);
}

FDelegateHandle FVivoxVoiceChat::RegisterOnVoiceChatBeforeRecvAudioRenderedDelegate(const FOnVoiceChatBeforeRecvAudioRenderedDelegate::FDelegate& Delegate)
{
	FScopeLock Lock(&BeforeRecvAudioRenderedLock);

	return OnVoiceChatBeforeRecvAudioRenderedDelegate.Add(Delegate);
}

void FVivoxVoiceChat::UnregisterOnVoiceChatBeforeRecvAudioRenderedDelegate(FDelegateHandle Handle)
{
	FScopeLock Lock(&BeforeRecvAudioRenderedLock);

	OnVoiceChatBeforeRecvAudioRenderedDelegate.Remove(Handle);
}

FString FVivoxVoiceChat::InsecureGetLoginToken(const FString& PlayerName)
{
	FString Token;

	if (IsInitialized())
	{
		VivoxClientApi::Uri UserUri = CreateUserUri(PlayerName);
		if (char* ANSIToken = vx_debug_generate_token(TCHAR_TO_ANSI(*VivoxIssuer), FDateTime::UtcNow().ToUnixTimestamp() + 90, "login", FMath::Rand(), nullptr, UserUri.ToString(), nullptr, (const unsigned char*)TCHAR_TO_ANSI(*VivoxInsecureSecret), VivoxInsecureSecret.Len()))
		{
			Token = ANSI_TO_TCHAR(ANSIToken);
			vx_free(ANSIToken);
		}
	}

	return Token;
}

FString FVivoxVoiceChat::InsecureGetJoinToken(const FString& ChannelName, EVoiceChatChannelType ChannelType, TOptional<FVoiceChatChannel3dProperties> Channel3dProperties)
{
	FString Token;

	if (IsInitialized() && IsLoggedIn())
	{
		VivoxClientApi::Uri ChannelUri = CreateChannelUri(ChannelName, ChannelType, Channel3dProperties);
		if (char* ANSIToken = vx_debug_generate_token(TCHAR_TO_ANSI(*VivoxIssuer), FDateTime::UtcNow().ToUnixTimestamp() + 90, "join", FMath::Rand(), nullptr, LoginSession.UserUri.ToString(), ChannelUri.ToString(), (const unsigned char*)TCHAR_TO_ANSI(*VivoxInsecureSecret), VivoxInsecureSecret.Len()))
		{
			Token = ANSI_TO_TCHAR(ANSIToken);
			vx_free(ANSIToken);
		}
	}

	return Token;
}

void FVivoxVoiceChat::InvokeOnUIThread(void (Func)(void* Arg0), void* Arg0)
{
	AsyncTask(ENamedThreads::GameThread, [Func, Arg0]()
	{
		if (Func)
		{
			(*Func)(Arg0);
		}
	});
}

void FVivoxVoiceChat::onLogStatementEmitted(LogLevel Level, long long NativeMillisecondsSinceEpoch, long ThreadId, const char* LogMessage)
{
	if (Level == LogLevelError)
	{
		UE_LOG(LogVivoxVoiceChat, Warning, TEXT("vivox: Error: %s"), ANSI_TO_TCHAR(LogMessage));
	}
	else
	{
		FString LogLevelString;
		switch (Level)
		{
		case LogLevelError:		LogLevelString = TEXT("Error"); break;
		case LogLevelWarning:	LogLevelString = TEXT("Warning"); break;
		case LogLevelInfo:		LogLevelString = TEXT("Info"); break;
		case LogLevelDebug:		LogLevelString = TEXT("Debug"); break;
		case LogLevelTrace:		LogLevelString = TEXT("Trace"); break;
		default:				LogLevelString = TEXT("Unknown"); break;
		}

		UE_LOG(LogVivoxVoiceChat, Log, TEXT("vivox: %s: %s"), *LogLevelString, ANSI_TO_TCHAR(LogMessage));
	}
}

void FVivoxVoiceChat::onConnectCompleted(const VivoxClientApi::Uri& Server)
{
	UE_LOG(LogVivoxVoiceChat, Log, TEXT("onConnectCompleted server:%s"), ANSI_TO_TCHAR(Server.ToString()));

	ConnectionState = EConnectionState::Connected;

	TriggerCompletionDelegates(OnVoiceChatConnectCompleteDelegates, ResultSuccess);
}

void FVivoxVoiceChat::onConnectFailed(const VivoxClientApi::Uri& Server, const VivoxClientApi::VCSStatus& Status)
{
	UE_LOG(LogVivoxVoiceChat, Warning, TEXT("onConnectFailed server:%s error:%s (%i)"), ANSI_TO_TCHAR(Server.ToString()), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());

	ConnectionState = EConnectionState::Disconnected;

	TriggerCompletionDelegates(OnVoiceChatConnectCompleteDelegates, ResultFromVivoxStatus(Status));
}

void FVivoxVoiceChat::onDisconnected(const VivoxClientApi::Uri& Server, const VivoxClientApi::VCSStatus& Status)
{
	if (Status.IsError())
	{
		UE_LOG(LogVivoxVoiceChat, Warning, TEXT("onDisconnected server:%s error:%s (%i)"), ANSI_TO_TCHAR(Server.ToString()), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
	}
	else
	{
		UE_LOG(LogVivoxVoiceChat, Log, TEXT("onDisconnected server:%s"), ANSI_TO_TCHAR(Server.ToString()));
	}

	ClearLoginSession();

	if (ConnectionState == EConnectionState::Disconnecting)
	{
		TriggerCompletionDelegates(OnVoiceChatDisconnectCompleteDelegates, ResultSuccess);
	}
	else
	{
		OnVoiceChatDisconnectedDelegate.Broadcast(ResultFromVivoxStatus(Status));
	}

	ConnectionState = EConnectionState::Disconnected;
}

void FVivoxVoiceChat::onLoginCompleted(const VivoxClientApi::AccountName& AccountName)
{
	UE_LOG(LogVivoxVoiceChat, Log, TEXT("onLoginCompleted account:%s"), ANSI_TO_TCHAR(AccountName.ToString()));

	FString PlayerName = GetPlayerNameFromAccountName(AccountName);

	LoginSession.State = FLoginSession::EState::LoggedIn;

	TriggerCompletionDelegate(OnVoiceChatLoginCompleteDelegate, PlayerName, ResultSuccess);
}

void FVivoxVoiceChat::onInvalidLoginCredentials(const VivoxClientApi::AccountName& AccountName)
{
	UE_LOG(LogVivoxVoiceChat, Warning, TEXT("onInvalidLoginCredentials account:%s"), ANSI_TO_TCHAR(AccountName.ToString()));

	LoginSession.State = FLoginSession::EState::LoggedOut;

	FString PlayerName = GetPlayerNameFromAccountName(AccountName);

	TriggerCompletionDelegate(OnVoiceChatLoginCompleteDelegate, PlayerName, ResultFromErrorString(TEXT("Invalid login credentials")));
}

void FVivoxVoiceChat::onLoginFailed(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::VCSStatus& Status)
{
	UE_LOG(LogVivoxVoiceChat, Warning, TEXT("onLoginFailed account:%s error:%s (%i)"), ANSI_TO_TCHAR(AccountName.ToString()), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());

	LoginSession.State = FLoginSession::EState::LoggedOut;

	FString PlayerName = GetPlayerNameFromAccountName(AccountName);

	TriggerCompletionDelegate(OnVoiceChatLoginCompleteDelegate, PlayerName, ResultFromVivoxStatus(Status));
}

void FVivoxVoiceChat::onLogoutCompleted(const VivoxClientApi::AccountName& AccountName)
{
	UE_LOG(LogVivoxVoiceChat, Log, TEXT("onLogoutCompleted account:%s"), ANSI_TO_TCHAR(AccountName.ToString()));

	ClearChannelSessions();

	LoginSession.State = FLoginSession::EState::LoggedOut;

	FString PlayerName = GetPlayerNameFromAccountName(AccountName);
	TriggerCompletionDelegate(OnVoiceChatLogoutCompleteDelegate, PlayerName, ResultSuccess);
}

void FVivoxVoiceChat::onLogoutFailed(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::VCSStatus& Status)
{
	UE_LOG(LogVivoxVoiceChat, Warning, TEXT("onLogoutFailed account:%s error:%s (%i)"), ANSI_TO_TCHAR(AccountName.ToString()), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());

	LoginSession.State = FLoginSession::EState::LoggedIn;

	FString PlayerName = GetPlayerNameFromAccountName(AccountName);
	TriggerCompletionDelegate(OnVoiceChatLogoutCompleteDelegate, PlayerName, ResultFromVivoxStatus(Status));
}

void FVivoxVoiceChat::onChannelJoined(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri)
{
	UE_LOG(LogVivoxVoiceChat, Log, TEXT("onChannelJoined channel:%s"), ANSI_TO_TCHAR(ChannelUri.ToString()));

	FChannelSession& ChannelSession = GetChannelSession(ChannelUri);
	ChannelSession.State = FChannelSession::EState::Connected;
	TriggerCompletionDelegate(ChannelSession.JoinDelegate, ChannelSession.ChannelName, ResultSuccess);
}

void FVivoxVoiceChat::onInvalidChannelCredentials(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri)
{
	UE_LOG(LogVivoxVoiceChat, Warning, TEXT("onInvalidChannelCredentials channel:%s"), ANSI_TO_TCHAR(ChannelUri.ToString()));

	FChannelSession& ChannelSession = GetChannelSession(ChannelUri);
	ChannelSession.State = FChannelSession::EState::Disconnected;
	TriggerCompletionDelegate(ChannelSession.JoinDelegate, ChannelSession.ChannelName, ResultFromErrorString(TEXT("Invalid join credentials")));

	RemoveChannelSession(ChannelSession.ChannelName);
}

void FVivoxVoiceChat::onChannelJoinFailed(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri, const VivoxClientApi::VCSStatus& Status)
{
	UE_LOG(LogVivoxVoiceChat, Warning, TEXT("onChannelJoinFailed channel:%s error:%s (%i)"), ANSI_TO_TCHAR(ChannelUri.ToString()), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());

	FChannelSession& ChannelSession = GetChannelSession(ChannelUri);
	ChannelSession.State = FChannelSession::EState::Disconnected;
	TriggerCompletionDelegate(ChannelSession.JoinDelegate, ChannelSession.ChannelName, ResultFromVivoxStatus(Status));

	RemoveChannelSession(ChannelSession.ChannelName);
}

void FVivoxVoiceChat::onChannelExited(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri, const VivoxClientApi::VCSStatus& ReasonCode)
{
	if (ReasonCode.IsError())
	{
		UE_LOG(LogVivoxVoiceChat, Warning, TEXT("onChannelExited channel:%s error:%s (%i)"), ANSI_TO_TCHAR(ChannelUri.ToString()), ANSI_TO_TCHAR(ReasonCode.ToString()), ReasonCode.GetStatusCode());
	}
	else
	{
		UE_LOG(LogVivoxVoiceChat, Log, TEXT("onChannelExited channel:%s"), ANSI_TO_TCHAR(ChannelUri.ToString()));
	}

	FChannelSession& ChannelSession = GetChannelSession(ChannelUri);
	const bool bWasConnected = ChannelSession.State == FChannelSession::EState::Connected;
	const bool bWasConnecting = ChannelSession.State == FChannelSession::EState::Connecting;
	const bool bWasDisconnecting = ChannelSession.State == FChannelSession::EState::Disconnecting;
	ChannelSession.State = FChannelSession::EState::Disconnected;

	if (bWasConnected)
	{
		OnVoiceChatChannelExitedDelegate.Broadcast(ChannelSession.ChannelName, ResultFromVivoxStatus(ReasonCode));
	}
	else if (bWasConnecting)
	{
		// timeouts while connecting call onChannelExited instead of OnChannelJoinFailed
		TriggerCompletionDelegate(ChannelSession.JoinDelegate, ChannelSession.ChannelName, ResultFromVivoxStatus(ReasonCode));
	}
	else if (bWasDisconnecting)
	{
		TriggerCompletionDelegate(ChannelSession.LeaveDelegate, ChannelSession.ChannelName, ResultFromVivoxStatus(ReasonCode));
	}

	RemoveChannelSession(ChannelSession.ChannelName);
}

void FVivoxVoiceChat::onCallStatsUpdated(const VivoxClientApi::AccountName& AccountName, vx_call_stats_t& Stats, bool bIsFinal)
{

}

void FVivoxVoiceChat::onParticipantAdded(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri, const VivoxClientApi::Uri& ParticipantUri, bool bIsLoggedInUser)
{
	UE_LOG(LogVivoxVoiceChat, Log, TEXT("onParticipantAdded channel:%s participant:%s"), ANSI_TO_TCHAR(ChannelUri.ToString()), ANSI_TO_TCHAR(ParticipantUri.ToString()));

	FChannelSession& ChannelSession = GetChannelSession(ChannelUri);
	FString PlayerName = GetPlayerNameFromUri(ParticipantUri);
	FParticipant& ChannelParticipant = ChannelSession.Participants.Add(PlayerName);
	ChannelParticipant.PlayerName = PlayerName;
	ChannelParticipant.UserUri = ParticipantUri;

	OnVoiceChatPlayerAddedDelegate.Broadcast(ChannelSession.ChannelName, PlayerName);

	const FParticipant& Participant = GetParticipant(PlayerName);

	// Apply any existing mutes
	if (Participant.bMuted != ChannelParticipant.bMuted)
	{
		const bool bShouldMute = Participant.bMuted || Participant.Volume < SMALL_NUMBER;
		VivoxClientApi::VCSStatus Status = VivoxClientConnection.SetParticipantMutedForMe(LoginSession.AccountName, ChannelParticipant.UserUri, ChannelSession.ChannelUri, bShouldMute);
		if (Status.IsError())
		{
			UE_LOG(LogVivoxVoiceChat, Warning, TEXT("SetParticipantMutedForMe failed: channel:%s user:%s muted:%s error:%s (%i)"), ANSI_TO_TCHAR(ChannelSession.ChannelUri.ToString()), ANSI_TO_TCHAR(ChannelParticipant.UserUri.ToString()), *LexToString(bShouldMute), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
			// TODO: This will fail only when Account/participant/channel is not found -> fixup our state
		}
	}

	// Apply any existing volume adjustments
	if (Participant.IntVolume != ChannelParticipant.IntVolume)
	{
		const bool bShouldMute = Participant.bMuted || Participant.Volume < SMALL_NUMBER;
		VivoxClientApi::VCSStatus Status = VivoxClientConnection.SetParticipantMutedForMe(LoginSession.AccountName, ChannelParticipant.UserUri, ChannelSession.ChannelUri, bShouldMute);
		if (Status.IsError())
		{
			UE_LOG(LogVivoxVoiceChat, Warning, TEXT("SetParticipantMutedForMe failed: channel:%s user:%s muted:%s error:%s (%i)"), ANSI_TO_TCHAR(ChannelSession.ChannelUri.ToString()), ANSI_TO_TCHAR(ChannelParticipant.UserUri.ToString()), *LexToString(bShouldMute), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
			// TODO: This will fail only when Account/participant/channel is not found -> fixup our state
		}

		Status = VivoxClientConnection.SetParticipantAudioOutputDeviceVolumeForMe(LoginSession.AccountName, ChannelParticipant.UserUri, ChannelSession.ChannelUri, Participant.IntVolume);
		if (Status.IsError())
		{
			UE_LOG(LogVivoxVoiceChat, Warning, TEXT("SetParticipantAudioOutputDeviceVolumeForMe failed: channel:%s user:%s volume:%i error:%s (%i)"), ANSI_TO_TCHAR(ChannelSession.ChannelUri.ToString()), ANSI_TO_TCHAR(ChannelParticipant.UserUri.ToString()), Participant.IntVolume, ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
			// TODO: This will fail only when Account/participant/channel is not found -> fixup our state
		}
	}
}

void FVivoxVoiceChat::onParticipantLeft(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri, const VivoxClientApi::Uri& ParticipantUri, bool bIsLoggedInUser, ParticipantLeftReason Reason)
{
	UE_LOG(LogVivoxVoiceChat, Log, TEXT("onParticipantLeft channel:%s participant:%s reason:%s"), ANSI_TO_TCHAR(ChannelUri.ToString()), ANSI_TO_TCHAR(ParticipantUri.ToString()), *LexToString(Reason));

	FChannelSession& ChannelSession = GetChannelSession(ChannelUri);
	FString PlayerName = GetPlayerNameFromUri(ParticipantUri);

	ChannelSession.Participants.Remove(PlayerName);

	OnVoiceChatPlayerRemovedDelegate.Broadcast(ChannelSession.ChannelName, PlayerName);
}

void FVivoxVoiceChat::onParticipantUpdated(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri, const VivoxClientApi::Uri& ParticipantUri, bool bIsLoggedInUser, bool bSpeaking, double MeterEnergy, bool bMutedForAll)
{
	UE_LOG(LogVivoxVoiceChat, VeryVerbose, TEXT("onParticipantUpdated channel:%s participant:%s speaking:%s energy:%f"), ANSI_TO_TCHAR(ChannelUri.ToString()), ANSI_TO_TCHAR(ParticipantUri.ToString()), *LexToString(bSpeaking), MeterEnergy);

	FChannelSession& ChannelSession = GetChannelSession(ChannelUri);
	FString PlayerName = GetPlayerNameFromUri(ParticipantUri);

	FParticipant& Participant = GetParticipant(PlayerName);
	Participant.bTalking = bSpeaking;

	if (FParticipant* ChannelParticipant = ChannelSession.Participants.Find(PlayerName))
	{
		ChannelParticipant->bTalking = bSpeaking;
		OnVoiceChatPlayerTalkingUpdatedDelegate.Broadcast(ChannelSession.ChannelName, ChannelParticipant->PlayerName, bSpeaking);
	}
}

void FVivoxVoiceChat::onAvailableAudioDevicesChanged()
{
	UE_LOG(LogVivoxVoiceChat, Verbose, TEXT("onAvailableAudioDevicesChanged"));

	OnVoiceChatAvailableAudioDevicesChangedDelegate.Broadcast();
}

void FVivoxVoiceChat::onOperatingSystemChosenAudioInputDeviceChanged(const VivoxClientApi::AudioDeviceId& DeviceId)
{
	UE_LOG(LogVivoxVoiceChat, Verbose, TEXT("onOperatingSystemChosenAudioInputDeviceChanged deviceid:%s"), UTF8_TO_TCHAR(DeviceId.ToString()));
}

void FVivoxVoiceChat::onSetApplicationChosenAudioInputDeviceCompleted(const VivoxClientApi::AudioDeviceId& DeviceId)
{
	UE_LOG(LogVivoxVoiceChat, Verbose, TEXT("onSetApplicationChosenAudioInputDeviceCompleted deviceid:%s"), UTF8_TO_TCHAR(DeviceId.ToString()));
}

void FVivoxVoiceChat::onSetApplicationChosenAudioInputDeviceFailed(const VivoxClientApi::AudioDeviceId& DeviceId, const VivoxClientApi::VCSStatus& Status)
{
	UE_LOG(LogVivoxVoiceChat, Warning, TEXT("onSetApplicationChosenAudioInputDeviceFailed deviceid:%s error:%s (%i)"), UTF8_TO_TCHAR(DeviceId.ToString()), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
}

void FVivoxVoiceChat::onOperatingSystemChosenAudioOutputDeviceChanged(const VivoxClientApi::AudioDeviceId& DeviceId)
{
	UE_LOG(LogVivoxVoiceChat, Verbose, TEXT("onOperatingSystemChosenAudioOutputDeviceChanged deviceid:%s"), UTF8_TO_TCHAR(DeviceId.ToString()));
}

void FVivoxVoiceChat::onSetApplicationChosenAudioOutputDeviceCompleted(const VivoxClientApi::AudioDeviceId& DeviceId)
{
	UE_LOG(LogVivoxVoiceChat, Verbose, TEXT("onSetApplicationChosenAudioOutputDeviceCompleted deviceid:%s"), UTF8_TO_TCHAR(DeviceId.ToString()));
}

void FVivoxVoiceChat::onSetApplicationChosenAudioOutputDeviceFailed(const VivoxClientApi::AudioDeviceId& DeviceId, const VivoxClientApi::VCSStatus& Status)
{
	UE_LOG(LogVivoxVoiceChat, Warning, TEXT("onSetApplicationChosenAudioOutputDeviceFailed deviceid:%s error:%s (%i)"), UTF8_TO_TCHAR(DeviceId.ToString()), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
}

void FVivoxVoiceChat::onSetParticipantAudioOutputDeviceVolumeForMeCompleted(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& TargetUser, const VivoxClientApi::Uri& ChannelUri, int Volume)
{
	UE_LOG(LogVivoxVoiceChat, Verbose, TEXT("onSetParticipantAudioOutputDeviceVolumeForMeCompleted channel:%s user:%s volume:%i"), ANSI_TO_TCHAR(ChannelUri.ToString()), ANSI_TO_TCHAR(TargetUser.ToString()), Volume);

	FChannelSession& ChannelSession = GetChannelSession(ChannelUri);
	FString PlayerName = GetPlayerNameFromUri(TargetUser);
	if (FParticipant* ChannelParticipant = ChannelSession.Participants.Find(PlayerName))
	{
		ChannelParticipant->Volume = static_cast<float>(Volume - VIVOX_MIN_VOL) / static_cast<float>(VIVOX_MAX_VOL - VIVOX_MIN_VOL);
		ChannelParticipant->IntVolume = Volume;
		OnVoiceChatPlayerVolumeUpdatedDelegate.Broadcast(ChannelSession.ChannelName, ChannelParticipant->PlayerName, ChannelParticipant->Volume);
	}
}

void FVivoxVoiceChat::onSetParticipantAudioOutputDeviceVolumeForMeFailed(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& TargetUser, const VivoxClientApi::Uri& ChannelUri, int Volume, const VivoxClientApi::VCSStatus& Status)
{
	UE_LOG(LogVivoxVoiceChat, Warning, TEXT("onSetParticipantAudioOutputDeviceVolumeForMeFailed channel:%s user:%s volume:%i error:%s (%i)"), ANSI_TO_TCHAR(ChannelUri.ToString()), ANSI_TO_TCHAR(TargetUser.ToString()), Volume, ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());

	FChannelSession& ChannelSession = GetChannelSession(ChannelUri);
	FString PlayerName = GetPlayerNameFromUri(TargetUser);
	if (FParticipant* ChannelParticipant = ChannelSession.Participants.Find(PlayerName))
	{
		// TODO: should this retry setting volume?
	}
}

void FVivoxVoiceChat::onSetChannelAudioOutputDeviceVolumeCompleted(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri, int Volume)
{
	UE_LOG(LogVivoxVoiceChat, Verbose, TEXT("onSetChannelAudioOutputDeviceVolumeCompleted channel:%s volume:%i"), ANSI_TO_TCHAR(ChannelUri.ToString()), Volume);
}

void FVivoxVoiceChat::onSetChannelAudioOutputDeviceVolumeFailed(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri, int Volume, const VivoxClientApi::VCSStatus& Status)
{
	UE_LOG(LogVivoxVoiceChat, Warning, TEXT("onSetChannelAudioOutputDeviceVolumeFailed channel:%s volume:%i error:%s (%i)"), ANSI_TO_TCHAR(ChannelUri.ToString()), Volume, ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
}

void FVivoxVoiceChat::onSetParticipantMutedForMeCompleted(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& Target, const VivoxClientApi::Uri& ChannelUri, bool bMuted)
{
	UE_LOG(LogVivoxVoiceChat, Verbose, TEXT("onSetParticipantMutedForMeCompleted channel:%s user:%s muted:%s"), ANSI_TO_TCHAR(ChannelUri.ToString()), ANSI_TO_TCHAR(Target.ToString()), *LexToString(bMuted));

	FChannelSession& ChannelSession = GetChannelSession(ChannelUri);
	FString PlayerName = GetPlayerNameFromUri(Target);
	if (FParticipant* ChannelParticipant = ChannelSession.Participants.Find(PlayerName))
	{
		// TODO: Determine how should this interact with mutes from setting volume to 0
		ChannelParticipant->bMuted = bMuted;
		OnVoiceChatPlayerMuteUpdatedDelegate.Broadcast(ChannelSession.ChannelName, ChannelParticipant->PlayerName, bMuted);
	}
}

void FVivoxVoiceChat::onSetParticipantMutedForMeFailed(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& Target, const VivoxClientApi::Uri& ChannelUri, bool bMuted, const VivoxClientApi::VCSStatus& Status)
{
	UE_LOG(LogVivoxVoiceChat, Warning, TEXT("onSetParticipantMutedForMeFailed channel:%s user:%s muted:%s error:%s (%i)"), ANSI_TO_TCHAR(ChannelUri.ToString()), ANSI_TO_TCHAR(Target.ToString()), *LexToString(bMuted), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());

	FChannelSession& ChannelSession = GetChannelSession(ChannelUri);
	FString PlayerName = GetPlayerNameFromUri(Target);
	if (FParticipant* ChannelParticipant = ChannelSession.Participants.Find(PlayerName))
	{
		// TODO: should this retry mute?
	}
}

void FVivoxVoiceChat::onSetChannelTransmissionToSpecificChannelCompleted(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri)
{
	UE_LOG(LogVivoxVoiceChat, Verbose, TEXT("onSetChannelTransmissionToSpecificChannelCompleted channel:%s"), ANSI_TO_TCHAR(ChannelUri.ToString()));
}

void FVivoxVoiceChat::onSetChannelTransmissionToSpecificChannelFailed(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri, const VivoxClientApi::VCSStatus& Status)
{
	UE_LOG(LogVivoxVoiceChat, Warning, TEXT("onSetChannelTransmissionToSpecificChannelCompleted channel:%s error:%s (%i)"), ANSI_TO_TCHAR(ChannelUri.ToString()), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
}

void FVivoxVoiceChat::onSetChannelTransmissionToAllCompleted(const VivoxClientApi::AccountName& AccountName)
{
	UE_LOG(LogVivoxVoiceChat, Verbose, TEXT("onSetChannelTransmissionToAllCompleted"));
}

void FVivoxVoiceChat::onSetChannelTransmissionToAllFailed(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::VCSStatus& Status)
{
	UE_LOG(LogVivoxVoiceChat, Warning, TEXT("onSetChannelTransmissionToAllFailed error:%s (%i)"), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
}

void FVivoxVoiceChat::onSetChannelTransmissionToNoneCompleted(const VivoxClientApi::AccountName& AccountName)
{
	UE_LOG(LogVivoxVoiceChat, Verbose, TEXT("onSetChannelTransmissionToNoneCompleted"));
}

void FVivoxVoiceChat::onSetChannelTransmissionToNoneFailed(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::VCSStatus& Status)
{
	UE_LOG(LogVivoxVoiceChat, Warning, TEXT("onSetChannelTransmissionToNoneFailed error:%s (%i)"), ANSI_TO_TCHAR(Status.ToString()), Status.GetStatusCode());
}

void FVivoxVoiceChat::onAudioUnitStarted(const VivoxClientApi::Uri& InitialTargetUri)
{

}

void FVivoxVoiceChat::onAudioUnitStopped(const VivoxClientApi::Uri& InitialTargetUri)
{

}

void FVivoxVoiceChat::onAudioUnitAfterCaptureAudioRead(const VivoxClientApi::Uri& InitialTargetUri, short* PcmFrames, int PcmFrameCount, int AudioFrameRate, int ChannelsPerFrame)
{
	if (InitialTargetUri.IsValid())
	{
		FScopeLock Lock(&AfterCaptureAudioReadLock);
		OnVoiceChatAfterCaptureAudioReadDelegate.Broadcast(MakeArrayView(PcmFrames, PcmFrameCount), AudioFrameRate, ChannelsPerFrame);
	}
	else
	{
		FScopeLock Lock(&AudioRecordLock);
		OnVoiceChatRecordSamplesAvailableDelegate.Broadcast(MakeArrayView(PcmFrames, PcmFrameCount), AudioFrameRate, ChannelsPerFrame);
	}
}

void FVivoxVoiceChat::onAudioUnitBeforeCaptureAudioSent(const VivoxClientApi::Uri& InitialTargetUri, short* PcmFrames, int PcmFrameCount, int AudioFrameRate, int ChannelsPerFrame, bool bSpeaking)
{
	if (InitialTargetUri.IsValid())
	{
		FScopeLock Lock(&BeforeCaptureAudioSentLock);
		OnVoiceChatBeforeCaptureAudioSentDelegate.Broadcast(MakeArrayView(PcmFrames, PcmFrameCount), AudioFrameRate, ChannelsPerFrame, bSpeaking);
	}
}

void FVivoxVoiceChat::onAudioUnitBeforeRecvAudioRendered(const VivoxClientApi::Uri& InitialTargetUri, short* PcmFrames, int PcmFrameCount, int AudioFrameRate, int ChannelsPerFrame, bool bSilence)
{
	if (InitialTargetUri.IsValid())
	{
		FScopeLock Lock(&BeforeRecvAudioRenderedLock);
		OnVoiceChatBeforeRecvAudioRenderedDelegate.Broadcast(MakeArrayView(PcmFrames, PcmFrameCount), AudioFrameRate, ChannelsPerFrame, bSilence);
	}
}

void FVivoxVoiceChat::SetVivoxSdkConfigHints(vx_sdk_config_t& Hints)
{
	Hints.pf_malloc_func = &VivoxMalloc;
	Hints.pf_realloc_func = &VivoxRealloc;
	Hints.pf_calloc_func = &VivoxCalloc;
	Hints.pf_malloc_aligned_func = &VivoxMallocAligned;
	Hints.pf_free_func = &VivoxFree;
	Hints.pf_free_aligned_func = &VivoxFreeAligned;

	bool bEnableAudioDucking = false;
	GConfig->GetBool(TEXT("VoiceChat.Vivox"), TEXT("bEnableAudioDucking"), bEnableAudioDucking, GEngineIni);
	Hints.disable_audio_ducking = !bEnableAudioDucking;
}

VivoxClientApi::AccountName FVivoxVoiceChat::CreateAccountName(const FString& PlayerName)
{
	// .Namespace.PlayerName.
	const FString AccountName  = FString::Printf(TEXT(".%s.%s."), *VivoxNamespace, *PlayerName);
	return VivoxClientApi::AccountName(TCHAR_TO_ANSI(*AccountName));
}

FString FVivoxVoiceChat::GetPlayerNameFromAccountName(const VivoxClientApi::AccountName& AccountName)
{
	FString AccountNameString = ANSI_TO_TCHAR(AccountName.ToString());
	// .Namespace.PlayerName.
	const int32 PrefixLength = 1 + VivoxNamespace.Len() + 1; // strlen(".") + VivoxNamespace.Len() + strlen(".")
	const int32 SuffixLength = 1; // strlen(".")
	if (PrefixLength + SuffixLength < AccountNameString.Len())
	{
		return AccountNameString.Mid(PrefixLength, AccountNameString.Len() - PrefixLength - SuffixLength);
	}
	else
	{
		return TEXT("INVALID");
	}
}

VivoxClientApi::Uri FVivoxVoiceChat::CreateUserUri(const FString& PlayerName)
{
	// sip:.Namespace.PlayerName.@Domain
	const FString UserUri = FString::Printf(TEXT("sip:.%s.%s.@%s"), *VivoxNamespace, *PlayerName, *VivoxDomain);
	return VivoxClientApi::Uri(TCHAR_TO_ANSI(*UserUri));
}

FString FVivoxVoiceChat::GetPlayerNameFromUri(const VivoxClientApi::Uri& UserUri)
{
	const FString UserUriString = ANSI_TO_TCHAR(UserUri.ToString());
	// sip:.Namespace.PlayerName.@Domain
	const int32 PrefixLength = 5 + VivoxNamespace.Len() + 1; // strlen("sip:.") + VivoxNamespace.Len() + strlen(".")
	const int32 SuffixLength = 2 + VivoxDomain.Len(); // strlen(".@") + VivoxDomain.Len()
	if (PrefixLength + SuffixLength < UserUriString.Len())
	{
		return UserUriString.Mid(PrefixLength, UserUriString.Len() - PrefixLength - SuffixLength);
	}
	else
	{
		return TEXT("INVALID");
	}
}

VivoxClientApi::Uri FVivoxVoiceChat::CreateChannelUri(const FString& ChannelName, EVoiceChatChannelType ChannelType, TOptional<FVoiceChatChannel3dProperties> Channel3dProperties)
{
	FString ChannelTypeString;
	switch (ChannelType)
	{
	case EVoiceChatChannelType::NonPositional:
		ChannelTypeString = TEXT("g");
		break;
	case EVoiceChatChannelType::Positional:
		ChannelTypeString = TEXT("d");
		break;
	case EVoiceChatChannelType::Echo:
		ChannelTypeString = TEXT("e");
		break;
	default:
		check(false);
		break;
	}

	FString Channel3dPropertiesString;
	if (ChannelType == EVoiceChatChannelType::Positional)
	{
		int AttenuationModelInt;
		switch (Channel3dProperties ? Channel3dProperties->AttenuationModel : AttenuationModel)
		{
		case EVoiceChatAttenuationModel::None:
			AttenuationModelInt = 0;
			break;
		default:
		case EVoiceChatAttenuationModel::InverseByDistance:
			AttenuationModelInt = 1;
			break;
		case EVoiceChatAttenuationModel::LinearByDistance:
			AttenuationModelInt = 2;
			break;
		case EVoiceChatAttenuationModel::ExponentialByDistance:
			AttenuationModelInt = 3;
			break;
		}

		// !MaxDistance-MinDistance-Rolloff-AttenuationModel
		if (Channel3dProperties)
		{
			Channel3dPropertiesString = FString::Printf(TEXT("!p-%i-%i-%.3f-%i"), static_cast<int>(Channel3dProperties->MaxDistance), static_cast<int>(Channel3dProperties->MinDistance), Channel3dProperties->Rolloff, AttenuationModelInt);
		}
		else
		{
			Channel3dPropertiesString = FString::Printf(TEXT("!p-%i-%i-%.3f-%i"), MaxDistance, MinDistance, Rolloff, AttenuationModelInt);
		}
	}

	// sip:confctl-?-Namespace.ChannelName[!3dProperties]@Domain
	const FString ChannelUri = FString::Printf(TEXT("sip:confctl-%s-%s.%s%s@%s"), *ChannelTypeString, *VivoxNamespace, *ChannelName, *Channel3dPropertiesString, *VivoxDomain);

	return VivoxClientApi::Uri(TCHAR_TO_ANSI(*ChannelUri));
}

FString FVivoxVoiceChat::GetChannelNameFromUri(const VivoxClientApi::Uri& ChannelUri)
{
	FString ChannelUriString = ANSI_TO_TCHAR(ChannelUri.ToString());
	// sip:confctl-?-Namespace.ChannelName@Domain
	const int32 PrefixLength = 14 + VivoxNamespace.Len() + 1; // strlen("sip:confctl-?-") + VivoxNamespace.Len() + strlen(".")
	const int32 SuffixLength = 1 + VivoxDomain.Len(); // strlen("@") + VivoxDomain.Len()

	if (PrefixLength + SuffixLength < ChannelUriString.Len())
	{
		FString Channel = ChannelUriString.Mid(PrefixLength, ChannelUriString.Len() - PrefixLength - SuffixLength);
		// strip off 3d properties
		int32 Channel3dParametersIndex = Channel.Find(TEXT("!p-"));
		if (Channel3dParametersIndex != INDEX_NONE)
		{
			Channel = Channel.Left(Channel3dParametersIndex);
		}
		return Channel;
	}
	else
	{
		return TEXT("INVALID");
	}
}

EVoiceChatChannelType FVivoxVoiceChat::GetChannelTypeFromUri(const VivoxClientApi::Uri& ChannelUri)
{
	// sip:confctl-'ChannelType'-...
	switch (ChannelUri.ToString()[12])
	{
	default:
	case 'g':
		return EVoiceChatChannelType::NonPositional;
	case 'd':
		return EVoiceChatChannelType::Positional;
	case 'e':
		return EVoiceChatChannelType::Echo;
	}
}

FVivoxVoiceChat::FParticipant& FVivoxVoiceChat::GetParticipant(const FString& PlayerName)
{
	if (FParticipant* Participant = LoginSession.Participants.Find(PlayerName))
	{
		return *Participant;
	}

	FParticipant& NewParticipant = LoginSession.Participants.Add(PlayerName);
	NewParticipant.PlayerName = PlayerName;
	NewParticipant.UserUri = CreateUserUri(PlayerName);
	return NewParticipant;
}

const FVivoxVoiceChat::FParticipant& FVivoxVoiceChat::GetParticipant(const FString& PlayerName) const
{
	if (const FParticipant* Participant = LoginSession.Participants.Find(PlayerName))
	{
		return *Participant;
	}
	else
	{
		static FParticipant NullParticipant;
		return NullParticipant;
	}
}

FVivoxVoiceChat::FChannelSession& FVivoxVoiceChat::GetChannelSession(const FString& ChannelName)
{
	if (FChannelSession* Session = LoginSession.ChannelSessions.Find(ChannelName))
	{
		return *Session;
	}

	FChannelSession& NewSession = LoginSession.ChannelSessions.Add(ChannelName);
	NewSession.ChannelName = ChannelName;
	NewSession.ChannelType = EVoiceChatChannelType::NonPositional;
	NewSession.ChannelUri = CreateChannelUri(ChannelName, NewSession.ChannelType);
	return NewSession;
}

const FVivoxVoiceChat::FChannelSession& FVivoxVoiceChat::GetChannelSession(const FString& ChannelName) const
{
	if (const FChannelSession* Session = LoginSession.ChannelSessions.Find(ChannelName))
	{
		return *Session;
	}
	else
	{
		static FChannelSession NullSession;
		return NullSession;
	}
}

FVivoxVoiceChat::FChannelSession& FVivoxVoiceChat::GetChannelSession(const VivoxClientApi::Uri& ChannelUri)
{
	FString ChannelName = GetChannelNameFromUri(ChannelUri);
	if (FChannelSession* Session = LoginSession.ChannelSessions.Find(ChannelName))
	{
		return *Session;
	}
	FChannelSession& Session = LoginSession.ChannelSessions.Add(ChannelName);
	Session.ChannelName = ChannelName;
	Session.ChannelType = GetChannelTypeFromUri(ChannelUri);
	Session.ChannelUri = ChannelUri;
	return Session;
}

void FVivoxVoiceChat::RemoveChannelSession(const FString& ChannelName)
{
	// TODO: Should this trigger participant leave delegates?
	LoginSession.ChannelSessions.Remove(ChannelName);
}

void FVivoxVoiceChat::ClearChannelSessions()
{
	// TODO: Should this trigger channel/participant leave delegates?
	LoginSession.ChannelSessions.Reset();
}

void FVivoxVoiceChat::ClearLoginSession()
{
	ClearChannelSessions();
	LoginSession.State = FLoginSession::EState::LoggedOut;
}

bool FVivoxVoiceChat::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("VIVOX")))
	{
		if (FParse::Command(&Cmd, TEXT("INFO")))
		{
			UE_LOG(LogVivoxVoiceChat, Log, TEXT("Initialized: %s"), *LexToString(IsInitialized()));
			if (IsInitialized())
			{
				UE_LOG(LogVivoxVoiceChat, Log, TEXT("  Input Devices: muted:%s volume:%.2f"), *LexToString(GetAudioInputDeviceMuted()), static_cast<float>(GetAudioInputVolume() - VIVOX_MIN_VOL) / static_cast<float>(VIVOX_MAX_VOL - VIVOX_MIN_VOL));
				const FString InputDevice = GetInputDevice();
				const FString DefaultInputDevice = GetDefaultInputDevice();
				if (InputDevice == DefaultInputDevice)
				{
					UE_LOG(LogVivoxVoiceChat, Log, TEXT("    %s (Selected) (Default)"), *DefaultInputDevice);
				}
				else
				{
					UE_LOG(LogVivoxVoiceChat, Log, TEXT("    %s (Selected)"), *InputDevice);
					UE_LOG(LogVivoxVoiceChat, Log, TEXT("    %s (Default)"), *DefaultInputDevice);
				}
				for (const FString& Device : GetAvailableInputDevices())
				{
					if (Device != DefaultInputDevice && Device != InputDevice)
					{
						UE_LOG(LogVivoxVoiceChat, Log, TEXT("    %s"), *Device);
					}
				}

				UE_LOG(LogVivoxVoiceChat, Log, TEXT("  Output Devices: muted:%s volume:%.2f"), *LexToString(GetAudioOutputDeviceMuted()), static_cast<float>(GetAudioOutputVolume() - VIVOX_MIN_VOL) / static_cast<float>(VIVOX_MAX_VOL - VIVOX_MIN_VOL));
				const FString OutputDevice = GetOutputDevice();
				const FString DefaultOutputDevice = GetDefaultOutputDevice();
				if (OutputDevice == DefaultOutputDevice)
				{
					UE_LOG(LogVivoxVoiceChat, Log, TEXT("    %s (Selected) (Default)"), *DefaultOutputDevice);
				}
				else
				{
					UE_LOG(LogVivoxVoiceChat, Log, TEXT("    %s (Selected)"), *OutputDevice);
					UE_LOG(LogVivoxVoiceChat, Log, TEXT("    %s (Default)"), *DefaultOutputDevice);
				}
				for (const FString& Device : GetAvailableOutputDevices())
				{
					if (Device != DefaultOutputDevice && Device != OutputDevice)
					{
						UE_LOG(LogVivoxVoiceChat, Log, TEXT("    %s"), *Device);
					}
				}

				UE_LOG(LogVivoxVoiceChat, Log, TEXT("Connection Status: %s"), *ToString(ConnectionState));
				if (IsConnected())
				{
					UE_LOG(LogVivoxVoiceChat, Log, TEXT("  Server: %s"), *VivoxServerUrl);
					UE_LOG(LogVivoxVoiceChat, Log, TEXT("  Domain: %s"), *VivoxDomain);
					UE_LOG(LogVivoxVoiceChat, Log, TEXT("Login Status: %s"), *ToString(LoginSession.State));
					if (IsLoggedIn())
					{
						UE_LOG(LogVivoxVoiceChat, Log, TEXT("  PlayerName: %s"), *LoginSession.PlayerName);
						UE_LOG(LogVivoxVoiceChat, Log, TEXT("  AccountName: %s"), ANSI_TO_TCHAR(LoginSession.AccountName.ToString()));
						UE_LOG(LogVivoxVoiceChat, Log, TEXT("  UserUri: %s"), ANSI_TO_TCHAR(LoginSession.UserUri.ToString()));

						VivoxClientApi::ChannelTransmissionPolicy TransmissionPolicy = VivoxClientConnection.GetChannelTransmissionPolicy(LoginSession.AccountName);
						FString TransmitString;
						switch (GetTransmitMode())
						{
						case EVoiceChatTransmitMode::All:
							TransmitString = TEXT("ALL");
							break;
						case EVoiceChatTransmitMode::None:
							TransmitString = TEXT("NONE");
							break;
						case EVoiceChatTransmitMode::Channel:
							TransmitString = FString::Printf(TEXT("CHANNEL:%s"), *GetTransmitChannel());
							break;
						}
						UE_LOG(LogVivoxVoiceChat, Log, TEXT("Channels: transmitting:%s"), *TransmitString);
						for (const TPair<FString, FChannelSession>& ChannelSessionPair : LoginSession.ChannelSessions)
						{
							const FString& ChannelName = ChannelSessionPair.Key;
							const FChannelSession& ChannelSession = ChannelSessionPair.Value;
							UE_LOG(LogVivoxVoiceChat, Log, TEXT("  %s"), *ChannelName);
							UE_LOG(LogVivoxVoiceChat, Log, TEXT("    Channel Status: %s"), *ToString(ChannelSession.State));
							UE_LOG(LogVivoxVoiceChat, Log, TEXT("    Channel Uri: %s"), ANSI_TO_TCHAR(ChannelSession.ChannelUri.ToString()));
							UE_LOG(LogVivoxVoiceChat, Log, TEXT("    Participants:"));
							for (const TPair<FString, FParticipant>& ParticipantPair : ChannelSession.Participants)
							{
								const FString& ParticipantName = ParticipantPair.Key;
								const FParticipant& Participant = ParticipantPair.Value;
								UE_LOG(LogVivoxVoiceChat, Log, TEXT("      %s uri:%s talking:%s muted:%s volume:%.2f"), *ParticipantName, ANSI_TO_TCHAR(Participant.UserUri.ToString()), *LexToString(Participant.bTalking), *LexToString(Participant.bMuted), Participant.Volume);
							}
						}
					}
				}
			}
			return true;
		}
#if !UE_BUILD_SHIPPING
		else if (FParse::Command(&Cmd, TEXT("INITIALIZE")))
		{
			Initialize();
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("UNINITIALIZE")))
		{
			Uninitialize();
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("INPUT")))
		{
			if (FParse::Command(&Cmd, TEXT("SETVOLUME")))
			{
				FString Volume;
				if (FParse::Token(Cmd, Volume, false))
				{
					SetAudioInputVolume(FCString::Atof(*Volume));
					return true;
				}
			}
			else if (FParse::Command(&Cmd, TEXT("MUTE")))
			{
				SetAudioInputDeviceMuted(true);
				return true;
			}
			else if (FParse::Command(&Cmd, TEXT("UNMUTE")))
			{
				SetAudioInputDeviceMuted(false);
				return true;
			}
			else if (FParse::Command(&Cmd, TEXT("LISTDEVICES")))
			{
				TArray<FString> Devices = GetAvailableInputDevices();
				UE_LOG(LogVivoxVoiceChat, Log, TEXT("Input Devices:"));
				for (const FString& Device : Devices)
				{
					UE_LOG(LogVivoxVoiceChat, Log, TEXT("  %s"), *Device);
				}
				return true;
			}
			else if (FParse::Command(&Cmd, TEXT("SETDEVICE")))
			{
				FString Device;
				if (FParse::Token(Cmd, Device, false))
				{
					SetInputDevice(Device);
					return true;
				}
			}
			else if (FParse::Command(&Cmd, TEXT("SETDEFAULTDEVICE")))
			{
				SetInputDevice(FString());
				return true;
			}
		}
		else if (FParse::Command(&Cmd, TEXT("OUTPUT")))
		{
			if (FParse::Command(&Cmd, TEXT("SETVOLUME")))
			{
				FString Volume;
				if (FParse::Token(Cmd, Volume, false))
				{
					SetAudioOutputVolume(FCString::Atof(*Volume));
					return true;
				}
			}
			else if (FParse::Command(&Cmd, TEXT("MUTE")))
			{
				SetAudioOutputDeviceMuted(true);
				return true;
			}
			else if (FParse::Command(&Cmd, TEXT("UNMUTE")))
			{
				SetAudioOutputDeviceMuted(false);
				return true;
			}
			else if (FParse::Command(&Cmd, TEXT("LISTDEVICES")))
			{
				TArray<FString> Devices = GetAvailableOutputDevices();
				UE_LOG(LogVivoxVoiceChat, Log, TEXT("Output Devices:"));
				for (const FString& Device : Devices)
				{
					UE_LOG(LogVivoxVoiceChat, Log, TEXT("  %s"), *Device);
				}
				return true;
			}
			else if (FParse::Command(&Cmd, TEXT("SETDEVICE")))
			{
				FString Device;
				if (FParse::Token(Cmd, Device, false))
				{
					SetOutputDevice(Device);
					return true;
				}
			}
			else if (FParse::Command(&Cmd, TEXT("SETDEFAULTDEVICE")))
			{
				SetOutputDevice(FString());
				return true;
			}
		}
		else if (FParse::Command(&Cmd, TEXT("CONNECT")))
		{
			Connect(FOnVoiceChatConnectCompleteDelegate::CreateLambda([](const FVoiceChatResult& Result)
			{
				UE_LOG(LogVivoxVoiceChat, Log, TEXT("VIVOX CONNECT result:%s"), *LexToString(Result));
			}));
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("DISCONNECT")))
		{
			Disconnect(FOnVoiceChatDisconnectCompleteDelegate::CreateLambda([](const FVoiceChatResult& Result)
			{
				UE_LOG(LogVivoxVoiceChat, Log, TEXT("VIVOX DISCONNECT result:%s"), *LexToString(Result));
			}));
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("LOGIN")))
		{
			FString PlayerName;
			if (FParse::Token(Cmd, PlayerName, false))
			{
				FString Token = InsecureGetLoginToken(PlayerName);

				Login(0, PlayerName, Token, FOnVoiceChatLoginCompleteDelegate::CreateLambda([](const FString& LoggedInPlayerName, const FVoiceChatResult& Result)
				{
					UE_LOG(LogVivoxVoiceChat, Log, TEXT("VIVOX LOGIN playername:%s result:%s"), *LoggedInPlayerName, *LexToString(Result));
				}));
				return true;
			}
		}
		else if (FParse::Command(&Cmd, TEXT("LOGOUT")))
		{
			Logout(FOnVoiceChatLogoutCompleteDelegate::CreateLambda([](const FString& PlayerName, const FVoiceChatResult& Result)
			{
				UE_LOG(LogVivoxVoiceChat, Log, TEXT("VIVOX LOGOUT playername:%s result:%s"), *PlayerName, *LexToString(Result));
			}));
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("CHANNEL")))
		{
			if (FParse::Command(&Cmd, TEXT("JOIN")))
			{
				FString ChannelName;
				if (FParse::Token(Cmd, ChannelName, false))
				{
					FString ChannelTypeString;
					EVoiceChatChannelType ChannelType = EVoiceChatChannelType::NonPositional;
					TOptional<FVoiceChatChannel3dProperties> Channel3dProperties;
					if (FParse::Token(Cmd, ChannelTypeString, false))
					{
						if (ChannelTypeString == TEXT("POSITIONAL"))
						{
							ChannelType = EVoiceChatChannelType::Positional;
						}
						else if (ChannelTypeString == TEXT("ECHO"))
						{
							ChannelType = EVoiceChatChannelType::Echo;
						}
					}

					FString Token = InsecureGetJoinToken(ChannelName, ChannelType, Channel3dProperties);

					JoinChannel(ChannelName, Token, ChannelType, FOnVoiceChatChannelJoinCompleteDelegate::CreateLambda([](const FString& JoinedChannelName, const FVoiceChatResult& Result)
					{
						UE_LOG(LogVivoxVoiceChat, Log, TEXT("VIVOX CHANNEL JOIN channelname:%s result:%s"), *JoinedChannelName, *LexToString(Result));
					}), Channel3dProperties);
					return true;
				}
			}
			else if (FParse::Command(&Cmd, TEXT("LEAVE")))
			{
				FString ChannelName;
				if (FParse::Token(Cmd, ChannelName, false))
				{
					LeaveChannel(ChannelName, FOnVoiceChatChannelLeaveCompleteDelegate::CreateLambda([](const FString& LeftChannelName, const FVoiceChatResult& Result)
					{
						UE_LOG(LogVivoxVoiceChat, Log, TEXT("VIVOX CHANNEL LEAVE channelname:%s result:%s"), *LeftChannelName, *LexToString(Result));
					}));
					return true;
				}
			}
			else if (FParse::Command(&Cmd, TEXT("TRANSMIT")))
			{
				FString ChannelName;
				if (FParse::Token(Cmd, ChannelName, false))
				{
					TransmitToSpecificChannel(ChannelName);
					return true;
				}
			}
			else if (FParse::Command(&Cmd, TEXT("TRANSMITALL")))
			{
				TransmitToAllChannels();
				return true;
			}
			else if (FParse::Command(&Cmd, TEXT("TRANSMITNONE")))
			{
				TransmitToNoChannels();
				return true;
			}
		}
		else if (FParse::Command(&Cmd, TEXT("PLAYER")))
		{
			if (FParse::Command(&Cmd, TEXT("MUTE")))
			{
				FString PlayerName;
				if (FParse::Token(Cmd, PlayerName, false))
				{
					SetPlayerMuted(PlayerName, true);
					return true;
				}
			}
			else if (FParse::Command(&Cmd, TEXT("UNMUTE")))
			{
				FString PlayerName;
				if (FParse::Token(Cmd, PlayerName, false))
				{
					SetPlayerMuted(PlayerName, false);
					return true;
				}
			}
			else if (FParse::Command(&Cmd, TEXT("SETVOLUME")))
			{
				FString PlayerName;
				if (FParse::Token(Cmd, PlayerName, false))
				{
					FString Volume;
					if (FParse::Token(Cmd, Volume, false))
					{
						SetPlayerVolume(PlayerName, FCString::Atof(*Volume));
						return true;
					}
				}
			}
			else if (FParse::Command(&Cmd, TEXT("BLOCK")))
			{
				TArray<FString> PlayerNames;
				FString PlayerName;
				while (FParse::Token(Cmd, PlayerName, false))
				{
					PlayerNames.Add(PlayerName);
				}
				if (PlayerNames.Num() > 0)
				{
					BlockPlayers(PlayerNames);
					return true;
				}
			}
			else if (FParse::Command(&Cmd, TEXT("UNBLOCK")))
			{
				TArray<FString> PlayerNames;
				FString PlayerName;
				while (FParse::Token(Cmd, PlayerName, false))
				{
					PlayerNames.Add(PlayerName);
				}
				if (PlayerNames.Num() > 0)
				{
					UnblockPlayers(PlayerNames);
					return true;
				}
			}
		}
#endif // !UE_BUILD_SHIPPING
	}

	return false;
}

FString FVivoxVoiceChat::ToString(EConnectionState State)
{
	switch (State)
	{
	case EConnectionState::Disconnected:	return TEXT("Disconnected");
	case EConnectionState::Disconnecting:	return TEXT("Disconnecting");
	case EConnectionState::Connecting:		return TEXT("Connecting");
	case EConnectionState::Connected:		return TEXT("Connected");
	default:								return TEXT("Unknown");
	}
}

FString FVivoxVoiceChat::ToString(FLoginSession::EState State)
{
	switch (State)
	{
	case FLoginSession::EState::LoggedOut:	return TEXT("LoggedOut");
	case FLoginSession::EState::LoggingOut:	return TEXT("LoggingOut");
	case FLoginSession::EState::LoggingIn:	return TEXT("LoggingIn");
	case FLoginSession::EState::LoggedIn:	return TEXT("LoggedIn");
	default:								return TEXT("Unknown");
	}
}

FString FVivoxVoiceChat::ToString(FChannelSession::EState State)
{
	switch (State)
	{
	case FChannelSession::EState::Disconnected:	return TEXT("Disconnected");
	case FChannelSession::EState::Disconnecting:return TEXT("Disconnecting");
	case FChannelSession::EState::Connecting:	return TEXT("Connecting");
	case FChannelSession::EState::Connected:	return TEXT("Connected");
	default:									return TEXT("Unknown");
	}
}
