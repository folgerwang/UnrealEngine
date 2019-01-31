// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
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

#pragma once

#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeBool.h"
#include "HAL/ThreadSafeCounter64.h"
#include "HAL/Event.h"
#include "Containers/Queue.h"
#include "Containers/Array.h"
#include "Misc/Variant.h"
#include "AppEventHandler.h"
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END
#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_api.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

struct FMagicLeapTask
{
	bool bSuccess;

	FMagicLeapTask()
	: bSuccess(false)
	{
	}
};

template <class TTaskType>
class FMagicLeapRunnable : public FRunnable
{
public:
#if WITH_MLSDK
	FMagicLeapRunnable(const TArray<MLPrivilegeID>& InRequiredPrivileges, const FString& InName)
	: AppEventHandler(InRequiredPrivileges)
#else
	FMagicLeapRunnable(const FString& InName)
	: AppEventHandler()
#endif //WITH_MLSDK
	, Thread(nullptr)
	, StopTaskCounter(0)
	, Semaphore(nullptr)
	, bPaused(false)
	{
		Semaphore = FGenericPlatformProcess::GetSynchEventFromPool(false);
#if PLATFORM_LUMIN
		Thread = FRunnableThread::Create(this, *InName, 0, TPri_BelowNormal, FLuminAffinity::GetPoolThreadMask());
#else
		Thread = FRunnableThread::Create(this, *InName, 0, TPri_BelowNormal);
#endif // PLATFORM_LUMIN

		AppEventHandler.SetOnAppPauseHandler([this]() 
		{
			OnAppPause();
		});

		AppEventHandler.SetOnAppResumeHandler([this]()
		{
			OnAppResume();
		});

		AppEventHandler.SetOnAppShutDownHandler([this]()
		{
			OnAppShutDown();
		});
	}

	virtual ~FMagicLeapRunnable()
	{
		StopTaskCounter.Increment();

		if (Semaphore)
		{
			Semaphore->Trigger();
			Thread->WaitForCompletion();
			FGenericPlatformProcess::ReturnSynchEventToPool(Semaphore);
			Semaphore = nullptr;
		}

		delete Thread;
		Thread = nullptr;
	}

	uint32 Run() override
	{
		while (StopTaskCounter.GetValue() == 0)
		{
			if (bPaused)
			{
				Pause();
				// Cancel any incoming tasks.
				CancelIncomingTasks();
				// Wait for signal from resume call.
				Semaphore->Wait();
				Resume();
			}
			else if (!IncomingTasks.IsEmpty())
			{
				DoNextTask();
			}
			else
			{
				Semaphore->Wait();
			}
		}

		return 0;
	}

	void OnAppPause()
	{
		bPaused = true;
		Semaphore->Trigger();
	}

	void OnAppResume()
	{
		bPaused = false;
		Semaphore->Trigger();
	}

	void OnAppShutDown()
	{
		Stop();
	}

	void PushNewTask(TTaskType InTask)
	{
		IncomingTasks.Enqueue(InTask);
		// wake up the worker to process the task
		Semaphore->Trigger();
	}

	void PushCompletedTask(TTaskType InTask)
	{
		CompletedTasks.Enqueue(InTask);
	}

	bool TryGetCompletedTask(TTaskType& OutCompletedTask)
	{
#if PLATFORM_LUMIN
		if (CompletedTasks.Peek(OutCompletedTask))
		{
			CompletedTasks.Pop();
			return true;
		}
#endif //PLATFORM_LUMIN
		return false;
	}

protected:
	void CancelIncomingTasks()
	{
		while (IncomingTasks.Dequeue(CurrentTask))
		{
			CurrentTask.bSuccess = false;
			CompletedTasks.Enqueue(CurrentTask);
		}
	}

	virtual bool ProcessCurrentTask() = 0;
	virtual void Pause() {}
	virtual void Resume() {}

	/** Internal thread this instance is running on */
	MagicLeap::IAppEventHandler AppEventHandler;
	FRunnableThread* Thread;
	FThreadSafeCounter StopTaskCounter;
	FEvent* Semaphore;
	FThreadSafeBool bPaused;
	TQueue<TTaskType, EQueueMode::Spsc> IncomingTasks;
	TQueue<TTaskType, EQueueMode::Spsc> CompletedTasks;
	TTaskType CurrentTask;

private:
	bool DoNextTask()
	{
		IncomingTasks.Dequeue(CurrentTask);
		CurrentTask.bSuccess = ProcessCurrentTask();

		if (bPaused)
		{
			return false;
		}

		CompletedTasks.Enqueue(CurrentTask);

		return CurrentTask.bSuccess;
	}
};
