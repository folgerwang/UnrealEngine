// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeCounter.h"
#include "Containers/Queue.h"

#include "ml_api.h"

class FMagicLeapMediaCodecPlayer;

enum class EInputWorkerTaskType : uint8
{
	None,
	Seek
};

struct FInputWorkerTask
{
public:
	EInputWorkerTaskType TaskType;
	FTimespan SeekTime;

public:
	FInputWorkerTask()
	: TaskType(EInputWorkerTaskType::None)
	, SeekTime(FTimespan::Zero())
	{}

	FInputWorkerTask(EInputWorkerTaskType InTaskType, const FTimespan& InSeekTime)
	: TaskType(InTaskType)
	, SeekTime(InSeekTime)
	{}
};

class FMediaCodecInputWorker : public FRunnable
{
public:
	FMediaCodecInputWorker();
	virtual ~FMediaCodecInputWorker();

	void InitThread(FMagicLeapMediaCodecPlayer& InOwnerPlayer, MLHandle& InExtractorHandle, FCriticalSection& InCriticalSection, FCriticalSection& InGT_IT_Mutex, FCriticalSection& InRT_IT_Mutex);
	void DestroyThread();
	virtual uint32 Run() override;

	void WakeUp();
	void Seek(FTimespan SeekTime);

	bool HasReachedInputEOS() const;

private:
	void ProcessInputSample_WorkerThread();
	bool Seek_WorkerThread(const FTimespan& SeekTime);

	FMagicLeapMediaCodecPlayer* OwnerPlayer;
	MLHandle* ExtractorHandle;
	FCriticalSection* CriticalSection;
	FCriticalSection* GT_IT_Mutex;
	FCriticalSection* RT_IT_Mutex;

	FRunnableThread* Thread;
	FEvent* Semaphore;
	FThreadSafeCounter StopTaskCounter;

	bool bReachedInputEndOfStream;

	TQueue<FInputWorkerTask, EQueueMode::Spsc> IncomingTasks;
};
