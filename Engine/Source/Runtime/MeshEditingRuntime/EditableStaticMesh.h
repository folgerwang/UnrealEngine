// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditableMesh.h"
#include "EditableMeshCustomVersion.h"
#include "StaticMeshResources.h"
#include "EditableStaticMesh.generated.h"


/**
* Perform custom serialization for TSparseArray.
* The default TSparseArray serialization also compacts all the elements, removing the gaps and changing the indices.
* The indices are significant in editable meshes, hence this is a custom serializer which preserves them.
*/
template <typename T>
void SerializeSparseArray( FArchive& Ar, TSparseArray<T>& Array )
{
	if( Ar.CustomVer( FEditableMeshCustomVersion::GUID ) < FEditableMeshCustomVersion::CustomSparseArraySerialization )
	{
		Ar << Array;
	}
	else
	{
		Array.CountBytes( Ar );

		if( Ar.IsLoading() )
		{
			// Load array
			TBitArray<> AllocatedIndices;
			Ar << AllocatedIndices;

			Array.Empty( AllocatedIndices.Num() );
			for( auto It = TConstSetBitIterator<>( AllocatedIndices ); It; ++It )
			{
				Array.Insert( It.GetIndex(), T() );
				Ar << Array[ It.GetIndex() ];
			}
		}
		else
		{
			// Save array
			const int32 MaxIndex = Array.GetMaxIndex();

			// We have to build the TBitArray representing allocated indices by hand, as we don't have access to it from outside TSparseArray.
			// @todo core: consider replacing TSparseArray serialization with this format.
			TBitArray<> AllocatedIndices( false, MaxIndex );
			for( int32 Index = 0; Index < MaxIndex; ++Index )
			{
				if( Array.IsAllocated( Index ) )
				{
					AllocatedIndices[ Index ] = true;
				}
			}
			Ar << AllocatedIndices;

			for( auto It = Array.CreateIterator(); It; ++It )
			{
				Ar << *It;
			}
		}
	}
}


USTRUCT()
struct FRenderingVertexID : public FElementID
{
	GENERATED_BODY()

	FRenderingVertexID()
	{
	}

	explicit FRenderingVertexID( const FElementID InitElementID )
		: FElementID( InitElementID.GetValue() )
	{
	}

	explicit FRenderingVertexID( const uint32 InitIDValue )
		: FElementID( InitIDValue )
	{
	}

	/** Invalid rendering vertex ID */
	MESHEDITINGRUNTIME_API static const FRenderingVertexID Invalid;
};


USTRUCT()
struct FEditableStaticMeshVertex
{
	GENERATED_BODY()

	/** Position of the vertex.  This is also stored in every rendering vertex in the actual static mesh, but
	    we need a copy here because the vertex might not have any rendering vertices (in the case where no
		triangles are connected to it.) */
	UPROPERTY()
	FVector VertexPosition;

	/** All of the extra rendering vertices generated for this editable mesh vertex.  A rendering mesh may have
	    multiple vertices that represent a single editable mesh vertex position (for discreet normals, etc.) */
	UPROPERTY()
	TArray< FRenderingVertexID > RenderingVertexIDs;

	/** The edges connected to this vertex */
	UPROPERTY()
	TArray< FEdgeID > ConnectedEdgeIDs;

	/** When subdivisions are enabled, this controls how sharp the vertex is, between 0.0 and 1.0. */
	UPROPERTY()
	float CornerSharpness;	// @todo mesheditor subdiv: Not really used by static meshes at all.  Only for UEditableMeshes that use subdivision features.  Move elsewhere.?

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FEditableStaticMeshVertex& Vertex )
	{
		Ar << Vertex.VertexPosition;
		Ar << Vertex.RenderingVertexIDs;
		Ar << Vertex.ConnectedEdgeIDs;
		Ar << Vertex.CornerSharpness;
		return Ar;
	}
};


USTRUCT()
struct FEditableStaticMeshRenderingVertex
{
	GENERATED_BODY()

	UPROPERTY()
	FVertexID VertexID;

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FEditableStaticMeshRenderingVertex& Vertex )
	{
		Ar << Vertex.VertexID;
		return Ar;
	}
};


