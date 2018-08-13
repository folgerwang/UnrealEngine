// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaCustomTimeStep.h"
#include "AjaMediaPrivate.h"
#include "AJA.h"

#include "HAL/CriticalSection.h"
#include "HAL/Event.h"

#include "Misc/App.h"
#include "Misc/ScopeLock.h"


//~ IAJASyncChannelCallbackInterface implementation
//--------------------------------------------------------------------
// Those are called from the AJA thread. There's a lock inside AJA to prevent this object from dying while in this thread.
struct UAjaCustomTimeStep::FAJACallback : public AJA::IAJASyncChannelCallbackInterface
{
	UAjaCustomTimeStep* Owner;
	FAJACallback(UAjaCustomTimeStep* InOwner)
		: Owner(InOwner)
	{}

	//~ IAJAInputCallbackInterface interface
	virtual void OnInitializationCompleted(bool bSucceed) override
	{
		Owner->State = bSucceed ? ECustomTimeStepSynchronizationState::Synchronized : ECustomTimeStepSynchronizationState::Error;
		if (!bSucceed)
		{
			UE_LOG(LogAjaMedia, Error, TEXT("The initialization of '%s' failed. The CustomTimeStep won't be synchronized."), *Owner->GetName());
		}
	}
};


//~ UFixedFrameRateCustomTimeStep implementation
//--------------------------------------------------------------------
UAjaCustomTimeStep::UAjaCustomTimeStep(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bUseReferenceIn(false)
	, TimecodeFormat(EAjaMediaTimecodeFormat::LTC)
	, bEnableOverrunDetection(false)
	, SyncChannel(nullptr)
	, SyncCallback(nullptr)
	, State(ECustomTimeStepSynchronizationState::Closed)
#if WITH_EDITORONLY_DATA
	, InitializedEngine(nullptr)
	, LastAutoSynchronizeInEditorAppTime(0.0)
#endif
	, bDidAValidUpdateTimeStep(false)
	, bWarnedAboutVSync(false)
{
}

bool UAjaCustomTimeStep::Initialize(UEngine* InEngine)
{
#if WITH_EDITORONLY_DATA
	InitializedEngine = nullptr;
#endif

	State = ECustomTimeStepSynchronizationState::Closed;
	bDidAValidUpdateTimeStep = false;

	if (!FAja::IsInitialized())
	{
		State = ECustomTimeStepSynchronizationState::Error;
		UE_LOG(LogAjaMedia, Warning, TEXT("The CustomTimeStep '%s' can't be initialized. Aja is not initialized on your machine."), *GetName());
		return false;
	}

	const FAjaMediaMode CurrentMediaMode = GetMediaMode();

	FString FailureReason;
	if (!FAjaMediaFinder::IsValid(MediaPort, CurrentMediaMode, FailureReason))
	{
		State = ECustomTimeStepSynchronizationState::Error;

		const bool bAddProjectSettingMessage = MediaPort.IsValid() && !bIsDefaultModeOverriden;
		const FString OverrideString = bAddProjectSettingMessage ? TEXT("The project settings haven't been set for this port.") : TEXT("");
		UE_LOG(LogAjaMedia, Warning, TEXT("The CustomTimeStep '%s' is invalid. %s %s"), *GetName(), *FailureReason, *OverrideString);
		return false;
	}

	check(SyncCallback == nullptr);
	SyncCallback = new FAJACallback(this);

	AJA::AJADeviceOptions DeviceOptions(MediaPort.DeviceIndex);

	//Convert Port Index to match what AJA expects
	AJA::AJASyncChannelOptions Options(*GetName(), MediaPort.PortIndex);
	Options.CallbackInterface = SyncCallback;
	Options.VideoFormatIndex = CurrentMediaMode.VideoFormatIndex;
	Options.bOutput = bUseReferenceIn;

	Options.TimecodeFormat = AJA::ETimecodeFormat::TCF_None;
	if (!Options.bOutput)
	{
		switch (TimecodeFormat)
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
	}

	check(SyncChannel == nullptr);
	SyncChannel = new AJA::AJASyncChannel();
	if (!SyncChannel->Initialize(DeviceOptions, Options))
	{
		State = ECustomTimeStepSynchronizationState::Error;
		delete SyncChannel;
		SyncChannel = nullptr;
		delete SyncCallback;
		SyncCallback = nullptr;
		return false;
	}

#if WITH_EDITORONLY_DATA
	InitializedEngine = InEngine;
#endif

	State = ECustomTimeStepSynchronizationState::Synchronizing;
	return true;
}

void UAjaCustomTimeStep::Shutdown(UEngine* InEngine)
{
#if WITH_EDITORONLY_DATA
	InitializedEngine = nullptr;
#endif

	State = ECustomTimeStepSynchronizationState::Closed;
	ReleaseResources();
}

bool UAjaCustomTimeStep::UpdateTimeStep(UEngine* InEngine)
{
	bool bRunEngineTimeStep = true;
	if (State == ECustomTimeStepSynchronizationState::Synchronized)
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VSync"));
		if (!bWarnedAboutVSync)
		{
			bool bLockToVsync = CVar->GetValueOnGameThread() != 0;
			if (bLockToVsync)
			{
				UE_LOG(LogAjaMedia, Warning, TEXT("The Engine is using VSync and the AJACustomTimeStep. It may break the 'genlock'."));
				bWarnedAboutVSync = true;
			}
		}

		// Updates logical last time to match logical current time from last tick
		UpdateApplicationLastTime();

		WaitForSync();

		// Use fixed delta time and update time.
		FApp::SetDeltaTime(GetFixedFrameRate().AsInterval());
		FApp::SetCurrentTime(FApp::GetCurrentTime() + FApp::GetDeltaTime());

		bRunEngineTimeStep = false;
		bDidAValidUpdateTimeStep = true;
	}
	else if (State == ECustomTimeStepSynchronizationState::Error)
	{
		ReleaseResources();

		// In Editor only, when not in pie, reinitialized the device
#if WITH_EDITORONLY_DATA && WITH_EDITOR
		if (InitializedEngine && !GIsPlayInEditorWorld && GIsEditor)
		{
			const double TimeBetweenAttempt = 1.0;
			if (FApp::GetCurrentTime() - LastAutoSynchronizeInEditorAppTime > TimeBetweenAttempt)
			{
				Initialize(InitializedEngine);
				LastAutoSynchronizeInEditorAppTime = FApp::GetCurrentTime();
			}
		}
#endif
	}

	return bRunEngineTimeStep;
}

