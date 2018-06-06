// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeCounter.h"
#include "ExternalOESTextureRenderer.h"

#include "ml_api.h"

class FMediaWorker : public FRunnable
{
public:
	FMediaWorker(MLHandle PlayerHandle, FCriticalSection& CriticalSectionRef);
	virtual ~FMediaWorker();

	void InitThread();
	virtual uint32 Run() override;

	void* GetReadBuffer(FIntPoint* Dim, FTimespan* Time);

	FThreadSafeCounter NextBufferAvailable;

private:
	void SwapBuffers();

	FRunnableThread* Thread;
	FThreadSafeCounter StopTaskCounter;

	MLHandle MediaPlayerHandle;
	FCriticalSection* CriticalSection;

	void* ReadBuffer;
	void* WriteBuffer;

	SIZE_T BufferSize;

	FExternalOESTextureRenderer* MediaRenderer;

	FIntPoint ReadBufferDimensions;
	FTimespan ReadBufferTime;
};
