// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AppleARKitAvailability.h"

#if SUPPORTS_ARKIT_1_0
	#import <ARKit/ARKit.h>
#endif


#include "ARSystem.h"
#include "AppleARKitHitTestResult.generated.h"

/**
 * Option set of hit-test result types.
 */
UENUM( BlueprintType, Category="AppleARKit", meta=(Bitflags) )
enum class EAppleARKitHitTestResultType : uint8
{
	None = 0,

    /** Result type from intersecting the nearest feature point. */
    FeaturePoint = 1 UMETA(DisplayName = "Feature Point"),
    
    /** Result type from intersecting a horizontal plane estimate, determined for the current frame. */
    EstimatedHorizontalPlane = 2 UMETA(DisplayName = "Estimated Horizontal Plane"),
    
    /** Result type from intersecting with an existing plane anchor. */
    ExistingPlane = 4 UMETA(DisplayName = "Existing Plane"),
    
    /** Result type from intersecting with an existing plane anchor. */
    ExistingPlaneUsingExtent = 8 UMETA(DisplayName= "Existing Plane Using Extent")
};
ENUM_CLASS_FLAGS(EAppleARKitHitTestResultType);

#if SUPPORTS_ARKIT_1_0

/** Conversion function from ARKit native ARHitTestResultType */
EAppleARKitHitTestResultType ToEAppleARKitHitTestResultType(ARHitTestResultType InTypes);

#endif

/**
 * A result of an intersection found during a hit-test.
 */
USTRUCT( BlueprintType, Category="AppleARKit")
struct APPLEARKIT_API FAppleARKitHitTestResult
{
    GENERATED_BODY();

    // Default constructor
	FAppleARKitHitTestResult()
		: Type(EAppleARKitHitTestResultType::None)
		, Distance(0.f)
	{};

#if SUPPORTS_ARKIT_1_0

	/** 
	 * This is a conversion copy-constructor that takes a raw ARHitTestResult and fills this 
	 * structs members with the UE4-ified versions of ARHitTestResult's properties.
	 */ 
	FAppleARKitHitTestResult( ARHitTestResult* InARHitTestResult, class UDEPRECATED_AppleARKitAnchor* InAnchor = nullptr, float WorldToMetersScale = 100.0f );

#endif

	/**
	 * The type of the hit-test result.
	 */
    UPROPERTY( BlueprintReadOnly, Category = "AppleARKitHitTestResult" )
    EAppleARKitHitTestResultType Type;
    
	/**
	 * The distance from the camera to the intersection in meters.
	 */
    UPROPERTY( BlueprintReadOnly, Category = "AppleARKitHitTestResult")
	float Distance;

	/**
	 * The transformation matrix that defines the intersection's rotation, translation and scale
	 * relative to the world.
	 */
    UPROPERTY( BlueprintReadOnly, Category = "AppleARKitHitTestResult")
	FTransform Transform;

	/**
	 * The anchor that the hit-test intersected.
	 * 
	 * An anchor will only be provided for existing plane result types.
	 */
	UPROPERTY()
	class UDEPRECATED_AppleARKitAnchor* Anchor_DEPRECATED = nullptr;
};
