// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapMediaPlayer.h"
#include "IMagicLeapMediaModule.h"
#include "Misc/Paths.h"
#include "Lumin/LuminPlatformFile.h"
#include "IMediaEventSink.h"
#include "MediaSamples.h"
#include "Misc/CoreDelegates.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "ExternalTexture.h"
#include "MagicLeapMediaTextureSample.h"
#include "ExternalOESTextureRenderer.h"
#include "Lumin/LuminEGL.h"
#include "MediaWorker.h"
#include "CameraCaptureComponent.h"
#include "MagicLeapHelperVulkan.h"

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

#include "ml_media_player.h"

#define LOCTEXT_NAMESPACE "FMagicLeapMediaModule"

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
	, ExternalRenderer(GSupportsImageExternal ? nullptr : new FExternalOESTextureRenderer(false))
	, Display(EGL_NO_DISPLAY)
	, Context(EGL_NO_CONTEXT)
	, SavedDisplay(EGL_NO_DISPLAY)
	, SavedContext(EGL_NO_CONTEXT)
	, bContextCreated(false)
	{}

	~FMagicLeapVideoTextureDataGL()
	{
		delete ExternalRenderer;
		ExternalRenderer = nullptr;
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
			UE_LOG(LogMagicLeapMedia, Error, TEXT("Error setting media player context."));
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
			UE_LOG(LogMagicLeapMedia, Error, TEXT("Error setting unreal context."));
		}
#endif
	}

	EGLImageKHR Image;
	FExternalOESTextureRenderer* ExternalRenderer;
	EGLDisplay Display;
	EGLContext Context;
	EGLDisplay SavedDisplay;
	EGLContext SavedContext;
	bool bContextCreated;
};

