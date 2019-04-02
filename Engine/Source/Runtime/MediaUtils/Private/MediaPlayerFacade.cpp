// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerFacade.h"
#include "MediaUtilsPrivate.h"

#include "HAL/PlatformMath.h"
#include "HAL/PlatformProcess.h"
#include "IMediaCache.h"
#include "IMediaControls.h"
#include "IMediaModule.h"
#include "IMediaOptions.h"
#include "IMediaPlayer.h"
#include "IMediaPlayerFactory.h"
#include "IMediaSamples.h"
#include "IMediaAudioSample.h"
#include "IMediaTextureSample.h"
#include "IMediaTracks.h"
#include "IMediaView.h"
#include "MediaPlayerOptions.h"
#include "Math/NumericLimits.h"
#include "Misc/CoreMisc.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"

#include "MediaHelpers.h"
#include "MediaSampleCache.h"
#include "MediaSampleQueueDepths.h"

#define MEDIAPLAYERFACADE_DISABLE_BLOCKING 0
#define MEDIAPLAYERFACADE_TRACE_SINKOVERFLOWS 0


/** Time spent in media player facade closing media. */
DECLARE_CYCLE_STAT(TEXT("MediaUtils MediaPlayerFacade Close"), STAT_MediaUtils_FacadeClose, STATGROUP_Media);

/** Time spent in media player facade opening media. */
DECLARE_CYCLE_STAT(TEXT("MediaUtils MediaPlayerFacade Open"), STAT_MediaUtils_FacadeOpen, STATGROUP_Media);

/** Time spent in media player facade event processing. */
DECLARE_CYCLE_STAT(TEXT("MediaUtils MediaPlayerFacade ProcessEvent"), STAT_MediaUtils_FacadeProcessEvent, STATGROUP_Media);

/** Time spent in media player facade fetch tick. */
DECLARE_CYCLE_STAT(TEXT("MediaUtils MediaPlayerFacade TickFetch"), STAT_MediaUtils_FacadeTickFetch, STATGROUP_Media);

/** Time spent in media player facade input tick. */
DECLARE_CYCLE_STAT(TEXT("MediaUtils MediaPlayerFacade TickInput"), STAT_MediaUtils_FacadeTickInput, STATGROUP_Media);

/** Time spent in media player facade output tick. */
DECLARE_CYCLE_STAT(TEXT("MediaUtils MediaPlayerFacade TickOutput"), STAT_MediaUtils_FacadeTickOutput, STATGROUP_Media);

/** Time spent in media player facade high frequency tick. */
DECLARE_CYCLE_STAT(TEXT("MediaUtils MediaPlayerFacade TickTickable"), STAT_MediaUtils_FacadeTickTickable, STATGROUP_Media);

/** Player time on main thread during last fetch tick. */
DECLARE_FLOAT_COUNTER_STAT(TEXT("MediaPlayerFacade TickPostEngine Time"), STAT_MediaUtils_FacadeTime, STATGROUP_Media);


/* Local helpers
*****************************************************************************/

namespace MediaPlayerFacade
{
	// @todo gmp: make these configurable in settings?

	const FTimespan AudioPreroll = FTimespan::FromSeconds(1.0);
	const FTimespan MetadataPreroll = FTimespan::FromSeconds(1.0);
}


/* FMediaPlayerFacade structors
*****************************************************************************/

FMediaPlayerFacade::FMediaPlayerFacade()
	: TimeDelay(FTimespan::Zero())
	, BlockOnTime(FTimespan::MinValue())
	, Cache(new FMediaSampleCache)
	, LastRate(0.0f)
	, LastAudioRenderedSampleTime(FTimespan::MinValue())
{ }


FMediaPlayerFacade::~FMediaPlayerFacade()
{
	if (Player.IsValid())
	{
		FScopeLock Lock(&CriticalSection);

		Player->Close();
		Player.Reset();
	}

	delete Cache;
	Cache = nullptr;
}


/* FMediaPlayerFacade interface
*****************************************************************************/

void FMediaPlayerFacade::AddAudioSampleSink(const TSharedRef<TMediaSampleSink<IMediaAudioSample>, ESPMode::ThreadSafe>& SampleSink)
{
	FScopeLock Lock(&CriticalSection);
	AudioSampleSinks.Add(SampleSink);
}


void FMediaPlayerFacade::AddCaptionSampleSink(const TSharedRef<TMediaSampleSink<IMediaOverlaySample>, ESPMode::ThreadSafe>& SampleSink)
{
	CaptionSampleSinks.Add(SampleSink);
}
	

