// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneCameraShakeTrack.h"
#include "Sections/MovieSceneCameraShakeSection.h"
#include "Evaluation/PersistentEvaluationData.h"
#include "Evaluation/MovieSceneCameraAnimTemplate.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Evaluation/MovieSceneEvaluationTemplate.h"
#include "Compilation/MovieSceneSegmentCompiler.h"
#include "Compilation/MovieSceneCompilerRules.h"
#include "MovieScene.h"

#define LOCTEXT_NAMESPACE "MovieSceneCameraShakeTrack"

UMovieSceneSection* UMovieSceneCameraShakeTrack::AddNewCameraShake(FFrameNumber KeyTime, TSubclassOf<UCameraShake> ShakeClass)
{
	UMovieSceneCameraShakeSection* const NewSection = Cast<UMovieSceneCameraShakeSection>(CreateNewSection());
	if (NewSection)
	{
		// #fixme get length
		FFrameTime Duration = 5.0 * GetTypedOuter<UMovieScene>()->GetTickResolution();
		NewSection->InitialPlacement(CameraShakeSections, KeyTime, Duration.FrameNumber.Value, SupportsMultipleRows());
		NewSection->ShakeData.ShakeClass = ShakeClass;
		
		AddSection(*NewSection);
	}

	return NewSection;
}

FMovieSceneTrackSegmentBlenderPtr UMovieSceneCameraShakeTrack::GetTrackSegmentBlender() const
{
	return FMovieSceneAdditiveCameraTrackBlender();
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneCameraShakeTrack::GetDisplayName() const
{
	return LOCTEXT("TrackName", "Camera Shake");
}
#endif




/* UMovieSceneTrack interface
*****************************************************************************/


const TArray<UMovieSceneSection*>& UMovieSceneCameraShakeTrack::GetAllSections() const
{
	return CameraShakeSections;
}


UMovieSceneSection* UMovieSceneCameraShakeTrack::CreateNewSection()
{
	return NewObject<UMovieSceneCameraShakeSection>(this, NAME_None, RF_Transactional);
}


void UMovieSceneCameraShakeTrack::RemoveAllAnimationData()
{
	CameraShakeSections.Empty();
}


bool UMovieSceneCameraShakeTrack::HasSection(const UMovieSceneSection& Section) const
{
	return CameraShakeSections.Contains(&Section);
}


void UMovieSceneCameraShakeTrack::AddSection(UMovieSceneSection& Section)
{
	CameraShakeSections.Add(&Section);
}


void UMovieSceneCameraShakeTrack::RemoveSection(UMovieSceneSection& Section)
{
	CameraShakeSections.Remove(&Section);
}


bool UMovieSceneCameraShakeTrack::IsEmpty() const
{
	return CameraShakeSections.Num() == 0;
}


#undef LOCTEXT_NAMESPACE
