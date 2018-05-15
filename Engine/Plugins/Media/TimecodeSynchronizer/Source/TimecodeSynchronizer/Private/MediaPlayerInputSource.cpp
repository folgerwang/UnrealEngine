// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerInputSource.h"
#include "TimecodeSynchronizerModule.h"

#include "IMediaCache.h"
#include "IMediaPlayer.h"
#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"
#include "MediaTexture.h"
#include "TimeSynchronizableMediaSource.h"


#if WITH_EDITOR
void UMediaPlayerInputSource::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMediaPlayerInputSource, MediaSource))
	{
		if (bUseForSynchronization && MediaSource && !MediaSource->bUseTimeSynchronization)
		{
			// Warn the use that the MediaSource he just added isn't set for using time synchronization
			UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("MediaSource %s doesn't have the option to use time synchronization enabled."), *MediaSource->GetName());
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

FFrameTime UMediaPlayerInputSource::GetNextSampleTime() const
{
	FFrameTime NextSampleTime;
	const auto& Player = MediaPlayer->GetPlayerFacade()->GetPlayer();
	if (Player.IsValid())
	{
		//If there is a sample in the Texture, we consider it as the next one to be used/rendered
		if (MediaTexture->GetAvailableSampleCount() > 0)
		{
			const FTimespan TextureTime = MediaTexture->GetNextSampleTime();
			NextSampleTime = FFrameTime::FromDecimal(TextureTime.GetTotalSeconds() * PlayerFrameRate.AsDecimal()).RoundToFrame();
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
				
				//todo : Next time sequencer is integrated to main, get changes to TRangeSet to be able to use this code. (CL 3967195) 
				//const FTimespan& MinSampleTime = SampleTimes.GetMinBoundValue();
			
				NextSampleTime = FFrameTime::FromDecimal(MinSampleTime.GetTotalSeconds() * PlayerFrameRate.AsDecimal()).RoundToFrame();
			}
		}
	}

	return NextSampleTime;
}

int32 UMediaPlayerInputSource::GetAvailableSampleCount() const
{
	int32 AvailableSampleCount = 0;
	const auto& Player = MediaPlayer->GetPlayerFacade()->GetPlayer();
	if (Player.IsValid())
	{
		const int32 TextureSampleCount = MediaTexture->GetAvailableSampleCount();
		const int32 PlayerSampleCount = Player->GetCache().GetSampleCount(EMediaCacheState::Loaded);
		AvailableSampleCount = TextureSampleCount + PlayerSampleCount;
	}

	return AvailableSampleCount;
}

FFrameRate UMediaPlayerInputSource::GetFrameRate() const
{
	return PlayerFrameRate;
}

bool UMediaPlayerInputSource::IsReady() const
{
	return MediaPlayer && MediaPlayer->IsReady() && MediaSource && MediaTexture;
}

bool UMediaPlayerInputSource::Open()
{
	bool bResult = false;
	if (MediaPlayer && MediaSource && MediaTexture)
	{
		if (bUseForSynchronization && !MediaSource->bUseTimeSynchronization)
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("MediaSource %s doesn't support timecode synchronization"), *MediaSource->GetName());
		}
		else
		{
			bResult = MediaPlayer->OpenSource(MediaSource);
			if (!bResult)
			{
				UE_LOG(LogTimecodeSynchronizer, Error, TEXT("Player could not be started."));
			}
			else
			{
				const auto& Player = MediaPlayer->GetPlayerFacade()->GetPlayer();
				if (Player.IsValid())
				{
					//Save the FrameRate of the current track of the media player for future use
					const int32 SelectedTrack = MediaPlayer->GetSelectedTrack(EMediaPlayerTrack::Video);
					const int32 SelectedFormat = MediaPlayer->GetTrackFormat(EMediaPlayerTrack::Video, SelectedTrack);
					const float FrameRate = MediaPlayer->GetVideoTrackFrameRate(SelectedTrack, SelectedFormat);

					//Convert using 1001 for DropFrame FrameRate detection
					const uint32 Precision = 1001;
					PlayerFrameRate = FFrameRate(FMath::RoundToInt(FrameRate * Precision), Precision);
				}
				else
				{
					UE_LOG(LogTimecodeSynchronizer, Error, TEXT("Current player is invalid."));
					MediaPlayer->Close();
				}
			}
		}
	}

	return bResult;
}

void UMediaPlayerInputSource::Start()
{
	//Once we're on the verge of playing the source, it's time to setup the delay
	if (!bUseForSynchronization)
	{
		MediaPlayer->SetTimeDelay(FTimespan::FromSeconds(TimeDelay));
	}

	MediaPlayer->Play();
}

void UMediaPlayerInputSource::Close()
{
	if (MediaPlayer && MediaSource && MediaTexture)
	{
		MediaPlayer->Close();
	}
}

FString UMediaPlayerInputSource::GetDisplayName() const
{
	return MediaPlayer ? *MediaPlayer->GetName() : FString();
}


