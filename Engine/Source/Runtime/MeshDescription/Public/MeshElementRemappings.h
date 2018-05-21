// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshTypes.h"

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

