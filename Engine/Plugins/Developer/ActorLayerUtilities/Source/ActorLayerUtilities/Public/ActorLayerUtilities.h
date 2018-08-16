// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ActorLayerUtilities.generated.h"

USTRUCT(BlueprintType)
struct FActorLayer
{
	GENERATED_BODY()

	/** The name of this layer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Layer)
	FName Name;
};


/**
 * Function library containing methods for interacting with editor layers
 */
UCLASS()
class ULayersBlueprintLibrary : public UBlueprintFunctionLibrary
{
public:

	GENERATED_BODY()

	/**
	 * Get all the actors in this layer
	 */
	UFUNCTION(BlueprintCallable, Category=Layer, meta=(WorldContext=WorldContextObject))
	static TArray<AActor*> GetActors(UObject* WorldContextObject, const FActorLayer& ActorLayer);
};