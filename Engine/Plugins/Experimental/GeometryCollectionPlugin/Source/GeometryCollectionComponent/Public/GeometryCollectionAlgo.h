// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryCollection.h"

namespace GeometryCollectionAlgo
{

	/*
	* Print the parent hierarchy of the collection.
	*/
	void 
	GEOMETRYCOLLECTIONCOMPONENT_API PrintParentHierarchy(const UGeometryCollection * Collection);

	/*
	*  Generate a contiguous array of int32's from 0 to Length-1
	*/
	TSharedRef<TArray<int32> > 
	GEOMETRYCOLLECTIONCOMPONENT_API ContiguousArray(int32 Length);

	/*
	*  Check if the Collection has multiple transform roots.
	*/
	bool 
	GEOMETRYCOLLECTIONCOMPONENT_API HasMultipleRoots(UGeometryCollection * Collection);

	/*
	* Parent a single transform
	*/
	void
	GEOMETRYCOLLECTIONCOMPONENT_API ParentTransform(UGeometryCollection* GeometryCollection, const int32 ParentIndex, const int32 ChildIndex);
		
	/*
	*  Parent the list of transforms to the selected index. 
	*/
	void 
	GEOMETRYCOLLECTIONCOMPONENT_API ParentTransforms(UGeometryCollection* GeometryCollection, const int32 InsertAtIndex, const TArray<int32>& SelectedBones);

	/*
	*  Find the average position of the transforms.
	*/
	FVector
	GEOMETRYCOLLECTIONCOMPONENT_API AveragePosition(UGeometryCollection* Collection, const TArray<int32>& Indices);

	/*
	*  Global Matrices of the collection based on list of indices
	*/
	void 
	GEOMETRYCOLLECTIONCOMPONENT_API GlobalMatrices(UGeometryCollection* GeometryCollection, const TArray<int32>& Indices, TArray<FTransform> & Transforms);

	/*
	*  Global Matrices of the collection, transforms will be resized to fit
	*/
	void 
	GEOMETRYCOLLECTIONCOMPONENT_API GlobalMatrices(UGeometryCollection* GeometryCollection, TArray<FTransform> & Transforms);

	/*
	*  Perpare for simulation moves the geometry to center of mass aligned
	*/
	void 
	GEOMETRYCOLLECTIONCOMPONENT_API PrepareForSimulation(UGeometryCollection* GeometryCollection);


}
