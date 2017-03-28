// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "EditableMeshTypes.generated.h"

// @todo mesheditor: Move elsewhere
namespace LogHelpers
{
	template<typename ArrayType>
	inline FString ArrayToString( const TArray<ArrayType>& Array )
	{
		FString String;
		if( Array.Num() == 0 )
		{
			String = TEXT( "Empty" );
		}
		else
		{
			for( const ArrayType& Element : Array )
			{
				if( !String.IsEmpty() )
				{
					String.Append( TEXT( ", " ) );
				}
				String.Append( Element.ToString() );
			}
			String = TEXT( "[" ) + String + TEXT( "]" );
		}
		return String;
	}

	template<>
	inline FString ArrayToString<int32>( const TArray<int32>& Array )
	{
		FString String;
		if( Array.Num() == 0 )
		{
			String = TEXT( "Empty" );
		}
		else
		{
			for( const int32& Element : Array )
			{
				if( !String.IsEmpty() )
				{
					String.Append( TEXT( ", " ) );
				}
				String.Append( FString::FormatAsNumber( Element ) );
			}
			String = TEXT( "[" ) + String + TEXT( "]" );
		}
		return String;
	}


	template<typename ArrayType>
	inline FString ArrayToString( const TArray<TArray<ArrayType>>& Array )
	{
		FString String;
		if( Array.Num() == 0 )
		{
			String = TEXT( "Empty" );
		}
		else
		{
			for( const TArray<ArrayType>& SubArray : Array )
			{
				if( !String.IsEmpty() )
				{
					String.Append( TEXT( ", " ) );
				}
				String.Append( FString::Printf( TEXT( "%s" ), *ArrayToString( SubArray ) ) );
			}
			String = TEXT( "[" ) + String + TEXT( "]" );
		}
		return String;
	}

	inline FString BoolToString( const bool Bool )
	{
		return Bool ? TEXT( "true" ) : TEXT( "false" );
	}
}


/**
 * The different components that make up a typical mesh
 */
enum class EEditableMeshElementType
{
	/** Invalid mesh element (or "none") */
	Invalid,

	/** A unique point in 3D space */
	Vertex,

	/** An edge that connects two vertices */
	Edge,

	/** A polygon with at least three 3D points.  It could be triangle, quad, or more complex shape */
	Polygon,

	/** Represents any element type */
	Any,
};


// @todo mesheditor: Need comments

USTRUCT( BlueprintType )
struct FElementID	// @todo mesheditor script: BP doesn't have name spaces, so we might need a more specific display name, or just rename our various types
{
	GENERATED_BODY()

	FElementID()
	{
	}

	explicit FElementID( const int32 InitIDValue )
		: IDValue( InitIDValue )
	{
	}

	FORCEINLINE int32 GetValue() const
	{
		return IDValue;
	}

	FORCEINLINE bool operator==( const FElementID& Other ) const
	{
		return IDValue == Other.IDValue;
	}

	FORCEINLINE bool operator!=( const FElementID& Other ) const
	{
		return IDValue != Other.IDValue;
	}

	FString ToString() const
	{
		return FString::Printf( TEXT( "%lu" ), GetValue() );
	}

	friend FArchive& operator<<( FArchive& Ar, FElementID& Element )
	{
		Ar << Element.IDValue;
		return Ar;
	}

	/** Invalid element ID */
	MESHEDITINGRUNTIME_API static const FElementID Invalid;

protected:

	/** The actual mesh element index this ID represents.  Read-only. */
	UPROPERTY( BlueprintReadOnly, Category="Editable Mesh" )
	int32 IDValue;
};


USTRUCT( BlueprintType )
struct FVertexID : public FElementID
{
	GENERATED_BODY()

	FVertexID()
	{
	}

	explicit FVertexID( const FElementID InitElementID )
		: FElementID( InitElementID.GetValue() )
	{
	}

	explicit FVertexID( const int32 InitIDValue )
		: FElementID( InitIDValue )
	{
	}

	FORCEINLINE friend uint32 GetTypeHash( const FVertexID& Other )
	{
		return GetTypeHash( Other.IDValue );
	}

	/** Invalid vertex ID */
	MESHEDITINGRUNTIME_API static const FVertexID Invalid;
};


