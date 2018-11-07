// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "GameFramework/Actor.h"

#include "LevelVariantSetsActor.generated.h"


class UVariantSet;


UCLASS(hideCategories=(Rendering, Physics, LOD, Activation, Input))
class VARIANTMANAGERCONTENT_API ALevelVariantSetsActor : public AActor
{
public:

	GENERATED_BODY()

	ALevelVariantSetsActor(const FObjectInitializer& Init);

	// AActor interface
	virtual void BeginPlay() override;
	//~ End AActor interface

	UFUNCTION(BlueprintCallable, Category="LevelVariantSets")
	ULevelVariantSets* GetLevelVariantSets(bool bLoad = false) const;

	UFUNCTION(BlueprintCallable, Category="LevelVariantSets")
	void SetLevelVariantSets(ULevelVariantSets* InVariantSets);

	UFUNCTION(BlueprintCallable, Category="LevelVariantSets")
	bool SwitchOnVariantByName(FString VariantSetName, FString VariantName);

	UFUNCTION(BlueprintCallable, Category="LevelVariantSets")
	bool SwitchOnVariantByIndex(int32 VariantSetIndex, int32 VariantIndex);

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LevelVariantSets", meta=(AllowedClasses="LevelVariantSets"))
	FSoftObjectPath LevelVariantSets;
};
