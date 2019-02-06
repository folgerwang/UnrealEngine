// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MeshDescription.h"
#include "EditableMeshTypes.h"
#include "EditableMesh/EditableMeshCustomVersion.h"
#include "Misc/Change.h"		// For TUniquePtr<FChange>
#include "Logging/LogMacros.h"
#include "Materials/MaterialInterface.h"
#include "GenericOctreePublic.h"	// For FOctreeElementId
#include "GeometryHitTest.h"
#include "EditableMesh.generated.h"

class UEditableMeshAdapter;

#define EDITABLE_MESH_DEBUG_LOG
#ifdef EDITABLE_MESH_DEBUG_LOG
EDITABLEMESH_API DECLARE_LOG_CATEGORY_EXTERN( LogEditableMesh, All, All );
#else
EDITABLEMESH_API DECLARE_LOG_CATEGORY_EXTERN( LogEditableMesh, Log, All );
#endif


// Specify which platforms currently support OpenSubdiv
#if PLATFORM_WINDOWS && PLATFORM_64BITS
#define EDITABLE_MESH_USE_OPENSUBDIV 1
#else
#define EDITABLE_MESH_USE_OPENSUBDIV 0
#endif


// @todo mesheditor: Comment these classes and enums!
#if EDITABLE_MESH_USE_OPENSUBDIV
namespace OpenSubdiv
{
	namespace v3_2_0
	{
		namespace Far
		{
			class TopologyRefiner;
		}
	}
}
#endif


/**
 * Additional mesh attributes required by EditableMesh
 */
namespace MeshAttribute
{
	namespace PolygonGroup
	{
		extern EDITABLEMESH_API const FName MaterialAssetName;
	}
}


UENUM( BlueprintType )
enum class EInsetPolygonsMode : uint8
{
	All,
	CenterPolygonOnly,
	SidePolygonsOnly,
};


UENUM( BlueprintType )
enum class ETriangleTessellationMode : uint8
{
	/** Connect each vertex to a new center vertex, forming three triangles */
	ThreeTriangles,

	/** Split each edge and create a center polygon that connects those new vertices, then three additional polygons for each original corner */
	FourTriangles,
};


UCLASS( BlueprintType )
class EDITABLEMESH_API UEditableMesh : public UObject
{
	GENERATED_BODY()

public:

	/** Default constructor that initializes good defaults for UEditableMesh */
	UEditableMesh();

	// UObject interface
	virtual void Serialize( FArchive& Ar ) override;
	virtual void PostLoad() override;

	/** Compacts mesh element arrays to remove gaps, and fixes up referenced IDs */
	void Compact();

	/** Remaps mesh element arrays according to the provided remappings, in order to undo a compact operation */
	void Uncompact( const FElementIDRemappings& Remappings );

	void SetMeshDescription( FMeshDescription* InMeshDescription );