USTRUCT( BlueprintType )
struct FEdgeID : public FElementID
{
	GENERATED_BODY()

	FEdgeID()
	{
	}

	explicit FEdgeID( const FElementID InitElementID )
		: FElementID( InitElementID.GetValue() )
	{
	}

	explicit FEdgeID( const int32 InitIDValue )
		: FElementID( InitIDValue )
	{
	}

	FORCEINLINE friend uint32 GetTypeHash( const FEdgeID& Other )
	{
		return GetTypeHash( Other.IDValue );
	}

	/** Invalid edge ID */
	MESHEDITINGRUNTIME_API static const FEdgeID Invalid;
};


USTRUCT( BlueprintType )
struct FSectionID : public FElementID
{
	GENERATED_BODY()

	FSectionID()
	{
	}

	explicit FSectionID( const FElementID InitElementID )
		: FElementID( InitElementID.GetValue() )
	{
	}

	explicit FSectionID( const int32 InitIDValue )
		: FElementID( InitIDValue )
	{
	}

	FORCEINLINE friend uint32 GetTypeHash( const FSectionID& Other )
	{
		return GetTypeHash( Other.IDValue );
	}

	/** Invalid section ID */
	MESHEDITINGRUNTIME_API static const FSectionID Invalid;
};


USTRUCT( BlueprintType )
struct FPolygonID : public FElementID
{
	GENERATED_BODY()

	FPolygonID()
	{
	}

	explicit FPolygonID( const FElementID InitElementID )
		: FElementID( InitElementID.GetValue() )
	{
	}

	explicit FPolygonID( const int32 InitIDValue )
		: FElementID( InitIDValue )
	{
	}

	FORCEINLINE friend uint32 GetTypeHash( const FPolygonID& Other )
	{
		return GetTypeHash( Other.IDValue );
	}

	/** Invalid polygon ID */
	MESHEDITINGRUNTIME_API static const FPolygonID Invalid;	// @todo mesheditor script: Can we expose these to BP nicely?	Do we even need to?
};


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
	MESHEDITINGRUNTIME_API static const FTriangleID Invalid;
};


/**
 * Unique identifies a specific sub-mesh within a component
 */
struct FEditableMeshSubMeshAddress
{
	/** Pointer that uniquely identifies the mesh object being edited (not the instance), for hashing/comparison purposes */
	void* MeshObjectPtr;

	/** The type of mesh format */
	// @todo mesheditor: Should SHOULD be OK, but if it somehow was deallocated while we were using it, we'd want to store an FName or ID here instead of a pointer.  Should be fine though.
	class IEditableMeshFormat* EditableMeshFormat;

	/** The index of the mesh within the component, for components that may define more than one mesh */
	int32 MeshIndex;

	/** The mesh level of detail index, or zero if not applicable to the type of mesh. */
	int32 LODIndex;


	/** Default constructor that initializes variables to an default sub-mesh address */
	FEditableMeshSubMeshAddress()
		: MeshObjectPtr( nullptr ),
		  EditableMeshFormat( nullptr ),
		  MeshIndex( 0 ),
		  LODIndex( 0 )
	{
	}

	/** Equality check */
	inline bool operator==( const FEditableMeshSubMeshAddress& Other ) const
	{
		return
			MeshObjectPtr == Other.MeshObjectPtr &&
			EditableMeshFormat == Other.EditableMeshFormat &&
			MeshIndex == Other.MeshIndex &&
			LODIndex == Other.LODIndex;
	}

	/** Hashing */
	FORCEINLINE friend uint32 GetTypeHash( const FEditableMeshSubMeshAddress& Other )
	{
		// @todo mesheditor: Hash could be improved a bit to consider LOD/MeshIndex, etc.
		return GetTypeHash( Other.MeshObjectPtr );
	}

	/** Convert to a string */
	inline FString ToString() const
	{
		return FString::Printf(
			TEXT( "PtrHash:%lu, FmtHash:%lu, MeshIndex:%lu, LODIndex:%lu" ),
			GetTypeHash( MeshObjectPtr ),
			GetTypeHash( EditableMeshFormat ),
			MeshIndex,
			LODIndex );
	}
};



