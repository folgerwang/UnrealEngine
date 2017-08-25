// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshTypes.h"
#include "MeshElementArray.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.generated.h"


USTRUCT()
struct FMeshVertex
{
	GENERATED_BODY()

	FMeshVertex()
		: VertexPosition( FVector::ZeroVector ),
		  CornerSharpness( 0.0f )
	{}

	/** Position of the vertex. */
	UPROPERTY()
	FVector VertexPosition;

	/** All of vertex instances which reference this vertex (for split vertex support) */
	UPROPERTY()
	TArray<FVertexInstanceID> VertexInstanceIDs;

	/** The edges connected to this vertex */
	UPROPERTY()
	TArray<FEdgeID> ConnectedEdgeIDs;

	/** When subdivisions are enabled, this controls how sharp the vertex is, between 0.0 and 1.0. */
	UPROPERTY()
	float CornerSharpness;	// @todo mesheditor subdiv: Not really used by static meshes at all.  Only for UEditableMeshes that use subdivision features.  Move elsewhere.?

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FMeshVertex& Vertex )
	{
		Ar << Vertex.VertexPosition;
		Ar << Vertex.VertexInstanceIDs;
		Ar << Vertex.ConnectedEdgeIDs;
		Ar << Vertex.CornerSharpness;
		return Ar;
	}
};


USTRUCT()
struct FMeshVertexInstance
{
	GENERATED_BODY()

	FMeshVertexInstance()
		: VertexID( FVertexID::Invalid ),
		  Normal( FVector::ZeroVector ),
		  Tangent( FVector::ZeroVector ),
		  BinormalSign( 0.0f ),
		  Color( FLinearColor::White )
	{}

	/** The vertex this is instancing */
	UPROPERTY()
	FVertexID VertexID;

	/** List of connected polygons */
	UPROPERTY()
	TArray<FPolygonID> ConnectedPolygons;

	/** UVs for the vertex instance */
	UPROPERTY()
	TArray<FVector2D> VertexUVs;

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
	friend FArchive& operator<<( FArchive& Ar, FMeshVertexInstance& VertexInstance )
	{
		Ar << VertexInstance.VertexID;
		Ar << VertexInstance.ConnectedPolygons;
		Ar << VertexInstance.VertexUVs;
		Ar << VertexInstance.Normal;
		Ar << VertexInstance.Tangent;
		Ar << VertexInstance.BinormalSign;
		Ar << VertexInstance.Color;
		return Ar;
	}
};


USTRUCT()
struct FMeshEdge
{
	GENERATED_BODY()

	FMeshEdge()
		: VertexIDs{ FVertexID::Invalid, FVertexID::Invalid },
		  bIsHardEdge( false ),
		  CreaseSharpness( 0.0f )
	{}

	/** IDs of the two editable mesh vertices that make up this edge.  The winding direction is not defined. */
	UPROPERTY()
	FVertexID VertexIDs[ 2 ];

	/** The polygons that share this edge.  It's best if there are always only two polygons that share
	    the edge, and those polygons are facing the same direction */
	UPROPERTY()
	TArray<FPolygonID> ConnectedPolygons;

	/** Whether this edge is 'hard' or not, for the purpose of vertex normal and tangent generation */
	UPROPERTY()
	bool bIsHardEdge;

	/** When subdivisions are enabled, this controls how sharp the creasing of this edge will be, between 0.0 and 1.0. */
	UPROPERTY()
	float CreaseSharpness;

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FMeshEdge& Edge )
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
struct FMeshPolygonContour
{
	GENERATED_BODY()

	/** The ordered list of vertex instances which make up the polygon contour. The winding direction is counter-clockwise. */
	UPROPERTY()
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
	friend FArchive& operator<<( FArchive& Ar, FMeshTriangle& Tri )
	{
		Ar << Tri.VertexInstanceID0;
		Ar << Tri.VertexInstanceID1;
		Ar << Tri.VertexInstanceID2;
		return Ar;
	}
};


USTRUCT()
struct FMeshPolygon
{
	GENERATED_BODY()

