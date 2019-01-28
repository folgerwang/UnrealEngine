// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapCameraPreviewPlayer.h"
#include "IMagicLeapMediaModule.h"
#include "IMediaEventSink.h"
#include "CameraCaptureComponent.h"
#include "ExternalTexture.h"

FMagicLeapCameraPreviewPlayer::FMagicLeapCameraPreviewPlayer(IMediaEventSink& InEventSink)
	: FMagicLeapMediaPlayer(InEventSink)
{}

bool FMagicLeapCameraPreviewPlayer::Open(const FString& Url, const IMediaOptions* Options)
{
	if (CurrentState == EMediaState::Error)
	{
		EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
		return false;
	}

	Close();

	EventSink.ReceiveMediaEvent(EMediaEvent::MediaConnecting);

	CurrentState = EMediaState::Preparing;

	return true;
}

bool FMagicLeapCameraPreviewPlayer::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const
{
	(void)TrackIndex;
	(void)FormatIndex;
	(void)OutFormat;
	OutFormat.Dim = FIntPoint(512, 512);
	// TODO: Don't hardcode. Get from C-API. ml_media_player api does not provide that right now. Try the ml_media_codec api.
	OutFormat.FrameRate = 30.0f;
	OutFormat.FrameRates = TRange<float>(30.0f);
	OutFormat.TypeName = TEXT("BGRA");

	return true;
}

bool FMagicLeapCameraPreviewPlayer::IsLooping() const
{
	return true;
}

FTimespan FMagicLeapCameraPreviewPlayer::GetTime() const
{
	return FTimespan::Zero();
}

FTimespan FMagicLeapCameraPreviewPlayer::GetDuration() const
{
	return FTimespan::Zero();
}

void FMagicLeapCameraPreviewPlayer::RegisterExternalTexture(const FGuid& InGuid, FTextureRHIRef& InTextureRHI, FSamplerStateRHIRef& InSamplerStateRHI)
{
	FExternalTextureRegistry::Get().RegisterExternalTexture(InGuid, InTextureRHI, InSamplerStateRHI, FLinearColor(1.0f, 0.0f, 0.0f, 1.0f), FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
}

bool FMagicLeapCameraPreviewPlayer::SetRateOne()
{
	CurrentState = EMediaState::Playing;
	EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackResumed);

	return true;
}

bool FMagicLeapCameraPreviewPlayer::GetMediaPlayerState(uint16 FlagToPoll) const
{
	(void)FlagToPoll;
	return UCameraCaptureComponent::GetPreviewHandle() != ML_INVALID_HANDLE;
}

bool FMagicLeapCameraPreviewPlayer::RenderThreadIsBufferAvailable(MLHandle MediaPlayerHandle)
{
	ensureMsgf(IsInRenderingThread(), TEXT("RenderThreadIsBufferAvailable called outside of render thread"));
	(void)MediaPlayerHandle;
	return UCameraCaptureComponent::GetPreviewHandle() != ML_INVALID_HANDLE;
}

bool FMagicLeapCameraPreviewPlayer::RenderThreadGetNativeBuffer(const MLHandle MediaPlayerHandle, MLHandle& NativeBuffer, bool& OutIsVideoTextureValid)
{
	ensureMsgf(IsInRenderingThread(), TEXT("RenderThreadGetNativeBuffer called outside of render thread"));
	(void)MediaPlayerHandle;
	NativeBuffer = UCameraCaptureComponent::GetPreviewHandle();
	OutIsVideoTextureValid = true;
	return NativeBuffer != ML_INVALID_HANDLE;
}

bool FMagicLeapCameraPreviewPlayer::RenderThreadReleaseNativeBuffer(const MLHandle MediaPlayerHandle, MLHandle NativeBuffer)
{
	ensureMsgf(IsInRenderingThread(), TEXT("RenderThreadReleaseNativeBuffer called outside of render thread"));
	(void)MediaPlayerHandle;
	(void)NativeBuffer;
	return true;
}

bool FMagicLeapCameraPreviewPlayer::RenderThreadGetCurrentPosition(const MLHandle MediaPlayerHandle, int32& CurrentPosition)
{
	ensureMsgf(IsInRenderingThread(), TEXT("RenderThreadGetCurrentPosition called outside of render thread"));
	(void)MediaPlayerHandle;
	CurrentPosition = 0;
	return true;
}