// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaSource.h"
#include "AjaMediaPrivate.h"

#include "MediaIOCorePlayerBase.h"

UAjaMediaSource::UAjaMediaSource()
	: TimecodeFormat(EAjaMediaTimecodeFormat::None)
	, bCaptureWithAutoCirculating(true)
	, bCaptureAncillary(false)
	, bCaptureAudio(false)
	, bCaptureVideo(true)
	, MaxNumAncillaryFrameBuffer(8)
	, AudioChannel(EAjaMediaAudioChannel::Channel8)
	, MaxNumAudioFrameBuffer(8)
	, ColorFormat(EAjaMediaSourceColorFormat::BGRA)
	, MaxNumVideoFrameBuffer(8)
	, bLogDropFrame(true)
	, bEncodeTimecodeInTexel(false)
{ }

/*
 * IMediaOptions interface
 */

bool UAjaMediaSource::GetMediaOption(const FName& Key, bool DefaultValue) const
{
	if (Key == AjaMediaOption::CaptureWithAutoCirculating)
	{
		return bCaptureWithAutoCirculating;
	}
	if (Key == AjaMediaOption::CaptureAncillary)
	{
		return bCaptureAncillary;
	}
	if (Key == AjaMediaOption::CaptureAudio)
	{
		return bCaptureAudio;
	}
	if (Key == AjaMediaOption::CaptureVideo)
	{
		return bCaptureVideo;
	}
	if (Key == AjaMediaOption::LogDropFrame)
	{
		return bLogDropFrame;
	}
	if (Key == AjaMediaOption::EncodeTimecodeInTexel)
	{
		return bEncodeTimecodeInTexel;
	}

	return Super::GetMediaOption(Key, DefaultValue);
}

int64 UAjaMediaSource::GetMediaOption(const FName& Key, int64 DefaultValue) const
{
	if (Key == AjaMediaOption::DeviceIndex)
	{
		return MediaPort.DeviceIndex;
	}
	if (Key == AjaMediaOption::PortIndex)
	{
		return MediaPort.PortIndex;
	}
	if (Key == FMediaIOCoreMediaOption::FrameRateNumerator)
	{
		const FAjaMediaMode CurrentMediaMode = GetMediaMode();
		return CurrentMediaMode.FrameRate.Numerator;
	}
	if (Key == FMediaIOCoreMediaOption::FrameRateDenominator)
	{
		const FAjaMediaMode CurrentMediaMode = GetMediaMode();
		return CurrentMediaMode.FrameRate.Denominator;
	}
	if (Key == FMediaIOCoreMediaOption::ResolutionWidth)
	{
		const FAjaMediaMode CurrentMediaMode = GetMediaMode();
		return CurrentMediaMode.TargetSize.X;
	}
	if (Key == FMediaIOCoreMediaOption::ResolutionHeight)
	{
		const FAjaMediaMode CurrentMediaMode = GetMediaMode();
		return CurrentMediaMode.TargetSize.Y;
	}
	if (Key == AjaMediaOption::TimecodeFormat)
	{
		return (int64)TimecodeFormat;
	}
	if (Key == AjaMediaOption::MaxAncillaryFrameBuffer)
	{
		return MaxNumAncillaryFrameBuffer;
	}
	if (Key == AjaMediaOption::AudioChannel)
	{
		return (int64)AudioChannel;
	}
	if (Key == AjaMediaOption::MaxAudioFrameBuffer)
	{
		return MaxNumAudioFrameBuffer;
	}
	if (Key == AjaMediaOption::AjaVideoFormat)
	{
		const FAjaMediaMode CurrentMediaMode = GetMediaMode();
		return CurrentMediaMode.VideoFormatIndex;
	}
	if (Key == AjaMediaOption::ColorFormat)
	{
		return (int64)ColorFormat;
	}
	if (Key == AjaMediaOption::MaxVideoFrameBuffer)
	{
		return MaxNumVideoFrameBuffer;
	}

	return Super::GetMediaOption(Key, DefaultValue);
}

FString UAjaMediaSource::GetMediaOption(const FName& Key, const FString& DefaultValue) const
{
	if (Key == FMediaIOCoreMediaOption::VideoStandard)
	{
		const FAjaMediaMode CurrentMediaMode = GetMediaMode();
		return CurrentMediaMode.ToString();
	}
	return Super::GetMediaOption(Key, DefaultValue);
}

bool UAjaMediaSource::HasMediaOption(const FName& Key) const
{
	if ((Key == AjaMediaOption::DeviceIndex) ||
		(Key == AjaMediaOption::PortIndex) ||
		(Key == FMediaIOCoreMediaOption::FrameRateNumerator) ||
		(Key == FMediaIOCoreMediaOption::FrameRateDenominator) ||
		(Key == FMediaIOCoreMediaOption::ResolutionWidth) ||
		(Key == FMediaIOCoreMediaOption::ResolutionHeight) ||
		(Key == FMediaIOCoreMediaOption::VideoStandard) ||
		(Key == AjaMediaOption::TimecodeFormat) ||
		(Key == AjaMediaOption::CaptureWithAutoCirculating) ||
		(Key == AjaMediaOption::CaptureAncillary) ||
		(Key == AjaMediaOption::CaptureAudio) ||
		(Key == AjaMediaOption::CaptureVideo) ||
		(Key == AjaMediaOption::MaxAncillaryFrameBuffer) ||
		(Key == AjaMediaOption::AudioChannel) ||
		(Key == AjaMediaOption::MaxAudioFrameBuffer) ||
		(Key == AjaMediaOption::AjaVideoFormat) ||
		(Key == AjaMediaOption::ColorFormat) ||
		(Key == AjaMediaOption::MaxVideoFrameBuffer) ||
		(Key == AjaMediaOption::LogDropFrame) ||
		(Key == AjaMediaOption::EncodeTimecodeInTexel)
		)
	{
		return true;
	}

	return Super::HasMediaOption(Key);
}

