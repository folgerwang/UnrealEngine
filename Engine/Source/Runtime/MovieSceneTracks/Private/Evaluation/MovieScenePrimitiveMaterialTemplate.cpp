// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieScenePrimitiveMaterialTemplate.h"
#include "Tracks/MovieScenePrimitiveMaterialTrack.h"
#include "Sections/MovieScenePrimitiveMaterialSection.h"
#include "UObject/StrongObjectPtr.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
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
	UMaterialInterface* NewMaterial;

	FPrimitiveMaterialExecToken(int32 InMaterialIndex, UMaterialInterface* InNewMaterial)
		: MaterialIndex(InMaterialIndex)
		, NewMaterial(InNewMaterial)
	{}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		static TMovieSceneAnimTypeIDContainer<int32> MaterialIndexToTypeID;
		FMovieSceneAnimTypeID TypeID = MaterialIndexToTypeID.GetAnimTypeID(MaterialIndex);

		for (TWeakObjectPtr<> WeakObject : Player.FindBoundObjects(Operand))
		{
			UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(WeakObject.Get());
			if (PrimitiveComponent && MaterialIndex >= 0 && MaterialIndex < PrimitiveComponent->GetNumMaterials())
			{
				UMaterialInterface*        ExistingMaterial = PrimitiveComponent->GetMaterial(MaterialIndex);
				UMaterialInstanceDynamic*  MaterialInstance = Cast<UMaterialInstanceDynamic>(ExistingMaterial);

				if (MaterialInstance && MaterialInstance->Parent && MaterialInstance->Parent == NewMaterial)
				{
					// Do not re-assign materials when a dynamic instance is already assigned with the same parent (since that's basically the same material, just with animated parameters)
					// This is required for supporting material switchers alongside parameter tracks
					continue;
				}
				
				Player.SavePreAnimatedState(*PrimitiveComponent, TypeID, FSetMaterialTokenProducer(MaterialIndex, ExistingMaterial));
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
	UObject* Ptr = nullptr;
	if (MaterialChannel.Evaluate(Context.GetTime(), Ptr))
	{
		// If the channel has been successfully evaluated, only assign the object if it's null, or a valid material interface
		if (Ptr == nullptr || Ptr->IsA<UMaterialInterface>())
		{
			ExecutionTokens.Add(FPrimitiveMaterialExecToken(MaterialIndex, Cast<UMaterialInterface>(Ptr)));
		}
	}
}
