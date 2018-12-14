// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/Set.h"

/**
* A set of triangles which are rendered with the same material.
*/
struct FGeometryCollectionSection
{
	/** Constructor. */
	FGeometryCollectionSection()
		: MaterialID(0)
		, FirstIndex(0)
		, NumTriangles(0)
		, MinVertexIndex(0)
		, MaxVertexIndex(0)
	{ }	
	
	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar, FGeometryCollectionSection& Section)
	{
		return Ar << Section.MaterialID << Section.FirstIndex << Section.NumTriangles << Section.MinVertexIndex << Section.MaxVertexIndex;
	}

	bool Serialize(FArchive& Ar)
	{
		Ar << *this;
		return true;
	}

	/** The index of the material with which to render this section. */
	int32 MaterialID;

	/** Range of vertices and indices used when rendering this section. */
	int32 FirstIndex;
	int32 NumTriangles;
	int32 MinVertexIndex;
	int32 MaxVertexIndex;

	static const int InvalidIndex = -1;
};