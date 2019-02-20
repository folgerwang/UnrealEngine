// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapMediaCodecPlayer.h"
#include "IMagicLeapMediaCodecModule.h"
#include "Misc/Paths.h"
#include "Lumin/LuminPlatformFile.h"
#include "Misc/FileHelper.h"
#include "MediaSamples.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ScopeTryLock.h"
#include "HAL/PlatformAtomics.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "ExternalTexture.h"
#include "MagicLeapMediaAudioSample.h"
#include "Lumin/LuminEGL.h"
#include "MagicLeapHelperVulkan.h"

#if WITH_MLSDK
#include "ml_version.h"
#include "ml_media_extractor.h"
#include "ml_media_codec.h"
#include "ml_media_codeclist.h"
#include "ml_media_format.h"
#include "ml_media_error.h"
#endif // WITH_MLSDK

#if MLSDK_VERSION_MAJOR == 0 && MLSDK_VERSION_MINOR < 18
MLMediaFormatKey MLMediaFormat_Key_Mime = "mime";
MLMediaFormatKey MLMediaFormat_Key_Frame_Rate = "frame-rate";
MLMediaFormatKey MLMediaFormat_Key_Width = "width";
MLMediaFormatKey MLMediaFormat_Key_Height = "height";
MLMediaFormatKey MLMediaFormat_Key_Duration = "durationUs";
MLMediaFormatKey MLMediaFormat_Key_Language = "language";
MLMediaFormatKey MLMediaFormat_Key_Sample_Rate = "sample-rate";
MLMediaFormatKey MLMediaFormat_Key_Channel_Count = "channel-count";
#endif // MLSDK_VERSION_MAJOR == 0 && MLSDK_VERSION_MINOR < 18

#define LOCTEXT_NAMESPACE "FMagicLeapMediaCodecModule"

/** TData source callback setup */
int64_t MediaDataSourceReadAt_Callback(MLHandle media_data_source, size_t position, size_t size, uint8_t *buffer, void *context)
{
	if (context != nullptr)
	{
		return reinterpret_cast<FMagicLeapMediaCodecPlayer*>(context)->MediaDataSourceReadAt(media_data_source, position, size, buffer);
	}

	return -1;
}

int64_t MediaDataSourceGetSize_Callback(MLHandle media_data_source, void *context)
{
	if (context != nullptr)
	{
		return reinterpret_cast<FMagicLeapMediaCodecPlayer*>(context)->MediaDataSourceGetSize(media_data_source);
	}

	return -1;
}

void MediaDataSourceClose_Callback(MLHandle media_data_source, void *context)
{
	if (context != nullptr)
	{
		reinterpret_cast<FMagicLeapMediaCodecPlayer*>(context)->MediaDataSourceClose(media_data_source);
	}
}

/** Texture data setup */
struct FMagicLeapVideoTextureData
{
public:
	FMagicLeapVideoTextureData()
		: bIsVideoTextureValid(false)
		, PreviousNativeBuffer(ML_INVALID_HANDLE)
	{}

	FTextureRHIRef VideoTexture;
	bool bIsVideoTextureValid;
	MLHandle PreviousNativeBuffer;
};

struct FMagicLeapVideoTextureDataVK : public FMagicLeapVideoTextureData
{
	FSamplerStateRHIRef VideoSampler;
	TMap<uint64, FTextureRHIRef> VideoTexturePool;
};

struct FMagicLeapVideoTextureDataGL : public FMagicLeapVideoTextureData
{
public:

	FMagicLeapVideoTextureDataGL()
		: Image(EGL_NO_IMAGE_KHR)
		, Display(EGL_NO_DISPLAY)
		, Context(EGL_NO_CONTEXT)
		, SavedDisplay(EGL_NO_DISPLAY)
		, SavedContext(EGL_NO_CONTEXT)
		, bContextCreated(false)
	{}

	~FMagicLeapVideoTextureDataGL()
	{
		PreviousNativeBuffer = ML_INVALID_HANDLE;
		eglDestroyContext(Display, Context);
		Display = EGL_NO_DISPLAY;
		Context = EGL_NO_CONTEXT;
	}

	bool InitContext()
	{
#if !PLATFORM_LUMINGL4
		if (Context == EGL_NO_CONTEXT)
		{
			Display = LuminEGL::GetInstance()->GetDisplay();
			EGLContext SharedContext = LuminEGL::GetInstance()->GetCurrentContext();
			Context = SharedContext; // LuminEGL::GetInstance()->CreateContext(SharedContext);
		}

		return (Context != EGL_NO_CONTEXT);
#else
		return false;
#endif
	}

	void SaveContext()
	{
#if !PLATFORM_LUMINGL4
		SavedDisplay = LuminEGL::GetInstance()->GetDisplay();
		SavedContext = LuminEGL::GetInstance()->GetCurrentContext();
#endif
	}

	void MakeCurrent()
	{
#if !PLATFORM_LUMINGL4
		return;	// skip for now
		EGLBoolean bResult = eglMakeCurrent(Display, EGL_NO_SURFACE, EGL_NO_SURFACE, Context);
		if (bResult == EGL_FALSE)
		{
			UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("Error setting media player context."));
		}
#endif
	}

	void RestoreContext()
	{
#if !PLATFORM_LUMINGL4
		return;	// skip for now
		EGLBoolean bResult = eglMakeCurrent(SavedDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, SavedContext);
		if (bResult == EGL_FALSE)
		{
			UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("Error setting unreal context."));
		}
#endif
	}

	EGLImageKHR Image;
	EGLDisplay Display;
	EGLContext Context;
	EGLDisplay SavedDisplay;
	EGLContext SavedContext;
	bool bContextCreated;
};

FMagicLeapMediaCodecPlayer::FMagicLeapMediaCodecPlayer(IMediaEventSink& InEventSink)
	: AudioCodecHandle(ML_INVALID_HANDLE)
	, VideoCodecHandle(ML_INVALID_HANDLE)
	, MediaExtractorHandle(ML_INVALID_HANDLE)
	, MediaDataSourceHandle(ML_INVALID_HANDLE)
	, bMediaPrepared(false)
	, CurrentState(EMediaState::Closed)
	, EventSink(InEventSink)
	, Samples(MakeShared<FMediaSamples, ESPMode::ThreadSafe>())
	, AudioSamplePool(new FMagicLeapMediaAudioSamplePool())
	, bWasMediaPlayingBeforeAppPause(false)
	, CurrentPlaybackTime(FTimespan::Zero())
	, bLoopPlayback(false)
	, bPlaybackCompleted(false)
	, bPlaybackCompleted_RenderThread(false)
	, bIsBufferAvailable(false)
	, bResetVideoCodecStartTime(false)
{
	if (FLuminPlatformMisc::ShouldUseVulkan())
	{
		TextureData = MakeShared<FMagicLeapVideoTextureDataVK, ESPMode::ThreadSafe>();
	}
	else
	{
		TextureData = MakeShared<FMagicLeapVideoTextureDataGL, ESPMode::ThreadSafe>();
	}
}

FMagicLeapMediaCodecPlayer::~FMagicLeapMediaCodecPlayer()
{
	Close();

	delete AudioSamplePool;
	AudioSamplePool = nullptr;
}

