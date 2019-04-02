// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
	, Mask(Section.GetMask())
{
	EMovieScene2DTransformChannel MaskChannels = Mask.GetChannels();

	if (EnumHasAllFlags(MaskChannels, EMovieScene2DTransformChannel::TranslationX))	Translation[0] = Section.Translation[0];
	if (EnumHasAllFlags(MaskChannels, EMovieScene2DTransformChannel::TranslationY))	Translation[1] = Section.Translation[1];

	if (EnumHasAllFlags(MaskChannels, EMovieScene2DTransformChannel::Rotation))		Rotation = Section.Rotation;

	if (EnumHasAllFlags(MaskChannels, EMovieScene2DTransformChannel::ScaleX))		Scale[0] = Section.Scale[0];
	if (EnumHasAllFlags(MaskChannels, EMovieScene2DTransformChannel::ScaleY))		Scale[1] = Section.Scale[1];

	if (EnumHasAllFlags(MaskChannels, EMovieScene2DTransformChannel::ShearX))		Shear[0] = Section.Shear[0];
	if (EnumHasAllFlags(MaskChannels, EMovieScene2DTransformChannel::ShearY))		Shear[1] = Section.Shear[1];
}

void FMovieScene2DTransformSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	const FFrameTime Time = Context.GetTime();
	MovieScene::TMultiChannelValue<float, 7> AnimatedData;

	EMovieScene2DTransformChannel ChannelMask = Mask.GetChannels();

	// Only activate channels if the curve has data associated with it
	auto EvalChannel = [&AnimatedData, Time, ChannelMask](uint8 ChanneIndex, EMovieScene2DTransformChannel ChannelType, const FMovieSceneFloatChannel& Channel)
	{
		float Value = 0.f;
		if (EnumHasAllFlags(ChannelMask, ChannelType) && Channel.Evaluate(Time, Value))
		{
			AnimatedData.Set(ChanneIndex, Value);
		}
	};

	EvalChannel(0, EMovieScene2DTransformChannel::TranslationX, Translation[0]);
	EvalChannel(1, EMovieScene2DTransformChannel::TranslationY, Translation[1]);

	EvalChannel(2, EMovieScene2DTransformChannel::ScaleX, Scale[0]);
	EvalChannel(3, EMovieScene2DTransformChannel::ScaleY, Scale[1]);

	EvalChannel(4, EMovieScene2DTransformChannel::ShearX, Shear[0]);
	EvalChannel(5, EMovieScene2DTransformChannel::ShearY, Shear[1]);

	EvalChannel(6, EMovieScene2DTransformChannel::Rotation, Rotation);

	if (!AnimatedData.IsEmpty())
	{
		FMovieSceneBlendingActuatorID ActuatorTypeID = EnsureActuator<FWidgetTransform>(ExecutionTokens.GetBlendingAccumulator());

		// Add the blendable to the accumulator
		float Weight = EvaluateEasing(Context.GetTime());
		ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<FWidgetTransform>(AnimatedData, BlendType, Weight));
	}
}

void FMovieScene2DTransformSectionTemplate::Interrogate(const FMovieSceneContext& Context, FMovieSceneInterrogationData& Container, UObject* BindingOverride) const
{
	const FFrameTime Time = Context.GetTime();
	MovieScene::TMultiChannelValue<float, 7> AnimatedData;

	EMovieScene2DTransformChannel ChannelMask = Mask.GetChannels();

	// Only activate channels if the curve has data associated with it
	auto EvalChannel = [&AnimatedData, Time, ChannelMask](uint8 ChanneIndex, EMovieScene2DTransformChannel ChannelType, const FMovieSceneFloatChannel& Channel)
	{
		float Value = 0.f;
		if (EnumHasAllFlags(ChannelMask, ChannelType) && Channel.Evaluate(Time, Value))
		{
			AnimatedData.Set(ChanneIndex, Value);
		}
	};

	EvalChannel(0, EMovieScene2DTransformChannel::TranslationX, Translation[0]);
	EvalChannel(1, EMovieScene2DTransformChannel::TranslationY, Translation[1]);

	EvalChannel(2, EMovieScene2DTransformChannel::ScaleX, Scale[0]);
	EvalChannel(3, EMovieScene2DTransformChannel::ScaleY, Scale[1]);

	EvalChannel(4, EMovieScene2DTransformChannel::ShearX, Shear[0]);
	EvalChannel(5, EMovieScene2DTransformChannel::ShearY, Shear[1]);

	EvalChannel(6, EMovieScene2DTransformChannel::Rotation, Rotation);

	FMovieSceneAnimTypeID TypeID = GetPropertyTypeID();
	static FMovieSceneBlendingActuatorID ActuatorTypeID(TypeID);
	if (!Container.GetAccumulator().FindActuator<FWidgetTransform>(ActuatorTypeID))
	{
		PropertyTemplate::FSectionData SectionData;
		SectionData.Initialize(PropertyData.PropertyName, PropertyData.PropertyPath, PropertyData.FunctionName, PropertyData.NotifyFunctionName);
		Container.GetAccumulator().DefineActuator(ActuatorTypeID, MakeShared<TPropertyActuator<FWidgetTransform>>(SectionData));
	}

	if (!AnimatedData.IsEmpty())
	{
		// Add the blendable to the accumulator
		float Weight = EvaluateEasing(Context.GetTime());
		Container.GetAccumulator().BlendToken(FMovieSceneEvaluationOperand(), ActuatorTypeID, FMovieSceneEvaluationScope(), Context, TBlendableToken<FWidgetTransform>(AnimatedData, BlendType, Weight));
	}
}
