// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "MeshTypes.h"
#include "MeshElementArray.h"
#include "MeshAttributeArray.h"
#include "UObject/EditorObjectVersion.h"
#include "UObject/ReleaseObjectVersion.h"
#include "Serialization/BulkData.h"
#include "Serialization/CustomVersion.h"
#include "MeshDescription.generated.h"

enum
{
	//Remove the _MD when FRawMesh will be remove
	MAX_MESH_TEXTURE_COORDS_MD = 8,
};

struct FMeshVertex
{
	FMeshVertex()
	{}

	/** All of vertex instances which reference this vertex (for split vertex support) */
	TArray<FVertexInstanceID> VertexInstanceIDs;

	/** The edges connected to this vertex */
	TArray<FEdgeID> ConnectedEdgeIDs;

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FMeshVertex& Vertex )
	{
		if( Ar.IsLoading() && Ar.CustomVer( FReleaseObjectVersion::GUID ) < FReleaseObjectVersion::MeshDescriptionNewSerialization )
		{
			Ar << Vertex.VertexInstanceIDs;
			Ar << Vertex.ConnectedEdgeIDs;
		}

		return Ar;
	}
};


struct FMeshVertexInstance
{
	FMeshVertexInstance()
		: VertexID( FVertexID::Invalid )
	{}

	/** The vertex this is instancing */
	FVertexID VertexID;

	/** List of connected polygons */
	TArray<FPolygonID> ConnectedPolygons;

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FMeshVertexInstance& VertexInstance )
	{
		Ar << VertexInstance.VertexID;
		if( Ar.IsLoading() && Ar.CustomVer( FReleaseObjectVersion::GUID ) < FReleaseObjectVersion::MeshDescriptionNewSerialization )
		{
			Ar << VertexInstance.ConnectedPolygons;
		}

		return Ar;
	}
};


struct FMeshEdge
{
	FMeshEdge()
	{
		VertexIDs[ 0 ] = FVertexID::Invalid;
		VertexIDs[ 1 ] = FVertexID::Invalid;
	}

	/** IDs of the two editable mesh vertices that make up this edge.  The winding direction is not defined. */
	FVertexID VertexIDs[ 2 ];

	/** The polygons that share this edge.  It's best if there are always only two polygons that share
	    the edge, and those polygons are facing the same direction */
	TArray<FPolygonID> ConnectedPolygons;

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FMeshEdge& Edge )
	{
		Ar << Edge.VertexIDs[ 0 ];
		Ar << Edge.VertexIDs[ 1 ];
		if( Ar.IsLoading() && Ar.CustomVer( FReleaseObjectVersion::GUID ) < FReleaseObjectVersion::MeshDescriptionNewSerialization )
		{
			Ar << Edge.ConnectedPolygons;
		}

		return Ar;
	}
};


struct FMeshPolygonContour
{
	/** The ordered list of vertex instances which make up the polygon contour. The winding direction is counter-clockwise. */
	TArray<FVertexInstanceID> VertexInstanceIDs;

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FMeshPolygonContour& Contour )
	{
		Ar << Contour.VertexInstanceIDs;
		return Ar;
	}
};


USTRUCT( BlueprintType )
struct FMeshTriangle
{
	GENERATED_BODY()

	FMeshTriangle()
		: VertexInstanceID0( FVertexInstanceID::Invalid ),
		  VertexInstanceID1( FVertexInstanceID::Invalid ),
		  VertexInstanceID2( FVertexInstanceID::Invalid )
	{}

	/** First vertex instance that makes up this triangle.  Indices must be ordered counter-clockwise. */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FVertexInstanceID VertexInstanceID0;

	/** Second vertex instance that makes up this triangle.  Indices must be ordered counter-clockwise. */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FVertexInstanceID VertexInstanceID1;

	/** Third vertex instance that makes up this triangle.  Indices must be ordered counter-clockwise. */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FVertexInstanceID VertexInstanceID2;

	/** Gets the specified triangle vertex instance ID.  Pass an index between 0 and 2 inclusive. */
	inline FVertexInstanceID GetVertexInstanceID( const int32 Index ) const
	{
		checkSlow( Index >= 0 && Index <= 2 );
		return reinterpret_cast<const FVertexInstanceID*>( this )[ Index ];
	}

	/** Sets the specified triangle vertex instance ID.  Pass an index between 0 and 2 inclusive, and the new vertex instance ID to store. */
	inline void SetVertexInstanceID( const int32 Index, const FVertexInstanceID NewVertexInstanceID )
	{
		checkSlow( Index >= 0 && Index <= 2 );
		( reinterpret_cast<FVertexInstanceID*>( this )[ Index ] ) = NewVertexInstanceID;
	}

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FMeshTriangle& Triangle )
	{
		Ar << Triangle.VertexInstanceID0;
		Ar << Triangle.VertexInstanceID1;
		Ar << Triangle.VertexInstanceID2;

		return Ar;
	}
};


struct FMeshPolygon
{
	FMeshPolygon()
		: PolygonGroupID( FPolygonGroupID::Invalid )
	{}

	/** The outer boundary edges of this polygon */
	FMeshPolygonContour PerimeterContour;

	/** List of triangles which make up this polygon */
	TArray<FMeshTriangle> Triangles;

	/** The polygon group which contains this polygon */
	FPolygonGroupID PolygonGroupID;

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FMeshPolygon& Polygon )
	{
		Ar << Polygon.PerimeterContour;
		if (Ar.IsLoading() && Ar.CustomVer( FEditorObjectVersion::GUID ) < FEditorObjectVersion::MeshDescriptionRemovedHoles)
		{
			TArray<FMeshPolygonContour> Empty;
			Ar << Empty;
		}
		if( Ar.IsLoading() && Ar.CustomVer( FReleaseObjectVersion::GUID ) < FReleaseObjectVersion::MeshDescriptionNewSerialization )
		{
			Ar << Polygon.Triangles;
		}
		Ar << Polygon.PolygonGroupID;

		return Ar;
	}
};


