// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneGeometryCacheTemplate.h"
#include "Compilation/MovieSceneCompilerRules.h"
#include "GeometryCacheComponent.h"
#include "Evaluation/MovieSceneEvaluation.h"
#include "IMovieScenePlayer.h"
#include "UObject/ObjectKey.h"
#include "GeometryCacheComponent.h"


DECLARE_CYCLE_STAT(TEXT("Geometry Cache Evaluate"), MovieSceneEval_GeometryCache_Evaluate, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Geometry Cache Token Execute"), MovieSceneEval_GeometryCache_TokenExecute, STATGROUP_MovieSceneEval);

/** Used to set Manual Tick back to previous when outside section */
struct FPreAnimatedGeometryCacheTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const
	{
		struct FToken : IMovieScenePreAnimatedToken
		{
			FToken(UGeometryCacheComponent* InComponent)
			{
				// Cache this object's current update flag and animation mode
				bInManualTick = InComponent->GetManualTick();
			}

			virtual void RestoreState(UObject& ObjectToRestore, IMovieScenePlayer& Player)
			{
				UGeometryCacheComponent* Component = CastChecked<UGeometryCacheComponent>(&ObjectToRestore);
				Component->SetManualTick(bInManualTick);
			}
			bool bInManualTick;
		};

		return FToken(CastChecked<UGeometryCacheComponent>(&Object));
	}
	static FMovieSceneAnimTypeID GetAnimTypeID()
	{
		return TMovieSceneAnimTypeID<FPreAnimatedGeometryCacheTokenProducer>();
	}
};


/** A movie scene execution token that executes a geometry cache */
struct FGeometryCacheExecutionToken
	: IMovieSceneExecutionToken
	
{
	FGeometryCacheExecutionToken(const FMovieSceneGeometryCacheSectionTemplateParameters &InParams):
		Params(InParams)
	{}

	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		if (Params.GeometryCache.ResolveObject())
		{
			MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_GeometryCache_TokenExecute)

			UGeometryCacheComponent* GeometryCache = Cast<UGeometryCacheComponent>(Params.GeometryCache.ResolveObject());

			Player.SavePreAnimatedState(*GeometryCache, FPreAnimatedGeometryCacheTokenProducer::GetAnimTypeID(), FPreAnimatedGeometryCacheTokenProducer());
			GeometryCache->SetManualTick(true);
			// calculate the time at which to evaluate the animation
			float EvalTime = Params.MapTimeToAnimation(Context.GetTime(), Context.GetFrameRate());
			GeometryCache->TickAtThisTime(EvalTime, true, Params.bReverse, true);
		}

	}

	FMovieSceneGeometryCacheSectionTemplateParameters Params;
};

FMovieSceneGeometryCacheSectionTemplate::FMovieSceneGeometryCacheSectionTemplate(const UMovieSceneGeometryCacheSection& InSection)
	: Params(InSection.Params, InSection.GetInclusiveStartFrame(), InSection.GetExclusiveEndFrame())
{
}

//We use a token here so we can set the manual tick state back to what it was previously when outside this section.
//This is similar to how Skeletal Animation evaluation also works.
void FMovieSceneGeometryCacheSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_GeometryCache_Evaluate)
	ExecutionTokens.Add(FGeometryCacheExecutionToken(Params));
}

float FMovieSceneGeometryCacheSectionTemplateParameters::MapTimeToAnimation(FFrameTime InPosition, FFrameRate InFrameRate) const
{
	InPosition = FMath::Clamp(InPosition, FFrameTime(SectionStartTime), FFrameTime(SectionEndTime-1));

	const float SectionPlayRate = PlayRate;
	const float AnimPlayRate = FMath::IsNearlyZero(SectionPlayRate) ? 1.0f : SectionPlayRate;

	const float SeqLength = GetSequenceLength() - (StartOffset + EndOffset);

	float AnimPosition = FFrameTime::FromDecimal((InPosition - SectionStartTime).AsDecimal() * AnimPlayRate) / InFrameRate;
	if (SeqLength > 0.f)
	{
		AnimPosition = FMath::Fmod(AnimPosition, SeqLength);
	}
	AnimPosition += StartOffset;
	if (bReverse)
	{
		AnimPosition = (SeqLength - (AnimPosition - StartOffset)) + StartOffset;
	}

	return AnimPosition;
}
