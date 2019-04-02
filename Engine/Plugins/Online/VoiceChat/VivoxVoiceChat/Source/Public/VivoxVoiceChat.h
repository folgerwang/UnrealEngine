// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
#include "vivoxclientapi/debugclientapieventhandler.h"
#include "vivoxclientapi/iclientapieventhandler.h"
#include "vivoxclientapi/clientconnection.h"
THIRD_PARTY_INCLUDES_END

#include "VoiceChat.h"
#include "Stats/Stats.h"
#include "Misc/CoreMisc.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVivoxVoiceChat, Log, All);

DECLARE_STATS_GROUP(TEXT("Vivox"), STATGROUP_Vivox, STATCAT_Advanced);

class FVivoxVoiceChat : public FSelfRegisteringExec, public IVoiceChat, protected VivoxClientApi::DebugClientApiEventHandler
{
public:
	FVivoxVoiceChat();
	virtual ~FVivoxVoiceChat();

	// ~Begin IVoiceChat Interface
	virtual bool Initialize() override;
	virtual bool Uninitialize() override;
	virtual bool IsInitialized() const override;
	virtual void SetAudioInputVolume(float Volume) override;
	virtual void SetAudioOutputVolume(float Volume) override;
	virtual float GetAudioInputVolume() const override;
	virtual float GetAudioOutputVolume() const override;
	virtual void SetAudioInputDeviceMuted(bool bIsMuted) override;
	virtual void SetAudioOutputDeviceMuted(bool bIsMuted) override;
	virtual bool GetAudioInputDeviceMuted() const override;
	virtual bool GetAudioOutputDeviceMuted() const override;
	virtual TArray<FString> GetAvailableInputDevices() const override;
	virtual TArray<FString> GetAvailableOutputDevices() const override;
	virtual FOnVoiceChatAvailableAudioDevicesChangedDelegate& OnVoiceChatAvailableAudioDevicesChanged() override { return OnVoiceChatAvailableAudioDevicesChangedDelegate; }
	virtual void SetInputDevice(const FString& InputDevice) override;
	virtual void SetOutputDevice(const FString& OutputDevice) override;
	virtual FString GetInputDevice() const override;
	virtual FString GetOutputDevice() const override;
	virtual FString GetDefaultInputDevice() const override;
	virtual FString GetDefaultOutputDevice() const override;
	virtual void Connect(const FOnVoiceChatConnectCompleteDelegate& Delegate) override;
	virtual void Disconnect(const FOnVoiceChatDisconnectCompleteDelegate& Delegate) override;
	virtual bool IsConnecting() const override;
	virtual bool IsConnected() const override;
	virtual FOnVoiceChatDisconnectedDelegate& OnVoiceChatDisconnected() override { return OnVoiceChatDisconnectedDelegate; }
	virtual FOnVoiceChatReconnectedDelegate& OnVoiceChatReconnected() override { return OnVoiceChatReconnectedDelegate; }
	virtual void Login(FPlatformUserId PlatformId, const FString& PlayerName, const FString& Credentials, const FOnVoiceChatLoginCompleteDelegate& Delegate) override;
	virtual void Logout(const FOnVoiceChatLogoutCompleteDelegate& Delegate) override;
	virtual bool IsLoggingIn() const override;
	virtual bool IsLoggedIn() const override;
	virtual FString GetLoggedInPlayerName() const override;
	virtual void BlockPlayers(const TArray<FString>& PlayerNames) override;
	virtual void UnblockPlayers(const TArray<FString>& PlayerNames) override;
	virtual void JoinChannel(const FString& ChannelName, const FString& ChannelCredentials, EVoiceChatChannelType ChannelType, const FOnVoiceChatChannelJoinCompleteDelegate& Delegate, TOptional<FVoiceChatChannel3dProperties> Channel3dProperties = TOptional<FVoiceChatChannel3dProperties>()) override;
	virtual void LeaveChannel(const FString& Channel, const FOnVoiceChatChannelLeaveCompleteDelegate& Delegate) override;
	virtual FOnVoiceChatChannelExitedDelegate& OnVoiceChatChannelExited() override { return OnVoiceChatChannelExitedDelegate; }
	virtual void Set3DPosition(const FString& ChannelName, const FVector& SpeakerPosition, const FVector& ListenerPosition, const FVector& ListenerForwardDirection, const FVector& ListenerUpDirection) override;
	virtual TArray<FString> GetChannels() const override;
	virtual TArray<FString> GetPlayersInChannel(const FString& ChannelName) const override;
	virtual EVoiceChatChannelType GetChannelType(const FString& ChannelName) const override;
	virtual FOnVoiceChatPlayerAddedDelegate& OnVoiceChatPlayerAdded() override { return OnVoiceChatPlayerAddedDelegate; }
	virtual FOnVoiceChatPlayerRemovedDelegate& OnVoiceChatPlayerRemoved() override { return OnVoiceChatPlayerRemovedDelegate; }
	virtual bool IsPlayerTalking(const FString& PlayerName) const override;
	virtual FOnVoiceChatPlayerTalkingUpdatedDelegate& OnVoiceChatPlayerTalkingUpdated() override { return OnVoiceChatPlayerTalkingUpdatedDelegate; }
	virtual void SetPlayerMuted(const FString& PlayerName, bool bMuted) override;
	virtual bool IsPlayerMuted(const FString& PlayerName) const override;
	virtual FOnVoiceChatPlayerMuteUpdatedDelegate& OnVoiceChatPlayerMuteUpdated() override { return OnVoiceChatPlayerMuteUpdatedDelegate; }
	virtual void SetPlayerVolume(const FString& PlayerName, float Volume) override;
	virtual float GetPlayerVolume(const FString& PlayerName) const override;
	virtual FOnVoiceChatPlayerVolumeUpdatedDelegate& OnVoiceChatPlayerVolumeUpdated() override { return OnVoiceChatPlayerVolumeUpdatedDelegate; }
	virtual void TransmitToAllChannels() override;
	virtual void TransmitToNoChannels() override;
	virtual void TransmitToSpecificChannel(const FString& ChannelName) override;
	virtual EVoiceChatTransmitMode GetTransmitMode() const override;
	virtual FString GetTransmitChannel() const override;
	virtual FDelegateHandle StartRecording(const FOnVoiceChatRecordSamplesAvailableDelegate::FDelegate& Delegate) override;
	virtual void StopRecording(FDelegateHandle Handle) override;
	virtual FDelegateHandle RegisterOnVoiceChatAfterCaptureAudioReadDelegate(const FOnVoiceChatAfterCaptureAudioReadDelegate::FDelegate& Delegate) override;
	virtual void UnregisterOnVoiceChatAfterCaptureAudioReadDelegate(FDelegateHandle Handle) override;
	virtual FDelegateHandle RegisterOnVoiceChatBeforeCaptureAudioSentDelegate(const FOnVoiceChatBeforeCaptureAudioSentDelegate::FDelegate& Delegate) override;
	virtual void UnregisterOnVoiceChatBeforeCaptureAudioSentDelegate(FDelegateHandle Handle) override;
	virtual FDelegateHandle RegisterOnVoiceChatBeforeRecvAudioRenderedDelegate(const FOnVoiceChatBeforeRecvAudioRenderedDelegate::FDelegate& Delegate) override;
	virtual void UnregisterOnVoiceChatBeforeRecvAudioRenderedDelegate(FDelegateHandle Handle) override;
	virtual FString InsecureGetLoginToken(const FString& PlayerName) override;
	virtual FString InsecureGetJoinToken(const FString& ChannelName, EVoiceChatChannelType ChannelType, TOptional<FVoiceChatChannel3dProperties> Channel3dProperties = TOptional<FVoiceChatChannel3dProperties>()) override;
	// ~End IVoiceChat Interface

protected:
	// ~Begin DebugClientApiEventHandler Interface
	virtual void InvokeOnUIThread(void (Func)(void* Arg0), void* Arg0) override;
	virtual void onLogStatementEmitted(LogLevel Level, long long NativeMillisecondsSinceEpoch, long ThreadId, const char* LogMessage) override;
	virtual void onConnectCompleted(const VivoxClientApi::Uri& Server) override;
	virtual void onConnectFailed(const VivoxClientApi::Uri& Server, const VivoxClientApi::VCSStatus& Status) override;
	virtual void onDisconnected(const VivoxClientApi::Uri& Server, const VivoxClientApi::VCSStatus& Status) override;
	virtual void onLoginCompleted(const VivoxClientApi::AccountName& AccountName) override;
	virtual void onInvalidLoginCredentials(const VivoxClientApi::AccountName& AccountName) override;
	virtual void onLoginFailed(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::VCSStatus& Status) override;
	virtual void onLogoutCompleted(const VivoxClientApi::AccountName& AccountName) override;
	virtual void onLogoutFailed(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::VCSStatus& Status) override;
	virtual void onChannelJoined(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri) override;
	virtual void onInvalidChannelCredentials(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri) override;
	virtual void onChannelJoinFailed(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri, const VivoxClientApi::VCSStatus& Status) override;
	virtual void onChannelExited(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri, const VivoxClientApi::VCSStatus& ReasonCode) override;
	virtual void onCallStatsUpdated(const VivoxClientApi::AccountName& AccountName, vx_call_stats_t& Stats, bool bIsFinal) override;
	virtual void onParticipantAdded(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri, const VivoxClientApi::Uri& ParticipantUri, bool bIsLoggedInUser) override;
	virtual void onParticipantLeft(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri, const VivoxClientApi::Uri& ParticipantUri, bool bIsLoggedInUser, ParticipantLeftReason Reason) override;
	virtual void onParticipantUpdated(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri, const VivoxClientApi::Uri& ParticipantUri, bool bIsLoggedInUser, bool bSpeaking, double MeterEnergy, bool bMutedForAll) override;
	virtual void onAvailableAudioDevicesChanged() override;
	virtual void onOperatingSystemChosenAudioInputDeviceChanged(const VivoxClientApi::AudioDeviceId& DeviceId) override;
	virtual void onSetApplicationChosenAudioInputDeviceCompleted(const VivoxClientApi::AudioDeviceId& DeviceId) override;
	virtual void onSetApplicationChosenAudioInputDeviceFailed(const VivoxClientApi::AudioDeviceId& DeviceId, const VivoxClientApi::VCSStatus& Status) override;
	virtual void onOperatingSystemChosenAudioOutputDeviceChanged(const VivoxClientApi::AudioDeviceId& DeviceId) override;
	virtual void onSetApplicationChosenAudioOutputDeviceCompleted(const VivoxClientApi::AudioDeviceId& DeviceId) override;
	virtual void onSetApplicationChosenAudioOutputDeviceFailed(const VivoxClientApi::AudioDeviceId& DeviceId, const VivoxClientApi::VCSStatus& Status) override;
	virtual void onSetParticipantAudioOutputDeviceVolumeForMeCompleted(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& TargetUser, const VivoxClientApi::Uri& ChannelUri, int Volume) override;
	virtual void onSetParticipantAudioOutputDeviceVolumeForMeFailed(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& TargetUser, const VivoxClientApi::Uri& ChannelUri, int Volume, const VivoxClientApi::VCSStatus& Status) override;
	virtual void onSetChannelAudioOutputDeviceVolumeCompleted(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri, int Volume) override;
	virtual void onSetChannelAudioOutputDeviceVolumeFailed(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri, int Volume, const VivoxClientApi::VCSStatus& Status) override;
	virtual void onSetParticipantMutedForMeCompleted(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& Target, const VivoxClientApi::Uri& ChannelUri, bool bMuted) override;
	virtual void onSetParticipantMutedForMeFailed(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& Target, const VivoxClientApi::Uri& ChannelUri, bool bMuted, const VivoxClientApi::VCSStatus& Status) override;
	virtual void onSetChannelTransmissionToSpecificChannelCompleted(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri) override;
	virtual void onSetChannelTransmissionToSpecificChannelFailed(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::Uri& ChannelUri, const VivoxClientApi::VCSStatus& Status) override;
	virtual void onSetChannelTransmissionToAllCompleted(const VivoxClientApi::AccountName& AccountName) override;
	virtual void onSetChannelTransmissionToAllFailed(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::VCSStatus& Status) override;
	virtual void onSetChannelTransmissionToNoneCompleted(const VivoxClientApi::AccountName& AccountName) override;
	virtual void onSetChannelTransmissionToNoneFailed(const VivoxClientApi::AccountName& AccountName, const VivoxClientApi::VCSStatus& Status) override;
	virtual void onAudioUnitStarted(const VivoxClientApi::Uri& InitialTargetUri) override;
	virtual void onAudioUnitStopped(const VivoxClientApi::Uri& InitialTargetUri) override;
	virtual void onAudioUnitAfterCaptureAudioRead(const VivoxClientApi::Uri& InitialTargetUri, short* PcmFrames, int PcmFrameCount, int AudioFrameRate, int ChannelsPerFrame) override;
	virtual void onAudioUnitBeforeCaptureAudioSent(const VivoxClientApi::Uri& InitialTargetUri, short* PcmFrames, int PcmFrameCount, int AudioFrameRate, int ChannelsPerFrame, bool bSpeaking) override;
	virtual void onAudioUnitBeforeRecvAudioRendered(const VivoxClientApi::Uri& InitialTargetUri, short* PcmFrames, int PcmFrameCount, int AudioFrameRate, int ChannelsPerFrame, bool bSilence) override;
	// ~End DebugClientApiEventHandler Interface