FAjaMediaMode UAjaMediaSource::GetMediaMode() const
{
	FAjaMediaMode CurrentMode;
	if (bIsDefaultModeOverriden == false)
	{
		CurrentMode = GetDefault<UAjaMediaSettings>()->GetInputMediaMode(MediaPort);
	}
	else
	{
		CurrentMode = MediaMode;
	}

	return CurrentMode;
}

FAjaMediaConfiguration UAjaMediaSource::GetMediaConfiguration() const
{
	FAjaMediaConfiguration Result;
	Result.MediaPort = MediaPort;
	Result.MediaMode = GetMediaMode();
	Result.bInput = true;
	return Result;
}

/*
 * UMediaSource interface
 */

FString UAjaMediaSource::GetUrl() const
{
	return MediaPort.ToUrl();
}

bool UAjaMediaSource::Validate() const
{
	FString FailureReason;
	const FAjaMediaMode CurrentMode = GetMediaMode();
	if (!FAjaMediaFinder::IsValid(MediaPort, CurrentMode, FailureReason))
	{
		const bool bAddProjectSettingMessage = MediaPort.IsValid() && !bIsDefaultModeOverriden;
		const FString OverrideString = bAddProjectSettingMessage ? TEXT("The project settings haven't been set for this port.") : TEXT("");
		UE_LOG(LogAjaMedia, Warning, TEXT("The MediaSource '%s' is invalid. %s %s"), *GetName(), *FailureReason, *OverrideString);
		return false;
	}

	TUniquePtr<AJA::AJADeviceScanner> Scanner = MakeUnique<AJA::AJADeviceScanner>();
	AJA::AJADeviceScanner::DeviceInfo DeviceInfo;
	if (!Scanner->GetDeviceInfo(MediaPort.DeviceIndex, DeviceInfo))
	{
		UE_LOG(LogAjaMedia, Warning, TEXT("The MediaSource '%s' use the device '%s' that doesn't exist on this machine."), *GetName(), *MediaPort.DeviceName.ToString());
		return false;
	}

	if (!DeviceInfo.bIsSupported)
	{
		UE_LOG(LogAjaMedia, Warning, TEXT("The MediaSource '%s' use the device '%s' that is not supported by the AJA SDK."), *GetName(), *MediaPort.DeviceName.ToString());
		return false;
	}

	if (!DeviceInfo.bCanDoCapture)
	{
		UE_LOG(LogAjaMedia, Warning, TEXT("The MediaSource '%s' use the device '%s' that can't capture."), *GetName(), *MediaPort.DeviceName.ToString());
		return false;
	}

	if (bCaptureAncillary && !DeviceInfo.bCanDoCustomAnc)
	{
		UE_LOG(LogAjaMedia, Warning, TEXT("The MediaSource '%s' use the device '%s' that can't capture Ancillary data."), *GetName(), *MediaPort.DeviceName.ToString());
		return false;
	}

	if (bUseTimeSynchronization && TimecodeFormat == EAjaMediaTimecodeFormat::None)
	{
		UE_LOG(LogAjaMedia, Warning, TEXT("The MediaSource '%s' use time synchronization but doesn't enabled the timecode."), *GetName());
		return false;
	}

	if (bCaptureVideo)
	{
		if (ColorFormat == EAjaMediaSourceColorFormat::BGRA && !DeviceInfo.bSupportPixelFormat8bitARGB)
		{
			UE_LOG(LogAjaMedia, Warning, TEXT("The MediaSource '%s' use the device '%s' that doesn't support the 8bit ARGB pixel format."), *GetName(), *MediaPort.DeviceName.ToString());
			return false;
		}
		if (ColorFormat == EAjaMediaSourceColorFormat::BGR10 && !DeviceInfo.bSupportPixelFormat10bitRGB)
		{
			UE_LOG(LogAjaMedia, Warning, TEXT("The MediaSource '%s' use the device '%s' that doesn't support the 10bit ARGB pixel format."), *GetName(), *MediaPort.DeviceName.ToString());
			return false;
		}
	}

	return true;
}

#if WITH_EDITOR
bool UAjaMediaSource::CanEditChange(const UProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UAjaMediaSource, bEncodeTimecodeInTexel))
	{
		return TimecodeFormat != EAjaMediaTimecodeFormat::None && bCaptureVideo;
	}

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UTimeSynchronizableMediaSource, bUseTimeSynchronization))
	{
		return TimecodeFormat != EAjaMediaTimecodeFormat::None;
	}

	return true;
}
#endif //WITH_EDITOR