void FMagicLeapMediaCodecPlayer::Close()
{
	if (CurrentState == EMediaState::Closed || CurrentState == EMediaState::Error)
	{
		return;
	}

	{
		FScopeLock Lock(&CriticalSection);
		bPlaybackCompleted = true;
	}

	InputWorker.DestroyThread();

	CurrentState = EMediaState::Closed;

	// remove delegates if registered
	if (ResumeHandle.IsValid())
	{
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(ResumeHandle);
		ResumeHandle.Reset();
	}

	if (PauseHandle.IsValid())
	{
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(PauseHandle);
		PauseHandle.Reset();
	}

	if (MLHandleIsValid(VideoCodecHandle))
	{
		if (GSupportsImageExternal)
		{
			if (FLuminPlatformMisc::ShouldUseVulkan())
			{
				// Unregister the external texture on render thread
				struct FReleaseVideoResourcesParams
				{
					FMagicLeapMediaCodecPlayer* MediaPlayer;
					TSharedPtr<FMagicLeapVideoTextureDataVK, ESPMode::ThreadSafe> TextureData;
					FGuid PlayerGuid;
					MLHandle VideoCodecHandle;
				};

				FReleaseVideoResourcesParams Params = { this, StaticCastSharedPtr<FMagicLeapVideoTextureDataVK>(TextureData), PlayerGuid, VideoCodecHandle };

				ENQUEUE_RENDER_COMMAND(MagicLeapMediaPlayerDestroy)(
					[Params](FRHICommandListImmediate& RHICmdList)
					{
						FExternalTextureRegistry::Get().UnregisterExternalTexture(Params.PlayerGuid);
						Params.TextureData->bIsVideoTextureValid = false;

						if (Params.TextureData->PreviousNativeBuffer != 0 && MLHandleIsValid(Params.TextureData->PreviousNativeBuffer))
						{
							Params.MediaPlayer->ReleaseNativeBuffer_RenderThread(Params.VideoCodecHandle, Params.TextureData->PreviousNativeBuffer);
							Params.TextureData->PreviousNativeBuffer = 0;
							Params.TextureData->VideoTexturePool.Empty();
						}
					});
			}
			else
			{
				// Unregister the external texture on render thread
				struct FReleaseVideoResourcesParams
				{
					FMagicLeapMediaCodecPlayer* MediaPlayer;
					// Can we make this a TWeakPtr? We need to ensure that FMagicLeapMediaCodecPlayer::TextureData is not destroyed before
					// we unregister the external texture and releae the VideoTexture.
					TSharedPtr<FMagicLeapVideoTextureDataGL, ESPMode::ThreadSafe> TextureData;
					FGuid PlayerGuid;
					MLHandle VideoCodecHandle;
				};

				FReleaseVideoResourcesParams Params = { this, StaticCastSharedPtr<FMagicLeapVideoTextureDataGL>(TextureData), PlayerGuid, VideoCodecHandle };

				ENQUEUE_RENDER_COMMAND(MagicLeapMediaPlayerDestroy)(
					[Params](FRHICommandListImmediate& RHICmdList)
					{
						FExternalTextureRegistry::Get().UnregisterExternalTexture(Params.PlayerGuid);
						Params.TextureData->bIsVideoTextureValid = false;

						// @todo: this causes a crash
						//Params.TextureData->VideoTexture->Release();
						Params.TextureData->SaveContext();
						Params.TextureData->MakeCurrent();

						if (Params.TextureData->Image != EGL_NO_IMAGE_KHR)
						{
							eglDestroyImageKHR(eglGetCurrentDisplay(), Params.TextureData->Image);
							Params.TextureData->Image = EGL_NO_IMAGE_KHR;
						}

						Params.TextureData->RestoreContext();
						if (Params.TextureData->PreviousNativeBuffer != 0 && MLHandleIsValid(Params.TextureData->PreviousNativeBuffer))
						{
							Params.MediaPlayer->ReleaseNativeBuffer_RenderThread(Params.VideoCodecHandle, Params.TextureData->PreviousNativeBuffer);
							Params.TextureData->PreviousNativeBuffer = 0;
						}
					});
			}

			FlushRenderingCommands();
		}
	}

	StopAndReset(EMediaTrackType::Audio, AudioCodecHandle);
	StopAndReset(EMediaTrackType::Video, VideoCodecHandle);

	MLResult Result = MLMediaExtractorDestroy(MediaExtractorHandle);
	UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaExtractorDestroy failed with error %s"), UTF8_TO_TCHAR(MLGetResultString(Result)));
	MediaExtractorHandle = ML_INVALID_HANDLE;

	if (MLHandleIsValid(MediaDataSourceHandle))
	{
		Result = MLMediaDataSourceDestroy(MediaDataSourceHandle);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaDataSourceDestroy failed with error %s"), UTF8_TO_TCHAR(MLGetResultString(Result)));
		MediaDataSourceHandle = ML_INVALID_HANDLE;

		DataSourceArchive.Reset();
	}

	switch (MediaSourceType)
	{
	case(EMediaSourceType::AudioOnly):
		Result = MLMediaCodecDestroy(AudioCodecHandle);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecDestroy failed with error %s"), UTF8_TO_TCHAR(MLGetResultString(Result)));
		break;
	case(EMediaSourceType::VideoOnly):
		Result = MLMediaCodecDestroy(VideoCodecHandle);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecDestroy failed with error %s"), UTF8_TO_TCHAR(MLGetResultString(Result)));
		break;
	case(EMediaSourceType::VideoAndAudio):
		Result = MLMediaCodecDestroy(AudioCodecHandle);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecDestroy failed with error %s"), UTF8_TO_TCHAR(MLGetResultString(Result)));
		Result = MLMediaCodecDestroy(VideoCodecHandle);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecDestroy failed with error %s"), UTF8_TO_TCHAR(MLGetResultString(Result)));
		break;
	}

	// TODO: Destroy format handles in FTrackData

	AudioCodecHandle = ML_INVALID_HANDLE;
	VideoCodecHandle = ML_INVALID_HANDLE;

	bMediaPrepared = false;
	Info.Empty();
	MediaUrl = FString();
	AudioSamplePool->Reset();
	TrackInfo.Empty();
	SelectedTrack.Empty();

	// notify listeners
	EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
	EventSink.ReceiveMediaEvent(EMediaEvent::MediaClosed);
}

IMediaCache& FMagicLeapMediaCodecPlayer::GetCache()
{
	return *this;
}

IMediaControls& FMagicLeapMediaCodecPlayer::GetControls()
{
	return *this;
}

FString FMagicLeapMediaCodecPlayer::GetInfo() const
{
	return Info;
}

FName FMagicLeapMediaCodecPlayer::GetPlayerName() const
{
	static FName PlayerName(TEXT("MagicLeapMediaCodec"));
	return PlayerName;
}

IMediaSamples& FMagicLeapMediaCodecPlayer::GetSamples()
{
	return *Samples.Get();
}

FString FMagicLeapMediaCodecPlayer::GetStats() const
{
	return TEXT("MagicLeapMediaCodec stats information not implemented yet");
}

IMediaTracks& FMagicLeapMediaCodecPlayer::GetTracks()
{
	return *this;
}

FString FMagicLeapMediaCodecPlayer::GetUrl() const
{
	return MediaUrl;
}

IMediaView& FMagicLeapMediaCodecPlayer::GetView()
{
	return *this;
}

