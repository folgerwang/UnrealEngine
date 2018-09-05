// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BlackmagicCustomTimeStep.h"
#include "BlackmagicMediaPrivate.h"
#include "BlackmagicHardwareSync.h"

#include "Misc/App.h"

UBlackmagicCustomTimeStep::UBlackmagicCustomTimeStep(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FixedFPS(30)
	, bEnableOverrunDetection(false)
	, AudioChannels(EBlackmagicMediaAudioChannel::Stereo2)
	, Device(nullptr)
	, Port(nullptr)
	, State(ECustomTimeStepSynchronizationState::Closed)
{
}

bool UBlackmagicCustomTimeStep::Initialize(class UEngine* InEngine)
{
	State = ECustomTimeStepSynchronizationState::Closed;

	if (!MediaPort.IsValid())
	{
		State = ECustomTimeStepSynchronizationState::Error;
		UE_LOG(LogBlackmagicMedia, Warning, TEXT("The Source of '%s' is not valid."), *GetName());
		return false;
	}

	Device = BlackmagicDevice::VideoIOCreateDevice(MediaPort.DeviceIndex);
	if (!Device)
	{
		State = ECustomTimeStepSynchronizationState::Error;
		UE_LOG(LogBlackmagicMedia, Warning, TEXT("The Blackmagic Device for '%s' could not be created."), *GetName());
		return false;
	}

	const uint32_t PortIndex = MediaPort.PortIndex;
	
	BlackmagicDevice::FFrameDesc FrameDesc;
	// Blackmagic requires YUV for input
	FrameDesc.PixelFormat = BlackmagicDevice::EPixelFormat::PF_UYVY;

	BlackmagicDevice::FPortOptions Options = {};
	Options.bUseTimecode = true;

	if (AudioChannels == EBlackmagicMediaAudioChannel::Surround8)
	{
		Options.AudioChannels = 8;
	}
	else
	{
		Options.AudioChannels = 2;
	}

	Port = BlackmagicDevice::VideoIODeviceOpenSharedPort(Device, PortIndex, FrameDesc, Options);
	
	if (!Port)
	{
		State = ECustomTimeStepSynchronizationState::Error;
		UE_LOG(LogBlackmagicMedia, Warning, TEXT("The Blackmagic port for '%s' could not be opened."), *GetName());
		BlackmagicDevice::VideoIOReleaseDevice(Device);
		Device = nullptr;
		return false;
	}

	if (bEnableOverrunDetection)
	{
		TSharedPtr<IMediaIOCoreHardwareSync> HardwareSync = MakeShared<FBlackmagicHardwareSync>(Port);
		VSyncThread = MakeUnique<FMediaIOCoreWaitVSyncThread>(HardwareSync);
		VSyncRunnableThread.Reset(FRunnableThread::Create(VSyncThread.Get(), TEXT("UBlackmagicCustomTimeStep::FBlackmagicMediaWaitVSyncThread"), TPri_AboveNormal));
	}

	State = ECustomTimeStepSynchronizationState::Synchronizing;
	return true;
}

void UBlackmagicCustomTimeStep::Shutdown(class UEngine* InEngine)
{
	State = ECustomTimeStepSynchronizationState::Closed;

	if (VSyncRunnableThread.IsValid())
	{
		check(VSyncThread.IsValid());
		VSyncThread->Stop(); 
		VSyncRunnableThread->WaitForCompletion();  // Wait for the thread to return.
		VSyncRunnableThread.Reset();
		VSyncThread.Reset();
	}

	if (Port)
	{
		Port->Release();
		Port = nullptr;
	}

	if (Device)
	{
		BlackmagicDevice::VideoIOReleaseDevice(Device);
		Device = nullptr;
	}
}

bool UBlackmagicCustomTimeStep::UpdateTimeStep(class UEngine* InEngine)
{
	bool bRunEngineTimeStep = true;
	if (Port && (State == ECustomTimeStepSynchronizationState::Synchronized || State == ECustomTimeStepSynchronizationState::Synchronizing))
	{
		WaitForVSync();
	
		// Updates logical last time to match logical current time from last tick
		FApp::UpdateLastTime();

		// Use fixed delta time and update time.
		const float FrameRate = 1.f / FixedFPS;
		FApp::SetDeltaTime(FrameRate);
		FApp::SetCurrentTime(FPlatformTime::Seconds());
		bRunEngineTimeStep = false;

		State = ECustomTimeStepSynchronizationState::Synchronized;
	}

	return bRunEngineTimeStep;
}

ECustomTimeStepSynchronizationState UBlackmagicCustomTimeStep::GetSynchronizationState() const
{
	return State;
}

void UBlackmagicCustomTimeStep::WaitForVSync() const
{
	if (VSyncThread.IsValid())
	{
		VSyncThread->Wait_GameOrRenderThread();
	}
	else
	{
		Port->WaitVSync();
	}
}