	virtual void SetVivoxSdkConfigHints(vx_sdk_config_t& Hints);

	struct FParticipant
	{
		FString PlayerName;
		VivoxClientApi::Uri UserUri;
		bool bTalking = false;
		bool bMuted = false;
		float Volume = 0.5f;
		int IntVolume = 50;
	};

	struct FChannelSession
	{
		FString ChannelName;
		EVoiceChatChannelType ChannelType = EVoiceChatChannelType::NonPositional;
		VivoxClientApi::Uri ChannelUri;
		enum class EState
		{
			Disconnected,
			Disconnecting,
			Connecting,
			Connected
		} State = EState::Disconnected;
		TMap<FString, FParticipant> Participants; // Contains participants in this channel and the current muted/volume/state

		FOnVoiceChatChannelJoinCompleteDelegate JoinDelegate;
		FOnVoiceChatChannelLeaveCompleteDelegate LeaveDelegate;
	};

	struct FLoginSession
	{
		FPlatformUserId PlatformId;
		FString PlayerName;
		VivoxClientApi::AccountName AccountName;
		VivoxClientApi::Uri UserUri;
		enum class EState
		{
			LoggedOut,
			LoggingOut,
			LoggingIn,
			LoggedIn
		} State = EState::LoggedOut;
		TMap<FString, FChannelSession> ChannelSessions;
		TMap<FString, FParticipant> Participants; // Contains participants from all channels and the desired muted/volume state
	};

