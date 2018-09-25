// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AudioMixerNullDevice.h"
#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"

namespace Audio
{
	uint32 FMixerNullCallback::Run()
	{
		while (!bShouldShutdown)
		{
			Callback();
			FPlatformProcess::Sleep(CallbackTime);
		}

		return 0;
	}

	FMixerNullCallback::FMixerNullCallback(float BufferDuration, TFunction<void()> InCallback)
		: Callback(InCallback)
		, CallbackTime(BufferDuration)
		, bShouldShutdown(false)
	{
		CallbackThread = FRunnableThread::Create(this, TEXT("AudioMixerNullCallbackThread"), 0, TPri_TimeCritical, FPlatformAffinity::GetAudioThreadMask());
	}

	FMixerNullCallback::~FMixerNullCallback()
	{
		bShouldShutdown = true;
		CallbackThread->Kill(true);
	}
}