	FMeshPolygon()
		: PolygonGroupID( FPolygonGroupID::Invalid ),
		  PolygonNormal( FVector::ZeroVector ),
		  PolygonTangent( FVector::ZeroVector ),
		  PolygonBinormal( FVector::ZeroVector ),
		  PolygonCenter( FVector::ZeroVector )
	{}

	/** The outer boundary edges of this polygon */
	UPROPERTY()
	FMeshPolygonContour PerimeterContour;

	/** Optional inner contours of this polygon that define holes inside of the polygon.  For the geometry to
	    be considered valid, the hole contours should reside within the boundary of the polygon perimeter contour, 
		and must not overlap each other.  No "nesting" of polygons inside the holes is supported -- those are 
		simply separate polygons */
	UPROPERTY()
	TArray<FMeshPolygonContour> HoleContours;

	/** List of triangles which make up this polygon */
	UPROPERTY()
	TArray<FMeshTriangle> Triangles;

	/** The polygon group which contains this polygon */
	UPROPERTY()
	FPolygonGroupID PolygonGroupID;

	/** Cached normal */
	UPROPERTY()
	FVector PolygonNormal;

	/** Cached tangent */
	UPROPERTY()
	FVector PolygonTangent;

	/** Cached binormal */
	UPROPERTY()
	FVector PolygonBinormal;

	/** Cached center */
	UPROPERTY()
	FVector PolygonCenter;

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FMeshPolygon& Polygon )
	{
		Ar << Polygon.PerimeterContour;
		Ar << Polygon.HoleContours;
		Ar << Polygon.Triangles;
		Ar << Polygon.PolygonGroupID;
		return Ar;
	}
};


USTRUCT()
struct FMeshPolygonGroup
{
	GENERATED_BODY()

	FMeshPolygonGroup()
		: bEnableCollision( true ),
		  bCastShadow( true )
	{}

	/** The material for this mesh section */
	UPROPERTY()
	FStringAssetReference MaterialAsset;

	/** A unique name identifying this polygon group */
	UPROPERTY()
	FName MaterialSlotName;

	UPROPERTY()
	FName ImportedMaterialSlotName;

	/** If true, collision is enabled for this section. */
	UPROPERTY()
	bool bEnableCollision;

	/** If true, this section will cast a shadow */
	UPROPERTY()
	bool bCastShadow;

	/** All polygons in this group */
	TArray<FPolygonID> Polygons;

	/** Serializer */
	friend FArchive& operator<<( FArchive& Ar, FMeshPolygonGroup& PolygonGroup )
	{
		Ar << PolygonGroup.MaterialAsset;
		Ar << PolygonGroup.MaterialSlotName;
		Ar << PolygonGroup.ImportedMaterialSlotName;
		Ar << PolygonGroup.bEnableCollision;
		Ar << PolygonGroup.bCastShadow;
		Ar << PolygonGroup.Polygons;
		return Ar;
	}
};


/** Define container types */
using FVertexArray = TMeshElementArray<FMeshVertex, FVertexID>;
using FVertexInstanceArray = TMeshElementArray<FMeshVertexInstance, FVertexInstanceID>;
using FEdgeArray = TMeshElementArray<FMeshEdge, FEdgeID>;
using FPolygonArray = TMeshElementArray<FMeshPolygon, FPolygonID>;
using FPolygonGroupArray = TMeshElementArray<FMeshPolygonGroup, FPolygonGroupID>;

/**
 * List of attribute types which are supported.
 * We do this so we can automatically generate the attribute containers and their associated accessors with
 * some template magic. Adding a new attribute type requires no extra code elsewhere in the class.
 */
using AttributeTypes = TTuple
<
	FVector4,
	FVector,
	FVector2D,
	float,
	int,
	bool,
	FName,
	FStringAssetReference
>;

