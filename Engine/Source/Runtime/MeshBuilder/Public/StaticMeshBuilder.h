// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshBuilder.h"
#include "MeshDescription.h"


class MESHBUILDER_API FStaticMeshBuilder : public FMeshBuilder
{
public:
	static FStaticMeshBuilder& Get()
	{
		static FStaticMeshBuilder Instance;
		return Instance;
	}

	virtual bool Build(class UStaticMesh* StaticMesh) override;
	virtual ~FStaticMeshBuilder() {}

private:
	FStaticMeshBuilder();

	void OnBuildRenderMeshStart(class UStaticMesh* StaticMesh, const bool bInvalidateLighting);
	void OnBuildRenderMeshFinish(class UStaticMesh* StaticMesh, const bool bRebuildBoundsAndCollision);
	
	int32 GetPolygonGroupTriangles(const class UMeshDescription *MeshDescription, TArray<FMeshTriangle>& OutTriangles, const FPolygonGroupID& PolygonGroupID);

	bool IsOrphanedVertex(const class UMeshDescription *MeshDescription, const FVertexID VertexID) const;
	void UpdateBounds(class UStaticMesh* StaticMesh);
	void UpdateCollision(UStaticMesh *StaticMesh);

	/** Used to refresh all components in the scene that may be using a mesh we're editing */
	TSharedPtr<class FStaticMeshComponentRecreateRenderStateContext> RecreateRenderStateContext;
};

