// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaBundleTimeSynchronizationSource.h"

#include "IMediaCache.h"
#include "IMediaPlayer.h"
#include "MediaBundle.h"
#include "MediaFrameworkUtilitiesModule.h"
#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"
#include "MediaTexture.h"
#include "TimeSynchronizableMediaSource.h"


#if WITH_EDITOR
void UMediaBundleTimeSynchronizationSource::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMediaBundleTimeSynchronizationSource, MediaBundle))
	{
		if (bUseForSynchronization && MediaBundle && MediaBundle->GetMediaSource() )
		{
			UTimeSynchronizableMediaSource* SynchronizableMediaSource = Cast<UTimeSynchronizableMediaSource>(MediaBundle->GetMediaSource());
			if (SynchronizableMediaSource == nullptr || SynchronizableMediaSource->bUseTimeSynchronization)
			{
				// Warn the user that the MediaSource he just added isn't set for using time synchronization
				UE_LOG(LogMediaFrameworkUtilities, Warning, TEXT("MediaSource %s doesn't have the option to use time synchronization enabled."), *MediaBundle->MediaSource->GetName());
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

FFrameTime UMediaBundleTimeSynchronizationSource::GetNextSampleTime() const
{
	FFrameTime NextSampleTime;

	if (MediaBundle && MediaBundle->GetMediaPlayer() && MediaBundle->GetMediaTexture())
	{
		const TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe>& Player = MediaBundle->GetMediaPlayer()->GetPlayerFacade()->GetPlayer();
		if (Player.IsValid())
		{
			//If there is a sample in the Texture, we consider it as the next one to be used/rendered
			if (MediaBundle->GetMediaTexture()->GetAvailableSampleCount() > 0)
			{
				const FTimespan TextureTime = MediaBundle->GetMediaTexture()->GetNextSampleTime();
				NextSampleTime = FFrameTime::FromDecimal(TextureTime.GetTotalSeconds() * GetFrameRate().AsDecimal()).RoundToFrame();
			}
			else if (Player->GetCache().GetSampleCount(EMediaCacheState::Loaded) > 0)
			{
				TRangeSet<FTimespan> SampleTimes;
				if (Player->GetCache().QueryCacheState(EMediaCacheState::Loaded, SampleTimes))
				{
					//Fetch the minimum sample time from all ranges queried from the player's cache
					TArray<TRange<FTimespan>> Ranges;
					SampleTimes.GetRanges(Ranges);
					check(Ranges.Num() > 0);

					TRangeBound<FTimespan> MinBound = Ranges[0].GetLowerBound();
					for (const auto& Range : Ranges)
					{
						MinBound = TRangeBound<FTimespan>::MinLower(MinBound, Range.GetLowerBound());
					}
					const FTimespan MinSampleTime = MinBound.GetValue();
			
					NextSampleTime = FFrameTime::FromDecimal(MinSampleTime.GetTotalSeconds() * GetFrameRate().AsDecimal()).RoundToFrame();
				}
			}
		}
	}

	return NextSampleTime;
}

int32 UMediaBundleTimeSynchronizationSource::GetAvailableSampleCount() const
{
	int32 AvailableSampleCount = 0;

	if (MediaBundle && MediaBundle->GetMediaPlayer() && MediaBundle->GetMediaTexture())
	{
		const TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe>& Player = MediaBundle->GetMediaPlayer()->GetPlayerFacade()->GetPlayer();
		if (Player.IsValid())
		{
			const int32 TextureSampleCount = MediaBundle->GetMediaTexture()->GetAvailableSampleCount();
			const int32 PlayerSampleCount = Player->GetCache().GetSampleCount(EMediaCacheState::Loaded);
			AvailableSampleCount = TextureSampleCount + PlayerSampleCount;
		}
	}

	return AvailableSampleCount;
}

FFrameRate UMediaBundleTimeSynchronizationSource::GetFrameRate() const
{
	if (IsReady())
	{
		UMediaPlayer* MediaPlayer = MediaBundle->GetMediaPlayer();
		check(MediaPlayer);
		const TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe>& Player = MediaPlayer->GetPlayerFacade()->GetPlayer();

		if (Player.IsValid())
		{
			//Save the FrameRate of the current track of the media player for future use
			const int32 SelectedTrack = MediaPlayer->GetSelectedTrack(EMediaPlayerTrack::Video);
			const int32 SelectedFormat = MediaPlayer->GetTrackFormat(EMediaPlayerTrack::Video, SelectedTrack);
			const float FrameRate = MediaPlayer->GetVideoTrackFrameRate(SelectedTrack, SelectedFormat);

			//Convert using 1001 for DropFrame FrameRate detection
			const uint32 Precision = 1001;
			return FFrameRate(FMath::RoundToInt(FrameRate * Precision), Precision);
		}
	}
	return FFrameRate();
}

bool UMediaBundleTimeSynchronizationSource::IsReady() const
{
	return MediaBundle && MediaBundle->GetMediaPlayer() && MediaBundle->GetMediaPlayer()->IsReady() && MediaBundle->GetMediaSource() && MediaBundle->GetMediaTexture();
}

bool UMediaBundleTimeSynchronizationSource::Open()
{
	bool bResult = false;
	if (MediaBundle)
	{
		UMediaPlayer* MediaPlayer = MediaBundle->GetMediaPlayer();
		if (MediaPlayer)
		{
			UTimeSynchronizableMediaSource* SynchronizableMediaSource = Cast<UTimeSynchronizableMediaSource>(MediaBundle->GetMediaSource());
			if (bUseForSynchronization && (SynchronizableMediaSource == nullptr || !SynchronizableMediaSource->bUseTimeSynchronization))
			{
				UE_LOG(LogMediaFrameworkUtilities, Error, TEXT("MediaBundle %s doesn't support timecode synchronization"), *MediaBundle->GetName());
			}
			else
			{
				bResult = MediaBundle->OpenMediaSource();
				if (!bResult)
				{
					UE_LOG(LogMediaFrameworkUtilities, Error, TEXT("MediaBundle '%s' could not be started."), *MediaBundle->GetName());
				}
				else
				{
					const auto& Player = MediaPlayer->GetPlayerFacade()->GetPlayer();
					if (!Player.IsValid())
					{
						UE_LOG(LogMediaFrameworkUtilities, Error, TEXT("Player, for MediaBundle '%s', is invalid."), *MediaBundle->GetName());
						MediaBundle->CloseMediaSource();
					}
				}
			}
		}
	}

	return bResult;
}

void UMediaBundleTimeSynchronizationSource::Start()
{
	UMediaPlayer* MediaPlayer = MediaBundle ? MediaBundle->GetMediaPlayer() : nullptr;
	if (MediaPlayer)
	{
		//Once we're on the verge of playing the source, it's time to setup the delay
		if (!bUseForSynchronization)
		{
			MediaPlayer->SetTimeDelay(FTimespan::FromSeconds(TimeDelay));
		}
	}
}

void UMediaBundleTimeSynchronizationSource::Close()
{
	if (MediaBundle)
	{
		MediaBundle->CloseMediaSource();
	}
}

FString UMediaBundleTimeSynchronizationSource::GetDisplayName() const
{
	return MediaBundle ? *MediaBundle->GetName() : FString();
}
