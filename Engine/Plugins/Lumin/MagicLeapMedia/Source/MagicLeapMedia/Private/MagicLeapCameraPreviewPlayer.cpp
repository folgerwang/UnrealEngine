// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE:  All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law.  Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY.  Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure  of  this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of  COMPANY.   ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC  PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE  OF THIS
// SOURCE CODE  WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES.  THE RECEIPT OR POSSESSION OF  THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT  MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------*/
// %BANNER_END%

#include "MagicLeapCameraPreviewPlayer.h"
#include "IMagicLeapMediaModule.h"
#include "IMediaEventSink.h"
#include "CameraCaptureComponent.h"

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

bool FMagicLeapCameraPreviewPlayer::RenderThreadGetNativeBuffer(const MLHandle MediaPlayerHandle, MLHandle& NativeBuffer)
{
	ensureMsgf(IsInRenderingThread(), TEXT("RenderThreadGetNativeBuffer called outside of render thread"));
	(void)MediaPlayerHandle;
	NativeBuffer = UCameraCaptureComponent::GetPreviewHandle();
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