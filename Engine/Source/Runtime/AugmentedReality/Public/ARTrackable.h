// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ARTypes.h"
#include "ARTrackable.generated.h"

class FARSupportInterface ;
class UAREnvironmentCaptureProbeTexture;

UCLASS(BlueprintType)
class AUGMENTEDREALITY_API UARTrackedGeometry : public UObject
{
	GENERATED_BODY()
	
public:
	UARTrackedGeometry();
	
	void InitializeNativeResource(IARRef* InNativeResource);

	virtual void DebugDraw( UWorld* World, const FLinearColor& OutlineColor, float OutlineThickness, float PersistForSeconds = 0.0f) const;
	
	void UpdateTrackedGeometry(const TSharedRef<FARSupportInterface , ESPMode::ThreadSafe>& InTrackingSystem, uint32 FrameNumber, double Timestamp, const FTransform& InLocalToTrackingTransform, const FTransform& InAlignmentTransform );
	
	void UpdateTrackingState( EARTrackingState NewTrackingState );
	
	void UpdateAlignmentTransform( const FTransform& NewAlignmentTransform );
	
	void SetDebugName( FName InDebugName );

	IARRef* GetNativeResource();
	
	UFUNCTION(BlueprintPure, Category="AR AugmentedReality|Tracked Geometry")
	FTransform GetLocalToWorldTransform() const;
	
	UFUNCTION(BlueprintPure, Category="AR AugmentedReality|Tracked Geometry")
	FTransform GetLocalToTrackingTransform() const;
	
	FTransform GetLocalToTrackingTransform_NoAlignment() const;
	
	UFUNCTION(BlueprintPure, Category="AR AugmentedReality|Tracked Geometry")
	EARTrackingState GetTrackingState() const;
	void SetTrackingState(EARTrackingState NewState);

	UFUNCTION(BlueprintPure, Category="AR AugmentedReality|Tracked Geometry")
	bool IsTracked() const;

	UFUNCTION(BlueprintPure, Category="AR AugmentedReality|Tracked Geometry")
	FName GetDebugName() const;
	
	UFUNCTION(BlueprintPure, Category="AR AugmentedReality|Tracked Geometry")
	int32 GetLastUpdateFrameNumber() const;
	
	UFUNCTION(BlueprintPure, Category="AR AugmentedReality|Tracked Geometry")
	float GetLastUpdateTimestamp() const;
	
protected:
	TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe> GetARSystem() const;
	
	UPROPERTY()
	FTransform LocalToTrackingTransform;
	
	UPROPERTY()
	FTransform LocalToAlignedTrackingTransform;
	
	UPROPERTY()
	EARTrackingState TrackingState;

	/** A pointer to the native resource in the native AR system */
	TUniquePtr<IARRef> NativeResource;
	
private:
	TWeakPtr<FARSupportInterface , ESPMode::ThreadSafe> ARSystem;
	
	/** The frame number this tracked geometry was last updated on */
	uint32 LastUpdateFrameNumber;
	
	/** The time reported by the AR system that this object was last updated */
	double LastUpdateTimestamp;
	
	/** A unique name that can be used to identify the anchor for debug purposes */
	FName DebugName;
};

UCLASS(BlueprintType)
class AUGMENTEDREALITY_API UARPlaneGeometry : public UARTrackedGeometry
{
	GENERATED_BODY()
	
public:

	void UpdateTrackedGeometry(const TSharedRef<FARSupportInterface , ESPMode::ThreadSafe>& InTrackingSystem, uint32 FrameNumber, double Timestamp, const FTransform& InLocalToTrackingTransform, const FTransform& InAlignmentTransform, const FVector InCenter, const FVector InExtent );
	
	void UpdateTrackedGeometry(const TSharedRef<FARSupportInterface , ESPMode::ThreadSafe>& InTrackingSystem, uint32 FrameNumber, double Timestamp, const FTransform& InLocalToTrackingTransform, const FTransform& InAlignmentTransform, const FVector InCenter, const FVector InExtent, const TArray<FVector>& InBoundingPoly, UARPlaneGeometry* InSubsumedBy);
	