USTRUCT( BlueprintType )
struct FPolygonRef
{
	GENERATED_BODY()

	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FSectionID SectionID;

	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FPolygonID PolygonID;

	FPolygonRef()
	{
	}

	FPolygonRef( const FSectionID InitSectionID, const FPolygonID InitPolygonID )
		: SectionID( InitSectionID ),
		  PolygonID( InitPolygonID )
	{
	}

	inline bool operator==( const FPolygonRef& Other ) const
	{
		return SectionID == Other.SectionID && PolygonID == Other.PolygonID;
	}

	inline bool operator!=( const FPolygonRef& Other ) const
	{
		return SectionID != Other.SectionID || PolygonID != Other.PolygonID;
	}

	/** Hashing */
	FORCEINLINE friend uint32 GetTypeHash( const FPolygonRef& Other )
	{
		return GetTypeHash( ( uint64( Other.PolygonID.GetValue() ) << 32 ) | uint64( Other.SectionID.GetValue() ) );
	}

	FString ToString() const
	{
		return FString::Printf( TEXT( "SectionID:%s, PolygonID:%s" ), *SectionID.ToString(), *PolygonID.ToString() );
	}

	friend FArchive& operator<<( FArchive& Ar, FPolygonRef& PolygonRef )
	{
		Ar << PolygonRef.SectionID;
		Ar << PolygonRef.PolygonID;
		return Ar;
	}

	/** Invalid polygon ref */
	MESHEDITINGRUNTIME_API static const FPolygonRef Invalid;
};


UCLASS( abstract )
class MESHEDITINGRUNTIME_API UEditableMeshAttribute : public UObject
{
	GENERATED_BODY()

public:

	//
	// Vertex data for any vertex
	//

	/** Static: The attribute name for vertex position */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FName VertexPosition()
	{
		return VertexPositionName;
	}

	/** Static: The attribute name for vertex corner sharpness (only applies to subdivision meshes) */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FName VertexCornerSharpness()
	{
		return VertexCornerSharpnessName;
	}

	//
	// Polygon-specific vertex data (can also be set on the vertex itself to update all polygon vertices.)
	//

	/** Static: The attribute name for vertex normal (tangent Z) */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FName VertexNormal()
	{
		return VertexNormalName;
	}

	/** Static: The attribute name for vertex tangent vector (tangent X) */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FName VertexTangent()
	{
		return VertexTangentName;
	}

	/** Static: The attribute name for the vertex basis determinant sign (used to calculate the direction of tangent Y) */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FName VertexBinormalSign()
	{
		return VertexBinormalSignName;
	}

	/** Static: The attribute name for vertex texture coordinate.  The attribute index defines which texture coordinate set. */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FName VertexTextureCoordinate()
	{
		return VertexTextureCoordinateName;
	}

	/** Static: The attribute name for the vertex color. */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FName VertexColor()
	{
		return VertexColorName;
	}

	//
	// Edges
	//

	/** Static: The attribute name for edge hardedness */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FName EdgeIsHard()
	{
		return EdgeIsHardName;
	}

	/** Static: The attribute name for edge crease sharpness (only applies to subdivision meshes) */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FName EdgeCreaseSharpness()
	{
		return EdgeCreaseSharpnessName;
	}

private:

	static const FName VertexPositionName;
	static const FName VertexCornerSharpnessName;
	static const FName VertexNormalName;
	static const FName VertexTangentName;
	static const FName VertexBinormalSignName;
	static const FName VertexTextureCoordinateName;
	static const FName VertexColorName;
	static const FName EdgeIsHardName;
	static const FName EdgeCreaseSharpnessName;
};


UENUM()
enum class EMeshModificationType : uint8
{
	/** The first Interim change since the last Final change.  This must be followed by either an Interim change or a Final change */
	FirstInterim,

	/** User is still in the middle of their interaction.  More changes to come, so don't bother finalizing everything yet (smoother performance) */
	Interim,

	/** User has finished their current interaction with this mesh, and everything needs to be finalized at this time */
	Final
};


UENUM()
enum class EMeshTopologyChange : uint8
{
	/** We won't be changing the mesh topology, but values could be changed (vertex positions, UVs, colors, etc.) */
	NoTopologyChange,

