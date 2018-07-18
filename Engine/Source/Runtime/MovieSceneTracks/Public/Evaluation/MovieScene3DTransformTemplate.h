// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/RichCurve.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Evaluation/Blending/BlendableToken.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Evaluation/Blending/MovieSceneMultiChannelBlending.h"
#include "MovieScene3DTransformTemplate.generated.h"

class UMovieScene3DTransformSection;

USTRUCT()
struct FMovieScene3DTransformTemplateData
{
	GENERATED_BODY()

	FMovieScene3DTransformTemplateData()
		: BlendType((EMovieSceneBlendType)0)
		, bUseQuaternionInterpolation(false) 
	{}
	FMovieScene3DTransformTemplateData(const UMovieScene3DTransformSection& Section);

	MovieScene::TMultiChannelValue<float, 9> Evaluate(FFrameTime InTime) const;

	UPROPERTY()
	FMovieSceneFloatChannel TranslationCurve[3];

	UPROPERTY()
	FMovieSceneFloatChannel RotationCurve[3];

	UPROPERTY()
	FMovieSceneFloatChannel ScaleCurve[3];

	UPROPERTY()
	FMovieSceneFloatChannel ManualWeight;

	UPROPERTY() 
	EMovieSceneBlendType BlendType;

	UPROPERTY()
	FMovieSceneTransformMask Mask;

	UPROPERTY()
	bool bUseQuaternionInterpolation;
};

USTRUCT()
struct FMovieSceneComponentTransformSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	UPROPERTY()
	FMovieScene3DTransformTemplateData TemplateData;

	FMovieSceneComponentTransformSectionTemplate(){}
	FMovieSceneComponentTransformSectionTemplate(const UMovieScene3DTransformSection& Section);

protected:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void SetupOverrides() override { EnableOverrides(RequiresInitializeFlag); }
	virtual void Initialize(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
	virtual void Interrogate(const FMovieSceneContext& Context, FMovieSceneInterrogationData& Container, UObject* BindingOverride) const override;
};