ECustomTimeStepSynchronizationState UAjaCustomTimeStep::GetSynchronizationState() const
{
	if (State == ECustomTimeStepSynchronizationState::Synchronized)
	{
		return bDidAValidUpdateTimeStep ? ECustomTimeStepSynchronizationState::Synchronized : ECustomTimeStepSynchronizationState::Synchronizing;
	}
	return State;
}

FFrameRate UAjaCustomTimeStep::GetFixedFrameRate() const
{
	return MediaMode.FrameRate;
}

//~ UObject implementation
//--------------------------------------------------------------------
void UAjaCustomTimeStep::BeginDestroy()
{
	ReleaseResources();
	Super::BeginDestroy();
}

FAjaMediaMode UAjaCustomTimeStep::GetMediaMode() const
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

void UAjaCustomTimeStep::OverrideMediaMode(const FAjaMediaMode& InMediaMode)
{
	bIsDefaultModeOverriden = true;
	MediaMode = InMediaMode;
}

//~ UAjaCustomTimeStep implementation
//--------------------------------------------------------------------
void UAjaCustomTimeStep::WaitForSync()
{
	check(SyncChannel);

	if (bEnableOverrunDetection && !VSyncThread.IsValid())
	{
		TSharedPtr<IMediaIOCoreHardwareSync> HardwareSync = MakeShared<FAjaHardwareSync>(SyncChannel);
		VSyncThread = MakeUnique<FMediaIOCoreWaitVSyncThread>(HardwareSync);
		VSyncRunnableThread.Reset(FRunnableThread::Create(VSyncThread.Get(), TEXT("UAjaCustomTimeStep::FAjaMediaWaitVSyncThread"), TPri_AboveNormal));
	}

	bool bWaitIsValid = true;
	if (VSyncThread.IsValid())
	{
		bWaitIsValid = VSyncThread->Wait_GameOrRenderThread();
	}
	else
	{
		AJA::FTimecode NewTimecode;
		bWaitIsValid = SyncChannel->WaitForSync(NewTimecode);
	}

	if (!bWaitIsValid)
	{
		State = ECustomTimeStepSynchronizationState::Error;
		UE_LOG(LogAjaMedia, Error, TEXT("The Engine couldn't run fast enough to keep up with the CustomTimeStep Sync. The wait timeout."));
	}
}

void UAjaCustomTimeStep::ReleaseResources()
{
	if (VSyncRunnableThread.IsValid())
	{
		check(VSyncThread.IsValid());
		VSyncThread->Stop();
		VSyncRunnableThread->WaitForCompletion();  // Wait for the thread to return.
		VSyncRunnableThread.Reset();
		VSyncThread.Reset();
	}

	if (SyncChannel)
	{
		SyncChannel->Uninitialize();
		delete SyncChannel;
		SyncChannel = nullptr;
		delete SyncCallback;
		SyncCallback = nullptr;
	}

	bWarnedAboutVSync = false;
}

