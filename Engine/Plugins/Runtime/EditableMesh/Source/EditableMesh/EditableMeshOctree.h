// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericOctreePublic.h"
#include "GenericOctree.h"
#include "EditableMesh.h"


struct FEditableMeshOctreePolygon
{
	class UEditableMesh& EditableMesh;
	FPolygonID PolygonID;
	FBoxCenterAndExtent PolygonBounds;

	FEditableMeshOctreePolygon( UEditableMesh& InitEditableMesh, const FPolygonID& InitPolygonID, const FBoxCenterAndExtent& InitPolygonBounds )
		: EditableMesh( InitEditableMesh ),
		  PolygonID( InitPolygonID ),
		  PolygonBounds( InitPolygonBounds )
	{
	}
};


struct FEditableMeshOctreeSemantics
{
	// @todo mesheditor perf: Profile and tweak these octree settings

	// When a leaf gets more than this number of elements, it will split itself into a node with multiple child leaves
	enum { MaxElementsPerLeaf = 6 };

	// This is used for incremental updates.  When removing a polygon, larger values will cause leaves to be removed and collapsed into a parent node.
	enum { MinInclusiveElementsPerNode = 7 };

	// How deep the tree can go.
	enum { MaxNodeDepth = 20 };


	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	FORCEINLINE static FBoxCenterAndExtent GetBoundingBox( const FEditableMeshOctreePolygon& Element )
	{
		return Element.PolygonBounds;
	}

	FORCEINLINE static bool AreElementsEqual( const FEditableMeshOctreePolygon& A, const FEditableMeshOctreePolygon& B )
	{
		return ( A.PolygonID == B.PolygonID );
	}

	FORCEINLINE static void SetElementId( const FEditableMeshOctreePolygon& Element, FOctreeElementId OctreeElementID )
	{
		Element.EditableMesh.PolygonIDToOctreeElementIDMap.Add( Element.PolygonID, OctreeElementID );
	}
};

class FEditableMeshOctree : public TOctree<FEditableMeshOctreePolygon, FEditableMeshOctreeSemantics>
{
public:
	FEditableMeshOctree( const FVector& InOrigin, float InExtent )
		: TOctree( InOrigin, InExtent )
	{
	}
};


