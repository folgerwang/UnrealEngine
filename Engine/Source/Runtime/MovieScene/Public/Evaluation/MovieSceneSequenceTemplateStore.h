// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/UniquePtr.h"
#include "Evaluation/MovieSceneEvaluationTemplate.h"

class UMovieSceneSequence;

/**
 * Interface that is used to retrieve an evaluation template for a given sequence
 */
struct IMovieSceneSequenceTemplateStore
{
	virtual ~IMovieSceneSequenceTemplateStore() {}

	/**
	 * Access the template for the specified sequence.
	 * Returned templates must be valid, and must never be re-allocated.
	 */
	virtual FMovieSceneEvaluationTemplate& AccessTemplate(UMovieSceneSequence& Sequence) = 0;
};


/**
 * Implementation of a template store that just returns UMovieSceneSequence::PrecompiledEvaluationTemplate
 */
struct FMovieSceneSequencePrecompiledTemplateStore : IMovieSceneSequenceTemplateStore
{
	MOVIESCENE_API virtual FMovieSceneEvaluationTemplate& AccessTemplate(UMovieSceneSequence& Sequence);
};