	enum class EConnectionState
	{
		Disconnected,
		Disconnecting,
		Connecting,
		Connected
	};

	VivoxClientApi::ClientConnection VivoxClientConnection;

	bool bInitialized;
	EConnectionState ConnectionState;
	FLoginSession LoginSession;

	// Settings
	FString VivoxServerUrl;
	FString VivoxDomain;
	FString VivoxIssuer;
	FString VivoxNamespace;
	FString VivoxInsecureSecret;
	EVoiceChatAttenuationModel AttenuationModel;
	int MinDistance;
	int MaxDistance;
	float Rolloff;

	// Delegates
	FOnVoiceChatAvailableAudioDevicesChangedDelegate OnVoiceChatAvailableAudioDevicesChangedDelegate;
	FOnVoiceChatDisconnectedDelegate OnVoiceChatDisconnectedDelegate;
	FOnVoiceChatReconnectedDelegate OnVoiceChatReconnectedDelegate;
	FOnVoiceChatChannelExitedDelegate OnVoiceChatChannelExitedDelegate;
	FOnVoiceChatPlayerAddedDelegate OnVoiceChatPlayerAddedDelegate;
	FOnVoiceChatPlayerTalkingUpdatedDelegate OnVoiceChatPlayerTalkingUpdatedDelegate;
	FOnVoiceChatPlayerMuteUpdatedDelegate OnVoiceChatPlayerMuteUpdatedDelegate;
	FOnVoiceChatPlayerVolumeUpdatedDelegate OnVoiceChatPlayerVolumeUpdatedDelegate;
	FOnVoiceChatPlayerRemovedDelegate OnVoiceChatPlayerRemovedDelegate;