struct FMeshPolygonGroup
{
	FMeshPolygonGroup()
	{}

	/** All polygons in this group */
	TArray<FPolygonID> Polygons;

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FMeshPolygonGroup& PolygonGroup )
	{
		if( Ar.IsLoading() && Ar.CustomVer( FReleaseObjectVersion::GUID ) < FReleaseObjectVersion::MeshDescriptionNewSerialization )
		{
			Ar << PolygonGroup.Polygons;
		}

		return Ar;
	}
};


/** Define container types */
using FVertexArray = TMeshElementArray<FMeshVertex, FVertexID>;
using FVertexInstanceArray = TMeshElementArray<FMeshVertexInstance, FVertexInstanceID>;
using FEdgeArray = TMeshElementArray<FMeshEdge, FEdgeID>;
using FPolygonArray = TMeshElementArray<FMeshPolygon, FPolygonID>;
using FPolygonGroupArray = TMeshElementArray<FMeshPolygonGroup, FPolygonGroupID>;

/** Define aliases for element attributes */
template <typename AttributeType> using TVertexAttributeIndicesArray = TAttributeIndicesArray<AttributeType, FVertexID>;
template <typename AttributeType> using TVertexInstanceAttributeIndicesArray = TAttributeIndicesArray<AttributeType, FVertexInstanceID>;
template <typename AttributeType> using TEdgeAttributeIndicesArray = TAttributeIndicesArray<AttributeType, FEdgeID>;
template <typename AttributeType> using TPolygonAttributeIndicesArray = TAttributeIndicesArray<AttributeType, FPolygonID>;
template <typename AttributeType> using TPolygonGroupAttributeIndicesArray = TAttributeIndicesArray<AttributeType, FPolygonGroupID>;

template <typename AttributeType> using TVertexAttributeArray = TMeshAttributeArray<AttributeType, FVertexID>;
template <typename AttributeType> using TVertexInstanceAttributeArray = TMeshAttributeArray<AttributeType, FVertexInstanceID>;
template <typename AttributeType> using TEdgeAttributeArray = TMeshAttributeArray<AttributeType, FEdgeID>;
template <typename AttributeType> using TPolygonAttributeArray = TMeshAttributeArray<AttributeType, FPolygonID>;
template <typename AttributeType> using TPolygonGroupAttributeArray = TMeshAttributeArray<AttributeType, FPolygonGroupID>;

template <typename AttributeType> using TVertexAttributesRef = TMeshAttributesRef<FVertexID, AttributeType>;
template <typename AttributeType> using TVertexInstanceAttributesRef = TMeshAttributesRef<FVertexInstanceID, AttributeType>;
template <typename AttributeType> using TEdgeAttributesRef = TMeshAttributesRef<FEdgeID, AttributeType>;
template <typename AttributeType> using TPolygonAttributesRef = TMeshAttributesRef<FPolygonID, AttributeType>;
template <typename AttributeType> using TPolygonGroupAttributesRef = TMeshAttributesRef<FPolygonGroupID, AttributeType>;

template <typename AttributeType> using TVertexAttributesConstRef = TMeshAttributesConstRef<FVertexID, AttributeType>;
template <typename AttributeType> using TVertexInstanceAttributesConstRef = TMeshAttributesConstRef<FVertexInstanceID, AttributeType>;
template <typename AttributeType> using TEdgeAttributesConstRef = TMeshAttributesConstRef<FEdgeID, AttributeType>;
template <typename AttributeType> using TPolygonAttributesConstRef = TMeshAttributesConstRef<FPolygonID, AttributeType>;
template <typename AttributeType> using TPolygonGroupAttributesConstRef = TMeshAttributesConstRef<FPolygonGroupID, AttributeType>;

template <typename AttributeType> using TVertexAttributesView = TMeshAttributesView<FVertexID, AttributeType>;
template <typename AttributeType> using TVertexInstanceAttributesView = TMeshAttributesView<FVertexInstanceID, AttributeType>;
template <typename AttributeType> using TEdgeAttributesView = TMeshAttributesView<FEdgeID, AttributeType>;
template <typename AttributeType> using TPolygonAttributesView = TMeshAttributesView<FPolygonID, AttributeType>;
template <typename AttributeType> using TPolygonGroupAttributesView = TMeshAttributesView<FPolygonGroupID, AttributeType>;

template <typename AttributeType> using TVertexAttributesConstView = TMeshAttributesConstView<FVertexID, AttributeType>;
template <typename AttributeType> using TVertexInstanceAttributesConstView = TMeshAttributesConstView<FVertexInstanceID, AttributeType>;
template <typename AttributeType> using TEdgeAttributesConstView = TMeshAttributesConstView<FEdgeID, AttributeType>;
template <typename AttributeType> using TPolygonAttributesConstView = TMeshAttributesConstView<FPolygonID, AttributeType>;
template <typename AttributeType> using TPolygonGroupAttributesConstView = TMeshAttributesConstView<FPolygonGroupID, AttributeType>;

UENUM()
enum class EComputeNTBsOptions : uint32
{
	None = 0x00000000,	// No flags
	Normals = 0x00000001, //Compute the normals
	Tangents = 0x00000002, //Compute the tangents
	WeightedNTBs = 0x00000004, //Use weight angle when computing NTBs to proportionally distribute the vertex instance contribution to the normal/tangent/binormal in a smooth group.    i.e. Weight solve the cylinder problem
};
ENUM_CLASS_FLAGS(EComputeNTBsOptions);


