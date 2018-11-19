// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieScenePrimitiveMaterialTemplate.h"
#include "Tracks/MovieScenePrimitiveMaterialTrack.h"
#include "Sections/MovieScenePrimitiveMaterialSection.h"
#include "UObject/StrongObjectPtr.h"
#include "Materials/MaterialInterface.h"
#include "Components/PrimitiveComponent.h"

struct FSetMaterialToken : IMovieScenePreAnimatedToken
{
	int32 MaterialIndex;
	TStrongObjectPtr<UMaterialInterface> Material;

	FSetMaterialToken(int32 InMaterialIndex, UMaterialInterface* InMaterial)
		: MaterialIndex(InMaterialIndex), Material(InMaterial)
	{}

	virtual void RestoreState(UObject& Object, IMovieScenePlayer& Player) override
	{
		CastChecked<UPrimitiveComponent>(&Object)->SetMaterial(MaterialIndex, Material.Get());
	}
};


struct FSetMaterialTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	int32 MaterialIndex;
	UMaterialInterface* Material;

	FSetMaterialTokenProducer(int32 InMaterialIndex, UMaterialInterface* InMaterial)
		: MaterialIndex(InMaterialIndex), Material(InMaterial)
	{}

	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
	{
		return FSetMaterialToken(MaterialIndex, Material);
	}
};



struct FPrimitiveMaterialExecToken : IMovieSceneExecutionToken
{
	int32 MaterialIndex;
	TSoftObjectPtr<> Value;

	FPrimitiveMaterialExecToken(int32 InMaterialIndex, TSoftObjectPtr<>&& InValue)
		: MaterialIndex(InMaterialIndex)
		, Value(MoveTemp(InValue))
	{}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		UMaterialInterface* NewMaterial = Cast<UMaterialInterface>(Value.LoadSynchronous());

		for (TWeakObjectPtr<> WeakObject : Player.FindBoundObjects(Operand))
		{
			UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(WeakObject.Get());
			if (PrimitiveComponent && MaterialIndex >= 0 && MaterialIndex < PrimitiveComponent->GetNumMaterials())
			{
				UMaterialInterface* ExistingMaterial = PrimitiveComponent->GetMaterial(MaterialIndex);
				Player.SavePreAnimatedState(*PrimitiveComponent, TMovieSceneAnimTypeID<FPrimitiveMaterialExecToken>(), FSetMaterialTokenProducer(MaterialIndex, ExistingMaterial));

				if (NewMaterial != ExistingMaterial)
				{
					PrimitiveComponent->SetMaterial(MaterialIndex, NewMaterial);
				}
			}
		}
	}
};


FMovieScenePrimitiveMaterialTemplate::FMovieScenePrimitiveMaterialTemplate(const UMovieScenePrimitiveMaterialSection& Section, const UMovieScenePrimitiveMaterialTrack& Track)
	: MaterialIndex(Track.MaterialIndex)
	, MaterialChannel(Section.MaterialChannel)
{}

void FMovieScenePrimitiveMaterialTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	TSoftObjectPtr<> Ptr;
	if (MaterialChannel.Evaluate(Context.GetTime(), Ptr))
	{
		ExecutionTokens.Add(FPrimitiveMaterialExecToken(MaterialIndex, MoveTemp(Ptr)));
	}
}
