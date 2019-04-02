// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Animation/MovieScene2DTransformSection.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Evaluation/MovieScenePropertyTemplate.h"
#include "Evaluation/Blending/MovieSceneMultiChannelBlending.h"
#include "Slate/WidgetTransform.h"
#include "Evaluation/MovieScenePropertyTemplate.h"
#include "MovieScene2DTransformTemplate.generated.h"

class UMovieScene2DTransformSection;
class UMovieScenePropertyTrack;

Expose_TNameOf(FWidgetTransform);

USTRUCT()
struct FMovieScene2DTransformSectionTemplate : public FMovieScenePropertySectionTemplate
{
	GENERATED_BODY()
	
	FMovieScene2DTransformSectionTemplate() : BlendType((EMovieSceneBlendType)0) {}
	FMovieScene2DTransformSectionTemplate(const UMovieScene2DTransformSection& Section, const UMovieScenePropertyTrack& Track);

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }

	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	virtual void Interrogate(const FMovieSceneContext& Context, FMovieSceneInterrogationData& Container, UObject* BindingOverride) const override;

	/** Translation curves */
	UPROPERTY()
	FMovieSceneFloatChannel Translation[2];
	
	/** Rotation curve */
	UPROPERTY()
	FMovieSceneFloatChannel Rotation;

	/** Scale curves */
	UPROPERTY()
	FMovieSceneFloatChannel Scale[2];

	/** Shear curve */
	UPROPERTY()
	FMovieSceneFloatChannel Shear[2];

	/** Blending method */
	UPROPERTY()
	EMovieSceneBlendType BlendType;

	UPROPERTY()
	FMovieScene2DTransformMask Mask;
};

template<>
inline void TPropertyActuator<FWidgetTransform>::Actuate(FMovieSceneInterrogationData& InterrogationData, typename TCallTraits<FWidgetTransform>::ParamType InValue, const TBlendableTokenStack<FWidgetTransform>& OriginalStack, const FMovieSceneContext& Context) const
{
	FWidgetTransform Value = InValue;
	InterrogationData.Add(Value, UMovieScene2DTransformSection::GetWidgetTransformInterrogationKey());
};

/** Access the unique runtime type identifier for a widget transform. */
template<> UMG_API FMovieSceneAnimTypeID GetBlendingDataType<FWidgetTransform>();

/** Inform the blending accumulator to use a 7 channel float to blend margins */
template<> struct TBlendableTokenTraits<FWidgetTransform>
{
	typedef MovieScene::TMaskedBlendable<float, 7> WorkingDataType;
};

namespace MovieScene
{
	/** Convert a widget transform into a 7 channel float */
	inline void MultiChannelFromData(const FWidgetTransform& In, TMultiChannelValue<float, 7>& Out)
	{
		Out = { In.Translation.X, In.Translation.Y, In.Scale.X, In.Scale.Y, In.Shear.X, In.Shear.Y, In.Angle };
	}

	/** Convert a 7 channel float into a widget transform */
	inline void ResolveChannelsToData(const TMultiChannelValue<float, 7>& In, FWidgetTransform& Out)
	{
		Out.Translation = FVector2D(In[0], In[1]);
		Out.Scale = FVector2D(In[2], In[3]);
		Out.Shear = FVector2D(In[4], In[5]);
		Out.Angle = In[6];
	}
}
