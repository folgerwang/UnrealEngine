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
#include "IMediaEventSink.h"
#include "IMediaOptions.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"
#include "Templates/Atomic.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "RHIResources.h"
#include "HAL/ThreadSafeCounter.h"
#include "Containers/Queue.h"
#include "Serialization/Archive.h"
#include "Serialization/ArrayReader.h"
#include "MediaCodecInputWorker.h"

#ifndef EGL_EGLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#endif // !EGL_EGLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif // !GL_GLEXT_PROTOTYPES

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "ml_api.h"
#include "ml_media_codec.h"
#include "ml_audio.h"
#include "ml_media_data_source.h"

class FMediaSamples;
class IMediaEventSink;
struct FMagicLeapVideoTextureData;
class FMagicLeapMediaAudioSamplePool;

/**
*	Implement media playback using the MagicLeap MediaPlayer interface.
*/
class FMagicLeapMediaCodecPlayer : public IMediaPlayer, public IMediaControls, public IMediaCache, public IMediaTracks, public IMediaView
{
public:

	/**
	* Create and initialize a new instance.
	*
	* @param InEventSink The object that receives media events from this player.
	*/
	FMagicLeapMediaCodecPlayer(IMediaEventSink& InEventSink);

	/** Virtual destructor. */
	virtual ~FMagicLeapMediaCodecPlayer();

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
	virtual void TickAudio() override;
	virtual void SetLastAudioRenderedSampleTime(FTimespan SampleTime) override;

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

public:
	void QueueMediaEvent(EMediaEvent MediaEvent);
	void FlushCodecs();
	bool GetCodecForTrackIndex(int64_t TrackIndex, MLHandle& SampleCodecHandle) const;
	bool IsPlaybackCompleted() const;
	void SetPlaybackCompleted(bool PlaybackCompleted);
	void QueueVideoCodecStartTimeReset();

	int64_t MediaDataSourceReadAt(MLHandle media_data_source, size_t position, size_t size, uint8_t *buffer);
	int64_t MediaDataSourceGetSize(MLHandle media_data_source);
	void MediaDataSourceClose(MLHandle media_data_source);

private:
	/** Callback for when the application resumed in the foreground. */
	void HandleApplicationHasEnteredForeground();

	/** Callback for when the application is being paused in the background. */
	void HandleApplicationWillEnterBackground();

	/** Track information */
	// TODO: Move or rename, get rid of height/width since this is used for both audio and video
	// Perhaps an FAudioTrackData and FVideoTrackData inheriting from FTrackData?
	// Also make the codec handle a part of this struct. Will make it easier to perform all operations.
	struct FTrackData 
	{
		FTrackData(FString Mime, int32 Index) : MimeType(Mime), TrackIndex(Index) {}
		FString MimeType;
		int32 TrackIndex = 0;
		int64 Position = 0;
		FTimespan Duration = 0;
		int32 Height = 0;
		int32 Width = 0;
		bool IsPlaying = false;
		MLMediaCodecBufferInfo CurrentBufferInfo = {};
		int64 CurrentBufferIndex = 0;
		bool bBufferPendingRender = false;
		FTimespan StartPresentationTime = FTimespan::Zero();
		FTimespan LastPresentationTime = FTimespan::Zero();
		FTimespan LastSampleQueueTime = FTimespan::Zero();
		FTimespan SampleDuration = FTimespan::Zero();
		int32 SampleRate = 0;
		int32 ChannelCount = 0;
		FString FormatName = "";
		FString Language = "";
		int32 FrameRate = 30;
	};

	enum EMediaSourceType
	{
		VideoOnly,
		AudioOnly,
		VideoAndAudio
	};

protected:
	FIntPoint GetVideoDimensions() const;

	MLHandle AudioCodecHandle;
	MLHandle VideoCodecHandle;
	MLHandle MediaExtractorHandle;
	MLHandle MediaDataSourceHandle;

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

	/** Audio sample object pool. */
	FMagicLeapMediaAudioSamplePool* AudioSamplePool;

	TSharedPtr<FMagicLeapVideoTextureData, ESPMode::ThreadSafe> TextureData;

	TMap<EMediaTrackType, TArray<FTrackData>> TrackInfo;
	TMap<EMediaTrackType, int32> SelectedTrack;

	// Used for bPlaybackCompleted, CurrentState, bLoopPlayback, bResetVideoCodecStartTime, bReachedInputEndOfStream
	FCriticalSection CriticalSection;
	// Lock between Game thread and Input worker thread, used for synchronizing the codec flush operation.
	FCriticalSection GT_IT_Mutex;
	// Lock between Render thread and Input worker thread, used for synchronizing the codec flush operation.
	FCriticalSection RT_IT_Mutex;

	bool bWasMediaPlayingBeforeAppPause;
	TAtomic<FTimespan> CurrentPlaybackTime;
	TAtomic<FTimespan> LastAudioRenderedSampleTime;
	EMediaSourceType MediaSourceType;

	bool bLoopPlayback;
	bool bPlaybackCompleted;
	// Render thread cache of bPlaybackCompleted, used to clear out the VideoTexturePool for Vulkan without requiring a lock.
	bool bPlaybackCompleted_RenderThread;
	bool bIsBufferAvailable;
	bool bReachedOutputEndOfStream;
	// TODO: move to track.
	bool bResetVideoCodecStartTime;

	TQueue<EMediaEvent, EQueueMode::Spsc> MediaEventQueue;

	FMediaCodecInputWorker InputWorker;

	TSharedPtr<FArchive, ESPMode::ThreadSafe> DataSourceArchive;

private:
	// Non-Interface functions
	virtual void GetTrackInformation();
	void UpdateCommonTrackInfo(MLHandle FormatHandle, FTrackData& CurrentTrackData);
	void UpdateAudioTrackInfo(MLHandle FormatHandle, FTrackData& CurrentTrackData);
	void UpdateVideoTrackInfo(MLHandle FormatHandle, FTrackData& CurrentTrackData);
	virtual bool StopAndReset(EMediaTrackType TrackType, MLHandle& CodecHandle);
	virtual bool SetRateOne();
	bool CreateMediaCodec(EMediaTrackType TrackType, MLHandle& CodecHandle);
	void StartMediaCodec(MLHandle& CodecHandle);
	bool UpdateTransformMatrix_RenderThread(MLHandle InVideoCodecHandle);
	virtual void RegisterExternalTexture_RenderThread(const FGuid& InGuid, FTextureRHIRef& InTextureRHI, FSamplerStateRHIRef& InSamplerStateRHI);
	virtual bool IsBufferAvailable_RenderThread(MLHandle MediaCodecHandle);
	virtual bool GetNativeBuffer_RenderThread(const MLHandle InMediaCodecHandle, MLHandle& NativeBuffer);
	virtual bool ReleaseNativeBuffer_RenderThread(const MLHandle InMediaCodecHandle, MLHandle NativeBuffer);
	virtual bool GetCurrentPosition_RenderThread(const MLHandle InMediaCodecHandle, int32& CurrentPosition);
	bool ProcessAudioOutputSample(MLHandle& CodecHandle, FTrackData& CurrentTrackData, FTimespan LastAudioSampleTime);
	bool ProcessVideoOutputSample_RenderThread(MLHandle& CodecHandle, FTrackData& CurrentTrackData, const FTimespan& Timecode);
	bool WriteAudioSample(FTrackData& CurrentTrackData, const uint8* SampleBuffer, uint64 SampleSize);
};