void FMediaPlayerFacade::AddMetadataSampleSink(const TSharedRef<TMediaSampleSink<IMediaBinarySample>, ESPMode::ThreadSafe>& SampleSink)
{
	FScopeLock Lock(&CriticalSection);
	MetadataSampleSinks.Add(SampleSink);
}


void FMediaPlayerFacade::AddSubtitleSampleSink(const TSharedRef<TMediaSampleSink<IMediaOverlaySample>, ESPMode::ThreadSafe>& SampleSink)
{
	SubtitleSampleSinks.Add(SampleSink);
}


void FMediaPlayerFacade::AddVideoSampleSink(const TSharedRef<TMediaSampleSink<IMediaTextureSample>, ESPMode::ThreadSafe>& SampleSink)
{
	VideoSampleSinks.Add(SampleSink);
}


bool FMediaPlayerFacade::CanPause() const
{
	return Player.IsValid() && Player->GetControls().CanControl(EMediaControl::Pause);
}


bool FMediaPlayerFacade::CanPlayUrl(const FString& Url, const IMediaOptions* Options)
{
	IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

	if (MediaModule == nullptr)
	{
		return false;
	}

	const FString RunningPlatformName(FPlatformProperties::IniPlatformName());
	const TArray<IMediaPlayerFactory*>& PlayerFactories = MediaModule->GetPlayerFactories();

	for (IMediaPlayerFactory* Factory : PlayerFactories)
	{
		if (Factory->SupportsPlatform(RunningPlatformName) && Factory->CanPlayUrl(Url, Options))
		{
			return true;
		}
	}

	return false;
}


bool FMediaPlayerFacade::CanResume() const
{
	return Player.IsValid() && Player->GetControls().CanControl(EMediaControl::Resume);
}


bool FMediaPlayerFacade::CanScrub() const
{
	return Player.IsValid() && Player->GetControls().CanControl(EMediaControl::Scrub);
}


bool FMediaPlayerFacade::CanSeek() const
{
	return Player.IsValid() && Player->GetControls().CanControl(EMediaControl::Seek);
}


void FMediaPlayerFacade::Close()
{
	SCOPE_CYCLE_COUNTER(STAT_MediaUtils_FacadeClose);

	if (CurrentUrl.IsEmpty())
	{
		return;
	}

	if (Player.IsValid())
	{
		FScopeLock Lock(&CriticalSection);
		Player->Close();
	}

	BlockOnTime = FTimespan::MinValue();
	Cache->Empty();
	CurrentUrl.Empty();
	LastRate = 0.0f;

	FlushSinks();
}


uint32 FMediaPlayerFacade::GetAudioTrackChannels(int32 TrackIndex, int32 FormatIndex) const
{
	FMediaAudioTrackFormat Format;
	return GetAudioTrackFormat(TrackIndex, FormatIndex, Format) ? Format.NumChannels : 0;
}


uint32 FMediaPlayerFacade::GetAudioTrackSampleRate(int32 TrackIndex, int32 FormatIndex) const
{
	FMediaAudioTrackFormat Format;
	return GetAudioTrackFormat(TrackIndex, FormatIndex, Format) ? Format.SampleRate : 0;
}


FString FMediaPlayerFacade::GetAudioTrackType(int32 TrackIndex, int32 FormatIndex) const
{
	FMediaAudioTrackFormat Format;
	return GetAudioTrackFormat(TrackIndex, FormatIndex, Format) ? Format.TypeName : FString();
}


FTimespan FMediaPlayerFacade::GetDuration() const
{
	return Player.IsValid() ? Player->GetControls().GetDuration() : FTimespan::Zero();
}


const FGuid& FMediaPlayerFacade::GetGuid()
{
	return PlayerGuid;
}


FString FMediaPlayerFacade::GetInfo() const
{
	return Player.IsValid() ? Player->GetInfo() : FString();
}


FText FMediaPlayerFacade::GetMediaName() const
{
	return Player.IsValid() ? Player->GetMediaName() : FText::GetEmpty();
}


int32 FMediaPlayerFacade::GetNumTracks(EMediaTrackType TrackType) const
{
	return Player.IsValid() ? Player->GetTracks().GetNumTracks(TrackType) : 0;
}


int32 FMediaPlayerFacade::GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return Player.IsValid() ? Player->GetTracks().GetNumTrackFormats(TrackType, TrackIndex) : 0;
}


FName FMediaPlayerFacade::GetPlayerName() const
{
	return Player.IsValid() ? Player->GetPlayerName() : NAME_None;
}


float FMediaPlayerFacade::GetRate() const
{
	return Player.IsValid() ? Player->GetControls().GetRate() : 0.0f;
}


int32 FMediaPlayerFacade::GetSelectedTrack(EMediaTrackType TrackType) const
{
	return Player.IsValid() ? Player->GetTracks().GetSelectedTrack((EMediaTrackType)TrackType) : INDEX_NONE;
}


