// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sequencer/MovieSceneControlRigTrack.h"
#include "Sequencer/MovieSceneControlRigSection.h"
#include "Compilation/MovieSceneCompilerRules.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Sequencer/ControlRigSequence.h"
#include "MovieScene.h"
#include "Sequencer/ControlRigBindingTrack.h"
#include "MovieSceneTimeHelpers.h"

#define LOCTEXT_NAMESPACE "MovieSceneControlRigTrack"

UMovieSceneControlRigTrack::UMovieSceneControlRigTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(108, 53, 0, 65);
#endif

	EvalOptions.bEvaluateNearestSection_DEPRECATED = EvalOptions.bCanEvaluateNearestSection = true;
}

void UMovieSceneControlRigTrack::AddNewControlRig(FFrameNumber KeyTime, UControlRigSequence* InSequence)
{
	UMovieSceneControlRigSection* NewSection = Cast<UMovieSceneControlRigSection>(CreateNewSection());
	{
		UMovieScene* OuterMovieScene = GetTypedOuter<UMovieScene>();
		UMovieScene* InnerMovieScene = InSequence->GetMovieScene();

		int32      InnerSequenceLength = MovieScene::DiscreteSize(InnerMovieScene->GetPlaybackRange());
		FFrameTime OuterSequenceLength = ConvertFrameTime(InnerSequenceLength, InnerMovieScene->GetTickResolution(), OuterMovieScene->GetTickResolution());

		NewSection->InitialPlacement(Sections, KeyTime, OuterSequenceLength.FrameNumber.Value, SupportsMultipleRows());
		NewSection->SetSequence(InSequence);
	}

	AddSection(*NewSection);
}

bool UMovieSceneControlRigTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneControlRigSection::StaticClass();
}

UMovieSceneSection* UMovieSceneControlRigTrack::CreateNewSection()
{
	return NewObject<UMovieSceneControlRigSection>(this, NAME_None, RF_Transactional);
}

#if WITH_EDITORONLY_DATA

FText UMovieSceneControlRigTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "ControlRig");
}

#endif

#undef LOCTEXT_NAMESPACE