FMagicLeapMediaPlayer::FMagicLeapMediaPlayer(IMediaEventSink& InEventSink)
: MediaPlayerHandle(ML_INVALID_HANDLE)
, bMediaPrepared(false)
, CurrentState(EMediaState::Closed)
, EventSink(InEventSink)
, Samples(MakeShared<FMediaSamples, ESPMode::ThreadSafe>())
, VideoSamplePool(new FMagicLeapMediaTextureSamplePool())
, MediaWorker(nullptr)
, bWasMediaPlayingBeforeAppPause(false)
, bPlaybackCompleted(false)
, CurrentPlaybackTime(FTimespan::Zero())
{
	if (FLuminPlatformMisc::ShouldUseVulkan())
	{
		TextureData = MakeShared<FMagicLeapVideoTextureDataVK, ESPMode::ThreadSafe>();
	}
	else
	{
		TextureData = MakeShared<FMagicLeapVideoTextureDataGL, ESPMode::ThreadSafe>();
	}
	
	MLResult Result = MLMediaPlayerCreate(&MediaPlayerHandle);
	UE_CLOG(Result != MLResult_Ok, LogMagicLeapMedia, Error, TEXT("MLMediaPlayerCreate failed with error %s."), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
	CurrentState = (Samples.IsValid() && Result == MLResult_Ok) ? EMediaState::Closed : EMediaState::Error;
	
	if (!GSupportsImageExternal)
	{
		MediaWorker = new FMediaWorker(MediaPlayerHandle, CriticalSection);
	}
}

FMagicLeapMediaPlayer::~FMagicLeapMediaPlayer()
{
	Close();

	delete MediaWorker;
	MediaWorker = nullptr;

	if (MLHandleIsValid(MediaPlayerHandle))
	{
		if (GSupportsImageExternal)
		{
			if (FLuminPlatformMisc::ShouldUseVulkan())
			{
				// Unregister the external texture on render thread
				struct FReleaseVideoResourcesParams
				{
					FMagicLeapMediaPlayer* MediaPlayer;
					TSharedPtr<FMagicLeapVideoTextureDataVK, ESPMode::ThreadSafe> TextureData;
					FGuid PlayerGuid;
					MLHandle MediaPlayerHandle;
				};

				FReleaseVideoResourcesParams Params = { this, StaticCastSharedPtr<FMagicLeapVideoTextureDataVK>(TextureData), PlayerGuid, MediaPlayerHandle };

				ENQUEUE_RENDER_COMMAND(MagicLeapMediaPlayerDestroy)(
					[Params](FRHICommandListImmediate& RHICmdList)
					{
						FExternalTextureRegistry::Get().UnregisterExternalTexture(Params.PlayerGuid);

						if (Params.TextureData->PreviousNativeBuffer != 0 && MLHandleIsValid(Params.TextureData->PreviousNativeBuffer))
						{
							Params.MediaPlayer->RenderThreadReleaseNativeBuffer(Params.MediaPlayerHandle, Params.TextureData->PreviousNativeBuffer);
						}

						if (MLHandleIsValid(Params.MediaPlayerHandle))
						{
							MLResult Result = MLMediaPlayerDestroy(Params.MediaPlayerHandle);
							UE_CLOG(Result != MLResult_Ok, LogMagicLeapMedia, Error, TEXT("MLMediaPlayerDestroy failed with error %s."), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
						}
					});
			}
			else
			{
				// Unregister the external texture on render thread
				struct FReleaseVideoResourcesParams
				{
					FMagicLeapMediaPlayer* MediaPlayer;
					// Can we make this a TWeakPtr? We need to ensure that FMagicLeapMediaPlayer::TextureData is not destroyed before
					// we unregister the external texture and releae the VideoTexture.
					TSharedPtr<FMagicLeapVideoTextureDataGL, ESPMode::ThreadSafe> TextureData;
					FGuid PlayerGuid;
					MLHandle MediaPlayerHandle;
				};

				FReleaseVideoResourcesParams Params = { this, StaticCastSharedPtr<FMagicLeapVideoTextureDataGL>(TextureData), PlayerGuid, MediaPlayerHandle };

				ENQUEUE_RENDER_COMMAND(MagicLeapMediaPlayerDestroy)(
					[Params](FRHICommandListImmediate& RHICmdList)
					{
						FExternalTextureRegistry::Get().UnregisterExternalTexture(Params.PlayerGuid);
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
							Params.MediaPlayer->RenderThreadReleaseNativeBuffer(Params.MediaPlayerHandle, Params.TextureData->PreviousNativeBuffer);
						}

						MLResult Result = MLMediaPlayerDestroy(Params.MediaPlayerHandle);
						UE_CLOG(Result != MLResult_Ok, LogMagicLeapMedia, Error, TEXT("MLMediaPlayerDestroy failed with error %s."), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
					});
			}

			FlushRenderingCommands();
		}
		else
		{
			MLResult Result = MLMediaPlayerDestroy(MediaPlayerHandle);
			UE_CLOG(Result != MLResult_Ok, LogMagicLeapMedia, Error, TEXT("MLMediaPlayerDestroy failed with error %s."), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		}

		MediaPlayerHandle = ML_INVALID_HANDLE;
	}

	delete VideoSamplePool;
	VideoSamplePool = nullptr;
}

void FMagicLeapMediaPlayer::Close()
{
	if (CurrentState == EMediaState::Closed || CurrentState == EMediaState::Error)
	{
		return;
	}

	{
		FScopeLock Lock(&CriticalSection);
		bPlaybackCompleted = true;
	}

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

	MLResult StopResult = MLMediaPlayerStop(MediaPlayerHandle);
	UE_CLOG(StopResult != MLResult_Ok, LogMagicLeapMedia, Error, TEXT("MLMediaPlayerStop failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(StopResult)));

	MLResult ResetResult = MLMediaPlayerReset(MediaPlayerHandle);
	UE_CLOG(ResetResult != MLResult_Ok, LogMagicLeapMedia, Error, TEXT("MLMediaPlayerReset failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(ResetResult)));

	CurrentState = EMediaState::Closed;

	bMediaPrepared = false;
	Info.Empty();
	MediaUrl = FString();
	VideoSamplePool->Reset();
	TrackInfo.Empty();
	SelectedTrack.Empty();

	// notify listeners
	EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
	EventSink.ReceiveMediaEvent(EMediaEvent::MediaClosed);
}

IMediaCache& FMagicLeapMediaPlayer::GetCache()
{
	return *this;
}

IMediaControls& FMagicLeapMediaPlayer::GetControls()
{
	return *this;
}

FString FMagicLeapMediaPlayer::GetInfo() const
{
	return Info;
}

FName FMagicLeapMediaPlayer::GetPlayerName() const
{
	static FName PlayerName(TEXT("MagicLeapMedia"));
	return PlayerName;
}

IMediaSamples& FMagicLeapMediaPlayer::GetSamples()
{
	return *Samples.Get();
}

FString FMagicLeapMediaPlayer::GetStats() const
{
	return TEXT("MagicLeapMedia stats information not implemented yet");
}

IMediaTracks& FMagicLeapMediaPlayer::GetTracks()
{
	return *this;
}

FString FMagicLeapMediaPlayer::GetUrl() const
{
	return MediaUrl;
}

IMediaView& FMagicLeapMediaPlayer::GetView()
{
	return *this;
}

bool FMagicLeapMediaPlayer::Open(const FString& Url, const IMediaOptions* Options)
{
	if (CurrentState == EMediaState::Error)
	{
		EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
		return false;
	}

	Close();

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
			UE_LOG(LogMagicLeapMedia, Error, TEXT("File doesn't exist %s."), *FilePath);
			EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
			return false;
		}

		MLResult Result = MLMediaPlayerSetDataSourceForPath(MediaPlayerHandle, TCHAR_TO_UTF8(*FilePath));
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerSetDataSourceForPath for path %s failed with error %s."), *FilePath, UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
			EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
			return false;
		}
	}
	else
	{    
		// open remote media
		MLResult Result = MLMediaPlayerSetDataSourceForURI(MediaPlayerHandle, TCHAR_TO_UTF8(*Url));
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerSetDataSourceForURI for remote media source %s failed with error %s."), *Url, UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
			EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
			return false;
		}
	}

	EventSink.ReceiveMediaEvent(EMediaEvent::MediaConnecting);

	// prepare media
	MediaUrl = Url;

	MLResult Result = MLMediaPlayerPrepareAsync(MediaPlayerHandle);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerPrepareAsync for media source %s failed with error %s"), *Url, UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
		return false;
	}

	CurrentState = EMediaState::Preparing;

	return true;
}

bool FMagicLeapMediaPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* Options)
{
	// TODO: MagicLeapMedia: implement opening media from FArchive
	return false;
}

bool FMagicLeapMediaPlayer::CanControl(EMediaControl Control) const
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

FTimespan FMagicLeapMediaPlayer::GetDuration() const
{
	FTimespan Duration = FTimespan::Zero();

	if (CurrentState == EMediaState::Playing || CurrentState == EMediaState::Paused || CurrentState == EMediaState::Stopped)
	{
		int32 durationMilSec = 0;
		MLResult Result = MLMediaPlayerGetDuration(MediaPlayerHandle, &durationMilSec);
		if (Result == MLResult_Ok)
		{
			Duration = FTimespan::FromMilliseconds(durationMilSec);
		}
		else
		{
			UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerGetDuration failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		}
	}

	return Duration;
}

float FMagicLeapMediaPlayer::GetRate() const
{
	return (CurrentState == EMediaState::Playing) ? 1.0f : 0.0f;
}

EMediaState FMagicLeapMediaPlayer::GetState() const
{
	return CurrentState;
}

EMediaStatus FMagicLeapMediaPlayer::GetStatus() const
{
	return EMediaStatus::None;
}

TRangeSet<float> FMagicLeapMediaPlayer::GetSupportedRates(EMediaRateThinning Thinning) const
{
	TRangeSet<float> Result;

	Result.Add(TRange<float>(0.0f));
	Result.Add(TRange<float>(1.0f));

	return Result;
}

FTimespan FMagicLeapMediaPlayer::GetTime() const
{
	return CurrentPlaybackTime.Load();
}

bool FMagicLeapMediaPlayer::IsLooping() const
{
	return GetMediaPlayerState(MLMediaPlayerPollingStateFlag_IsLooping);
}

bool FMagicLeapMediaPlayer::Seek(const FTimespan& Time)
{
	bool bSuccess = true;

	if ((CurrentState == EMediaState::Closed) || (CurrentState == EMediaState::Error) || (CurrentState == EMediaState::Preparing))
	{
		bSuccess = false;
		UE_LOG(LogMagicLeapMedia, Warning, TEXT("Cannot seek while closed, preparing, or in error state"));
	}
	else if (CurrentState == EMediaState::Playing || CurrentState == EMediaState::Paused)
	{
		MLResult Result = MLMediaPlayerSeekTo(MediaPlayerHandle, Time.GetTotalMilliseconds(), MLMediaSeekMode_Closest_Sync);
		if (Result != MLResult_Ok)
		{
			bSuccess = false;
			UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerSeekTo failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		}
	}

	return bSuccess;
}

bool FMagicLeapMediaPlayer::SetLooping(bool Looping)
{
	MLResult Result = MLMediaPlayerSetLooping(MediaPlayerHandle, Looping);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerSetLooping failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		return false;
	}

	return true;
}

