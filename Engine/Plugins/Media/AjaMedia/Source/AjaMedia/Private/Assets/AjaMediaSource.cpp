// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaSource.h"
#include "AjaMediaPrivate.h"

UAjaMediaSource::UAjaMediaSource()
	: TimecodeFormat(EAjaMediaTimecodeFormat::None)
	, bCaptureWithAutoCirculating(true)
	, bCaptureAncillary1(false)
	, bCaptureAncillary2(false)
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
	if (Key == AjaMediaOption::CaptureAncillary1)
	{
		return bCaptureAncillary1;
	}
	if (Key == AjaMediaOption::CaptureAncillary2)
	{
		return bCaptureAncillary2;
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
	if (Key == AjaMediaOption::FrameRateNumerator)
	{
		const FAjaMediaMode CurrentMediaMode = GetMediaMode();
		return CurrentMediaMode.FrameRate.Numerator;
	}
	if (Key == AjaMediaOption::FrameRateDenominator)
	{
		const FAjaMediaMode CurrentMediaMode = GetMediaMode();
		return CurrentMediaMode.FrameRate.Denominator;
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

bool UAjaMediaSource::HasMediaOption(const FName& Key) const
{
	if ((Key == AjaMediaOption::FrameRateNumerator) ||
		(Key == AjaMediaOption::FrameRateDenominator) ||
		(Key == AjaMediaOption::TimecodeFormat) ||
		(Key == AjaMediaOption::CaptureWithAutoCirculating) ||
		(Key == AjaMediaOption::CaptureAncillary1) ||
		(Key == AjaMediaOption::CaptureAncillary2) ||
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

void UAjaMediaSource::OverrideMediaMode(const FAjaMediaMode& InMediaMode)
{
	bIsDefaultModeOverriden = true;
	MediaMode = InMediaMode;
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

	return true;
}

#if WITH_EDITOR
bool UAjaMediaSource::CanEditChange(const UProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UAjaMediaSource, MaxNumAncillaryFrameBuffer))
	{
		return bCaptureAncillary1 || bCaptureAncillary2;
	}
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UAjaMediaSource, bEncodeTimecodeInTexel))
	{
		return TimecodeFormat != EAjaMediaTimecodeFormat::None && bCaptureVideo;
	}	

	return true;
}

void UAjaMediaSource::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UAjaMediaSource, bCaptureWithAutoCirculating))
	{
		if (!bCaptureWithAutoCirculating)
		{
			bCaptureAncillary1 = false;
			bCaptureAncillary2 = false;
			bCaptureAudio = false;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif //WITH_EDITOR
