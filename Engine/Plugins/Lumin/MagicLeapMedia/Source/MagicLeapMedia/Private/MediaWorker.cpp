// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MediaWorker.h"
#include "IMagicLeapMediaModule.h"
#include "Misc/ScopeLock.h"

#include "ml_media_player.h"

FMediaWorker::FMediaWorker(MLHandle PlayerHandle, FCriticalSection& CriticalSectionRef)
	: Thread(nullptr)
	, MediaPlayerHandle(PlayerHandle)
	, CriticalSection(&CriticalSectionRef)
	, ReadBuffer(nullptr)
	, WriteBuffer(nullptr)
	, BufferSize(0)
	, MediaRenderer(nullptr)
{}

FMediaWorker::~FMediaWorker()
{
	StopTaskCounter.Increment();

	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;		
	}

	if (ReadBuffer != nullptr)
	{
		FMemory::Free(ReadBuffer);
		ReadBuffer = nullptr;
	}

	if (WriteBuffer != nullptr)
	{
		FMemory::Free(WriteBuffer);
		WriteBuffer = nullptr;
	}

	if (MediaRenderer != nullptr)
	{
		delete MediaRenderer;
		MediaRenderer = nullptr;
	}
}

void FMediaWorker::InitThread()
{
	if (Thread == nullptr)
	{
		Thread = FRunnableThread::Create(this, TEXT("MLMediaWorker"), 0, TPri_Normal);
		MediaRenderer = new FExternalOESTextureRenderer(true);
	}
}

uint32 FMediaWorker::Run()
{
	while (StopTaskCounter.GetValue() == 0)
	{
		uint16_t States = 0;
		MLResult Result = MLMediaPlayerPollStates(MediaPlayerHandle, MLMediaPlayerPollingStateFlag_IsBufferAvailable, &States);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMedia, Error, TEXT("MLMediaPlayerPollStates failed with error %s."), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		if (Result != MLResult_Ok || !(MLMediaPlayerPollingStateFlag_IsBufferAvailable & States))
		{
			continue;
		}

		int32 currentPositionMilSec = 0;
		Result = MLMediaPlayerGetCurrentPosition(MediaPlayerHandle, &currentPositionMilSec);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMedia, Error, TEXT("MLMediaPlayerGetCurrentPosition failed with error %s."), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		const FTimespan Time = FTimespan::FromMilliseconds(currentPositionMilSec);

		int32 width = 0;
		int32 height = 0;
		Result = MLMediaPlayerGetVideoSize(MediaPlayerHandle, &width, &height);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerGetVideoSize failed with error %s."), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
			continue;
		}

		const SIZE_T RequiredBufferSize = width * height * sizeof(int32);
		if (BufferSize < RequiredBufferSize || WriteBuffer == nullptr)
		{
			if (BufferSize == 0 || WriteBuffer == nullptr)
			{
				WriteBuffer = FMemory::Malloc(RequiredBufferSize);
			}
			else
			{
				WriteBuffer = FMemory::Realloc(WriteBuffer, RequiredBufferSize);
			}

			BufferSize = RequiredBufferSize;
		}

		// MLHandle because Unreal's uint64 is 'unsigned long long *' whereas uint64_t for the C-API is 'unsigned long *'
		// TODO: Fix the Unreal types for the above comment.
		MLHandle nativeBuffer = ML_INVALID_HANDLE;
		Result = MLMediaPlayerAcquireNextAvailableBuffer(MediaPlayerHandle, &nativeBuffer);

		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerAcquireNextAvailableBuffer failed with error %s."), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
			continue;
		}

		bool TextureCopyResult = MediaRenderer->CopyFrameTexture(0, nativeBuffer, FIntPoint(width, height), WriteBuffer);
		if (!TextureCopyResult)
		{
			UE_LOG(LogMagicLeapMedia, Error, TEXT("CopyFrameTexture failed"));
		}

		Result = MLMediaPlayerReleaseBuffer(MediaPlayerHandle, nativeBuffer);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMedia, Error, TEXT("MLMediaPlayerReleaseBuffer failed with error %s."), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		{
			FScopeLock Lock(CriticalSection);
			SwapBuffers();
			ReadBufferDimensions = FIntPoint(width, height);
			ReadBufferTime = Time;
			NextBufferAvailable.Increment();
		}
	}

	return 0;
}

void* FMediaWorker::GetReadBuffer(FIntPoint* Dim, FTimespan* Time)
{
	if (Dim != nullptr)
	{
		*Dim = ReadBufferDimensions;
	}

	if (Time != nullptr)
	{
		*Time = ReadBufferTime;
	}

	return ReadBuffer;
}

void FMediaWorker::SwapBuffers()
{
	void* Temp = ReadBuffer;
	ReadBuffer = WriteBuffer;
	WriteBuffer = Temp;
}