/** Define aliases for element attributes */
template <typename AttributeType> using TVertexAttributeArray = TMeshElementArray<AttributeType, FVertexID>;
template <typename AttributeType> using TVertexInstanceAttributeArray = TMeshElementArray<AttributeType, FVertexInstanceID>;
template <typename AttributeType> using TEdgeAttributeArray = TMeshElementArray<AttributeType, FEdgeID>;
template <typename AttributeType> using TPolygonAttributeArray = TMeshElementArray<AttributeType, FPolygonID>;
template <typename AttributeType> using TPolygonGroupAttributeArray = TMeshElementArray<AttributeType, FPolygonGroupID>;


/**
  * Define type for an attribute container of a single type.
  * Attributes have a name and an index, and are keyed on an ElementIDType.
  *
  * This implies the below data structure:
  * A TMap keyed on the attribute name,
  * yielding a TArray indexed by attribute index,
  * yielding a TMeshElementArray keyed on an Element ID,
  * yielding an item of type AttributeType.
  *
  * This looks complicated, but actually makes attribute lookup easy when we are interested in a particular attribute for many element IDs.
  * By caching the TMeshElementArray arrived at by the attribute name and index, we have O(1) access to that attribute for all elements.
  */
template <typename AttributeType, typename ElementIDType>
using TAttributesArray = TMap<FName, TArray<TMeshElementArray<AttributeType, ElementIDType>>>;


/**
 * Helper template which transforms a tuple of types into a tuple of TAttributesArrays of those types.
 *
 * We need to instance TAttributeArrays for each type in the AttributeTypes tuple.
 * Then we can access the appropriate array (as long as we know what its index is).
 *
 * This template, given ElementIDType and TTuple<A, B>, will generate:
 * TTuple<TAttributesArray<A, ElementIDType>, TAttributesArray<B, ElementIDType>>
 */
template <typename ElementIDType, typename Tuple>
struct TMakeAttributesSet;

template <typename ElementIDType, typename... TupleTypes>
struct TMakeAttributesSet<ElementIDType, TTuple<TupleTypes...>>
{
	using Type = TTuple<TAttributesArray<TupleTypes, ElementIDType>...>;
};


/**
 * Helper template which gets the tuple index of a given type from a given TTuple.
 *
 * Given Type = char, and Tuple = TTuple<int, float, char>,
 * TTupleIndex<Type, Tuple>::Value will be 2.
 */
template <typename Type, typename Tuple>
struct TTupleIndex;

template <typename Type, typename... Types>
struct TTupleIndex<Type, TTuple<Type, Types...>>
{
	static const uint32 Value = 0U;
};

template <typename Type, typename Head, typename... Tail>
struct TTupleIndex<Type, TTuple<Head, Tail...>>
{
	static const uint32 Value = 1U + TTupleIndex<Type, TTuple<Tail...>>::Value;
};


/**
 * This is the actual type of the attribute container.
 * It is a TTuple of TAttributesArray<attributeType>.
 */
template <typename ElementIDType>
struct TAttributesSet : public TMakeAttributesSet<ElementIDType, AttributeTypes>::Type
{
	using Super = typename TMakeAttributesSet<ElementIDType, AttributeTypes>::Type;

	/**
	 * Register a new attribute name with the given type (must be a member of the AttributeTypes tuple).
	 *
	 * Example of use:
	 *
	 *		VertexInstanceAttributes().RegisterAttribute<FVector2D>( "UV", 8 );
	 *                        . . .
	 *		TVertexInstanceAttributeArray<FVector2D>& UV0 = VertexInstanceAttributes().GetAttributes<FVector2D>( "UV", 0 );
	 *		UV0[ VertexInstanceID ] = FVector2D( 1.0f, 1.0f );
	 */
	template <typename AttributeType>
	void RegisterAttribute( const FName AttributeName, const int32 NumberOfIndices = 1 )
	{
		this->Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().Emplace( AttributeName ).SetNum( NumberOfIndices );
	}

	/** Unregister an attribute name with the given type */
	template <typename AttributeType>
	void UnregisterAttribute( const FName AttributeName )
	{
		this->Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().Remove( AttributeName );
	}

	/** Determines whether an attribute of the given type exists with the given name */
	template <typename AttributeType>
	bool HasAttribute( const FName AttributeName ) const
	{
		return this->Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().Contains( AttributeName );
	}

