// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/GeometryCollection.h"
#include "Templates/SharedPointer.h"

namespace GeometryCollection 
{

	/****
	* MakeCubeElement
	*   Utility to create a triangulated unit cube using the FGeometryCollection format.
	*/
	TSharedPtr<FGeometryCollection> 
	GEOMETRYCOLLECTIONCORE_API 
	MakeCubeElement(const FTransform& center, FVector Scale);

	/****
	* SetupCubeGridExample
	*   Utility to create a grid (10x10x10) of triangulated unit cube using the FGeometryCollection format.
	*/
	void 
	GEOMETRYCOLLECTIONCORE_API 
	SetupCubeGridExample(TSharedPtr<FGeometryCollection> GeometryCollection);


	/****
	* Setup Nested Hierarchy Example	
	*/
	void
	GEOMETRYCOLLECTIONCORE_API
	SetupNestedBoneCollection(FGeometryCollection * Collection);

	/****
	* Setup Two Clustered Cubes : 
	* ... geometry       { (-9,0,0) && (9,0,0)}
	* ... center of mass { (-10,0,0) && (10,0,0)}
	*/
	void
	GEOMETRYCOLLECTIONCORE_API
	SetupTwoClusteredCubesCollection(FGeometryCollection * Collection);


	/***
	* Add the geometry group to a collection. Mostly for backwards compatibility with older files. 
	*/
	void 
	GEOMETRYCOLLECTIONCORE_API 
	AddGeometryProperties(FGeometryCollection * Collection);

	/***
	* Ensure Material indices are setup correctly. Mostly for backwards compatibility with older files. 
	*/
	void 
	GEOMETRYCOLLECTIONCORE_API 
	MakeMaterialsContiguous(FGeometryCollection * Collection);
};
