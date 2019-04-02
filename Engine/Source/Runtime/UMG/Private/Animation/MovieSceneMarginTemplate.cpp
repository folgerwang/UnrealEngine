// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Animation/MovieSceneMarginTemplate.h"

#include "Animation/MovieSceneMarginSection.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "Evaluation/MovieSceneEvaluation.h"

template<> FMovieSceneAnimTypeID GetBlendingDataType<FMargin>()
{
	static FMovieSceneAnimTypeID TypeId = FMovieSceneAnimTypeID::Unique();
	return TypeId;
}

FMovieSceneMarginSectionTemplate::FMovieSceneMarginSectionTemplate(const UMovieSceneMarginSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, TopCurve(Section.TopCurve)
	, LeftCurve(Section.LeftCurve)
	, RightCurve(Section.RightCurve)
	, BottomCurve(Section.BottomCurve)
	, BlendType(Section.GetBlendType().Get())
{
}

void FMovieSceneMarginSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	const FFrameTime Time = Context.GetTime();
	MovieScene::TMultiChannelValue<float, 4> AnimatedData;

	float Value = 0.f;
	if (LeftCurve.Evaluate(Time, Value))
	{
		AnimatedData.Set(0, Value);
	}

	if (TopCurve.Evaluate(Time, Value))
	{
		AnimatedData.Set(1, Value);
	}

	if (RightCurve.Evaluate(Time, Value))
	{
		AnimatedData.Set(2, Value);
	}

	if (BottomCurve.Evaluate(Time, Value))
	{
		AnimatedData.Set(3, Value);
	}

	if (!AnimatedData.IsEmpty())
	{
		FMovieSceneBlendingActuatorID ActuatorTypeID = EnsureActuator<FMargin>(ExecutionTokens.GetBlendingAccumulator());

		// Add the blendable to the accumulator
		float Weight = EvaluateEasing(Context.GetTime());
		ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<FMargin>(AnimatedData, BlendType, Weight));
	}
}


void FMovieSceneMarginSectionTemplate::Interrogate(const FMovieSceneContext& Context, FMovieSceneInterrogationData& Container, UObject* BindingOverride) const
{
	const FFrameTime Time = Context.GetTime();
	MovieScene::TMultiChannelValue<float, 4> AnimatedData;

	float Value = 0.f;
	if (LeftCurve.Evaluate(Time, Value))
	{
		AnimatedData.Set(0, Value);
	}

	if (TopCurve.Evaluate(Time, Value))
	{
		AnimatedData.Set(1, Value);
	}

	if (RightCurve.Evaluate(Time, Value))
	{
		AnimatedData.Set(2, Value);
	}

	if (BottomCurve.Evaluate(Time, Value))
	{
		AnimatedData.Set(3, Value);
	}

	FMovieSceneAnimTypeID TypeID = GetPropertyTypeID();
	static FMovieSceneBlendingActuatorID ActuatorTypeID(TypeID);
	if (!Container.GetAccumulator().FindActuator<FMargin>(ActuatorTypeID))
	{
		PropertyTemplate::FSectionData SectionData;
		SectionData.Initialize(PropertyData.PropertyName, PropertyData.PropertyPath, PropertyData.FunctionName, PropertyData.NotifyFunctionName);
		Container.GetAccumulator().DefineActuator(ActuatorTypeID, MakeShared<TPropertyActuator<FMargin>>(SectionData));
	}

	if (!AnimatedData.IsEmpty())
	{
		// Add the blendable to the accumulator
		float Weight = EvaluateEasing(Context.GetTime());
		Container.GetAccumulator().BlendToken(FMovieSceneEvaluationOperand(), ActuatorTypeID, FMovieSceneEvaluationScope(), Context, TBlendableToken<FMargin>(AnimatedData, BlendType, Weight));
	}
}
