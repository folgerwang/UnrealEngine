// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaTimecodeProvider.h"
#include "AjaMediaPrivate.h"
#include "AJA.h"

#include "Misc/App.h"


//~ IAJASyncChannelCallbackInterface implementation
//--------------------------------------------------------------------
// Those are called from the AJA thread. There's a lock inside AJA to prevent this object from dying while in this thread.
struct UAjaTimecodeProvider::FAJACallback : public AJA::IAJASyncChannelCallbackInterface
{
	UAjaTimecodeProvider* Owner;
	FAJACallback(UAjaTimecodeProvider* InOwner)
		: Owner(InOwner)
	{}

	//~ IAJAInputCallbackInterface interface
	virtual void OnInitializationCompleted(bool bSucceed) override
	{
		Owner->State = bSucceed ? ETimecodeProviderSynchronizationState::Synchronized : ETimecodeProviderSynchronizationState::Error;
		if (!bSucceed)
		{
			UE_LOG(LogAjaMedia, Error, TEXT("The initialization of '%s' failed. The TimecodeProvider won't be synchronized."), *Owner->GetName());
		}
	}
};


//~ UAjaTimecodeProvider implementation
//--------------------------------------------------------------------
UAjaTimecodeProvider::UAjaTimecodeProvider(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TimecodeFormat(EAjaMediaTimecodeFormat::LTC)
	, SyncChannel(nullptr)
	, SyncCallback(nullptr)
#if WITH_EDITORONLY_DATA
	, InitializedEngine(nullptr)
	, LastAutoSynchronizeInEditorAppTime(0.0)
#endif
	, State(ETimecodeProviderSynchronizationState::Closed)
{
}

FTimecode UAjaTimecodeProvider::GetTimecode() const
{
	if (SyncChannel)
	{
		if (State == ETimecodeProviderSynchronizationState::Synchronized)
		{
			AJA::FTimecode NewTimecode;
			if (SyncChannel->GetTimecode(NewTimecode))
			{
				return FAja::ConvertAJATimecode2Timecode(NewTimecode, GetFrameRate());
			}
			else
			{
				const_cast<UAjaTimecodeProvider*>(this)->State = ETimecodeProviderSynchronizationState::Error;
			}
		}
	}
	return FTimecode();
}

bool UAjaTimecodeProvider::Initialize(class UEngine* InEngine)
{
#if WITH_EDITORONLY_DATA
	InitializedEngine = nullptr;
#endif

	State = ETimecodeProviderSynchronizationState::Closed;

	if (!FAja::IsInitialized())
	{
		State = ETimecodeProviderSynchronizationState::Error;
		UE_LOG(LogAjaMedia, Warning, TEXT("The TimecodeProvider '%s' can't be initialized. Aja is not initialized on your machine."), *GetName());
		return false;
	}

	const FAjaMediaMode CurrentMediaMode = GetMediaMode();

	FString FailureReason;
	if (!FAjaMediaFinder::IsValid(MediaPort, CurrentMediaMode, FailureReason))
	{
		State = ETimecodeProviderSynchronizationState::Error;

		const bool bAddProjectSettingMessage = MediaPort.IsValid() && !bIsDefaultModeOverriden;
		const TCHAR* OverrideString = bAddProjectSettingMessage ? TEXT("The project settings haven't been set for this port.") : TEXT("");
		UE_LOG(LogAjaMedia, Warning, TEXT("The TimecodeProvider '%s' is invalid. %s %s"), *GetName(), *FailureReason, OverrideString);
		return false;
	}

	check(SyncCallback == nullptr);
	SyncCallback = new FAJACallback(this);

	AJA::AJADeviceOptions DeviceOptions(MediaPort.DeviceIndex);

	AJA::AJASyncChannelOptions Options(*GetName(), MediaPort.PortIndex);
	Options.CallbackInterface = SyncCallback;
	Options.VideoFormatIndex = CurrentMediaMode.VideoFormatIndex;
	Options.TimecodeFormat = AJA::ETimecodeFormat::TCF_None;
	Options.bReadTimecodeFromReferenceIn = false;
	Options.LTCSourceIndex = 1;
	switch(TimecodeFormat)
	{
		case EAjaMediaTimecodeFormat::None:
			Options.TimecodeFormat = AJA::ETimecodeFormat::TCF_None;
			break;
		case EAjaMediaTimecodeFormat::LTC:
			Options.TimecodeFormat = AJA::ETimecodeFormat::TCF_LTC;
			break;
		case EAjaMediaTimecodeFormat::VITC:
			Options.TimecodeFormat = AJA::ETimecodeFormat::TCF_VITC1;
			break;
		default:
			break;
	}

	check(SyncChannel == nullptr);
	SyncChannel = new AJA::AJASyncChannel();
	if (!SyncChannel->Initialize(DeviceOptions, Options))
	{
		State = ETimecodeProviderSynchronizationState::Error;
		ReleaseResources();
		return false;
	}

#if WITH_EDITORONLY_DATA
	InitializedEngine = InEngine;
#endif

	State = ETimecodeProviderSynchronizationState::Synchronizing;
	return true;
}

void UAjaTimecodeProvider::Shutdown(class UEngine* InEngine)
{
#if WITH_EDITORONLY_DATA
	InitializedEngine = nullptr;
#endif

	State = ETimecodeProviderSynchronizationState::Closed;
	ReleaseResources();
}

void UAjaTimecodeProvider::BeginDestroy()
{
	ReleaseResources();
	Super::BeginDestroy();
}

FAjaMediaMode UAjaTimecodeProvider::GetMediaMode() const
{
	FAjaMediaMode CurrentMode;
	if (bIsDefaultModeOverriden == false)
	{
		CurrentMode = GetDefault<UAjaMediaSettings>()->GetInputMediaMode(MediaPort);
	}
	else
	{
		CurrentMode = MediaMode;
	}

	return CurrentMode;
}

void UAjaTimecodeProvider::OverrideMediaMode(const FAjaMediaMode& InMediaMode)
{
	bIsDefaultModeOverriden = true;
	MediaMode = InMediaMode;
}

void UAjaTimecodeProvider::ReleaseResources()
{
	if (SyncChannel)
	{
		SyncChannel->Uninitialize();
		delete SyncChannel;
		SyncChannel = nullptr;

		check(SyncCallback);
		delete SyncCallback;
		SyncCallback = nullptr;
	}
}

ETickableTickType UAjaTimecodeProvider::GetTickableTickType() const
{
#if WITH_EDITORONLY_DATA && WITH_EDITOR
	return ETickableTickType::Conditional;
#endif
	return ETickableTickType::Never;
}

bool UAjaTimecodeProvider::IsTickable() const
{
	return State == ETimecodeProviderSynchronizationState::Error;
}

void UAjaTimecodeProvider::Tick(float DeltaTime)
{
#if WITH_EDITORONLY_DATA && WITH_EDITOR
	if (State == ETimecodeProviderSynchronizationState::Error)
	{
		ReleaseResources();

		// In Editor only, when not in pie, reinitialized the device
		if (InitializedEngine && !GIsPlayInEditorWorld && GIsEditor)
		{
			const double TimeBetweenAttempt = 1.0;
			if (FApp::GetCurrentTime() - LastAutoSynchronizeInEditorAppTime > TimeBetweenAttempt)
			{
				Initialize(InitializedEngine);
				LastAutoSynchronizeInEditorAppTime = FApp::GetCurrentTime();
			}
		}
	}
#endif
}
