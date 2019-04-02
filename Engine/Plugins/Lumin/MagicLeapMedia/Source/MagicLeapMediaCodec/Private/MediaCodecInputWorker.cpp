// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MediaCodecInputWorker.h"
#include "Misc/ScopeLock.h"
#include "HAL/RunnableThread.h"
#include "HAL/Event.h"
#include "MagicLeapMediaCodecPlayer.h"
#include "Lumin/LuminAffinity.h"
#include "IMagicLeapMediaCodecModule.h"

#include "ml_media_extractor.h"
#include "ml_media_codec.h"
#include "ml_media_error.h"

FMediaCodecInputWorker::FMediaCodecInputWorker()
: OwnerPlayer(nullptr)
, ExtractorHandle(nullptr)
, CriticalSection(nullptr)
, GT_IT_Mutex(nullptr)
, RT_IT_Mutex(nullptr)
, Thread(nullptr)
, Semaphore(nullptr)
, StopTaskCounter(0)
, bReachedInputEndOfStream(false)
{}

FMediaCodecInputWorker::~FMediaCodecInputWorker()
{
	DestroyThread();
}

void FMediaCodecInputWorker::InitThread(FMagicLeapMediaCodecPlayer& InOwnerPlayer, MLHandle& InExtractorHandle, FCriticalSection& InCriticalSection, FCriticalSection& InGT_IT_Mutex, FCriticalSection& InRT_IT_Mutex)
{
	OwnerPlayer = &InOwnerPlayer;
	ExtractorHandle = &InExtractorHandle;
	CriticalSection = &InCriticalSection;
	GT_IT_Mutex = &InGT_IT_Mutex;
	RT_IT_Mutex = &InRT_IT_Mutex;
	StopTaskCounter.Set(0);
	bReachedInputEndOfStream = false;

	if (Semaphore == nullptr)
	{
		Semaphore = FGenericPlatformProcess::GetSynchEventFromPool(false);
		Thread = FRunnableThread::Create(this, TEXT("MLMediaCodecInputWorker"), 0, TPri_Normal, FLuminAffinity::GetPoolThreadMask());
	}
}

void FMediaCodecInputWorker::DestroyThread()
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

uint32 FMediaCodecInputWorker::Run()
{
	FInputWorkerTask QueuedTask;

	while (StopTaskCounter.GetValue() == 0)
	{
		EMediaState CurrentState = EMediaState::Error;

		{
			FScopeLock Lock(CriticalSection);
			CurrentState = OwnerPlayer->GetState();
		}

		if (CurrentState != EMediaState::Playing && CurrentState != EMediaState::Preparing)
		{
			Semaphore->Wait();
		}

		{
			FScopeLock Lock(CriticalSection);
			if (OwnerPlayer->IsPlaybackCompleted() && OwnerPlayer->IsLooping())
			{
				bReachedInputEndOfStream = false;
				OwnerPlayer->SetPlaybackCompleted(false);
				// Enqueue on the incoming tasks queue instead of directly calling Seek_WorkerThread so that CriticalSection mutex is not locked by this worker thread while seeking.
				Seek(FTimespan::Zero());
			}
		}

		if (IncomingTasks.Dequeue(QueuedTask))
		{
			if (QueuedTask.TaskType == EInputWorkerTaskType::Seek)
			{
				FScopeLock LockGT(GT_IT_Mutex);
				FScopeLock LockRT(RT_IT_Mutex);
				Seek_WorkerThread(QueuedTask.SeekTime);
			}
		}

		ProcessInputSample_WorkerThread();

		// ~120Hz
		// TODO: make configurable?
		// TODO: dont sleep if there is something in the the IncomingTasks queue.
		FPlatformProcess::Sleep(0.008f);
	}

	return 0;
}

void FMediaCodecInputWorker::WakeUp()
{
	if (Semaphore != nullptr)
	{
		Semaphore->Trigger();
	}
}

void FMediaCodecInputWorker::Seek(FTimespan SeekTime)
{
	IncomingTasks.Enqueue(FInputWorkerTask(EInputWorkerTaskType::Seek, SeekTime));
}

bool FMediaCodecInputWorker::HasReachedInputEOS() const
{
	return bReachedInputEndOfStream;
}

