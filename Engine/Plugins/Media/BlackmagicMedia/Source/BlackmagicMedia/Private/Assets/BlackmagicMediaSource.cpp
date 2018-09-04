// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BlackmagicMediaSource.h"
#include "BlackmagicMediaPrivate.h"

UBlackmagicMediaSource::UBlackmagicMediaSource()
	: UseTimecode(false)
	, CaptureStyle(EBlackmagicMediaCaptureStyle::AudioVideo)
	, AudioChannels(EBlackmagicMediaAudioChannel::Stereo2)
	, bEncodeTimecodeInTexel(false)
	, UseStreamBuffer(false)
	, NumberFrameBuffers(8)
{ }

/*
 * IMediaOptions interface
 */

int64 ConveryAudioEnumToChannels(EBlackmagicMediaAudioChannel InAudioEnum)
{
	switch (InAudioEnum)
	{
	case EBlackmagicMediaAudioChannel::Surround8:
		return 8;
	}
	return 2;
}

bool UBlackmagicMediaSource::GetMediaOption(const FName& Key, bool DefaultValue) const
{
	if (Key == BlackmagicMedia::UseTimecodeOption)
	{
		return UseTimecode;
	}
	else if (Key == BlackmagicMedia::UseStreamBufferOption)
	{
		return UseStreamBuffer;
	}
	else if (Key == BlackmagicMedia::EncodeTimecodeInTexel)
	{
		return bEncodeTimecodeInTexel;
	}

	return Super::GetMediaOption(Key, DefaultValue);
}

int64 UBlackmagicMediaSource::GetMediaOption(const FName& Key, int64 DefaultValue) const
{
	if (Key == BlackmagicMedia::AudioChannelOption)
	{
		return (int64)ConveryAudioEnumToChannels(AudioChannels);
	}
	else if (Key == BlackmagicMedia::CaptureStyleOption)
	{
		return (int64)CaptureStyle;
	}
	else if (Key == BlackmagicMedia::MediaModeOption)
	{
		return (int64)MediaMode.Mode;
	}
	else if (Key == BlackmagicMedia::NumFrameBufferOption)
	{
		return (int64)NumberFrameBuffers;
	}

	return Super::GetMediaOption(Key, DefaultValue);
}

bool UBlackmagicMediaSource::HasMediaOption(const FName& Key) const
{
	if ((Key == BlackmagicMedia::AudioChannelOption) ||
		(Key == BlackmagicMedia::CaptureStyleOption) ||
		(Key == BlackmagicMedia::MediaModeOption) ||
		(Key == BlackmagicMedia::NumFrameBufferOption) ||
		(Key == BlackmagicMedia::UseStreamBufferOption) ||
		(Key == BlackmagicMedia::UseTimecodeOption))
	{
		return true;
	}

	return Super::HasMediaOption(Key);
}

/*
 * UMediaSource interface
 */

FString UBlackmagicMediaSource::GetUrl() const
{
	return MediaPort.ToUrl();
}

bool UBlackmagicMediaSource::Validate() const
{
	return MediaPort.IsValid();
}