FString FMediaPlayerFacade::GetStats() const
{
	return Player.IsValid() ? Player->GetStats() : FString();
}


TRangeSet<float> FMediaPlayerFacade::GetSupportedRates(bool Unthinned) const
{
	const EMediaRateThinning Thinning = Unthinned ? EMediaRateThinning::Unthinned : EMediaRateThinning::Thinned;

	return Player.IsValid() ? Player->GetControls().GetSupportedRates(Thinning) : TRangeSet<float>();
}


FTimespan FMediaPlayerFacade::GetTime() const
{
	if (!Player.IsValid())
	{
		return FTimespan::Zero(); // no media opened
	}

	FTimespan Result = Player->GetControls().GetTime() - TimeDelay;
	if (Result.GetTicks() < 0)
	{
		Result = FTimespan::Zero();
	}
	return Result;
}


FText FMediaPlayerFacade::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return Player.IsValid() ? Player->GetTracks().GetTrackDisplayName((EMediaTrackType)TrackType, TrackIndex) : FText::GetEmpty();
}


int32 FMediaPlayerFacade::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return Player.IsValid() ? Player->GetTracks().GetTrackFormat((EMediaTrackType)TrackType, TrackIndex) : INDEX_NONE;
}


FString FMediaPlayerFacade::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return Player.IsValid() ? Player->GetTracks().GetTrackLanguage((EMediaTrackType)TrackType, TrackIndex) : FString();
}


float FMediaPlayerFacade::GetVideoTrackAspectRatio(int32 TrackIndex, int32 FormatIndex) const
{
	FMediaVideoTrackFormat Format;
	return (GetVideoTrackFormat(TrackIndex, FormatIndex, Format) && (Format.Dim.Y != 0)) ? ((float)(Format.Dim.X) / Format.Dim.Y) : 0.0f;
}


FIntPoint FMediaPlayerFacade::GetVideoTrackDimensions(int32 TrackIndex, int32 FormatIndex) const
{
	FMediaVideoTrackFormat Format;
	return GetVideoTrackFormat(TrackIndex, FormatIndex, Format) ? Format.Dim : FIntPoint::ZeroValue;
}


float FMediaPlayerFacade::GetVideoTrackFrameRate(int32 TrackIndex, int32 FormatIndex) const
{
	FMediaVideoTrackFormat Format;
	return GetVideoTrackFormat(TrackIndex, FormatIndex, Format) ? Format.FrameRate : 0.0f;
}


TRange<float> FMediaPlayerFacade::GetVideoTrackFrameRates(int32 TrackIndex, int32 FormatIndex) const
{
	FMediaVideoTrackFormat Format;
	return GetVideoTrackFormat(TrackIndex, FormatIndex, Format) ? Format.FrameRates : TRange<float>::Empty();
}


FString FMediaPlayerFacade::GetVideoTrackType(int32 TrackIndex, int32 FormatIndex) const
{
	FMediaVideoTrackFormat Format;
	return GetVideoTrackFormat(TrackIndex, FormatIndex, Format) ? Format.TypeName : FString();
}


bool FMediaPlayerFacade::GetViewField(float& OutHorizontal, float& OutVertical) const
{
	return Player.IsValid() ? Player->GetView().GetViewField(OutHorizontal, OutVertical) : false;
}


bool FMediaPlayerFacade::GetViewOrientation(FQuat& OutOrientation) const
{
	return Player.IsValid() ? Player->GetView().GetViewOrientation(OutOrientation) : false;
}


bool FMediaPlayerFacade::HasError() const
{
	return Player.IsValid() && (Player->GetControls().GetState() == EMediaState::Error);
}


bool FMediaPlayerFacade::IsBuffering() const
{
	return Player.IsValid() && EnumHasAnyFlags(Player->GetControls().GetStatus(), EMediaStatus::Buffering);
}


bool FMediaPlayerFacade::IsConnecting() const
{
	return Player.IsValid() && EnumHasAnyFlags(Player->GetControls().GetStatus(), EMediaStatus::Connecting);
}


bool FMediaPlayerFacade::IsLooping() const
{
	return Player.IsValid() && Player->GetControls().IsLooping();
}


bool FMediaPlayerFacade::IsPaused() const
{
	return Player.IsValid() && (Player->GetControls().GetState() == EMediaState::Paused);
}


bool FMediaPlayerFacade::IsPlaying() const
{
	return Player.IsValid() && (Player->GetControls().GetState() == EMediaState::Playing);
}


bool FMediaPlayerFacade::IsPreparing() const
{
	return Player.IsValid() && (Player->GetControls().GetState() == EMediaState::Preparing);
}