USTRUCT()
struct FEditableStaticMeshEdge
{
	GENERATED_BODY()

	/** IDs of the two editable mesh vertices that make up this edge.  The winding direction is not defined. */
	UPROPERTY()
	FVertexID VertexIDs[ 2 ];

	/** The polygons that share this edge.  It's best if there are always only two polygons that share
	    the edge, and those polygons are facing the same direction */
	UPROPERTY()
	TArray< FPolygonRef > ConnectedPolygons;

	/** Whether this edge is 'hard' or not, for the purpose of vertex normal and tangent generation */
	UPROPERTY()
	bool bIsHardEdge;

	/** When subdivisions are enabled, this controls how sharp the creasing of this edge will be, between 0.0 and 1.0. */
	UPROPERTY()
	float CreaseSharpness;

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FEditableStaticMeshEdge& Edge )
	{
		Ar << Edge.VertexIDs[ 0 ];
		Ar << Edge.VertexIDs[ 1 ];
		Ar << Edge.ConnectedPolygons;
		Ar << Edge.bIsHardEdge;
		Ar << Edge.CreaseSharpness;
		return Ar;
	}
};


USTRUCT()
struct FEditableStaticMeshTriangle
{
	GENERATED_BODY()

	/** The three vertices that make up this triangle.  The winding direction is not defined.  Note that the order of these 
	    are the same as the order of the triangles in the static mesh index buffer.  They must always be kept in sync!  To find
		the first rendering triangle vertex index in the index buffer, multiply the triangle index in this array
		by three (three vertices per triangle), then add the rendering section's start index */
	UPROPERTY()
	FRenderingVertexID RenderingVertexIDs[ 3 ];

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FEditableStaticMeshTriangle& Tri )
	{
		Ar << Tri.RenderingVertexIDs[ 0 ];
		Ar << Tri.RenderingVertexIDs[ 1 ];
		Ar << Tri.RenderingVertexIDs[ 2 ];
		return Ar;
	}
};


USTRUCT()
struct FEditableStaticMeshPolygonContourVertex
{
	GENERATED_BODY()

	/** Vertex ID representing this vertex */
	UPROPERTY()
	FVertexID VertexID;

	/** Rendering vertex ID representing this vertex */
	UPROPERTY()
	FRenderingVertexID RenderingVertexID;

	/** Other per-vertex polygon contour attributes follow. First UVs. */
	UPROPERTY()
	TArray< FVector2D > VertexUVs;

	/** Normal vector */
	UPROPERTY()
	FVector Normal;
	
	/** Tangent vector */
	UPROPERTY()
	FVector Tangent;

	/** Basis determinant sign used to calculate the sense of the binormal */
	UPROPERTY()
	float BinormalSign;

	/** Vertex color */
	UPROPERTY()
	FLinearColor Color;

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FEditableStaticMeshPolygonContourVertex& Vertex )
	{
		Ar << Vertex.VertexID;
		Ar << Vertex.RenderingVertexID;	// @todo mesheditor serialization: Should not need to be serialized if we triangulate after load
		Ar << Vertex.VertexUVs;
		Ar << Vertex.Normal;	// @todo mesheditor serialization: Need a way to specify if a normal has been overridden and hence needs custom serialization instead of being generated
		Ar << Vertex.Tangent;	// @todo mesheditor serialization: Should not need to be serialized if we regenerate normals and tangents after load
		Ar << Vertex.BinormalSign;	// @todo mesheditor serialization: Should not need to be serialized if we regenerate normals and tangents after load

		if( Ar.CustomVer( FEditableMeshCustomVersion::GUID ) >= FEditableMeshCustomVersion::WithVertexColors )
		{
			Ar << Vertex.Color;
		}

		return Ar;
	}
};


USTRUCT()
struct FEditableStaticMeshPolygonContour
{
	GENERATED_BODY()

	/** The ordered list of vertices that make up the polygon contour. The winding direction is counter-clockwise. */
	UPROPERTY()
	TArray< FEditableStaticMeshPolygonContourVertex > Vertices;

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FEditableStaticMeshPolygonContour& Contour )
	{
		Ar << Contour.Vertices;
		return Ar;
	}
};


