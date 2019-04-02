// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ImgMediaPrivate.h"

#include "IMediaClock.h"
#include "IMediaModule.h"
#include "Misc/QueuedThreadPool.h"
#include "Modules/ModuleManager.h"

#include "ImgMediaPlayer.h"
#include "ImgMediaScheduler.h"
#include "IImgMediaModule.h"


DEFINE_LOG_CATEGORY(LogImgMedia);


#if USE_IMGMEDIA_DEALLOC_POOL
struct FImgMediaThreadPool
{
public:

	FImgMediaThreadPool() :
		Pool(nullptr),
		bHasInit(false)
	{
	}

	~FImgMediaThreadPool()
	{
		Reset();
	}

	void Reset()
	{
		FScopeLock Lock(&CriticalSection);
		if (Pool != nullptr)
		{
			Pool->Destroy();
			Pool = nullptr;
		}

		bHasInit = false;
	}

	FQueuedThreadPool* GetThreadPool()
	{
		FScopeLock Lock(&CriticalSection);
		if (bHasInit)
		{
			return Pool;
		}

		// initialize worker thread pools
		if (FPlatformProcess::SupportsMultithreading())
		{
			// initialize dealloc thread pool
			const int32 ThreadPoolSize = 1;
			const uint32 StackSize = 128 * 1024;

			Pool = FQueuedThreadPool::Allocate();
			verify(Pool->Create(ThreadPoolSize, StackSize, TPri_Normal));
		}

		bHasInit = true;

		return Pool;
	}

private:
	FCriticalSection CriticalSection;
	FQueuedThreadPool* Pool;
	bool bHasInit;
};

FImgMediaThreadPool ImgMediaThreadPool;

FQueuedThreadPool* GetImgMediaThreadPoolSlow()
{
	return ImgMediaThreadPool.GetThreadPool();
}
#endif // USE_IMGMEDIA_DEALLOC_POOL


/**
 * Implements the AVFMedia module.
 */
class FImgMediaModule
	: public IImgMediaModule
{
public:

	/** Default constructor. */
	FImgMediaModule() { }

public:

	//~ IImgMediaModule interface

	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override
	{
		if (!Scheduler.IsValid())
		{
			InitScheduler();
		}

		return MakeShared<FImgMediaPlayer, ESPMode::ThreadSafe>(EventSink, Scheduler.ToSharedRef());
	}

public:

	//~ IModuleInterface interface

	virtual void StartupModule() override
	{

	}

	virtual void ShutdownModule() override
	{
		Scheduler.Reset();

#if USE_IMGMEDIA_DEALLOC_POOL
		ImgMediaThreadPool.Reset();
#endif
	}

private:

	void InitScheduler()
	{
		// initialize scheduler
		Scheduler = MakeShared<FImgMediaScheduler, ESPMode::ThreadSafe>();
		Scheduler->Initialize();

		IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

		if (MediaModule != nullptr)
		{
			MediaModule->GetClock().AddSink(Scheduler.ToSharedRef());
		}
	}

	TSharedPtr<FImgMediaScheduler, ESPMode::ThreadSafe> Scheduler;
};


IMPLEMENT_MODULE(FImgMediaModule, ImgMedia);
