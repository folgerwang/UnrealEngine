// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ARTrackable.h"
#include "Engine/DataAsset.h"

#include "ARSessionConfig.generated.h"

UENUM(BlueprintType, Category="AR AugmentedReality", meta=(Experimental))
enum class EARWorldAlignment : uint8
{
	/** Aligns the world with gravity that is defined by vector (0, -1, 0) */
	Gravity,

	/**
	 * Aligns the world with gravity that is defined by the vector (0, -1, 0)
	 * and heading (w.r.t. True North) that is given by the vector (0, 0, -1)
	 */
	GravityAndHeading,

	/** Aligns the world with the camera's orientation, which is best for Face AR */
	Camera
};

UENUM(BlueprintType, Category = "AR AugmentedReality", meta = (Experimental))
enum class EARSessionType : uint8
{
	/** AR tracking is not supported */
	None,

	/** AR session used to track orientation of the device only */
	Orientation,

	/** AR meant to overlay onto the world with tracking */
	World,

	/** AR meant to overlay onto a face */
	Face,

//@joeg -- Added image tracking support
    /** Tracking of images supplied by the app. No world tracking, just images */
    Image,

//@joeg -- Object scanning support
	/** A session used to scan objects for object detection in a world tracking session */
	ObjectScanning
};

UENUM(BlueprintType, Category = "AR AugmentedReality", meta = (Experimental, Bitflags))
enum class EARPlaneDetectionMode : uint8
{
	None = 0,
	
	/* Detect Horizontal Surfaces */
	HorizontalPlaneDetection = 1,

	/* Detects Vertical Surfaces */
	VerticalPlaneDetection = 2
};
ENUM_CLASS_FLAGS(EARPlaneDetectionMode);

UENUM(BlueprintType, Category = "AR AugmentedReality", meta = (Experimental))
enum class EARLightEstimationMode : uint8
{
	/** Light estimation disabled. */
	None = 0,
	/** Enable light estimation for ambient intensity; returned as a UARBasicLightEstimate */
	AmbientLightEstimate = 1,
	/**
	* Enable directional light estimation of environment with an additional key light.
	* Currently not supported.
	*/
	DirectionalLightEstimate = 2
};

UENUM(BlueprintType, Category = "AR AugmentedReality", meta = (Experimental))
enum class EARFrameSyncMode : uint8
{
	/** Unreal tick will be synced with the camera image update rate. */
	SyncTickWithCameraImage = 0,
	/** Unreal tick will not related to the camera image update rate. */
	SyncTickWithoutCameraImage = 1,
};

//@joeg -- Added texture probe settings
/**
 * Tells the AR system what type of environmental texture capturing to perform
 */
UENUM(BlueprintType)
enum class EAREnvironmentCaptureProbeType : uint8
{
	/** No capturing will happen */
	None,
	/** Capturing will be manual with the app specifying where the probes are and their size */
	Manual,
	/** Capturing will be automatic with probes placed by the AR system */
	Automatic
};

UCLASS(BlueprintType, Category="AR AugmentedReality")
class AUGMENTEDREALITY_API UARSessionConfig : public UDataAsset
{
	GENERATED_BODY()

public:

	UARSessionConfig();
	
public:
	/** @see EARWorldAlignment */
	EARWorldAlignment GetWorldAlignment() const;

	/** @see SessionType */
	EARSessionType GetSessionType() const;

	/** @see PlaneDetectionMode */
	EARPlaneDetectionMode GetPlaneDetectionMode() const;

	/** @see LightEstimationMode */
	EARLightEstimationMode GetLightEstimationMode() const;

	/** @see FrameSyncMode */
	EARFrameSyncMode GetFrameSyncMode() const;

	/** @see bEnableAutomaticCameraOverlay */
	bool ShouldRenderCameraOverlay() const;

	/** @see bEnableAutomaticCameraTracking */
	bool ShouldEnableCameraTracking() const;