bool FMediaPlayerFacade::IsClosed() const
{
	return Player.IsValid() && (Player->GetControls().GetState() == EMediaState::Closed);
}

bool FMediaPlayerFacade::IsReady() const
{
	if (!Player.IsValid())
	{
		return false;
	}

	return ((Player->GetControls().GetState() != EMediaState::Closed) &&
			(Player->GetControls().GetState() != EMediaState::Error) &&
			(Player->GetControls().GetState() != EMediaState::Preparing));
}


bool FMediaPlayerFacade::Open(const FString& Url, const IMediaOptions* Options, const FMediaPlayerOptions* PlayerOptions)
{
	SCOPE_CYCLE_COUNTER(STAT_MediaUtils_FacadeOpen);

	ActivePlayerOptions.Reset();

	if (IsRunningDedicatedServer())
	{
		return false;
	}

	Close();

	if (Url.IsEmpty())
	{
		return false;
	}

	// find & initialize new player
	TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> NewPlayer = GetPlayerForUrl(Url, Options);

	if (NewPlayer != Player)
	{
		FScopeLock Lock(&CriticalSection);
		Player = NewPlayer;
	}

	if (!Player.IsValid())
	{
		return false;
	}

	// update the Guid
	Player->SetGuid(PlayerGuid);

	CurrentUrl = Url;

	if (PlayerOptions)
	{
		ActivePlayerOptions = *PlayerOptions;
	}

	// open the new media source
	if (!Player->Open(Url, Options, PlayerOptions))
	{
		CurrentUrl.Empty();
		ActivePlayerOptions.Reset();

		return false;
	}

	return true;
}


void FMediaPlayerFacade::QueryCacheState(EMediaTrackType TrackType, EMediaCacheState State, TRangeSet<FTimespan>& OutTimeRanges) const
{
	if (!Player.IsValid())
	{
		return;
	}

	if (State == EMediaCacheState::Cached)
	{
		if (TrackType == EMediaTrackType::Audio)
		{
			Cache->GetCachedAudioSampleRanges(OutTimeRanges);
		}
		else if (TrackType == EMediaTrackType::Video)
		{
			Cache->GetCachedVideoSampleRanges(OutTimeRanges);
		}
	}
	else
	{
		if (TrackType == EMediaTrackType::Video)
		{
			Player->GetCache().QueryCacheState(State, OutTimeRanges);
		}
	}
}


bool FMediaPlayerFacade::Seek(const FTimespan& Time)
{
	if (!Player.IsValid() || !Player->GetControls().Seek(Time))
	{
		return false;
	}

	if (Player.IsValid() && Player->FlushOnSeekStarted())
	{
		FlushSinks();
	}

	return true;
}


bool FMediaPlayerFacade::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	if (!Player.IsValid() || !Player->GetTracks().SelectTrack((EMediaTrackType)TrackType, TrackIndex))
	{
		return false;
	}
	
	FlushSinks();

	return true;
}


void FMediaPlayerFacade::SetBlockOnTime(const FTimespan& Time)
{
	BlockOnTime = Time;
}


void FMediaPlayerFacade::SetCacheWindow(FTimespan Ahead, FTimespan Behind)
{
	Cache->SetCacheWindow(Ahead, Behind);
}


void FMediaPlayerFacade::SetGuid(FGuid& Guid)
{
	PlayerGuid = Guid;
}


bool FMediaPlayerFacade::SetLooping(bool Looping)
{
	return Player.IsValid() && Player->GetControls().SetLooping(Looping);
}


bool FMediaPlayerFacade::SetRate(float Rate)
{
	if (!Player.IsValid() || !Player->GetControls().SetRate(Rate))
	{
		return false;
	}

	if ((LastRate * Rate) < 0.0f)
	{
		FlushSinks(); // direction change
	}

	return true;
}


bool FMediaPlayerFacade::SetNativeVolume(float Volume)
{
	if (!Player.IsValid())
	{
		return false;
	}

	return Player->SetNativeVolume(Volume);
}


bool FMediaPlayerFacade::SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex)
{
	return Player.IsValid() ? Player->GetTracks().SetTrackFormat((EMediaTrackType)TrackType, TrackIndex, FormatIndex) : false;
}


bool FMediaPlayerFacade::SetVideoTrackFrameRate(int32 TrackIndex, int32 FormatIndex, float FrameRate)
{
	return Player.IsValid() ? Player->GetTracks().SetVideoTrackFrameRate(TrackIndex, FormatIndex, FrameRate) : false;
}


bool FMediaPlayerFacade::SetViewField(float Horizontal, float Vertical, bool Absolute)
{
	return Player.IsValid() && Player->GetView().SetViewField(Horizontal, Vertical, Absolute);
}


