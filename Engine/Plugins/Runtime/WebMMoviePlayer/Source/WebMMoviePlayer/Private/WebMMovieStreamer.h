// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MoviePlayer.h"
#include "WebMAudioBackend.h"

class FWebMVideoDecoder;
class FWebMAudioDecoder;
class FMediaSamples;
class FWebMContainer;
class FRunnableThread;

class FWebMMovieStreamer : public IMovieStreamer
{
public:
	FWebMMovieStreamer();
	~FWebMMovieStreamer();

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

	bool DecodeMoreFramesInAnotherThread();
	bool IsPlaying() const { return bPlaying; }
	int32 GetFramesCurrentlyProcessing() { return FramesCurrentlyProcessing; }

private:
	class FWebMBackgroundReader;
	TArray<FString> MovieQueue;
	FString MovieName;
	TUniquePtr<FWebMVideoDecoder> VideoDecoder;
	TUniquePtr<FWebMAudioDecoder> AudioDecoder;
	TUniquePtr<FWebMContainer> Container;
	TUniquePtr<FWebMAudioBackend> AudioBackend;
	TUniquePtr<FRunnableThread> BackgroundReaderThread;
	TUniquePtr<FWebMBackgroundReader> BackgroundReader;
	TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> Samples;
	TSharedPtr<FMovieViewport> Viewport;
	TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe> SlateVideoTexture;
	int32 FramesCurrentlyProcessing;
	float CurrentTime;
	bool bPlaying;

	bool StartNextMovie();
	void ReleaseAcquiredResources();
	bool DisplayFrames(float InDeltaTime);
	bool SendAudio(float InDeltaTime);
};
