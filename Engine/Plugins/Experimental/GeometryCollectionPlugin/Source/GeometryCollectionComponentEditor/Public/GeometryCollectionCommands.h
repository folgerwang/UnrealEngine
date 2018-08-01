// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/World.h"

class UStaticMesh;
class USkeletalMesh;
class UGeometryCollection;

/**
* The public interface to this module
*/
class GEOMETRYCOLLECTIONCOMPONENTEDITOR_API FGeometryCollectionCommands
{
public:

	/**
	*  Command invoked from "GeometryCollection.ToString", uses the selected GeometryCollectionActor to output the RestCollection to the Log
	*  @param World
	*/
	static void ToString(UWorld * World);


	/*
	*  Split across xz-plane
	*/
	static void SplitAcrossYZPlane(UWorld * World);

	/*
	*  Ensure single root.
	*/
	static int32 EnsureSingleRoot(UGeometryCollection* RestCollection);

};