bool FMagicLeapMediaCodecPlayer::Open(const FString& Url, const IMediaOptions* Options)
{
	Close();

	if (!MLHandleIsValid(MediaExtractorHandle))
	{
		MLResult Result = MLMediaExtractorCreate(&MediaExtractorHandle);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaExtractorCreate failed with error %s."), UTF8_TO_TCHAR(MLGetResultString(Result)));
		CurrentState = (Samples.IsValid() && Result == MLResult_Ok) ? EMediaState::Closed : EMediaState::Error;
	}
	if (CurrentState == EMediaState::Error)
	{
		EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
		return false;
	}

	if ((Url.IsEmpty()))
	{
		EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
		return false;
	}

	MediaUrl = Url;

	const FString localFileSchema = "file://";

	// open the media
	if (Url.StartsWith(localFileSchema))
	{
		FString FilePath = Url.RightChop(localFileSchema.Len());
		FPaths::NormalizeFilename(FilePath);

		IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
		// This module is only for Lumin so this is fine for now.
		FLuminPlatformFile* LuminPlatformFile = static_cast<FLuminPlatformFile*>(&PlatformFile);
		// make sure the file exists
		if (!LuminPlatformFile->FileExists(*FilePath, FilePath))
		{
			UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("File doesn't exist %s."), *FilePath);
			EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
			return false;
		}

		const bool Precache = (Options != nullptr) ? Options->GetMediaOption("PrecacheFile", false) : false;

		if (Precache)
		{
			UE_LOG(LogMagicLeapMediaCodec, Display, TEXT("Precaching media file %s"), *FilePath);

			FArrayReader* Reader = new FArrayReader();

			if (FFileHelper::LoadFileToArray(*Reader, *FilePath))
			{
				DataSourceArchive = MakeShareable(Reader);
			}
			else
			{
				delete Reader;
			}

			if (!DataSourceArchive.IsValid())
			{
				UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("Failed to open or read media file %s"), *FilePath);
				EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
				return false;
			}

			if (DataSourceArchive->TotalSize() == 0)
			{
				UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("Cannot open media from empty file %s."), *FilePath);
				EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
				return false;
			}

			MLResult Result = MLMediaDataSourceCreate(MediaDataSourceReadAt_Callback, MediaDataSourceGetSize_Callback, MediaDataSourceClose_Callback, reinterpret_cast<void*>(this), &MediaDataSourceHandle);
			if (Result != MLResult_Ok)
			{
				UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaDataSourceCreate for path %s failed with error %s."), *FilePath, UTF8_TO_TCHAR(MLGetResultString(Result)));
				EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
				return false;
			}

			Result = MLMediaExtractorSetMediaDataSource(MediaExtractorHandle, MediaDataSourceHandle);
			if (Result != MLResult_Ok)
			{
				UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaExtractorSetMediaDataSource for path %s failed with error %s."), *FilePath, UTF8_TO_TCHAR(MLGetResultString(Result)));
				EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
				return false;
			}
		}
		else
		{
			MLResult Result = MLMediaExtractorSetDataSourceForPath(MediaExtractorHandle, TCHAR_TO_UTF8(*FilePath));
			if (Result != MLResult_Ok)
			{
				UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaExtractorSetDataSourceForPath for path %s failed with error %s."), *FilePath, UTF8_TO_TCHAR(MLGetResultString(Result)));
				EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
				return false;
			}
		}
	}
	else
	{
		// open remote media
		MLResult Result = MLMediaExtractorSetDataSourceForURI(MediaExtractorHandle, TCHAR_TO_UTF8(*Url));
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaExtractorSetDataSourceForURI for remote media source %s failed with error %s."), *Url, UTF8_TO_TCHAR(MLGetResultString(Result)));
			EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
			return false;
		}
	}

	EventSink.ReceiveMediaEvent(EMediaEvent::MediaConnecting);

	// prepare media
	MediaUrl = Url;

	if (!bMediaPrepared)
	{
		// TODO: For remote source, track information will probably not be available immediately.
		GetTrackInformation();
		bMediaPrepared = true;
		bool bVideoCodecStatus = CreateMediaCodec(EMediaTrackType::Video, VideoCodecHandle);
		bool bAudioCodecStatus = CreateMediaCodec(EMediaTrackType::Audio, AudioCodecHandle);
		if (bVideoCodecStatus && !(bAudioCodecStatus))
		{
			MediaSourceType = EMediaSourceType::VideoOnly;
		}
		else if (bAudioCodecStatus && !(bVideoCodecStatus))
		{
			MediaSourceType = EMediaSourceType::AudioOnly;
		}
		else if (bVideoCodecStatus && bAudioCodecStatus)
		{
			MediaSourceType = EMediaSourceType::VideoAndAudio;
		}
		else
		{
			EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
			return false;
		}
	}

	switch (MediaSourceType)
	{
	case(EMediaSourceType::VideoOnly):
		StartMediaCodec(VideoCodecHandle);
		break;
	case(EMediaSourceType::AudioOnly):
		StartMediaCodec(AudioCodecHandle);
		break;
	case(EMediaSourceType::VideoAndAudio):
		StartMediaCodec(VideoCodecHandle);
		StartMediaCodec(AudioCodecHandle);
		break;
	};

	InputWorker.InitThread(*this, MediaExtractorHandle, CriticalSection, GT_IT_Mutex, RT_IT_Mutex);

	CurrentState = EMediaState::Stopped;
	// notify listeners
	EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
	EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpened);

	return true;
}

bool FMagicLeapMediaCodecPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* Options)
{
	// TODO: MagicLeapMedia: implement opening media from FArchive
	return false;
}

bool FMagicLeapMediaCodecPlayer::CanControl(EMediaControl Control) const
{
	if (Control == EMediaControl::Pause)
	{
		return (CurrentState == EMediaState::Playing);
	}

	if (Control == EMediaControl::Resume)
	{
		return (CurrentState == EMediaState::Paused);
	}

	if (Control == EMediaControl::Seek)
	{
		return (CurrentState == EMediaState::Playing) || (CurrentState == EMediaState::Paused);
	}

	return false;
}

FTimespan FMagicLeapMediaCodecPlayer::GetDuration() const
{
	FTimespan Duration = FTimespan::Zero();

	if (CurrentState == EMediaState::Playing || CurrentState == EMediaState::Paused || CurrentState == EMediaState::Stopped)
	{
		if (TrackInfo[EMediaTrackType::Video].Num() > 0)
		{
			return TrackInfo[EMediaTrackType::Video][SelectedTrack[EMediaTrackType::Video]].Duration;
		}
	}

	return Duration;
}

float FMagicLeapMediaCodecPlayer::GetRate() const
{
	return (CurrentState == EMediaState::Playing) ? 1.0f : 0.0f;
}

EMediaState FMagicLeapMediaCodecPlayer::GetState() const
{
	return CurrentState;
}

EMediaStatus FMagicLeapMediaCodecPlayer::GetStatus() const
{
	return EMediaStatus::None;
}

TRangeSet<float> FMagicLeapMediaCodecPlayer::GetSupportedRates(EMediaRateThinning Thinning) const
{
	TRangeSet<float> Result;

	Result.Add(TRange<float>(0.0f));
	Result.Add(TRange<float>(1.0f));

	return Result;
}

FTimespan FMagicLeapMediaCodecPlayer::GetTime() const
{
	if (CurrentState == EMediaState::Playing || CurrentState == EMediaState::Paused)
	{
		return CurrentPlaybackTime.Load();
	}

	return FTimespan::Zero();
}

bool FMagicLeapMediaCodecPlayer::IsLooping() const
{
	return bLoopPlayback;
}

bool FMagicLeapMediaCodecPlayer::Seek(const FTimespan& Time)
{
	if ((CurrentState == EMediaState::Closed) || (CurrentState == EMediaState::Error) || (CurrentState == EMediaState::Preparing))
	{
		UE_LOG(LogMagicLeapMediaCodec, Warning, TEXT("Cannot seek while closed, preparing, or in error state"));
		return false;
	}
	else if (CurrentState == EMediaState::Playing || CurrentState == EMediaState::Paused)
	{
		InputWorker.Seek(Time);
	}
	return true;
}

bool FMagicLeapMediaCodecPlayer::SetLooping(bool Looping)
{
	bLoopPlayback = Looping;
	return bLoopPlayback;
}

bool FMagicLeapMediaCodecPlayer::SetRate(float Rate)
{
	if ((CurrentState == EMediaState::Closed) || (CurrentState == EMediaState::Error) || (CurrentState == EMediaState::Preparing))
	{
		UE_LOG(LogMagicLeapMediaCodec, Warning, TEXT("Cannot set rate while closed, preparing, or in error state"));
		return false;
	}

	if (Rate == GetRate())
	{
		// rate already set
		return true;
	}
	{
		// Scope lock for CurrentState and StartPresentationTime. These are read and written to in the render thread for the video codec.
		FScopeLock Lock(&CriticalSection);
		if (Rate == 0.0f)
		{
			CurrentState = EMediaState::Paused;
			bResetVideoCodecStartTime = true;
			EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
			return true;
		}
		else if (Rate == 1.0f)
		{
			if (CurrentState != EMediaState::Playing)
			{
				CurrentState = EMediaState::Playing;
				EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackResumed);
				InputWorker.WakeUp();
			}
			return true;
		}
		else
		{
			UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("Rate %f not supported by MagicLeapMedia."), Rate);
			return false;
		}
	}

	return false;
}

bool FMagicLeapMediaCodecPlayer::SetNativeVolume(float Volume)
{
	UE_LOG(LogMagicLeapMediaCodec, Warning, TEXT("SetNativeVolume() is not supported for MagicLeapMedia. Use UMediaSoundComponent::SetVolumeMultiplier() instead."));
	return false;
}

void FMagicLeapMediaCodecPlayer::SetGuid(const FGuid& Guid)
{
	PlayerGuid = Guid;
}