USTRUCT()
struct FEditableStaticMeshPolygon
{
	GENERATED_BODY()

	/** The outer boundary edges of this polygon */
	UPROPERTY()
	FEditableStaticMeshPolygonContour PerimeterContour;

	/** Optional inner contours of this polygon that define holes inside of the polygon.  For the geometry to
	    be considered valid, the hole contours should reside within the boundary of the polygon perimeter contour, 
		and must not overlap each other.  No "nesting" of polygons inside the holes is supported -- those are 
		simply separate polygons */
	UPROPERTY()
	TArray< FEditableStaticMeshPolygonContour > HoleContours;

	/** Static meshes currently only support triangles.  We'll always triangulate polygons and keep track
	    of all of the triangles here */
	// @todo mesheditor: For other mesh formats, we may not need to assume triangles.  For example, modern
	//		GPUs can render quads.  We still need to consider collision geometry though.
	// @todo mesheditor: This should probably always be a transient thing, right?  We can re-triangulate on 
	//		load?  Not sure.  We might want some edit-time control over triangulation, which should be saved/loaded.
	UPROPERTY()
	TArray< FTriangleID > TriangulatedPolygonTriangleIndices;

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FEditableStaticMeshPolygon& Polygon )
	{
		Ar << Polygon.PerimeterContour;
		Ar << Polygon.HoleContours;
		Ar << Polygon.TriangulatedPolygonTriangleIndices;
		return Ar;
	}
};


USTRUCT()
struct FEditableStaticMeshSection
{
	GENERATED_BODY()

	/** The rendering section index for this mesh section */
	UPROPERTY()
	uint32 RenderingSectionIndex;

	/** The material index for this mesh section */
	UPROPERTY()
	int32 MaterialIndex;

	/** If true, collision is enabled for this section. */
	UPROPERTY()
	bool bEnableCollision;

	/** If true, this section will cast a shadow */
	UPROPERTY()
	bool bCastShadow;

	/** Maximum number of triangles which have been reserved in the index buffer */
	UPROPERTY()
	int32 MaxTriangles;

	/** All polygons in the mesh */
	// @todo mesheditor: These are no longer serializable.  Need UProperty sparse array, or need to native serialize using normal arrays
	TSparseArray< FEditableStaticMeshPolygon > Polygons;

	/** Sparse array of triangles, that matches the triangles in the mesh index buffers.  Elements that
	    aren't allocated will be stored as degenerates in the mesh index buffer. */
	TSparseArray< FEditableStaticMeshTriangle > Triangles;


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
	friend FArchive& operator<<( FArchive& Ar, FEditableStaticMeshSection& Section )
	{
		Ar << Section.RenderingSectionIndex;
		Ar << Section.MaterialIndex;
		Ar << Section.bEnableCollision;
		Ar << Section.bCastShadow;
		Ar << Section.MaxTriangles;	// @todo mesheditor serialization: Should not need to be serialized if we triangulate after load
		SerializeSparseArray( Ar, Section.Polygons );
		SerializeSparseArray( Ar, Section.Triangles );	// @todo mesheditor serialization: Should not need to be serialized if we triangulate after load
		return Ar;
	}
};


UCLASS(MinimalAPI)
class UEditableStaticMesh : public UEditableMesh
{
	GENERATED_BODY()

public:

	/** Default constructor that initializes good defaults for UEditableStaticMesh */
	UEditableStaticMesh();

	virtual void Serialize( FArchive& Ar ) override;

	/** Creates a editable static mesh from the specified component and sub-mesh address */
	void InitEditableStaticMesh( class UPrimitiveComponent& Component, const FEditableMeshSubMeshAddress& InitSubMeshAddress );
	MESHEDITINGRUNTIME_API void InitFromBlankStaticMesh( UStaticMesh& InStaticMesh );

	struct FElementIDRemappings
	{
		TSparseArray<FVertexID> NewVertexIndexLookup;
		TSparseArray<FRenderingVertexID> NewRenderingVertexIndexLookup;
		TSparseArray<FEdgeID> NewEdgeIndexLookup;
		TSparseArray<FSectionID> NewSectionIndexLookup;

