// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MagicLeapScreensMsg.h"
#include "Async/Async.h"
#include "Containers/Queue.h"
#include "MagicLeapHMD.h"
#include "MagicLeapUtils.h"
#include "MagicLeapScreensPlugin.h"

class FScreensWorker : public FRunnable, public MagicLeap::IAppEventHandler
{
public:
	FScreensWorker()
		: Thread(nullptr)
		, StopTaskCounter(0)
		, Semaphore(nullptr)
	{}

	~FScreensWorker()
	{
		StopTaskCounter.Increment();
		if (Semaphore != nullptr)
		{
			Semaphore->Trigger();
			Thread->WaitForCompletion();
			FGenericPlatformProcess::ReturnSynchEventToPool(Semaphore);
			Semaphore = nullptr;
			delete Thread;
			Thread = nullptr;
		}
	}

	void EngineInited()
	{
		if (Semaphore == nullptr)
		{
			Semaphore = FGenericPlatformProcess::GetSynchEventFromPool(false);
#if PLATFORM_LUMIN
			Thread = FRunnableThread::Create(this, TEXT("FScreensWorker"), 0, TPri_BelowNormal, FLuminAffinity::GetPoolThreadMask());
#else
			Thread = FRunnableThread::Create(this, TEXT("FScreensWorker"), 0, TPri_BelowNormal);
#endif // PLATFORM_LUMIN
		}
		// wake up the worker to process the event
		Semaphore->Trigger();
	}

	virtual uint32 Run() override
	{
		while (StopTaskCounter.GetValue() == 0)
		{
			if (IncomingMessages.Dequeue(CurrentMessage))
			{
				DoScreensTasks();
			}
			else
			{
				Semaphore->Wait();
			}
		}

		return 0;
	}

	void ProcessMessage(const FScreensMessage& InMsg)
	{
		IncomingMessages.Enqueue(InMsg);
		if (Semaphore != nullptr)
		{
			// wake up the worker to process the event
			Semaphore->Trigger();
		}
	}

	void DoScreensTasks()
	{
		switch (CurrentMessage.TaskType)
		{
		case EScreensTaskType::None: break;
		case EScreensTaskType::GetHistory: GetWatchHistory(); break;
		case EScreensTaskType::AddToHistory: AddToHistory();  break;
		case EScreensTaskType::UpdateEntry: UpdateWatchHistoryEntry(); break;
		}
	}

	void AddToHistory()
	{
		check(CurrentMessage.WatchHistory.Num() != 0);
		FScreensMessage Msg = FMagicLeapScreensPlugin::AddToWatchHistory(CurrentMessage.WatchHistory[0]);
		Msg.EntryDelegate = CurrentMessage.EntryDelegate;
		check(Msg.WatchHistory.Num() > 0);
		OutgoingMessages.Enqueue(Msg);
	}

	void UpdateWatchHistoryEntry()
	{
		check(CurrentMessage.WatchHistory.Num() != 0);
		FScreensMessage Msg = FMagicLeapScreensPlugin::UpdateWatchHistoryEntry(CurrentMessage.WatchHistory[0]);
		Msg.EntryDelegate = CurrentMessage.EntryDelegate;
		check(Msg.WatchHistory.Num() > 0);
		OutgoingMessages.Enqueue(Msg);
	}

	void GetWatchHistory()
	{
		FScreensMessage Msg = FMagicLeapScreensPlugin::GetWatchHistoryEntries();
		Msg.HistoryDelegate = CurrentMessage.HistoryDelegate;
		OutgoingMessages.Enqueue(Msg);
	}

	FRunnableThread* Thread;
	FThreadSafeCounter StopTaskCounter;
	TQueue<FScreensMessage, EQueueMode::Spsc> IncomingMessages;
	TQueue<FScreensMessage, EQueueMode::Spsc> OutgoingMessages;
	FEvent* Semaphore;
	FScreensMessage CurrentMessage;
};