	virtual void DebugDraw( UWorld* World, const FLinearColor& OutlineColor, float OutlineThickness, float PersistForSeconds = 0.0f) const override;
	
public:
	UFUNCTION(BlueprintPure, Category="AR AugmentedReality|Plane Geometry")
	FVector GetCenter() const { return Center; }
	
	UFUNCTION(BlueprintPure, Category="AR AugmentedReality|Plane Geometry")
	FVector GetExtent() const { return Extent; }
	
	UFUNCTION(BlueprintPure, Category = "AR AugmentedReality|Plane Geometry")
	TArray<FVector> GetBoundaryPolygonInLocalSpace() const { return BoundaryPolygon; }

	UFUNCTION(BlueprintPure, Category = "AR AugmentedReality|Plane Geometry")
	UARPlaneGeometry* GetSubsumedBy() const { return SubsumedBy; };

private:
	UPROPERTY()
	FVector Center;
	
	UPROPERTY()
	FVector Extent;
	
	TArray<FVector> BoundaryPolygon;

	// Used by ARCore Only
	UPROPERTY()
	UARPlaneGeometry* SubsumedBy = nullptr;
};

UCLASS(BlueprintType)
class AUGMENTEDREALITY_API UARTrackedPoint : public UARTrackedGeometry
{
	GENERATED_BODY()

public:
	virtual void DebugDraw(UWorld* World, const FLinearColor& OutlineColor, float OutlineThickness, float PersistForSeconds = 0.0f) const override;

	void UpdateTrackedGeometry(const TSharedRef<FARSupportInterface , ESPMode::ThreadSafe>& InTrackingSystem, uint32 FrameNumber, double Timestamp, const FTransform& InLocalToTrackingTransform, const FTransform& InAlignmentTransform);
};

UCLASS(BlueprintType)
class AUGMENTEDREALITY_API UARTrackedImage : public UARTrackedGeometry
{
	GENERATED_BODY()

public:
	virtual void DebugDraw(UWorld* World, const FLinearColor& OutlineColor, float OutlineThickness, float PersistForSeconds = 0.0f) const override;

	void UpdateTrackedGeometry(const TSharedRef<FARSupportInterface, ESPMode::ThreadSafe>& InTrackingSystem, uint32 FrameNumber, double Timestamp, const FTransform& InLocalToTrackingTransform, const FTransform& InAlignmentTransform, FVector2D InEstimatedSize, UARCandidateImage* InDetectedImage);

	/** @see DetectedImage */
	UFUNCTION(BlueprintPure, Category = "AR AugmentedReality|Image Detection")
	UARCandidateImage* GetDetectedImage() const { return DetectedImage; };
	
	/*
	 * Get the estimate size of the detected image, where X is the estimated width, and Y is the estimated height.
	 *
	 * Note that ARCore can return a valid estimate size of the detected image when the tracking state of the UARTrackedImage
	 * is tracking. The size should reflect the actual size of the image target, which could be different than the input physical
	 * size of the candidate image.
	 *
	 * ARKit will return the physical size of the ARCandidate image.
	 */
	UFUNCTION(BlueprintPure, Category = "AR AugmentedReality|Image Detection")
	FVector2D GetEstimateSize();

	UE_DEPRECATED(4.21, "This property is now deprecated, please use GetTrackingState() and check for EARTrackingState::Tracking or IsTracked() instead.")
	/** Whether the image is currently being tracked by the AR system */
	UPROPERTY(BlueprintReadOnly, Category="AR AugmentedReality|Face Geometry")
	bool bIsTracked;

protected:
	/** The candidate image that was detected in the scene */
	UPROPERTY()
	UARCandidateImage* DetectedImage;

	/** The estimated image size that was detected in the scene */
	UPROPERTY()
	FVector2D EstimatedSize;
};

