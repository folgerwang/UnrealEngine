// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/MovieSceneObjectPathChannel.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "MovieScenePrimitiveMaterialTemplate.generated.h"

class UMovieScenePrimitiveMaterialTrack;
class UMovieScenePrimitiveMaterialSection;

USTRUCT()
struct FMovieScenePrimitiveMaterialTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FMovieScenePrimitiveMaterialTemplate() : MaterialIndex(0) {}
	FMovieScenePrimitiveMaterialTemplate(const UMovieScenePrimitiveMaterialSection& Section, const UMovieScenePrimitiveMaterialTrack& Track);

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	int32 MaterialIndex;

	UPROPERTY()
	FMovieSceneObjectPathChannel MaterialChannel;
};
