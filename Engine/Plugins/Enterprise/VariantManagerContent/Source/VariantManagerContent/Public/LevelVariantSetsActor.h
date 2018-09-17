// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "GameFramework/Actor.h"

#include "LevelVariantSetsActor.generated.h"


UCLASS(hideCategories=(Rendering, Physics, LOD, Activation, Input))
class VARIANTMANAGERCONTENT_API ALevelVariantSetsActor : public AActor
{
public:

	GENERATED_BODY()

	ALevelVariantSetsActor(const FObjectInitializer& Init);

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Variants", meta=(AllowedClasses="LevelVariantSets"))
	FSoftObjectPath LevelVariantSets;

public:

	UFUNCTION(BlueprintCallable, Category="Variants")
	ULevelVariantSets* GetLevelVariantSets(bool bLoad = false) const;

	UFUNCTION(BlueprintCallable, Category="Variants")
	void SetLevelVariantSets(ULevelVariantSets* InVariantSets);
};
