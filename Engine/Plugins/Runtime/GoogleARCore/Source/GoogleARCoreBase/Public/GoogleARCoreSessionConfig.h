// Copyright 2017 Google Inc.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "GoogleARCoreTypes.h"
#include "ARSessionConfig.h"

#include "GoogleARCoreSessionConfig.generated.h"


UCLASS(BlueprintType, Category = "AR AugmentedReality")
class GOOGLEARCOREBASE_API UGoogleARCoreSessionConfig : public UARSessionConfig
{
	GENERATED_BODY()

	// We keep the type here so that we could extends ARCore specific session config later.
	
public:
	static UGoogleARCoreSessionConfig* CreateARCoreSessionConfig(bool bHorizontalPlaneDetection, bool bVerticalPlaneDetection, EARLightEstimationMode InLightEstimationMode, EARFrameSyncMode InFrameSyncMode, bool bInEnableAutomaticCameraOverlay, bool bInEnableAutomaticCameraTracking)
	{
		UGoogleARCoreSessionConfig* NewARCoreConfig = NewObject<UGoogleARCoreSessionConfig>();
		NewARCoreConfig->bHorizontalPlaneDetection = bHorizontalPlaneDetection;
		NewARCoreConfig->bHorizontalPlaneDetection = bVerticalPlaneDetection;
		NewARCoreConfig->LightEstimationMode = InLightEstimationMode;
		NewARCoreConfig->FrameSyncMode = InFrameSyncMode;
		NewARCoreConfig->bEnableAutomaticCameraOverlay = bInEnableAutomaticCameraOverlay;
		NewARCoreConfig->bEnableAutomaticCameraTracking = bInEnableAutomaticCameraTracking;
		
		return NewARCoreConfig;
	};
};