	/**
	 * Get a set of attributes with the given type, name and index.
	 *
	 * Example of use:
	 *
	 *		const TVertexAttributeArray<FVector>& VertexPositions = VertexAttributes().GetAttributes<FVector>( "Position" );
	 *		for( const FVertexID VertexID : GetVertices().GetElementIDs() )
	 *		{
	 *			const FVector Position = VertexPositions[ VertexID ];
	 *			DoSomethingWith( Position );
	 *		}
	 */
	template <typename AttributeType>
	TMeshElementArray<AttributeType, ElementIDType>& GetAttributes( const FName AttributeName, const int32 AttributeIndex = 0 )
	{
		// @todo mesh description: should this handle non-existent attribute names and indices gracefully?
		return this->Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().FindChecked( AttributeName )[ AttributeIndex ];
	}

	template <typename AttributeType>
	const TMeshElementArray<AttributeType, ElementIDType>& GetAttributes( const FName AttributeName, const int32 AttributeIndex = 0 ) const
	{
		// @todo mesh description: should this handle non-existent attribute names and indices gracefully?
		return this->Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().FindChecked( AttributeName )[ AttributeIndex ];
	}

	/** Returns the number of indices for the attribute with the given name */
	template <typename AttributeType>
	int32 GetAttributeIndexCount( const FName AttributeName ) const
	{
		// @todo mesh description: should this handle non-existent attribute names and indices gracefully?
		return this->Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().FindChecked( AttributeName ).Num();
	}

	/** Returns an array of all the attribute names registered for this attribute type */
	template <typename AttributeType, typename Allocator>
	void GetAttributeNames( TArray<FName, Allocator>& OutAttributeNames ) const
	{
		this->Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().GetKeys( OutAttributeNames );
	}

	template <typename AttributeType>
	AttributeType GetAttribute( const ElementIDType ElementID, const FName AttributeName, const int32 AttributeIndex = 0 )
	{
		return this->Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().FindChecked( AttributeName )[ AttributeIndex ][ ElementID ];
	}

	template <typename AttributeType>
	AttributeType SetAttribute( const ElementIDType ElementID, const FName AttributeName, const int32 AttributeIndex, const AttributeType& AttributeValue )
	{
		this->Get<TTupleIndex<AttributeType, AttributeTypes>::Value>().FindChecked( AttributeName )[ AttributeIndex ][ ElementID ] = AttributeValue;
	}

	friend FArchive& operator<<( FArchive& Ar, TAttributesSet& AttributesSet )
	{
		Ar << static_cast<Super&>( AttributesSet );
		return Ar;
	}
};


/**
 * This is a structure which holds the ID remappings returned by a Compact operation, or passed to a Remap operation.
 */
struct FElementIDRemappings
{
	TSparseArray<FVertexID> NewVertexIndexLookup;
	TSparseArray<FVertexInstanceID> NewVertexInstanceIndexLookup;
	TSparseArray<FEdgeID> NewEdgeIndexLookup;
	TSparseArray<FPolygonID> NewPolygonIndexLookup;
	TSparseArray<FPolygonGroupID> NewPolygonGroupIndexLookup;

	FVertexID GetRemappedVertexID( FVertexID VertexID ) const
	{
		checkSlow( NewVertexIndexLookup.IsAllocated( VertexID.GetValue() ) );
		return NewVertexIndexLookup[ VertexID.GetValue() ];
	}

	FVertexInstanceID GetRemappedVertexInstanceID( FVertexInstanceID VertexInstanceID ) const
	{
		checkSlow( NewVertexInstanceIndexLookup.IsAllocated( VertexInstanceID.GetValue() ) );
		return NewVertexInstanceIndexLookup[ VertexInstanceID.GetValue() ];
	}

	FEdgeID GetRemappedEdgeID( FEdgeID EdgeID ) const
	{
		checkSlow( NewEdgeIndexLookup.IsAllocated( EdgeID.GetValue() ) );
		return NewEdgeIndexLookup[ EdgeID.GetValue() ];
	}

