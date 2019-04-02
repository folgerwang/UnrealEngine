// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Windows/WindowsHWrapper.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#if WITH_WINDOWS_MIXED_REALITY
#include "Windows/AllowWindowsPlatformTypes.h"
#include "MixedRealityInterop.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

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

	/**
	* Locks the mouse cursor to the center of the screen if the hmd is worn.
	* Default is true to help guarantee mouse focus when the hmd is worn.
	* Set this to false to override the default behavior if your application requires free mouse movement.
	* locked: true to lock to center, false to not lock.
	*/
	UFUNCTION(BlueprintCallable, Category = "WindowsMixedRealityHMD")
	static void LockMouseToCenter(bool locked);
};