void FMagicLeapMediaCodecPlayer::TickFetch(FTimespan DeltaTime, FTimespan Timecode)
{
	// TODO: this condition needs to be revised based on how we handle EMediaState::Preparing as preapring would probably require processing some output samples.
	// We should not process output samples whem playback is paused. Not even to flush the already decoded samples as we need a frame accurate sync.
	if (CurrentState != EMediaState::Playing)
	{
		return;
	}

	if (GSupportsImageExternal)
	{
		if (FLuminPlatformMisc::ShouldUseVulkan())
		{
			struct FWriteVideoSampleParams
			{
				FMagicLeapMediaCodecPlayer* MediaPlayer;
				TWeakPtr<FMagicLeapVideoTextureDataVK, ESPMode::ThreadSafe> TextureData;
				FGuid PlayerGuid;
				MLHandle VideoCodecHandle;
				FTimespan FrameTimecode;
			};

			FWriteVideoSampleParams Params = { this, StaticCastSharedPtr<FMagicLeapVideoTextureDataVK>(TextureData), PlayerGuid, VideoCodecHandle, Timecode };

			ENQUEUE_RENDER_COMMAND(MagicLeapMediaPlayerWriteVideoSample)(
				[Params](FRHICommandListImmediate& RHICmdList) mutable
				{
					auto TextureDataPtr = Params.TextureData.Pin();

					Params.MediaPlayer->bIsBufferAvailable = Params.MediaPlayer->ProcessVideoOutputSample_RenderThread(Params.VideoCodecHandle, Params.MediaPlayer->TrackInfo[EMediaTrackType::Video][Params.MediaPlayer->SelectedTrack[EMediaTrackType::Video]], Params.FrameTimecode);
					if (!Params.MediaPlayer->IsBufferAvailable_RenderThread(Params.VideoCodecHandle))
					{
						// UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("No video buffer available at TickFetch!"));
						return;
					}

					if (TextureDataPtr->PreviousNativeBuffer != 0 && MLHandleIsValid(TextureDataPtr->PreviousNativeBuffer))
					{
						Params.MediaPlayer->ReleaseNativeBuffer_RenderThread(Params.VideoCodecHandle, TextureDataPtr->PreviousNativeBuffer);
						TextureDataPtr->PreviousNativeBuffer = 0;
					}

					MLHandle NativeBuffer = ML_INVALID_HANDLE;
					if (!Params.MediaPlayer->GetNativeBuffer_RenderThread(Params.VideoCodecHandle, NativeBuffer))
					{
						return;
					}

					check(MLHandleIsValid(NativeBuffer));

					if (Params.MediaPlayer->bPlaybackCompleted_RenderThread)
					{
						TextureDataPtr->VideoTexturePool.Empty();
					}

					if (!TextureDataPtr->VideoTexturePool.Contains((uint64)NativeBuffer))
					{
						FTextureRHIRef NewMediaTexture;
						if (!FMagicLeapHelperVulkan::GetMediaTexture(NewMediaTexture, TextureDataPtr->VideoSampler, NativeBuffer))
						{
							UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("Failed to get next media texture."));
							return;
						}

						TextureDataPtr->VideoTexturePool.Add((uint64)NativeBuffer, NewMediaTexture);
					
						if (TextureDataPtr->VideoTexture == nullptr)
						{
							FRHIResourceCreateInfo CreateInfo;
							TextureDataPtr->VideoTexture = RHICmdList.CreateTextureExternal2D(1, 1, PF_R8G8B8A8, 1, 1, 0, CreateInfo);
						}

						FMagicLeapHelperVulkan::AliasMediaTexture(TextureDataPtr->VideoTexture, NewMediaTexture);
					}
					else
					{
						FTextureRHIRef* const PooledMediaTexture = TextureDataPtr->VideoTexturePool.Find((uint64)NativeBuffer);
						check(PooledMediaTexture != nullptr);
						FMagicLeapHelperVulkan::AliasMediaTexture(TextureDataPtr->VideoTexture, *PooledMediaTexture);
					}

					TextureDataPtr->bIsVideoTextureValid = (TextureDataPtr->bIsVideoTextureValid && !Params.MediaPlayer->UpdateTransformMatrix_RenderThread(Params.VideoCodecHandle));

					if (!TextureDataPtr->bIsVideoTextureValid)
					{
						Params.MediaPlayer->RegisterExternalTexture_RenderThread(Params.PlayerGuid, TextureDataPtr->VideoTexture, TextureDataPtr->VideoSampler);
						TextureDataPtr->bIsVideoTextureValid = true;
					}

					TextureDataPtr->PreviousNativeBuffer = NativeBuffer;
				});
		}
		else
		{
			struct FWriteVideoSampleParams
			{
				FMagicLeapMediaCodecPlayer* MediaPlayer;
				TWeakPtr<FMagicLeapVideoTextureDataGL, ESPMode::ThreadSafe> TextureData;
				FGuid PlayerGuid;
				MLHandle VideoCodecHandle;
				FTimespan FrameTimecode;
			};

			FWriteVideoSampleParams Params = { this, StaticCastSharedPtr<FMagicLeapVideoTextureDataGL>(TextureData), PlayerGuid, VideoCodecHandle, Timecode };

			ENQUEUE_RENDER_COMMAND(MagicLeapMediaPlayerWriteVideoSample)(
				[Params](FRHICommandListImmediate& RHICmdList) mutable
				{
					auto TextureDataPtr = Params.TextureData.Pin();

					Params.MediaPlayer->bIsBufferAvailable = Params.MediaPlayer->ProcessVideoOutputSample_RenderThread(Params.VideoCodecHandle, Params.MediaPlayer->TrackInfo[EMediaTrackType::Video][Params.MediaPlayer->SelectedTrack[EMediaTrackType::Video]], Params.FrameTimecode);
					if (!Params.MediaPlayer->IsBufferAvailable_RenderThread(Params.VideoCodecHandle))
					{
						return;
					}

					FTextureRHIRef MediaVideoTexture = TextureDataPtr->VideoTexture;
					if (MediaVideoTexture == nullptr)
					{
						FRHIResourceCreateInfo CreateInfo;
						MediaVideoTexture = RHICmdList.CreateTextureExternal2D(1, 1, PF_R8G8B8A8, 1, 1, 0, CreateInfo);
						TextureDataPtr->VideoTexture = MediaVideoTexture;

						if (MediaVideoTexture == nullptr)
						{
							UE_LOG(LogMagicLeapMediaCodec, Warning, TEXT("CreateTextureExternal2D failed!"));
							return;
						}

						TextureDataPtr->bIsVideoTextureValid = false;
					}

					// MLHandle because Unreal's uint64 is 'unsigned long long *' whereas uint64_t for the C-API is 'unsigned long *'
					// TODO: Fix the Unreal types for the above comment.
					MLHandle nativeBuffer = ML_INVALID_HANDLE;
					if (!Params.MediaPlayer->GetNativeBuffer_RenderThread(Params.VideoCodecHandle, nativeBuffer))
					{
						return;
					}

					int32 CurrentFramePosition = 0;
					if (!Params.MediaPlayer->GetCurrentPosition_RenderThread(Params.VideoCodecHandle, CurrentFramePosition))
					{
						return;
					}

					// Clear gl errors as they can creep in from the UE4 renderer.
					glGetError();

					if (!TextureDataPtr->bContextCreated)
					{
						TextureDataPtr->InitContext();
						TextureDataPtr->bContextCreated = true;
					}
					TextureDataPtr->SaveContext();
					TextureDataPtr->MakeCurrent();

					int32 TextureID = *reinterpret_cast<int32*>(MediaVideoTexture->GetNativeResource());
					if (TextureDataPtr->Image != EGL_NO_IMAGE_KHR)
					{
						eglDestroyImageKHR(eglGetCurrentDisplay(), TextureDataPtr->Image);
						TextureDataPtr->Image = EGL_NO_IMAGE_KHR;
					}
					if (TextureDataPtr->PreviousNativeBuffer != 0 && MLHandleIsValid(TextureDataPtr->PreviousNativeBuffer))
					{
						Params.MediaPlayer->ReleaseNativeBuffer_RenderThread(Params.VideoCodecHandle, TextureDataPtr->PreviousNativeBuffer);
					}
					TextureDataPtr->PreviousNativeBuffer = nativeBuffer;

					// Wrap latest decoded frame into a new gl texture oject
					TextureDataPtr->Image = eglCreateImageKHR(TextureDataPtr->Display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, (EGLClientBuffer)(void *)nativeBuffer, NULL);
					if (TextureDataPtr->Image == EGL_NO_IMAGE_KHR)
					{
						EGLint errorcode = eglGetError();
						UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("Failed to create EGLImage from the buffer. %d"), errorcode);
						TextureDataPtr->RestoreContext();
						return;
					}
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_EXTERNAL_OES, TextureID);
					glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, TextureDataPtr->Image);
					glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
					glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

					TextureDataPtr->RestoreContext();

					TextureDataPtr->bIsVideoTextureValid = Params.MediaPlayer->UpdateTransformMatrix_RenderThread(Params.VideoCodecHandle);

					if (!TextureDataPtr->bIsVideoTextureValid)
					{
						FSamplerStateInitializerRHI SamplerStateInitializer(SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp);
						FSamplerStateRHIRef SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
						Params.MediaPlayer->RegisterExternalTexture_RenderThread(Params.PlayerGuid, MediaVideoTexture, SamplerStateRHI);
						TextureDataPtr->bIsVideoTextureValid = true;
					}
				});
		}
	}
}