bool FMagicLeapMediaPlayer::SetRate(float Rate)
{
	if ((CurrentState == EMediaState::Closed) || (CurrentState == EMediaState::Error) || (CurrentState == EMediaState::Preparing))
	{
		UE_LOG(LogMagicLeapMedia, Warning, TEXT("Cannot set rate while closed, preparing, or in error state"));
		return false;
	}

	if (Rate == GetRate())
	{
		// rate already set
		return true;
	}

	bool bResult = false;
	if (Rate == 0.0f)
	{
		MLResult Result = MLMediaPlayerPause(MediaPlayerHandle);
		if (Result == MLResult_Ok)
		{
			CurrentState = EMediaState::Paused;
			EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
		}
		else
		{
			UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerPause failed with error %s!"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		}
	}
	else if (Rate == 1.0f)
	{
		bResult = SetRateOne();
	}
	else
	{
		UE_LOG(LogMagicLeapMedia, Error, TEXT("Rate %f not supported by MagicLeapMedia."), Rate);
		bResult = false;
	}

	return bResult;
}

bool FMagicLeapMediaPlayer::SetNativeVolume(float Volume)
{
	bool bSuccess = true;

	if (MLHandleIsValid(MediaPlayerHandle))
	{
		Volume = (Volume < 0.0f) ? 0.0f : ((Volume < 1.0f) ? Volume : 1.0f);
		MLResult Result = MLMediaPlayerSetVolume(MediaPlayerHandle, Volume);
		if (Result != MLResult_Ok)
		{
			bSuccess = false;
			UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerSetVolume failed with error %s."), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		}
	}

	return bSuccess;
}

void FMagicLeapMediaPlayer::SetGuid(const FGuid& Guid)
{
	PlayerGuid = Guid;
}

void FMagicLeapMediaPlayer::TickFetch(FTimespan DeltaTime, FTimespan Timecode)
{
	if (CurrentState != EMediaState::Playing && CurrentState != EMediaState::Paused)
	{
		return;
	}

	int32 currentPositionMilSec = 0;
	MLResult Result = MLMediaPlayerGetCurrentPosition(MediaPlayerHandle, &currentPositionMilSec);
	if (Result == MLResult_Ok)
	{
		CurrentPlaybackTime = FTimespan::FromMilliseconds(currentPositionMilSec);
	}
	else
	{
		UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerGetCurrentPosition failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
	}

	// deal with resolution changes (usually from streams)
	if (GetMediaPlayerState(MLMediaPlayerPollingStateFlag_HasSizeChanged))
	{
		FIntPoint Dimensions = FIntPoint(0, 0);
		TextureData->bIsVideoTextureValid = false;
	}

	if (GSupportsImageExternal)
	{
		if (FLuminPlatformMisc::ShouldUseVulkan())
		{
			struct FWriteVideoSampleParams
			{
				FMagicLeapMediaPlayer* MediaPlayer;
				TWeakPtr<FMagicLeapVideoTextureDataVK, ESPMode::ThreadSafe> TextureData;
				FGuid PlayerGuid;
				MLHandle MediaPlayerHandle;
			};

			FWriteVideoSampleParams Params = { this, StaticCastSharedPtr<FMagicLeapVideoTextureDataVK>(TextureData), PlayerGuid, MediaPlayerHandle };

			ENQUEUE_RENDER_COMMAND(MagicLeapMediaPlayerWriteVideoSample)(
				[Params](FRHICommandListImmediate& RHICmdList)
				{
					auto TextureDataPtr = Params.TextureData.Pin();
			
					if (!Params.MediaPlayer->RenderThreadIsBufferAvailable(Params.MediaPlayerHandle))
					{
						return;
					}

					if (TextureDataPtr->PreviousNativeBuffer != 0 && MLHandleIsValid(TextureDataPtr->PreviousNativeBuffer))
					{
						Params.MediaPlayer->RenderThreadReleaseNativeBuffer(Params.MediaPlayerHandle, TextureDataPtr->PreviousNativeBuffer);
						TextureDataPtr->PreviousNativeBuffer = 0;
					}

					MLHandle NativeBuffer = ML_INVALID_HANDLE;
					if (!Params.MediaPlayer->RenderThreadGetNativeBuffer(Params.MediaPlayerHandle, NativeBuffer, TextureDataPtr->bIsVideoTextureValid))
					{
						return;
					}

					check(MLHandleIsValid(NativeBuffer));

					{
						FScopeLock Lock(&Params.MediaPlayer->CriticalSection);
						if (Params.MediaPlayer->bPlaybackCompleted)
						{
							TextureDataPtr->VideoTexturePool.Empty();
							Params.MediaPlayer->bPlaybackCompleted = false;
						}
					}

					if (!TextureDataPtr->VideoTexturePool.Contains((uint64)NativeBuffer))
					{
						FTextureRHIRef NewMediaTexture;
						if (!FMagicLeapHelperVulkan::GetMediaTexture(NewMediaTexture, TextureDataPtr->VideoSampler, NativeBuffer))
						{
							UE_LOG(LogMagicLeapMedia, Error, TEXT("Failed to get next media texture."));
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

					if (!TextureDataPtr->bIsVideoTextureValid)
					{
						Params.MediaPlayer->RegisterExternalTexture(Params.PlayerGuid, TextureDataPtr->VideoTexture, TextureDataPtr->VideoSampler);
						TextureDataPtr->bIsVideoTextureValid = true;
					}

					TextureDataPtr->PreviousNativeBuffer = NativeBuffer;
				});
		}
		else
		{
			struct FWriteVideoSampleParams
			{
				FMagicLeapMediaPlayer* MediaPlayer;
				TWeakPtr<FMagicLeapVideoTextureDataGL, ESPMode::ThreadSafe> TextureData;
				FGuid PlayerGuid;
				MLHandle MediaPlayerHandle;
			};

			FWriteVideoSampleParams Params = { this, StaticCastSharedPtr<FMagicLeapVideoTextureDataGL>(TextureData), PlayerGuid, MediaPlayerHandle };

			ENQUEUE_RENDER_COMMAND(MagicLeapMediaPlayerWriteVideoSample)(
				[Params](FRHICommandListImmediate& RHICmdList)
				{
					auto TextureDataPtr = Params.TextureData.Pin();

					if (!Params.MediaPlayer->RenderThreadIsBufferAvailable(Params.MediaPlayerHandle))
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
							UE_LOG(LogMagicLeapMedia, Warning, TEXT("CreateTextureExternal2D failed!"));
							return;
						}

						TextureDataPtr->bIsVideoTextureValid = false;
					}

					// MLHandle because Unreal's uint64 is 'unsigned long long *' whereas uint64_t for the C-API is 'unsigned long *'
					// TODO: Fix the Unreal types for the above comment.
					MLHandle nativeBuffer = ML_INVALID_HANDLE;
					if (!Params.MediaPlayer->RenderThreadGetNativeBuffer(Params.MediaPlayerHandle, nativeBuffer, TextureDataPtr->bIsVideoTextureValid))
					{
						return;
					}

					int32 CurrentFramePosition = 0;
					if (!Params.MediaPlayer->RenderThreadGetCurrentPosition(Params.MediaPlayerHandle, CurrentFramePosition))
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
						Params.MediaPlayer->RenderThreadReleaseNativeBuffer(Params.MediaPlayerHandle, TextureDataPtr->PreviousNativeBuffer);
					}
					TextureDataPtr->PreviousNativeBuffer = nativeBuffer;

					// Wrap latest decoded frame into a new gl texture oject
					TextureDataPtr->Image = eglCreateImageKHR(TextureDataPtr->Display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, (EGLClientBuffer)(void *)nativeBuffer, NULL);
					if (TextureDataPtr->Image == EGL_NO_IMAGE_KHR)
					{
						EGLint errorcode = eglGetError();
						UE_LOG(LogMagicLeapMedia, Error, TEXT("Failed to create EGLImage from the buffer. %d"), errorcode);
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

					if (!TextureDataPtr->bIsVideoTextureValid)
					{
						FSamplerStateInitializerRHI SamplerStateInitializer(SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp);
						FSamplerStateRHIRef SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
						Params.MediaPlayer->RegisterExternalTexture(Params.PlayerGuid, MediaVideoTexture, SamplerStateRHI);
						TextureDataPtr->bIsVideoTextureValid = true;
					}
				});
		}
	}
	else
	{
		FMediaVideoTrackFormat TrackFormat;
		bool bTrackFormatValid = GetVideoTrackFormat(0, 0, TrackFormat);
		if (!bTrackFormatValid)
		{
			UE_LOG(LogMagicLeapMedia, Error, TEXT("Invalid track format. Skipping frame."));
			return;
		}

		auto VideoSample = VideoSamplePool->AcquireShared();
		if (!VideoSample->Initialize(TrackFormat.Dim, FTimespan::FromSeconds(1.0 / TrackFormat.FrameRate)))
		{
			UE_LOG(LogMagicLeapMedia, Error, TEXT("Could not initialize video sample."));
			return;
		}

		struct FWriteVideoSampleParams
		{
			FMagicLeapMediaPlayer* MediaPlayer;
			TWeakPtr<FMagicLeapVideoTextureDataGL, ESPMode::ThreadSafe> TextureData;
			MLHandle MediaPlayerHandle;
			TWeakPtr<FMediaSamples, ESPMode::ThreadSafe> SamplesPtr;
			TSharedRef<FMagicLeapMediaTextureSample, ESPMode::ThreadSafe> VideoSample;
			FCriticalSection* CriticalSectionPtr;
			FMediaWorker* MediaWorkerPtr;
		}
		Params = { this, StaticCastSharedPtr<FMagicLeapVideoTextureDataGL>(TextureData), MediaPlayerHandle, Samples, VideoSample, &CriticalSection, MediaWorker };

		ENQUEUE_RENDER_COMMAND(MagicLeapMediaPlayerWriteVideoSample)(
			[Params](FRHICommandListImmediate& RHICmdList)
			{
				auto PinnedTextureData = Params.TextureData.Pin();
				auto PinnedSamples = Params.SamplesPtr.Pin();

				if (!PinnedTextureData.IsValid() || !PinnedSamples.IsValid())
				{
					UE_LOG(LogMagicLeapMedia, Error, TEXT("Invalid texture data or samples."));
					return;
				}

				if (FLuminPlatformMisc::ShouldUseVulkan() || FLuminPlatformMisc::ShouldUseDesktopOpenGL())
				{
					bool BufferAvailable = false;
					{
						FScopeLock Lock(Params.CriticalSectionPtr);
						if (Params.MediaWorkerPtr->NextBufferAvailable.GetValue() > 0)
						{
							FIntPoint CurrentDim;
							FTimespan CurrentTime;
							void* ReadBuffer = Params.MediaWorkerPtr->GetReadBuffer(&CurrentDim, &CurrentTime);
							Params.VideoSample->Initialize(CurrentDim, Params.VideoSample->GetDuration());
							Params.VideoSample->InitializeBuffer(CurrentTime, ReadBuffer, true);
							Params.MediaWorkerPtr->NextBufferAvailable.Decrement();
							BufferAvailable = true;
						}
					}

					if (BufferAvailable)
					{
						PinnedSamples->AddVideo(Params.VideoSample);
					}
				}
				else
				{
					if (!Params.MediaPlayer->RenderThreadIsBufferAvailable(Params.MediaPlayerHandle))
					{
						return;
					}

					// MLHandle because Unreal's uint64 is 'unsigned long long *' whereas uint64_t for the C-API is 'unsigned long *'
					// TODO: Fix the Unreal types for the above comment.
					MLHandle NativeBuffer = ML_INVALID_HANDLE;
					if (!Params.MediaPlayer->RenderThreadGetNativeBuffer(Params.MediaPlayerHandle, NativeBuffer, PinnedTextureData->bIsVideoTextureValid))
					{
						UE_LOG(LogMagicLeapMedia, Error, TEXT("Error acquiring next video buffer"));
						return;
					}

					// write frame into texture
					FRHITexture2D* Texture = Params.VideoSample->InitializeTexture(Params.MediaPlayer->GetTime());
					if (Texture != nullptr)
					{
						int32 Resource = *reinterpret_cast<int32*>(Texture->GetNativeResource());
						bool TextureCopyResult = PinnedTextureData->ExternalRenderer->CopyFrameTexture(Resource, NativeBuffer, Params.VideoSample->GetDim(), nullptr);
						if (!TextureCopyResult)
						{
							UE_LOG(LogMagicLeapMedia, Error, TEXT("CopyFrameTexture failed"));
						}
					}

					Params.MediaPlayer->RenderThreadReleaseNativeBuffer(Params.MediaPlayerHandle, NativeBuffer);
					PinnedSamples->AddVideo(Params.VideoSample);
				}
			});
	}
}

void FMagicLeapMediaPlayer::RegisterExternalTexture(const FGuid& InGuid, FTextureRHIRef& InTextureRHI, FSamplerStateRHIRef& InSamplerStateRHI)
{
	FExternalTextureRegistry::Get().RegisterExternalTexture(InGuid, InTextureRHI, InSamplerStateRHI, FLinearColor(UScale, 0.0f, 0.0f, VScale), FLinearColor(UOffset,  VOffset, 0.0f, 0.0f));
}

void FMagicLeapMediaPlayer::TickInput(FTimespan DeltaTime, FTimespan Timecode)
{
	if (!bMediaPrepared)
	{
		bMediaPrepared = GetMediaPlayerState(MLMediaPlayerPollingStateFlag_HasBeenPrepared);

		if (bMediaPrepared)
		{
			CurrentState = EMediaState::Stopped;

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

			uint32 NumTracks = 0;
			MLMediaPlayerGetTrackCount(MediaPlayerHandle, &NumTracks);
			for (uint32 i = 0; i < NumTracks; ++i)
			{
				MLMediaPlayerTrackType TrackType;
				MLMediaPlayerGetTrackType(MediaPlayerHandle, i, &TrackType);
				switch(TrackType)
				{
					case MediaPlayerTrackType_Video:
						TrackInfo[EMediaTrackType::Video].Add(static_cast<int32>(i));
						SelectedTrack[EMediaTrackType::Video] = 0;
						break;
					case MediaPlayerTrackType_Audio:
						TrackInfo[EMediaTrackType::Audio].Add(static_cast<int32>(i));
						SelectedTrack[EMediaTrackType::Audio] = 0;
						break;
					case MediaPlayerTrackType_TimedText:
						TrackInfo[EMediaTrackType::Caption].Add(static_cast<int32>(i));
						SelectedTrack[EMediaTrackType::Caption] = 0;
						break;
					case MediaPlayerTrackType_Subtitle:
						TrackInfo[EMediaTrackType::Subtitle].Add(static_cast<int32>(i));
						SelectedTrack[EMediaTrackType::Subtitle] = 0;
						break;
					case MediaPlayerTrackType_Metadata:
						TrackInfo[EMediaTrackType::Metadata].Add(static_cast<int32>(i));
						SelectedTrack[EMediaTrackType::Metadata] = 0;
						break;
				}
			}
			// notify listeners
			EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
			EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpened);

			if (!GSupportsImageExternal)
			{
				MediaWorker->InitThread();
			}
		}
		else
		{
			return;
		}
	}

	if (GetMediaPlayerState(MLMediaPlayerPollingStateFlag_HasSeekCompleted))
	{
		EventSink.ReceiveMediaEvent(EMediaEvent::SeekCompleted);
	}

	if (CurrentState != EMediaState::Playing)
	{
		return;
	}

	if (GetMediaPlayerState(MLMediaPlayerPollingStateFlag_HasPlaybackCompleted))
	{
		{
			FScopeLock Lock(&CriticalSection);
			bPlaybackCompleted = true;
		}
		if (!IsLooping())
		{
			CurrentState = EMediaState::Stopped;
		}
		EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackEndReached);
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
		ResumeHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FMagicLeapMediaPlayer::HandleApplicationHasEnteredForeground);
	}
	if (!PauseHandle.IsValid())
	{
		PauseHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FMagicLeapMediaPlayer::HandleApplicationWillEnterBackground);
	}
}

