// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneMediaData.h"

#include "IMediaEventSink.h"
#include "MediaPlayer.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"


/* FMediaSectionData structors
 *****************************************************************************/

FMovieSceneMediaData::FMovieSceneMediaData()
	: bOverrideMediaPlayer(false)
	, MediaPlayer(nullptr)
	, SeekOnOpenTime(FTimespan::MinValue())
{ }


FMovieSceneMediaData::~FMovieSceneMediaData()
{
	if (MediaPlayer != nullptr)
	{
		MediaPlayer->OnMediaEvent().RemoveAll(this);
		MediaPlayer->Close();
		MediaPlayer->RemoveFromRoot();
	}
}


/* FMediaSectionData interface
 *****************************************************************************/

UMediaPlayer* FMovieSceneMediaData::GetMediaPlayer()
{
	return MediaPlayer;
}


void FMovieSceneMediaData::SeekOnOpen(FTimespan Time)
{
	SeekOnOpenTime = Time;
}


void FMovieSceneMediaData::Setup(UMediaPlayer* OverrideMediaPlayer)
{
	// Ensure we don't already have a media player set. Setup should only be called once
	check(!MediaPlayer);

	if (OverrideMediaPlayer)
	{
		MediaPlayer = OverrideMediaPlayer;
		bOverrideMediaPlayer = true;
	}
	else if (MediaPlayer == nullptr)
	{
		MediaPlayer = NewObject<UMediaPlayer>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UMediaPlayer::StaticClass()));
	}

	MediaPlayer->PlayOnOpen = false;
	MediaPlayer->OnMediaEvent().AddRaw(this, &FMovieSceneMediaData::HandleMediaPlayerEvent);
	MediaPlayer->AddToRoot();
}


/* FMediaSectionData callbacks
 *****************************************************************************/

void FMovieSceneMediaData::HandleMediaPlayerEvent(EMediaEvent Event)
{
	if ((Event != EMediaEvent::MediaOpened) || (SeekOnOpenTime < FTimespan::Zero()))
	{
		return; // we only care about seek on open
	}

	if (!MediaPlayer->SupportsSeeking())
	{
		return; // media can't seek
	}

	const FTimespan MediaTime = SeekOnOpenTime % MediaPlayer->GetDuration();

	MediaPlayer->SetRate(0.0f);
	MediaPlayer->Seek(MediaTime);

	SeekOnOpenTime = FTimespan::MinValue();
}