bool FMagicLeapMediaCodecPlayer::UpdateTransformMatrix_RenderThread(MLHandle InVideoCodecHandle)
{
	MLResult Result = MLMediaCodecGetFrameTransformationMatrix(InVideoCodecHandle, FrameTransformationMatrix);
	if (UScale != FrameTransformationMatrix[0] ||
		UOffset != FrameTransformationMatrix[12] ||
		VScale != -FrameTransformationMatrix[5] ||
		VOffset != (1.0f - FrameTransformationMatrix[13]))
	{
		UScale = FrameTransformationMatrix[0];
		UOffset = FrameTransformationMatrix[12];
		VScale = -FrameTransformationMatrix[5];
		VOffset = 1.0f - FrameTransformationMatrix[13];
		return true;
	}

	return false;
}

void FMagicLeapMediaCodecPlayer::RegisterExternalTexture_RenderThread(const FGuid& InGuid, FTextureRHIRef& InTextureRHI, FSamplerStateRHIRef& InSamplerStateRHI)
{
	FExternalTextureRegistry::Get().RegisterExternalTexture(InGuid, InTextureRHI, InSamplerStateRHI, FLinearColor(UScale, 0.0f, 0.0f, VScale), FLinearColor(UOffset, VOffset, 0.0f, 0.0f));
}

void FMagicLeapMediaCodecPlayer::TickInput(FTimespan DeltaTime, FTimespan Timecode)
{
	if (!bMediaPrepared)
	{
		return;
	}

	// Fire all pending media events. Likely queued from MediaCodecInputWorker thread.
	EMediaEvent PendingMediaEvent;
	while (MediaEventQueue.Dequeue(PendingMediaEvent))
	{
		EventSink.ReceiveMediaEvent(PendingMediaEvent);
	}

	{
		FScopeLock Lock(&CriticalSection);
		// TODO: bPlaybackCompleted is updated on the render thread as well as the input worker thread.
		// So consider firing this event based on an event queue instead of directly testing for the condition here.
		if (bPlaybackCompleted)
		{
			if (!IsLooping())
			{
				CurrentState = EMediaState::Stopped;
			}
			EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackEndReached);
		}
	}

	if (CurrentState != EMediaState::Playing)
	{
		// remove delegates if registered
		if (ResumeHandle.IsValid())
		{
			FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(ResumeHandle);
			ResumeHandle.Reset();
		}

		if (PauseHandle.IsValid())
		{
			FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(PauseHandle);
			PauseHandle.Reset();
		}
	}

	// register delegate if not registered
	if (!ResumeHandle.IsValid())
	{
		ResumeHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FMagicLeapMediaCodecPlayer::HandleApplicationHasEnteredForeground);
	}
	if (!PauseHandle.IsValid())
	{
		PauseHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FMagicLeapMediaCodecPlayer::HandleApplicationWillEnterBackground);
	}
}

void FMagicLeapMediaCodecPlayer::SetLastAudioRenderedSampleTime(FTimespan SampleTime)
{
	LastAudioRenderedSampleTime = SampleTime;
}

void FMagicLeapMediaCodecPlayer::TickAudio()
{
	if (GetSelectedTrack(EMediaTrackType::Audio) != INDEX_NONE)
	{
		ProcessAudioOutputSample(AudioCodecHandle, TrackInfo[EMediaTrackType::Audio][SelectedTrack[EMediaTrackType::Audio]], LastAudioRenderedSampleTime);
	}
}

bool FMagicLeapMediaCodecPlayer::GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const
{
	if ((FormatIndex != 0) || TrackIndex >= TrackInfo[EMediaTrackType::Audio].Num() || TrackIndex < 0)
	{
		return false;
	}

	OutFormat.BitsPerSample = 16;
	OutFormat.NumChannels = TrackInfo[EMediaTrackType::Audio][TrackIndex].ChannelCount;
	OutFormat.SampleRate = TrackInfo[EMediaTrackType::Audio][TrackIndex].SampleRate;
	OutFormat.TypeName = TrackInfo[EMediaTrackType::Audio][TrackIndex].FormatName;

	return true;
}

int32 FMagicLeapMediaCodecPlayer::GetNumTracks(EMediaTrackType TrackType) const
{
	if (TrackInfo.Contains(TrackType))
	{
		return TrackInfo[TrackType].Num();
	}

	return 0;
}

int32 FMagicLeapMediaCodecPlayer::GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return ((TrackIndex >= 0) && (TrackIndex < GetNumTracks(TrackType))) ? 1 : 0;
}

int32 FMagicLeapMediaCodecPlayer::GetSelectedTrack(EMediaTrackType TrackType) const
{
	if (SelectedTrack.Contains(TrackType))
	{
		return SelectedTrack[TrackType];
	}

	return INDEX_NONE;
}

FText FMagicLeapMediaCodecPlayer::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return FText::GetEmpty();
}

int32 FMagicLeapMediaCodecPlayer::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return (GetSelectedTrack(TrackType) != INDEX_NONE) ? 0 : INDEX_NONE;
}

FString FMagicLeapMediaCodecPlayer::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if (TrackIndex >= 0 && TrackIndex < TrackInfo[TrackType].Num())
	{
		return TrackInfo[TrackType][TrackIndex].Language;
	}
	return FString();
}

FString FMagicLeapMediaCodecPlayer::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	// Track names not supported in ML.
	return FString();
}

bool FMagicLeapMediaCodecPlayer::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const
{
	if ((FormatIndex != 0) || TrackIndex >= TrackInfo[EMediaTrackType::Video].Num())
	{
		return false;
	}

	OutFormat.Dim = FIntPoint(TrackInfo[EMediaTrackType::Video][TrackIndex].Width, TrackInfo[EMediaTrackType::Video][TrackIndex].Height);
	OutFormat.FrameRate = static_cast<float>(TrackInfo[EMediaTrackType::Video][TrackIndex].FrameRate);
	OutFormat.FrameRates = TRange<float>(OutFormat.FrameRate);
	OutFormat.TypeName = TrackInfo[EMediaTrackType::Video][TrackIndex].FormatName;

	return true;
}

bool FMagicLeapMediaCodecPlayer::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	if (TrackInfo.Contains(TrackType) && CurrentState != EMediaState::Preparing)
	{
		if (TrackInfo[TrackType].IsValidIndex(TrackIndex))
		{
			// TODO: codec needs to be changed.
			MLResult Result = MLMediaExtractorSelectTrack(MediaExtractorHandle, static_cast<SIZE_T>(TrackInfo[TrackType][TrackIndex].TrackIndex));
			if (Result == MLResult_Ok)
			{
				SelectedTrack[TrackType] = TrackIndex;
				return true;
			}
			else
			{
				UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaExtractorSelectTrack failed with error %s"), UTF8_TO_TCHAR(MLGetResultString(Result)));
			}
		}
	}
	return false;
}

bool FMagicLeapMediaCodecPlayer::SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex)
{
	return false;
}

void FMagicLeapMediaCodecPlayer::QueueMediaEvent(EMediaEvent MediaEvent)
{
	MediaEventQueue.Enqueue(MediaEvent);
}

void FMagicLeapMediaCodecPlayer::FlushCodecs()
{
	MLResult Result;
	if (MLHandleIsValid(AudioCodecHandle))
	{
		Result = MLMediaCodecFlush(AudioCodecHandle);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecFlush() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
	}
	if (MLHandleIsValid(VideoCodecHandle))
	{
		Result = MLMediaCodecFlush(VideoCodecHandle);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecFlush() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		// UE_LOG(LogMagicLeapMediaCodec, Warning, TEXT("video codec flushed."));
	}

	for (const auto& TrackPair : SelectedTrack)
	{
		if (TrackPair.Value != INDEX_NONE)
		{
			TrackInfo[TrackPair.Key][TrackPair.Value].bBufferPendingRender = false;
			if (TrackPair.Key == EMediaTrackType::Audio)
			{
				TrackInfo[TrackPair.Key][TrackPair.Value].LastPresentationTime = FTimespan::Zero();
			}
		}
	}

	Samples->FlushSamples();
}