//USTRUCT()
struct MESHDESCRIPTION_API FMeshDescription
{
public:

	// Mesh description should be a moveable type.
	// Hence explicitly declare all the below as defaulted, to ensure they will be generated by the compiler.
	FMeshDescription() = default;
	~FMeshDescription() = default;
	FMeshDescription( const FMeshDescription& ) = default;
	FMeshDescription( FMeshDescription&& ) = default;
	FMeshDescription& operator=( const FMeshDescription& ) = default;
	FMeshDescription& operator=( FMeshDescription&& ) = default;

	friend MESHDESCRIPTION_API FArchive& operator<<( FArchive& Ar, FMeshDescription& MeshDescription );

	// Empty the meshdescription
	void Empty();

	// Return whether the mesh description is empty
	bool IsEmpty() const;

	FVertexArray& Vertices() { return VertexArray; }
	const FVertexArray& Vertices() const { return VertexArray; }

	FMeshVertex& GetVertex( const FVertexID VertexID ) { return VertexArray[ VertexID ]; }
	const FMeshVertex& GetVertex( const FVertexID VertexID ) const { return VertexArray[ VertexID ]; }

	FVertexInstanceArray& VertexInstances() { return VertexInstanceArray; }
	const FVertexInstanceArray& VertexInstances() const { return VertexInstanceArray; }

	FMeshVertexInstance& GetVertexInstance( const FVertexInstanceID VertexInstanceID ) { return VertexInstanceArray[ VertexInstanceID ]; }
	const FMeshVertexInstance& GetVertexInstance( const FVertexInstanceID VertexInstanceID ) const { return VertexInstanceArray[ VertexInstanceID ]; }

	FEdgeArray& Edges() { return EdgeArray; }
	const FEdgeArray& Edges() const { return EdgeArray; }

	FMeshEdge& GetEdge( const FEdgeID EdgeID ) { return EdgeArray[ EdgeID ]; }
	const FMeshEdge& GetEdge( const FEdgeID EdgeID ) const { return EdgeArray[ EdgeID ]; }

	FPolygonArray& Polygons() { return PolygonArray; }
	const FPolygonArray& Polygons() const { return PolygonArray; }

	FMeshPolygon& GetPolygon( const FPolygonID PolygonID ) { return PolygonArray[ PolygonID ]; }
	const FMeshPolygon& GetPolygon( const FPolygonID PolygonID ) const { return PolygonArray[ PolygonID ]; }

	FPolygonGroupArray& PolygonGroups() { return PolygonGroupArray; }
	const FPolygonGroupArray& PolygonGroups() const { return PolygonGroupArray; }

	FMeshPolygonGroup& GetPolygonGroup( const FPolygonGroupID PolygonGroupID ) { return PolygonGroupArray[ PolygonGroupID ]; }
	const FMeshPolygonGroup& GetPolygonGroup( const FPolygonGroupID PolygonGroupID ) const { return PolygonGroupArray[ PolygonGroupID ]; }

	TAttributesSet<FVertexID>& VertexAttributes() { return VertexAttributesSet; }
	const TAttributesSet<FVertexID>& VertexAttributes() const { return VertexAttributesSet; }

	TAttributesSet<FVertexInstanceID>& VertexInstanceAttributes() { return VertexInstanceAttributesSet; }
	const TAttributesSet<FVertexInstanceID>& VertexInstanceAttributes() const { return VertexInstanceAttributesSet; }

	TAttributesSet<FEdgeID>& EdgeAttributes() { return EdgeAttributesSet; }
	const TAttributesSet<FEdgeID>& EdgeAttributes() const { return EdgeAttributesSet; }

	TAttributesSet<FPolygonID>& PolygonAttributes() { return PolygonAttributesSet; }
	const TAttributesSet<FPolygonID>& PolygonAttributes() const { return PolygonAttributesSet; }

	TAttributesSet<FPolygonGroupID>& PolygonGroupAttributes() { return PolygonGroupAttributesSet; }
	const TAttributesSet<FPolygonGroupID>& PolygonGroupAttributes() const { return PolygonGroupAttributesSet; }

	/** Reserves space for this number of new vertices */
	void ReserveNewVertices( const int32 NumVertices )
	{
		VertexArray.Reserve( VertexArray.Num() + NumVertices );
	}

private:
	void CreateVertex_Internal( const FVertexID VertexID )
	{
		VertexAttributesSet.Insert( VertexID );
	}

public:
	/** Adds a new vertex to the mesh and returns its ID */
	FVertexID CreateVertex()
	{
		const FVertexID VertexID = VertexArray.Add();
		CreateVertex_Internal( VertexID );
		return VertexID;
	}

	/** Adds a new vertex to the mesh with the given ID */
	void CreateVertexWithID( const FVertexID VertexID )
	{
		VertexArray.Insert( VertexID );
		CreateVertex_Internal( VertexID );
	}

	/** Deletes a vertex from the mesh */
	void DeleteVertex( const FVertexID VertexID )
	{
		check( VertexArray[ VertexID ].ConnectedEdgeIDs.Num() == 0 );
		check( VertexArray[ VertexID ].VertexInstanceIDs.Num() == 0 );
		VertexArray.Remove( VertexID );
		VertexAttributesSet.Remove( VertexID );
	}

	/** Returns whether the passed vertex ID is valid */
	bool IsVertexValid( const FVertexID VertexID ) const
	{
		return VertexArray.IsValid( VertexID );
	}