	const FMeshDescription* GetMeshDescription() const { return MeshDescription; }
	FMeshDescription* GetMeshDescription() { return MeshDescription; }

	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void InitializeAdapters();
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void RebuildRenderMesh();
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void StartModification( const EMeshModificationType MeshModificationType, const EMeshTopologyChange MeshTopologyChange );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void EndModification( const bool bFromUndo = false );
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) bool IsCommitted() const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) bool IsCommittedAsInstance() const;
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void Commit();
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) UEditableMesh* CommitInstance( UPrimitiveComponent* ComponentToInstanceTo );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void Revert();
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) UEditableMesh* RevertInstance();
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void PropagateInstanceChanges();

	/** Returns the number of vertices in this mesh */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	int32 GetVertexCount() const;

	/** Returns whether the given vertex ID is valid */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	bool IsValidVertex( const FVertexID VertexID ) const;

	/** Returns whether the given vertex ID is orphaned */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	bool IsOrphanedVertex( const FVertexID VertexID ) const;

	/** Returns the number of edges connected to this vertex */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	int32 GetVertexConnectedEdgeCount( const FVertexID VertexID ) const;

	/** Returns the requested edge connected to this vertex */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	FEdgeID GetVertexConnectedEdge( const FVertexID VertexID, const int32 ConnectedEdgeNumber ) const;

	/** Returns the number of vertex instances in this mesh */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	int32 GetVertexInstanceCount() const;

	/** Returns the vertex ID which the given vertex instance is instancing */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	FVertexID GetVertexInstanceVertex( const FVertexInstanceID VertexInstanceID ) const;

	/** Returns the number of polygons connected to this vertex instance */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	int32 GetVertexInstanceConnectedPolygonCount( const FVertexInstanceID VertexInstanceID ) const;

	/** Returns the indexed polygon connected to this vertex instance */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	FPolygonID GetVertexInstanceConnectedPolygon( const FVertexInstanceID VertexInstanceID, const int32 ConnectedPolygonNumber ) const;

	/** Returns the number of edges in this mesh */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	int32 GetEdgeCount() const;

	/** Returns whether the given edge ID is valid */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	bool IsValidEdge( const FEdgeID EdgeID ) const;

	/** Returns the given indexed vertex for this edge. EdgeVertexNumber must be 0 or 1. */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	FVertexID GetEdgeVertex( const FEdgeID EdgeID, const int32 EdgeVertexNumber ) const;

	/** Returns the number of polygons connected to this edge */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	int32 GetEdgeConnectedPolygonCount( const FEdgeID EdgeID ) const;

	/** Returns the indexed polygon connected to this edge */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	FPolygonID GetEdgeConnectedPolygon( const FEdgeID EdgeID, const int32 ConnectedPolygonNumber ) const;

	/** Returns the number of polygon groups in this mesh */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	int32 GetPolygonGroupCount() const;

	/** Returns whether the given polygon group ID is valid */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	bool IsValidPolygonGroup( const FPolygonGroupID PolygonGroupID ) const;

	/** Returns the number of polygons in this polygon group */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	int32 GetPolygonCountInGroup( const FPolygonGroupID PolygonGroupID ) const;

	/** Returns the given indexed polygon in this polygon group */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	FPolygonID GetPolygonInGroup( const FPolygonGroupID PolygonGroupID, const int32 PolygonNumber ) const;

	/** Returns the number of polygons in this mesh */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	int32 GetPolygonCount() const;

	/** Returns whether the given polygon ID is valid */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	bool IsValidPolygon( const FPolygonID PolygonID ) const;

	/** Returns the polygon group this polygon is assigned to */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	FPolygonGroupID GetGroupForPolygon( const FPolygonID PolygonID ) const;

	/** Returns the number of vertices on this polygon's perimeter */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	int32 GetPolygonPerimeterVertexCount( const FPolygonID PolygonID ) const;

	/** Returns the indexed vertex on this polygon's perimeter */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	FVertexID GetPolygonPerimeterVertex( const FPolygonID PolygonID, const int32 PolygonVertexNumber ) const;

	/** Returns the indexed vertex instance on this polygon's perimeter */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	FVertexInstanceID GetPolygonPerimeterVertexInstance( const FPolygonID PolygonID, const int32 PolygonVertexNumber ) const;

	/** Returns the number of triangles which make up this polygon */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	int32 GetPolygonTriangulatedTriangleCount( const FPolygonID PolygonID ) const;

	/** Returns the indexed triangle of the triangulated polygon */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	FMeshTriangle GetPolygonTriangulatedTriangle( const FPolygonID PolygonID, int32 PolygonTriangleNumber ) const;

