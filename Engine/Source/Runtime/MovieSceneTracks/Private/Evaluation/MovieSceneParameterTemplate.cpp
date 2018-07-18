// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneParameterTemplate.h"
#include "Tracks/MovieSceneMaterialTrack.h"
#include "Components/PrimitiveComponent.h"
#include "Evaluation/MovieSceneEvaluation.h"


FMovieSceneParameterSectionTemplate::FMovieSceneParameterSectionTemplate(const UMovieSceneParameterSection& Section)
	: Scalars(Section.GetScalarParameterNamesAndCurves())
	, Vectors(Section.GetVectorParameterNamesAndCurves())
	, Colors(Section.GetColorParameterNamesAndCurves())
{
}

void FMovieSceneParameterSectionTemplate::EvaluateCurves(const FMovieSceneContext& Context, FEvaluatedParameterSectionValues& Values) const
{
	const FFrameTime Time = Context.GetTime();

	for ( const FScalarParameterNameAndCurve& Scalar : Scalars )
	{
		float Value = 0;
		if (Scalar.ParameterCurve.Evaluate(Time, Value))
		{
			Values.ScalarValues.Emplace(Scalar.ParameterName, Value);
		}
	}

	for ( const FVectorParameterNameAndCurves& Vector : Vectors )
	{
		FVector Value(ForceInitToZero);

		bool bAnyEvaluated = false;
		bAnyEvaluated |= Vector.XCurve.Evaluate(Time, Value.X);
		bAnyEvaluated |= Vector.YCurve.Evaluate(Time, Value.Y);
		bAnyEvaluated |= Vector.ZCurve.Evaluate(Time, Value.Z);

		if (bAnyEvaluated)
		{
			Values.VectorValues.Emplace(Vector.ParameterName, Value);
		}
	}

	for ( const FColorParameterNameAndCurves& Color : Colors )
	{
		FLinearColor ColorValue = FLinearColor::White;

		bool bAnyEvaluated = false;
		bAnyEvaluated |= Color.RedCurve.Evaluate(  Time, ColorValue.R);
		bAnyEvaluated |= Color.GreenCurve.Evaluate(Time, ColorValue.G);
		bAnyEvaluated |= Color.BlueCurve.Evaluate( Time, ColorValue.B);
		bAnyEvaluated |= Color.AlphaCurve.Evaluate(Time, ColorValue.A);

		if (bAnyEvaluated)
		{
			Values.ColorValues.Emplace(Color.ParameterName, ColorValue);
		}
	}
}

void FDefaultMaterialAccessor::Apply(UMaterialInstanceDynamic& Material, const FEvaluatedParameterSectionValues& Values)
{
	for (const FScalarParameterNameAndValue& ScalarValue : Values.ScalarValues)
	{
		Material.SetScalarParameterValue(ScalarValue.ParameterName, ScalarValue.Value);
	}
	for (const FVectorParameterNameAndValue& VectorValue : Values.VectorValues)
	{
		Material.SetVectorParameterValue(VectorValue.ParameterName, VectorValue.Value);
	}
	for (const FColorParameterNameAndValue& ColorValue : Values.ColorValues)
	{
		Material.SetVectorParameterValue(ColorValue.ParameterName, ColorValue.Value);
	}
}


TMovieSceneAnimTypeIDContainer<int32> MaterialIndexAnimTypeIDs;

struct FComponentMaterialAccessor : FDefaultMaterialAccessor
{
	FComponentMaterialAccessor(int32 InMaterialIndex)
		: MaterialIndex(InMaterialIndex)
	{}

	FComponentMaterialAccessor(const FComponentMaterialAccessor&) = default;
	FComponentMaterialAccessor& operator=(const FComponentMaterialAccessor&) = default;
	
	FMovieSceneAnimTypeID GetAnimTypeID() const
	{
		return MaterialIndexAnimTypeIDs.GetAnimTypeID(MaterialIndex);
	}

	UMaterialInterface* GetMaterialForObject(UObject& Object) const
	{
		UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(&Object);
		return Component ? Component->GetMaterial(MaterialIndex) : nullptr;
	}

	void SetMaterialForObject(UObject& Object, UMaterialInterface& Material) const
	{
		UPrimitiveComponent* Component = CastChecked<UPrimitiveComponent>(&Object);
		Component->SetMaterial(MaterialIndex, &Material);
	}

	int32 MaterialIndex;
};

FMovieSceneComponentMaterialSectionTemplate::FMovieSceneComponentMaterialSectionTemplate(const UMovieSceneParameterSection& Section, const UMovieSceneComponentMaterialTrack& Track)
	: FMovieSceneParameterSectionTemplate(Section)
	, MaterialIndex(Track.GetMaterialIndex())
{
}

void FMovieSceneComponentMaterialSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	TMaterialTrackExecutionToken<FComponentMaterialAccessor> ExecutionToken(MaterialIndex);

	EvaluateCurves(Context, ExecutionToken.Values);

	ExecutionTokens.Add(MoveTemp(ExecutionToken));
}