	/** Reserves space for this number of new vertex instances */
	void ReserveNewVertexInstances( const int32 NumVertexInstances )
	{
		VertexInstanceArray.Reserve( VertexInstanceArray.Num() + NumVertexInstances );
	}

private:
	void CreateVertexInstance_Internal( const FVertexInstanceID VertexInstanceID, const FVertexID VertexID )
	{
		VertexInstanceArray[ VertexInstanceID ].VertexID = VertexID;
		check( !VertexArray[ VertexID ].VertexInstanceIDs.Contains( VertexInstanceID ) );
		VertexArray[ VertexID ].VertexInstanceIDs.Add( VertexInstanceID );
		VertexInstanceAttributesSet.Insert( VertexInstanceID );
	}

public:
	/** Adds a new vertex instance to the mesh and returns its ID */
	FVertexInstanceID CreateVertexInstance( const FVertexID VertexID )
	{
		const FVertexInstanceID VertexInstanceID = VertexInstanceArray.Add();
		CreateVertexInstance_Internal( VertexInstanceID, VertexID );
		return VertexInstanceID;
	}

	/** Adds a new vertex instance to the mesh with the given ID */
	void CreateVertexInstanceWithID( const FVertexInstanceID VertexInstanceID, const FVertexID VertexID )
	{
		VertexInstanceArray.Insert( VertexInstanceID );
		CreateVertexInstance_Internal( VertexInstanceID, VertexID );
	}

	/** Deletes a vertex instance from a mesh */
	void DeleteVertexInstance( const FVertexInstanceID VertexInstanceID, TArray<FVertexID>* InOutOrphanedVerticesPtr = nullptr )
	{
		check( VertexInstanceArray[ VertexInstanceID ].ConnectedPolygons.Num() == 0 );
		const FVertexID VertexID = VertexInstanceArray[ VertexInstanceID ].VertexID;
		verify( VertexArray[ VertexID ].VertexInstanceIDs.Remove( VertexInstanceID ) == 1 );
		if( InOutOrphanedVerticesPtr && VertexArray[ VertexID ].VertexInstanceIDs.Num() == 0 && VertexArray[ VertexID ].ConnectedEdgeIDs.Num() == 0 )
		{
			InOutOrphanedVerticesPtr->AddUnique( VertexID );
		}
		VertexInstanceArray.Remove( VertexInstanceID );
		VertexInstanceAttributesSet.Remove( VertexInstanceID );
	}

	/** Returns whether the passed vertex instance ID is valid */
	bool IsVertexInstanceValid( const FVertexInstanceID VertexInstanceID ) const
	{
		return VertexInstanceArray.IsValid( VertexInstanceID );
	}

	/** Reserves space for this number of new edges */
	void ReserveNewEdges( const int32 NumEdges )
	{
		EdgeArray.Reserve( EdgeArray.Num() + NumEdges );
	}

private:
	void CreateEdge_Internal( const FEdgeID EdgeID, const FVertexID VertexID0, const FVertexID VertexID1, const TArray<FPolygonID>& ConnectedPolygons )
	{
		FMeshEdge& Edge = EdgeArray[ EdgeID ];
		Edge.VertexIDs[ 0 ] = VertexID0;
		Edge.VertexIDs[ 1 ] = VertexID1;
		Edge.ConnectedPolygons = ConnectedPolygons;
		VertexArray[ VertexID0 ].ConnectedEdgeIDs.AddUnique( EdgeID );
		VertexArray[ VertexID1 ].ConnectedEdgeIDs.AddUnique( EdgeID );
		EdgeAttributesSet.Insert( EdgeID );
	}

public:
	/** Adds a new edge to the mesh and returns its ID */
	FEdgeID CreateEdge( const FVertexID VertexID0, const FVertexID VertexID1, const TArray<FPolygonID>& ConnectedPolygons = TArray<FPolygonID>() )
	{
		const FEdgeID EdgeID = EdgeArray.Add();
		CreateEdge_Internal( EdgeID, VertexID0, VertexID1, ConnectedPolygons );
		return EdgeID;
	}

	/** Adds a new edge to the mesh with the given ID */
	void CreateEdgeWithID( const FEdgeID EdgeID, const FVertexID VertexID0, const FVertexID VertexID1, const TArray<FPolygonID>& ConnectedPolygons = TArray<FPolygonID>() )
	{
		EdgeArray.Insert( EdgeID );
		CreateEdge_Internal( EdgeID, VertexID0, VertexID1, ConnectedPolygons );
	}

	/** Deletes an edge from a mesh */
	void DeleteEdge( const FEdgeID EdgeID, TArray<FVertexID>* InOutOrphanedVerticesPtr = nullptr )
	{
		FMeshEdge& Edge = EdgeArray[ EdgeID ];
		for( const FVertexID EdgeVertexID : Edge.VertexIDs )
		{
			FMeshVertex& Vertex = VertexArray[ EdgeVertexID ];
			verify( Vertex.ConnectedEdgeIDs.RemoveSingle( EdgeID ) == 1 );
			if( InOutOrphanedVerticesPtr && Vertex.ConnectedEdgeIDs.Num() == 0 )
			{
				check( Vertex.VertexInstanceIDs.Num() == 0 );  // We must already have deleted any vertex instances
				InOutOrphanedVerticesPtr->AddUnique( EdgeVertexID );
			}
		}
		EdgeArray.Remove( EdgeID );
		EdgeAttributesSet.Remove( EdgeID );
	}

	/** Returns whether the passed edge ID is valid */
	bool IsEdgeValid( const FEdgeID EdgeID ) const
	{
		return EdgeArray.IsValid( EdgeID );
	}

