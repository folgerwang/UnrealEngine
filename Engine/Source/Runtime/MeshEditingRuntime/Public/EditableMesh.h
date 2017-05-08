// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EditableMeshTypes.h"
#include "Change.h"		// For TUniquePtr<FChange>
#include "EditableMesh.generated.h"


// @todo mesheditor: Comment these classes and enums!

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


UCLASS( abstract, BlueprintType )
class MESHEDITINGRUNTIME_API UEditableMesh : public UObject
{
	GENERATED_BODY()

public:

	/** Default constructor that initializes good defaults for UEditableMesh */
	UEditableMesh();

	// UObject interface
	virtual void Serialize( FArchive& Ar ) override;
	virtual void PostLoad() override;

	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) virtual void RebuildRenderMesh() PURE_VIRTUAL(,);
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) virtual void StartModification( const EMeshModificationType MeshModificationType, const EMeshTopologyChange MeshTopologyChange ) PURE_VIRTUAL(,);
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) virtual void EndModification( const bool bFromUndo = false ) PURE_VIRTUAL(,);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual bool IsCommitted() const PURE_VIRTUAL(, return false;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual bool IsCommittedAsInstance() const PURE_VIRTUAL(, return false;);
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) virtual void Commit() PURE_VIRTUAL(,);
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) virtual UEditableMesh* CommitInstance( UPrimitiveComponent* ComponentToInstanceTo ) PURE_VIRTUAL(,return nullptr;);
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) virtual void Revert() PURE_VIRTUAL(,);
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) virtual UEditableMesh* RevertInstance() PURE_VIRTUAL(,return nullptr;);
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) virtual void PropagateInstanceChanges() PURE_VIRTUAL(,);

	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual int32 GetRenderingVertexCount() const PURE_VIRTUAL(, return 0;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual int32 GetRenderingVertexArraySize() const PURE_VIRTUAL(, return 0;);

	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual int32 GetVertexCount() const PURE_VIRTUAL(,return 0;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual int32 GetVertexArraySize() const PURE_VIRTUAL(,return 0;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual bool IsValidVertex( const FVertexID VertexID ) const PURE_VIRTUAL(,return false;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual FVector4 GetVertexAttribute( const FVertexID VertexID, const FName AttributeName, const int32 AttributeIndex ) const PURE_VIRTUAL(,return FVector4(0.0f););
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual int32 GetVertexConnectedEdgeCount( const FVertexID VertexID ) const PURE_VIRTUAL(,return 0;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual FEdgeID GetVertexConnectedEdge( const FVertexID VertexID, const int32 ConnectedEdgeNumber ) const PURE_VIRTUAL(,return FEdgeID::Invalid;);

	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual FVector4 GetEdgeAttribute( const FEdgeID EdgeID, const FName AttributeName, const int32 AttributeIndex ) const PURE_VIRTUAL(,return FVector4(0.0f););
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual int32 GetEdgeCount() const PURE_VIRTUAL(,return 0;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual int32 GetEdgeArraySize() const PURE_VIRTUAL(,return 0;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual bool IsValidEdge( const FEdgeID EdgeID ) const PURE_VIRTUAL(,return false;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual FVertexID GetEdgeVertex( const FEdgeID EdgeID, const int32 EdgeVertexNumber ) const PURE_VIRTUAL(,return FVertexID::Invalid;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual int32 GetEdgeConnectedPolygonCount( const FEdgeID EdgeID ) const PURE_VIRTUAL(,return 0;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual FPolygonRef GetEdgeConnectedPolygon( const FEdgeID EdgeID, const int32 ConnectedPolygonNumber ) const PURE_VIRTUAL( , return FPolygonRef(););

	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual int32 GetSectionCount() const PURE_VIRTUAL(,return 0;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual int32 GetSectionArraySize() const PURE_VIRTUAL(,return 0;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual bool IsValidSection( const FSectionID SectionID ) const PURE_VIRTUAL(,return false;);

	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual int32 GetPolygonCount( const FSectionID SectionID ) const PURE_VIRTUAL(,return 0;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual int32 GetPolygonArraySize( const FSectionID SectionID ) const PURE_VIRTUAL(,return 0;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual bool IsValidPolygon( const FPolygonRef PolygonRef ) const PURE_VIRTUAL(,return false;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual int32 GetTriangleCount( const FSectionID SectionID ) const PURE_VIRTUAL(,return 0;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual int32 GetTriangleArraySize( const FSectionID SectionID ) const PURE_VIRTUAL(,return 0;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual int32 GetPolygonPerimeterVertexCount( const FPolygonRef PolygonRef ) const PURE_VIRTUAL(,return 0;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual FVertexID GetPolygonPerimeterVertex( const FPolygonRef PolygonRef, const int32 PolygonVertexNumber ) const PURE_VIRTUAL(,return FVertexID::Invalid;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual FVector4 GetPolygonPerimeterVertexAttribute( const FPolygonRef PolygonRef, const int32 PolygonVertexNumber, const FName AttributeName, const int32 AttributeIndex ) const PURE_VIRTUAL(,return FVector4(0.0f););
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual int32 GetPolygonHoleCount( const FPolygonRef PolygonRef ) const PURE_VIRTUAL(,return 0;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual int32 GetPolygonHoleVertexCount( const FPolygonRef PolygonRef, const int32 HoleNumber ) const PURE_VIRTUAL(,return 0;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual FVertexID GetPolygonHoleVertex( const FPolygonRef PolygonRef, const int32 HoleNumber, const int32 PolygonVertexNumber ) const PURE_VIRTUAL(,return FVertexID::Invalid;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual FVector4 GetPolygonHoleVertexAttribute( const FPolygonRef PolygonRef, const int32 HoleNumber, const int32 PolygonVertexNumber, const FName AttributeName, const int32 AttributeIndex ) const PURE_VIRTUAL(,return FVector4(0.0f););
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual int32 GetPolygonTriangulatedTriangleCount( const FPolygonRef PolygonRef ) const PURE_VIRTUAL(,return 0;);
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) virtual FVector GetPolygonTriangulatedTriangleVertexPosition( const FPolygonRef PolygonRef, const int32 PolygonTriangleNumber, const int32 TriangleVertexNumber ) const PURE_VIRTUAL(,return FVector(0.0f););

	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) virtual void RetriangulatePolygons( const TArray<FPolygonRef>& PolygonRefs, const bool bOnlyOnUndo ) PURE_VIRTUAL(,);

protected:

	virtual void DeleteOrphanVertices_Internal( const TArray<FVertexID>& VertexIDsToDelete ) PURE_VIRTUAL( , );
	virtual void SetVertexAttribute_Internal( const FVertexID VertexID, const FName AttributeName, const int32 AttributeIndex, const FVector4& NewAttributeValue ) PURE_VIRTUAL( , );
	virtual void CreateEmptyVertexRange_Internal( const int32 NumVerticesToAdd, const TArray<FVertexID>* OverrideVertexIDsForRedo, TArray<FVertexID>& OutNewVertexIDs ) PURE_VIRTUAL(,);
	virtual void CreateEdge_Internal( const FVertexID VertexIDA, const FVertexID VertexIDB, const TArray<FPolygonRef>& ConnectedPolygons, const FEdgeID OverrideEdgeIDForRedo, FEdgeID& OutNewEdgeID ) PURE_VIRTUAL(,);
	virtual void SetEdgeAttribute_Internal( const FEdgeID EdgeID, const FName AttributeName, const int32 AttributeIndex, const FVector4& NewAttributeValue ) PURE_VIRTUAL( , );
	virtual void DeleteEdges_Internal( const TArray<FEdgeID>& EdgeIDsToDelete, const bool bDeleteOrphanedVertices ) PURE_VIRTUAL(,);
	virtual void SetEdgeVertices_Internal( const FEdgeID EdgeID, const FVertexID NewVertexID0, const FVertexID NewVertexID1 ) PURE_VIRTUAL(,);
	virtual void CreatePolygon_Internal( const FSectionID SectionID, const TArray<FVertexID>& VertexIDs, const TArray<TArray<FVertexID>>& VertexIDsForEachHole, const FPolygonID OverridePolygonIDForRedo, FPolygonRef& OutNewPolygonRef, TArray<FEdgeID>& OutNewEdgeIDs ) PURE_VIRTUAL( ,);
	virtual void DeletePolygon_Internal( const FPolygonRef PolygonRef, const bool bDeleteOrphanedEdges, const bool bDeleteOrphanedVertices, const bool bDeleteEmptySections ) PURE_VIRTUAL(,);
	virtual void SetPolygonPerimeterVertexAttribute_Internal( const FPolygonRef PolygonRef, const int32 PolygonVertexNumber, const FName AttributeName, const int32 AttributeIndex, const FVector4& NewAttributeValue ) PURE_VIRTUAL( , );
	virtual void SetPolygonHoleVertexAttribute_Internal( const FPolygonRef PolygonRef, const int32 HoleNumber, const int32 PolygonVertexNumber, const FName AttributeName, const int32 AttributeIndex, const FVector4& NewAttributeValue ) PURE_VIRTUAL( , );
	virtual void InsertPolygonPerimeterVertices_Internal( const FPolygonRef PolygonRef, const int32 InsertBeforeVertexNumber, const TArray<FVertexAndAttributes>& VerticesToInsert ) PURE_VIRTUAL(,);
	virtual void RemovePolygonPerimeterVertices_Internal( const FPolygonRef PolygonRef, const int32 FirstVertexNumberToRemove, const int32 NumVerticesToRemove ) PURE_VIRTUAL(,);
	virtual FSectionID GetSectionIDFromMaterial_Internal( UMaterialInterface* Material, bool bCreateNewSectionIfNotFound ) PURE_VIRTUAL(, return FSectionID::Invalid; );
	virtual FSectionID CreateSection_Internal( const FSectionToCreate& SectionToCreate ) PURE_VIRTUAL(,return FSectionID::Invalid;);
	virtual void DeleteSection_Internal( const FSectionID SectionID ) PURE_VIRTUAL(,);

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
	 * @return	Returns true if there are any existing tracked changes that can be undo right now
	 */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" )
	bool AnyChangesToUndo() const;


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

	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static const TArray<FName>& GetValidVertexAttributes();
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static const TArray<FName>& GetValidPolygonVertexAttributes();
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static const TArray<FName>& GetValidEdgeAttributes();
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FVertexID InvalidVertexID()
	{
		return FVertexID::Invalid;
	}
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FEdgeID InvalidEdgeID()
	{
		return FEdgeID::Invalid;
	}
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FSectionID InvalidSectionID()
	{
		return FSectionID::Invalid;
	}
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FPolygonID InvalidPolygonID()
	{
		return FPolygonID::Invalid;
	}
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FPolygonRef InvalidPolygonRef()
	{
		return FPolygonRef::Invalid;
	}
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FVertexID MakeVertexID( const int32 VertexIndex )
	{
		return FVertexID( VertexIndex );
	}
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FEdgeID MakeEdgeID( const int32 EdgeIndex )
	{
		return FEdgeID( EdgeIndex );
	}
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FSectionID MakeSectionID( const int32 SectionIndex )
	{
		return FSectionID( SectionIndex );
	}
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FPolygonID MakePolygonID( const int32 PolygonIndex )
	{
		return FPolygonID( PolygonIndex );
	}

	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) int32 GetMaxAttributeIndex( const FName AttributeName ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FSectionID GetFirstValidSection() const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) int32 GetTotalPolygonCount() const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) int32 GetTextureCoordinateCount() const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) int32 GetSubdivisionCount() const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) bool IsPreviewingSubdivisions() const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetVertexConnectedEdges( const FVertexID VertexID, TArray<FEdgeID>& OutConnectedEdgeIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetVertexConnectedPolygons( const FVertexID VertexID, TArray<FPolygonRef>& OutConnectedPolygonRefs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetVertexAdjacentVertices( const FVertexID VertexID, TArray< FVertexID >& OutAdjacentVertexIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetEdgeVertices( const FEdgeID EdgeID, FVertexID& OutEdgeVertexID0, FVertexID& OutEdgeVertexID1 ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetEdgeConnectedPolygons( const FEdgeID EdgeID, TArray<FPolygonRef>& OutConnectedPolygonRefs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetEdgeLoopElements( const FEdgeID EdgeID, TArray<FEdgeID>& EdgeLoopIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FEdgeID GetEdgeThatConnectsVertices( const FVertexID VertexID0, const FVertexID VertexID1 ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) int32 GetPolygonPerimeterEdgeCount( const FPolygonRef PolygonRef ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) int32 GetPolygonHoleEdgeCount( const FPolygonRef PolygonRef, const int32 HoleNumber ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetPolygonPerimeterVertices( const FPolygonRef PolygonRef, TArray<FVertexID>& OutPolygonPerimeterVertexIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetPolygonHoleVertices( const FPolygonRef PolygonRef, const int32 HoleNumber, TArray<FVertexID>& OutHoleVertexIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FEdgeID GetPolygonPerimeterEdge( const FPolygonRef PolygonRef, const int32 PerimeterEdgeNumber, bool& bOutEdgeWindingIsReversedForPolygon ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FEdgeID GetPolygonHoleEdge( const FPolygonRef PolygonRef, const int32 HoleNumber, const int32 HoleEdgeNumber ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetPolygonPerimeterEdges( const FPolygonRef PolygonRef, TArray<FEdgeID>& OutPolygonPerimeterEdgeIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetPolygonHoleEdges( const FPolygonRef PolygonRef, const int32 HoleNumber, TArray<FEdgeID>& OutHoleEdgeIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void GetPolygonAdjacentPolygons( const FPolygonRef PolygonRef, TArray<FPolygonRef>& OutAdjacentPolygons ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) int32 FindPolygonPerimeterVertexNumberForVertex( const FPolygonRef PolygonRef, const FVertexID VertexID ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) int32 FindPolygonHoleVertexNumberForVertex( const FPolygonRef PolygonRef, const int32 HoleNumber, const FVertexID VertexID ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) int32 FindPolygonPerimeterEdgeNumberForVertices( const FPolygonRef PolygonRef, const FVertexID EdgeVertexID0, const FVertexID EdgeVertexID1 ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) int32 FindPolygonHoleEdgeNumberForVertices( const FPolygonRef PolygonRef, const int32 HoleNumber, const FVertexID EdgeVertexID0, const FVertexID EdgeVertexID1 ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FBox ComputeBoundingBox() const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FBoxSphereBounds ComputeBoundingBoxAndSphere() const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FVector ComputePolygonCenter( const FPolygonRef PolygonRef ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FPlane ComputePolygonPlane( const FPolygonRef PolygonRef ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FVector ComputePolygonNormal( const FPolygonRef PolygonRef ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) FVector ComputePolygonPerimeterVertexNormal( const FPolygonRef PolygonRef, const int32 PolygonVertexNumber ) const;
	void ComputePolygonTriangulation( const FPolygonRef PolygonRef, TArray<int32>& OutPerimeterVertexNumbersForTriangles ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) const FSubdivisionLimitData& GetSubdivisionLimitData() const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void ComputePolygonTriangulation( const FPolygonRef PolygonRef, TArray<FVertexID>& OutTrianglesVertexIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) bool ComputeBarycentricWeightForPointOnPolygon( const FPolygonRef PolygonRef, const FVector PointOnPolygon, TArray<int32>& PerimeterVertexIndices, FVector& TriangleVertexWeights ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void ComputeTextureCoordinatesForPointOnPolygon( const FPolygonRef PolygonRef, const FVector PointOnPolygon, bool& bOutWereTextureCoordinatesFound, TArray<FVector4>& OutInterpolatedTextureCoordinates );
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void ComputePolygonsSharedEdges( const TArray<FPolygonRef>& PolygonRefs, TArray<FEdgeID>& OutSharedEdgeIDs ) const;
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) void FindPolygonLoop( const FEdgeID EdgeID, TArray<FEdgeID>& OutEdgeLoopEdgeIDs, TArray<FEdgeID>& OutFlippedEdgeIDs, TArray<FEdgeID>& OutReversedEdgeIDPathToTake, TArray<FPolygonRef>& OutPolygonRefsToSplit ) const;


	// Low poly editing features
	// @todo mesheditor: Move elsewhere
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetSubdivisionCount( const int32 NewSubdivisionCount );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void MoveVertices( const TArray<FVertexToMove>& VerticesToMove );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void CreateMissingPolygonPerimeterEdges( const FPolygonRef PolygonRef, TArray<FEdgeID>& OutNewEdgeIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void CreateMissingPolygonHoleEdges( const FPolygonRef PolygonRef, const int32 HoleNumber, TArray<FEdgeID>& OutNewEdgeIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SplitEdge( const FEdgeID EdgeID, const TArray<float>& Splits, TArray<FVertexID>& OutNewVertexIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void InsertEdgeLoop( const FEdgeID EdgeID, const TArray<float>& Splits, TArray<FEdgeID>& OutNewEdgeIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SplitPolygons( const TArray<FPolygonToSplit>& PolygonsToSplit, TArray<FEdgeID>& OutNewEdgeIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void DeleteEdgeAndConnectedPolygons( const FEdgeID EdgeID, const bool bDeleteOrphanedEdges, const bool bDeleteOrphanedVertices, const bool bDeleteEmptySections );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void DeleteVertexAndConnectedEdgesAndPolygons( const FVertexID VertexID, const bool bDeleteOrphanedEdges, const bool bDeleteOrphanedVertices, const bool bDeleteEmptySections );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void CreateEmptyVertexRange( const int32 NumVerticesToAdd, TArray<FVertexID>& OutNewVertexIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void DeleteOrphanVertices( const TArray<FVertexID>& VertexIDsToDelete );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void DeleteEdges( const TArray<FEdgeID>& EdgeIDsToDelete, const bool bDeleteOrphanedVertices );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void CreateVertices( const TArray<FVertexToCreate>& VerticesToCreate, TArray<FVertexID>& OutNewVertexIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void CreateEdges( const TArray<FEdgeToCreate>& EdgesToCreate, TArray<FEdgeID>& OutNewEdgeIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void CreatePolygons( const TArray<FPolygonToCreate>& PolygonsToCreate, TArray<FPolygonRef>& OutNewPolygonRefs, TArray<FEdgeID>& OutNewEdgeIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void DeletePolygons( const TArray<FPolygonRef>& PolygonRefsToDelete, const bool bDeleteOrphanedEdges, const bool bDeleteOrphanedVertices, const bool bDeleteEmptySections );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetVerticesAttributes( const TArray<FAttributesForVertex>& AttributesForVertices );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetEdgesAttributes( const TArray<FAttributesForEdge>& AttributesForEdges );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetPolygonsVertexAttributes( const TArray<FVertexAttributesForPolygon>& VertexAttributesForPolygons );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void TryToRemovePolygonEdge( const FEdgeID EdgeID, bool& bOutWasEdgeRemoved, FPolygonRef& OutNewPolygonRef );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void TryToRemoveVertex( const FVertexID VertexID, bool& bOutWasVertexRemoved, FEdgeID& OutNewEdgeID );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void ExtrudePolygons( const TArray<FPolygonRef>& Polygons, const float ExtrudeDistance, const bool bKeepNeighborsTogether, TArray<FPolygonRef>& OutNewExtrudedFrontPolygons );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void ExtendEdges( const TArray<FEdgeID>& EdgeIDs, const bool bWeldNeighbors, TArray<FEdgeID>& OutNewExtendedEdgeIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void ExtendVertices( const TArray<FVertexID>& VertexIDs, const bool bOnlyExtendClosestEdge, const FVector ReferencePosition, TArray<FVertexID>& OutNewExtendedVertexIDs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void InsetPolygons( const TArray<FPolygonRef>& PolygonRefs, const float InsetFixedDistance, const float InsetProgressTowardCenter, const EInsetPolygonsMode Mode, TArray<FPolygonRef>& OutNewCenterPolygonRefs, TArray<FPolygonRef>& OutNewSidePolygonRefs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void BevelPolygons( const TArray<FPolygonRef>& PolygonRefs, const float BevelFixedDistance, const float BevelProgressTowardCenter, TArray<FPolygonRef>& OutNewCenterPolygonRefs, TArray<FPolygonRef>& OutNewSidePolygonRefs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void GenerateNormalsAndTangentsForPolygons( const TArray< FPolygonRef >& PolygonRefs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void GenerateNormalsAndTangentsForPolygonsAndAdjacents( const TArray< FPolygonRef >& PolygonRefs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetVerticesCornerSharpness( const TArray<FVertexID>& VertexIDs, const TArray<float>& VerticesNewCornerSharpness );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetEdgesCreaseSharpness( const TArray<FEdgeID>& EdgeIDs, const TArray<float>& EdgesNewCreaseSharpness );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetEdgesHardness( const TArray<FEdgeID>& EdgeIDs, const TArray<bool>& EdgesNewIsHard );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetEdgesHardnessAutomatically( const TArray<FEdgeID>& EdgeIDs, const float MaxDotProductForSoftEdge );	// @todo mesheditor: Not used for anything yet.  Remove it?  Use it during import/convert?
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetEdgesVertices( const TArray<FVerticesForEdge>& VerticesForEdges );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void InsertPolygonPerimeterVertices( const FPolygonRef PolygonRef, const int32 InsertBeforeVertexNumber, const TArray<FVertexAndAttributes>& VerticesToInsert );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void RemovePolygonPerimeterVertices( const FPolygonRef PolygonRef, const int32 FirstVertexNumberToRemove, const int32 NumVerticesToRemove );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void FlipPolygons( const TArray<FPolygonRef>& PolygonRefs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void TriangulatePolygons( const TArray<FPolygonRef>& PolygonRefs, TArray<FPolygonRef>& OutNewTrianglePolygons );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) FSectionID CreateSection( const FSectionToCreate& SectionToCreate );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void DeleteSection( const FSectionID SectionID );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void AssignMaterialToPolygons( const TArray<FPolygonRef>& PolygonRefs, UMaterialInterface* Material, TArray<FPolygonRef>& NewPolygonRefs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void WeldVertices( const TArray<FVertexID>& VertexIDs, FVertexID& OutNewVertexID );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void TessellatePolygons( const TArray<FPolygonRef>& PolygonRefs, const ETriangleTessellationMode TriangleTessellationMode, TArray<FPolygonRef>& OutNewPolygonRefs );
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void SetTextureCoordinateCount(int32 NumTexCoords);
	UFUNCTION( BlueprintCallable, Category="Editable Mesh" ) void QuadrangulateMesh( TArray<FPolygonRef>& NewPolygonRefs );
protected:

	/** Adds a new change that can be used to undo a modification that happened to the mesh.  If bAllowUndo is false, then
	    the undo will not be stored */
	void AddUndo( TUniquePtr<FChange> NewUndo );

	/** Refreshes the entire OpenSubdiv state for this mesh and generates subdivision geometry (if the mesh is configured to have subdivision levels) */
	void RefreshOpenSubdiv();

	/** Generates new limit surface data and caches it (if the mesh is configured to have subdivision levels).  This can be used to update the
	    limit surface after a non-topology-effective edit has happened */
	void GenerateOpenSubdivLimitSurfaceData();


private:

	void BevelOrInsetPolygons( const TArray<FPolygonRef>& PolygonRefs, const float InsetFixedDistance, const float InsetProgressTowardCenter, const EInsetPolygonsMode Mode, const bool bShouldBevel, TArray<FPolygonRef>& OutNewCenterPolygonRefs, TArray<FPolygonRef>& OutNewSidePolygonRefs );

	void GenerateTangentsForPolygons( const TArray< FPolygonRef >& PolygonRefs );


protected:

	/** The sub-mesh we came from */
	FEditableMeshSubMeshAddress SubMeshAddress;

	/** True if undo features are enabled on this mesh.  You're only allowed to call MakeUndo() if this is set to true. */
	bool bAllowUndo;

	/** When bAllowUndo is enabled, this will store the changes that can be applied to revert anything that happened to this
	    mesh since the last time that MakeUndo() was called. */
	TUniquePtr<FCompoundChangeInput> Undo;

	/** The number of texture coordinates stored on the vertices of this mesh */
	UPROPERTY( BlueprintReadOnly, Category="Editable Mesh" )
	int32 TextureCoordinateCount;

	/** How many levels to subdivide this mesh.  Zero will turn off subdivisions */
	UPROPERTY( BlueprintReadOnly, Category="Editable Mesh" ) 
	int32 SubdivisionCount;

	/** True if StartModification() has been called.  Call EndModification() when you've finished changing the mesh. */
	bool bIsBeingModified;

	/** While the mesh is being edited (between calls to StartModification() and EndModification()), this is the type of modification being performed */
	EMeshModificationType CurrentModificationType;

	/** While the mesh is being edited (between calls to StartModification() and EndModification()), stores whether topology could be affected */
	EMeshTopologyChange CurrentToplogyChange;

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

	/** The resulting limit surface geometry after GenerateOpenSubdivLimitSurfaceData() is called */
	FSubdivisionLimitData SubdivisionLimitData;
};
