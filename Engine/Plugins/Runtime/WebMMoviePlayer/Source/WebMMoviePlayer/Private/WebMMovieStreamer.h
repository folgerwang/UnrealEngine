// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_WEBM_LIBS

#include "MoviePlayer.h"
#include "WebMAudioBackend.h"
#include "WebMMediaFrame.h"
#include "Containers/Queue.h"
#include "WebMSamplesSink.h"

class FWebMVideoDecoder;
class FWebMAudioDecoder;
class FMediaSamples;
class FWebMContainer;

class FWebMMovieStreamer : public IMovieStreamer, public IWebMSamplesSink
{
public:
	FWebMMovieStreamer();
	~FWebMMovieStreamer();

	//~ IWebMSamplesSink interface
	virtual void AddVideoSampleFromDecodingThread(TSharedRef<FWebMMediaTextureSample, ESPMode::ThreadSafe> Sample) override;
	virtual void AddAudioSampleFromDecodingThread(TSharedRef<FWebMMediaAudioSample, ESPMode::ThreadSafe> Sample) override;

	//~ IMediaOutput interface
	virtual bool Init(const TArray<FString>& InMoviePaths, TEnumAsByte<EMoviePlaybackType> InPlaybackType) override;
	virtual void ForceCompletion() override;
	virtual bool Tick(float InDeltaTime) override;
	virtual TSharedPtr<class ISlateViewport> GetViewportInterface() override;
	virtual float GetAspectRatio() const override;
	virtual FString GetMovieName() override;
	virtual bool IsLastMovieInPlaylist() override;
	virtual void Cleanup() override;
	virtual FTexture2DRHIRef GetTexture() override;

	FOnCurrentMovieClipFinished OnCurrentMovieClipFinishedDelegate;
	virtual FOnCurrentMovieClipFinished& OnCurrentMovieClipFinished() override { return OnCurrentMovieClipFinishedDelegate; }

private:
	TArray<FString> MovieQueue;
	TQueue<TArray<TSharedPtr<FWebMFrame>>> VideoFramesToDecodeLater;
	FString MovieName;
	TUniquePtr<FWebMVideoDecoder> VideoDecoder;
	TUniquePtr<FWebMAudioDecoder> AudioDecoder;
	TUniquePtr<FWebMContainer> Container;
	TUniquePtr<FWebMAudioBackend> AudioBackend;
	TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> Samples;
	TSharedPtr<FMovieViewport> Viewport;
	TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe> SlateVideoTexture;
	int32 VideoFramesCurrentlyProcessing;
	double StartTime;
	bool bPlaying;

	/** 
	 * Number of ticks to wait after the movie is complete before moving on to the next one. 
	 * 
	 * This allows us to defer texture deletion while it is being displayed.
	 */
	int32 TicksLeftToWaitPostCompletion;

	bool StartNextMovie();
	void ReleaseAcquiredResources();
	bool DisplayFrames(float InDeltaTime);
	bool SendAudio(float InDeltaTime);
	bool ReadMoreFrames();
};

#endif // WITH_WEBM_LIBS
