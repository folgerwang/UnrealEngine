// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "MeshTrackerComponent.generated.h"

/** Type of mesh to query from the underlying system. */
UENUM(BlueprintType)
enum class EMeshType : uint8
{
	/** The full, high resolution mesh. */
	Full,
	/*! Meshing should be done in blocks. */
	Blocks,
	/*! Return mesh vertices as a point cloud. */
	PointCloud
};

/** Type of mesh to query from the underlying system. */
UENUM(BlueprintType)
enum class EMLMeshVertexColorMode : uint8
{
	/** Vertex Color is not set. */
	None    UMETA(DisplayName = "No Vertex Color"),
	/*! Vertex confidence is interpolated between two specified colors. */
	Confidence  UMETA(DisplayName = "Vertex Confidence"),
	/*! Each block is given a color from a list. */
	Block  UMETA(DisplayName = "Blocks Colored")
};

/**
	The MeshTrackerComponent class manages requests for environmental mesh data, processes the results and provides
	them to the calling system. The calling system is able request environmental mesh data within a specified area.
	Various other search criteria can be set via this class's public properties.  Mesh data requests are processed
	on a separate thread.  Once a mesh data request has been processed the calling system will be notified via an
	FOnMeshTrackerUpdated broadcast.
*/
UCLASS(ClassGroup = MagicLeap, BlueprintType, Blueprintable, EditInlineNew, meta = (BlueprintSpawnableComponent))
class MAGICLEAP_API UMeshTrackerComponent
	: public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:
	/** Destroys the FMeshTrackerImpl instance.*/
	~UMeshTrackerComponent();

	/**
		Sets the procedural mesh component that will store and display the environmental mesh results.
		@param InMRMeshPtr The procedural mesh component to store the query result in.
	*/
	UFUNCTION(BlueprintCallable, Category = "Mesh Reconstruction")
	void ConnectMRMesh(class UMRMeshComponent* InMRMeshPtr);

	/**
		Unlinks the current procedural mesh component from the mesh tracking system.
		@param InMRMeshPtr The procedural mesh component to unlink from the mesh tracking system.
	*/
	UFUNCTION(BlueprintCallable, Category = "Mesh Reconstruction")
	void DisconnectMRMesh(class UMRMeshComponent* InMRMeshPtr);

	/**
		Delegate used by OnMeshUpdated().
		@param Index The index of the mesh section in the ProceduralMeshComponent that was updated.
		@param Vertices List of all vertices in the updated mesh section.
		@param Triangles List of all triangles in the updated mesh section.
		@param Normals List of the normals of all triangles in the updated mesh section.
		@param Confidence List of the confidence values per vertex in the updated mesh section. This can be used to determine if the user needs to scan more.
	*/
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnMeshTrackerUpdated, int32, Index, const TArray<FVector>&, Vertices, const TArray<int32>&, Triangles, const TArray<FVector>&, Normals, const TArray<float>&, Confidence);

	/** Activated whenever new information about this mesh tracker is detected. */
	UPROPERTY(BlueprintAssignable)
	FOnMeshTrackerUpdated OnMeshTrackerUpdated;

	/** Set to true to start scanning the world for meshes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	bool ScanWorld = true;

	/** The type of mesh to query. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	EMeshType MeshType = EMeshType::Blocks;

	/**
	  Specifies the time (in seconds) to query if new mesh data is available.
	  Decreasing this time may give more mesh data, but may degrade performance. Use 0 to turn off auto updating.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	float MeshingPollTime = 1.0f;

	/** Bounding box for the mesh scan. The mesh will be scanned for only within this box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	class UBoxComponent* BoundingVolume;

	/** Mesh everything within range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	bool IgnoreBoundingVolume = false;

	/**
	  Maximum number of triangles allowed for the mesh in Full mode, or per mesh block in Blocks mode. Use 0 to turn off simplification.
	  Setting this to a reasonably high enough number because of target hardware limitation. It is recommended to leave this number as low as possible but never 0.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap", meta = (ClampMin = 0))
	int32 TargetNumberTriangles = 5000;

	/** If true, the system will attempt to fill small gaps to create a more connected mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	bool FillGaps = true;

	/** The perimeter (in Unreal Units) of gaps to be filled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	float PerimeterOfGapsToFill = 100.0f;

	/** If true, the system will planarize the returned mesh i.e. planar regions will be smoothed out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	bool Planarize = false;

	/** If true, the system will remove mesh sections that are disconnected from the main mesh
		and which have an area less than DisconnectedSectionArea. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	bool RemoveDisconnectedSections = true;

	/** Any section that is disconnected from the main mesh and has an area (in Unreal Units squared) less than this value will be removed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	float DisconnectedSectionArea = 25.0f;

	/** Minimum distance that a bounding box has to move to cause a rescan. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap", meta = (ClampMin = 0.0f))
	float MinDistanceRescan = 50.0f;

	/** If true, the system will generate normals for the triangle vertices. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	bool RequestNormals = true;

	/**
		If true, the system will generate the mesh confidence values for the triangle vertices.
		These confidence values can be used to determine if the user needs to scan more.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	bool RequestVertexConfidence = false;

	/**
	* Vertex Colors can be unused, or filled with several types of information.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	EMLMeshVertexColorMode VertexColorMode = EMLMeshVertexColorMode::None;

	/** Colors through which we cycle when setting vertex color by block. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	TArray<FColor> BlockVertexColors;

	/** Color mapped to confidence value of zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	FLinearColor VertexColorFromConfidenceZero;

	/** Color mapped to confidence value of one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	FLinearColor VertexColorFromConfidenceOne;

	/**
		If true, overlapping area between two mesh blocks will be removed.
		This field is only valid when the MeshType is Blocks.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	bool RemoveOverlappingTriangles = false;

	/** MRMeshComponent can render and provide collision based on the Mesh data. */
	UPROPERTY(transient)
	class UMRMeshComponent* MRMesh;

	/**
		The procedural mesh generated should bake physics and update its collision data.
		TODO: Can we calculate this based on the collision flags of the UProceduralMeshComponent?
		It seems like those are not being respected directly by the component.
	*/

	/** Force an update on a non-live updating mesh tracker. */
	UFUNCTION(BlueprintCallable, Category = "Meshing|MagicLeap")
	bool ForceMeshUpdate();

	/** Polls for and handles the results of the environmental mesh queries. */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/** Unlinks the current procedural mesh component from the mesh tracking system. */
	virtual void BeginDestroy() override;

	/** Destroys the interface object to the mesh tracking api*/
	virtual void FinishDestroy() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& e) override;
#endif

private:
	void LogMLDataArray(const struct MLDataArray& Data) const;
	void TickWithFakeData();
	class FMeshTrackerImpl *Impl;

#if WITH_EDITOR
private:
	void PrePIEEnded(bool bWasSimulatingInEditor);
#endif
};
