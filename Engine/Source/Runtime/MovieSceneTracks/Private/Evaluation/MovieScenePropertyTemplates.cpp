// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieScenePropertyTemplates.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Sections/MovieSceneByteSection.h"
#include "Sections/MovieSceneEnumSection.h"
#include "Sections/MovieSceneIntegerSection.h"
#include "Sections/MovieSceneVectorSection.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneStringSection.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieScene.h"
#include "Evaluation/MovieSceneEvaluation.h"
#include "MovieSceneTemplateCommon.h"
#include "Evaluation/Blending/MovieSceneMultiChannelBlending.h"

namespace
{
	FName SanitizeBoolPropertyName(FName InPropertyName)
	{
		FString PropertyVarName = InPropertyName.ToString();
		PropertyVarName.RemoveFromStart("b", ESearchCase::CaseSensitive);
		return FName(*PropertyVarName);
	}
}

//	----------------------------------------------------------------------------
//	Boolean Property Template
FMovieSceneBoolPropertySectionTemplate::FMovieSceneBoolPropertySectionTemplate(const UMovieSceneBoolSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, BoolCurve(Section.GetChannel())
{
	PropertyData.PropertyName = SanitizeBoolPropertyName(PropertyData.PropertyName);
}

void FMovieSceneBoolPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	// Only evaluate if the curve has any data
	bool Result = false;
	if (BoolCurve.Evaluate(Context.GetTime(), Result))
	{
		ExecutionTokens.Add(TPropertyTrackExecutionToken<bool>(Result));
	}
}


//	----------------------------------------------------------------------------
//	Float Property Template
FMovieSceneFloatPropertySectionTemplate::FMovieSceneFloatPropertySectionTemplate(const UMovieSceneFloatSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, FloatFunction(Section.GetChannel())
	, BlendType(Section.GetBlendType().Get())
{}

void FMovieSceneFloatPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	float Result = 0.f;

	// Only evaluate if the curve has any data
	if (FloatFunction.Evaluate(Context.GetTime(), Result))
	{
		// Actuator type ID for this property
		FMovieSceneBlendingActuatorID ActuatorTypeID = EnsureActuator<float>(ExecutionTokens.GetBlendingAccumulator());

		// Add the blendable to the accumulator
		const float Weight = EvaluateEasing(Context.GetTime());
		ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<float>(Result, BlendType, Weight));
	}
}


//	----------------------------------------------------------------------------
//	Byte Property Template
FMovieSceneBytePropertySectionTemplate::FMovieSceneBytePropertySectionTemplate(const UMovieSceneByteSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, ByteCurve(Section.ByteCurve)
{}

void FMovieSceneBytePropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	uint8 Result = 0;
	if (ByteCurve.Evaluate(Context.GetTime(), Result))
	{
		ExecutionTokens.Add(TPropertyTrackExecutionToken<uint8>(Result));
	}
}


//	----------------------------------------------------------------------------
//	Enum Property Template
FMovieSceneEnumPropertySectionTemplate::FMovieSceneEnumPropertySectionTemplate(const UMovieSceneEnumSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, EnumCurve(Section.EnumCurve)
{}

void FMovieSceneEnumPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	uint8 Result = 0;
	if (EnumCurve.Evaluate(Context.GetTime(), Result))
	{
		ExecutionTokens.Add(TPropertyTrackExecutionToken<uint8>(Result));
	}
}


//	----------------------------------------------------------------------------
//	Integer Property Template
FMovieSceneIntegerPropertySectionTemplate::FMovieSceneIntegerPropertySectionTemplate(const UMovieSceneIntegerSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, IntegerCurve(Section.GetChannel())
	, BlendType(Section.GetBlendType().Get())
{}

void FMovieSceneIntegerPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	int32 Result = 0;
	if (IntegerCurve.Evaluate(Context.GetTime(), Result))
	{
		// Actuator type ID for this property
		FMovieSceneBlendingActuatorID ActuatorTypeID = EnsureActuator<int32>(ExecutionTokens.GetBlendingAccumulator());

		// Add the blendable to the accumulator
		const float Weight = EvaluateEasing(Context.GetTime());
		ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<int32>(Result, BlendType, Weight));
	}
}


//	----------------------------------------------------------------------------
//	String Property Template
FMovieSceneStringPropertySectionTemplate::FMovieSceneStringPropertySectionTemplate(const UMovieSceneStringSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, StringCurve(Section.GetChannel())
{}

void FMovieSceneStringPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	const FString* Result = StringCurve.Evaluate(Context.GetTime());
	if (Result)
	{
		ExecutionTokens.Add(TPropertyTrackExecutionToken<FString>(*Result));
	}
}