	/** @see bEnableAutoFocus */
	bool ShouldEnableAutoFocus() const;

	/** @see CandidateImages */
	const TArray<UARCandidateImage*>& GetCandidateImageList() const;
    
//@joeg -- Added image tracking support
	/** @see MaxNumSimultaneousImagesTracked */
    int32 GetMaxNumSimultaneousImagesTracked() const;
	
//@joeg -- Added environmental texture probe support
	/** @see EnvironmentCaptureProbeType */
	EAREnvironmentCaptureProbeType GetEnvironmentCaptureProbeType() const;
	
//@joeg -- Added for load/save of worlds
	/** @see WorldMapData */
	const TArray<uint8>& GetWorldMapData() const;
	/** @see WorldMapData */
	void SetWorldMapData(TArray<uint8> WorldMapData);

//@joeg -- For object detection
	const TArray<UARCandidateObject*>& GetCandidateObjectList() const;
	void AddCandidateObject(UARCandidateObject* CandidateObject);
	
private:
	//~ UObject interface
	virtual void Serialize(FArchive& Ar) override;
	//~ UObject interface

protected:
	UPROPERTY(EditAnywhere, Category = "AR Settings")
	/** @see EARWorldAlignment */
	EARWorldAlignment WorldAlignment;

	/** @see EARSessionType */
	UPROPERTY(EditAnywhere, Category = "AR Settings")
	EARSessionType SessionType;

	/** @see EARPlaneDetectionMode */
	UPROPERTY()
	EARPlaneDetectionMode PlaneDetectionMode_DEPRECATED;
	
	/** Should we detect flat horizontal surfaces: e.g. table tops, windows sills */
	UPROPERTY(EditAnywhere, Category = "AR Settings")
	bool bHorizontalPlaneDetection;
	
	/** Should we detect flat vertical surfaces: e.g. paintings, monitors, book cases */
	UPROPERTY(EditAnywhere, Category = "AR Settings")
	bool bVerticalPlaneDetection;

	/** Whether the camera should use autofocus or not (can cause subtle shifts in position for small objects at macro camera distance) */
	UPROPERTY(EditAnywhere, Category = "AR Settings")
	bool bEnableAutoFocus;

	/** @see EARLightEstimationMode */
	UPROPERTY(EditAnywhere, Category = "AR Settings")
	EARLightEstimationMode LightEstimationMode;

	/** @see EARFrameSyncMode */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "AR Settings")
	EARFrameSyncMode FrameSyncMode;

	/** Whether the AR camera feed should be drawn as an overlay or not. Defaults to true. */
	UPROPERTY(EditAnywhere, Category="AR Settings")
	bool bEnableAutomaticCameraOverlay;

	/** Whether the game camera should track the device movement or not. Defaults to true. */
	UPROPERTY(EditAnywhere, Category="AR Settings")
	bool bEnableAutomaticCameraTracking;

	/** The list of candidate images to detect within the AR camera view */
	UPROPERTY(EditAnywhere, Category="AR Settings")
	TArray<UARCandidateImage*> CandidateImages;

//@joeg -- Added image tracking support
    /** The maximum number of images to track at the same time. Defaults to 1 */
    UPROPERTY(EditAnywhere, Category="AR Settings")
    int32 MaxNumSimultaneousImagesTracked;
	
//@joeg -- Added environmental texture probe support
	/** How the AR system should handle texture probe capturing */
	UPROPERTY(EditAnywhere, Category="AR Settings")
	EAREnvironmentCaptureProbeType EnvironmentCaptureProbeType;

//@joeg -- For loading a saved world
	/** A previously saved world that is to be loaded when the session starts */
	UPROPERTY(VisibleAnywhere, Category="AR Settings")
	TArray<uint8> WorldMapData;

//@joeg -- For object detection
	/** A list of candidate objects to search for in the scene */
	UPROPERTY(EditAnywhere, Category="AR Settings")
	TArray<UARCandidateObject*> CandidateObjects;
};