	/** Topology is changing with this edit, potentially along with other changes */
	TopologyChange
};



USTRUCT( BlueprintType )
struct FSubdividedQuadVertex
{
	GENERATED_BODY()

	/** The index of the vertex position (into the FSubdivisionLimitData's VertexPositions array) used for this vertex */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )	int32 VertexPositionIndex;

	/** Texture coordinates for this vertex.  We only support up to two, for now. (Just to avoid TArrays/allocations) */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )	FVector2D TextureCoordinate0;
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )	FVector2D TextureCoordinate1;

	/** Vertex color */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )	FColor VertexColor;

	/** Quad vertex normal */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )	FVector VertexNormal;

	/** Quad vertex tangent */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )	FVector VertexTangent;

	/** Quad vertex binormal sign (-1.0 or 1.0)*/
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )	float VertexBinormalSign;
};


USTRUCT( BlueprintType )
struct FSubdividedQuad
{
	GENERATED_BODY()

	// NOTE: The reason we're using separate variables instead of a static array is so that we can expose these to Blueprints, which doesn't support static array properties

	/** The vertices for the four corners of this quad */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )	FSubdividedQuadVertex QuadVertex0;
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )	FSubdividedQuadVertex QuadVertex1;
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )	FSubdividedQuadVertex QuadVertex2;
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )	FSubdividedQuadVertex QuadVertex3;

	const FSubdividedQuadVertex& GetQuadVertex( const int32 Index ) const
	{
		switch( Index )
		{
			default:
			case 0:
				return QuadVertex0;

			case 1:
				return QuadVertex1;

			case 2:
				return QuadVertex2;
			
			case 3:
				return QuadVertex3;
		}
	}

	FSubdividedQuadVertex& AccessQuadVertex( const int32 Index )
	{
		switch( Index )
		{
			default:
			case 0:
				return QuadVertex0;

			case 1:
				return QuadVertex1;

			case 2:
				return QuadVertex2;

			case 3:
				return QuadVertex3;
		}
	}
};


USTRUCT( BlueprintType )
struct FSubdividedWireEdge
{
	GENERATED_BODY()

	// NOTE: The reason we're using separate variables instead of a static array is so that we can expose these to Blueprints, which doesn't support static array properties

	/** The vertex indices for the two corners of this quad */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )	int32 EdgeVertex0PositionIndex;
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )	int32 EdgeVertex1PositionIndex;

	/** True if this edge is a counterpart to an original base cage edge of the mesh.  Otherwise it's a new edge that exists only
	    in the subdivision surfaces */
	bool bIsBaseCageCounterpartEdge;
};


USTRUCT( BlueprintType )
struct FSubdivisionLimitSection
{
	GENERATED_BODY()

	/** All of the quads in this section, as a result from subdividing the mesh */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	TArray<FSubdividedQuad> SubdividedQuads;
};


USTRUCT( BlueprintType )
struct FSubdivisionLimitData
{
	GENERATED_BODY()

	/** Positions of all of the vertices for this subdivision level.  Many vertex positions may be shared between subdivided quads. */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	TArray<FVector> VertexPositions;

	/** Data for each of the sections in the mesh.  This array will have the same number of elements as the editable mesh's 
	    section list (not necessarily the same indices though, due to sparseness). */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	TArray<FSubdivisionLimitSection> Sections;	

	/** All of the wire edges in the entire mesh (for all sections) */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	TArray<FSubdividedWireEdge> SubdividedWireEdges;
};


USTRUCT( BlueprintType )
struct FMeshElementAttributeData
{
	GENERATED_BODY()

	/** Name of the attribute */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FName AttributeName;

	/** Index of the attribute */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	int32 AttributeIndex;

	/** The value of this attribute */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FVector4 AttributeValue;

	/** Default constructor */
	FMeshElementAttributeData()
		: AttributeName( NAME_None ),
		  AttributeIndex( 0 ),
		  AttributeValue( 0.0f )
	{
	}

	/** Constructor that fills in all fields */
	FMeshElementAttributeData( const FName InitAttributeName, const int32 InitAttributeIndex, const FVector4 InitAttributeValue )
		: AttributeName( InitAttributeName ),
		  AttributeIndex( InitAttributeIndex ),
		  AttributeValue( InitAttributeValue )
	{
	}