bool FMagicLeapMediaCodecPlayer::GetCodecForTrackIndex(int64_t TrackIndex, MLHandle& SampleCodecHandle) const
{
	if (TrackIndex == TrackInfo[EMediaTrackType::Video][SelectedTrack[EMediaTrackType::Video]].TrackIndex)
	{
		SampleCodecHandle = VideoCodecHandle;
	}
	else if (TrackIndex == TrackInfo[EMediaTrackType::Audio][SelectedTrack[EMediaTrackType::Audio]].TrackIndex)
	{
		SampleCodecHandle = AudioCodecHandle;
	}
	else
	{
		return false;
	}

	return true;
}

bool FMagicLeapMediaCodecPlayer::IsPlaybackCompleted() const
{
	return bPlaybackCompleted;
}

void FMagicLeapMediaCodecPlayer::SetPlaybackCompleted(bool PlaybackCompleted)
{
	bPlaybackCompleted = PlaybackCompleted;
}

void FMagicLeapMediaCodecPlayer::QueueVideoCodecStartTimeReset()
{
	bResetVideoCodecStartTime = true;
}

int64_t FMagicLeapMediaCodecPlayer::MediaDataSourceReadAt(MLHandle media_data_source, size_t position, size_t size, uint8_t *buffer)
{
	if (media_data_source == MediaDataSourceHandle && DataSourceArchive.IsValid() && buffer != nullptr)
	{
		int64_t BytesToRead = size;

		DataSourceArchive->Seek(position);
		int64 Size = DataSourceArchive->TotalSize();

		if (BytesToRead > Size)
		{
			BytesToRead = Size;
		}

		if ((Size - BytesToRead) < DataSourceArchive->Tell())
		{
			BytesToRead = Size - position;
		}

		if (BytesToRead > 0)
		{
			DataSourceArchive->Serialize(buffer, BytesToRead);
		}

		return BytesToRead;
	}

	return -1;
}

int64_t FMagicLeapMediaCodecPlayer::MediaDataSourceGetSize(MLHandle media_data_source)
{
	if (media_data_source == MediaDataSourceHandle && DataSourceArchive.IsValid())
	{
		return DataSourceArchive->TotalSize();
	}

	return -1;
}

void FMagicLeapMediaCodecPlayer::MediaDataSourceClose(MLHandle media_data_source)
{

}

void FMagicLeapMediaCodecPlayer::HandleApplicationHasEnteredForeground()
{
	// check state in case changed before ticked
	if (CurrentState == EMediaState::Paused && bWasMediaPlayingBeforeAppPause)
	{
		// pause
		SetRate(1.0f);
	}
}

void FMagicLeapMediaCodecPlayer::HandleApplicationWillEnterBackground()
{
	bWasMediaPlayingBeforeAppPause = (CurrentState == EMediaState::Playing);
	// check state in case changed before ticked
	if (bWasMediaPlayingBeforeAppPause)
	{
		// pause
		SetRate(0.0f);
	}
}

FIntPoint FMagicLeapMediaCodecPlayer::GetVideoDimensions() const
{
	int32 width = 0;
	int32 height = 0;
	FMediaVideoTrackFormat VideoTrackFormat;
	if (GetVideoTrackFormat(SelectedTrack[EMediaTrackType::Video], 0, VideoTrackFormat))
	{
		return VideoTrackFormat.Dim;
	}
	return FIntPoint(width, height);
}

bool FMagicLeapMediaCodecPlayer::StopAndReset(EMediaTrackType TrackType, MLHandle& CodecHandle)
{
	if (CodecHandle == ML_INVALID_HANDLE)
	{
		return true;
	}
	MLResult FlushResult = MLMediaCodecFlush(CodecHandle);
	MLResult CodecStopResult = MLMediaCodecStop(CodecHandle);
	if ((FlushResult != MLResult_Ok) || (CodecStopResult != MLResult_Ok))
	{
		UE_CLOG(FlushResult != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecFlush failed with error %d"), FlushResult);
		UE_CLOG(CodecStopResult != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecStop failed with error %d"), CodecStopResult);
		return false;
	}
	return true;
}

bool FMagicLeapMediaCodecPlayer::SetRateOne()
{
	StartMediaCodec(VideoCodecHandle);
	StartMediaCodec(AudioCodecHandle);

	CurrentState = EMediaState::Playing;
	EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackResumed);
	return true;
}

void FMagicLeapMediaCodecPlayer::GetTrackInformation()
{
	uint64 NumTracks = 0;
	MLResult Result = MLMediaExtractorGetTrackCount(MediaExtractorHandle, reinterpret_cast<uint64_t*>(&NumTracks));
	UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaExtractorGetTrackCount() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));

	char Mime[MAX_KEY_STRING_SIZE] = "";
	FString MimeTypeString;

	TrackInfo.Add(EMediaTrackType::Video);
	TrackInfo.Add(EMediaTrackType::Audio);
	TrackInfo.Add(EMediaTrackType::Caption);
	TrackInfo.Add(EMediaTrackType::Subtitle);
	TrackInfo.Add(EMediaTrackType::Metadata);

	SelectedTrack.Add(EMediaTrackType::Video, INDEX_NONE);
	SelectedTrack.Add(EMediaTrackType::Audio, INDEX_NONE);
	SelectedTrack.Add(EMediaTrackType::Caption, INDEX_NONE);
	SelectedTrack.Add(EMediaTrackType::Subtitle, INDEX_NONE);
	SelectedTrack.Add(EMediaTrackType::Metadata, INDEX_NONE);

	for (SIZE_T TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
	{
		MLHandle TrackFormatHandle = ML_INVALID_HANDLE;
		Result = MLMediaExtractorGetTrackFormat(MediaExtractorHandle, TrackIndex, &TrackFormatHandle);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaExtractorGetTrackFormat() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
			continue;
		}

		// HACK: Fix this!
		// Keys should be defined in libs, ran into linking issue, SDK-1402
		Result = MLMediaFormatGetKeyString(TrackFormatHandle, MLMediaFormat_Key_Mime, Mime);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaFormatGetKeyString(mime) failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));

		MimeTypeString = FString(Mime);

		// TODO: refactor this. Lot of duplication!
		if (MimeTypeString.Contains("audio/"))
		{
			int32 Index = TrackInfo[EMediaTrackType::Audio].Add(FTrackData(MimeTypeString, TrackIndex));
			UpdateAudioTrackInfo(TrackFormatHandle, TrackInfo[EMediaTrackType::Audio][Index]);

			SelectedTrack[EMediaTrackType::Audio] = 0;
		}
		else if (MimeTypeString.Contains("video/"))
		{
			int32 Index = TrackInfo[EMediaTrackType::Video].Add(FTrackData(MimeTypeString, TrackIndex));
			UpdateVideoTrackInfo(TrackFormatHandle, TrackInfo[EMediaTrackType::Video][Index]);

			SelectedTrack[EMediaTrackType::Video] = 0;
		}
		// What are the mimetypes for captions/subtitle/metadata?
		// TODO: Process subtitles and metadata
		else if (MimeTypeString.Contains("text/"))
		{
		}

		Result = MLMediaFormatDestroy(TrackFormatHandle);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaFormatDestroy() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
	}
}

void FMagicLeapMediaCodecPlayer::UpdateCommonTrackInfo(MLHandle FormatHandle, FTrackData& CurrentTrackData)
{
	char FormatName[MAX_FORMAT_STRING_SIZE] = "";
	MLResult Result = MLMediaFormatObjectToString(FormatHandle, FormatName);
	UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaFormatObjectToString() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
	CurrentTrackData.FormatName = (Result == MLResult_Ok) ? FString(FormatName) : FString();

	int64 Duration = 0;
	Result = MLMediaFormatGetKeyValueInt64(FormatHandle, MLMediaFormat_Key_Duration, reinterpret_cast<int64_t*>(&Duration));
	UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaFormatGetKeyValueInt64(duration) failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
	CurrentTrackData.Duration = (Result == MLResult_Ok) ? FTimespan::FromMicroseconds(Duration) : CurrentTrackData.Duration;

	char Language[MAX_KEY_STRING_SIZE] = "";
	Result = MLMediaFormatGetKeyString(FormatHandle, MLMediaFormat_Key_Language, Language);
	UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaFormatGetKeyString(language) failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
	CurrentTrackData.Language = (Result == MLResult_Ok) ? FString(Language) : FString();
}

void FMagicLeapMediaCodecPlayer::UpdateAudioTrackInfo(MLHandle FormatHandle, FTrackData& CurrentTrackData)
{
	UpdateCommonTrackInfo(FormatHandle, CurrentTrackData);

	MLResult Result = MLMediaFormatGetKeyValueInt32(FormatHandle, MLMediaFormat_Key_Sample_Rate, &CurrentTrackData.SampleRate);
	UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaFormatGetKeyValueInt32(sample-rate) failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
	Result = MLMediaFormatGetKeyValueInt32(FormatHandle, MLMediaFormat_Key_Channel_Count, &CurrentTrackData.ChannelCount);
	UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaFormatGetKeyValueInt32(channel-count) failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
}

