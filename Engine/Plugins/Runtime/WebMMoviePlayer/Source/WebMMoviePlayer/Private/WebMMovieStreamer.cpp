// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "WebMMovieStreamer.h"
#include "MediaShaders.h"
#include "MediaSamples.h"
#include "WebMMovieCommon.h"
#include "Misc/Paths.h"
#include "SceneUtils.h"
#include "StaticBoundShaderState.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "WebMVideoDecoder.h"
#include "WebMAudioDecoder.h"
#include "WebMMediaFrame.h"
#include "WebMContainer.h"
#include "WebMMediaAudioSample.h"
#include "WebMMediaTextureSample.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/SingleThreadRunnable.h"

DEFINE_LOG_CATEGORY(LogWebMMoviePlayer);

class FWebMMovieStreamer::FWebMBackgroundReader : public FRunnable, private FSingleThreadRunnable
{
public:
	FWebMBackgroundReader(FWebMMovieStreamer& InStreamer)
		: Streamer(InStreamer)
		, bIsFinished(false)
	{
	}

	//~ FRunnable interface
	virtual FSingleThreadRunnable* GetSingleThreadInterface() override
	{
		return this;
	}

	virtual uint32 Run() override
	{
		bIsFinished = false;

		while (!bIsFinished && Streamer.IsPlaying())
		{
			if (Streamer.GetFramesCurrentlyProcessing() < 30)
			{
				bIsFinished = Streamer.DecodeMoreFramesInAnotherThread();
			}
		}

		return 0;
	}

protected:
	//~ FSingleThreadRunnable interface
	virtual void Tick() override
	{
		if (!bIsFinished && Streamer.IsPlaying())
		{
			bIsFinished = Streamer.DecodeMoreFramesInAnotherThread();
		}
	}

private:
	FWebMMovieStreamer& Streamer;
	bool bIsFinished;
};

FWebMMovieStreamer::FWebMMovieStreamer()
	: AudioBackend(MakeUnique<FWebMAudioBackend>())
	, BackgroundReader(MakeUnique<FWebMBackgroundReader>(*this))
	, Viewport(MakeShareable(new FMovieViewport()))
	, FramesCurrentlyProcessing(0)
	, CurrentTime(0)
	, bPlaying(false)
{
	AudioBackend->InitializePlatform();
}

FWebMMovieStreamer::~FWebMMovieStreamer()
{
	Cleanup();

	AudioBackend->ShutdownPlatform();
}

void FWebMMovieStreamer::Cleanup()
{
	bPlaying = false;

	if (BackgroundReaderThread)
	{
		BackgroundReaderThread->WaitForCompletion();
		BackgroundReaderThread.Reset();
	}

	Samples.Reset();
	VideoDecoder.Reset();
	AudioDecoder.Reset();
	Container.Reset();

	ReleaseAcquiredResources();

	AudioBackend->StopStreaming();
}

FTexture2DRHIRef FWebMMovieStreamer::GetTexture()
{
	return SlateVideoTexture.IsValid() ? SlateVideoTexture->GetRHIRef() : nullptr;
}

bool FWebMMovieStreamer::Init(const TArray<FString>& InMoviePaths, TEnumAsByte<EMoviePlaybackType> InPlaybackType)
{
	// Initializes the streamer for audio and video playback of the given path(s).
	// NOTE: If multiple paths are provided, it is expect that they be played back seamlessly.

	// Add the given paths to the movie queue
	MovieQueue.Append(InMoviePaths);

	// start our first movie playing
	return StartNextMovie();
}

bool FWebMMovieStreamer::StartNextMovie()
{
	if (MovieQueue.Num() > 0)
	{
		Cleanup();

		MovieName = MovieQueue[0];

		MovieQueue.RemoveAt(0);

		FString MoviePath = FPaths::ProjectContentDir() + TEXT("Movies/") + MovieName + TEXT(".webm");

		if (FPaths::FileExists(MoviePath))
		{
			Container.Reset(new FWebMContainer());
		}
		else
		{
			UE_LOG(LogWebMMoviePlayer, Error, TEXT("Movie '%s' not found."));
		}

		UE_LOG(LogWebMMoviePlayer, Verbose, TEXT("Starting '%s'"), *MoviePath);

		if (!Container->Open(MoviePath))
		{
			MovieName = FString();
			return false;
		}

		Samples = MakeShareable(new FMediaSamples());
		AudioDecoder.Reset(new FWebMAudioDecoder(Samples));
		VideoDecoder.Reset(new FWebMVideoDecoder(Samples));

		FWebMAudioTrackInfo DefaultAudioTrack = Container->GetCurrentAudioTrackInfo();
		check(DefaultAudioTrack.bIsValid);

		FWebMVideoTrackInfo DefaultVideoTrack = Container->GetCurrentVideoTrackInfo();
		check(DefaultVideoTrack.bIsValid);

		AudioDecoder->Initialize(DefaultAudioTrack.CodecName, DefaultAudioTrack.SampleRate, DefaultAudioTrack.NumOfChannels, DefaultAudioTrack.CodecPrivateData, DefaultAudioTrack.CodecPrivateDataSize);
		VideoDecoder->Initialize(DefaultVideoTrack.CodecName);

		AudioBackend->StartStreaming(DefaultAudioTrack.SampleRate, DefaultAudioTrack.NumOfChannels);

		CurrentTime = 0;
		bPlaying = true;

		BackgroundReaderThread.Reset(FRunnableThread::Create(BackgroundReader.Get(), TEXT("FWebMBackgroundReader"), 0, TPri_BelowNormal));

		return true;
	}
	else
	{
		UE_LOG(LogWebMMoviePlayer, Verbose, TEXT("No Movie to start."));
		return false;
	}
}