bool FMediaPlayerFacade::SetViewOrientation(const FQuat& Orientation, bool Absolute)
{
	return Player.IsValid() && Player->GetView().SetViewOrientation(Orientation, Absolute);
}


bool FMediaPlayerFacade::SupportsRate(float Rate, bool Unthinned) const
{
	EMediaRateThinning Thinning = Unthinned ? EMediaRateThinning::Unthinned : EMediaRateThinning::Thinned;
	return Player.IsValid() && Player->GetControls().GetSupportedRates(Thinning).Contains(Rate);
}

void FMediaPlayerFacade::SetLastAudioRenderedSampleTime(FTimespan SampleTime)
{
	LastAudioRenderedSampleTime = SampleTime;
}

FTimespan FMediaPlayerFacade::GetLastAudioRenderedSampleTime() const
{
	return LastAudioRenderedSampleTime.Load();
}

FTimespan FMediaPlayerFacade::GetLastAudioSampleProcessedTime() const
{
	return LastAudioSampleProcessedTime.Load();
}

FTimespan FMediaPlayerFacade::GetLastVideoSampleProcessedTime() const
{
	return LastVideoSampleProcessedTime.Load();
}

/* FMediaPlayerFacade implementation
*****************************************************************************/

bool FMediaPlayerFacade::BlockOnFetch() const
{
#if MEDIAPLAYERFACADE_DISABLE_BLOCKING
	return false;
#endif

	check(Player.IsValid());

	if (BlockOnTime == FTimespan::MinValue())
	{
		return false; // no blocking requested
	}

	if (!Player->GetControls().CanControl(EMediaControl::BlockOnFetch))
	{
		return false; // not supported by player plug-in
	}

	if (IsPreparing())
	{
		return true; // block on media opening
	}

	if (!IsPlaying() || (GetRate() < 0.0f))
	{
		return false; // block only in forward play
	}

	const bool VideoReady = (VideoSampleSinks.Num() == 0) || (BlockOnTime < NextVideoSampleTime);

	if (VideoReady)
	{
		return false; // video is ready
	}

	return true;
}


void FMediaPlayerFacade::FlushSinks()
{
	UE_LOG(LogMediaUtils, Verbose, TEXT("PlayerFacade %p: Flushing sinks"), this);

	FScopeLock Lock(&CriticalSection);

	AudioSampleSinks.Flush();
	CaptionSampleSinks.Flush();
	MetadataSampleSinks.Flush();
	SubtitleSampleSinks.Flush();
	VideoSampleSinks.Flush();

	if (Player.IsValid())
	{
		Player->GetSamples().FlushSamples();
	}

	NextVideoSampleTime = FTimespan::MinValue();

	LastAudioSampleProcessedTime = FTimespan::MinValue();
	LastVideoSampleProcessedTime = FTimespan::MinValue();
	LastAudioRenderedSampleTime = FTimespan::MinValue();
}


bool FMediaPlayerFacade::GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const
{
	if (TrackIndex == INDEX_NONE)
	{
		TrackIndex = GetSelectedTrack(EMediaTrackType::Audio);
	}

	if (FormatIndex == INDEX_NONE)
	{
		FormatIndex = GetTrackFormat(EMediaTrackType::Audio, TrackIndex);
	}

	return (Player.IsValid() && Player->GetTracks().GetAudioTrackFormat(TrackIndex, FormatIndex, OutFormat));
}


TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> FMediaPlayerFacade::GetPlayerForUrl(const FString& Url, const IMediaOptions* Options)
{
	FName PlayerName;
	
	if (DesiredPlayerName != NAME_None)
	{
		PlayerName = DesiredPlayerName;
	}
	else if (Options != nullptr)
	{
		PlayerName = Options->GetDesiredPlayerName();
	}
	else
	{
		PlayerName = NAME_None;
	}

	// reuse existing player if requested
	if (Player.IsValid() && (PlayerName == Player->GetPlayerName()))
	{
		return Player;
	}

	// load media module
	IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

	if (MediaModule == nullptr)
	{
		UE_LOG(LogMediaUtils, Error, TEXT("Failed to load Media module"));
		return nullptr;
	}

	// try to create requested player
	if (PlayerName != NAME_None)
	{
		IMediaPlayerFactory* Factory = MediaModule->GetPlayerFactory(PlayerName);

		if (Factory == nullptr)
		{
			UE_LOG(LogMediaUtils, Error, TEXT("Could not find desired player %s for %s"), *PlayerName.ToString(), *Url);
			return nullptr;
		}

		TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> NewPlayer = Factory->CreatePlayer(*this);

		if (!NewPlayer.IsValid())
		{
			UE_LOG(LogMediaUtils, Error, TEXT("Failed to create desired player %s for %s"), *PlayerName.ToString(), *Url);
			return nullptr;
		}

		return NewPlayer;
	}

	// try to reuse existing player
	if (Player.IsValid())
	{
		IMediaPlayerFactory* Factory = MediaModule->GetPlayerFactory(Player->GetPlayerName());

		if ((Factory != nullptr) && Factory->CanPlayUrl(Url, Options))
		{
			return Player;
		}
	}

	const FString RunningPlatformName(FPlatformProperties::IniPlatformName());

	// try to auto-select new player
	const TArray<IMediaPlayerFactory*>& PlayerFactories = MediaModule->GetPlayerFactories();

	for (IMediaPlayerFactory* Factory : PlayerFactories)
	{
		if (!Factory->SupportsPlatform(RunningPlatformName) || !Factory->CanPlayUrl(Url, Options))
		{
			continue;
		}

		TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> NewPlayer = Factory->CreatePlayer(*this);

		if (NewPlayer.IsValid())
		{
			return NewPlayer;
		}
	}

	// no suitable player found
	if (PlayerFactories.Num() > 0)
	{
		UE_LOG(LogMediaUtils, Error, TEXT("Cannot play %s, because none of the enabled media player plug-ins support it:"), *Url);

		for (IMediaPlayerFactory* Factory : PlayerFactories)
		{
			if (Factory->SupportsPlatform(RunningPlatformName))
			{
				UE_LOG(LogMediaUtils, Log, TEXT("| %s (URI scheme or file extension not supported)"), *Factory->GetPlayerName().ToString());
			}
			else
			{
				UE_LOG(LogMediaUtils, Log, TEXT("| %s (only available on %s, but not on %s)"), *Factory->GetPlayerName().ToString(), *FString::Join(Factory->GetSupportedPlatforms(), TEXT(", ")), *RunningPlatformName);
			}	
		}
	}
	else
	{
		UE_LOG(LogMediaUtils, Error, TEXT("Cannot play %s: no media player plug-ins are installed and enabled in this project"), *Url);
	}

	return nullptr;
}


bool FMediaPlayerFacade::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const
{
	if (TrackIndex == INDEX_NONE)
	{
		TrackIndex = GetSelectedTrack(EMediaTrackType::Video);
	}

	if (FormatIndex == INDEX_NONE)
	{
		FormatIndex = GetTrackFormat(EMediaTrackType::Video, TrackIndex);
	}

	return (Player.IsValid() && Player->GetTracks().GetVideoTrackFormat(TrackIndex, FormatIndex, OutFormat));
}


void FMediaPlayerFacade::ProcessEvent(EMediaEvent Event)
{
	SCOPE_CYCLE_COUNTER(STAT_MediaUtils_FacadeProcessEvent);

	if (Event == EMediaEvent::TracksChanged)
	{
		SelectDefaultTracks();
	}
	else if ((Event == EMediaEvent::MediaOpened) || (Event == EMediaEvent::MediaOpenFailed))
	{
		if (Event == EMediaEvent::MediaOpenFailed)
		{
			CurrentUrl.Empty();
		}

		const FString MediaInfo = Player->GetInfo();

		if (MediaInfo.IsEmpty())
		{
			UE_LOG(LogMediaUtils, Verbose, TEXT("PlayerFacade %p: Media Info: n/a"), this);
		}
		else
		{
			UE_LOG(LogMediaUtils, Verbose, TEXT("PlayerFacade %p: Media Info:\n%s"), this, *MediaInfo);
		}
	}

	if ((Event == EMediaEvent::PlaybackEndReached) ||
		(Event == EMediaEvent::TracksChanged))
	{
		FlushSinks();
	}
	else if (Event == EMediaEvent::SeekCompleted)
	{
		if (!Player.IsValid() || Player->FlushOnSeekCompleted())
		{
			FlushSinks();
		}
	}

	MediaEvent.Broadcast(Event);
}


void FMediaPlayerFacade::SelectDefaultTracks()
{
	if (!Player.IsValid())
	{
		return;
	}

	IMediaTracks& Tracks = Player->GetTracks();

	// @todo gmp: consider locale when selecting default media tracks

	FMediaPlayerTrackOptions TrackOptions;
	if (ActivePlayerOptions.IsSet())
	{
		TrackOptions = ActivePlayerOptions.GetValue().Tracks;
	}

	Tracks.SelectTrack(EMediaTrackType::Audio, TrackOptions.Audio);
	Tracks.SelectTrack(EMediaTrackType::Caption, TrackOptions.Caption);
	Tracks.SelectTrack(EMediaTrackType::Metadata, TrackOptions.Metadata);
	Tracks.SelectTrack(EMediaTrackType::Subtitle, TrackOptions.Subtitle);
	Tracks.SelectTrack(EMediaTrackType::Video, TrackOptions.Video);
}


