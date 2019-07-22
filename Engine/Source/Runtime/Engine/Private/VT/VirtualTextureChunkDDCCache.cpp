// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureChunkDDCCache.h"

#if WITH_EDITOR

#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Async/AsyncWork.h"
#include "Misc/CoreMisc.h"
#include "DerivedDataCacheInterface.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "VirtualTextureBuiltData.h"
#include "VirtualTextureChunkManager.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/Runnable.h"
#include "Misc/ConfigCacheIni.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVTDiskCache, Log, All);
DEFINE_LOG_CATEGORY(LogVTDiskCache);

class FVirtualTextureDCCCacheCleanup final : public FRunnable
{
	/** Singleton instance */
	static FVirtualTextureDCCCacheCleanup* Runnable;

	FRunnableThread* Thread;
	FThreadSafeCounter StopTaskCounter;
	FString Directory;

	FTimespan UnusedFileTime;
	int32 MaxContinuousFileChecks = -1;
	
	FVirtualTextureDCCCacheCleanup(const FString& InDirectory)
		: Directory(InDirectory)
	{
		check(GConfig);
		int32 UnusedFileAge = 17;
		GConfig->GetInt(TEXT("VirtualTextureChunkDDCCache"), TEXT("UnusedFileAge"), UnusedFileAge, GEngineIni);
		UnusedFileTime = FTimespan(UnusedFileAge, 0, 0, 0);
		GConfig->GetInt(TEXT("VirtualTextureChunkDDCCache"), TEXT("MaxFileChecksPerSec"), MaxContinuousFileChecks, GEngineIni);

		// Don't delete the runnable automatically. It's going to be manually deleted in FDDCCleanup::Shutdown.
		Thread = FRunnableThread::Create(this, TEXT("FVirtualTextureDCCCacheCleanup"), 0, TPri_BelowNormal, FPlatformAffinity::GetPoolThreadMask());
	}

	virtual ~FVirtualTextureDCCCacheCleanup()
	{
		delete Thread;
	}

	virtual uint32 Run() override
	{
		// Give up some time to the engine to start up and load everything
		Wait(120.0f, 0.5f);

		// find all files in the directory
		TArray<FString> FileNames;
		IFileManager::Get().FindFilesRecursive(FileNames, *Directory, TEXT("*.*"), true, false);

		// Cleanup
		int32 NumFilesChecked = 0;
		for (int32 FileIndex = 0; FileIndex < FileNames.Num() && ShouldStop() == false; FileIndex++)
		{
			const FDateTime LastModificationTime = IFileManager::Get().GetTimeStamp(*FileNames[FileIndex]);
			const FDateTime LastAccessTime = IFileManager::Get().GetAccessTimeStamp(*FileNames[FileIndex]);
			if ((LastAccessTime != FDateTime::MinValue()) || (LastModificationTime != FDateTime::MinValue()))
			{
				const FTimespan TimeSinceLastAccess = FDateTime::UtcNow() - LastAccessTime;
				const FTimespan TimeSinceLastModification = FDateTime::UtcNow() - LastModificationTime;
				if (TimeSinceLastAccess >= UnusedFileTime && TimeSinceLastModification >= UnusedFileTime)
				{
					// Delete the file
					bool Result = IFileManager::Get().Delete(*FileNames[FileIndex], false, true, true);
				}
			}

			if (++NumFilesChecked >= MaxContinuousFileChecks && MaxContinuousFileChecks > 0 && ShouldStop() == false)
			{
				NumFilesChecked = 0;
				Wait(1.0f);
			}
			else
			{
				// Give up a tiny amount of time so that we're not consuming too much cpu/hdd resources.
				Wait(0.05f);
			}
		}

		return 0;
	}

	virtual void Stop() override
	{
		StopTaskCounter.Increment();
	}

	FORCEINLINE bool ShouldStop() const
	{
		return StopTaskCounter.GetValue() > 0;
	}

	/**
	* Waits for a given amount of time periodically checking if there's been any Stop requests.
	*
	* @param InSeconds time in seconds to wait.
	* @param InSleepTime interval at which to check for Stop requests.
	*/
	void Wait(const float InSeconds, const float InSleepTime = 0.1f)
	{
		// Instead of waiting the given amount of seconds doing nothing
		// check periodically if there's been any Stop requests.
		for (float TimeToWait = InSeconds; TimeToWait > 0.0f && ShouldStop() == false; TimeToWait -= InSleepTime)
		{
			FPlatformProcess::Sleep(FMath::Min(InSleepTime, TimeToWait));
		}
	}
	
	void EnsureCompletion()
	{
		Stop();
		Thread->WaitForCompletion();
	}

public:

	static void Startup(const FString& Directory)
	{
		if (Runnable == nullptr && FPlatformProcess::SupportsMultithreading())
		{
			Runnable = new FVirtualTextureDCCCacheCleanup(Directory);
		}
	}