	/** Reserves space for this number of new polygons */
	void ReserveNewPolygons( const int32 NumPolygons )
	{
		PolygonArray.Reserve( PolygonArray.Num() + NumPolygons );
	}

	/** Pair of IDs representing the vertex instance ID on a contour, and the edge ID which starts at that point, winding counter-clockwise */
	struct FContourPoint
	{
		FVertexInstanceID VertexInstanceID;
		FEdgeID EdgeID;
	};

private:
	void CreatePolygon_Internal( const FPolygonID PolygonID, const FPolygonGroupID PolygonGroupID, const TArray<FContourPoint>& Perimeter );
	void CreatePolygon_Internal( const FPolygonID PolygonID, const FPolygonGroupID PolygonGroupID, const TArray<FVertexInstanceID>& VertexInstanceIDs, TArray<FEdgeID>* OutEdgeIDs );

public:
	/** Adds a new polygon to the mesh and returns its ID */
	UE_DEPRECATED(4.22, "Please use a different CreatePolygon() overload.")
	FPolygonID CreatePolygon( const FPolygonGroupID PolygonGroupID, const TArray<FContourPoint>& Perimeter )
	{
		const FPolygonID PolygonID = PolygonArray.Add();
		CreatePolygon_Internal( PolygonID, PolygonGroupID, Perimeter );
		return PolygonID;
	}

	FPolygonID CreatePolygon( const FPolygonGroupID PolygonGroupID, const TArray<FVertexInstanceID>& VertexInstanceIDs, TArray<FEdgeID>* OutEdgeIDs = nullptr )
	{
		const FPolygonID PolygonID = PolygonArray.Add();
		CreatePolygon_Internal( PolygonID, PolygonGroupID, VertexInstanceIDs, OutEdgeIDs );
		return PolygonID;
	}

	/** Adds a new polygon to the mesh with the given ID */
	UE_DEPRECATED(4.22, "Please use a different CreatePolygonWithID() overload.")
	void CreatePolygonWithID( const FPolygonID PolygonID, const FPolygonGroupID PolygonGroupID, const TArray<FContourPoint>& Perimeter )
	{
		PolygonArray.Insert( PolygonID );
		CreatePolygon_Internal( PolygonID, PolygonGroupID, Perimeter );
	}

	void CreatePolygonWithID( const FPolygonID PolygonID, const FPolygonGroupID PolygonGroupID, const TArray<FVertexInstanceID>& VertexInstanceIDs, TArray<FEdgeID>* OutEdgeIDs = nullptr )
	{
		PolygonArray.Insert( PolygonID );
		CreatePolygon_Internal( PolygonID, PolygonGroupID, VertexInstanceIDs, OutEdgeIDs );
	}

private:
	void DeletePolygonContour_Internal( const FPolygonID PolygonID, const TArray<FVertexInstanceID>& VertexInstanceIDs, TArray<FEdgeID>* InOutOrphanedEdgesPtr, TArray<FVertexInstanceID>* InOutOrphanedVertexInstancesPtr )
	{
		FVertexInstanceID LastVertexInstanceID = VertexInstanceIDs.Last();
		for( const FVertexInstanceID VertexInstanceID : VertexInstanceIDs )
		{
			FMeshVertexInstance& VertexInstance = VertexInstanceArray[ VertexInstanceID ];
			verify( VertexInstance.ConnectedPolygons.Remove( PolygonID ) == 1 );

			if( InOutOrphanedVertexInstancesPtr && VertexInstance.ConnectedPolygons.Num() == 0 )
			{
				InOutOrphanedVertexInstancesPtr->AddUnique( VertexInstanceID );
			}

			const FEdgeID EdgeID = GetVertexPairEdge( VertexInstanceArray[ LastVertexInstanceID ].VertexID, VertexInstanceArray[ VertexInstanceID ].VertexID );
			check( EdgeID != FEdgeID::Invalid );
			
			FMeshEdge& Edge = EdgeArray[ EdgeID ];
			verify( Edge.ConnectedPolygons.Remove( PolygonID ) == 1 );

			if( InOutOrphanedEdgesPtr && Edge.ConnectedPolygons.Num() == 0 )
			{
				InOutOrphanedEdgesPtr->AddUnique( EdgeID );
			}

			LastVertexInstanceID = VertexInstanceID;
		}
	}

public:
	/** Deletes a polygon from the mesh */
	void DeletePolygon( const FPolygonID PolygonID, TArray<FEdgeID>* InOutOrphanedEdgesPtr = nullptr, TArray<FVertexInstanceID>* InOutOrphanedVertexInstancesPtr = nullptr, TArray<FPolygonGroupID>* InOutOrphanedPolygonGroupsPtr = nullptr )
	{
		FMeshPolygon& Polygon = PolygonArray[ PolygonID ];
		DeletePolygonContour_Internal( PolygonID, Polygon.PerimeterContour.VertexInstanceIDs, InOutOrphanedEdgesPtr, InOutOrphanedVertexInstancesPtr );

		FMeshPolygonGroup& PolygonGroup = PolygonGroupArray[ Polygon.PolygonGroupID ];
		verify( PolygonGroup.Polygons.Remove( PolygonID ) == 1 );

		if( InOutOrphanedPolygonGroupsPtr && PolygonGroup.Polygons.Num() == 0 )
		{
			InOutOrphanedPolygonGroupsPtr->AddUnique( Polygon.PolygonGroupID );
		}

		PolygonArray.Remove( PolygonID );
		PolygonAttributesSet.Remove( PolygonID );
	}

	/** Returns whether the passed polygon ID is valid */
	bool IsPolygonValid( const FPolygonID PolygonID ) const
	{
		return PolygonArray.IsValid( PolygonID );
	}