	FPolygonID GetRemappedPolygonID( FPolygonID PolygonID ) const
	{
		checkSlow( NewPolygonIndexLookup.IsAllocated( PolygonID.GetValue() ) );
		return NewPolygonIndexLookup[ PolygonID.GetValue() ];
	}

	FPolygonGroupID GetRemappedPolygonGroupID( FPolygonGroupID PolygonGroupID ) const
	{
		check( NewPolygonGroupIndexLookup.IsAllocated( PolygonGroupID.GetValue() ) );
		return NewPolygonGroupIndexLookup[ PolygonGroupID.GetValue() ];
	}
};


UCLASS()
class MESHDESCRIPTION_API UMeshDescription : public UObject
{
public:

	GENERATED_BODY()

	UMeshDescription();

	// UObject interface
	virtual void Serialize( FArchive& Ar ) override;
	virtual void PostLoad() override;

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

	/** Adds a new vertex to the mesh and returns its ID */
	FVertexID CreateVertex()
	{
		return VertexArray.Add();
	}

	/** Adds a new vertex to the mesh with the given ID */
	void CreateVertexWithID( const FVertexID VertexID )
	{
		VertexArray.Insert( VertexID );
	}

	/** Deletes a vertex from the mesh */
	void DeleteVertex( const FVertexID VertexID )
	{
		check( VertexArray[ VertexID ].ConnectedEdgeIDs.Num() == 0 );
		check( VertexArray[ VertexID ].VertexInstanceIDs.Num() == 0 );
		VertexArray.Remove( VertexID );
	}

private:
	inline void CreateVertexInstance_Internal( const FVertexInstanceID VertexInstanceID, const FVertexID VertexID )
	{
		VertexInstanceArray[ VertexInstanceID ].VertexID = VertexID;
		check( !VertexArray[ VertexID ].VertexInstanceIDs.Contains( VertexInstanceID ) );
		VertexArray[ VertexID ].VertexInstanceIDs.Add( VertexInstanceID );
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
		const FVertexID VertexID = VertexInstanceArray[ VertexInstanceID ].VertexID;
		verify( VertexArray[ VertexID ].VertexInstanceIDs.Remove( VertexInstanceID ) == 1 );
		if( InOutOrphanedVerticesPtr && VertexArray[ VertexID ].VertexInstanceIDs.Num() == 0 && VertexArray[ VertexID ].ConnectedEdgeIDs.Num() == 0 )
		{
			InOutOrphanedVerticesPtr->Add( VertexID );
		}
		VertexInstanceArray.Remove( VertexInstanceID );
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
				InOutOrphanedVerticesPtr->Add( EdgeVertexID );
			}
		}
		EdgeArray.Remove( EdgeID );
	}

	/** Pair of IDs representing the vertex instance ID on a contour, and the edge ID which starts at that point, winding counter-clockwise */
	struct FContourPoint
	{
		FVertexInstanceID VertexInstanceID;
		FEdgeID EdgeID;
	};

private:
	void CreatePolygonContour_Internal( const FPolygonID PolygonID, const TArray<FContourPoint>& ContourPoints, FMeshPolygonContour& Contour )
	{
		Contour.VertexInstanceIDs.Reset( ContourPoints.Num() );
		for( const FContourPoint ContourPoint : ContourPoints )
		{
			const FVertexInstanceID VertexInstanceID = ContourPoint.VertexInstanceID;
			const FEdgeID EdgeID = ContourPoint.EdgeID;

			Contour.VertexInstanceIDs.Add( VertexInstanceID );
			check( !VertexInstanceArray[ VertexInstanceID ].ConnectedPolygons.Contains( PolygonID ) );
			VertexInstanceArray[ VertexInstanceID ].ConnectedPolygons.Add( PolygonID );

			check( !EdgeArray[ EdgeID ].ConnectedPolygons.Contains( PolygonID ) );
			EdgeArray[ EdgeID ].ConnectedPolygons.Add( PolygonID );
		}
	}

	void CreatePolygon_Internal( const FPolygonID PolygonID, const TArray<FContourPoint>& Perimeter, const TArray<TArray<FContourPoint>>& Holes )
	{
		FMeshPolygon& Polygon = PolygonArray[ PolygonID ];
		CreatePolygonContour_Internal( PolygonID, Perimeter, Polygon.PerimeterContour );
		for( const TArray<FContourPoint>& Hole : Holes )
		{
			Polygon.HoleContours.Emplace();
			CreatePolygonContour_Internal( PolygonID, Hole, Polygon.HoleContours.Last() );
		}
	}

