// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "CoreMinimal.h"

struct FMeshElement;
class UEditableMesh;
class UMaterialInterface;

class MESHEDITOR_API FMeshEditorUtilities
{
public:
	/**
	 * Assigns a material to polygons
	 *
	 * @param SelectedMaterial	The material to assign
	 * @param EditableMesh		The mesh for which the polygons belong to
	 * @param PolygonElements	The polygons to assign the material to
	 *
	 * @return whether successful
	 */
	static bool AssignMaterialToPolygons( UMaterialInterface* SelectedMaterial, UEditableMesh* EditableMesh, const TArray< FMeshElement >& PolygonElements );
};
