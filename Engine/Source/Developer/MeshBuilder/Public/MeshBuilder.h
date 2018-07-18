// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Abstract class which is the base class of all builder.
 * All share code to build some render data should be found inside this class
 */
class MESHBUILDER_API FMeshBuilder
{
public:
	FMeshBuilder();
	
	/**
	 * Build function should be override and is the starting point for all mesh builders
	 */
	virtual bool Build(class FStaticMeshRenderData& OutRenderData, class UStaticMesh* StaticMesh, const class FStaticMeshLODGroup& LODGroup) =0;

private:

};