		struct FPerPolygonLookups
		{
			TSparseArray<FPolygonID> NewPolygonIndexLookup;
			TSparseArray<FTriangleID> NewTriangleIndexLookup;
		};

		TSparseArray<FPerPolygonLookups> PerPolygon;

		FVertexID GetRemappedVertexID( FVertexID VertexID ) const
		{
			check( NewVertexIndexLookup.IsAllocated( VertexID.GetValue() ) );
			return NewVertexIndexLookup[ VertexID.GetValue() ];
		}

		FRenderingVertexID GetRemappedRenderingVertexID( FRenderingVertexID RenderingVertexID ) const
		{
			check( NewRenderingVertexIndexLookup.IsAllocated( RenderingVertexID.GetValue() ) );
			return NewRenderingVertexIndexLookup[ RenderingVertexID.GetValue() ];
		}

		FEdgeID GetRemappedEdgeID( FEdgeID EdgeID ) const
		{
			check( NewEdgeIndexLookup.IsAllocated( EdgeID.GetValue() ) );
			return NewEdgeIndexLookup[ EdgeID.GetValue() ];
		}

		FSectionID GetRemappedSectionID( FSectionID SectionID ) const
		{
			check( NewSectionIndexLookup.IsAllocated( SectionID.GetValue() ) );
			return NewSectionIndexLookup[ SectionID.GetValue() ];
		}

		FPolygonRef GetRemappedPolygonRef( FPolygonRef PolygonRef ) const
		{
			const FSectionID NewSectionID( GetRemappedSectionID( PolygonRef.SectionID ) );

			check( PerPolygon.IsAllocated( NewSectionID.GetValue() ) );
			const TSparseArray<FPolygonID>& NewPolygonIndexLookup = PerPolygon[ NewSectionID.GetValue() ].NewPolygonIndexLookup;

			check( NewPolygonIndexLookup.IsAllocated( PolygonRef.PolygonID.GetValue() ) );
			return FPolygonRef( NewSectionID, NewPolygonIndexLookup[ PolygonRef.PolygonID.GetValue() ] );
		}

		FTriangleID GetRemappedTriangleID( FSectionID RemappedSectionID, FTriangleID TriangleID ) const
		{
			check( PerPolygon.IsAllocated( RemappedSectionID.GetValue() ) );
			const TSparseArray<FTriangleID>& NewTriangleIndexLookup = PerPolygon[ RemappedSectionID.GetValue() ].NewTriangleIndexLookup;

			check( NewTriangleIndexLookup.IsAllocated( TriangleID.GetValue() ) );
			return NewTriangleIndexLookup[ TriangleID.GetValue() ];
		}

	};

	/** Compacts mesh element arrays to remove gaps, and fixes up referenced IDs */
	void Compact();

	/** Remaps mesh element arrays according to the provided remappings, in order to undo a compact operation */
	void Uncompact( const FElementIDRemappings& Remappings );

	// UEditableMesh overrides
	virtual void RebuildRenderMesh() override;
	virtual void StartModification( const EMeshModificationType MeshModificationType, const EMeshTopologyChange MeshTopologyChange ) override;
	virtual void EndModification( const bool bFromUndo = false ) override;
	virtual bool IsCommitted() const override;
	virtual bool IsCommittedAsInstance() const override;
	virtual void Commit() override;
	virtual UEditableMesh* CommitInstance( UPrimitiveComponent* ComponentToInstanceTo ) override;
	virtual void Revert() override;
	virtual UEditableMesh* RevertInstance() override;
	virtual void PropagateInstanceChanges() override;

	virtual int32 GetRenderingVertexCount() const override;
	virtual int32 GetRenderingVertexArraySize() const override;