UENUM(BlueprintType, Category="AR AugmentedReality", meta=(Experimental))
enum class EARFaceTrackingDirection : uint8
{
	/** Blend shapes are tracked as if looking out of the face, e.g. right eye is the mesh's right eye and left side of screen if facing you */
	FaceRelative,
	/** Blend shapes are tracked as if looking at the face, e.g. right eye is the mesh's left eye and right side of screen if facing you (like a mirror) */
	FaceMirrored
};

UENUM(BlueprintType, Category="AR AugmentedReality", meta=(Experimental))
enum class EARFaceBlendShape : uint8
{
	// Left eye blend shapes
	EyeBlinkLeft,
	EyeLookDownLeft,
	EyeLookInLeft,
	EyeLookOutLeft,
	EyeLookUpLeft,
	EyeSquintLeft,
	EyeWideLeft,
	// Right eye blend shapes
	EyeBlinkRight,
	EyeLookDownRight,
	EyeLookInRight,
	EyeLookOutRight,
	EyeLookUpRight,
	EyeSquintRight,
	EyeWideRight,
	// Jaw blend shapes
	JawForward,
	JawLeft,
	JawRight,
	JawOpen,
	// Mouth blend shapes
	MouthClose,
	MouthFunnel,
	MouthPucker,
	MouthLeft,
	MouthRight,
	MouthSmileLeft,
	MouthSmileRight,
	MouthFrownLeft,
	MouthFrownRight,
	MouthDimpleLeft,
	MouthDimpleRight,
	MouthStretchLeft,
	MouthStretchRight,
	MouthRollLower,
	MouthRollUpper,
	MouthShrugLower,
	MouthShrugUpper,
	MouthPressLeft,
	MouthPressRight,
	MouthLowerDownLeft,
	MouthLowerDownRight,
	MouthUpperUpLeft,
	MouthUpperUpRight,
	// Brow blend shapes
	BrowDownLeft,
	BrowDownRight,
	BrowInnerUp,
	BrowOuterUpLeft,
	BrowOuterUpRight,
	// Cheek blend shapes
	CheekPuff,
	CheekSquintLeft,
	CheekSquintRight,
	// Nose blend shapes
	NoseSneerLeft,
	NoseSneerRight,
	TongueOut,
	// Treat the head rotation as curves for LiveLink support
	HeadYaw,
	HeadPitch,
	HeadRoll,
	// Treat eye rotation as curves for LiveLink support
	LeftEyeYaw,
	LeftEyePitch,
	LeftEyeRoll,
	RightEyeYaw,
	RightEyePitch,
	RightEyeRoll,
	MAX
};

UENUM(BlueprintType, Category="AR AugmentedReality", meta=(Experimental))
enum class EAREye : uint8
{
	LeftEye,
	RightEye
};

typedef TMap<EARFaceBlendShape, float> FARBlendShapeMap;

UCLASS(BlueprintType)
class AUGMENTEDREALITY_API UARFaceGeometry : public UARTrackedGeometry
{
	GENERATED_BODY()
	
public:
	void UpdateFaceGeometry(const TSharedRef<FARSupportInterface , ESPMode::ThreadSafe>& InTrackingSystem, uint32 FrameNumber, double Timestamp, const FTransform& InTransform, const FTransform& InAlignmentTransform, FARBlendShapeMap& InBlendShapes, TArray<FVector>& InVertices, const TArray<int32>& Indices, const FTransform& InLeftEyeTransform, const FTransform& InRightEyeTransform, const FVector& InLookAtTarget);
	
	virtual void DebugDraw( UWorld* World, const FLinearColor& OutlineColor, float OutlineThickness, float PersistForSeconds = 0.0f) const override;
	
public:
	UFUNCTION(BlueprintPure, Category="AR AugmentedReality|Face Geometry")
	float GetBlendShapeValue(EARFaceBlendShape BlendShape) const;
	
	UFUNCTION(BlueprintPure, Category="AR AugmentedReality|Face Geometry")
	const TMap<EARFaceBlendShape, float> GetBlendShapes() const;

	const FARBlendShapeMap& GetBlendShapesRef() const { return BlendShapes; }
	
