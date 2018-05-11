// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 201x Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

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