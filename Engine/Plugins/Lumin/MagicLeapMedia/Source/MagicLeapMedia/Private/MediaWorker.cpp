// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
		if (!(MLMediaPlayerPollingStateFlag_IsBufferAvailable & MLMediaPlayerPollStates(MediaPlayerHandle, MLMediaPlayerPollingStateFlag_IsBufferAvailable)))
		{
			continue;
		}

		int32 currentPositionMilSec = 0;
		bool bResult = MLMediaPlayerGetCurrentPosition(MediaPlayerHandle, &currentPositionMilSec);
		const FTimespan Time = FTimespan::FromMilliseconds(currentPositionMilSec);

		int32 width = 0;
		int32 height = 0;
		bResult = MLMediaPlayerGetVideoSize(MediaPlayerHandle, &width, &height);
		if (!bResult)
		{
			UE_LOG(LogMagicLeapMedia, Error, TEXT("Error getting video dimensions"));
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
		bResult = MLMediaPlayerAcquireNextAvailableBuffer(MediaPlayerHandle, &nativeBuffer);

		if (!bResult)
		{
			UE_LOG(LogMagicLeapMedia, Error, TEXT("Error acquiring next video buffer"));
			continue;
		}

		bool TextureCopyResult = MediaRenderer->CopyFrameTexture(0, nativeBuffer, FIntPoint(width, height), WriteBuffer);
		if (!TextureCopyResult)
		{
			UE_LOG(LogMagicLeapMedia, Error, TEXT("CopyFrameTexture failed"));
		}

		MLMediaPlayerReleaseBuffer(MediaPlayerHandle, nativeBuffer);

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