/* IMediaClockSink interface
*****************************************************************************/

void FMediaPlayerFacade::TickFetch(FTimespan DeltaTime, FTimespan Timecode)
{
	SCOPE_CYCLE_COUNTER(STAT_MediaUtils_FacadeTickFetch);

	// let the player generate samples & process events
	if (Player.IsValid())
	{
		Player->TickFetch(DeltaTime, Timecode);
	}

	// process deferred events
	EMediaEvent Event;

	while (QueuedEvents.Dequeue(Event))
	{
		ProcessEvent(Event);
	}

	if (!Player.IsValid())
	{
		return;
	}

	// get current play rate
	float Rate = Player->GetControls().GetRate();

	if (Rate == 0.0f)
	{
		Rate = LastRate;
	}
	else
	{
		LastRate = Rate;
	}

	// determine range of valid samples
	TRange<FTimespan> TimeRange;

	const FTimespan CurrentTime = GetTime();

	SET_FLOAT_STAT(STAT_MediaUtils_FacadeTime, CurrentTime.GetTotalMilliseconds());

	if (Rate > 0.0f)
	{
		TimeRange = TRange<FTimespan>::AtMost(CurrentTime);
	}
	else if (Rate < 0.0f)
	{
		TimeRange = TRange<FTimespan>::AtLeast(CurrentTime);
	}
	else
	{
		TimeRange = TRange<FTimespan>(CurrentTime);
	}

	// process samples in range
	IMediaSamples& Samples = Player->GetSamples();

	bool Blocked = false;
	FDateTime BlockedTime;

	while (true)
	{
		ProcessCaptionSamples(Samples, TimeRange);
		ProcessSubtitleSamples(Samples, TimeRange);
		ProcessVideoSamples(Samples, TimeRange);

		if (!BlockOnFetch())
		{
			break;
		}

		if (Blocked)
		{
			if ((FDateTime::UtcNow() - BlockedTime) >= FTimespan::FromSeconds(MEDIAUTILS_MAX_BLOCKONFETCH_SECONDS))
			{
				UE_LOG(LogMediaUtils, Verbose, TEXT("PlayerFacade %p: Aborted block on fetch %s after %i seconds"),
					this,
					*BlockOnTime.ToString(TEXT("%h:%m:%s.%t")),
					MEDIAUTILS_MAX_BLOCKONFETCH_SECONDS
				);

				break;
			}
		}
		else
		{
			UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Blocking on fetch %s"), this, *BlockOnTime.ToString(TEXT("%h:%m:%s.%t")));

			Blocked = true;
			BlockedTime = FDateTime::UtcNow();
		}

		FPlatformProcess::Sleep(0.0f);
	}
}


void FMediaPlayerFacade::TickInput(FTimespan DeltaTime, FTimespan Timecode)
{
	SCOPE_CYCLE_COUNTER(STAT_MediaUtils_FacadeTickInput);

	if (Player.IsValid())
	{
		Player->TickInput(DeltaTime, Timecode);
	}
}


void FMediaPlayerFacade::TickOutput(FTimespan DeltaTime, FTimespan /*Timecode*/)
{
	SCOPE_CYCLE_COUNTER(STAT_MediaUtils_FacadeTickOutput);

	if (!Player.IsValid())
	{
		return;
	}

	IMediaControls& Controls = Player->GetControls();
	Cache->Tick(DeltaTime, Controls.GetRate(), GetTime());
}


/* IMediaTickable interface
*****************************************************************************/

void FMediaPlayerFacade::TickTickable()
{
	SCOPE_CYCLE_COUNTER(STAT_MediaUtils_FacadeTickTickable);

	if (LastRate == 0.0f)
	{
		return;
	}

	FScopeLock Lock(&CriticalSection);

	if (!Player.IsValid())
	{
		return;
	}

	Player->SetLastAudioRenderedSampleTime(LastAudioRenderedSampleTime.Load());

	Player->TickAudio();

	// determine range of valid samples
	TRange<FTimespan> AudioTimeRange;
	TRange<FTimespan> MetadataTimeRange;

	const FTimespan Time = GetTime();

	if (LastRate > 0.0f)
	{
		AudioTimeRange = TRange<FTimespan>::Inclusive(FTimespan::MinValue(), Time + MediaPlayerFacade::AudioPreroll);
		MetadataTimeRange = TRange<FTimespan>::Inclusive(FTimespan::MinValue(), Time + MediaPlayerFacade::MetadataPreroll);
	}
	else
	{
		AudioTimeRange = TRange<FTimespan>::Inclusive(Time - MediaPlayerFacade::AudioPreroll, FTimespan::MaxValue());
		MetadataTimeRange = TRange<FTimespan>::Inclusive(Time - MediaPlayerFacade::MetadataPreroll, FTimespan::MaxValue());
	}

	// process samples in range
	IMediaSamples& Samples = Player->GetSamples();
	
	ProcessAudioSamples(Samples, AudioTimeRange);
	ProcessMetadataSamples(Samples, MetadataTimeRange);	
}

