// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Compilation/MovieSceneSegmentCompiler.h"
#include "Compilation/MovieSceneCompilerRules.h"
#include "Evaluation/MovieSceneEvaluationTree.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
void FMovieSceneSegmentCompilerRules::Blend(FSegmentBlendData& BlendData) const
{
	FMovieSceneSegment TmpSegment;

	for (int32 Index = 0; Index < BlendData.Num(); ++Index)
	{
		TmpSegment.Impls.Add(FSectionEvaluationData(Index, BlendData[Index].Flags));
	}

	BlendSegment(TmpSegment, BlendData);

	FSegmentBlendData NewBlendData;
	for (FSectionEvaluationData EvalData : TmpSegment.Impls)
	{
		NewBlendData.Add(BlendData[EvalData.ImplIndex]);
		NewBlendData.Last().Flags = EvalData.Flags;
	}
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

bool FMovieSceneAdditiveCameraTrackBlender::SortByStartTime(const FMovieSceneSectionData& A, const FMovieSceneSectionData& B)
{
	return TRangeBound<FFrameNumber>::MinLower(A.Section->GetRange().GetLowerBound(), B.Section->GetRange().GetLowerBound()) == A.Section->GetRange().GetLowerBound();
}