public:
	/** Adds a new polygon to the mesh and returns its ID */
	FPolygonID CreatePolygon( const TArray<FContourPoint>& Perimeter, const TArray<TArray<FContourPoint>>& Holes = TArray<TArray<FContourPoint>>() )
	{
		const FPolygonID PolygonID = PolygonArray.Add();
		CreatePolygon_Internal( PolygonID, Perimeter, Holes );
		return PolygonID;
	}

	/** Adds a new polygon to the mesh with the given ID */
	void CreatePolygonWithID( const FPolygonID PolygonID, const TArray<FContourPoint>& Perimeter, const TArray<TArray<FContourPoint>>& Holes = TArray<TArray<FContourPoint>>() )
	{
		PolygonArray.Insert( PolygonID );
		CreatePolygon_Internal( PolygonID, Perimeter, Holes );
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
				InOutOrphanedVertexInstancesPtr->Add( VertexInstanceID );
			}

			const FEdgeID EdgeID = GetVertexPairEdge( VertexInstanceArray[ LastVertexInstanceID ].VertexID, VertexInstanceArray[ VertexInstanceID ].VertexID );
			check( EdgeID != FEdgeID::Invalid );
			
			FMeshEdge& Edge = EdgeArray[ EdgeID ];
			verify( Edge.ConnectedPolygons.Remove( PolygonID ) == 1 );

			if( InOutOrphanedEdgesPtr && Edge.ConnectedPolygons.Num() == 0 )
			{
				InOutOrphanedEdgesPtr->Add( EdgeID );
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

		for( FMeshPolygonContour& HoleContour : Polygon.HoleContours )
		{
			DeletePolygonContour_Internal( PolygonID, HoleContour.VertexInstanceIDs, InOutOrphanedEdgesPtr, InOutOrphanedVertexInstancesPtr );
		}

		FMeshPolygonGroup& PolygonGroup = PolygonGroupArray[ Polygon.PolygonGroupID ];
		verify( PolygonGroup.Polygons.Remove( PolygonID ) == 1 );

		if( InOutOrphanedPolygonGroupsPtr && PolygonGroup.Polygons.Num() == 0 )
		{
			InOutOrphanedPolygonGroupsPtr->Add( Polygon.PolygonGroupID );
		}

		PolygonArray.Remove( PolygonID );
	}

	/** Adds a new polygon group to the mesh and returns its ID */
	FPolygonGroupID CreatePolygonGroup()
	{
		return PolygonGroupArray.Add();
	}

	/** Adds a new polygon group to the mesh with the given ID */
	void CreatePolygonGroupWithID( const FPolygonGroupID PolygonGroupID )
	{
		PolygonGroupArray.Insert( PolygonGroupID );
	}

	/** Deletes a polygon group from the mesh */
	void DeletePolygonGroup( const FPolygonGroupID PolygonGroupID )
	{
		check( PolygonGroupArray[ PolygonGroupID ].Polygons.Num() == 0 );
		PolygonGroupArray.Remove( PolygonGroupID );
	}


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


	/** Compacts the data held in the mesh description, and returns an object describing how the IDs have been remapped. */
	void Compact( FElementIDRemappings& OutRemappings );

	/** Remaps the element IDs in the mesh description according to the passed in object */
	void Remap( const FElementIDRemappings& Remappings );


private:

	/** Given a set of index remappings, fixes up references to element IDs */
	void FixUpElementIDs( const FElementIDRemappings& Remappings );

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