	static void Shutdown()
	{
		if (Runnable)
		{
			Runnable->EnsureCompletion();
			delete Runnable;
			Runnable = nullptr;
		}
	}
};
FVirtualTextureDCCCacheCleanup* FVirtualTextureDCCCacheCleanup::Runnable = nullptr;

class FAsyncFillCacheWorker : public FNonAbandonableTask
{
public:
	FString	Filename;
	FVirtualTextureDataChunk* Chunk;

	FAsyncFillCacheWorker(const FString& InFilename, FVirtualTextureDataChunk* InChunk)
		: Filename(InFilename)
		, Chunk(InChunk)
	{
	}

	void DoWork()
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		FDerivedDataCacheInterface& DDC = GetDerivedDataCacheRef();

		//The file might be resident but this is the first request to it, flag as available
		if (PlatformFile.FileExists(*Filename))
		{
			Chunk->bFileAvailableInVTDDCDache = true;
			return;
		}

		// Fetch data from DDC
		TArray<uint8> Results;
		//TODO(ddebaets) this sync request seems to be blocking here while it uses the job pool. add overload to perform it on this thread?
		const bool DDCResult = DDC.GetSynchronous(*Chunk->DerivedDataKey, Results);
		if (DDCResult == false)
		{
			UE_LOG(LogVTDiskCache, Error, TEXT("Failed to fetch data from DDC (key: %s)"), *Chunk->DerivedDataKey);
			return;
		}

		// Write to Disk
		FArchive* Ar = IFileManager::Get().CreateFileWriter(*Filename, 0);
		if (Ar == nullptr)
		{
			UE_LOG(LogVTDiskCache, Error, TEXT("Failed to write to %s"), *Filename);
			return;
		}
		Ar->Serialize(const_cast<uint8*>(Results.GetData() + 4), Results.Num() - 4); // skip size embedded in DDC entry
		delete Ar;

		// File is now available
		Chunk->bFileAvailableInVTDDCDache = true;
	}

	FORCEINLINE TStatId GetStatId() const
	{
		return TStatId();
	}
};

FVirtualTextureChunkDDCCache* GetVirtualTextureChunkDDCCache()
{
	static FVirtualTextureChunkDDCCache DDCCache;
	return &DDCCache;
}

void FVirtualTextureChunkDDCCache::Initialize()
{
	// setup the cache folder
	auto& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	check(GConfig);
	GConfig->GetString(TEXT("VirtualTextureChunkDDCCache"), TEXT("Path"), AbsoluteCachePath, GEngineIni);
	AbsoluteCachePath = FPaths::ConvertRelativePathToFull(AbsoluteCachePath);
	if (PlatformFile.DirectoryExists(*AbsoluteCachePath) == false)
	{
		PlatformFile.CreateDirectoryTree(*AbsoluteCachePath);
	}

	// test if the folder is accessible
	FString TempFilename = AbsoluteCachePath / FGuid::NewGuid().ToString() + ".tmp";
	FFileHelper::SaveStringToFile(FString("TEST"), *TempFilename);
	int32 TestFileSize = IFileManager::Get().FileSize(*TempFilename);
	if (TestFileSize < 4)
	{
		UE_LOG(LogVTDiskCache, Warning, TEXT("Fail to write to %s, derived data cache to this directory will be read only."), *AbsoluteCachePath);
	}
	if (TestFileSize >= 0)
	{
		IFileManager::Get().Delete(*TempFilename, false, false, true);
	}

	FVirtualTextureDCCCacheCleanup::Startup(AbsoluteCachePath);
}

void FVirtualTextureChunkDDCCache::ShutDown()
{
	ActiveChunks.Empty();
	FVirtualTextureDCCCacheCleanup::Shutdown();
}

void FVirtualTextureChunkDDCCache::UpdateRequests()
{
	ActiveChunks.RemoveAll([](auto Chunk) -> bool {return Chunk->bFileAvailableInVTDDCDache == true; });
}

bool FVirtualTextureChunkDDCCache::MakeChunkAvailable(FVirtualTextureDataChunk* Chunk, FString& ChunkFileName, bool bAsync)
{
	const FString CachedFilePath = AbsoluteCachePath / Chunk->ShortDerivedDataKey;

	// File already available? 
	if (Chunk->bFileAvailableInVTDDCDache)
	{
		ChunkFileName = CachedFilePath;
		return true;
	}

	// Are we already processing this chunk ?
	const int32 ChunkInProgressIdx = ActiveChunks.Find(Chunk);
	if (ChunkInProgressIdx != -1)
	{
		return false;
	}
	
	// start filling it to the cache
	if (bAsync)
	{
		ActiveChunks.Add(Chunk);
		(new FAutoDeleteAsyncTask<FAsyncFillCacheWorker>(CachedFilePath, Chunk))->StartBackgroundTask();
	}
	else
	{
		FAsyncFillCacheWorker SyncWorker(CachedFilePath, Chunk);
		SyncWorker.DoWork();
		if (Chunk->bFileAvailableInVTDDCDache)
		{
			ChunkFileName = CachedFilePath;
			return true;
		}
	}

	return false;
}


#endif