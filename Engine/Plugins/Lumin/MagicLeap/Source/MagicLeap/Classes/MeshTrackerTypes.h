// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Engine.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MeshTrackerTypes.generated.h"

/** Type of mesh to query from the underlying system. */
UENUM(BlueprintType)
enum class EMeshType : uint8
{
	/** Meshing should be done as triangles. */
	Triangles,
	/** Return mesh vertices as a point cloud. */
	PointCloud
};

/** Vertex color mode. */
UENUM(BlueprintType)
enum class EMLMeshVertexColorMode : uint8
{
	/** Vertex Color is not set. */
	None		UMETA(DisplayName = "No Vertex Color"),
	/** Vertex confidence is interpolated between two specified colors. */
	Confidence	UMETA(DisplayName = "Vertex Confidence"),
	/** Each block is given a color from a list. */
	Block		UMETA(DisplayName = "Blocks Colored"),
	/** Each LOD is given a color from a list. */
	LOD			UMETA(DisplayName = "LODs Colored")
};

/** Discrete level of detail required. */
UENUM(BlueprintType)
enum class EMeshLOD : uint8
{
	/** Minimum LOD. */
	Minimum,
	/** Medium LOD. */
	Medium,
	/** Maximum LOD. */
	Maximum,
};

/** State of a block mesh. */
UENUM(BlueprintType)
enum class EMeshState : uint8
{
	/** Mesh has been created */
	New,
	/** Mesh has been updated. */
	Updated,
	/** Mesh has been deleted. */
	Deleted,
	/** Mesh is unchanged. */
	Unchanged
};

/** Representation of a mesh block. */
USTRUCT(BlueprintType)
struct FMeshBlockInfo
{
	GENERATED_BODY()

public:
	/** The coordinate frame UID to represent the block. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meshing|MagicLeap")
	FGuid BlockID;

	/** The center of the mesh block bounding box. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meshing|MagicLeap")
	FVector BlockPosition;

	/** The orientation of the mesh block bounding box.*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meshing|MagicLeap")
	FRotator BlockOrientation;

	/** The size of the mesh block bounding box (in Unreal Units). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meshing|MagicLeap")
	FVector BlockDimensions;

	/** The timestamp when block was updated. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meshing|MagicLeap")
	FTimespan Timestamp;

	/** The state of the mesh block. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meshing|MagicLeap")
	EMeshState BlockState;
};

/** Response structure for the mesh block info. */
USTRUCT(BlueprintType)
struct FMLTrackingMeshInfo
{
	GENERATED_BODY()

public:
	/** The response timestamp to a earlier request. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meshing|MagicLeap")
	FTimespan Timestamp;

	/** The meshinfo returned by the system. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meshing|MagicLeap")
	TArray<FMeshBlockInfo> BlockData;
};

/** Request structure to get the actual mesh for a block. */
USTRUCT(BlueprintType)
struct FMeshBlockRequest
{
	GENERATED_BODY()

public:
	/** The UID to represent the block. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	FGuid BlockID;

	/** The LOD level to request. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshing|MagicLeap")
	EMeshLOD LevelOfDetail;
};