/* FMediaPlayerFacade implementation
*****************************************************************************/

void FMediaPlayerFacade::ProcessAudioSamples(IMediaSamples& Samples, TRange<FTimespan> TimeRange)
{
	TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe> Sample;

	while (Samples.FetchAudio(TimeRange, Sample))
	{
		if (!Sample.IsValid())
		{
			continue;
		}

		LastAudioSampleProcessedTime = Sample->GetTime();

		if (!AudioSampleSinks.Enqueue(Sample.ToSharedRef(), FMediaPlayerQueueDepths::MaxAudioSinkDepth))
		{
			#if MEDIAPLAYERFACADE_TRACE_SINKOVERFLOWS
				UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Audio sample sink overflow"), this);
			#endif
		}
	}
}


void FMediaPlayerFacade::ProcessCaptionSamples(IMediaSamples& Samples, TRange<FTimespan> TimeRange)
{
	TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe> Sample;

	while (Samples.FetchCaption(TimeRange, Sample))
	{
		if (Sample.IsValid() && !CaptionSampleSinks.Enqueue(Sample.ToSharedRef(), FMediaPlayerQueueDepths::MaxCaptionSinkDepth))
		{
			#if MEDIAPLAYERFACADE_TRACE_SINKOVERFLOWS
				UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Caption sample sink overflow"), this);
			#endif
		}
	}
}


void FMediaPlayerFacade::ProcessMetadataSamples(IMediaSamples& Samples, TRange<FTimespan> TimeRange)
{
	TSharedPtr<IMediaBinarySample, ESPMode::ThreadSafe> Sample;

	while (Samples.FetchMetadata(TimeRange, Sample))
	{
		if (Sample.IsValid() && !MetadataSampleSinks.Enqueue(Sample.ToSharedRef(), FMediaPlayerQueueDepths::MaxMetadataSinkDepth))
		{
			#if MEDIAPLAYERFACADE_TRACE_SINKOVERFLOWS
				UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Metadata sample sink overflow"), this);
			#endif
		}
	}
}


void FMediaPlayerFacade::ProcessSubtitleSamples(IMediaSamples& Samples, TRange<FTimespan> TimeRange)
{
	TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe> Sample;

	while (Samples.FetchSubtitle(TimeRange, Sample))
	{
		if (Sample.IsValid() && !SubtitleSampleSinks.Enqueue(Sample.ToSharedRef(), FMediaPlayerQueueDepths::MaxSubtitleSinkDepth))
		{
			#if MEDIAPLAYERFACADE_TRACE_SINKOVERFLOWS
				UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Subtitle sample sink overflow"), this);
			#endif
		}
	}
}


void FMediaPlayerFacade::ProcessVideoSamples(IMediaSamples& Samples, TRange<FTimespan> TimeRange)
{
	TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> Sample;

	while (Samples.FetchVideo(TimeRange, Sample))
	{
		if (!Sample.IsValid())
		{
			continue;
		}

		LastVideoSampleProcessedTime = Sample->GetTime();

		UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Fetched video sample %s"), this, *Sample->GetTime().ToString(TEXT("%h:%m:%s.%t")));

		if (VideoSampleSinks.Enqueue(Sample.ToSharedRef(), FMediaPlayerQueueDepths::MaxVideoSinkDepth))
		{
			if (GetRate() >= 0.0f)
			{
				NextVideoSampleTime = Sample->GetTime() + Sample->GetDuration();
				UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Next video sample time %s"), this, *NextVideoSampleTime.ToString(TEXT("%h:%m:%s.%t")));
			}
		}
		else
		{
			#if MEDIAPLAYERFACADE_TRACE_SINKOVERFLOWS
				UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Video sample sink overflow"), this);
			#endif
		}
	}
}


/* IMediaEventSink interface
*****************************************************************************/

void FMediaPlayerFacade::ReceiveMediaEvent(EMediaEvent Event)
{
	UE_LOG(LogMediaUtils, VeryVerbose, TEXT("PlayerFacade %p: Received media event %s"), this, *MediaUtils::EventToString(Event));
	QueuedEvents.Enqueue(Event);
}
