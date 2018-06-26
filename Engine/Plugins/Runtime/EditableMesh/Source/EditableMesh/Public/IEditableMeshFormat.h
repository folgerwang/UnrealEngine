// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "Features/IModularFeatures.h"


/**
 * Interface for mesh primitives that can be interactively edited (low-poly editing, sculpting, attribute painting, etc.)
 */
class IEditableMeshFormat : public IModularFeature
{

public:

	// @todo mesheditor: Comments!
	virtual void FillMeshObjectPtr( class UPrimitiveComponent& Component, struct FEditableMeshSubMeshAddress& SubMeshAddress ) = 0;
	virtual class UEditableMesh* MakeEditableMesh( class UPrimitiveComponent& Component, const struct FEditableMeshSubMeshAddress& SubMeshAddress ) = 0;

};