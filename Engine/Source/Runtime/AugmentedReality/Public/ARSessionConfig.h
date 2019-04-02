// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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

    /** Tracking of images supplied by the app. No world tracking, just images */
    Image,

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

/**
 * Tells the AR system how much of the face work to perform
 */
UENUM(BlueprintType)
enum class EARFaceTrackingUpdate : uint8
{
	/** Curves and geometry will be updated (only needed for mesh visualization) */
	CurvesAndGeo,
	/** Only the curve data is updated */
	CurvesOnly
};

UCLASS(BlueprintType, Category="AR Settings")
class AUGMENTEDREALITY_API UARSessionConfig : public UDataAsset
{
	GENERATED_BODY()

public:

	UARSessionConfig();
	
public:
	/** @see EARWorldAlignment */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	EARWorldAlignment GetWorldAlignment() const;

	/** @see SessionType */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	EARSessionType GetSessionType() const;

	/** @see PlaneDetectionMode */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	EARPlaneDetectionMode GetPlaneDetectionMode() const;

	/** @see LightEstimationMode */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	EARLightEstimationMode GetLightEstimationMode() const;

	/** @see FrameSyncMode */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	EARFrameSyncMode GetFrameSyncMode() const;

	/** @see bEnableAutomaticCameraOverlay */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	bool ShouldRenderCameraOverlay() const;

	/** @see bEnableAutomaticCameraTracking */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	bool ShouldEnableCameraTracking() const;

	/** @see bEnableAutoFocus */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	bool ShouldEnableAutoFocus() const;

	/** @see bEnableAutoFocus */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	void SetEnableAutoFocus(bool bNewValue);

	/** @see bResetCameraTracking */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	bool ShouldResetCameraTracking() const;
	
	/** @see bResetCameraTracking */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	void SetResetCameraTracking(bool bNewValue);

	/** @see bResetTrackedObjects */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	bool ShouldResetTrackedObjects() const;

	/** @see bResetTrackedObjects */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	void SetResetTrackedObjects(bool bNewValue);
	
	/** @see CandidateImages */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	const TArray<UARCandidateImage*>& GetCandidateImageList() const;

	// Add a new CandidateImage to the ARSessionConfig.
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	void AddCandidateImage(UARCandidateImage* NewCandidateImage);
    
	/** @see MaxNumSimultaneousImagesTracked */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
    int32 GetMaxNumSimultaneousImagesTracked() const;
	
	/** @see EnvironmentCaptureProbeType */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	EAREnvironmentCaptureProbeType GetEnvironmentCaptureProbeType() const;
	
	/** @see WorldMapData */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	const TArray<uint8>& GetWorldMapData() const;
	/** @see WorldMapData */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	void SetWorldMapData(TArray<uint8> WorldMapData);

	/** @see CandidateObjects */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	const TArray<UARCandidateObject*>& GetCandidateObjectList() const;
	/** @see CandidateObjects */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	void SetCandidateObjectList(const TArray<UARCandidateObject*>& InCandidateObjects);
	/** @see CandidateObjects */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	void AddCandidateObject(UARCandidateObject* CandidateObject);

	/** @see DesiredVideoFormat */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	FARVideoFormat GetDesiredVideoFormat() const;
	/** @see DesiredVideoFormat */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	void SetDesiredVideoFormat(FARVideoFormat NewFormat);

	/** @see FaceTrackingDirection */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	EARFaceTrackingDirection GetFaceTrackingDirection() const;
	/** @see FaceTrackingDirection */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	void SetFaceTrackingDirection(EARFaceTrackingDirection InDirection);
	
	/** @see FaceTrackingUpdate */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	EARFaceTrackingUpdate GetFaceTrackingUpdate() const;
	/** @see FaceTrackingUpdate */
	UFUNCTION(BlueprintCallable, Category = "AR Settings")
	void SetFaceTrackingUpdate(EARFaceTrackingUpdate InUpdate);


	bool ShouldDoHorizontalPlaneDetection() const { return bHorizontalPlaneDetection; }
	bool ShouldDoVerticalPlaneDetection() const { return bVerticalPlaneDetection; }
	
	const TArray<uint8>& GetSerializedARCandidateImageDatabase() const;	
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

	/** Whether the AR system should reset camera tracking (origin, transform) or not. Defaults to true. */
	UPROPERTY(EditAnywhere, Category="AR Settings")
	bool bResetCameraTracking;
	
	/** Whether the AR system should remove any tracked objects or not. Defaults to true. */
	UPROPERTY(EditAnywhere, Category="AR Settings")
	bool bResetTrackedObjects;
	
	/** The list of candidate images to detect within the AR camera view */
	UPROPERTY(EditAnywhere, Category="AR Settings")
	TArray<UARCandidateImage*> CandidateImages;

    /** The maximum number of images to track at the same time. Defaults to 1 */
    UPROPERTY(EditAnywhere, Category="AR Settings")
    int32 MaxNumSimultaneousImagesTracked;
	
	/** How the AR system should handle texture probe capturing */
	UPROPERTY(EditAnywhere, Category="AR Settings")
	EAREnvironmentCaptureProbeType EnvironmentCaptureProbeType;

	/** A previously saved world that is to be loaded when the session starts */
	UPROPERTY(VisibleAnywhere, Category="AR Settings")
	TArray<uint8> WorldMapData;

	/** A list of candidate objects to search for in the scene */
	UPROPERTY(EditAnywhere, Category="AR Settings")
	TArray<UARCandidateObject*> CandidateObjects;

	/**
	 * The desired video format (or the default if not supported) that this session should use if the camera is enabled
	 * Note: Call GetSupportedVideoFormats to get a list of device supported formats
	 */
	UPROPERTY(EditAnywhere, Category="AR Settings")
	FARVideoFormat DesiredVideoFormat;
	
	/** Whether to track the face as if you are looking out of the device or as a mirror */
	UPROPERTY(EditAnywhere, Category="Face AR Settings")
	EARFaceTrackingDirection FaceTrackingDirection;

	/** Whether to track the face as if you are looking out of the device or as a mirror */
	UPROPERTY(EditAnywhere, Category="Face AR Settings")
	EARFaceTrackingUpdate FaceTrackingUpdate;
	
	/** Data array for storing the cooked image database */
	UPROPERTY()
	TArray<uint8> SerializedARCandidateImageDatabase;	
};
