// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AsyncDestroyer.h"
#include "Engine/Engine.h"
#include "AppEventHandler.h"
#if PLATFORM_LUMIN
#include "Lumin/LuminAffinity.h"
#endif // PLATFORM_LUMIN

namespace MagicLeap
{
	FAsyncDestroyer::FAsyncDestroyer()
		: Thread(nullptr)
		, StopTaskCounter(0)
		, Semaphore(FGenericPlatformProcess::GetSynchEventFromPool(false))
	{
		Semaphore = FGenericPlatformProcess::GetSynchEventFromPool(false);
#if PLATFORM_LUMIN
		Thread = FRunnableThread::Create(this, TEXT("FAsyncDestroyer"), 0, TPri_BelowNormal, FLuminAffinity::GetPoolThreadMask());
#else
		Thread = FRunnableThread::Create(this, TEXT("FAsyncDestroyer"), 0, TPri_BelowNormal);
#endif // PLATFORM_LUMIN
	}

	FAsyncDestroyer::~FAsyncDestroyer()
	{
		StopTaskCounter.Increment();
		Semaphore->Trigger();
		Thread->WaitForCompletion();
		FGenericPlatformProcess::ReturnSynchEventToPool(Semaphore);
		Semaphore = nullptr;
		delete Thread;
		Thread = nullptr;
	}

	uint32 FAsyncDestroyer::Run()
	{
		while (StopTaskCounter.GetValue() == 0)
		{
			IAppEventHandler* EventHandler = nullptr;
			if (IncomingEventHandlers.Dequeue(EventHandler))
			{
				delete EventHandler;
				EventHandler = nullptr;
			}

			Semaphore->Wait();
		}

		return 0;
	}

	void FAsyncDestroyer::AddRaw(IAppEventHandler* InEventHandler)
	{
		IncomingEventHandlers.Enqueue(InEventHandler);
		// wake up the worker to process the event
		Semaphore->Trigger();
	}
} // MagicLeap