void FMagicLeapMediaCodecPlayer::UpdateVideoTrackInfo(MLHandle FormatHandle, FTrackData& CurrentTrackData)
{
	UpdateCommonTrackInfo(FormatHandle, CurrentTrackData);

	MLResult Result = MLMediaFormatGetKeyValueInt32(FormatHandle, MLMediaFormat_Key_Height, &CurrentTrackData.Height);
	UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaFormatGetKeyValueInt32(height) failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
	Result = MLMediaFormatGetKeyValueInt32(FormatHandle, MLMediaFormat_Key_Width, &CurrentTrackData.Width);
	UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaFormatGetKeyValueInt32(width) failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
	Result = MLMediaFormatGetKeyValueInt32(FormatHandle, MLMediaFormat_Key_Frame_Rate, &CurrentTrackData.FrameRate);
	UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaFormatGetKeyValueInt32(frame-rate) failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
}

bool FMagicLeapMediaCodecPlayer::CreateMediaCodec(EMediaTrackType TrackType, MLHandle& CodecHandle)
{
	if (SelectedTrack[TrackType] == INDEX_NONE)
	{
		return false;
	}

	FTrackData& CurrentTrack = TrackInfo[TrackType][SelectedTrack[TrackType]];
	MLHandle TrackFormatHandle = ML_INVALID_HANDLE;

	// When Configuring the codec, the correct track format must be set or you get absolute garbo
	MLResult Result = MLMediaExtractorGetTrackFormat(MediaExtractorHandle, CurrentTrack.TrackIndex, &TrackFormatHandle);
	UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaExtractorGetTrackFormat() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));

	Result = MLMediaExtractorSelectTrack(MediaExtractorHandle, CurrentTrack.TrackIndex);
	UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaExtractorSelectTrack() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));

	if (!MLHandleIsValid(CodecHandle))
	{
		Result = MLMediaCodecCreateCodec(MLMediaCodecCreation_ByType, MLMediaCodecType_Decoder, TCHAR_TO_UTF8(*CurrentTrack.MimeType), &CodecHandle);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecCreateCodec() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
			return false;
		}
	}

	if (TrackType == EMediaTrackType::Video)
	{
		Result = MLMediaCodecSetSurfaceHint(VideoCodecHandle, MLMediaCodecSurfaceHint::MLMediaCodecSurfaceHint_Hardware);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecSetSurfaceHint() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
	}

	// TODO: Handle crypto for DRM
	Result = MLMediaCodecConfigure(CodecHandle, TrackFormatHandle, static_cast<MLHandle>(0));
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecConfigure failed with error %s."), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		return false;
	}

	Result = MLMediaFormatDestroy(TrackFormatHandle);
	UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaFormatDestroy() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));

	return true;
}

void FMagicLeapMediaCodecPlayer::StartMediaCodec(MLHandle& CodecHandle)
{
	if (MLHandleIsValid(CodecHandle))
	{
		MLResult Result = MLMediaCodecStart(CodecHandle);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecStart() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));

		// Output streams are related to the codecs, so lets reset the flags here.
		bPlaybackCompleted = false;
		bReachedOutputEndOfStream = false;
		CurrentPlaybackTime = FTimespan::Zero();
	}
}

// TODO: Remove this, we know when a buffer is available
bool FMagicLeapMediaCodecPlayer::IsBufferAvailable_RenderThread(MLHandle InMediaPlayerHandle)
{
	ensureMsgf(IsInRenderingThread(), TEXT("IsBufferAvailable_RenderThread called outside of render thread"));
	return bIsBufferAvailable;
}

bool FMagicLeapMediaCodecPlayer::GetNativeBuffer_RenderThread(const MLHandle InVideoCodecHandle, MLHandle& NativeBuffer)
{
	ensureMsgf(IsInRenderingThread(), TEXT("GetNativeBuffer_RenderThread called outside of render thread"));
	MLResult Result = MLMediaCodecAcquireNextAvailableFrame(InVideoCodecHandle, &NativeBuffer);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecAcquireNextAvailableFrame failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		return false;
	}
	return true;
}

bool FMagicLeapMediaCodecPlayer::ReleaseNativeBuffer_RenderThread(const MLHandle InVideoCodecHandle, MLHandle NativeBuffer)
{
	ensureMsgf(IsInRenderingThread(), TEXT("ReleaseNativeBuffer_RenderThread called outside of render thread"));
	MLResult Result = MLMediaCodecReleaseFrame(InVideoCodecHandle, NativeBuffer);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecReleaseFrame failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		return false;
	}
	return true;
}

bool FMagicLeapMediaCodecPlayer::GetCurrentPosition_RenderThread(const MLHandle InVideoCodecHandle, int32& CurrentPosition)
{
	return true;
}

