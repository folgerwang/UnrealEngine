// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AppleARKitAvailability.h"

// ARKit
#if SUPPORTS_ARKIT_1_0
	#import <ARKit/ARKit.h>
#endif

// AppleARKit
#include "AppleARKitAnchor.h"
#include "AppleARKitPlaneAnchor.generated.h"

UCLASS( BlueprintType, Deprecated )
class APPLEARKIT_API UDEPRECATED_AppleARKitPlaneAnchor : public UDEPRECATED_AppleARKitAnchor
{
	GENERATED_BODY()

public: 

	/**
	 * The center of the plane in the anchor’s coordinate space.
	 */
	UFUNCTION( BlueprintPure, Category = "AppleARKit|PlaneAnchor" )
	FVector GetCenter() const;

	/**
	 * The extent of the plane in the anchor’s coordinate space.
	 */
	UFUNCTION( BlueprintPure, Category = "AppleARKit|PlaneAnchor")
	FVector GetExtent() const;

	UFUNCTION( BlueprintPure, Category = "AppleARKit|PlaneAnchor")
	FTransform GetTransformToCenter() const;

#if SUPPORTS_ARKIT_1_0

	virtual void Update_DelegateThread( ARAnchor* Anchor ) override;

#endif

private:

	/**
	 * The center of the plane in the anchor’s coordinate space.
	 */
	FVector Center;

	/**
	 * The extent of the plane in the anchor’s coordinate space.
	 */
	FVector Extent;
};
