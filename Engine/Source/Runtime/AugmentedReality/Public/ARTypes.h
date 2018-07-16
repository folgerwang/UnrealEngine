// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
//@joeg -- ARKit 2.0 support
#include "HAL/ThreadSafeBool.h"
#include "ARTypes.generated.h"

class FARSystemBase;
class USceneComponent;
class IXRTrackingSystem;
class UARPin;
class UARTrackedGeometry;
class UARLightEstimate;
struct FARTraceResult;
class UTexture2D;

UENUM(BlueprintType, Category="AR AugmentedReality", meta=(Experimental))
enum class EARTrackingState : uint8
{
	/** Currently not tracking. */
	Tracking,
	
	/** Currently not tracking, but may resume tracking later. */
	NotTracking,
	
	/** Stopped tracking forever. */
	StoppedTracking,
	
};


/**
 * Channels that let users select which kind of tracked geometry to trace against.
 */
UENUM( BlueprintType, Category="AR AugmentedReality|Trace Result", meta=(Bitflags) )
enum class EARLineTraceChannels : uint8
{
	None = 0,
	
	/** Trace against points that the AR system considers significant . */
	FeaturePoint = 1,
	
	/** Trace against estimated plane that does not have an associated tracked geometry. */
	GroundPlane = 2,
	
	/** Trace against any plane tracked geometries using Center and Extent. */
	PlaneUsingExtent = 4,
	
	/** Trace against any plane tracked geometries using the boundary polygon. */
	PlaneUsingBoundaryPolygon = 8
};
ENUM_CLASS_FLAGS(EARLineTraceChannels);



UENUM(BlueprintType, Category="AR AugmentedReality", meta=(Experimental))
enum class EARTrackingQuality : uint8
{
	/** The tracking quality is not available. */
	NotTracking,
	
	/** The tracking quality is limited, relying only on the device's motion. */
	OrientationOnly,
	
	/** The tracking quality is good. */
	OrientationAndPosition
};

/**
 * Describes the current status of the AR session.
 */
UENUM(BlueprintType, meta=(ScriptName="ARSessionStatusType"))
enum class EARSessionStatus : uint8
{
	/** Unreal AR session has not started yet.*/
	NotStarted,
	/** Unreal AR session is running. */
	Running,
	/** Unreal AR session failed to start due to the AR subsystem not being supported by the device. */
	NotSupported,
	/** The AR session encountered fatal error; the developer should call `StartARSession()` to re-start the AR subsystem. */
	FatalError,
	/** AR session failed to start because it lacks the necessary permission (likely access to the camera or the gyroscope). */
	PermissionNotGranted,
	/** AR session failed to start because the configuration isn't supported. */
	UnsupportedConfiguration,
	/** Session isn't running due to unknown reason; @see FARSessionStatus::AdditionalInfo for more information */
	Other,
};

//@joeg -- ARKit 2.0 support
/** Gives feedback as to whether the AR data can be saved and relocalized or not */
UENUM(BlueprintType, Category="AR AugmentedReality", meta=(Experimental))
enum class EARWorldMappingState : uint8
{
	/** World mapping is not available */
	NotAvailable,
	/** World mapping is still in progress but without enough data for relocalization */
	StillMappingNotRelocalizable,
	/** World mapping is still in progress but there is enough data captured for relocalization */
	StillMappingRelocalizable,
	/** World mapping has mapped the area and is fully relocalizable */
	Mapped
};


/** The current state of the AR subsystem including an optional explanation string. */
USTRUCT(BlueprintType)
struct AUGMENTEDREALITY_API FARSessionStatus
{
public:
	
	GENERATED_BODY()

	FARSessionStatus()
	:FARSessionStatus(EARSessionStatus::Other)
	{}

	FARSessionStatus(EARSessionStatus InStatus, FString InExtraInfo = FString())
		: AdditionalInfo(InExtraInfo)
		, Status(InStatus)
	{

	}
	
	/** Optional information about the current status of the system. */
	UPROPERTY(BlueprintReadOnly, Category="AR AugmentedReality|Session")
	FString AdditionalInfo;

	/** The current status of the AR subsystem. */
	UPROPERTY(BlueprintReadOnly, Category = "AR AugmentedReality|Session")
	EARSessionStatus Status;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnARTrackingStateChanged, EARTrackingState, NewTrackingState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnARTransformUpdated, const FTransform&, OldToNewTransform );

UCLASS()
class UARTypesDummyClass : public UObject
{
	GENERATED_BODY()
};

/** A reference to a system-level AR object  */
class IARRef
{
public:
	virtual void AddRef() = 0;
	virtual void RemoveRef() = 0;
public:
	virtual ~IARRef() {}

};

/** Tells the image detection code how to assume the image is oriented */
UENUM(BlueprintType)
enum class EARCandidateImageOrientation : uint8
{
	Landscape,
	Portrait
};