	/** Reserves space for this number of new polygon groups */
	void ReserveNewPolygonGroups( const int32 NumPolygonGroups )
	{
		PolygonGroupArray.Reserve( PolygonGroupArray.Num() + NumPolygonGroups );
	}

private:
	void CreatePolygonGroup_Internal( const FPolygonGroupID PolygonGroupID )
	{
		PolygonGroupAttributesSet.Insert( PolygonGroupID );
	}

public:
	/** Adds a new polygon group to the mesh and returns its ID */
	FPolygonGroupID CreatePolygonGroup()
	{
		const FPolygonGroupID PolygonGroupID = PolygonGroupArray.Add();
		CreatePolygonGroup_Internal( PolygonGroupID );
		return PolygonGroupID;
	}

	/** Adds a new polygon group to the mesh with the given ID */
	void CreatePolygonGroupWithID( const FPolygonGroupID PolygonGroupID )
	{
		PolygonGroupArray.Insert( PolygonGroupID );
		CreatePolygonGroup_Internal( PolygonGroupID );
	}

	/** Deletes a polygon group from the mesh */
	void DeletePolygonGroup( const FPolygonGroupID PolygonGroupID )
	{
		check( PolygonGroupArray[ PolygonGroupID ].Polygons.Num() == 0 );
		PolygonGroupArray.Remove( PolygonGroupID );
		PolygonGroupAttributesSet.Remove( PolygonGroupID );
	}

	/** Returns whether the passed polygon group ID is valid */
	bool IsPolygonGroupValid( const FPolygonGroupID PolygonGroupID ) const
	{
		return PolygonGroupArray.IsValid( PolygonGroupID );
	}

//////////////////////////////////////////////////////////////////////////
// Meshdescription general functions

public:

	/** Returns whether a given vertex is orphaned, i.e. it doesn't form part of any polygon */
	bool IsVertexOrphaned( const FVertexID VertexID ) const;

	/** Returns the edge ID defined by the two given vertex IDs, if there is one; otherwise FEdgeID::Invalid */
	FEdgeID GetVertexPairEdge( const FVertexID VertexID0, const FVertexID VertexID1 ) const
	{
		for( const FEdgeID VertexConnectedEdgeID : VertexArray[ VertexID0 ].ConnectedEdgeIDs )
		{
			const FVertexID EdgeVertexID0 = EdgeArray[ VertexConnectedEdgeID ].VertexIDs[ 0 ];
			const FVertexID EdgeVertexID1 = EdgeArray[ VertexConnectedEdgeID ].VertexIDs[ 1 ];
			if( ( EdgeVertexID0 == VertexID0 && EdgeVertexID1 == VertexID1 ) || ( EdgeVertexID0 == VertexID1 && EdgeVertexID1 == VertexID0 ) )
			{
				return VertexConnectedEdgeID;
			}
		}

		return FEdgeID::Invalid;
	}

	/** Returns reference to an array of Edge IDs connected to this vertex */
	const TArray<FEdgeID>& GetVertexConnectedEdges( const FVertexID VertexID ) const
	{
		return VertexArray[ VertexID ].ConnectedEdgeIDs;
	}

	/** Returns reference to an array of VertexInstance IDs instanced from this vertex */
	const TArray<FVertexInstanceID>& GetVertexVertexInstances( const FVertexID VertexID ) const
	{
		return VertexArray[ VertexID ].VertexInstanceIDs;
	}

	/** Populates the passed array of PolygonIDs with the polygons connected to this vertex */
	void GetVertexConnectedPolygons( const FVertexID VertexID, TArray<FPolygonID>& OutConnectedPolygonIDs ) const
	{
		OutConnectedPolygonIDs.Reset();
		for( const FVertexInstanceID VertexInstanceID : VertexArray[ VertexID ].VertexInstanceIDs )
		{
			OutConnectedPolygonIDs.Append( VertexInstanceArray[ VertexInstanceID ].ConnectedPolygons );
		}
	}

	/** Populates the passed array of VertexIDs with the vertices adjacent to this vertex */
	void GetVertexAdjacentVertices( const FVertexID VertexID, TArray<FVertexID>& OutAdjacentVertexIDs ) const
	{
		const TArray<FEdgeID>& ConnectedEdgeIDs = VertexArray[ VertexID ].ConnectedEdgeIDs;
		OutAdjacentVertexIDs.SetNumUninitialized( ConnectedEdgeIDs.Num() );

		int32 Index = 0;
		for( const FEdgeID EdgeID : ConnectedEdgeIDs )
		{
			const FMeshEdge& Edge = EdgeArray[ EdgeID ];
			OutAdjacentVertexIDs[ Index ] = ( Edge.VertexIDs[ 0 ] == VertexID ) ? Edge.VertexIDs[ 1 ] : Edge.VertexIDs[ 0 ];
			Index++;
		}
	}

	/** Returns reference to an array of polygon IDs connected to this edge */
	const TArray<FPolygonID>& GetEdgeConnectedPolygons( const FEdgeID EdgeID ) const
	{
		return EdgeArray[ EdgeID ].ConnectedPolygons;
	}

	/** Returns the vertex ID corresponding to one of the edge endpoints */
	FVertexID GetEdgeVertex( const FEdgeID EdgeID, int32 VertexNumber ) const
	{
		check( VertexNumber == 0 || VertexNumber == 1 );
		return EdgeArray[ EdgeID ].VertexIDs[ VertexNumber ];
	}

	/** Returns a pair of vertex IDs defining the edge */
	TTuple<FVertexID, FVertexID> GetEdgeVertices( const FEdgeID EdgeID ) const
	{
		const FMeshEdge& Edge = EdgeArray[ EdgeID ];
		return MakeTuple( Edge.VertexIDs[ 0 ], Edge.VertexIDs[ 1 ] );
	}

