// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "ARTypes.h"
#include "ARTrackable.h"
#include "ARTraceResult.h"
#include "ARSystem.h"
#include "ARPin.h"

#include "GoogleARCoreTypes.generated.h"

/// @defgroup GoogleARCoreBase Google ARCore Base
/// The base module for Google ARCore plugin

#if PLATFORM_ANDROID
// Include <Camera/NdkCameraMetadata.h> to use this type.
typedef struct ACameraMetadata ACameraMetadata;

// Forward decalare type defined in arcore_c_api.h
typedef struct ArTrackable_ ArTrackable;
typedef struct ArPlane_ ArPlane;
typedef struct ArPoint_ ArPoint;
typedef struct ArPointCloud_ ArPointCloud;
typedef struct ArAnchor_ ArAnchor;
#endif

UENUM(BlueprintType)
enum class EGoogleARCoreAvailability : uint8
{
	/* An internal error occurred while determining ARCore availability. */
	UnknownError = 0,
	/* ARCore is not installed, and a query has been issued to check if ARCore is is supported. */
	UnknownChecking = 1,
	/*
	 * ARCore is not installed, and the query to check if ARCore is supported
	 * timed out. This may be due to the device being offline.
	 */
	UnknownTimedOut = 2,
	/* ARCore is not supported on this device.*/
	UnsupportedDeviceNotCapable = 100,
	/* The device and Android version are supported, but the ARCore APK is not installed.*/
	SupportedNotInstalled = 201,
	/*
	 * The device and Android version are supported, and a version of the
	 * ARCore APK is installed, but that ARCore APK version is too old.
	 */
	SupportedApkTooOld = 202,
	/* ARCore is supported, installed, and available to use. */
	SupportedInstalled = 203
};

UENUM(BlueprintType)
enum class EGoogleARCoreInstallStatus : uint8
{
	/* The requested resource is already installed.*/
	Installed = 0,
	/* Installation of the resource was requested. The current activity will be paused. */
	Requrested = 1,
};

UENUM(BlueprintType)
enum class EGoogleARCoreInstallRequestResult : uint8
{
	/* The ARCore APK is installed*/
	Installed,
	/* ARCore APK install request failed because the device is not compatible. */
	DeviceNotCompatible,
	/* ARCore APK install request failed because the current version of android is too old to support ARCore. */
	UserDeclinedInstallation,
	/* ARCore APK install request failed because unknown error happens while checking or requesting installation. */
	FatalError
};

/**
 * @ingroup GoogleARCoreBase
 * Describes the status of most ARCore functions.
 */
UENUM(BlueprintType)
enum class EGoogleARCoreFunctionStatus : uint8
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
	/** Function failed due to ARCore session hasn't started or the requested resource isn't available yet. */
	NotAvailable,
	/** Function failed due to the function augment has invalid type. */
	InvalidType,
	/** Function failed due to it is invoked at an illegal or inappropriate time. */
	IllegalState,
	/** Function failed with unknown reason. */
	Unknown
};

/**
 * @ingroup GoogleARCoreBase
 * Describes the tracking state of the current ARCore session.
 */
UENUM(BlueprintType)
enum class EGoogleARCoreTrackingState : uint8
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
struct FGoogleARCoreLightEstimate
{
	GENERATED_BODY()

	/** Whether this light estimation is valid. */
	UPROPERTY(BlueprintReadOnly, Category = "GoogleARCore|LightEstimate")
	bool bIsValid;

	/** The average pixel intensity of the passthrough camera image. */
	UPROPERTY(BlueprintReadOnly, Category = "GoogleARCore|LightEstimate")
	float PixelIntensity;

	/**
	 * The RGB scale to match the color of the light in the real environment.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "GoogleARCore|LightEstimate")
	FVector RGBScaleFactor;
};

/**
 * @ingroup GoogleARCoreBase
 * Describes which channel ARLineTrace will be performed on.
 */
UENUM(BlueprintType, Category = "GoogleARCore|TraceChannel", meta = (Bitflags))
enum class EGoogleARCoreLineTraceChannel : uint8
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
	/** Trace against augmented images. */
	AugmentedImage = 32,
};
ENUM_CLASS_FLAGS(EGoogleARCoreLineTraceChannel);

/**
 * Camera configuration from ARCore.
 */
USTRUCT(BlueprintType)
struct GOOGLEARCOREBASE_API FGoogleARCoreCameraConfig
{
	GENERATED_BODY()

	/**
	 * CPU-accessible camera image resolution.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "GoogleARCore|CameraConfig")
	FIntPoint CameraImageResolution;

	/**
	 * Texture resolution for the camera image accessible to the
	 * graphics API and shaders.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "GoogleARCore|CameraConfig")
	FIntPoint CameraTextureResolution;

	/**
	 * Comparison operator.
	 *
	 * @param OtherConfig	The other configuration to compare this against.
	 * @return True if this configuration is identical to OtherConfig.
	 */
	bool operator==(const FGoogleARCoreCameraConfig& OtherConfig) const
	{
		return CameraImageResolution == OtherConfig.CameraImageResolution && CameraTextureResolution == OtherConfig.CameraTextureResolution;
	}
};

