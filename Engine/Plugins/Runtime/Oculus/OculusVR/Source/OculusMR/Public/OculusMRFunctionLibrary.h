// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OculusMRFunctionLibrary.generated.h"

class USceneComponent;
class UOculusMR_Settings;
struct FTrackedCamera;
namespace OculusHMD
{
	class FOculusHMD;
}

UCLASS()
class OCULUSMR_API UOculusMRFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	// Get the OculusMR settings object
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|MR", meta = (DisplayName = "Get Oculus MR Settings"))
	static UOculusMR_Settings* GetOculusMRSettings();

	// Get the component that the OculusMR camera is tracking
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|MR")
	static USceneComponent* GetTrackingReferenceComponent();
 
	// Set the component for the OculusMR camera to track
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|MR")
	static void SetTrackingReferenceComponent(USceneComponent* Component);

public:

	static class OculusHMD::FOculusHMD* GetOculusHMD();

	/** Retrieve an array of all (calibrated) tracked cameras which were calibrated through the CameraTool */
	static void GetAllTrackedCamera(TArray<FTrackedCamera>& TrackedCameras, bool bCalibratedOnly = true);

	static bool GetTrackingReferenceLocationAndRotationInWorldSpace(USceneComponent* TrackingReferenceComponent, FVector& TRLocation, FRotator& TRRotation);
};