	virtual int32 GetVertexCount() const override;
	virtual int32 GetVertexArraySize() const override;
	virtual bool IsValidVertex( const FVertexID VertexID ) const override;
	virtual FVector4 GetVertexAttribute( const FVertexID VertexID, const FName AttributeName, const int32 AttributeIndex ) const override;
	virtual void SetVertexAttribute_Internal( const FVertexID VertexID, const FName AttributeName, const int32 AttributeIndex, const FVector4& NewAttributeValue ) override;
	virtual int32 GetVertexConnectedEdgeCount( const FVertexID VertexID ) const override;
	virtual FEdgeID GetVertexConnectedEdge( const FVertexID VertexID, const int32 ConnectedEdgeNumber ) const override;

	virtual FVector4 GetEdgeAttribute( const FEdgeID EdgeID, const FName AttributeName, const int32 AttributeIndex ) const override;
	virtual void SetEdgeAttribute_Internal( const FEdgeID EdgeID, const FName AttributeName, const int32 AttributeIndex, const FVector4& NewAttributeValue ) override;
	virtual int32 GetEdgeCount() const override;
	virtual int32 GetEdgeArraySize() const override;
	virtual bool IsValidEdge( const FEdgeID EdgeID ) const override;
	virtual FVertexID GetEdgeVertex( const FEdgeID EdgeID, const int32 EdgeVertexNumber ) const override;
	virtual int32 GetEdgeConnectedPolygonCount( const FEdgeID EdgeID ) const override;
	virtual FPolygonRef GetEdgeConnectedPolygon( const FEdgeID EdgeID, const int32 ConnectedPolygonNumber ) const override;

	virtual int32 GetSectionCount() const override;
	virtual int32 GetSectionArraySize() const override;
	virtual bool IsValidSection( const FSectionID SectionID ) const override;

	virtual int32 GetPolygonCount( const FSectionID SectionID ) const override;
	virtual int32 GetPolygonArraySize( const FSectionID SectionID ) const override;
	virtual bool IsValidPolygon( const FPolygonRef PolygonRef ) const override;
	virtual int32 GetTriangleCount( const FSectionID SectionID ) const override;
	virtual int32 GetTriangleArraySize( const FSectionID SectionID ) const override;
	virtual int32 GetPolygonPerimeterVertexCount( const FPolygonRef PolygonRef ) const override;
	virtual FVertexID GetPolygonPerimeterVertex( const FPolygonRef PolygonRef, const int32 PolygonVertexNumber ) const override;
	virtual FVector4 GetPolygonPerimeterVertexAttribute( const FPolygonRef PolygonRef, const int32 PolygonVertexNumber, const FName AttributeName, const int32 AttributeIndex ) const override;
	virtual void SetPolygonPerimeterVertexAttribute_Internal( const FPolygonRef PolygonRef, const int32 PolygonVertexNumber, const FName AttributeName, const int32 AttributeIndex, const FVector4& NewAttributeValue ) override;

	virtual int32 GetPolygonHoleCount( const FPolygonRef PolygonRef ) const override;
	virtual int32 GetPolygonHoleVertexCount( const FPolygonRef PolygonRef, const int32 HoleNumber ) const override;
	virtual FVertexID GetPolygonHoleVertex( const FPolygonRef PolygonRef, const int32 HoleNumber, const int32 PolygonVertexNumber ) const override;
	virtual FVector4 GetPolygonHoleVertexAttribute( const FPolygonRef PolygonRef, const int32 HoleNumber, const int32 PolygonVertexNumber, const FName AttributeName, const int32 AttributeIndex ) const override;
	virtual void SetPolygonHoleVertexAttribute_Internal( const FPolygonRef PolygonRef, const int32 HoleNumber, const int32 PolygonVertexNumber, const FName AttributeName, const int32 AttributeIndex, const FVector4& NewAttributeValue ) override;
	virtual int32 GetPolygonTriangulatedTriangleCount( const FPolygonRef PolygonRef ) const override;
	virtual FVector GetPolygonTriangulatedTriangleVertexPosition( const FPolygonRef PolygonRef, const int32 PolygonTriangleNumber, const int32 TriangleVertexNumber ) const override;
	virtual void RetriangulatePolygons( const TArray<FPolygonRef>& PolygonRefs, const bool bOnlyOnUndo ) override;

protected:

