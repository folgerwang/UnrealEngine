// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Windows/WindowsHWrapper.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MixedRealityInterop.h"

#include "WindowsMixedRealityFunctionLibrary.Generated.h"

/**
* Windows Mixed Reality Extensions Function Library
*/
UCLASS()
class WINDOWSMIXEDREALITYHMD_API UWindowsMixedRealityFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	/**
	* Returns name of WindowsMR device type.
	*/
	UFUNCTION(BlueprintPure, Category = "WindowsMixedRealityHMD")
	static FString GetVersionString();

	/**
	* Sets game context to immersive or slate.
	* immersive: true for immersive context, false for slate.
	*/
	UFUNCTION(BlueprintCallable, Category = "WindowsMixedRealityHMD")
	static void ToggleImmersive(bool immersive);

	/**
	* Returns true if currently rendering immersive, or false if rendering as a slate.
	*/
	UFUNCTION(BlueprintPure, Category = "WindowsMixedRealityHMD")
	static bool IsCurrentlyImmersive();
};