//	----------------------------------------------------------------------------
//	Vector Property Template
FMovieSceneVectorPropertySectionTemplate::FMovieSceneVectorPropertySectionTemplate(const UMovieSceneVectorSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, NumChannelsUsed(Section.GetChannelsUsed())
	, BlendType(Section.GetBlendType().Get())
{
	for (int32 Index = 0; Index < NumChannelsUsed; ++Index)
	{
		ComponentCurves[Index] = Section.GetChannel(Index);
	}
}

/** Helper function fo evaluating a number of curves for a specific vector type */
template<typename VectorType, uint8 N>
void EvaluateVectorCurve(EMovieSceneBlendType BlendType, float Weight, FFrameTime Time, const FMovieSceneFloatChannel* Channels, FMovieSceneBlendingActuatorID ActuatorTypeID, FMovieSceneExecutionTokens& ExecutionTokens)
{
	MovieScene::TMultiChannelValue<float, N> AnimatedChannels;

	for (uint8 Index = 0; Index < N; ++Index)
	{
		const FMovieSceneFloatChannel& Channel = Channels[Index];
		float Result = 0;
		if (Channel.Evaluate(Time, Result))
		{
			AnimatedChannels.Set(Index, Result);
		}
	}

	// Only blend the token if at least one of the channels was animated
	if (!AnimatedChannels.IsEmpty())
	{
		ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<VectorType>(AnimatedChannels, BlendType, Weight));
	}
}

void FMovieSceneVectorPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	FMovieSceneBlendingAccumulator& Accumulator = ExecutionTokens.GetBlendingAccumulator();

	const FFrameTime Time   = Context.GetTime();
	const float      Weight = EvaluateEasing(Context.GetTime());

	switch (NumChannelsUsed)
	{
	case 2:
		EvaluateVectorCurve<FVector2D, 2>(BlendType, Weight, Time, ComponentCurves, EnsureActuator<FVector2D>(Accumulator), ExecutionTokens);
		break;

	case 3:
		EvaluateVectorCurve<FVector, 3>(BlendType, Weight, Time, ComponentCurves, EnsureActuator<FVector>(Accumulator), ExecutionTokens);
		break;

	case 4:
		EvaluateVectorCurve<FVector4, 4>(BlendType, Weight, Time, ComponentCurves, EnsureActuator<FVector4>(Accumulator), ExecutionTokens);
		break;

	default:
		UE_LOG(LogMovieScene, Warning, TEXT("Invalid number of channels(%d) for vector track"), NumChannelsUsed );
		break;
	}
}


//	----------------------------------------------------------------------------
//	Transform Property Template
FMovieSceneTransformPropertySectionTemplate::FMovieSceneTransformPropertySectionTemplate(const UMovieScene3DTransformSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, TemplateData(Section)
{}

void FMovieSceneTransformPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	MovieScene::TMultiChannelValue<float, 9> TransformValue = TemplateData.Evaluate(Context.GetTime());

	// Actuator type ID for this property
	FMovieSceneBlendingActuatorID ActuatorTypeID = EnsureActuator<FTransform>(ExecutionTokens.GetBlendingAccumulator());

	// Evaluate the easing, and multiply this with the manual weight specified on the manual weight curve
	float Weight = EvaluateEasing(Context.GetTime());
	if (EnumHasAllFlags(TemplateData.Mask.GetChannels(), EMovieSceneTransformChannel::Weight))
	{
		float ManualWeight = 1.f;
		TemplateData.ManualWeight.Evaluate(Context.GetTime(), ManualWeight);
		Weight *= ManualWeight;
	}

	// Add the blendable to the accumulator
	ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<FTransform>(TransformValue, TemplateData.BlendType, Weight));
}


//	----------------------------------------------------------------------------
//	Euler transform Property Template
FMovieSceneEulerTransformPropertySectionTemplate::FMovieSceneEulerTransformPropertySectionTemplate(const UMovieScene3DTransformSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, TemplateData(Section)
{}

void FMovieSceneEulerTransformPropertySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	MovieScene::TMultiChannelValue<float, 9> TransformValue = TemplateData.Evaluate(Context.GetTime());

	// Actuator type ID for this property
	FMovieSceneBlendingActuatorID ActuatorTypeID = EnsureActuator<FEulerTransform>(ExecutionTokens.GetBlendingAccumulator());

	// Add the blendable to the accumulator
	float Weight = EvaluateEasing(Context.GetTime());
	if (EnumHasAllFlags(TemplateData.Mask.GetChannels(), EMovieSceneTransformChannel::Weight))
	{
		float ChannelWeight = 0.f;
		if (TemplateData.ManualWeight.Evaluate(Context.GetTime(), ChannelWeight))
		{
			Weight *= ChannelWeight;
		}
	}

	// Add the blendable to the accumulator
	ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<FEulerTransform>(TransformValue, TemplateData.BlendType, Weight));
}