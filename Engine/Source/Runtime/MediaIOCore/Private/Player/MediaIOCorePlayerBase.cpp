// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaIOCorePlayerBase.h"

#include "Engine/Engine.h"
#include "Engine/TimecodeProvider.h"
#include "IMediaEventSink.h"
#include "IMediaOptions.h"
#include "MediaIOCoreSamples.h"
#include "Misc/App.h"
#include "Misc/ScopeLock.h"
#include "TimeSynchronizableMediaSource.h"


#define LOCTEXT_NAMESPACE "MediaIOCorePlayerBase"

/* FMediaIOCorePlayerBase structors
 *****************************************************************************/

FMediaIOCorePlayerBase::FMediaIOCorePlayerBase(IMediaEventSink& InEventSink)
	:bIsTimecodeLogEnable(false)
	, CurrentState(EMediaState::Closed)
	, CurrentTime(FTimespan::Zero())
	, EventSink(InEventSink)
	, LastAudioChannels(0)
	, LastAudioSampleRate(0)
	, LastVideoDim(FIntPoint::ZeroValue)
	, VideoFrameRate(30, 1)
	, LastFrameDropCount(0)
	, Samples(new FMediaIOCoreSamples)
	, bUseFrameTimecode(false)
	, bUseTimeSynchronization(false)
	, VideoSampleFormat(EMediaTextureSampleFormat::CharBGRA)
	, PreviousFrameTimespan(FTimespan::Zero())
{
}

FMediaIOCorePlayerBase::~FMediaIOCorePlayerBase()
{
	delete Samples;
	Samples = nullptr;
}

/* IMediaPlayer interface
*****************************************************************************/

void FMediaIOCorePlayerBase::Close()
{
	CurrentState = EMediaState::Closed;
	CurrentTime = FTimespan::Zero();
	LastVideoDim = FIntPoint::ZeroValue;
	EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
	EventSink.ReceiveMediaEvent(EMediaEvent::MediaClosed);
}

FString FMediaIOCorePlayerBase::GetInfo() const
{
	FString Info;

	if (LastAudioChannels > 0)
	{
		Info += FString::Printf(TEXT("Stream\n"));
		Info += FString::Printf(TEXT("    Type: Audio\n"));
		Info += FString::Printf(TEXT("    Channels: %i\n"), LastAudioChannels);
		Info += FString::Printf(TEXT("    Sample Rate: %i Hz\n"), LastAudioSampleRate);
		Info += FString::Printf(TEXT("    Bits Per Sample: 32\n"));
	}

	if (LastVideoDim != FIntPoint::ZeroValue)
	{
		if (!Info.IsEmpty())
		{
			Info += TEXT("\n");
		}
		Info += FString::Printf(TEXT("Stream\n"));
		Info += FString::Printf(TEXT("    Type: Video\n"));
		Info += FString::Printf(TEXT("    Dimensions: %i x %i\n"), LastVideoDim.X, LastVideoDim.Y);
		Info += FString::Printf(TEXT("    Frame Rate: %g fps\n"), VideoFrameRate.AsDecimal());
	}
	return Info;
}

IMediaCache& FMediaIOCorePlayerBase::GetCache()
{
	return *this;
}

IMediaControls& FMediaIOCorePlayerBase::GetControls()
{
	return *this;
}

IMediaSamples& FMediaIOCorePlayerBase::GetSamples()
{
	return *Samples;
}

FString FMediaIOCorePlayerBase::GetStats() const
{
	return FString();
}

IMediaTracks& FMediaIOCorePlayerBase::GetTracks()
{
	return *this;
}

IMediaView& FMediaIOCorePlayerBase::GetView()
{
	return *this;
}

bool FMediaIOCorePlayerBase::Open(const FString& /*Url*/, const IMediaOptions* /*Options*/)
{
	return false;
}

bool FMediaIOCorePlayerBase::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& /*Archive*/, const FString& /*OriginalUrl*/, const IMediaOptions* /*Options*/)
{
	return false;
}

bool FMediaIOCorePlayerBase::TickTimeManagement()
{
	bool bUseDefaultTime = true;
	if (bUseTimeSynchronization)
	{
		FTimecode Timecode = FApp::GetTimecode();
		FFrameRate FrameRate;
		if (const UTimecodeProvider* Provider = GEngine->GetTimecodeProvider())
		{
			if (Provider->GetSynchronizationState() == ETimecodeProviderSynchronizationState::Synchronized)
			{
				FrameRate = Provider->GetFrameRate();
				bUseDefaultTime = false;
			}
		}
		else
		{
			FrameRate = GEngine->DefaultTimecodeFrameRate;
			bUseDefaultTime = false;
		}

		if (!bUseDefaultTime)
		{
			CurrentTime = FTimespan(0, Timecode.Hours, Timecode.Minutes, Timecode.Seconds, static_cast<int32>((ETimespan::TicksPerSecond * Timecode.Frames) / FrameRate.AsDecimal()) * ETimespan::NanosecondsPerTick);
		}
	}
	return bUseDefaultTime;
}

/* IMediaCache interface
 *****************************************************************************/
