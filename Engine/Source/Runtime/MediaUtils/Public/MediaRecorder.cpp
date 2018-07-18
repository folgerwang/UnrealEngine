// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaRecorder.h"

#include "Async/Async.h"
#include "Containers/UnrealString.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "IMediaClock.h"
#include "IMediaClockSink.h"
#include "IMediaModule.h"
#include "Modules/ModuleManager.h"
#include "Serialization/Archive.h"

#include "MediaPlayerFacade.h"


/* Local helpers
 *****************************************************************************/

/**
 * Media clock sink for media textures.
 */
class FMediaRecorderClockSink
	: public IMediaClockSink
{
public:

	FMediaRecorderClockSink(FMediaRecorder& InOwner)
		: Owner(InOwner)
	{ }

	virtual ~FMediaRecorderClockSink() { }

public:

	virtual void TickOutput(FTimespan DeltaTime, FTimespan Timecode) override
	{
		Owner.TickRecording(Timecode);
	}

private:

	FMediaRecorder& Owner;
};


/* FMediaRecorder structors
 *****************************************************************************/

FMediaRecorder::FMediaRecorder()
	: FrameCount(0)
	, Recording(false)
{ }


/* FMediaRecorder interface
 *****************************************************************************/

void FMediaRecorder::StartRecording(const TSharedRef<FMediaPlayerFacade, ESPMode::ThreadSafe>& PlayerFacade)
{
	if (Recording)
	{
		StopRecording();
	}

	IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

	if (MediaModule == nullptr)
	{
		return;
	}

	SampleQueue = MakeShared<FMediaTextureSampleQueue, ESPMode::ThreadSafe>();
	PlayerFacade->AddVideoSampleSink(SampleQueue.ToSharedRef());

	ClockSink = MakeShared<FMediaRecorderClockSink, ESPMode::ThreadSafe>(*this);
	MediaModule->GetClock().AddSink(ClockSink.ToSharedRef());

	FrameCount = 0;
	Recording = true;
}


void FMediaRecorder::StopRecording()
{
	Recording = false;

	ClockSink.Reset();
	SampleQueue.Reset();
}


/* FMediaRecorder interface
 *****************************************************************************/

void FMediaRecorder::TickRecording(FTimespan Timecode)
{
	if (!Recording)
	{
		return; // not recording
	}

	check(SampleQueue.IsValid());

	IImageWrapperModule* ImageWrapperModule = FModuleManager::LoadModulePtr<IImageWrapperModule>(FName("ImageWrapper"));

	if (ImageWrapperModule == nullptr)
	{
		while (SampleQueue->Pop());
	}
	else
	{
		TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> Sample;

		while (SampleQueue->Dequeue(Sample) && Sample.IsValid())
		{
			const void* SampleBuffer = Sample->GetBuffer();

			if (SampleBuffer == nullptr)
			{
				continue; // only raw samples supported right now
			}

			if (Sample->GetDim().GetMin() == 0)
			{
				continue; // nothing to save
			}

			if (Sample->GetFormat() != EMediaTextureSampleFormat::CharBGRA)
			{
				continue; // only supports BGRA for now
			}

			TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule->CreateImageWrapper(EImageFormat::PNG);

			if (!ImageWrapper.IsValid())
			{
				continue; // failed to create image wrapper
			}

			const int32 FrameNumber = FrameCount;

			// convert, compress and write frame asynchronously
			TFunction<void()> AsyncWrite = [FrameNumber, ImageWrapper, Sample]()
			{
				const FIntPoint BufferDim = Sample->GetDim();
				const SIZE_T BufferSize = BufferDim.X * BufferDim.Y * 4;
				const FString Filename = FString::Printf(TEXT("%08d.png"), FrameNumber);

				IFileManager* FileManager = &IFileManager::Get();
				FArchive* Archive = FileManager->CreateFileWriter(*Filename);

				if (Archive != nullptr)
				{
					ImageWrapper->SetRaw(Sample->GetBuffer(), BufferSize, BufferDim.X, BufferDim.Y, ERGBFormat::BGRA, 32);

					const TArray<uint8>& CompressedData = ImageWrapper->GetCompressed((int32)EImageCompressionQuality::Default);
					const int32 CompressedSize = CompressedData.Num();

					Archive->Serialize((void*)CompressedData.GetData(), CompressedSize);

					delete Archive;
				}
			};

			Async(EAsyncExecution::ThreadPool, AsyncWrite);

			++FrameCount;
		}
	}
}
