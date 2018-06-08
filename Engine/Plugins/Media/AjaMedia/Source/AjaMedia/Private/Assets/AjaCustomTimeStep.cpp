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
	, bEnableOverrunDetection(false)
	, SyncChannel(nullptr)
	, SyncCallback(nullptr)
	, State(ECustomTimeStepSynchronizationState::Closed)
	, bDidAValidUpdateTimeStep(false)
{
}

bool UAjaCustomTimeStep::Initialize(class UEngine* InEngine)
{
	State = ECustomTimeStepSynchronizationState::Closed;
	bDidAValidUpdateTimeStep = false;

	if (!MediaPort.IsValid())
	{
		UE_LOG(LogAjaMedia, Warning, TEXT("The Source of '%s' is not valid."), *GetName());
		State = ECustomTimeStepSynchronizationState::Error;
		return false;
	}

	check(SyncCallback == nullptr);
	SyncCallback = new FAJACallback(this);

	AJA::AJADeviceOptions DeviceOptions(MediaPort.DeviceIndex);

	//Convert Port Index to match what AJA expects
	AJA::AJASyncChannelOptions Options(*GetName(), MediaPort.PortIndex);
	Options.CallbackInterface = SyncCallback;
	Options.bUseTimecode = false;

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

	State = ECustomTimeStepSynchronizationState::Synchronizing;
	return true;
}

void UAjaCustomTimeStep::Shutdown(class UEngine* InEngine)
{
	State = ECustomTimeStepSynchronizationState::Closed;
	ReleaseResources();
}

bool UAjaCustomTimeStep::UpdateTimeStep(class UEngine* InEngine)
{
	bool bRunEngineTimeStep = true;
	if (State == ECustomTimeStepSynchronizationState::Synchronized)
	{
		// Updates logical last time to match logical current time from last tick
		UpdateApplicationLastTime();

		WaitForSync();

		// Use fixed delta time and update time.
		FApp::SetDeltaTime(FixedFrameRate.AsInterval());
		FApp::SetCurrentTime(FApp::GetCurrentTime() + FApp::GetDeltaTime());

		bRunEngineTimeStep = false;
		bDidAValidUpdateTimeStep = true;
	}
	else if (State == ECustomTimeStepSynchronizationState::Error)
	{
		ReleaseResources();
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

//~ UObject implementation
//--------------------------------------------------------------------
void UAjaCustomTimeStep::BeginDestroy()
{
	ReleaseResources();
	Super::BeginDestroy();
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
}

