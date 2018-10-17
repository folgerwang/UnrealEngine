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

DEFINE_LOG_CATEGORY(LogWebMMoviePlayer);

FWebMMovieStreamer::FWebMMovieStreamer()
	: CurrentTexture(0)
	, Samples(new FMediaSamples())
	, CurrentTime(0)
	, bPlaying(false)
{
	Viewport = MakeShareable(new FMovieViewport());

	AudioBackend = MakeUnique<FWebMAudioBackend>();
	AudioBackend->InitializePlatform();
}

FWebMMovieStreamer::~FWebMMovieStreamer()
{
	Cleanup();

	AudioBackend->ShutdownPlatform();
}

void FWebMMovieStreamer::Cleanup()
{
	VideoDecoder.Reset();
	AudioDecoder.Reset();
	Container.Reset();

	ReleaseAcquiredResources();

	AudioBackend->StopStreaming();
}

FTexture2DRHIRef FWebMMovieStreamer::GetTexture()
{
	return BufferedVideoTextures[CurrentTexture].IsValid() ? BufferedVideoTextures[CurrentTexture]->GetRHIRef() : nullptr;
}

bool FWebMMovieStreamer::Init(const TArray<FString>& InMoviePaths, TEnumAsByte<EMoviePlaybackType> InPlaybackType)
{
	// Initializes the streamer for audio and video playback of the given path(s).
	// NOTE: If multiple paths are provided, it is expect that they be played back seamlessly.

	// Add the given paths to the movie queue
	MovieQueue.Append(InMoviePaths);

	// start our first movie playing
	bPlaying = StartNextMovie();

	// Play the next movie in the queue
	return bPlaying;
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

		bHaveThingsToDo |= DecodeMoreFrames();

		CurrentTime += InDeltaTime;

		return !bHaveThingsToDo;
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
	// NOTE: Called from the main thread
	for (int32 i = 0; i < NumBufferedTextures; ++i)
	{
		TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe>& VideoTexture = BufferedVideoTextures[i];
		if (VideoTexture.IsValid())
		{
			// Schedule the release of the video texture(s) and wait the release to complete before
			// losing the reference to the texture
			BeginReleaseResource(VideoTexture.Get());
			FlushRenderingCommands();
			VideoTexture.Reset();
		}
	}

	Viewport->SetTexture(nullptr);
}

bool FWebMMovieStreamer::DisplayFrames(float InDeltaTime)
{
	TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> VideoSample;
	TRange<FTimespan> TimeRange(FTimespan::Zero(), FTimespan::FromSeconds(CurrentTime));
	bool bFoundSample = Samples->FetchVideo(TimeRange, VideoSample);

	if (bFoundSample && VideoSample)
	{
		CurrentTexture = (CurrentTexture + 1) % NumBufferedTextures;

		FWebMMediaTextureSample* WebMSample = StaticCast<FWebMMediaTextureSample*>(VideoSample.Get());

		if (!BufferedVideoTextures[CurrentTexture].IsValid())
		{
			BufferedVideoTextures[CurrentTexture] = MakeShareable(new FSlateTexture2DRHIRef(nullptr, 0, 0));
		}

		BufferedVideoTextures[CurrentTexture]->SetRHIRef(WebMSample->GetTextureRef(), WebMSample->GetDim().X, WebMSample->GetDim().Y);

		Viewport->SetTexture(BufferedVideoTextures[CurrentTexture]);
	}

	return Samples->NumVideoSamples() > 0 || VideoDecoder->IsBusy();
}

bool FWebMMovieStreamer::SendAudio(float InDeltaTime)
{
	TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe> AudioSample;
	TRange<FTimespan> TimeRange(FTimespan::Zero(), FTimespan::FromSeconds(CurrentTime));
	bool bFoundSample = Samples->FetchAudio(TimeRange, AudioSample);

	if (bFoundSample && AudioSample)
	{
		FWebMMediaAudioSample* WebMSample = StaticCast<FWebMMediaAudioSample*>(AudioSample.Get());

		AudioBackend->SendAudio(WebMSample->GetDataBuffer().GetData(), WebMSample->GetDataBuffer().Num());
	}

	return Samples->NumAudio() > 0 || AudioDecoder->IsBusy();
}

bool FWebMMovieStreamer::DecodeMoreFrames()
{
	// Read frames up to 1 secs in the future
	FTimespan ReadBufferLength = FTimespan::FromSeconds(0.1);

	TArray<TSharedPtr<FWebMFrame>> AudioFrames;
	TArray<TSharedPtr<FWebMFrame>> VideoFrames;

	Container->ReadFrames(ReadBufferLength, AudioFrames, VideoFrames);

	// Trigger video decoding
	if (VideoFrames.Num() > 0)
	{
		VideoDecoder->DecodeVideoFramesAsync(VideoFrames);
	}

	// Trigger audio decoding
	if (AudioFrames.Num() > 0)
	{
		AudioDecoder->DecodeAudioFramesAsync(AudioFrames);
	}

	return VideoFrames.Num() > 0 || AudioFrames.Num() > 0;
}