bool FMagicLeapMediaPlayer::GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const
{
	// TODO: can we implement this using the audio handle coming from MLMediaPlayerGetAudioHandle()?
	return false;
}

int32 FMagicLeapMediaPlayer::GetNumTracks(EMediaTrackType TrackType) const
{
	if (TrackInfo.Contains(TrackType))
	{
		return TrackInfo[TrackType].Num();
	}

	return 0;
}

int32 FMagicLeapMediaPlayer::GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return ((TrackIndex >= 0) && (TrackIndex < GetNumTracks(TrackType))) ? 1 : 0;
}

int32 FMagicLeapMediaPlayer::GetSelectedTrack(EMediaTrackType TrackType) const
{
	if (SelectedTrack.Contains(TrackType))
	{
		return SelectedTrack[TrackType];
	}

	return INDEX_NONE;
}

FText FMagicLeapMediaPlayer::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return FText::GetEmpty();
}

int32 FMagicLeapMediaPlayer::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return (GetSelectedTrack(TrackType) != INDEX_NONE) ? 0 : INDEX_NONE;
}

FString FMagicLeapMediaPlayer::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if (TrackInfo.Contains(TrackType) && TrackIndex >= 0 && TrackIndex < TrackInfo[TrackType].Num())
	{
		char* TrackLanguage = nullptr;
		MLResult Result = MLMediaPlayerGetTrackLanguage(MediaPlayerHandle, static_cast<uint32>(TrackInfo[TrackType][TrackIndex]), &TrackLanguage);
		if (Result == MLResult_Ok)
		{
			FString Language(UTF8_TO_TCHAR(TrackLanguage));
			free(TrackLanguage);
			return Language;
		}
		else
		{
			UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerGetTrackLanguage failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		}
	}

	return FString();
}

