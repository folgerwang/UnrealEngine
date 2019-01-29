// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#if WITH_MLSDK
#include <ml_api.h>
#endif //WITH_MLSDK

#include "ARTypes.h"
#include "ARTrackable.h"
#include "ARTraceResult.h"
#include "ARSystem.h"
#include "ARPin.h"

#include "LuminARTypes.generated.h"

/// @defgroup LuminARBase
/// The base module for LuminAR plugin

#if PLATFORM_LUMIN

typedef enum
{
	LUMIN_AR_TRACKABLE_NOT_VALID,
	LUMIN_AR_TRACKABLE_PLANE,
	LUMIN_AR_TRACKABLE_POINT
}
ArTrackableType;

//TODO - Proper data?
struct ArPose
{
	FVector Pos;
	FQuat Quat;
};

struct LuminArTrackable
{
	LuminArTrackable(ArPose InPose)
		: Handle(ML_INVALID_HANDLE)
	{
	}

	MLHandle Handle;
};

struct LuminArAnchor : public LuminArTrackable
{
	LuminArAnchor(ArPose InPose, MLHandle InParentTrackable)
		: LuminArTrackable(InPose)
		, ParentTrackable(InParentTrackable)
	{}

	void Detach()
	{
		ParentTrackable = ML_INVALID_HANDLE;
	}

	MLHandle ParentTrackable;
};

typedef struct MLPlane ArPlane;
typedef struct MLPlane ArPoint;

//TODO - Add details for ArImage
struct ArImage
{
};

#endif

UENUM(BlueprintType)
enum class ELuminARAvailability : uint8
{
	/* An internal error occurred while determining Lumin AR availability. */
	UnknownError = 0,

	/* ARCore is supported, installed, and available to use. */
	SupportedInstalled = 200
};

/**
 * @ingroup LuminARBase
 * Describes the status of most LuminAR functions.
 */
UENUM(BlueprintType)
enum class ELuminARFunctionStatus : uint8
{
	/** Function returned successfully. */
	Success,
	/** Function failed due to Fatal error. */
	Fatal,
	/** Function failed due to the session isn't running. */
	SessionPaused,
	/** Function failed due to ARCore session isn't in tracking state. */
	NotTracking,
	/** Function failed due to the requested resource is exhausted. */
	ResourceExhausted,
	/** Function failed due to the requested resource isn't available yet. */
	NotAvailable,
	/** Function failed due to the function augment has invalid type. */
	InvalidType,
	/** Function failed with unknown reason. */
	Unknown
};

/**
 * @ingroup LuminARBase
 * Describes the tracking state of the current ARCore session.
 */
UENUM(BlueprintType)
enum class ELuminARTrackingState : uint8
{
	/** Tracking is valid. */
	Tracking = 0,
	/** Tracking is temporary lost but could recover in the future. */
	NotTracking = 1,
	/** Tracking is lost will not recover. */
	StoppedTracking = 2
};

/**
 * A struct describes the ARCore light estimation.
 */
USTRUCT(BlueprintType)
struct FLuminARLightEstimate
{
	GENERATED_BODY()

	/** Whether this light estimation is valid. */
	UPROPERTY(BlueprintReadOnly, Category = "LuminAR|LightEstimate")
	bool bIsValid;

	/** The average pixel intensity of the passthrough camera image. */
	UPROPERTY(BlueprintReadOnly, Category = "LuminAR|LightEstimate")
	float PixelIntensity;
	
	/**
	 * The RGB scale to match the color of the light in the real environment.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "LuminAR|LightEstimate")
	FVector RGBScaleFactor;
};

/**
 * @ingroup LuminARBase
 * Describes which channel ARLineTrace will be performed on.
 */
UENUM(BlueprintType, Category = "LuminAR|TraceChannel", meta = (Bitflags))
enum class ELuminARLineTraceChannel : uint8
{
	None = 0,
	/** Trace against feature point cloud. */
	FeaturePoint = 1,
	/** Trace against the infinite plane. */
	InfinitePlane = 2,
	/** Trace against the plane using its extent. */
	PlaneUsingExtent = 4,
	/** Trace against the plane using its boundary polygon. */
	PlaneUsingBoundaryPolygon = 8,
	/**
	 * Trace against feature point and attempt to estimate the normal of the surface centered around the trace hit point.
	 * Surface normal estimation is most likely to succeed on textured surfaces and with camera motion.
	 */
	FeaturePointWithSurfaceNormal = 16,
};
ENUM_CLASS_FLAGS(ELuminARLineTraceChannel);


