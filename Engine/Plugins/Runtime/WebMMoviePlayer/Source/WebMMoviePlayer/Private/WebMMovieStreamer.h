// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MoviePlayer.h"
#include "WebMAudioBackend.h"

class FWebMVideoDecoder;
class FWebMAudioDecoder;
class FMediaSamples;
class FWebMContainer;

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

private:
	//Theoretically only need 2 buffered textures, but we have extra to avoid needing to make a copy of the AvPlayer data to pass to an RHI thread command.  Instead, we buffer deeper and update the textures on the Render thread.
	static const int32 NumBufferedTextures = 4;

	/** Texture and viewport data for displaying to Slate.  SlateVideoTexture is always used by the viewport, but it's texture reference is swapped out when a new frame is available.
	 Method assumes 1 Tick() call per frame, and that the Streamer Tick comes before Slate rendering */
	TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe> BufferedVideoTextures[NumBufferedTextures];
	int32 CurrentTexture;

	TArray<FString> MovieQueue;
	FString MovieName;
	TUniquePtr<FWebMVideoDecoder> VideoDecoder;
	TUniquePtr<FWebMAudioDecoder> AudioDecoder;
	TUniquePtr<FWebMContainer> Container;
	TUniquePtr<FWebMAudioBackend> AudioBackend;
	TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> Samples;
	TSharedPtr<FMovieViewport> Viewport;
	float CurrentTime;
	bool bPlaying;

	bool StartNextMovie();
	void ReleaseAcquiredResources();
	bool DisplayFrames(float InDeltaTime);
	bool SendAudio(float InDeltaTime);
	bool DecodeMoreFrames();
};
