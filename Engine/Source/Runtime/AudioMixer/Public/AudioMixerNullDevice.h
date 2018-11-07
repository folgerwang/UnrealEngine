// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

namespace Audio
{
	/**
	 * FMixerNullCallback
	 * This class, when started, spawns a new high priority thread that exists to query an FAudioMixerPlatformInterface
	 * and immediately throw out whatever buffers it receives.
	 */
	class FMixerNullCallback : protected FRunnable
	{
	public:

		/**
		 * Constructing the FMixerNullCallback immediately begins calling
		 * InCallback every BufferDuration seconds.
		 */
		FMixerNullCallback(float BufferDuration, TFunction<void()> InCallback);

		/**
		 * The destructor waits on Callback to be completed before stopping the thread.
		 */
		~FMixerNullCallback();

		// FRunnable override:
		virtual uint32 Run() override;

	private:

		// Default constructor intentionally suppressed:
		FMixerNullCallback();

		// Callback used.
		TFunction<void()> Callback;

		// Used to determine amount of time we should wait between callbacks.
		float CallbackTime;

		// Flagged on destructor.
		uint8 bShouldShutdown:1;

		FRunnableThread* CallbackThread;
		
	};
}

