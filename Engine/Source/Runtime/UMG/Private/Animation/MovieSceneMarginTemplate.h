// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Evaluation/MovieScenePropertyTemplate.h"
#include "Evaluation/Blending/MovieSceneMultiChannelBlending.h"
#include "Layout/Margin.h"
#include "Animation/MovieSceneMarginSection.h"
#include "MovieSceneMarginTemplate.generated.h"

class UMovieSceneMarginSection;
class UMovieScenePropertyTrack;

Expose_TNameOf(FMargin);

USTRUCT()
struct FMovieSceneMarginSectionTemplate : public FMovieScenePropertySectionTemplate
{
	GENERATED_BODY()
	
	FMovieSceneMarginSectionTemplate() : BlendType((EMovieSceneBlendType)0) {}
	FMovieSceneMarginSectionTemplate(const UMovieSceneMarginSection& Section, const UMovieScenePropertyTrack& Track);

	const FMovieSceneFloatChannel& GetTopCurve() const    { return TopCurve; }
	const FMovieSceneFloatChannel& GetLeftCurve() const   { return LeftCurve; }
	const FMovieSceneFloatChannel& GetRightCurve() const  { return RightCurve; }
	const FMovieSceneFloatChannel& GetBottomCurve() const { return BottomCurve; }

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
	virtual void Interrogate(const FMovieSceneContext& Context, FMovieSceneInterrogationData& Container, UObject* BindingOverride) const override;

	UPROPERTY()
	FMovieSceneFloatChannel TopCurve;

	UPROPERTY()
	FMovieSceneFloatChannel LeftCurve;

	UPROPERTY()
	FMovieSceneFloatChannel RightCurve;

	UPROPERTY()
	FMovieSceneFloatChannel BottomCurve;

	UPROPERTY()
	EMovieSceneBlendType BlendType;
};

template<>
inline void TPropertyActuator<FMargin>::Actuate(FMovieSceneInterrogationData& InterrogationData, typename TCallTraits<FMargin>::ParamType InValue, const TBlendableTokenStack<FMargin>& OriginalStack, const FMovieSceneContext& Context) const
{
	FMargin Value = InValue;
	InterrogationData.Add(Value, UMovieSceneMarginSection::GetMarginInterrogationKey());
};

/** Access the unique runtime type identifier for a margin. */
template<> UMG_API FMovieSceneAnimTypeID GetBlendingDataType<FMargin>();

/** Inform the blending accumulator to use a 4 channel float to blend margins */
template<> struct TBlendableTokenTraits<FMargin>
{
	typedef MovieScene::TMaskedBlendable<float, 4> WorkingDataType;
};

namespace MovieScene
{
	/** Convert a margin into a 4 channel blendable float */
	inline void MultiChannelFromData(FMargin In, TMultiChannelValue<float, 4>& Out)
	{
		Out = { In.Left, In.Top, In.Right, In.Bottom };
	}

	/** Convert a 4 channel blendable float into a margin */
	inline void ResolveChannelsToData(const TMultiChannelValue<float, 4>& In, FMargin& Out)
	{
		Out = FMargin(In[0], In[1], In[2], In[3]);
	}
}