	/** Returns reference to the array of triangles representing the triangulated polygon */
	TArray<FMeshTriangle>& GetPolygonTriangles( const FPolygonID PolygonID )
	{
		return PolygonArray[ PolygonID ].Triangles;
	}

	const TArray<FMeshTriangle>& GetPolygonTriangles( const FPolygonID PolygonID ) const
	{
		return PolygonArray[ PolygonID ].Triangles;
	}

	/** Returns reference to an array of VertexInstance IDs forming the perimeter of this polygon */
	const TArray<FVertexInstanceID>& GetPolygonPerimeterVertexInstances( const FPolygonID PolygonID ) const
	{
		return PolygonArray[ PolygonID ].PerimeterContour.VertexInstanceIDs;
	}

	/** Populates the passed array of VertexIDs with the vertices which form the polygon perimeter */
	void GetPolygonPerimeterVertices( const FPolygonID PolygonID, TArray<FVertexID>& OutPolygonPerimeterVertexIDs ) const;

	void GetPolygonEdges(const FPolygonID PolygonID, TArray<FEdgeID>& OutPolygonEdges) const
	{
		const FMeshPolygonContour& PerimeterContour = PolygonArray[PolygonID].PerimeterContour;
		const int32 ContourCount = PerimeterContour.VertexInstanceIDs.Num();
		for (int32 ContourIndex = 0; ContourIndex < ContourCount; ++ContourIndex)
		{
			int32 ContourPlusOne = (ContourIndex + 1) % ContourCount;
			OutPolygonEdges.Add(GetVertexPairEdge(GetVertexInstanceVertex(PerimeterContour.VertexInstanceIDs[ContourIndex]), GetVertexInstanceVertex(PerimeterContour.VertexInstanceIDs[ContourPlusOne])));
		}
	}

	/** Return the polygon group associated with a polygon */
	FPolygonGroupID GetPolygonPolygonGroup( const FPolygonID PolygonID ) const
	{
		return PolygonArray[ PolygonID ].PolygonGroupID;
	}

	/** Sets the polygon group associated with a polygon */
	void SetPolygonPolygonGroup( const FPolygonID PolygonID, const FPolygonGroupID PolygonGroupID )
	{
		FMeshPolygon& Polygon = PolygonArray[ PolygonID ];
		verify( PolygonGroupArray[ Polygon.PolygonGroupID ].Polygons.Remove( PolygonID ) == 1 );
		Polygon.PolygonGroupID = PolygonGroupID;
		check( !PolygonGroupArray[ PolygonGroupID ].Polygons.Contains( PolygonID ) );
		PolygonGroupArray[ PolygonGroupID ].Polygons.Add( PolygonID );
	}

	/** Return the vertex instance which corresponds to the given vertex on the given polygon, or FVertexInstanceID::Invalid */
	FVertexInstanceID GetVertexInstanceForPolygonVertex( const FPolygonID PolygonID, const FVertexID VertexID ) const
	{
		const FVertexInstanceID* VertexInstanceIDPtr = PolygonArray[ PolygonID ].PerimeterContour.VertexInstanceIDs.FindByPredicate(
			[ this, VertexID ]( const FVertexInstanceID VertexInstanceID )
			{
				return GetVertexInstanceVertex( VertexInstanceID ) == VertexID;
			});

		return VertexInstanceIDPtr ? *VertexInstanceIDPtr : FVertexInstanceID::Invalid;
	}

	/** Returns the vertex ID associated with the given vertex instance */
	FVertexID GetVertexInstanceVertex( const FVertexInstanceID VertexInstanceID ) const
	{
		return VertexInstanceArray[ VertexInstanceID ].VertexID;
	}

	/** Returns reference to an array of Polygon IDs connected to this vertex instance */
	const TArray<FPolygonID>& GetVertexInstanceConnectedPolygons( const FVertexInstanceID VertexInstanceID ) const
	{
		return VertexInstanceArray[ VertexInstanceID ].ConnectedPolygons;
	}

	/** Returns the polygons associated with the given polygon group */
	const TArray<FPolygonID>& GetPolygonGroupPolygons( const FPolygonGroupID PolygonGroupID ) const
	{
		return PolygonGroupArray[ PolygonGroupID ].Polygons;
	}

	/** Compacts the data held in the mesh description, and returns an object describing how the IDs have been remapped. */
	void Compact( FElementIDRemappings& OutRemappings );

	/** Remaps the element IDs in the mesh description according to the passed in object */
	void Remap( const FElementIDRemappings& Remappings );


	void ComputePolygonTriangulation(const FPolygonID PolygonID, TArray<FMeshTriangle>& OutTriangles);
	void TriangulateMesh();

	/** Set the polygon tangent and normal only for the specified polygonIDs */
	void ComputePolygonTangentsAndNormals(const TArray<FPolygonID>& PolygonIDs, float ComparisonThreshold = 0.0f);
	/** Set the polygon tangent and normal for all polygons in the mesh description. */
	void ComputePolygonTangentsAndNormals(float ComparisonThreshold = 0.0f);
	
	/** Set the vertex instance tangent and normal only for the specified VertexInstanceIDs */
	void ComputeTangentsAndNormals(const TArray<FVertexInstanceID>& VertexInstanceIDs, EComputeNTBsOptions ComputeNTBsOptions);
	/** Set the vertex instance tangent and normal for all vertex instances in the mesh description. */
	void ComputeTangentsAndNormals(EComputeNTBsOptions ComputeNTBsOptions);

