// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditableMeshAdapter.h"
#include "EditableMeshCustomVersion.h"
#include "StaticMeshResources.h"
#include "EditableStaticMeshAdapter.generated.h"


USTRUCT()
struct FTriangleID : public FElementID
{
	GENERATED_BODY()

	FTriangleID()
	{
	}

	explicit FTriangleID( const FElementID InitElementID )
		: FElementID( InitElementID.GetValue() )
	{
	}

	explicit FTriangleID( const uint32 InitIDValue )
		: FElementID( InitIDValue )
	{
	}

	FORCEINLINE friend uint32 GetTypeHash( const FTriangleID& Other )
	{
		return GetTypeHash( Other.IDValue );
	}

	/** Invalid triangle ID */
	static const FTriangleID Invalid;
};


USTRUCT()
struct FRenderingPolygon
{
	GENERATED_BODY()

	/** Static meshes currently only support triangles.  We'll always triangulate polygons and keep track
	    of all of the triangles here */
	// @todo mesheditor: For other mesh formats, we may not need to assume triangles.  For example, modern
	//		GPUs can render quads.  We still need to consider collision geometry though.
	// @todo mesheditor: This should probably always be a transient thing, right?  We can re-triangulate on 
	//		load?  Not sure.  We might want some edit-time control over triangulation, which should be saved/loaded.
	UPROPERTY()
	TArray<FTriangleID> TriangulatedPolygonTriangleIndices;

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FRenderingPolygon& Polygon )
	{
		Ar << Polygon.TriangulatedPolygonTriangleIndices;
		return Ar;
	}
};


USTRUCT()
struct FRenderingPolygonGroup
{
	GENERATED_BODY()

	/** The rendering section index for this mesh section */
	UPROPERTY()
	uint32 RenderingSectionIndex;

	/** Maximum number of triangles which have been reserved in the index buffer */
	UPROPERTY()
	int32 MaxTriangles;

	/** Sparse array of triangles, that matches the triangles in the mesh index buffers.  Elements that
	    aren't allocated will be stored as degenerates in the mesh index buffer. */
	TSparseArray<FMeshTriangle> Triangles;


	/** Converts from an index in our Triangles array to an index of a rendering triangle's first vertex in the rendering mesh's index buffer */
	inline static FTriangleID RenderingTriangleFirstIndexToTriangleIndex( const FStaticMeshSection& RenderingSection, const uint32 RenderingTriangleFirstIndex )
	{
		return FTriangleID( ( RenderingTriangleFirstIndex - RenderingSection.FirstIndex ) / 3 );
	}

	/** Converts from an index of a rendering triangle's first vertex in the rendering mesh's index buffer to an index in our Triangles array */
	inline static uint32 TriangleIndexToRenderingTriangleFirstIndex( const FStaticMeshSection& RenderingSection, const FTriangleID TriangleIndex )
	{
		return TriangleIndex.GetValue() * 3 + RenderingSection.FirstIndex;
	}

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FRenderingPolygonGroup& Section )
	{
		Ar << Section.RenderingSectionIndex;
		Ar << Section.MaxTriangles;	// @todo mesheditor serialization: Should not need to be serialized if we triangulate after load
		SerializeSparseArray( Ar, Section.Triangles );	// @todo mesheditor serialization: Should not need to be serialized if we triangulate after load
		return Ar;
	}
};




UCLASS(MinimalAPI)
class UEditableStaticMeshAdapter : public UEditableMeshAdapter
{
	GENERATED_BODY()

public:

	/** Default constructor that initializes good defaults for UEditableStaticMeshAdapter */
	UEditableStaticMeshAdapter();

	virtual void Serialize( FArchive& Ar ) override;

	/** Creates a editable static mesh from the specified component and sub-mesh address */
	void InitEditableStaticMesh( UEditableMesh* EditableMesh, class UPrimitiveComponent& Component, const FEditableMeshSubMeshAddress& InitSubMeshAddress );
	EDITABLEMESH_API void InitFromBlankStaticMesh( UEditableMesh* EditableMesh, UStaticMesh& InStaticMesh );

	virtual void OnRebuildRenderMeshStart( const UEditableMesh* EditableMesh, const bool bRefreshBounds, const bool bInvalidateLighting ) override;
	virtual void OnRebuildRenderMesh( const UEditableMesh* EditableMesh ) override;
	virtual void OnRebuildRenderMeshFinish( const UEditableMesh* EditableMesh, const bool bUpdateCollision ) override;
	virtual void OnStartModification( const UEditableMesh* EditableMesh, const EMeshModificationType MeshModificationType, const EMeshTopologyChange MeshTopologyChange ) override;
	virtual void OnEndModification( const UEditableMesh* EditableMesh ) override;
	virtual void OnReindexElements( const UEditableMesh* EditableMesh, const FElementIDRemappings& Remappings ) override;
	virtual bool IsCommitted( const UEditableMesh* EditableMesh ) const override;
	virtual bool IsCommittedAsInstance( const UEditableMesh* EditableMesh ) const override;
	virtual void OnCommit( UEditableMesh* EditableMesh ) override;
	virtual UEditableMesh* OnCommitInstance( UEditableMesh* EditableMesh, UPrimitiveComponent* ComponentToInstanceTo ) override;
	virtual void OnRevert( UEditableMesh* EditableMesh ) override;
	virtual UEditableMesh* OnRevertInstance( UEditableMesh* EditableMesh ) override;
	virtual void OnPropagateInstanceChanges( UEditableMesh* EditableMesh ) override;