	// Recording Delegates and Critical sections
	FCriticalSection AudioRecordLock;
	FOnVoiceChatRecordSamplesAvailableDelegate OnVoiceChatRecordSamplesAvailableDelegate;
	FCriticalSection AfterCaptureAudioReadLock;
	FOnVoiceChatAfterCaptureAudioReadDelegate OnVoiceChatAfterCaptureAudioReadDelegate;
	FCriticalSection BeforeCaptureAudioSentLock;
	FOnVoiceChatBeforeCaptureAudioSentDelegate OnVoiceChatBeforeCaptureAudioSentDelegate;
	FCriticalSection BeforeRecvAudioRenderedLock;
	FOnVoiceChatBeforeRecvAudioRenderedDelegate OnVoiceChatBeforeRecvAudioRenderedDelegate;

	// Completion delegates
	TArray<FOnVoiceChatConnectCompleteDelegate> OnVoiceChatConnectCompleteDelegates;
	TArray<FOnVoiceChatDisconnectCompleteDelegate> OnVoiceChatDisconnectCompleteDelegates;
	FOnVoiceChatLoginCompleteDelegate OnVoiceChatLoginCompleteDelegate;
	FOnVoiceChatLogoutCompleteDelegate OnVoiceChatLogoutCompleteDelegate;

