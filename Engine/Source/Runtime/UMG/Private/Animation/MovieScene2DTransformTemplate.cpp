// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Animation/MovieScene2DTransformTemplate.h"
#include "Animation/MovieScene2DTransformSection.h"
#include "Animation/MovieScene2DTransformTrack.h"
#include "Evaluation/MovieSceneEvaluation.h"

template<>
FMovieSceneAnimTypeID GetBlendingDataType<FWidgetTransform>()
{
	static FMovieSceneAnimTypeID TypeId = FMovieSceneAnimTypeID::Unique();
	return TypeId;
}

FMovieScene2DTransformSectionTemplate::FMovieScene2DTransformSectionTemplate(const UMovieScene2DTransformSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, BlendType(Section.GetBlendType().Get())
{
	Translation[0] = Section.Translation[0];
	Translation[1] = Section.Translation[1];

	Rotation = Section.Rotation;

	Scale[0] = Section.Scale[0];
	Scale[1] = Section.Scale[1];

	Shear[0] = Section.Shear[0];
	Shear[1] = Section.Shear[1];
}

void FMovieScene2DTransformSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	const FFrameTime Time = Context.GetTime();
	MovieScene::TMultiChannelValue<float, 7> AnimatedData;

	// Only activate channels if the curve has data associated with it
	auto EvalChannel = [&AnimatedData, Time](uint8 ChanneIndex, const FMovieSceneFloatChannel& Channel)
	{
		float Value = 0.f;
		if (Channel.Evaluate(Time, Value))
		{
			AnimatedData.Set(ChanneIndex, Value);
		}
	};

	EvalChannel(0, Translation[0]);
	EvalChannel(1, Translation[1]);

	EvalChannel(2, Scale[0]);
	EvalChannel(3, Scale[1]);

	EvalChannel(4, Shear[0]);
	EvalChannel(5, Shear[1]);

	EvalChannel(6, Rotation);

	if (!AnimatedData.IsEmpty())
	{
		FMovieSceneBlendingActuatorID ActuatorTypeID = EnsureActuator<FWidgetTransform>(ExecutionTokens.GetBlendingAccumulator());

		// Add the blendable to the accumulator
		float Weight = EvaluateEasing(Context.GetTime());
		ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<FWidgetTransform>(AnimatedData, BlendType, Weight));
	}
}