	virtual void OnDeleteVertexInstances( const UEditableMesh* EditableMesh, const TArray<FVertexInstanceID>& VertexInstanceIDs ) override;
	virtual void OnDeleteOrphanVertices( const UEditableMesh* EditableMesh, const TArray<FVertexID>& VertexIDs ) override;
	virtual void OnCreateEmptyVertexRange( const UEditableMesh* EditableMesh, const TArray<FVertexID>& VertexIDs ) override;
	virtual void OnCreateVertices( const UEditableMesh* EditableMesh, const TArray<FVertexID>& VertexIDs ) override;
	virtual void OnCreateVertexInstances( const UEditableMesh* EditableMesh, const TArray<FVertexInstanceID>& VertexInstanceIDs ) override;
	virtual void OnSetVertexAttribute( const UEditableMesh* EditableMesh, const FVertexID VertexID, const FName AttributeName, const int32 AttributeIndex, const FVector4 AttributeValue ) override;
	virtual void OnSetVertexInstanceAttribute( const UEditableMesh* EditableMesh, const FVertexInstanceID VertexInstanceID, const FName AttributeName, const int32 AttributeIndex, const FVector4 AttributeValue ) override;
	virtual void OnCreateEdges( const UEditableMesh* EditableMesh, const TArray<FEdgeID>& EdgeIDs ) override;
	virtual void OnDeleteEdges( const UEditableMesh* EditableMesh, const TArray<FEdgeID>& EdgeIDs ) override;
	virtual void OnSetEdgeAttribute( const UEditableMesh* EditableMesh, const FEdgeID EdgeID, const FName AttributeName, const int32 AttributeIndex, const FVector4 AttributeValue ) override;
	virtual void OnCreatePolygons( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs ) override;
	virtual void OnDeletePolygons( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs ) override;
	virtual void OnChangePolygonVertexInstances( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs ) override;
	virtual void OnCreatePolygonGroups( const UEditableMesh* EditableMesh, const TArray<FPolygonGroupID>& PolygonGroupIDs ) override;
	virtual void OnDeletePolygonGroups( const UEditableMesh* EditableMesh, const TArray<FPolygonGroupID>& PolygonGroupIDs ) override;
	virtual void OnRetriangulatePolygons( const UEditableMesh* EditableMesh, const TArray<FPolygonID>& PolygonIDs ) override;


private:


	/** Deletes all of a polygon's triangles (including rendering triangles from the index buffer.) */
	void DeletePolygonTriangles( const UEditableMesh* EditableMesh, const FPolygonID PolygonID );

	/** Helper function to get a specific static mesh LOD structure for a mesh */
	const struct FStaticMeshLODResources& GetStaticMeshLOD() const;
	struct FStaticMeshLODResources& GetStaticMeshLOD();

	/** Makes sure our mesh's index buffer is 32-bit, converting if needed */
	void EnsureIndexBufferIs32Bit();

	/** If any of the specified triangles contain vertex indices greater than 0xffff, updates our mesh's index buffer to be 32-bit if it isn't already */
	void UpdateIndexBufferFormatIfNeeded( const TArray<FMeshTriangle>& Triangles );

	/** Rebuilds bounds */
	void UpdateBoundsAndCollision( const UEditableMesh* EditableMesh, const bool bUpdateCollision );

	/** Gets the editable mesh section index which corresponds to the given rendering section index */
	FPolygonGroupID GetSectionForRenderingSectionIndex( const int32 RenderingSectionIndex ) const;

private:

	/** The static mesh asset we're representing */
	UPROPERTY()
	UStaticMesh* StaticMesh;

	UPROPERTY()
	UStaticMesh* OriginalStaticMesh;

	UPROPERTY()
	int32 StaticMeshLODIndex;

	/** Size of the extra gaps in the index buffer between different sections (so new triangles have some space to be added into,
		without requiring the index buffer to be manipulated) */
	static const uint32 IndexBufferInterSectionGap = 32;

	/** All of the polygons in this mesh */
	TSparseArray< FRenderingPolygon > RenderingPolygons;

	/** All of the polygon groups in this mesh */
	TSparseArray< FRenderingPolygonGroup > RenderingPolygonGroups;


	/** Used to refresh all components in the scene that may be using a mesh we're editing */
	TSharedPtr< class FStaticMeshComponentRecreateRenderStateContext > RecreateRenderStateContext;
};
