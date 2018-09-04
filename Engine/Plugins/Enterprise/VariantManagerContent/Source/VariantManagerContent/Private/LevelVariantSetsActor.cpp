// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LevelVariantSetsActor.h"

#include "LevelVariantSets.h"

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"


ALevelVariantSetsActor::ALevelVariantSetsActor(const FObjectInitializer& Init)
	: Super(Init)
{
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
	RootComponent = SceneComponent;
}

ULevelVariantSets* ALevelVariantSetsActor::GetLevelVariantSets(bool bLoad) const
{
	if (LevelVariantSets.IsValid())
	{
		ULevelVariantSets* VarSets = Cast<ULevelVariantSets>(LevelVariantSets.ResolveObject());
		if (VarSets)
		{
			return VarSets;
		}

		if (bLoad)
		{
			if (IsAsyncLoading())
			{
				LoadPackageAsync(LevelVariantSets.GetLongPackageName());
				return nullptr;
			}
			else
			{
				return Cast<ULevelVariantSets>(LevelVariantSets.TryLoad());
			}
		}
	}

	return nullptr;
}

void ALevelVariantSetsActor::SetLevelVariantSets(ULevelVariantSets* InVariantSets)
{
    LevelVariantSets = InVariantSets;
}