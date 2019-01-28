// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "Misc/Guid.h"
#include "IMediaCache.h"
#include "IMediaControls.h"
#include "IMediaPlayer.h"
#include "IMediaTracks.h"
#include "IMediaView.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"
#include "Templates/Atomic.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "RHIResources.h"

#include "ml_api.h"

class FMediaSamples;
class IMediaEventSink;
class FMagicLeapMediaTextureSamplePool;
struct FMagicLeapVideoTextureData;

/**
 *	Implement media playback using the MagicLeap MediaPlayer interface.
 */
class FMagicLeapMediaPlayer : public IMediaPlayer, public IMediaControls, public IMediaCache, public IMediaTracks, public IMediaView
{
public:
  
	/**
	 * Create and initialize a new instance.
	 *
	 * @param InEventSink The object that receives media events from this player.
	 */
	FMagicLeapMediaPlayer(IMediaEventSink& InEventSink);

	/** Virtual destructor. */
	virtual ~FMagicLeapMediaPlayer();

public:
  /** IMediaPlayer interface */
	virtual void Close() override;
	virtual IMediaCache& GetCache() override;
	virtual IMediaControls& GetControls() override;
	virtual FString GetInfo() const override;
	virtual FName GetPlayerName() const override;
	virtual IMediaSamples& GetSamples() override;
	virtual FString GetStats() const override;
	virtual IMediaTracks& GetTracks() override;
	virtual FString GetUrl() const override;
	virtual IMediaView& GetView() override;
	virtual bool Open(const FString& Url, const IMediaOptions* Options) override;
	virtual bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* Options) override;
	virtual void SetGuid(const FGuid& Guid) override;
	virtual void TickFetch(FTimespan DeltaTime, FTimespan Timecode) override;
	virtual void TickInput(FTimespan DeltaTime, FTimespan Timecode) override;

public:
	/** IMediaControls interface */
	virtual bool CanControl(EMediaControl Control) const override;
	virtual FTimespan GetDuration() const override;
	virtual float GetRate() const override;
	virtual EMediaState GetState() const override;
	virtual EMediaStatus GetStatus() const override;
	virtual TRangeSet<float> GetSupportedRates(EMediaRateThinning Thinning) const override;
	virtual FTimespan GetTime() const override;
	virtual bool IsLooping() const override;
	virtual bool Seek(const FTimespan& Time) override;
	virtual bool SetLooping(bool Looping) override;
	virtual bool SetRate(float Rate) override;
	virtual bool SetNativeVolume(float Volume) override;

public:
	/** IMediaTracks interface */
	virtual bool GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const override;
	virtual int32 GetNumTracks(EMediaTrackType TrackType) const override;
	virtual int32 GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual int32 GetSelectedTrack(EMediaTrackType TrackType) const override;
	virtual FText GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual int32 GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual bool GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const override;
	virtual bool SelectTrack(EMediaTrackType TrackType, int32 TrackIndex) override;
	virtual bool SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex) override;

private:
	/** Callback for when the application resumed in the foreground. */
	void HandleApplicationHasEnteredForeground();

	/** Callback for when the application is being paused in the background. */
	void HandleApplicationWillEnterBackground();

protected:

	// TODO: mature this to return track dimensions.
	FIntPoint GetVideoDimensions() const;

	MLHandle MediaPlayerHandle;

	bool bMediaPrepared;

	/** Frame UV scale and offsets */
	float UScale = 1.0f;
	float VScale = 1.0f;
	float UOffset = 0.0f;
	float VOffset = 0.0f;

	/** Frame transformation matrix used to determine UV offset and scaling */
	float FrameTransformationMatrix[16] = {};

	/** Current player state. */
	EMediaState CurrentState;

	/** The media event handler. */
	IMediaEventSink& EventSink;

	/** Media information string. */
	FString Info;

	/** Currently opened media. */
	FString MediaUrl;

	/** Media player Guid */
	FGuid PlayerGuid;

	/** Foreground/background delegate for pause. */
	FDelegateHandle PauseHandle;

	/** Foreground/background delegate for resume. */
	FDelegateHandle ResumeHandle;

	/** The media sample queue. */
	TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> Samples;

	/** Video sample object pool. */
	FMagicLeapMediaTextureSamplePool* VideoSamplePool;

	TSharedPtr<FMagicLeapVideoTextureData, ESPMode::ThreadSafe> TextureData;

	TMap<EMediaTrackType, TArray<int32>> TrackInfo;

	TMap<EMediaTrackType, int32> SelectedTrack;

	class FMediaWorker* MediaWorker;

	FCriticalSection CriticalSection;

	bool bWasMediaPlayingBeforeAppPause;

	bool bPlaybackCompleted;

	TAtomic<FTimespan> CurrentPlaybackTime;

private:
	virtual bool SetRateOne();
	virtual bool GetMediaPlayerState(uint16 FlagToPoll) const;
	virtual void RegisterExternalTexture(const FGuid& InGuid, FTextureRHIRef& InTextureRHI, FSamplerStateRHIRef& InSamplerStateRHI);
	virtual bool RenderThreadIsBufferAvailable(MLHandle MediaPlayerHandle);
	virtual bool RenderThreadGetNativeBuffer(const MLHandle MediaPlayerHandle, MLHandle& NativeBuffer, bool& OutIsVideoTextureValid);
	virtual bool RenderThreadReleaseNativeBuffer(const MLHandle MediaPlayerHandle, MLHandle NativeBuffer);
	virtual bool RenderThreadGetCurrentPosition(const MLHandle MediaPlayerHandle, int32& CurrentPosition);
};