FString FMagicLeapMediaPlayer::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	// Track names not supported in ML.
	return FString();
}

bool FMagicLeapMediaPlayer::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const
{
	if ((FormatIndex != 0) || TrackIndex >= TrackInfo[EMediaTrackType::Video].Num())
	{
		return false;
	}

	int32 width = 0;
	int32 height = 0;
	MLResult Result = MLMediaPlayerGetVideoSize(MediaPlayerHandle, &width, &height);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerGetVideoSize failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		return false;
	}

	OutFormat.Dim = FIntPoint(width, height);
	// TODO: Don't hardcode. Get from C-API. ml_media_player api does not provide that right now. Try the ml_media_codec api.
	OutFormat.FrameRate = 30.0f;
	OutFormat.FrameRates = TRange<float>(30.0f);
	OutFormat.TypeName = TEXT("BGRA");
	return true;
}

bool FMagicLeapMediaPlayer::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	if (TrackInfo.Contains(TrackType) && CurrentState != EMediaState::Preparing)
	{
		if (TrackInfo[TrackType].IsValidIndex(TrackIndex))
		{
			MLResult Result = MLMediaPlayerSelectTrack(MediaPlayerHandle, static_cast<uint32>(TrackInfo[TrackType][TrackIndex]));
			if (Result == MLResult_Ok)
			{
				SelectedTrack[TrackType] = TrackIndex;
				return true;
			}
			else
			{
				UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerSelectTrack failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
			}
		}
	}
	return false;
}