/** An asset that points to an image to be detected in a scene and provides the size of the object in real life */
UCLASS(BlueprintType)
class AUGMENTEDREALITY_API UARCandidateImage :
	public UDataAsset
{
	GENERATED_BODY()

public:
	/** @see CandidateTexture */
	UFUNCTION(BlueprintPure, Category = "AR AugmentedReality|Image Detection")
	UTexture2D* GetCandidateTexture() const { return CandidateTexture; }

	/** @see FriendlyName */
	UFUNCTION(BlueprintPure, Category = "AR AugmentedReality|Image Detection")
	const FString& GetFriendlyName() const { return FriendlyName; }

	/** @see Width */
	UFUNCTION(BlueprintPure, Category = "AR AugmentedReality|Image Detection")
	float GetPhysicalWidth() const { return Width; }

	/** @see Height */
	UFUNCTION(BlueprintPure, Category = "AR AugmentedReality|Image Detection")
	float GetPhysicalHeight() const { return Height; }

	/** @see Orientation */
	UFUNCTION(BlueprintPure, Category = "AR AugmentedReality|Image Detection")
	EARCandidateImageOrientation GetOrientation() const { return Orientation; }

private:
	/** The image to detect in scenes */
	UPROPERTY(EditAnywhere, Category = "AR Candidate Image")
	UTexture2D* CandidateTexture;

	/** The friendly name to report back when the image is detected in scenes */
	UPROPERTY(EditAnywhere, Category = "AR Candidate Image")
	FString FriendlyName;

	/** The physical width in centimeters of the object that this candidate image represents */
	UPROPERTY(EditAnywhere, Category = "AR Candidate Image")
	float Width;

	/** The physical height in centimeters of the object that this candidate image represents */
	UPROPERTY(EditAnywhere, Category = "AR Candidate Image")
	float Height;

	/** The orientation to treat the candidate image as */
	UPROPERTY(EditAnywhere, Category = "AR Candidate Image")
	EARCandidateImageOrientation Orientation;
};

//@joeg -- ARKit 2.0 support

/** An asset that points to an object to be detected in a scene */
UCLASS(BlueprintType)
class AUGMENTEDREALITY_API UARCandidateObject :
	public UDataAsset
{
	GENERATED_BODY()
	
public:
	/** @see CandidateObjectData */
	UFUNCTION(BlueprintPure, Category = "AR AugmentedReality|Object Detection")
	const TArray<uint8>& GetCandidateObjectData() const { return CandidateObjectData; }

	UFUNCTION(BlueprintCallable, Category = "AR AugmentedReality|Object Detection")
	void SetCandidateObjectData(const TArray<uint8>& InCandidateObject) { CandidateObjectData = InCandidateObject; }

	/** @see FriendlyName */
	UFUNCTION(BlueprintPure, Category = "AR AugmentedReality|Object Detection")
	const FString& GetFriendlyName() const { return FriendlyName; }

	/** @see BoundingBox */
	UFUNCTION(BlueprintPure, Category = "AR AugmentedReality|Object Detection")
	const FBox& GetBoundingBox() const { return BoundingBox; }

	UFUNCTION(BlueprintCallable, Category = "AR AugmentedReality|Object Detection")
	void SetBoundingBox(const FBox& InBoundingBox) { BoundingBox = InBoundingBox; }

private:
	/** The object to detect in scenes */
	UPROPERTY(EditAnywhere, Category = "AR Candidate Object")
	TArray<uint8> CandidateObjectData;
	
	/** The friendly name to report back when the object is detected in scenes */
	UPROPERTY(EditAnywhere, Category = "AR Candidate Object")
	FString FriendlyName;
	
	/** The physical bounds in centimeters of the object that this candidate object represents */
	UPROPERTY(EditAnywhere, Category = "AR Candidate Image")
	FBox BoundingBox;
};

/**
 * Base class for async AR requests
 */
class AUGMENTEDREALITY_API FARAsyncTask
{
public:
	virtual ~FARAsyncTask() {}
	
	/** @return whether the task succeeded or not */
	bool HadError() const;
	/** @return information about the error if there was one */
	FString GetErrorString() const;
	/** @return whether the task has completed or not */
	bool IsDone() const;
	
protected:
	FThreadSafeBool bIsDone;
	FThreadSafeBool bHadError;
	FString Error;
};

/** Async task that saves the world data into a buffer */
class AUGMENTEDREALITY_API FARSaveWorldAsyncTask :
	public FARAsyncTask
{
public:
	/** @return the byte array that the world was saved into. Note uses MoveTemp() for efficiency so only valid once */
	TArray<uint8> GetSavedWorldData();
	
protected:
	TArray<uint8> WorldData;
};

/** Async task that builds a candidate object used for detection from the ar session */
class AUGMENTEDREALITY_API FARGetCandidateObjectAsyncTask :
	public FARAsyncTask
{
public:
	/** @return the candidate object that you can use for detection later */
	virtual UARCandidateObject* GetCandidateObject() = 0;
};

class FARErrorGetCandidateObjectAsyncTask :
	public FARGetCandidateObjectAsyncTask
{
public:
	FARErrorGetCandidateObjectAsyncTask(FString InError)
	{
		Error = InError;
		bHadError = true;
		bIsDone = true;
	}
	virtual UARCandidateObject* GetCandidateObject() override { return nullptr; }
};

class FARErrorSaveWorldAsyncTask :
	public FARSaveWorldAsyncTask
{
public:
	FARErrorSaveWorldAsyncTask(FString InError)
	{
		Error = InError;
		bHadError = true;
		bIsDone = true;
	}
};
