// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ImgMediaLoader.h"
#include "ImgMediaPrivate.h"

#include "Algo/Reverse.h"
#include "Misc/FrameRate.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/QueuedThreadPool.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

#include "GenericImgMediaReader.h"
#include "IImgMediaReader.h"
#include "ImgMediaLoaderWork.h"
#include "ImgMediaScheduler.h"
#include "ImgMediaTextureSample.h"

#if IMGMEDIA_EXR_SUPPORTED_PLATFORM
	#include "ExrImgMediaReader.h"
#endif


/** Time spent loading a new image sequence. */
DECLARE_CYCLE_STAT(TEXT("ImgMedia Loader Load Sequence"), STAT_ImgMedia_LoaderLoadSequence, STATGROUP_Media);

/** Time spent releasing cache in image loader destructor. */
DECLARE_CYCLE_STAT(TEXT("ImgMedia Loader Release Cache"), STAT_ImgMedia_LoaderReleaseCache, STATGROUP_Media);


/* FImgMediaLoader structors
 *****************************************************************************/

FImgMediaLoader::FImgMediaLoader(const TSharedRef<FImgMediaScheduler, ESPMode::ThreadSafe>& InScheduler)
	: Frames(1)
	, ImageWrapperModule(FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper"))
	, Initialized(false)
	, NumLoadAhead(0)
	, NumLoadBehind(0)
	, Scheduler(InScheduler)
	, SequenceDim(FIntPoint::ZeroValue)
	, SequenceDuration(FTimespan::Zero())
	, SequenceFrameRate(0, 0)
	, LastRequestedFrame(INDEX_NONE)
{
	UE_LOG(LogImgMedia, Verbose, TEXT("Loader %p: Created"), this);
}


FImgMediaLoader::~FImgMediaLoader()
{
	UE_LOG(LogImgMedia, Verbose, TEXT("Loader %p: Destroyed"), this);

	// clean up work item pool
	for (auto Work : WorkPool)
	{
		delete Work;
	}

	WorkPool.Empty();

	// release cache
	{
		SCOPE_CYCLE_COUNTER(STAT_ImgMedia_LoaderReleaseCache);

		Frames.Empty();
		PendingFrameNumbers.Empty();
	}
}


/* FImgMediaLoader interface
 *****************************************************************************/

uint64 FImgMediaLoader::GetBitRate() const
{
	FScopeLock Lock(&CriticalSection);
	return SequenceDim.X * SequenceDim.Y * sizeof(uint16) * 8 * SequenceFrameRate.AsDecimal();
}


void FImgMediaLoader::GetBusyTimeRanges(TRangeSet<FTimespan>& OutRangeSet) const
{
	FScopeLock Lock(&CriticalSection);
	FrameNumbersToTimeRanges(QueuedFrameNumbers, OutRangeSet);
}


void FImgMediaLoader::GetCompletedTimeRanges(TRangeSet<FTimespan>& OutRangeSet) const
{
	FScopeLock Lock(&CriticalSection);

	TArray<int32> CompletedFrames;
	Frames.GetKeys(CompletedFrames);
	FrameNumbersToTimeRanges(CompletedFrames, OutRangeSet);
}


TSharedPtr<FImgMediaTextureSample, ESPMode::ThreadSafe> FImgMediaLoader::GetFrameSample(FTimespan Time)
{
	const int32 FrameIndex = TimeToFrameNumber(Time);

	if (FrameIndex == INDEX_NONE)
	{
		return nullptr;
	}

	FScopeLock ScopeLock(&CriticalSection);

	const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>* Frame = Frames.FindAndTouch(FrameIndex);

	if (Frame == nullptr)
	{
		return nullptr;
	}

	const FTimespan FrameStartTime = FrameNumberToTime(FrameIndex);
	const FTimespan NextStartTime = FrameNumberToTime(FrameIndex + 1);

	auto Sample = MakeShared<FImgMediaTextureSample, ESPMode::ThreadSafe>();
	
	if (!Sample->Initialize(*Frame->Get(), SequenceDim, FrameStartTime, NextStartTime - FrameStartTime))
	{
		return nullptr;
	}

	return Sample;
}


void FImgMediaLoader::GetPendingTimeRanges(TRangeSet<FTimespan>& OutRangeSet) const
{
	FScopeLock Lock(&CriticalSection);
	FrameNumbersToTimeRanges(PendingFrameNumbers, OutRangeSet);
}


IQueuedWork* FImgMediaLoader::GetWork()
{
	FScopeLock Lock(&CriticalSection);

	if (PendingFrameNumbers.Num() == 0)
	{
		return nullptr;
	}

	int32 FrameNumber = PendingFrameNumbers.Pop(false);
	FImgMediaLoaderWork* Work = (WorkPool.Num() > 0) ? WorkPool.Pop() : new FImgMediaLoaderWork(AsShared(), Reader.ToSharedRef());
	
	Work->Initialize(FrameNumber, ImagePaths[FrameNumber]);
	QueuedFrameNumbers.Add(FrameNumber);

	return Work;
}


void FImgMediaLoader::Initialize(const FString& SequencePath, const FFrameRate& FrameRateOverride, bool Loop)
{
	UE_LOG(LogImgMedia, Verbose, TEXT("Loader %p: Initializing with %s (FrameRateOverride = %s, Loop = %i)"),
		this,
		*SequencePath,
		*FrameRateOverride.ToPrettyText().ToString(),
		Loop
	);

	check(!Initialized); // reinitialization not allowed for now

	LoadSequence(SequencePath, FrameRateOverride, Loop);
	FPlatformMisc::MemoryBarrier();

	Initialized = true;
}


bool FImgMediaLoader::RequestFrame(FTimespan Time, float PlayRate, bool Loop)
{
	const int32 FrameNumber = TimeToFrameNumber(Time);

	if ((FrameNumber == INDEX_NONE) || (FrameNumber == LastRequestedFrame))
	{
		UE_LOG(LogImgMedia, VeryVerbose, TEXT("Loader %p: Skipping frame %i for time %s"), this, FrameNumber, *Time.ToString(TEXT("%h:%m:%s.%t")));
		return false;
	}

	UE_LOG(LogImgMedia, VeryVerbose, TEXT("Loader %p: Requesting frame %i for time %s"), this, FrameNumber, *Time.ToString(TEXT("%h:%m:%s.%t")));

	Update(FrameNumber, PlayRate, Loop);
	LastRequestedFrame = FrameNumber;

	return true;
}


/* FImgMediaLoader implementation
 *****************************************************************************/

void FImgMediaLoader::FrameNumbersToTimeRanges(const TArray<int32>& FrameNumbers, TRangeSet<FTimespan>& OutRangeSet) const
{
	if (!SequenceFrameRate.IsValid() || (SequenceFrameRate.Numerator <= 0))
	{
		return;
	}

	for (const auto FrameNumber : FrameNumbers)
	{
		const FTimespan FrameStartTime = FrameNumberToTime(FrameNumber);
		const FTimespan NextStartTime = FrameNumberToTime(FrameNumber + 1);

		OutRangeSet.Add(TRange<FTimespan>(FrameStartTime, NextStartTime));
	}
}


FTimespan FImgMediaLoader::FrameNumberToTime(uint32 FrameNumber) const
{
	return FTimespan(FMath::DivideAndRoundNearest(
		FrameNumber * SequenceFrameRate.Denominator * ETimespan::TicksPerSecond,
		(int64)SequenceFrameRate.Numerator
	));
}


void FImgMediaLoader::LoadSequence(const FString& SequencePath, const FFrameRate& FrameRateOverride, bool Loop)
{
	SCOPE_CYCLE_COUNTER(STAT_ImgMedia_LoaderLoadSequence);

	if (SequencePath.IsEmpty())
	{
		return;
	}

	// locate image sequence files
	TArray<FString> FoundFiles;
	IFileManager::Get().FindFiles(FoundFiles, *SequencePath, TEXT("*"));

	if (FoundFiles.Num() == 0)
	{
		UE_LOG(LogImgMedia, Error, TEXT("The directory %s does not contain any image files"), *SequencePath);
		return;
	}

	UE_LOG(LogImgMedia, Verbose, TEXT("Loader %p: Found %i image files in %s"), this, FoundFiles.Num(), *SequencePath);

	FoundFiles.Sort();

	for (const auto& File : FoundFiles)
	{
		ImagePaths.Add(FPaths::Combine(SequencePath, File));
	}

	FScopeLock Lock(&CriticalSection);

	// create image reader
	const FString FirstExtension = FPaths::GetExtension(ImagePaths[0]);

	if (FirstExtension == TEXT("exr"))
	{
#if IMGMEDIA_EXR_SUPPORTED_PLATFORM
		Reader = MakeShareable(new FExrImgMediaReader);
#else
		UE_LOG(LogImgMedia, Error, TEXT("EXR image sequences are currently supported on macOS and Windows only"));
		return;
#endif
	}
	else
	{
		Reader = MakeShareable(new FGenericImgMediaReader(ImageWrapperModule));
	}

	// fetch sequence attributes from first image
	FImgMediaFrameInfo FirstFrameInfo;

	if (!Reader->GetFrameInfo(ImagePaths[0], FirstFrameInfo))
	{
		UE_LOG(LogImgMedia, Error, TEXT("Failed to get frame information from first image in %s"), *SequencePath);
		return;
	}

	if (FirstFrameInfo.UncompressedSize == 0)
	{
		UE_LOG(LogImgMedia, Error, TEXT("The first image in sequence %s does not have a valid frame size"), *SequencePath);
		return;
	}

	if (FirstFrameInfo.Dim.GetMin() <= 0)
	{
		UE_LOG(LogImgMedia, Error, TEXT("The first image in sequence %s does not have a valid dimension"), *SequencePath);
		return;
	}

	SequenceDim = FirstFrameInfo.Dim;

	if (FrameRateOverride.IsValid() && (FrameRateOverride.Numerator > 0))
	{
		SequenceFrameRate = FrameRateOverride;
	}
	else
	{
		SequenceFrameRate = FirstFrameInfo.FrameRate;
	}

	SequenceDuration = FrameNumberToTime(ImagePaths.Num());

	// initialize loader
	auto Settings = GetDefault<UImgMediaSettings>();

	const FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
	const SIZE_T DesiredCacheSize = Settings->CacheSizeGB * 1024 * 1024 * 1024;
	const SIZE_T CacheSize = FMath::Clamp(DesiredCacheSize, (SIZE_T)0, (SIZE_T)Stats.AvailablePhysical);

	const int32 MaxFramesToLoad = (int32)(CacheSize / FirstFrameInfo.UncompressedSize);
	const int32 NumFramesToLoad = FMath::Clamp(MaxFramesToLoad, 0, ImagePaths.Num());
	const float LoadBehindScale = FMath::Clamp(Settings->CacheBehindPercentage, 0.0f, 100.0f) / 100.0f;

	NumLoadBehind = (int32)(LoadBehindScale * NumFramesToLoad);
	NumLoadAhead = NumFramesToLoad - NumLoadBehind;

	Frames.Empty(NumFramesToLoad);
	Update(0, 0.0f, Loop);

	// update info
	Info = TEXT("Image Sequence\n");
	Info += FString::Printf(TEXT("    Dimension: %i x %i\n"), SequenceDim.X, SequenceDim.Y);
	Info += FString::Printf(TEXT("    Format: %s\n"), *FirstFrameInfo.FormatName);
	Info += FString::Printf(TEXT("    Compression: %s\n"), *FirstFrameInfo.CompressionName);
	Info += FString::Printf(TEXT("    Frames: %i\n"), ImagePaths.Num());
	Info += FString::Printf(TEXT("    Frame Rate: %.2f (%i/%i)\n"), SequenceFrameRate.AsDecimal(), SequenceFrameRate.Numerator, SequenceFrameRate.Denominator);
}


uint32 FImgMediaLoader::TimeToFrameNumber(FTimespan Time) const
{
	if ((Time < FTimespan::Zero()) || (Time > SequenceDuration))
	{
		return INDEX_NONE;
	}

	return (Time.GetTicks() * SequenceFrameRate.Numerator) / (ETimespan::TicksPerSecond * SequenceFrameRate.Denominator);
}


void FImgMediaLoader::Update(int32 PlayHeadFrame, float PlayRate, bool Loop)
{
	// @todo gmp: ImgMedia: take PlayRate and DeltaTime into account when determining frames to load
	
	// determine frame numbers to be loaded
	TArray<int32> FramesToLoad;
	{
		const int32 NumImagePaths = ImagePaths.Num();

		FramesToLoad.Empty(NumLoadAhead + NumLoadBehind);

		int32 FrameOffset = (PlayRate >= 0.0f) ? 1 : -1;

		int32 LoadAheadCount = NumLoadAhead;
		int32 LoadAheadIndex = PlayHeadFrame;

		int32 LoadBehindCount = NumLoadBehind;
		int32 LoadBehindIndex = PlayHeadFrame - FrameOffset;

		// alternate between look ahead and look behind
		while ((LoadAheadCount > 0) || (LoadBehindCount > 0))
		{
			if (LoadAheadCount > 0)
			{
				if (LoadAheadIndex < 0)
				{
					if (Loop)
					{
						LoadAheadIndex += NumImagePaths;
					}
					else
					{
						LoadAheadCount = 0;
					}
				}
				else if (LoadAheadIndex >= NumImagePaths)
				{
					if (Loop)
					{
						LoadAheadIndex -= NumImagePaths;
					}
					else
					{
						LoadAheadCount = 0;
					}
				}

				if (LoadAheadCount > 0)
				{
					FramesToLoad.Add(LoadAheadIndex);
				}

				LoadAheadIndex += FrameOffset;
				--LoadAheadCount;
			}

			if (LoadBehindCount > 0)
			{
				if (LoadBehindIndex < 0)
				{
					if (Loop)
					{
						LoadBehindIndex += NumImagePaths;
					}
					else
					{
						LoadBehindCount = 0;
					}
				}
				else if (LoadBehindIndex >= NumImagePaths)
				{
					if (Loop)
					{
						LoadBehindIndex -= NumImagePaths;
					}
					else
					{
						LoadBehindCount = 0;
					}
				}

				if (LoadBehindCount > 0)
				{
					FramesToLoad.Add(LoadBehindIndex);
				}

				LoadBehindIndex -= FrameOffset;
				--LoadBehindCount;
			}
		}
	}

	FScopeLock ScopeLock(&CriticalSection);

	// determine queued frame numbers that can be discarded
	for (int32 Idx = QueuedFrameNumbers.Num() - 1; Idx >= 0; --Idx)
	{
		const int32 FrameNumber = QueuedFrameNumbers[Idx];

		if (!FramesToLoad.Contains(FrameNumber))
		{
			QueuedFrameNumbers.RemoveAtSwap(Idx);
		}
	}

	// determine frame numbers that need to be cached
	PendingFrameNumbers.Empty();

	for (int32 FrameNumber : FramesToLoad)
	{
		if ((Frames.FindAndTouch(FrameNumber) == nullptr) && !QueuedFrameNumbers.Contains(FrameNumber))
		{
			PendingFrameNumbers.Add(FrameNumber);
		}
	}

	Algo::Reverse(PendingFrameNumbers);
}


/* IImgMediaLoader interface
 *****************************************************************************/

void FImgMediaLoader::NotifyWorkComplete(FImgMediaLoaderWork& CompletedWork, int32 FrameNumber, const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>& Frame)
{
	FScopeLock Lock(&CriticalSection);

	// if frame is still needed, add it to the cache
	if (QueuedFrameNumbers.Remove(FrameNumber) > 0)
	{
		if (Frame.IsValid())
		{
			UE_LOG(LogImgMedia, VeryVerbose, TEXT("Loader %p: Loaded frame %i"), this, FrameNumber);
			Frames.Add(FrameNumber, Frame);
		}
	}

	WorkPool.Push(&CompletedWork);
}