bool FMagicLeapMediaPlayer::SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex)
{
	return false;
}

void FMagicLeapMediaPlayer::HandleApplicationHasEnteredForeground()
{
	// check state in case changed before ticked
	if (CurrentState == EMediaState::Paused && bWasMediaPlayingBeforeAppPause)
	{
		// pause
		SetRate(1.0f);
	}
}

void FMagicLeapMediaPlayer::HandleApplicationWillEnterBackground()
{
	bWasMediaPlayingBeforeAppPause = (CurrentState == EMediaState::Playing);
	// check state in case changed before ticked
	if (bWasMediaPlayingBeforeAppPause)
	{
		// pause
		SetRate(0.0f);
	}
}

FIntPoint FMagicLeapMediaPlayer::GetVideoDimensions() const
{
	int32 width = 0;
	int32 height = 0;
	MLResult Result = MLMediaPlayerGetVideoSize(MediaPlayerHandle, &width, &height);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerGetVideoSize failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
	}
	return FIntPoint(width, height);
}

bool FMagicLeapMediaPlayer::SetRateOne()
{
	MLResult Result = MLMediaPlayerStart(MediaPlayerHandle);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerStart failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		return false;
	}
	
	CurrentState = EMediaState::Playing;
	EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackResumed);
	return true;
}

