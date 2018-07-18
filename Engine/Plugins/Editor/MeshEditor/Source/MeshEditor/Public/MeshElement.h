// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditableMesh.h"
#include "EditableMeshTypes.h"
#include "Components/PrimitiveComponent.h"

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


	/** Default constructor that initializes variables to an invalid element address */
	FEditableMeshElementAddress()
		: SubMeshAddress(),
		  ElementType( EEditableMeshElementType::Invalid ),
		  ElementID( FElementID::Invalid )
	{
	}

	FEditableMeshElementAddress( const FEditableMeshSubMeshAddress& InSubMeshAddress, FVertexID InVertexID )
		: SubMeshAddress( InSubMeshAddress ),
		  ElementType( EEditableMeshElementType::Vertex ),
		  ElementID( InVertexID )
	{
	}

	FEditableMeshElementAddress( const FEditableMeshSubMeshAddress& InSubMeshAddress, FEdgeID InEdgeID )
		: SubMeshAddress( InSubMeshAddress ),
		  ElementType( EEditableMeshElementType::Edge ),
		  ElementID( InEdgeID )
	{
	}

	FEditableMeshElementAddress( const FEditableMeshSubMeshAddress& InSubMeshAddress, FPolygonID InPolygonID )
		: SubMeshAddress( InSubMeshAddress ),
		  ElementType( EEditableMeshElementType::Polygon ),
		  ElementID( InPolygonID )
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


struct MESHEDITOR_API FMeshElement
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
	FMeshElement()
		: Component( nullptr ),
		  ElementAddress(),
		  LastHoverTime( 0.0 ),
		  LastSelectTime( 0.0 )
	{
	}

	FMeshElement( UPrimitiveComponent* InComponent, const FEditableMeshSubMeshAddress& InSubMeshAddress, FVertexID InVertexID, double InLastHoverTime = 0.0, double InLastSelectTime = 0.0 )
		: Component( InComponent ),
		  ElementAddress( InSubMeshAddress, InVertexID ),
		  LastHoverTime( InLastHoverTime ),
		  LastSelectTime( InLastSelectTime )
	{
	}

	FMeshElement( UPrimitiveComponent* InComponent, const FEditableMeshSubMeshAddress& InSubMeshAddress, FEdgeID InEdgeID, double InLastHoverTime = 0.0, double InLastSelectTime = 0.0 )
		: Component( InComponent ),
		  ElementAddress( InSubMeshAddress, InEdgeID ),
		  LastHoverTime( InLastHoverTime ),
		  LastSelectTime( InLastSelectTime )
	{
	}

	FMeshElement( UPrimitiveComponent* InComponent, const FEditableMeshSubMeshAddress& InSubMeshAddress, FPolygonID InPolygonID, double InLastHoverTime = 0.0, double InLastSelectTime = 0.0 )
		: Component( InComponent ),
		  ElementAddress( InSubMeshAddress, InPolygonID ),
		  LastHoverTime( InLastHoverTime ),
		  LastSelectTime( InLastSelectTime )
	{
	}

	/** Checks to see if we have something valid */
	inline bool IsValidMeshElement() const
	{
		return
			( Component.IsValid() &&
				ElementAddress.SubMeshAddress.EditableMeshFormat != nullptr &&
				ElementAddress.ElementType != EEditableMeshElementType::Invalid );
	}

	/** Checks to see if this mesh element points to the same element as another mesh element */
	inline bool IsSameMeshElement( const FMeshElement& Other ) const
	{
		// NOTE: We only care that the element addresses are the same, not other transient state
		return Component == Other.Component && ElementAddress == Other.ElementAddress;
	}

	/** Convert to a string */
	FString ToString() const
	{
		return FString::Printf(
			TEXT( "Component:%s, %s" ),
			Component.IsValid() ? *Component->GetName() : TEXT( "<Invalid>" ),
			*ElementAddress.ToString() );
	}

	/** Checks to see that the mesh element actually exists in the mesh */
	bool IsElementIDValid( const UEditableMesh* EditableMesh ) const
	{
		bool bIsValid = false;

		if( EditableMesh != nullptr && ElementAddress.ElementID != FElementID::Invalid )
		{
			switch( ElementAddress.ElementType )
			{
			case EEditableMeshElementType::Vertex:
				bIsValid = EditableMesh->IsValidVertex( FVertexID( ElementAddress.ElementID ) );
				break;

			case EEditableMeshElementType::Edge:
				bIsValid = EditableMesh->IsValidEdge( FEdgeID( ElementAddress.ElementID ) );
				break;

			case EEditableMeshElementType::Polygon:
				bIsValid = EditableMesh->IsValidPolygon( FPolygonID( ElementAddress.ElementID ) );
				break;
			}
		}

		return bIsValid;
	}
};