	virtual void CreateEmptyVertexRange_Internal( const int32 NumVerticesToAdd, const TArray<FVertexID>* OverrideVertexIDsForRedo, TArray<FVertexID>& OutNewVertexIDs ) override;
	virtual void CreateEdge_Internal( const FVertexID VertexIDA, const FVertexID VertexIDB, const TArray<FPolygonRef>& ConnectedPolygons, const FEdgeID OverrideEdgeIDForRedo, FEdgeID& OutNewEdgeID ) override;
	virtual void CreatePolygon_Internal( const FSectionID SectionID, const TArray<FVertexID>& VertexIDs, const TArray<TArray<FVertexID>>& VertexIDsForEachHole, const FPolygonID OverridePolygonIDForRedo, FPolygonRef& OutNewPolygonRef, TArray<FEdgeID>& OutNewEdgeIDs ) override;
	virtual void DeleteOrphanVertices_Internal( const TArray<FVertexID>& VertexIDsToDelete ) override;
	virtual void DeleteEdges_Internal( const TArray<FEdgeID>& EdgeIDsToDelete, const bool bDeleteOrphanedVertices ) override;
	virtual void DeletePolygon_Internal( const FPolygonRef PolygonRef, const bool bDeleteOrphanedEdges, const bool bDeleteOrphanedVertices, const bool bDeleteEmptySections ) override;
	virtual void SetEdgeVertices_Internal( const FEdgeID EdgeID, const FVertexID NewVertexID0, const FVertexID NewVertexID1 ) override;
	virtual void InsertPolygonPerimeterVertices_Internal( const FPolygonRef PolygonRef, const int32 InsertBeforeVertexNumber, const TArray<FVertexAndAttributes>& VerticesToInsert ) override;
	virtual void RemovePolygonPerimeterVertices_Internal( const FPolygonRef PolygonRef, const int32 FirstVertexNumberToRemove, const int32 NumVerticesToRemove ) override;
	virtual FSectionID GetSectionIDFromMaterial_Internal( UMaterialInterface* Material, bool bCreateNewSectionIfNotFound ) override;
	virtual FSectionID CreateSection_Internal( const FSectionToCreate& SectionToCreate ) override;
	virtual void DeleteSection_Internal( const FSectionID SectionID ) override;


private:

	/** Prepares render buffers for updating */
	void RebuildRenderMeshStart( const bool bRefreshBounds, const bool bInvalidateLighting );

	/** */
	void RebuildRenderMeshFinish( const bool bUpdateCollision );

	/** */
	void RebuildRenderMeshInternal();

	/** Given a set of index remappings, fixes up references to element IDs */
	void FixUpElementIDs( const FElementIDRemappings& Remappings );

	void InitializeStaticMeshBuildVertex( FStaticMeshBuildVertex& StaticMeshVertex, const FEditableStaticMeshPolygonContourVertex ContourVertex );

	/** Gets the named attribute value from a polygon contour vertex */
	FVector4 GetPolygonContourVertexAttribute( const FEditableStaticMeshPolygonContourVertex& PolygonContourVertex, const FName AttributeName, const int32 AttributeIndex ) const;

	/** Sets the named attribute value for a polygon contour vertex */
	void SetPolygonContourVertexAttribute( FEditableStaticMeshPolygonContourVertex& PolygonContourVertex, const FName AttributeName, const int32 AttributeIndex, const FVector4& NewAttributeValue );

	FRenderingVertexID GetPolygonPerimeterRenderingVertex( const FPolygonRef PolygonRef, const int32 PolygonVertexNumber ) const;
	FRenderingVertexID GetPolygonHoleRenderingVertex( const FPolygonRef PolygonRef, const int32 HoleNumber, const int32 PolygonVertexNumber ) const;

	/** Adds the specified number of new rendering vertices, and returns the IDs of the new rendering vertices */
	void CreateRenderingVertices( const TArray<FVertexID>& VertexIDs, const TOptional<FRenderingVertexID> OptionalCopyFromRenderingVertexID, TArray<FRenderingVertexID>& OutNewRenderingVertexIDs );

	/** Deletes a rendering vertex that was confirmed to be no longer used by any polygons */
	void DeleteOrphanRenderingVertices( const TArray<FRenderingVertexID>& RenderingVertexIDsToDelete );