bool FMagicLeapMediaCodecPlayer::ProcessVideoOutputSample_RenderThread(MLHandle& CodecHandle, FTrackData& CurrentTrackData, const FTimespan& Timecode)
{
	// Doing this to support a thread safe FlushCodecs() function.
	FScopeTryLock LockIT(&RT_IT_Mutex);
	// No point in waiting here, blocking the entire render thread. This would be locked only during a flush operation which requires GameThread lock as well.
	// When the flush succeeds, we won't immediately get output buffers. We can afford to come back on the next frame.
	if (!LockIT.IsLocked())
	{
		return false;
	}

	if (!CurrentTrackData.bBufferPendingRender)
	{
		MLResult Result = MLMediaCodecDequeueOutputBuffer(CodecHandle, &CurrentTrackData.CurrentBufferInfo, 0, (int64_t*)&CurrentTrackData.CurrentBufferIndex);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecDequeueOutputBuffer failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
			return false;
		}
	}

	FTimespan CurrentPresentationTime = FTimespan::FromMicroseconds(CurrentTrackData.CurrentBufferInfo.presentation_time_us);
	if (0 <= CurrentTrackData.CurrentBufferIndex)
	{
		MLResult Result;

		{
			FScopeLock Lock(&CriticalSection);

			if (bResetVideoCodecStartTime)
			{
				CurrentTrackData.StartPresentationTime = FTimespan::Zero();
				bResetVideoCodecStartTime = false;
			}

			// Resets StartPresentationTime when video is looping.
			// TODO: this might not run ever
			if (bReachedOutputEndOfStream && !InputWorker.HasReachedInputEOS())
			{
				CurrentTrackData.StartPresentationTime = FTimespan::Zero();
				bReachedOutputEndOfStream = false;
			}
		}

		if (CurrentTrackData.StartPresentationTime.IsZero())
		{
			CurrentTrackData.StartPresentationTime = Timecode - CurrentPresentationTime;
			UE_LOG(LogMagicLeapMediaCodec, Warning, TEXT("Reset start presentation time to %f. (%f - %f)"), CurrentTrackData.StartPresentationTime.GetTotalMilliseconds(), Timecode.GetTotalMilliseconds(), CurrentPresentationTime.GetTotalMilliseconds());
		}

		switch (MediaSourceType)
		{
			case EMediaSourceType::VideoOnly:
			{
				if ((CurrentTrackData.StartPresentationTime + CurrentPresentationTime - Timecode) > FTimespan::Zero())
				{
					CurrentTrackData.bBufferPendingRender = true;
					return false;
				}
				break;
			}
			case EMediaSourceType::VideoAndAudio:
			{
				int32 SelectedAudioTrack = SelectedTrack[EMediaTrackType::Audio];
				if (SelectedAudioTrack != INDEX_NONE)
				{
					const FTrackData& CurrentAudioTrack = TrackInfo[EMediaTrackType::Audio][SelectedAudioTrack];
					const FTimespan LastAudioPresentationTime = FTimespan(FPlatformAtomics::AtomicRead(reinterpret_cast<const int64*>(&CurrentAudioTrack.LastPresentationTime)));
					if ((CurrentPresentationTime - LastAudioPresentationTime) > FTimespan::FromMicroseconds(250))
					{
						// UE_LOG(LogMagicLeapMediaCodec, Warning, TEXT("Waiting on audio to catch up."));
						CurrentTrackData.bBufferPendingRender = true;
						// Reset StartPresentationTime to prevent video stuttering.
						CurrentTrackData.StartPresentationTime = Timecode - CurrentPresentationTime;
						return false;
					}
				}
				if ((CurrentTrackData.StartPresentationTime + CurrentPresentationTime - Timecode) > FTimespan::Zero())
				{
					// UE_LOG(LogMagicLeapMediaCodec, Warning, TEXT("Wait for time to catchup. Start presentation time = %f, Timecode = %f, Presenation Time = %f"), CurrentTrackData.StartPresentationTime.GetTotalMilliseconds(), Timecode.GetTotalMilliseconds(), CurrentPresentationTime.GetTotalMilliseconds());
					CurrentTrackData.bBufferPendingRender = true;
					return false;
				}
				break;
			}
			default:
				UE_LOG(LogMagicLeapMediaCodec, Warning, TEXT("What sorcery is this?"));
		}

		CurrentTrackData.bBufferPendingRender = false;
		const FTimespan CachedCurrentPlaybackTime = CurrentPlaybackTime.Load();

		Result = MLMediaCodecReleaseOutputBuffer(CodecHandle, (MLHandle)CurrentTrackData.CurrentBufferIndex, CurrentTrackData.CurrentBufferInfo.size != 0);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecReleaseOutputBuffer(video) failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
			return false;
		}
		if (CurrentPresentationTime >= GetDuration())
		{
			FScopeLock Lock(&CriticalSection);
			bReachedOutputEndOfStream = true;
			bPlaybackCompleted = true;
			bPlaybackCompleted_RenderThread = true;
			CurrentTrackData.StartPresentationTime = FTimespan::Zero();
			UE_LOG(LogMagicLeapMediaCodec, Display, TEXT("Playback ended on stream %s"), *CurrentTrackData.MimeType);
		}
		else if (!CachedCurrentPlaybackTime.IsZero() && CurrentPresentationTime == CachedCurrentPlaybackTime)
		{
			FScopeLock Lock(&CriticalSection);
			bReachedOutputEndOfStream = true;
			bPlaybackCompleted = true;
			bPlaybackCompleted_RenderThread = true;
			CurrentTrackData.StartPresentationTime = FTimespan::Zero();
			UE_LOG(LogMagicLeapMediaCodec, Display, TEXT("Playback ended on stream %s"), *CurrentTrackData.MimeType);
		}

		CurrentTrackData.LastPresentationTime = Timecode;
		CurrentPlaybackTime = CurrentPresentationTime;

		return true;
	}
	else if (CurrentTrackData.CurrentBufferIndex == MLMediaCodec_FormatChanged)
	{
		UE_LOG(LogMagicLeapMediaCodec, Display, TEXT("%s MLMediaCodec_FormatChanged"), *CurrentTrackData.MimeType);

		MLHandle NewFormatHandle;
		MLResult Result = MLMediaCodecGetOutputFormat(CodecHandle, &NewFormatHandle);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecGetOutputFormat() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
			return false;
		}

		UpdateVideoTrackInfo(NewFormatHandle, CurrentTrackData);

		Result = MLMediaFormatDestroy(NewFormatHandle);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaFormatDestroy() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
	}
	else if (CurrentTrackData.CurrentBufferIndex == MLMediaCodec_TryAgainLater)
	{
		if (InputWorker.HasReachedInputEOS())
		{
			// FScopeLock Lock(&CriticalSection);
			bReachedOutputEndOfStream = true;
			bPlaybackCompleted = true;
			bPlaybackCompleted_RenderThread = true;
			CurrentTrackData.StartPresentationTime = FTimespan::Zero();
			UE_LOG(LogMagicLeapMediaCodec, Warning, TEXT("Playback ended on stream %s"), *CurrentTrackData.MimeType);
		}
		else
		{
			// UE_LOG(LogMagicLeapMediaCodec, Warning, TEXT("no video buffers available"));
		}
	}
	// MLMediaCodec_OutputBuffersChanged is deprecated and doesnt need to be handled.
	// else if (CurrentTrackData.CurrentBufferIndex == MLMediaCodec_OutputBuffersChanged){}

	return false;
}

bool FMagicLeapMediaCodecPlayer::ProcessAudioOutputSample(MLHandle& CodecHandle, FTrackData& CurrentTrackData, FTimespan LastAudioSampleTime)
{
	// Doing this to support a thread safe FlushCodecs() function.
	// No point in waiting here, blocking the entire game thread. This would be locked only during a flush operation which requires RenderThread lock as well.
	// When the flush succeeds, we won't immediately get output buffers. We can afford to come back on the next frame.
	FScopeTryLock LockIT(&GT_IT_Mutex);
	if (!LockIT.IsLocked())
	{
		return false;
	}

	// Atomic becuase this will be used on the render thread to determine playback.
	FPlatformAtomics::InterlockedExchange(reinterpret_cast<int64*>(&CurrentTrackData.LastPresentationTime), LastAudioSampleTime.GetTicks());

	if (!CurrentTrackData.bBufferPendingRender)
	{
		MLResult Result = MLMediaCodecDequeueOutputBuffer(CodecHandle, &CurrentTrackData.CurrentBufferInfo, 0, (int64_t*)&CurrentTrackData.CurrentBufferIndex);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecDequeueOutputBuffer failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
			return false;
		}
	}

	FTimespan CurrentPresentationTime = FTimespan::FromMicroseconds(CurrentTrackData.CurrentBufferInfo.presentation_time_us);
	if (0 <= CurrentTrackData.CurrentBufferIndex)
	{
		SIZE_T BufferSize = 0;
		const uint8_t* Buffer = nullptr;
		MLResult Result = MLMediaCodecGetOutputBufferPointer(CodecHandle, (MLHandle)CurrentTrackData.CurrentBufferIndex, &Buffer, (size_t*)(&BufferSize));

		bool bAudioBufferWritten = WriteAudioSample(CurrentTrackData, Buffer, BufferSize);

		Result = MLMediaCodecReleaseOutputBuffer(CodecHandle, CurrentTrackData.CurrentBufferIndex, false);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecReleaseOutputBuffer(audio) failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));

		if (bAudioBufferWritten)
		{
			CurrentTrackData.LastSampleQueueTime = CurrentPresentationTime.GetTicks();
		}
		else
		{
			return false;
		}

		return true;
	}
	else if (CurrentTrackData.CurrentBufferIndex == MLMediaCodec_FormatChanged)
	{
		UE_LOG(LogMagicLeapMediaCodec, Display, TEXT("%s MLMediaCodec_FormatChanged"), *CurrentTrackData.MimeType);

		MLHandle NewFormatHandle;
		MLResult Result = MLMediaCodecGetOutputFormat(CodecHandle, &NewFormatHandle);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeapMediaCodec, Error, TEXT("MLMediaCodecGetOutputFormat() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
			return false;
		}

		UpdateAudioTrackInfo(NewFormatHandle, CurrentTrackData);

		Result = MLMediaFormatDestroy(NewFormatHandle);
		UE_CLOG(Result != MLResult_Ok, LogMagicLeapMediaCodec, Error, TEXT("MLMediaFormatDestroy() failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
	}
	else if (CurrentTrackData.CurrentBufferIndex == MLMediaCodec_TryAgainLater)
	{}
	// MLMediaCodec_OutputBuffersChanged is deprecated and doesnt need to be handled.
	// else if (CurrentTrackData.CurrentBufferIndex == MLMediaCodec_OutputBuffersChanged){}

	return false;
}

bool FMagicLeapMediaCodecPlayer::WriteAudioSample(FTrackData& CurrentTrackData, const uint8* SampleBuffer, uint64 SampleSize)
{
	if ((SampleSize < 0) || (SampleBuffer == nullptr && SampleSize != 0))
	{
		return false;
	}

	CurrentTrackData.SampleDuration = (SampleSize * ETimespan::TicksPerSecond) / (CurrentTrackData.ChannelCount * CurrentTrackData.SampleRate * sizeof(int16));

	// create & add sample to queue
	const TSharedRef<FMagicLeapMediaAudioSample, ESPMode::ThreadSafe> AudioSample = AudioSamplePool->AcquireShared();

	if (AudioSample->Initialize(SampleBuffer, SampleSize, CurrentTrackData.ChannelCount, CurrentTrackData.SampleRate, FTimespan::FromMicroseconds(CurrentTrackData.CurrentBufferInfo.presentation_time_us), CurrentTrackData.SampleDuration))
	{
		Samples->AddAudio(AudioSample);
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