	/** Determine the edge hardnesses from existing normals */
	void DetermineEdgeHardnessesFromVertexInstanceNormals( const float Tolerance = KINDA_SMALL_NUMBER );

	/** Determine UV seams from existing vertex instance UVs */
	void DetermineUVSeamsFromUVs( const int32 UVIndex, const float Tolerance = KINDA_SMALL_NUMBER );

	/** Get polygons in the same UV chart as the specified polygon */
	void GetPolygonsInSameChartAsPolygon( const FPolygonID PolygonID, TArray<FPolygonID>& OutPolygonIDs );

	/** Get array of all UV charts */
	void GetAllCharts( TArray<TArray<FPolygonID>>& OutCharts );

	void ReversePolygonFacing(const FPolygonID PolygonID);
	void ReverseAllPolygonFacing();

private:
	bool VectorsOnSameSide(const FVector& Vec, const FVector& A, const FVector& B, const float SameSideDotProductEpsilon);
	bool PointInTriangle(const FVector& A, const FVector& B, const FVector& C, const FVector& P, const float InsideTriangleDotProductEpsilon);
	FPlane ComputePolygonPlane(const FPolygonID PolygonID) const;
	FVector ComputePolygonNormal(const FPolygonID PolygonID) const;

	float GetPolygonCornerAngleForVertex(const FPolygonID PolygonID, const FVertexID VertexID) const;
	bool ComputePolygonTangentsAndNormals(
		  const FPolygonID PolygonID
		, float ComparisonThreshold
		, const TVertexAttributesRef<FVector> VertexPositions
		, const TVertexInstanceAttributesRef<FVector2D> VertexUVs
		, TPolygonAttributesRef<FVector> PolygonTangents
		, TPolygonAttributesRef<FVector> PolygonBinormals
		, TPolygonAttributesRef<FVector> PolygonCenters
		, TPolygonAttributesRef<FVector> PolygonNormals
	);

	void GetVertexConnectedPolygonsInSameSoftEdgedGroup(const FVertexID VertexID, const FPolygonID PolygonID, TArray<FPolygonID>& OutPolygonIDs) const;
	void GetPolygonsInSameSoftEdgedGroupAsPolygon(const FPolygonID PolygonID, const TArray<FPolygonID>& CandidatePolygonIDs, const TArray<FEdgeID>& SoftEdgeIDs, TArray<FPolygonID>& OutPolygonIDs) const;
	void GetConnectedSoftEdges(const FVertexID VertexID, TArray<FEdgeID>& OutConnectedSoftEdges) const;
	void ComputeTangentsAndNormals(
		  const FVertexInstanceID VertexInstanceID
		, EComputeNTBsOptions ComputeNTBsOptions
		, const TPolygonAttributesRef<FVector> PolygonNormals
		, const TPolygonAttributesRef<FVector> PolygonTangents
		, const TPolygonAttributesRef<FVector> PolygonBinormals
		, TVertexInstanceAttributesRef<FVector> VertexNormals
		, TVertexInstanceAttributesRef<FVector> VertexTangents
		, TVertexInstanceAttributesRef<float> VertexBinormalSigns
	);
private:

	/** Given a set of index remappings, fixes up references to element IDs */
	void FixUpElementIDs( const FElementIDRemappings& Remappings );

	/** Given a set of index remappings, remaps all attributes accordingly */
	void RemapAttributes( const FElementIDRemappings& Remappings );


	FVertexArray VertexArray;
	FVertexInstanceArray VertexInstanceArray;
	FEdgeArray EdgeArray;
	FPolygonArray PolygonArray;
	FPolygonGroupArray PolygonGroupArray;

	TAttributesSet<FVertexID> VertexAttributesSet;
	TAttributesSet<FVertexInstanceID> VertexInstanceAttributesSet;
	TAttributesSet<FEdgeID> EdgeAttributesSet;
	TAttributesSet<FPolygonID> PolygonAttributesSet;
	TAttributesSet<FPolygonGroupID> PolygonGroupAttributesSet;
};


/**
 * Bulk data storage for FMeshDescription
 */
struct MESHDESCRIPTION_API FMeshDescriptionBulkData
{
public:
	FMeshDescriptionBulkData()
		: bBulkDataUpdated( false )
	{
		BulkData.SetBulkDataFlags( BULKDATA_SerializeCompressed | BULKDATA_SerializeCompressedBitWindow );
	}

#if WITH_EDITORONLY_DATA
	/** Serialization */
	void Serialize( FArchive& Ar, UObject* Owner );

	/** Store a new mesh description in the bulk data */
	void SaveMeshDescription( FMeshDescription& MeshDescription );

	/** Load the mesh description from the bulk data */
	void LoadMeshDescription( FMeshDescription& MeshDescription );

	/** Empties the bulk data */
	void Empty();

	/** Returns true if there is no bulk data available */
	bool IsEmpty() const { return BulkData.GetBulkDataSize() == 0; }

	/** Return unique ID string for this bulk data */
	FString GetIdString() const;
#endif

private:
	/** Internally store bulk data as bytes */
	FByteBulkData BulkData;

	/** GUID associated with the data stored herein. */
	FGuid Guid;

	/** Take a copy of the bulk data versioning so it can be propagated to the bulk data reader when deserializing MeshDescription */
	FCustomVersionContainer CustomVersions;

	/** Whether the bulk data has been written via SaveMeshDescription */
	bool bBulkDataUpdated;
};


UCLASS(deprecated)
class MESHDESCRIPTION_API UDEPRECATED_MeshDescription : public UObject
{
public:
	GENERATED_BODY()

	// UObject interface
	virtual void Serialize( FArchive& Ar ) override;
};
