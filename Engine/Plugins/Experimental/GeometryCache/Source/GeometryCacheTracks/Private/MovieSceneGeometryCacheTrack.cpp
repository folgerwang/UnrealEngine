// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneGeometryCacheTrack.h"
#include "GeometryCacheComponent.h"
#include "MovieSceneGeometryCacheSection.h"
#include "Compilation/MovieSceneCompilerRules.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "MovieSceneGeometryCacheTemplate.h"
#include "Compilation/IMovieSceneTemplateGenerator.h"
#include "MovieScene.h"

#define LOCTEXT_NAMESPACE "MovieSceneGeometryCacheTrack"


/* UMovieSceneGeometryCacheTrack structors
 *****************************************************************************/

UMovieSceneGeometryCacheTrack::UMovieSceneGeometryCacheTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(124, 15, 124, 65);
#endif

	SupportedBlendTypes.Add(EMovieSceneBlendType::Absolute);

	EvalOptions.bCanEvaluateNearestSection = true;
	EvalOptions.bEvaluateInPreroll = true;
}


/* UMovieSceneGeometryCacheTrack interface
 *****************************************************************************/

UMovieSceneSection* UMovieSceneGeometryCacheTrack::AddNewAnimation(FFrameNumber KeyTime, UGeometryCacheComponent* GeomCacheComp)
{
	UMovieSceneGeometryCacheSection* NewSection = Cast<UMovieSceneGeometryCacheSection>(CreateNewSection());
	{
		FFrameTime AnimationLength = GeomCacheComp->GetDuration()* GetTypedOuter<UMovieScene>()->GetTickResolution();
		NewSection->InitialPlacementOnRow(AnimationSections, KeyTime, AnimationLength.FrameNumber.Value, INDEX_NONE);
		
		NewSection->Params.GeometryCacheAsset = (GeomCacheComp->GetGeometryCache());
	}

	AddSection(*NewSection);

	return NewSection;
}


TArray<UMovieSceneSection*> UMovieSceneGeometryCacheTrack::GetAnimSectionsAtTime(FFrameNumber Time)
{
	TArray<UMovieSceneSection*> Sections;
	for (auto Section : AnimationSections)
	{
		if (Section->IsTimeWithinSection(Time))
		{
			Sections.Add(Section);
		}
	}

	return Sections;
}


/* UMovieSceneTrack interface
 *****************************************************************************/


const TArray<UMovieSceneSection*>& UMovieSceneGeometryCacheTrack::GetAllSections() const
{
	return AnimationSections;
}


bool UMovieSceneGeometryCacheTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneGeometryCacheSection::StaticClass();
}

UMovieSceneSection* UMovieSceneGeometryCacheTrack::CreateNewSection()
{
	return NewObject<UMovieSceneGeometryCacheSection>(this, NAME_None, RF_Transactional);
}


void UMovieSceneGeometryCacheTrack::RemoveAllAnimationData()
{
	AnimationSections.Empty();
}


bool UMovieSceneGeometryCacheTrack::HasSection(const UMovieSceneSection& Section) const
{
	return AnimationSections.Contains(&Section);
}


void UMovieSceneGeometryCacheTrack::AddSection(UMovieSceneSection& Section)
{
	AnimationSections.Add(&Section);
}


void UMovieSceneGeometryCacheTrack::RemoveSection(UMovieSceneSection& Section)
{
	AnimationSections.Remove(&Section);
}


bool UMovieSceneGeometryCacheTrack::IsEmpty() const
{
	return AnimationSections.Num() == 0;
}

#if WITH_EDITORONLY_DATA

FText UMovieSceneGeometryCacheTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "Geometry Cache");
}

#endif


#undef LOCTEXT_NAMESPACE
