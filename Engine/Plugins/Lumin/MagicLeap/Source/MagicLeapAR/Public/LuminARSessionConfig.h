// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "LuminARTypes.h"
#include "PlanesComponent.h"
#include "ARSessionConfig.h"

#include "LuminARSessionConfig.generated.h"


UCLASS(BlueprintType, Category = "AR AugmentedReality")
class MAGICLEAPAR_API ULuminARSessionConfig : public UARSessionConfig
{
	GENERATED_BODY()

public:
	ULuminARSessionConfig()
		: UARSessionConfig()
		, PlaneSearchExtents(10000.0f, 10000.0f, 10000.0f)
	{
	}

	/** The maximum number of plane results that will be returned. */
	UPROPERTY(EditAnywhere, Category = "Lumin AR Settings")
	int32 MaxPlaneQueryResults = 200;

	/** The minimum area (in square cm) of planes to be returned. This value cannot be lower than 400 (lower values will be capped to this minimum). A good default value is 2500. */
	UPROPERTY(EditAnywhere, Category = "Lumin AR Settings")
	int32 MinPlaneArea = 400;

	/** Should we detect planes with any orientation (ie not just horizontal or vertical). */
	UPROPERTY(EditAnywhere, Category = "Lumin AR Settings")
	bool bArbitraryOrientationPlaneDetection = false; // default to false, for now anyway, because some other platforms do not support this.

	//UPROPERTY(EditAnywhere, Category = "Lumin AR Settings")
	//bool bGetInnerPlanes = false;

	/** The dimensions of the box within which plane results will be returned.  The box center and rotation are those of the tracking to world transform origin. */
	UPROPERTY(EditAnywhere, Category = "Lumin AR Settings")
	FVector PlaneSearchExtents;

	/** Additional Flags to apply to the plane queries. Note: the plane orientation detection settings also cause flags to be set.  It is ok to duplicate those here.*/
	UPROPERTY(EditAnywhere, Category = "Lumin AR Settings")
	TArray<EPlaneQueryFlags> PlaneQueryFlags;

	/** If true discard any 'plane' objects that come through with zero extents and only polygon edge data.*/
	UPROPERTY(EditAnywhere, Category = "Lumin AR Settings")
	bool bDiscardZeroExtentPlanes = true;

	
	static ULuminARSessionConfig* CreateARCoreSessionConfig(bool bHorizontalPlaneDetection, bool bVerticalPlaneDetection, EARLightEstimationMode InLightEstimationMode, EARFrameSyncMode InFrameSyncMode, bool bInEnableAutomaticCameraOverlay, bool bInEnableAutomaticCameraTracking)
	{
		ULuminARSessionConfig* NewARSessionConfig = NewObject<ULuminARSessionConfig>();

		NewARSessionConfig->bHorizontalPlaneDetection = bHorizontalPlaneDetection;
		NewARSessionConfig->bHorizontalPlaneDetection = bVerticalPlaneDetection;
		NewARSessionConfig->LightEstimationMode = InLightEstimationMode;
		NewARSessionConfig->FrameSyncMode = InFrameSyncMode;
		NewARSessionConfig->bEnableAutomaticCameraOverlay = bInEnableAutomaticCameraOverlay;
		NewARSessionConfig->bEnableAutomaticCameraTracking = bInEnableAutomaticCameraTracking;
		
		return NewARSessionConfig;
	};
};
