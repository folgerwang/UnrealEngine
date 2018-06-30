// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AppleARKitAvailability.h"
#include "ARTrackable.h"
#include "AppleARKitTrackable.generated.h"

class UAppleARKitEnvironmentCaptureProbeTexture;

//@joeg -- Added support for environment capture
UCLASS(BlueprintType)
class APPLEARKIT_API UAppleARKitEnvironmentCaptureProbe :
	public UAREnvironmentCaptureProbe
{
	GENERATED_BODY()

public:
	UAppleARKitEnvironmentCaptureProbe();
	
#if PLATFORM_IOS
	/** Updates the current capture with the new metal texture. This will trigger a render resource update if the textures have changed */
	void UpdateEnvironmentCapture(const TSharedRef<FARSystemBase, ESPMode::ThreadSafe>& InTrackingSystem, uint32 FrameNumber, double Timestamp, const FTransform& InLocalToTrackingTransform, const FTransform& InAlignmentTransform, FVector InExtent, id<MTLTexture> InMetalTexture);
#endif

private:
	/** The cube map of the reflected environment */
	UPROPERTY()
	UAppleARKitEnvironmentCaptureProbeTexture* ARKitEnvironmentTexture;
};
