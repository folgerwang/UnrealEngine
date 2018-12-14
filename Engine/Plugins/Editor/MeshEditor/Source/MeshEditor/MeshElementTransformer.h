// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewportTransformer.h"
#include "MeshElementTransformer.generated.h"


UCLASS()
class UMeshElementTransformer : public UViewportTransformer
{
	GENERATED_BODY()

public:

	// UViewportTransformer overrides
	virtual void Init( class UViewportWorldInteraction* InitViewportWorldInteraction ) override;
	virtual void Shutdown() override;
	virtual bool ShouldCenterTransformGizmoPivot() const override
	{
		// Mesh elements always default to being pivoted from the center of all elements' bounds
		return true;
	}

protected:


};