	const TArray<FVector>& GetVertexBuffer() const { return VertexBuffer; }
	const TArray<int32>& GetIndexBuffer() const { return IndexBuffer; }
	const TArray<FVector2D>& GetUVs() const { return UVs; }
	
	UFUNCTION(BlueprintPure, Category="AR AugmentedReality|Face Geometry")
	const FTransform& GetLocalSpaceEyeTransform(EAREye Eye) const;

	UFUNCTION(BlueprintPure, Category="AR AugmentedReality|Face Geometry")
	FTransform GetWorldSpaceEyeTransform(EAREye Eye) const;

	/** The target the eyes are looking at */
	UPROPERTY(BlueprintReadOnly, Category="AR AugmentedReality|Face Geometry")
	FVector LookAtTarget;

	UE_DEPRECATED(4.21, "This property is now deprecated, please use GetTrackingState() and check for EARTrackingState::Tracking or IsTracked() instead.")
	/** Whether the face is currently being tracked by the AR system */
	UPROPERTY(BlueprintReadOnly, Category="AR AugmentedReality|Face Geometry")
	bool bIsTracked;

private:
	UPROPERTY()
	TMap<EARFaceBlendShape, float> BlendShapes;
	
	// Holds the face data for one or more face components that want access
	TArray<FVector> VertexBuffer;
	TArray<int32> IndexBuffer;
	// @todo JoeG - route the uvs in
	TArray<FVector2D> UVs;

	/** The transform for the left eye */
	FTransform LeftEyeTransform;
	/** The transform for the right eye */
	FTransform RightEyeTransform;
};

/** A tracked environment texture probe that gives you a cube map for reflections */
UCLASS(BlueprintType)
class AUGMENTEDREALITY_API UAREnvironmentCaptureProbe :
	public UARTrackedGeometry
{
	GENERATED_BODY()
	
public:
	UAREnvironmentCaptureProbe();
	
	/** Draw a box visulizing the bounds of the probe */
	virtual void DebugDraw(UWorld* World, const FLinearColor& OutlineColor, float OutlineThickness, float PersistForSeconds = 0.0f) const override;
	
	void UpdateEnvironmentCapture(const TSharedRef<FARSupportInterface , ESPMode::ThreadSafe>& InTrackingSystem, uint32 FrameNumber, double Timestamp, const FTransform& InLocalToTrackingTransform, const FTransform& InAlignmentTransform, FVector InExtent);

	/** @see Extent */
	UFUNCTION(BlueprintPure, Category="AR AugmentedReality|Environment Capture Probe")
	FVector GetExtent() const;
	/** @see EnvironmentCaptureTexture */
	UFUNCTION(BlueprintPure, Category="AR AugmentedReality|Environment Capture Probe")
	UAREnvironmentCaptureProbeTexture* GetEnvironmentCaptureTexture();

protected:
	/** The size of area this probe covers */
	FVector Extent;

	/** The cube map of the reflected environment */
	UPROPERTY()
	UAREnvironmentCaptureProbeTexture* EnvironmentCaptureTexture;
};

UCLASS(BlueprintType)
class AUGMENTEDREALITY_API UARTrackedObject : public UARTrackedGeometry
{
	GENERATED_BODY()
	
public:
	virtual void DebugDraw(UWorld* World, const FLinearColor& OutlineColor, float OutlineThickness, float PersistForSeconds = 0.0f) const override;
	
	void UpdateTrackedGeometry(const TSharedRef<FARSupportInterface , ESPMode::ThreadSafe>& InTrackingSystem, uint32 FrameNumber, double Timestamp, const FTransform& InLocalToTrackingTransform, const FTransform& InAlignmentTransform, UARCandidateObject* InDetectedObject);
	
	/** @see DetectedObject */
	UFUNCTION(BlueprintPure, Category = "AR AugmentedReality|Object Detection")
	UARCandidateObject* GetDetectedObject() const { return DetectedObject; };
	
private:
	/** The candidate object that was detected in the scene */
	UPROPERTY()
	UARCandidateObject* DetectedObject;
};
