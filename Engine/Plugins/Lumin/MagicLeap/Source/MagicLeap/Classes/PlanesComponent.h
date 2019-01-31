// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Runtime/Core/Public/Misc/Build.h"
#include "HAL/Platform.h"

#if WITH_MLSDK
#if PLATFORM_WINDOWS
#pragma warning( push )
#pragma warning( disable : 4201)
#endif // PLATFORM_WINDOWS
#include "ml_planes.h"
#if PLATFORM_WINDOWS
#pragma warning( pop )
#endif //PLATFORM_WINDOWS
#endif //WITH_MLSDK

#include "Components/SceneComponent.h"
#include "PlanesComponent.generated.h"

/** Control flags for plane queries. */
UENUM(BlueprintType)
enum class EPlaneQueryFlags : uint8
{
	/** Include planes whose normal is perpendicular to gravity. */
	Vertical,

	/** Include planes whose normal is parallel to gravity. */
	Horizontal,

	/** Include planes with arbitrary normals. */
	Arbitrary,

	/** If set, non-horizontal planes will be aligned perpendicular to gravity. */
	OrientToGravity,

	/** If set, inner planes will be returned; if not set, outer planes will be returned. */
	PreferInner,

	/** If set, holes in planar surfaces will be ignored. */
	IgnoreHoles,

	/** If set, include planes semantically tagged as ceiling. */
	Ceiling,

	/** If set, include planes semantically tagged as floor. */
	Floor,

	/** If set, include planes semantically tagged as wall. */
	Wall
};

#if WITH_MLSDK

MLPlanesQueryFlags UnrealToMLPlanesQueryFlagMap(EPlaneQueryFlags QueryFlag);

EPlaneQueryFlags MLToUnrealPlanesQueryFlagMap(MLPlanesQueryFlags QueryFlag);

MLPlanesQueryFlags UnrealToMLPlanesQueryFlags(const TArray<EPlaneQueryFlags>& QueryFlags);

void MLToUnrealPlanesQueryFlags(uint32 QueryFlags, TArray<EPlaneQueryFlags>& OutPlaneFlags);

#endif //WITH_MLSDK


/** Represents a plane returned from the ML-API. */
USTRUCT(BlueprintType)
struct FPlaneResult
{
	GENERATED_BODY()

public:
	/** Position of the center of the plane in world coordinates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planes|MagicLeap")
	FVector PlanePosition;

	/** Orientation of the plane. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planes|MagicLeap")
	FRotator PlaneOrientation;

	/** Orientation of the content with its up-vector orthogonal to the plane. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planes|MagicLeap")
	FRotator ContentOrientation;

	/** Width and height of the plane (in Unreal units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planes|MagicLeap")
	FVector2D PlaneDimensions;

	/** The flags which describe this plane. TODO: Should be a TSet but that is misbehaving in the editor.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planes|MagicLeap")
	TArray<EPlaneQueryFlags> PlaneFlags;

	/** The Boundary of the plane in plane local space.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planes|MagicLeap")
	TArray<FVector> BoundaryPolygon;

	/** ID of the plane result. This ID is persistent across queries */
	UPROPERTY(BlueprintReadOnly, Category = "Planes|MagicLeap")
	FGuid ID;

	/** ID of the plane result. This ID is persistent across queries */
	uint64 ID_64;
};

/** 
  The PlanesComponent class manages requests for planes, processes the results and provides them to the calling system.
  The calling system is able request planes within a specified area.  Various other search criteria can be set via this
  class's public properties.  Planes requests are processed on a separate thread.  Once a planes request has been processed
  the calling system will be notified via an FPlaneResultDelegate broadcast.
*/
UCLASS(ClassGroup = MagicLeap, BlueprintType, Blueprintable, EditInlineNew, meta = (BlueprintSpawnableComponent))
class MAGICLEAP_API UPlanesComponent
	: public USceneComponent
{
	GENERATED_BODY()

public:
	UPlanesComponent();
	~UPlanesComponent();

	/**
	  Delegate used to convey the result of a plane query.
	  @param QuerySucceeded True if the planes query succeeded, false otherwise.
	  @param Planes Array of planes returned by the query.
	  @param UserData Data set while requesting the planes query. Can be used to identify which query this result corresponds to.
	*/
	DECLARE_DYNAMIC_DELEGATE_ThreeParams(FPlaneResultDelegate, bool, QuerySucceeded, const TArray<FPlaneResult>&, Planes, int32, UserData);

	/** The flags to apply to this query. TODO: Should be a TSet but that is misbehaving in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planes|MagicLeap")
	TArray<EPlaneQueryFlags> QueryFlags;

	/** Bounding box for searching planes in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planes|MagicLeap")
	class UBoxComponent* SearchVolume;

	/** The maximum number of planes that should be returned in the result. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planes|MagicLeap", meta = (ClampMin = 0))
	int32 MaxResults;

	/**
	  If EPlaneQueryFlags::IgnoreHoles is not a query flag then holes with a perimeter (in Unreal Units)
	  smaller than this value will be ignored, and can be part of the plane.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planes|MagicLeap", meta = (ClampMin = 0))
	float MinHolePerimeter;

	/**
	  The minimum area (in squared Unreal Units) of planes to be returned.
	  This value cannot be lower than 400 (lower values will be capped to this minimum).
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planes|MagicLeap", meta = (ClampMin = 400))
	float MinPlaneArea;

	/**
	  Requests planes with the current value of QueryFlags, SearchVolume and MaxResults.
	  @param UserData User data for this request. The same data will be included in the result for query identification.
	  @param ResultDelegate Delegate which will be called when the planes result is ready.
	  @returns True if the planes query was successfully placed, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "MagicLeap|Planes")
	bool RequestPlanes(int32 UserData, const FPlaneResultDelegate& ResultDelegate);

	/** Creates the planes tracker handle for the component */
	virtual void BeginPlay() override;

	/** Polls for and handles the results of the plane queries. */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/** Destroys the interface object to the planes api*/
	virtual void FinishDestroy() override;

	UPROPERTY()
	bool IgnoreBoundingVolume_DEPRECATED;

private:
	struct FPlanesRequestMetaData
	{
	public:
		FPlaneResultDelegate ResultDelegate;
		int32 UserData;
		uint32 MaxResults;
	};

	TMap<uint64, FPlanesRequestMetaData> PendingRequests;
	TArray<uint64> CompletedRequests;

	class FPlanesTrackerImpl *Impl;

#if WITH_EDITOR
private:
	void PrePIEEnded(bool bWasSimulatingInEditor);
#endif
};
