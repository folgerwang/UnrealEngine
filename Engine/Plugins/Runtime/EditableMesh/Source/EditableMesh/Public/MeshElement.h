// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditableMeshTypes.h"
#include "Components/PrimitiveComponent.h"

class UEditableMesh;

/**
 * Uniquely identifies a specific element within a mesh
 */
struct FEditableMeshElementAddress
{
	/** The sub-mesh address that the element is contained by */
	FEditableMeshSubMeshAddress SubMeshAddress;

	/** The type of element */
	EEditableMeshElementType ElementType;

	/** The ID of the element within the mesh */
	FElementID ElementID;

	/** Group or bone within a skeletal mesh */
	FPolygonGroupID BoneID;


	/** Default constructor that initializes variables to an invalid element address */
	FEditableMeshElementAddress()
		: SubMeshAddress(),
		  ElementType( EEditableMeshElementType::Invalid ),
		  ElementID( FElementID::Invalid ),
		  BoneID(FPolygonGroupID::Invalid )
	{
	}

	FEditableMeshElementAddress( const FEditableMeshSubMeshAddress& InSubMeshAddress, FVertexID InVertexID )
		: SubMeshAddress( InSubMeshAddress ),
		  ElementType( EEditableMeshElementType::Vertex ),
		  ElementID(InVertexID),
		  BoneID(FPolygonGroupID::Invalid)
	{
	}

	FEditableMeshElementAddress( const FEditableMeshSubMeshAddress& InSubMeshAddress, FEdgeID InEdgeID )
		: SubMeshAddress( InSubMeshAddress ),
		  ElementType( EEditableMeshElementType::Edge ),
		  ElementID(InEdgeID),
		  BoneID(FPolygonGroupID::Invalid)
	{
	}

	FEditableMeshElementAddress( const FEditableMeshSubMeshAddress& InSubMeshAddress, FPolygonID InPolygonID )
		: SubMeshAddress( InSubMeshAddress ),
		  ElementType( EEditableMeshElementType::Polygon ),
		  ElementID(InPolygonID),
		  BoneID(FPolygonGroupID::Invalid)
	{
	}

	/** Equality check */
	inline bool operator==( const FEditableMeshElementAddress& Other ) const
	{
		return
			SubMeshAddress == Other.SubMeshAddress &&
			ElementType == Other.ElementType &&
			ElementID == Other.ElementID;
	}

	/** Convert to a string */
	inline FString ToString() const
	{
		FString ElementTypeString;
		switch( ElementType )
		{
			case EEditableMeshElementType::Invalid:
				ElementTypeString = TEXT( "Invalid" );
				break;

			case EEditableMeshElementType::Vertex:
				ElementTypeString = TEXT( "Vertex" );
				break;

			case EEditableMeshElementType::Edge:
				ElementTypeString = TEXT( "Edge" );
				break;

			case EEditableMeshElementType::Polygon:
				ElementTypeString = TEXT( "Polygon" );
				break;

			default:
				check( 0 );	// Unrecognized type
		}

		return FString::Printf(
			TEXT( "%s, ElementType:%s, ElementID:%s" ),
			*SubMeshAddress.ToString(),
			*ElementTypeString,
			*ElementID.ToString() );
	}
};


struct EDITABLEMESH_API FMeshElement
{
	/** The component that is referencing the mesh.  Does not necessarily own the mesh!  The mesh could be shared
		between many components. */
	TWeakObjectPtr<class UPrimitiveComponent> Component;

	/** The address of the mesh element */
	FEditableMeshElementAddress ElementAddress;

	/** Real time in seconds that we were last hovered over */
	double LastHoverTime;

	/** Real time in seconds that we were last selected */
	double LastSelectTime;


	/** Default constructor that initializes everything to safe values */
	FMeshElement();

	FMeshElement( UPrimitiveComponent* InComponent, const FEditableMeshSubMeshAddress& InSubMeshAddress, FVertexID InVertexID, double InLastHoverTime = 0.0, double InLastSelectTime = 0.0 );

	FMeshElement( UPrimitiveComponent* InComponent, const FEditableMeshSubMeshAddress& InSubMeshAddress, FEdgeID InEdgeID, double InLastHoverTime = 0.0, double InLastSelectTime = 0.0 );

	FMeshElement( UPrimitiveComponent* InComponent, const FEditableMeshSubMeshAddress& InSubMeshAddress, FPolygonID InPolygonID, double InLastHoverTime = 0.0, double InLastSelectTime = 0.0 );

	/** Checks to see if we have something valid */
	bool IsValidMeshElement() const;

	/** Checks to see if this mesh element points to the same element as another mesh element */
	bool IsSameMeshElement( const FMeshElement& Other ) const;

	/** Convert to a string */
	FString ToString() const;

	/** Checks to see that the mesh element actually exists in the mesh */
	bool IsElementIDValid( const UEditableMesh* EditableMesh ) const;
};