bool FMediaIOCorePlayerBase::QueryCacheState(EMediaCacheState State, TRangeSet<FTimespan>& OutTimeRanges) const
{
	if (!Samples || Samples->NumVideoSamples() <= 0)
	{
		return false;
	}
	
	bool bHasQueried = false;
	if (State == EMediaCacheState::Loaded)
	{
		const FTimespan FrameDuration = FTimespan::FromSeconds(VideoFrameRate.AsInterval());
		const FTimespan NextSampleTime = Samples->GetNextVideoSampleTime();
		OutTimeRanges.Add(TRange<FTimespan>(NextSampleTime, NextSampleTime + FrameDuration * Samples->NumVideoSamples()));
		bHasQueried = true;
	}

	return bHasQueried;
}

int32 FMediaIOCorePlayerBase::GetSampleCount(EMediaCacheState State) const
{
	int32 Count = 0;
	if (State == EMediaCacheState::Loaded)
	{
		if (Samples)
		{
			Count = Samples->NumVideoSamples();
		}
	}

	return Count;
}


/* IMediaControls interface
 *****************************************************************************/

bool FMediaIOCorePlayerBase::CanControl(EMediaControl Control) const
{
	return false;
}


FTimespan FMediaIOCorePlayerBase::GetDuration() const
{
	return (CurrentState == EMediaState::Playing) ? FTimespan::MaxValue() : FTimespan::Zero();
}


float FMediaIOCorePlayerBase::GetRate() const
{
	return (CurrentState == EMediaState::Playing) ? 1.0f : 0.0f;
}


EMediaState FMediaIOCorePlayerBase::GetState() const
{
	return CurrentState;
}


EMediaStatus FMediaIOCorePlayerBase::GetStatus() const
{
	return (CurrentState == EMediaState::Preparing) ? EMediaStatus::Connecting : EMediaStatus::None;
}

TRangeSet<float> FMediaIOCorePlayerBase::GetSupportedRates(EMediaRateThinning /*Thinning*/) const
{
	TRangeSet<float> Result;
	Result.Add(TRange<float>(0.0f));
	Result.Add(TRange<float>(1.0f));
	return Result;
}

FTimespan FMediaIOCorePlayerBase::GetTime() const
{
	return CurrentTime;
}


bool FMediaIOCorePlayerBase::IsLooping() const
{
	return false; // not supported
}


bool FMediaIOCorePlayerBase::Seek(const FTimespan& Time)
{
	return false; // not supported
}


bool FMediaIOCorePlayerBase::SetLooping(bool Looping)
{
	return false; // not supported
}


bool FMediaIOCorePlayerBase::SetRate(float Rate)
{
	return false;
}


/* IMediaTracks interface
 *****************************************************************************/

bool FMediaIOCorePlayerBase::GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const
{
	if (!IsHardwareReady() || TrackIndex != 0 || FormatIndex != 0)
	{
		return false;
	}

	OutFormat = AudioTrackFormat;
	return true;
}


int32 FMediaIOCorePlayerBase::GetNumTracks(EMediaTrackType TrackType) const
{
	return 1;
}

int32 FMediaIOCorePlayerBase::GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return 1;
}


int32 FMediaIOCorePlayerBase::GetSelectedTrack(EMediaTrackType TrackType) const
{
	switch (TrackType)
	{
	case EMediaTrackType::Audio:
	case EMediaTrackType::Video:
		return 0;

	default:
		return INDEX_NONE;
	}
}


FText FMediaIOCorePlayerBase::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if (!IsHardwareReady() || TrackIndex != 0)
	{
		return FText::GetEmpty();
	}

	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		return LOCTEXT("DefaultAudioTrackName", "Audio Track");

	case EMediaTrackType::Video:
		return LOCTEXT("DefaultVideoTrackName", "Video Track");

	default:
		return FText::GetEmpty();
	}

	return FText::GetEmpty();
}


int32 FMediaIOCorePlayerBase::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if (TrackType == EMediaTrackType::Video) {
		return 0;
	}
	return INDEX_NONE;
}


FString FMediaIOCorePlayerBase::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return FString();
}


FString FMediaIOCorePlayerBase::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return FString();
}


bool FMediaIOCorePlayerBase::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const
{
	if (!IsHardwareReady() || TrackIndex != 0 || FormatIndex != 0)
	{
		return false;
	}

	OutFormat = VideoTrackFormat;
	return true;
}


bool FMediaIOCorePlayerBase::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	if (!IsHardwareReady() || TrackIndex < INDEX_NONE || TrackIndex != 0)
	{
		return false;
	}

	// Only 1 track supported
	return (TrackType == EMediaTrackType::Audio || TrackType == EMediaTrackType::Video);
}

bool FMediaIOCorePlayerBase::SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex)
{
	return false;
}

bool FMediaIOCorePlayerBase::ReadMediaOptions(const IMediaOptions* Options)
{
	bUseTimeSynchronization = Options->GetMediaOption(TimeSynchronizableMedia::UseTimeSynchronizatioOption, false);
	return true;
}

bool FMediaIOCorePlayerBase::Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
#if !UE_BUILD_SHIPPING
	if (FParse::Command(&Cmd, TEXT("MediaIO")))
	{
		if (FParse::Command(&Cmd, TEXT("ShowInputTimecode")))
		{
			bIsTimecodeLogEnable = true;
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("HideInputTimecode")))
		{
			bIsTimecodeLogEnable = false;
			return true;
		}
	}
#endif
	return false;
}

#undef LOCTEXT_NAMESPACE