/**
 * Delegate to call on camera configuration.
 */
class GOOGLEARCOREBASE_API FGoogleARCoreDelegates
{
public:
/// @cond EXCLUDE_FROM_DOXYGEN
	DECLARE_MULTICAST_DELEGATE_OneParam(FGoogleARCoreOnConfigCameraDelegate, const TArray<FGoogleARCoreCameraConfig>&);
/// @endcond

	/**
	 * A delegate can be bound through C++. It will be called before
	 * ARSession started and returns a list of supported ARCore camera
	 * configurations. Bind this delegate if you want to choose a
	 * specific camera config in your app. Call
	 * UGoogleARCoreSessionFunctionLibrary::ConfigARCoreCamera after
	 * the delegate is triggered.
	 */
	static FGoogleARCoreOnConfigCameraDelegate OnCameraConfig;
};

/**
 * Manager for ARCore delegates.
 */
UCLASS(BlueprintType, Category = "AR AugmentedReality")
class GOOGLEARCOREBASE_API UGoogleARCoreEventManager : public UObject
{
	GENERATED_BODY()
public:
/// @cond EXCLUDE_FROM_DOXYGEN
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGoogleARCoreOnConfigCameraDynamicDelegate, const TArray<FGoogleARCoreCameraConfig>&, SupportedCameraConfig);
/// @endcond

	UGoogleARCoreEventManager()
	{
		RegisterDelegates();
	}

	~UGoogleARCoreEventManager()
	{
		UnregisterDelegates();
	}

	/**
	 * A dynamic delegate can be assigned through blueprint. Will be called before ARSession started and returns
	 * an array of supported ARCore camera config.
	 *
	 * The array will always return 3 camera configs. The GPU texture resolutions
	 * are the same in all three configs. Currently, most devices provide GPU
	 * texture resolution of 1920 x 1080, but devices might provide higher or lower
	 * resolution textures, depending on device capabilities. The CPU image
	 * resolutions returned are VGA, 720p, and a resolution matching the GPU
	 * texture.
	 *
	 * Bind this delegate if you want to choose a specific camera config in your app. Call
	 * UGoogleARCoreSessionFunctionLibrary::ConfigARCoreCamera after the delegate is triggered.
	 */
	UPROPERTY(BlueprintAssignable)
	FGoogleARCoreOnConfigCameraDynamicDelegate OnConfigCamera;

private:
	void RegisterDelegates()
	{
		FGoogleARCoreDelegates::OnCameraConfig.AddUObject(this, &UGoogleARCoreEventManager::OnConfigCamera_Handler);
	}
	void UnregisterDelegates()
	{
		FGoogleARCoreDelegates::OnCameraConfig.RemoveAll(this);
	}

	void OnConfigCamera_Handler(const TArray<FGoogleARCoreCameraConfig>& SupportedCameraConfig)
	{
		OnConfigCamera.Broadcast(SupportedCameraConfig);
	}
};

class FGoogleARCoreSession;
/**
 * A UObject that contains a set of observed 3D points and confidence values.
 */
UCLASS(BlueprintType)
class GOOGLEARCOREBASE_API UGoogleARCorePointCloud : public UObject
{
	friend class FGoogleARCoreFrame;
	friend class FGoogleARCoreSession;

	GENERATED_BODY()
public:
	/** Destructor */
	~UGoogleARCorePointCloud();

	/** Returns the timestamp in nanosecond when this point cloud was observed. */
	int64 GetUpdateTimestamp();

	/** Checks if this point cloud has been updated in this frame. */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|PointCloud")
	bool IsUpdated();

	/** Returns the number of point inside this point cloud. */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|PointCloud")
	int GetPointNum();

	/** Returns the point position in Unreal world space and it's confidence value from 0 ~ 1. */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|PointCloud")
	void GetPoint(int Index, FVector& OutWorldPosition, float& OutConfidence);

	/**
	 * Returns the point Id of the point at the given index.
	 *
	 * Each point has a unique identifier (within a session) that is persistent
	 * across frames. That is, if a point from point cloud 1 has the same id as the
	 * point from point cloud 2, then it represents the same point in space.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|PointCloud")
	int GetPointId(int Index);

	/** Returns the point position in Unreal AR Tracking space. */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|PointCloud")
	void GetPointInTrackingSpace(int Index, FVector& OutTrackingSpaceLocation, float& OutConfidence);

	/** Release PointCloud's resources back to ArCore. Data will not be available after releasePointCloud is called. */
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|PointCloud")
	void ReleasePointCloud();

private:
	TWeakPtr<FGoogleARCoreSession> Session;
	bool bIsUpdated = false;
#if PLATFORM_ANDROID
	ArPointCloud* PointCloudHandle;
#endif
};

/**
* Helper class used to expose FGoogleARCoreSessionConfig setting in the Editor plugin settings.
*/
UCLASS(config = Engine, defaultconfig)
class GOOGLEARCOREBASE_API UGoogleARCoreEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	/** Check this option if you app require ARCore to run on Android. */
	UPROPERTY(EditAnywhere, config, Category = "ARCore Plugin Settings", meta = (ShowOnlyInnerProperties))
	bool bARCoreRequiredApp = true;
};
