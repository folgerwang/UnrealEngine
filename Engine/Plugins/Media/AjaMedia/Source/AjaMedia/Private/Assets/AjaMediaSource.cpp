// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaSource.h"
#include "AjaMediaPrivate.h"

UAjaMediaSource::UAjaMediaSource()
	: FrameRate(30, 1)
	, TimecodeFormat(EAjaMediaTimecodeFormat::None)
	, bCaptureWithAutoCirculating(true)
	, bCaptureAncillary1(false)
	, bCaptureAncillary2(false)
	, bCaptureAudio(false)
	, bCaptureVideo(true)
	, MaxNumAncillaryFrameBuffer(8)
	, AudioChannel(EAjaMediaAudioChannel::Channel8)
	, MaxNumAudioFrameBuffer(8)
	, bIsProgressivePicture(true)
	, ColorFormat(EAjaMediaColorFormat::BGRA)
	, MaxNumVideoFrameBuffer(8)
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
	if (Key == AjaMediaOption::IsProgressivePicture)
	{
		return bIsProgressivePicture;
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
		return FrameRate.Numerator;
	}
	if (Key == AjaMediaOption::FrameRateDenominator)
	{
		return FrameRate.Denominator;
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
		(Key == AjaMediaOption::IsProgressivePicture) ||
		(Key == AjaMediaOption::ColorFormat) ||
		(Key == AjaMediaOption::MaxVideoFrameBuffer) ||
		(Key == AjaMediaOption::EncodeTimecodeInTexel)
		)
	{
		return true;
	}

	return Super::HasMediaOption(Key);
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
	return MediaPort.IsValid();
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
