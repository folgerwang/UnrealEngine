// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "AssetData.h"
#include "TakesCoreBlueprintLibrary.generated.h"


UCLASS()
class TAKESCORE_API UTakesCoreBlueprintLibrary : public UBlueprintFunctionLibrary
{
public:

	GENERATED_BODY()

	/**
	 * Compute the next unused sequential take number for the specified slate
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder")
	static int32 ComputeNextTakeNumber(const FString& Slate);


	/**
	 * Find all the existing takes that were recorded with the specified slate
	 *
	 * @param Slate        The slate to filter by
	 * @param TakeNumber   The take number to filter by. <=0 denotes all takes
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder")
	static TArray<FAssetData> FindTakes(const FString& Slate, int32 TakeNumber = 0);
};
