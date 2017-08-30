// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshBuilder.h"
#include "MeshDescription.h"


class MESHBUILDER_API FStaticMeshBuilder : public FMeshBuilder
{
public:
	static FStaticMeshBuilder& Get()
	{
		static FStaticMeshBuilder Instance;
		return Instance;
	}

	virtual bool Build(class UStaticMesh* StaticMesh) override;
	virtual ~FStaticMeshBuilder() {}

private:
	FStaticMeshBuilder();

	void OnBuildRenderMeshStart(class UStaticMesh* StaticMesh, const bool bInvalidateLighting);
	void OnBuildRenderMeshFinish(class UStaticMesh* StaticMesh, const bool bRebuildBoundsAndCollision);
	
	bool IsOrphanedVertex(class UStaticMesh* StaticMesh, const FVertexID VertexID) const;
	void UpdateBounds(class UStaticMesh* StaticMesh);
	void UpdateCollision(UStaticMesh *StaticMesh);

	/** Used to refresh all components in the scene that may be using a mesh we're editing */
	TSharedPtr<class FStaticMeshComponentRecreateRenderStateContext> RecreateRenderStateContext;
};

struct FTriangleID : public FElementID
{
	FTriangleID()
	{
	}

	explicit FTriangleID(const FElementID InitElementID)
		: FElementID(InitElementID.GetValue())
	{
	}

	explicit FTriangleID(const uint32 InitIDValue)
		: FElementID(InitIDValue)
	{
	}

	FORCEINLINE friend uint32 GetTypeHash(const FTriangleID& Other)
	{
		return GetTypeHash(Other.IDValue);
	}

	/** Invalid triangle ID */
	static const FTriangleID Invalid;
};


struct FRenderingPolygon
{

	/** Which rendering polygon group the polygon is in */
	FPolygonGroupID PolygonGroupID;

	/** This is a list of indices of triangles in the FRenderingPolygonGroup::Triangles array.
	We use this to maintain a record of which triangles in the section belong to this polygon. */
	TArray<FTriangleID> TriangulatedPolygonTriangleIndices;
};


struct FRenderingPolygonGroup
{

	/** The rendering section index for this mesh section */
	uint32 RenderingSectionIndex;

	/** Maximum number of triangles which have been reserved in the index buffer */
	int32 MaxTriangles;

	/** Sparse array of triangles, that matches the triangles in the mesh index buffers.  Elements that
	aren't allocated will be stored as degenerates in the mesh index buffer. */
	TMeshElementArray<FMeshTriangle, FTriangleID> Triangles;


	/** Converts from an index in our Triangles array to an index of a rendering triangle's first vertex in the rendering mesh's index buffer */
	inline static FTriangleID RenderingTriangleFirstIndexToTriangleIndex(const FStaticMeshSection& RenderingSection, const uint32 RenderingTriangleFirstIndex)
	{
		return FTriangleID((RenderingTriangleFirstIndex - RenderingSection.FirstIndex) / 3);
	}

	/** Converts from an index of a rendering triangle's first vertex in the rendering mesh's index buffer to an index in our Triangles array */
	inline static uint32 TriangleIndexToRenderingTriangleFirstIndex(const FStaticMeshSection& RenderingSection, const FTriangleID TriangleIndex)
	{
		return TriangleIndex.GetValue() * 3 + RenderingSection.FirstIndex;
	}
};