	VivoxClientApi::AccountName CreateAccountName(const FString& PlayerName);
	FString GetPlayerNameFromAccountName(const VivoxClientApi::AccountName& AccountName);

	VivoxClientApi::Uri CreateUserUri(const FString& PlayerName);
	FString GetPlayerNameFromUri(const VivoxClientApi::Uri& UserUri);

	VivoxClientApi::Uri CreateChannelUri(const FString& ChannelName, EVoiceChatChannelType ChannelType, TOptional<FVoiceChatChannel3dProperties> Channel3dProperties = TOptional<FVoiceChatChannel3dProperties>());
	FString GetChannelNameFromUri(const VivoxClientApi::Uri& ChannelUri);
	EVoiceChatChannelType GetChannelTypeFromUri(const VivoxClientApi::Uri& ChannelUri);

	FParticipant& GetParticipant(const FString& PlayerName);
	const FParticipant& GetParticipant(const FString& PlayerName) const;

	FChannelSession& GetChannelSession(const FString& ChannelName);
	const FChannelSession& GetChannelSession(const FString& ChannelName) const;
	FChannelSession& GetChannelSession(const VivoxClientApi::Uri& ChannelUri);
	void RemoveChannelSession(const FString& ChannelName);
	void ClearChannelSessions();

	void ClearLoginSession();

	// ~Begin FSelfRegisteringExec Interface
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	// ~End FSelfRegisteringExec Interface

	static FString ToString(EConnectionState State);
	static FString ToString(FLoginSession::EState State);
	static FString ToString(FChannelSession::EState State);
};