	FString ToString() const
	{
		return FString::Printf(
			TEXT( "Name:%s, Index:%lu, Value:%s" ),
			*AttributeName.ToString(),
			AttributeIndex,
			*AttributeValue.ToString() );
	}
};


USTRUCT( BlueprintType )
struct FMeshElementAttributeList
{
	GENERATED_BODY()

	/** List of attributes to apply to a mesh element */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	TArray<FMeshElementAttributeData> Attributes;

	/** Default constructor */
	FMeshElementAttributeList()
		: Attributes()
	{
	}

	FString ToString() const
	{
		return FString::Printf(
			TEXT( "Attributes:%s" ),
			*LogHelpers::ArrayToString( Attributes ) );
	}
};


USTRUCT( BlueprintType )
struct FVertexToCreate
{
	GENERATED_BODY()

	/** Attributes of this vertex itself */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FMeshElementAttributeList VertexAttributes;

	/** The original ID of the vertex.  Should only be used by the undo system. */
	UPROPERTY()
	FVertexID OriginalVertexID;

	FVertexToCreate()
		: VertexAttributes(),
		  OriginalVertexID( FVertexID::Invalid )
	{
	}

	FString ToString() const
	{
		return FString::Printf(
			TEXT( "VertexAttributes:%s, OriginalVertexID:%s" ),
			*VertexAttributes.ToString(),
			*OriginalVertexID.ToString() );
	}
};


USTRUCT( BlueprintType )
struct FEdgeToCreate
{
	GENERATED_BODY()
		
	/** The first vertex this edge connects */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FVertexID VertexID0;

	/** The second vertex this edge connects */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FVertexID VertexID1;

	/** The polygons that are connected to this edge. */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	TArray<FPolygonRef> ConnectedPolygons;

	/** Attributes of this edge itself */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FMeshElementAttributeList EdgeAttributes;

	/** The original ID of the edge.  Should only be used by the undo system. */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FEdgeID OriginalEdgeID;

	/** Default constructor */
	FEdgeToCreate()
		: VertexID0( FVertexID::Invalid ),
		  VertexID1( FVertexID::Invalid ),
		  ConnectedPolygons(),
		  EdgeAttributes(),
		  OriginalEdgeID( FEdgeID::Invalid )
	{
	}

	FString ToString() const
	{
		return FString::Printf(
			TEXT( "VertexID0:%s, VertexID1:%s, ConnectedPolygons:%s, OriginalEdgeID:%s" ),
			*VertexID0.ToString(), 
			*VertexID1.ToString(),
			*LogHelpers::ArrayToString( ConnectedPolygons ),
			*EdgeAttributes.ToString(),
			OriginalEdgeID.GetValue() );
	}
};



USTRUCT( BlueprintType )
struct FVertexAndAttributes
{
	GENERATED_BODY()

	/** The vertex ID to insert into the polygon */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FVertexID VertexID;

	/** A list of polygon attributes to set for the vertex on the polygon we're inserting it into */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FMeshElementAttributeList PolygonVertexAttributes;

	/** Default constructor */
	FVertexAndAttributes()
		: VertexID( 0 ),
		  PolygonVertexAttributes()
	{
	}

	FString ToString() const
	{
		return FString::Printf(
			TEXT( "VertexID:%s, PolygonVertexAttributes:%s" ),
			*VertexID.ToString(),
			*PolygonVertexAttributes.ToString() );
	}
};


USTRUCT( BlueprintType )
struct FPolygonHoleVertices
{
	GENERATED_BODY()

	/** Ordered list of vertices that defines the hole's contour, along with the polygon vertex attributes to set for each vertex */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	TArray<FVertexAndAttributes> HoleVertices;

	/** Default constructor */
	FPolygonHoleVertices()
		: HoleVertices()
	{
	}

	FString ToString() const
	{
		return FString::Printf(
			TEXT( "HoleVertices:%s" ),
			*LogHelpers::ArrayToString( HoleVertices ) );
	}
};



USTRUCT( BlueprintType )
struct FPolygonToCreate
{
	GENERATED_BODY()