bool FMagicLeapMediaPlayer::GetMediaPlayerState(uint16 FlagToPoll) const
{
	uint16_t StateFlags = 0;
	MLResult Result = MLMediaPlayerPollStates(MediaPlayerHandle, FlagToPoll, &StateFlags);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerPollStates failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		return false;
	}

	return FlagToPoll & StateFlags;
}

bool FMagicLeapMediaPlayer::RenderThreadIsBufferAvailable(MLHandle InMediaPlayerHandle)
{
	ensureMsgf(IsInRenderingThread(), TEXT("RenderThreadIsBufferAvailable called outside of render thread"));
	uint16_t StateFlags = 0;
	MLResult Result = MLMediaPlayerPollStates(InMediaPlayerHandle, MLMediaPlayerPollingStateFlag_IsBufferAvailable, &StateFlags);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerPollStates failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		return false;
	}

	return MLMediaPlayerPollingStateFlag_IsBufferAvailable & StateFlags;
}

bool FMagicLeapMediaPlayer::RenderThreadGetNativeBuffer(const MLHandle InMediaPlayerHandle, MLHandle& NativeBuffer, bool& OutIsVideoTextureValid)
{
	ensureMsgf(IsInRenderingThread(), TEXT("RenderThreadGetNativeBuffer called outside of render thread"));
	MLResult Result = MLMediaPlayerAcquireNextAvailableBuffer(InMediaPlayerHandle, &NativeBuffer);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerAcquireNextAvailableBuffer failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		return false;
	}

	Result = MLMediaPlayerGetFrameTransformationMatrix(InMediaPlayerHandle, FrameTransformationMatrix);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerGetFrameTransformationMatrix failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		return false;
	}

	if (UScale != FrameTransformationMatrix[0] ||
		UOffset != FrameTransformationMatrix[12] ||
		(-VScale) != FrameTransformationMatrix[5] /*||
		(1.0f - VOffset) != FrameTransformationMatrix[13]*/)
	{
		UScale = FrameTransformationMatrix[0];
		UOffset = FrameTransformationMatrix[12];
		VScale = -FrameTransformationMatrix[5];
		//VOffset = 1.0f - FrameTransformationMatrix[13];
		OutIsVideoTextureValid = false;
	}

	return true;
}

bool FMagicLeapMediaPlayer::RenderThreadReleaseNativeBuffer(const MLHandle InMediaPlayerHandle, MLHandle NativeBuffer)
{
	ensureMsgf(IsInRenderingThread(), TEXT("RenderThreadReleaseNativeBuffer called outside of render thread"));
	MLResult Result = MLMediaPlayerReleaseBuffer(InMediaPlayerHandle, NativeBuffer);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerReleaseBuffer failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		return false;
	}

	return true;
}

bool FMagicLeapMediaPlayer::RenderThreadGetCurrentPosition(const MLHandle InMediaPlayerHandle, int32& CurrentPosition)
{
	ensureMsgf(IsInRenderingThread(), TEXT("RenderThreadGetCurrentPosition called outside of render thread"));
	MLResult Result = MLMediaPlayerGetCurrentPosition(InMediaPlayerHandle, &CurrentPosition);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeapMedia, Error, TEXT("MLMediaPlayerGetCurrentPosition failed with error %s"), UTF8_TO_TCHAR(MLMediaResultGetString(Result)));
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