protected:

	/** Given a set of index remappings, fixes up references in the octree */
	void RemapOctreeIDs( const FElementIDRemappings& Remappings );

	void SetVertexAttribute( const FVertexID VertexID, const FMeshElementAttributeData& Attribute );
	void SetVertexInstanceAttribute( const FVertexInstanceID VertexInstanceID, const FMeshElementAttributeData& Attribute );
	void SetEdgeAttribute( const FEdgeID EdgeID, const FMeshElementAttributeData& Attribute );
	void SetPolygonAttribute( const FPolygonID PolygonID, const FMeshElementAttributeData& Attribute );
	void SetPolygonGroupAttribute( const FPolygonGroupID PolygonGroupID, const FMeshElementAttributeData& Attribute );
	FVertexInstanceID CreateVertexInstanceForContourVertex( const FVertexAndAttributes& ContourVertex, const FPolygonID PolygonID );
	void CreatePolygonContour( const TArray<FVertexAndAttributes>& Contour, TArray<FVertexInstanceID>& OutVertexInstanceIDs );
	void BackupPolygonContour( const FMeshPolygonContour& Contour, TArray<FVertexAndAttributes>& OutVerticesAndAttributes );
	void GetConnectedSoftEdges( const FVertexID VertexID, TArray<FEdgeID>& OutConnectedSoftEdges ) const;
	void GetVertexConnectedPolygonsInSameSoftEdgedGroup( const FVertexID VertexInstanceID, const FPolygonID PolygonID, TArray<FPolygonID>& OutPolygonIDs ) const;
	FVertexInstanceID GetVertexInstanceInPolygonForVertex( const FPolygonID PolygonID, const FVertexID VertexID ) const;
	void SetPolygonContourVertexAttributes( FMeshPolygonContour& Contour, const FPolygonID PolygonID, const TArray<FMeshElementAttributeList>& AttributeLists );
	void SplitVertexInstanceInPolygons( const FVertexInstanceID VertexInstanceID, const TArray<FPolygonID>& PolygonIDs );
	void ReplaceVertexInstanceInPolygons( const FVertexInstanceID OldVertexInstanceID, const FVertexInstanceID NewVertexInstanceID, const TArray<FPolygonID>& PolygonIDs );
	float GetPolygonCornerAngleForVertex( const FPolygonID PolygonID, const FVertexID VertexID ) const;
	void SplitVerticesIfNecessary( const TArray<FVertexID>& VerticesToSplit );
	void GetPolygonsInSameSoftEdgedGroupAsPolygon( const FPolygonID PolygonID, const TArray<FPolygonID>& PolygonIDsToCheck, const TArray<FEdgeID>& SoftEdgeIDs, TArray<FPolygonID>& OutPolygonIDs ) const;

