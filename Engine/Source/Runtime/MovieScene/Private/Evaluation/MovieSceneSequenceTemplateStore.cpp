// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneSequenceTemplateStore.h"
#include "MovieSceneSequence.h"
#include "UObject/ObjectKey.h"

FMovieSceneEvaluationTemplate& FMovieSceneSequencePrecompiledTemplateStore::AccessTemplate(UMovieSceneSequence& Sequence)
{
	return Sequence.PrecompiledEvaluationTemplate;
}