FString FWebMMovieStreamer::GetMovieName()
{
	return MovieName;
}

bool FWebMMovieStreamer::IsLastMovieInPlaylist()
{
	return MovieQueue.Num() == 0;
}

bool FWebMMovieStreamer::Tick(float InDeltaTime)
{
	if (bPlaying)
	{
		bool bHaveThingsToDo = false;

		bHaveThingsToDo |= DisplayFrames(InDeltaTime);
		bHaveThingsToDo |= SendAudio(InDeltaTime);

		CurrentTime += InDeltaTime;

		if (bHaveThingsToDo)
		{
			// We're still playing this movie
			return false;
		}
		else
		{
			// Try to start next movie from the queue
			return !StartNextMovie();
		}
	}
	else
	{
		// We're done playing
		return true;
	}
}

TSharedPtr<class ISlateViewport> FWebMMovieStreamer::GetViewportInterface()
{
	return Viewport;
}

float FWebMMovieStreamer::GetAspectRatio() const
{
	return static_cast<float>(Viewport->GetSize().X) / static_cast<float>(Viewport->GetSize().Y);
}

void FWebMMovieStreamer::ForceCompletion()
{
	if (bPlaying)
	{
		bPlaying = false;
	}

	MovieQueue.Empty();
}

void FWebMMovieStreamer::ReleaseAcquiredResources()
{
	SlateVideoTexture.Reset();
	Viewport->SetTexture(nullptr);
}

bool FWebMMovieStreamer::DisplayFrames(float InDeltaTime)
{
	if (!SlateVideoTexture.IsValid())
	{
		SlateVideoTexture = MakeShareable(new FSlateTexture2DRHIRef(nullptr, 0, 0));
	}

	TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> VideoSample;
	TRange<FTimespan> TimeRange(FTimespan::Zero(), FTimespan::FromSeconds(CurrentTime));
	bool bFoundSample = Samples->FetchVideo(TimeRange, VideoSample);

	if (bFoundSample && VideoSample)
	{
		FPlatformAtomics::InterlockedDecrement(&FramesCurrentlyProcessing);

		FWebMMediaTextureSample* WebMSample = StaticCast<FWebMMediaTextureSample*>(VideoSample.Get());

		if (SlateVideoTexture->IsValid())
		{
			SlateVideoTexture->ReleaseDynamicRHI();
		}

		SlateVideoTexture->SetRHIRef(WebMSample->GetTextureRef(), WebMSample->GetDim().X, WebMSample->GetDim().Y);

		Viewport->SetTexture(SlateVideoTexture);
	}

	return Samples->NumVideoSamples() > 0 || VideoDecoder->IsBusy();
}

bool FWebMMovieStreamer::SendAudio(float InDeltaTime)
{
	// Just send all available audio for processing
	
	TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe> AudioSample;
	TRange<FTimespan> TimeRange(FTimespan::Zero(), FTimespan::MaxValue());
	bool bFoundSample = Samples->FetchAudio(TimeRange, AudioSample);

	while (bFoundSample && AudioSample)
	{
		FWebMMediaAudioSample* WebMSample = StaticCast<FWebMMediaAudioSample*>(AudioSample.Get());
		AudioBackend->SendAudio(WebMSample->GetDataBuffer().GetData(), WebMSample->GetDataBuffer().Num());

		bFoundSample = Samples->FetchAudio(TimeRange, AudioSample);
	}

	return Samples->NumAudio() > 0 || AudioDecoder->IsBusy();
}

bool FWebMMovieStreamer::DecodeMoreFramesInAnotherThread()
{
	FTimespan ReadBufferLength = FTimespan::FromSeconds(0.1);

	TArray<TSharedPtr<FWebMFrame>> AudioFrames;
	TArray<TSharedPtr<FWebMFrame>> VideoFrames;

	Container->ReadFrames(ReadBufferLength, AudioFrames, VideoFrames);

	// Trigger video decoding
	if (VideoFrames.Num() > 0)
	{
		FPlatformAtomics::InterlockedAdd(&FramesCurrentlyProcessing, VideoFrames.Num());

		VideoDecoder->DecodeVideoFramesAsync(VideoFrames);
	}

	// Trigger audio decoding
	if (AudioFrames.Num() > 0)
	{
		AudioDecoder->DecodeAudioFramesAsync(AudioFrames);
	}

	return VideoFrames.Num() == 0 && AudioFrames.Num() == 0;
}
