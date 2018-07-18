// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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

UCLASS( BlueprintType )
class APPLEARKIT_API UAppleARKitPlaneAnchor : public UAppleARKitAnchor
{
	GENERATED_BODY()

public: 

	/**
	 * The center of the plane in the anchor’s coordinate space.
	 */
	UFUNCTION( BlueprintPure, Category = "AppleARKitPlaneAnchor" )
	FVector GetCenter() const;

	/**
	 * The extent of the plane in the anchor’s coordinate space.
	 */
	UFUNCTION( BlueprintPure, Category = "AppleARKitPlaneAnchor")
	FVector GetExtent() const;

	UFUNCTION( BlueprintPure, Category = "AppleARKitPlaneAnchor")
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
