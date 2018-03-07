// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MixedRealityUtilLibrary.generated.h"

class UCameraComponent;
class APawn;
class APlayerController;
class UMaterialBillboardComponent;

UCLASS()
class MIXEDREALITYFRAMEWORK_API UMixedRealityUtilLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="MixedReality|Utils")
	static void SetMaterialBillboardSize(UMaterialBillboardComponent* Target, float NewSizeX, float NewSizeY);

public:
	static APawn* FindAssociatedPlayerPawn(AActor* ActorInst);
	static USceneComponent* FindAssociatedHMDRoot(AActor* ActorInst);

	static USceneComponent* GetHMDRootComponent(const UObject* WorldContextObject, int32 PlayerIndex);
	static USceneComponent* GetHMDRootComponent(const APawn* PlayerPawn);
	static UCameraComponent* GetHMDCameraComponent(const APawn* PlayerPawn);
};