	/** The section the polygon will be added to */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FSectionID SectionID;

	/** Ordered list of vertices that defines the polygon's perimeter, along with the polygon vertex attributes to set for each vertex */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	TArray<FVertexAndAttributes> PerimeterVertices;
	
	/** For each hole in the polygon, an ordered list of vertices that defines that hole's boundary */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	TArray<FPolygonHoleVertices> PolygonHoles;

	/** The original ID of the polygon.  Should only be used by the undo system. */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FPolygonID OriginalPolygonID;

	/** Default constructor */
	FPolygonToCreate()
		: SectionID( 0 ),
		  PerimeterVertices(),
		  PolygonHoles(),
		  OriginalPolygonID( FPolygonID::Invalid )
	{
	}

	FString ToString() const
	{
		return FString::Printf(
			TEXT( "SectionID:%s, PerimeterVertices:%s, PolygonHoles:%s, OriginalPolygonID:%s" ),
			*SectionID.ToString(),
			*LogHelpers::ArrayToString( PerimeterVertices ),
			*LogHelpers::ArrayToString( PolygonHoles ),
			*OriginalPolygonID.ToString() );
	}
};



USTRUCT( BlueprintType )
struct FVertexPair
{
	GENERATED_BODY()

	/** The first vertex ID in this pair */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FVertexID VertexID0;

	/** The second vertex ID in this pair */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FVertexID VertexID1;

	/** Default constructor */
	FVertexPair()
		: VertexID0( FVertexID::Invalid ),
		  VertexID1( FVertexID::Invalid )
	{
	}

	// @todo mesheditor: Do we need a ToString() function?  Would it ever be called?
};


USTRUCT( BlueprintType )
struct FPolygonToSplit
{
	GENERATED_BODY()

	/** The polygon that we'll be splitting */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FPolygonRef PolygonRef;

	/** A list of pairs of vertices that new edges will be created at.  The pairs must be ordered, and the vertices
	    must already exist and be connected to the polygon */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	TArray<FVertexPair> VertexPairsToSplitAt;

	/** Default constructor */
	FPolygonToSplit()
		: PolygonRef( FSectionID::Invalid, FPolygonID::Invalid ),
		  VertexPairsToSplitAt()
	{
	}

	// @todo mesheditor: Do we need a ToString() function?  Would it ever be called?
};


USTRUCT( BlueprintType )
struct FAttributesForVertex
{
	GENERATED_BODY()

	/** The vertex ID to set attributes on */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FVertexID VertexID;

	/** A list of attributes to set for the vertex */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FMeshElementAttributeList VertexAttributes;

	/** Default constructor */
	FAttributesForVertex()
		: VertexID( 0 ),
		  VertexAttributes()
	{
	}

	FString ToString() const
	{
		return FString::Printf(
			TEXT( "VertexID:%s, VertexAttributes:%s" ),
			*VertexID.ToString(),
			*VertexAttributes.ToString() );
	}
};


USTRUCT( BlueprintType )
struct FAttributesForEdge
{
	GENERATED_BODY()

	/** The edge ID to set attributes on */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FEdgeID EdgeID;

	/** A list of attributes to set for the edge */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FMeshElementAttributeList EdgeAttributes;

	/** Default constructor */
	FAttributesForEdge()
		: EdgeID( 0 ),
		  EdgeAttributes()
	{
	}

	FString ToString() const
	{
		return FString::Printf(
			TEXT( "EdgeID:%s, EdgeAttributes:%s" ),
			*EdgeID.ToString(),
			*EdgeAttributes.ToString() );
	}
};


USTRUCT( BlueprintType )
struct FVertexAttributesForPolygonHole
{
	GENERATED_BODY()

	/** For each hole vertex, a list of attributes for that vertex.  You can leave a given array empty for 
	    a specific hole index if you don't want to set attributes for select holes. */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	TArray<FMeshElementAttributeList> VertexAttributeList;

	/** Default constructor */
	FVertexAttributesForPolygonHole()
		: VertexAttributeList()
	{
	}

	FString ToString() const
	{
		return FString::Printf(
			TEXT( "VertexAttributeList:%s" ),
			*LogHelpers::ArrayToString( VertexAttributeList ) );
	}
};


