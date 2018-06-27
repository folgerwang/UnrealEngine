// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "MeshTrackerComponent.generated.h"

/** Type of mesh to query from the underlying system. */
UENUM(BlueprintType)
enum class EMeshType : uint8
{
	/*! Meshing should be done as triangles. */
	Triangles,
	/*! Return mesh vertices as a point cloud. */
	PointCloud
};

/** Vertex color mode. */
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

/** Discrete level of detail required. */
UENUM(BlueprintType)
enum class EMeshLOD : uint8
{
	/*! Minimum LOD. */
	Minimum,
	/*! Medium LOD. */
	Medium,
	/*! Maximum LOD. */
	Maximum,
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
	EMeshType MeshType = EMeshType::Triangles;

	/** Bounding box for the mesh scan. The mesh will be scanned for only within this box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	class UBoxComponent* BoundingVolume;

	/** Meshing LOD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	EMeshLOD LevelOfDetail = EMeshLOD::Medium;

	/** The perimeter (in Unreal Units) of gaps to be filled. 0 means do not fill. A good value is 300cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	float PerimeterOfGapsToFill = 300.0f;

	/** If true, the system will planarize the returned mesh i.e. planar regions will be smoothed out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	bool Planarize = false;

	/** Any section that is disconnected from the main mesh and has an area (in Unreal Units squared) 
	    less than this value will be removed. 
		0 means do not remove disconnected sections. A good value is 50cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	float DisconnectedSectionArea = 50.0f;

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
	class FMeshTrackerImpl *Impl;

#if WITH_EDITOR
private:
	void PrePIEEnded(bool bWasSimulatingInEditor);
#endif
};