public:

	/**
	 * Called at initialization time to set this mesh's sub-mesh address
	 *
	 * @param	NewSubMeshAddress	The new sub-mesh address for this mesh
	 */
	void SetSubMeshAddress( const FEditableMeshSubMeshAddress& NewSubMeshAddress );

	/**
	 * @return	Returns true if StartModification() was called and the mesh is able to be modified currently.  Remember to call EndModification() when finished.
	 */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual bool IsBeingModified() const
	{
		return bIsBeingModified;
	}

	/**
	 * @return	Returns true if undo tracking is enabled on this mesh
	 */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	bool IsUndoAllowed() const
	{
		return bAllowUndo;
	}

	/**
	 * Sets whether undo is allowed on this mesh
	 *
	 * @param	bInAllowUndo	True if undo features are enabled on this mesh.  You're only allowed to call MakeUndo() if this is set to true.
	 */
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" )
	void SetAllowUndo( const bool bInAllowUndo )
	{
		bAllowUndo = bInAllowUndo;
	}

	/**
	 * @return	Returns true if our octree spatial database is enabled for this mesh
	 */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	bool IsSpatialDatabaseAllowed() const
	{
		return bAllowSpatialDatabase;
	}

	/**
	 * Sets whether this mesh should automatically generate and maintain an octree spatial database.  Certain queries may only be
	 * supported when the mesh has an octree generated.  The octree is never saved or loaded, and always generated on demand.  This
	 * feature adds significant overhead to editable mesh initialization and modification, so only use it if you really need to.
	 *
	 * @param	bInAllowSpatialDatabase		True if an octree should be generated and maintained for this mesh.
	 */
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" )
	void SetAllowSpatialDatabase( const bool bInAllowSpatialDatabase );

	/**
	 * @return	Returns true if there are any existing tracked changes that can be undo right now
	 */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	bool AnyChangesToUndo() const;


	/**
	 * @return	Returns true if compaction is enabled on this mesh
	 */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	bool IsCompactAllowed() const
	{
		return bAllowCompact;
	}

	/**
	 * Sets whether the mesh can be sporadically compacted as modifications are performed
	 *
	 * @param	bInAllowCompact		True if compaction is enabled on this mesh.
	 */
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" )
	void SetAllowCompact( const bool bInAllowCompact )
	{
		bAllowCompact = bInAllowCompact;
	}


	DECLARE_EVENT_TwoParams( UEditableMesh, FElementIDsRemapped, UEditableMesh*, const FElementIDRemappings& );
	FElementIDsRemapped& OnElementIDsRemapped()
	{
		return ElementIDsRemappedEvent;
	}

	/**
	 * Gets the sub-mesh address for this mesh which uniquely identifies the mesh among other sub-meshes in the same component
	 *
	 * @return	The sub-mesh address for the mesh
	 */
	const FEditableMeshSubMeshAddress& GetSubMeshAddress() const;

	/** Grabs any outstanding changes to this mesh and returns a change that can be used to undo those changes.  Calling this
	    function will clear the history of changes.  This function will return a null pointer if bAllowUndo is false. */
	// @todo mesheditor script: We might need this to be available for BP editable meshes, in some form at least.  Probably it should just apply the undo right away.
	TUniquePtr<FChange> MakeUndo();


	/**
	 * Statics
	 */

	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FVertexID InvalidVertexID()
	{
		return FVertexID::Invalid;
	}
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FEdgeID InvalidEdgeID()
	{
		return FEdgeID::Invalid;
	}
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FPolygonGroupID InvalidPolygonGroupID()
	{
		return FPolygonGroupID::Invalid;
	}
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FPolygonID InvalidPolygonID()
	{
		return FPolygonID::Invalid;
	}
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FVertexID MakeVertexID( const int32 VertexIndex )
	{
		return FVertexID( VertexIndex );
	}
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FEdgeID MakeEdgeID( const int32 EdgeIndex )
	{
		return FEdgeID( EdgeIndex );
	}
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FPolygonGroupID MakePolygonGroupID( const int32 PolygonGroupIndex )
	{
		return FPolygonGroupID( PolygonGroupIndex );
	}
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FPolygonID MakePolygonID( const int32 PolygonIndex )
	{
		return FPolygonID( PolygonIndex );
	}

	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FPolygonGroupID GetFirstValidPolygonGroup() const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) int32 GetTextureCoordinateCount() const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) int32 GetSubdivisionCount() const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) bool IsPreviewingSubdivisions() const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetVertexConnectedEdges( const FVertexID VertexID, TArray<FEdgeID>& OutConnectedEdgeIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetVertexConnectedPolygons( const FVertexID VertexID, TArray<FPolygonID>& OutConnectedPolygonIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetVertexInstanceConnectedPolygons( const FVertexInstanceID VertexInstanceID, TArray<FPolygonID>& OutConnectedPolygonIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetVertexAdjacentVertices( const FVertexID VertexID, TArray< FVertexID >& OutAdjacentVertexIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FEdgeID GetVertexPairEdge( const FVertexID VertexID, const FVertexID NextVertexID, bool& bOutEdgeWindingIsReversed ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetEdgeVertices( const FEdgeID EdgeID, FVertexID& OutEdgeVertexID0, FVertexID& OutEdgeVertexID1 ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetEdgeConnectedPolygons( const FEdgeID EdgeID, TArray<FPolygonID>& OutConnectedPolygonIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetEdgeLoopElements( const FEdgeID EdgeID, TArray<FEdgeID>& EdgeLoopIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FEdgeID GetEdgeThatConnectsVertices( const FVertexID VertexID0, const FVertexID VertexID1 ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) int32 GetPolygonPerimeterEdgeCount( const FPolygonID PolygonID ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetPolygonPerimeterVertices( const FPolygonID PolygonID, TArray<FVertexID>& OutPolygonPerimeterVertexIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetPolygonPerimeterVertexInstances( const FPolygonID PolygonID, TArray<FVertexInstanceID>& OutPolygonPerimeterVertexInstanceIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FEdgeID GetPolygonPerimeterEdge( const FPolygonID PolygonID, const int32 PerimeterEdgeNumber, bool& bOutEdgeWindingIsReversedForPolygon ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetPolygonPerimeterEdges( const FPolygonID PolygonID, TArray<FEdgeID>& OutPolygonPerimeterEdgeIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetPolygonAdjacentPolygons( const FPolygonID PolygonID, TArray<FPolygonID>& OutAdjacentPolygons ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) int32 FindPolygonPerimeterVertexNumberForVertex( const FPolygonID PolygonID, const FVertexID VertexID ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) int32 FindPolygonPerimeterEdgeNumberForVertices( const FPolygonID PolygonID, const FVertexID EdgeVertexID0, const FVertexID EdgeVertexID1 ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FBox ComputeBoundingBox() const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FBoxSphereBounds ComputeBoundingBoxAndSphere() const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FVector ComputePolygonCenter( const FPolygonID PolygonID ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FPlane ComputePolygonPlane( const FPolygonID PolygonID ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FVector ComputePolygonNormal( const FPolygonID PolygonID ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) const FSubdivisionLimitData& GetSubdivisionLimitData() const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void ComputePolygonTriangulation( const FPolygonID PolygonID, TArray<FMeshTriangle>& OutTriangles ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) bool ComputeBarycentricWeightForPointOnPolygon( const FPolygonID PolygonID, const FVector PointOnPolygon, FMeshTriangle& OutTriangle, FVector& OutTriangleVertexWeights ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void ComputePolygonsSharedEdges( const TArray<FPolygonID>& PolygonIDs, TArray<FEdgeID>& OutSharedEdgeIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void FindPolygonLoop( const FEdgeID EdgeID, TArray<FEdgeID>& OutEdgeLoopEdgeIDs, TArray<FEdgeID>& OutFlippedEdgeIDs, TArray<FEdgeID>& OutReversedEdgeIDPathToTake, TArray<FPolygonID>& OutPolygonIDsToSplit ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void SearchSpatialDatabaseForPolygonsPotentiallyIntersectingLineSegment( const FVector LineSegmentStart, const FVector LineSegmentEnd, TArray<FPolygonID>& OutPolygons ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void SearchSpatialDatabaseForPolygonsInVolume( const TArray<FPlane>& Planes, TArray<FPolygonID>& OutPolygons ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void SearchSpatialDatabaseForPolygonsPotentiallyIntersectingPlane( const FPlane& InPlane, TArray<FPolygonID>& OutPolygons ) const;


	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetSubdivisionCount( const int32 NewSubdivisionCount );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void MoveVertices( const TArray<FVertexToMove>& VerticesToMove );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void CreateMissingPolygonPerimeterEdges( const FPolygonID PolygonID, TArray<FEdgeID>& OutNewEdgeIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SplitEdge( const FEdgeID EdgeID, const TArray<float>& Splits, TArray<FVertexID>& OutNewVertexIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void InsertEdgeLoop( const FEdgeID EdgeID, const TArray<float>& Splits, TArray<FEdgeID>& OutNewEdgeIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SplitPolygons( const TArray<FPolygonToSplit>& PolygonsToSplit, TArray<FEdgeID>& OutNewEdgeIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void DeleteEdgeAndConnectedPolygons( const FEdgeID EdgeID, const bool bDeleteOrphanedEdges, const bool bDeleteOrphanedVertices, const bool bDeleteOrphanedVertexInstances, const bool bDeleteEmptyPolygonGroups );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void DeleteVertexAndConnectedEdgesAndPolygons( const FVertexID VertexID, const bool bDeleteOrphanedEdges, const bool bDeleteOrphanedVertices, const bool bDeleteOrphanedVertexInstances, const bool bDeleteEmptyPolygonGroups );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void DeleteOrphanVertices( const TArray<FVertexID>& VertexIDsToDelete );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void DeleteVertexInstances( const TArray<FVertexInstanceID>& VertexInstanceIDsToDelete, const bool bDeleteOrphanedVertices );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void DeleteEdges( const TArray<FEdgeID>& EdgeIDsToDelete, const bool bDeleteOrphanedVertices );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void CreateEmptyVertexRange( const int32 NumVerticesToCreate, TArray<FVertexID>& OutNewVertexIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void CreateVertices( const TArray<FVertexToCreate>& VerticesToCreate, TArray<FVertexID>& OutNewVertexIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void CreateVertexInstances( const TArray<FVertexInstanceToCreate>& VertexInstancesToCreate, TArray<FVertexInstanceID>& OutNewVertexInstanceIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void CreateEdges( const TArray<FEdgeToCreate>& EdgesToCreate, TArray<FEdgeID>& OutNewEdgeIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void CreatePolygons( const TArray<FPolygonToCreate>& PolygonsToCreate, TArray<FPolygonID>& OutNewPolygonIDs, TArray<FEdgeID>& OutNewEdgeIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void DeletePolygons( const TArray<FPolygonID>& PolygonIDsToDelete, const bool bDeleteOrphanedEdges, const bool bDeleteOrphanedVertices, const bool bDeleteOrphanedVertexInstances, const bool bDeleteEmptyPolygonGroups );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetVerticesAttributes( const TArray<FAttributesForVertex>& AttributesForVertices );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetVertexInstancesAttributes( const TArray<FAttributesForVertexInstance>& AttributesForVertexInstances );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetEdgesAttributes( const TArray<FAttributesForEdge>& AttributesForEdges );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetPolygonsVertexAttributes( const TArray<FVertexAttributesForPolygon>& VertexAttributesForPolygons );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void ChangePolygonsVertexInstances( const TArray<FChangeVertexInstancesForPolygon>& VertexInstancesForPolygons );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void TryToRemovePolygonEdge( const FEdgeID EdgeID, bool& bOutWasEdgeRemoved, FPolygonID& OutNewPolygonID );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void TryToRemoveVertex( const FVertexID VertexID, bool& bOutWasVertexRemoved, FEdgeID& OutNewEdgeID );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void ExtrudePolygons( const TArray<FPolygonID>& Polygons, const float ExtrudeDistance, const bool bKeepNeighborsTogether, TArray<FPolygonID>& OutNewExtrudedFrontPolygons );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void ExtendEdges( const TArray<FEdgeID>& EdgeIDs, const bool bWeldNeighbors, TArray<FEdgeID>& OutNewExtendedEdgeIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void ExtendVertices( const TArray<FVertexID>& VertexIDs, const bool bOnlyExtendClosestEdge, const FVector ReferencePosition, TArray<FVertexID>& OutNewExtendedVertexIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void InsetPolygons( const TArray<FPolygonID>& PolygonIDs, const float InsetFixedDistance, const float InsetProgressTowardCenter, const EInsetPolygonsMode Mode, TArray<FPolygonID>& OutNewCenterPolygonIDs, TArray<FPolygonID>& OutNewSidePolygonIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void BevelPolygons( const TArray<FPolygonID>& PolygonIDs, const float BevelFixedDistance, const float BevelProgressTowardCenter, TArray<FPolygonID>& OutNewCenterPolygonIDs, TArray<FPolygonID>& OutNewSidePolygonIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetVerticesCornerSharpness( const TArray<FVertexID>& VertexIDs, const TArray<float>& VerticesNewCornerSharpness );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetEdgesCreaseSharpness( const TArray<FEdgeID>& EdgeIDs, const TArray<float>& EdgesNewCreaseSharpness );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetEdgesHardness( const TArray<FEdgeID>& EdgeIDs, const TArray<bool>& EdgesNewIsHard );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetEdgesHardnessAutomatically( const TArray<FEdgeID>& EdgeIDs, const float MaxDotProductForSoftEdge );	// @todo mesheditor: Not used for anything yet.  Remove it?  Use it during import/convert?
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetEdgesVertices( const TArray<FVerticesForEdge>& VerticesForEdges );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void InsertPolygonPerimeterVertices( const FPolygonID PolygonID, const int32 InsertBeforeVertexNumber, const TArray<FVertexAndAttributes>& VerticesToInsert );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void RemovePolygonPerimeterVertices( const FPolygonID PolygonID, const int32 FirstVertexNumberToRemove, const int32 NumVerticesToRemove, const bool bDeleteOrphanedVertexInstances );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void FlipPolygons( const TArray<FPolygonID>& PolygonIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void TriangulatePolygons( const TArray<FPolygonID>& PolygonIDs, TArray<FPolygonID>& OutNewTrianglePolygons );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void CreatePolygonGroups( const TArray<FPolygonGroupToCreate>& PolygonGroupsToCreate, TArray<FPolygonGroupID>& OutNewPolygonGroupIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void DeletePolygonGroups( const TArray<FPolygonGroupID>& PolygonGroupIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void AssignPolygonsToPolygonGroups( const TArray<FPolygonGroupForPolygon>& PolygonGroupForPolygons, const bool bDeleteOrphanedPolygonGroups );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void WeldVertices( const TArray<FVertexID>& VertexIDs, FVertexID& OutNewVertexID );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void TessellatePolygons( const TArray<FPolygonID>& PolygonIDs, const ETriangleTessellationMode TriangleTessellationMode, TArray<FPolygonID>& OutNewPolygonIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetTextureCoordinateCount( const int32 NumTexCoords );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void QuadrangulateMesh( TArray<FPolygonID>& OutNewPolygonIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void GeneratePolygonTangentsAndNormals( const TArray<FPolygonID>& PolygonIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SplitPolygonalMesh( const FPlane& InPlane, TArray<FPolygonID>& PolygonIDs1, TArray<FPolygonID>& PolygonIDs2, TArray<FEdgeID>& BoundaryIDs);

	void GeometryHitTest(const FHitParamsIn& InParams, FHitParamsOut& OutParams);

protected:

	/** Adds a new change that can be used to undo a modification that happened to the mesh.  If bAllowUndo is false, then
	    the undo will not be stored */
	void AddUndo( TUniquePtr<FChange> NewUndo );

	void SearchSpatialDatabaseWithPredicate( TFunctionRef< bool( const FBox& Bounds ) > Predicate, TArray< FPolygonID >& OutPolygons ) const;

public:
	// @todo mesheditor: temporarily changed access to public so the adapter can call it when building the editable mesh from the source static mesh. Think about this some more.
	/** Refreshes the entire OpenSubdiv state for this mesh and generates subdivision geometry (if the mesh is configured to have subdivision levels) */
#if EDITABLE_MESH_USE_OPENSUBDIV
	void RefreshOpenSubdiv();
#endif

protected:

#if EDITABLE_MESH_USE_OPENSUBDIV
	/** Generates new limit surface data and caches it (if the mesh is configured to have subdivision levels).  This can be used to update the
	    limit surface after a non-topology-effective edit has happened */
	void GenerateOpenSubdivLimitSurfaceData();
#endif


private:

	void BevelOrInsetPolygons( const TArray<FPolygonID>& PolygonIDs, const float InsetFixedDistance, const float InsetProgressTowardCenter, const EInsetPolygonsMode Mode, const bool bShouldBevel, TArray<FPolygonID>& OutNewCenterPolygonIDs, TArray<FPolygonID>& OutNewSidePolygonIDs );

	/** Method used internally by the quadrangulate action */
	void QuadrangulatePolygonGroup( const FPolygonGroupID PolygonGroupID, TArray<FPolygonID>& OutNewPolygonIDs );

	/** Called during end modification to generate tangents and normals on the pending polygon list */
	void GenerateTangentsAndNormals();

	/** Called during end modification to flip tangents and normals on the pending polygon list */
	void FlipTangentsAndNormals();

	/** Called during end modification to retriangulate polygons in the pending polygon list */
	void RetriangulatePolygons();

	/** Called during end modification to merge vertex instances whose mergeable attributes are all equal */
	void MergeVertexInstances();

	/** Tries to incrementally update the octree based off geometry that has changed since last time.  If that's not
	    reasonable to do, then this will rebuild the entire octree from scratch */
	void UpdateOrRebuildOctree();

public:
	// @todo mesheditor: temporarily changed access to public so the adapter can call it when building the editable mesh from the source static mesh. Think about this some more.
	/** Rebuilds the octree */
	void RebuildOctree();


public:

	/** Pointer to the active mesh description for this editable mesh */
	FMeshDescription* MeshDescription;

	/** Owned mesh description for this editable mesh */
	FMeshDescription OwnedMeshDescription;

// @todo mesheditor: sort out member access. Currently StaticMesh adapter relies on accessing this stuff directly
//protected:

	/** The sub-mesh we came from */
	FEditableMeshSubMeshAddress SubMeshAddress;

	/** True if undo features are enabled on this mesh.  You're only allowed to call MakeUndo() if this is set to true. */
	bool bAllowUndo;

	/** True if compact is enabled on this mesh. If true, the mesh description will be sporadically compacted and tidied up. */
	bool bAllowCompact;

	/** When bAllowUndo is enabled, this will store the changes that can be applied to revert anything that happened to this
	    mesh since the last time that MakeUndo() was called. */
	TUniquePtr<FCompoundChangeInput> Undo;

	/** Adapters registered with this editable mesh */
	UPROPERTY()
	TArray<UEditableMeshAdapter*> Adapters;

	UEditableMeshAdapter* PrimaryAdapter;

	/** The number of texture coordinates stored on the vertices of this mesh */
	UPROPERTY( BlueprintReadOnly, Category="Editable Mesh" )
	int32 TextureCoordinateCount;

	/** List of polygons which need their tangent basis recalculating (and consequently their associated vertex instances) */
	TSet<FPolygonID> PolygonsPendingNewTangentBasis;

	/** List of polygons which need their tangent basis flipped (and consequently their associated vertex instances) */
	TSet<FPolygonID> PolygonsPendingFlipTangentBasis;

	/** List of polygons requiring retriangulation */
	TSet<FPolygonID> PolygonsPendingTriangulation;

	/** List of candidate vertices for merging instances */
	TSet<FVertexID> VerticesPendingMerging;

	/** True if StartModification() has been called.  Call EndModification() when you've finished changing the mesh. */
	bool bIsBeingModified;

	/** While the mesh is being edited (between calls to StartModification() and EndModification()), this is the type of modification being performed */
	EMeshModificationType CurrentModificationType;

	/** While the mesh is being edited (between calls to StartModification() and EndModification()), stores whether topology could be affected */
	EMeshTopologyChange CurrentToplogyChange;

	/** Counter to determine when we should compact data */
	UPROPERTY()
	int32 PendingCompactCounter;

	/** Data will be compacted after this many topology modifying actions. */
	static const int32 CompactFrequency = 50;

	/** How many levels to subdivide this mesh.  Zero will turn off subdivisions */
	UPROPERTY( BlueprintReadOnly, Category="Editable Mesh" ) 
	int32 SubdivisionCount;

#if EDITABLE_MESH_USE_OPENSUBDIV
	/** OpenSubdiv topology refiner object.  This is generated for meshes that have subdivision levels, and reused to generate new limit surfaces 
	    when geometry is moved.  When the mesh's topology changes, this object is regenerated from scratch. */
	TSharedPtr<OpenSubdiv::v3_2_0::Far::TopologyRefiner> OsdTopologyRefiner;

	/** Various cached arrays of mesh data in the form that OpenSubdiv expects to read it.  Required by GenerateOpenSubdivLimitSurfaceData(). */
	TArray<int> OsdNumVerticesPerFace;
	TArray<int> OsdVertexIndicesPerFace;
	TArray<int> OsdCreaseVertexIndexPairs;
	TArray<float> OsdCreaseWeights;
	TArray<int> OsdCornerVertexIndices;
	TArray<float> OsdCornerWeights;

	TArray<int> OsdFVarIndicesPerFace;
	struct FOsdFVarChannel
	{
		int ValueCount;
		int const* ValueIndices;
	};
	TArray<FOsdFVarChannel> OsdFVarChannels;
#endif

	/** The resulting limit surface geometry after GenerateOpenSubdivLimitSurfaceData() is called */
	FSubdivisionLimitData SubdivisionLimitData;

	/** Broadcast event when element IDs are remapped (for example, following Compact / Uncompact) */
	FElementIDsRemapped ElementIDsRemappedEvent;


	//
	// Spatial database
	//

	/** True if we should generate and maintain an octree spatial database for this mesh. */
	bool bAllowSpatialDatabase;

	/** Octree to accelerate spatial queries against the mesh.  This is never serialized, and only created and maintained if bAllowSpatialDatabase is enabled. */
	TSharedPtr<class FEditableMeshOctree> Octree;

	/** Maps our polygon IDs to octree element IDs */
	TMap<FPolygonID, FOctreeElementId> PolygonIDToOctreeElementIDMap;

	/** Polygons that were deleted since the last time our octree was refreshed */
	TSet<FPolygonID> DeletedOctreePolygonIDs;

	/** Newly-created polygons since the last time our octree was refreshed */
	TSet<FPolygonID> NewOctreePolygonIDs;

	friend struct FEditableMeshOctreeSemantics;	// NOTE: Allows inline access to PolygonIDToOctreeElementIDMap


};