USTRUCT( BlueprintType )
struct FVertexAttributesForPolygon
{
	GENERATED_BODY()

	/** The polygon to set vertex attributes on */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FPolygonRef PolygonRef;

	/** For each polygon vertex, a list of attributes for that vertex.  Can be left empty if you don't want to set any attributes. */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	TArray<FMeshElementAttributeList> PerimeterVertexAttributeLists;

	/** For each hole vertex, a list of attributes for that vertex.  Can be left empty if you don't want to set any attributes.  Also
	    you can leave a given array empty for a specific hole index if you don't want to set attributes for select holes. */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	TArray<FVertexAttributesForPolygonHole> VertexAttributeListsForEachHole;

	/** Default constructor */
	FVertexAttributesForPolygon()
		: PolygonRef( FSectionID::Invalid, FPolygonID::Invalid ),
		  PerimeterVertexAttributeLists(),
		  VertexAttributeListsForEachHole()
	{
	}

	FString ToString() const
	{
		return FString::Printf(
			TEXT( "PolygonRef:%s, PerimeterVertexAttributeLists:%s, VertexAttributeListsForEachHole:%s" ),
			*PolygonRef.ToString(),
			*LogHelpers::ArrayToString( PerimeterVertexAttributeLists ),
			*LogHelpers::ArrayToString( VertexAttributeListsForEachHole ) );
	}
};


USTRUCT( BlueprintType )
struct FVerticesForEdge
{
	GENERATED_BODY()

	/** The edge ID */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FEdgeID EdgeID;

	/** First new vertex ID for this edge */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FVertexID NewVertexID0;

	/** Second new vertex ID for this edge */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FVertexID NewVertexID1;

	/** Default constructor */
	FVerticesForEdge()
		: EdgeID( 0 ),
		  NewVertexID0( FVertexID::Invalid ),	// @todo mesheditor urgent: Typesafety isn't working -- able to assign EdgeID to VertexID!
		  NewVertexID1( FVertexID::Invalid )
	{
	}

	FString ToString() const
	{
		return FString::Printf(
			TEXT( "EdgeID:%s, NewVertexID0:%s, NewVertexID1:%s]" ),
			*EdgeID.ToString(),
			*NewVertexID0.ToString(), 
			*NewVertexID1.ToString() );
	}
};


USTRUCT( BlueprintType )
struct FVertexToMove
{
	GENERATED_BODY()

	/** The vertex we'll be moving around */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FVertexID VertexID;

	/** The new position of the vertex */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FVector NewVertexPosition;


	FVertexToMove()
		: VertexID( FVertexID::Invalid ),
		  NewVertexPosition( FVector::ZeroVector )
	{
	}

	FString ToString() const
	{
		return FString::Printf(
			TEXT( "VertexID:%s, NewVertexPosition:%s" ),
			*VertexID.ToString(),
			*NewVertexPosition.ToString() );
	}
};


USTRUCT( BlueprintType )
struct FSectionToCreate
{
	GENERATED_BODY()

	/** Material to assign to the new section */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	class UMaterialInterface* Material;

	/** Whether the new section should have collision enabled */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	bool bEnableCollision;

	/** Whether the new section casts a shadow */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	bool bCastShadow;

	/** The original ID of the section.  Should only be used by the undo system. */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	FSectionID OriginalSectionID;

	/** The original rendering section index.  Should only be used by the undo system. */
	UPROPERTY( BlueprintReadWrite, Category="Editable Mesh" )
	int32 OriginalRenderingSectionIndex;

	/** Default constructor */
	FSectionToCreate()
		: Material( nullptr )
		, bEnableCollision( false )
		, bCastShadow( false )
		, OriginalSectionID( FSectionID::Invalid )
		, OriginalRenderingSectionIndex( INDEX_NONE )
	{
	}

	FString ToString() const
	{
		return FString::Printf(
			TEXT( "Material:%s, bEnableCollision:%s, bCastShadow:%s, OriginalSectionID:%s" ),
			Material ? *Material->GetName() : TEXT( "<none>" ),
			*LexicalConversion::ToString( bEnableCollision ),
			*LexicalConversion::ToString( bCastShadow ),
			*OriginalSectionID.ToString() );
	}
};
