// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieScene3DAttachSectionRecorder.h"
#include "Modules/ModuleManager.h"
#include "Sections/MovieScene3DAttachSection.h"
#include "Tracks/MovieScene3DAttachTrack.h"
#include "SequenceRecorderUtils.h"
#include "ISequenceRecorder.h"

TSharedPtr<IMovieSceneSectionRecorder> FMovieScene3DAttachSectionRecorderFactory::CreateSectionRecorder(const struct FActorRecordingSettings& InActorRecordingSettings) const
{
	return MakeShareable(new FMovieScene3DAttachSectionRecorder);
}

bool FMovieScene3DAttachSectionRecorderFactory::CanRecordObject(UObject* InObjectToRecord) const
{
	return InObjectToRecord->IsA<AActor>();
}

void FMovieScene3DAttachSectionRecorder::CreateSection(UObject* InObjectToRecord, UMovieScene* InMovieScene, const FGuid& Guid, float Time)
{
	ObjectGuid = Guid;
	ActorToRecord = CastChecked<AActor>(InObjectToRecord);
	MovieScene = InMovieScene;
	TimecodeSource = SequenceRecorderUtils::GetTimecodeSource();
}

void FMovieScene3DAttachSectionRecorder::FinalizeSection(float CurrentTime)
{
}

void FMovieScene3DAttachSectionRecorder::Record(float CurrentTime)
{
	if(ActorToRecord.IsValid())
	{
		if(MovieSceneSection.IsValid())
		{
			FFrameRate TickResolution = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
			FFrameNumber CurrentFrame = (CurrentTime * TickResolution).FloorToFrame();

			MovieSceneSection->ExpandToFrame(CurrentFrame);
		}

		// get attachment and check if the actor we are attached to is being recorded
		FName SocketName;
		FName ComponentName;
		AActor* AttachedToActor = SequenceRecorderUtils::GetAttachment(ActorToRecord.Get(), SocketName, ComponentName);

		ISequenceRecorder& SequenceRecorder = FModuleManager::GetModuleChecked<ISequenceRecorder>("SequenceRecorder");
		FGuid Guid = SequenceRecorder.GetRecordingGuid(AttachedToActor);
		if(AttachedToActor && Guid.IsValid())
		{
			// create the track if we haven't already
			if(!AttachTrack.IsValid())
			{
				AttachTrack = MovieScene->AddTrack<UMovieScene3DAttachTrack>(ObjectGuid);
			}

			// check if we need a section or if the actor we are attached to has changed
			if(!MovieSceneSection.IsValid() || AttachedToActor != ActorAttachedTo.Get())
			{
				MovieSceneSection = Cast<UMovieScene3DAttachSection>(AttachTrack->CreateNewSection());

				FFrameRate TickResolution = MovieSceneSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
				FFrameNumber CurrentFrame = (CurrentTime * TickResolution).FloorToFrame();

				MovieSceneSection->SetRange(TRange<FFrameNumber>::Inclusive(CurrentFrame, CurrentFrame));
				MovieSceneSection->SetConstraintId(Guid);
				MovieSceneSection->AttachSocketName = SocketName;
				MovieSceneSection->AttachComponentName = ComponentName;

				MovieSceneSection->TimecodeSource = TimecodeSource;
			}

			ActorAttachedTo = AttachedToActor;
		}
		else
		{
			// no attachment, so end the section recording if we have any
			MovieSceneSection = nullptr;
		}
	}
}
