// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshTypes.generated.h"

MESHDESCRIPTION_API DECLARE_LOG_CATEGORY_EXTERN( LogMeshDescription, Log, All );


// @todo mesheditor: Need comments

USTRUCT( BlueprintType )
struct FElementID	// @todo mesheditor script: BP doesn't have name spaces, so we might need a more specific display name, or just rename our various types
{
	GENERATED_BODY()

	FElementID()
		: IDValue(Invalid.GetValue())
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
		return ( IDValue == Invalid.GetValue() ) ? TEXT( "Invalid" ) : FString::Printf( TEXT( "%d" ), IDValue );
	}

	friend FArchive& operator<<( FArchive& Ar, FElementID& Element )
	{
		Ar << Element.IDValue;
		return Ar;
	}

	/** Invalid element ID */
	MESHDESCRIPTION_API static const FElementID Invalid;

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
	MESHDESCRIPTION_API static const FVertexID Invalid;
};


USTRUCT( BlueprintType )
struct FVertexInstanceID : public FElementID
{
	GENERATED_BODY()

	FVertexInstanceID()
	{
	}

	explicit FVertexInstanceID( const FElementID InitElementID )
		: FElementID( InitElementID.GetValue() )
	{
	}

	explicit FVertexInstanceID( const uint32 InitIDValue )
		: FElementID( InitIDValue )
	{
	}

	FORCEINLINE friend uint32 GetTypeHash( const FVertexInstanceID& Other )
	{
		return GetTypeHash( Other.IDValue );
	}

	/** Invalid rendering vertex ID */
	MESHDESCRIPTION_API static const FVertexInstanceID Invalid;
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
	MESHDESCRIPTION_API static const FEdgeID Invalid;
};


USTRUCT( BlueprintType )
struct FPolygonGroupID : public FElementID
{
	GENERATED_BODY()

	FPolygonGroupID()
	{
	}

	explicit FPolygonGroupID( const FElementID InitElementID )
		: FElementID( InitElementID.GetValue() )
	{
	}

	explicit FPolygonGroupID( const int32 InitIDValue )
		: FElementID( InitIDValue )
	{
	}

	FORCEINLINE friend uint32 GetTypeHash( const FPolygonGroupID& Other )
	{
		return GetTypeHash( Other.IDValue );
	}

	/** Invalid section ID */
	MESHDESCRIPTION_API static const FPolygonGroupID Invalid;
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
	MESHDESCRIPTION_API static const FPolygonID Invalid;	// @todo mesheditor script: Can we expose these to BP nicely?	Do we even need to?
};


#if 0
UCLASS( abstract )
class MESHDESCRIPTION_API UEditableMeshAttribute : public UObject
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
	// Vertex instance data
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

	//
	// Polygons
	//

	/** Static: The attribute name for polygon normal */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FName PolygonNormal()
	{
		return PolygonNormalName;
	}

	/** Static: The attribute name for polygon center */
	UFUNCTION( BlueprintPure, Category="Editable Mesh" ) static inline FName PolygonCenter()
	{
		return PolygonCenterName;
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
	static const FName PolygonNormalName;
	static const FName PolygonCenterName;
};
#endif
