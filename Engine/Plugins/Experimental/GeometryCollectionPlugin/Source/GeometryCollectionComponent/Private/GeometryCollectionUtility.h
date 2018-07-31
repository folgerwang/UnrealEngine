// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "Engine/EngineTypes.h"
#include "GeometryCollection.h"
#include "Templates/SharedPointer.h"

namespace GeometryCollection 
{

	/****
	* MakeCubeElement
	*   Utility to create a triangulated unit cube using the UGeometryCollection format.
	*/
	TSharedPtr<UGeometryCollection> MakeCubeElement(const FTransform& center, float Scale);

	/****
	* SetupCubeGridExample
	*   Utility to create a grid (10x10x10) of triangulated unit cube using the UGeometryCollection format.
	*/
	void SetupCubeGridExample(TSharedPtr<UGeometryCollection> GeometryCollection);

};
