// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshBuilder.h"

DECLARE_LOG_CATEGORY_EXTERN(LogStaticMeshBuilder, Log, All);

class MESHBUILDER_API FStaticMeshBuilder : public FMeshBuilder
{
public:
	FStaticMeshBuilder();

	virtual bool Build(class FStaticMeshRenderData& OutRenderData, class UStaticMesh* StaticMesh, const class FStaticMeshLODGroup& LODGroup) override;
	virtual ~FStaticMeshBuilder() {}

private:

	void OnBuildRenderMeshStart(class UStaticMesh* StaticMesh, const bool bInvalidateLighting);
	void OnBuildRenderMeshFinish(class UStaticMesh* StaticMesh, const bool bRebuildBoundsAndCollision);

	/** Used to refresh all components in the scene that may be using a mesh we're editing */
	TSharedPtr<class FStaticMeshComponentRecreateRenderStateContext> RecreateRenderStateContext;
};

