// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSpawnSectionRecorder.h"
#include "GameFramework/Actor.h"
#include "KeyParams.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Tracks/MovieSceneSpawnTrack.h"
#include "MovieScene.h"
#include "SequenceRecorderSettings.h"
#include "Channels/MovieSceneChannelProxy.h"

TSharedPtr<IMovieSceneSectionRecorder> FMovieSceneSpawnSectionRecorderFactory::CreateSectionRecorder(const struct FActorRecordingSettings& InActorRecordingSettings) const
{
	return MakeShareable(new FMovieSceneSpawnSectionRecorder);
}

bool FMovieSceneSpawnSectionRecorderFactory::CanRecordObject(UObject* InObjectToRecord) const
{
	return InObjectToRecord->IsA<AActor>();
}

void FMovieSceneSpawnSectionRecorder::CreateSection(UObject* InObjectToRecord, UMovieScene* MovieScene, const FGuid& Guid, float Time)
{
	ObjectToRecord = InObjectToRecord;

	UMovieSceneSpawnTrack* SpawnTrack = MovieScene->AddTrack<UMovieSceneSpawnTrack>(Guid);
	if(SpawnTrack)
	{
		MovieSceneSection = Cast<UMovieSceneBoolSection>(SpawnTrack->CreateNewSection());

		SpawnTrack->AddSection(*MovieSceneSection);
		SpawnTrack->SetObjectId(Guid);

		FMovieSceneBoolChannel* BoolChannel = MovieSceneSection->GetChannelProxy().GetChannel<FMovieSceneBoolChannel>(0);
		check(BoolChannel);
		BoolChannel->SetDefault(false);
		BoolChannel->GetInterface().AddKey(0, false);

		FFrameRate FrameResolution = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetFrameResolution();
		FFrameNumber CurrentFrame = (Time * FrameResolution).FloorToFrame();
		MovieSceneSection->SetRange(TRange<FFrameNumber>::Inclusive(CurrentFrame, CurrentFrame));
	}

	bWasSpawned = false;
}

void FMovieSceneSpawnSectionRecorder::FinalizeSection()
{
	const bool bSpawned = ObjectToRecord.IsValid();
	if(bSpawned != bWasSpawned)
	{
		FMovieSceneBoolChannel* Channel = MovieSceneSection->GetChannelProxy().GetChannel<FMovieSceneBoolChannel>(0);
		if (ensure(Channel) && MovieSceneSection->HasEndFrame())
		{
			Channel->GetInterface().AddKey(MovieSceneSection->GetExclusiveEndFrame()-1, bSpawned);
		}
	}

	// If the section is degenerate, assume the actor was spawned and destroyed. Give it a 1 frame spawn section.
	if (MovieSceneSection->GetRange().IsDegenerate() && MovieSceneSection->HasEndFrame())
	{
		FMovieSceneBoolChannel* Channel = MovieSceneSection->GetChannelProxy().GetChannel<FMovieSceneBoolChannel>(0);
		if (ensure(Channel))
		{
			double       OneFrameInterval = 1.0/GetDefault<USequenceRecorderSettings>()->DefaultAnimationSettings.SampleRate;

			FFrameRate   FrameResolution  = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetFrameResolution();
			FFrameNumber StartTime        = MovieSceneSection->GetExclusiveEndFrame() - (OneFrameInterval * FrameResolution).CeilToFrame();

			Channel->GetInterface().AddKey(StartTime, true);
			MovieSceneSection->SetStartFrame(StartTime);
		}
	}
}

void FMovieSceneSpawnSectionRecorder::Record(float CurrentTime)
{
	if(ObjectToRecord.IsValid())
	{
		FFrameRate FrameResolution = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetFrameResolution();
		MovieSceneSection->ExpandToFrame((CurrentTime * FrameResolution).FloorToFrame());
	}

	const bool bSpawned = ObjectToRecord.IsValid();
	if(bSpawned != bWasSpawned)
	{
		FMovieSceneBoolChannel* Channel = MovieSceneSection->GetChannelProxy().GetChannel<FMovieSceneBoolChannel>(0);
		if (ensure(Channel))
		{
			FFrameRate   FrameResolution = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetFrameResolution();
			FFrameNumber KeyTime         = (CurrentTime * FrameResolution).FloorToFrame();

			Channel->GetInterface().UpdateOrAddKey(KeyTime, bSpawned);
		}
	}
	bWasSpawned = bSpawned;
}
