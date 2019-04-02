// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieScene/MovieSceneNiagaraSystemTrack.h"
#include "CoreMinimal.h"
#include "MovieScene.h"
#include "MovieScene/MovieSceneNiagaraSystemSpawnSection.h"
#include "MovieScene/MovieSceneNiagaraSystemTrackTemplate.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Compilation/IMovieSceneTemplateGenerator.h"
#include "Compilation/MovieSceneSegmentCompiler.h"
#include "Compilation/MovieSceneCompilerRules.h"


bool UMovieSceneNiagaraSystemTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneNiagaraSystemSpawnSection::StaticClass();
}

UMovieSceneSection* UMovieSceneNiagaraSystemTrack::CreateNewSection()
{
	return NewObject<UMovieSceneNiagaraSystemSpawnSection>(this, NAME_None, RF_Transactional);
}

FMovieSceneEvalTemplatePtr UMovieSceneNiagaraSystemTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneEvalTemplatePtr();
}

void UMovieSceneNiagaraSystemTrack::PostCompile(FMovieSceneEvaluationTrack& OutTrack, const FMovieSceneTrackCompilerArgs& Args) const
{
	UMovieSceneSection* const* SpawnSectionPtr = Sections.FindByPredicate([](const UMovieSceneSection* Section) { return Section->GetClass() == UMovieSceneNiagaraSystemSpawnSection::StaticClass(); });
	if (SpawnSectionPtr != nullptr)
	{
		UMovieSceneNiagaraSystemSpawnSection* SpawnSection = CastChecked<UMovieSceneNiagaraSystemSpawnSection>(*SpawnSectionPtr);
		UMovieScene* ParentMovieScene = GetTypedOuter<UMovieScene>();
		OutTrack.SetTrackImplementation(FMovieSceneNiagaraSystemTrackImplementation(SpawnSection->GetInclusiveStartFrame(), SpawnSection->GetExclusiveEndFrame()));
	}
}

struct FNiagaraSystemTrackSegmentBlender : public FMovieSceneTrackSegmentBlender
{
	FNiagaraSystemTrackSegmentBlender()
	{
		bAllowEmptySegments = true;
		bCanFillEmptySpace = true;
	}

	virtual TOptional<FMovieSceneSegment> InsertEmptySpace(const TRange<FFrameNumber>& Range, const FMovieSceneSegment* PreviousSegment, const FMovieSceneSegment* NextSegment) const override
	{
		return MovieSceneSegmentCompiler::EvaluateNearestSegment(Range, PreviousSegment, NextSegment);
	}
};

FMovieSceneTrackSegmentBlenderPtr UMovieSceneNiagaraSystemTrack::GetTrackSegmentBlender() const
{
	return FNiagaraSystemTrackSegmentBlender();
}