	/** Deletes all of a polygon's triangles (including rendering triangles from the index buffer.) */
	void DeletePolygonTriangles( const FPolygonRef PolygonRef );

	/** Deletes triangles from the index buffer for the specified section and list of triangle indices */
	void DeleteRenderingTrianglesForSectionTriangles( const FSectionID SectionID, const TArray<FTriangleID>& SectionTriangleIDsToRemove );

	/** Helper function to get a specific static mesh LOD structure for a mesh */
	const struct FStaticMeshLODResources& GetStaticMeshLOD() const;
	struct FStaticMeshLODResources& GetStaticMeshLOD();

	/** Figures out whether a rendering vertex is unique to the specified polygon */
	bool DoesPolygonPerimeterVertexHaveDiscreetRenderingVertex( const FPolygonRef PolygonRef, const int32 PolygonVertexNumber ) const;

	/** Given a polygon vertex, returns a rendering vertex that is guaranteed to be unique for that polygon */
	FRenderingVertexID MakeDiscreetPolygonPerimeterRenderingVertexIfNeeded( const FPolygonRef PolygonRef, const int32 PolygonVertexNumber );

	/** Adds a new rendering vertex to the specified polygon's perimeter at the specified vertex number of that polygon.  Returns the new rendering vertex index. */
	FRenderingVertexID AddNewRenderingVertexToPolygonPerimeter( const FPolygonRef PolygonRef, const int32 PolygonVertexNumber, const TOptional<FRenderingVertexID> OptionalCopyFromRenderingVertexID = TOptional<FRenderingVertexID>() );

	/** Makes sure our mesh's index buffer is 32-bit, converting if needed */
	void EnsureIndexBufferIs32Bit();

	/** If any of the specified vertex indices are greater than 0xffff, updates our mesh's index buffer to be 32-bit if it isn't already */
	void UpdateIndexBufferFormatIfNeeded( const TArray<FRenderingVertexID>& RenderingVertexIDs );

	/** If the specified vertex index is greater than 0xffff, updates our mesh's index buffer to be 32-bit if it isn't already */
	void UpdateIndexBufferFormatIfNeeded( const FRenderingVertexID RenderingVertexID );

	/** Rebuilds bounds */
	void UpdateBoundsAndCollision( const bool bUpdateCollision );

	/** Updates the index buffer to make room for more indices in the given section */
	void AllocateExtraIndicesForSection( const FSectionID SectionID, int32 NumExtraTriangles = IndexBufferInterSectionGap );

	/** Gets the editable mesh section index which corresponds to the given rendering section index */
	FSectionID GetSectionForRenderingSectionIndex( const int32 RenderingSectionIndex ) const;

private:

	/** The static mesh asset we're representing */
	UPROPERTY()
	UStaticMesh* StaticMesh;

	UPROPERTY()
	UStaticMesh* OriginalStaticMesh;

	/** Counter to determine when we should compact data */
	UPROPERTY()
	int32 PendingCompactCounter;

	/** Data will be compacted after this many topology modifying actions. */
	static const int32 CompactFrequency = 1;

	/** Size of the extra gaps in the index buffer between different sections (so new triangles have some space to be added into,
		without requiring the index buffer to be manipulated) */
	static const uint32 IndexBufferInterSectionGap = 32;

	/** Each editable vertex in this mesh. */
	TSparseArray< FEditableStaticMeshVertex > Vertices;

	/** Sparse array of rendering vertices, that matches the vertices in the mesh vertex buffers */
	TSparseArray< FEditableStaticMeshRenderingVertex > RenderingVertices;

	/** All editable mesh edges.  Note that some of these edges will be internal polygon edges, synthesized while
	    triangulating polygons into triangles.  Static meshes currently only support triangles. */
	TSparseArray< FEditableStaticMeshEdge > Edges;

	/** All of the sections in this mesh */
	TSparseArray< FEditableStaticMeshSection > Sections;


	/** Used to refresh all components in the scene that may be using a mesh we're editing */
	TSharedPtr< class FStaticMeshComponentRecreateRenderStateContext > RecreateRenderStateContext;
};