void FMediaCodecInputWorker::ProcessInputSample_WorkerThread()
{
	{
		FScopeLock Lock(CriticalSection);
		if (bReachedInputEndOfStream)
		{
			return;
		}
	}

	int64 BufferIndex = MLMediaCodec_TryAgainLater;
	SIZE_T BufferSize = 0;
	uint8_t *Buffer = nullptr;
	int64 SampleSize = -1;
	int64 PresentationTimeUs = -1;
	int64 TrackIndex = -1;
	MLResult Result;
	MLHandle SampleCodecHandle = ML_INVALID_HANDLE;

	// loop until we find a vald track index
	do
	{
		Result = MLMediaExtractorGetSampleTrackIndex(*ExtractorHandle, reinterpret_cast<int64_t*>(&TrackIndex));
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaExtractorGetSampleTrackIndex() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));

		if (TrackIndex >= 0)
		{
			if (!OwnerPlayer->GetCodecForTrackIndex(TrackIndex, SampleCodecHandle))
			{
				Result = MLMediaExtractorAdvance(*ExtractorHandle);
				UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaExtractorAdvance(audio track) failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
			}
		}
		else
		{
			UE_LOG(LogMagicLeapMediaCodec, Display, TEXT("negative track index from MLMediaExtractorGetSampleTrackIndex. Reached input EOS."));
		
			{
				FScopeLock Lock(CriticalSection);
				bReachedInputEndOfStream = true;
			}

			return;
		}
	} while (!MLHandleIsValid(SampleCodecHandle));

	Result = MLMediaCodecDequeueInputBuffer(SampleCodecHandle, 0, reinterpret_cast<int64_t*>(&BufferIndex));
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecDequeueInputBuffer failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		return;
	}
	if (BufferIndex == MLMediaCodec_TryAgainLater)
	{
		// UE_LOG(LogMagicLeapMediaCodec, Warning, TEXT("no input buffers available"));
		return;
	}

	Result = MLMediaCodecGetInputBufferPointer(SampleCodecHandle, BufferIndex, &Buffer, reinterpret_cast<size_t*>(&BufferSize));
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecGetInputBufferPointer failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		return;
	}
	else if (Buffer == nullptr)
	{
		UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("Got a null buffer pointer from MLMediaCodecGetInputBufferPointer"));
		return;
	}

	Result = MLMediaExtractorReadSampleData(*ExtractorHandle, Buffer, BufferSize, 0, reinterpret_cast<int64_t*>(&SampleSize));
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaExtractorReadSampleData failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		return;
	}
	if (SampleSize < 0)
	{
		SampleSize = 0;
		UE_LOG(LogMagicLeapMediaCodec, Display, TEXT("negative sample size from MLMediaExtractorReadSampleData. Reached input EOS."));

		{
			FScopeLock Lock(CriticalSection);
			bReachedInputEndOfStream = true;
		}
	}

	Result = MLMediaExtractorGetSampleTime(*ExtractorHandle, reinterpret_cast<int64_t*>(&PresentationTimeUs));
	UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaExtractorGetSampleTime() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));

	Result = MLMediaCodecQueueInputBuffer(SampleCodecHandle, (MLHandle)BufferIndex, 0, SampleSize, PresentationTimeUs, bReachedInputEndOfStream ? MLMediaCodecBufferFlag_EOS : 0);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecQueueInputBuffer failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		return;
	}

	Result = MLMediaExtractorAdvance(*ExtractorHandle);
	UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaExtractorAdvance() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
}

bool FMediaCodecInputWorker::Seek_WorkerThread(const FTimespan& SeekTime)
{
	UE_LOG(LogMagicLeapMediaCodec, Display, TEXT("Seek(%f)"), SeekTime.GetTotalMilliseconds());
	MLResult Result = MLMediaExtractorSeekTo(*ExtractorHandle, SeekTime.GetTotalMicroseconds(), MLMediaSeekMode_Closest_Sync);
	if (Result == MLResult_Ok)
	{
		// TODO: Make thread safe.
		// Flush codec samples after seeking
		OwnerPlayer->FlushCodecs();
		OwnerPlayer->QueueMediaEvent(EMediaEvent::SeekCompleted);
		OwnerPlayer->QueueVideoCodecStartTimeReset();

		return true;
	}
	else
	{
		UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaExtractorSeekTo failed with error %s"), UTF8_TO_TCHAR(MLGetResultString(Result)));
		return false;
	}
}
