// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshUtilitiesLibrary.generated.h"

/** Blueprint library for altering and analyzing static mesh data */
UCLASS()
class UStaticMeshUtilitiesLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Enables/disables mesh section collision */
	UFUNCTION(BlueprintCallable, Category = "StaticMeshUtilitiesLibrary")
	static void EnableSectionCollision(UStaticMesh* StaticMesh, bool bCollisionEnabled, int32 LODIndex, int32 SectionIndex);

	/** Checks if a mesh section has collision */
	UFUNCTION(BlueprintPure, Category = "StaticMeshUtilitiesLibrary")
	static bool IsSectionCollisionEnabled(UStaticMesh* StaticMesh, int32 LODIndex, int32 SectionIndex);

	/** Enables/disables mesh section shadow casting */
	UFUNCTION(BlueprintCallable, Category = "StaticMeshUtilitiesLibrary")
	static void EnableSectionCastShadow(UStaticMesh* StaticMesh, bool bCastShadow, int32 LODIndex, int32 SectionIndex);

	/** Check whether a static mesh has vertex colors */
	UFUNCTION(BlueprintPure, Category = "StaticMeshUtilitiesLibrary")
	static bool HasVertexColors(UStaticMesh* StaticMesh);

	/** Check whether a static mesh component has vertex colors */
	UFUNCTION(BlueprintPure, Category = "StaticMeshUtilitiesLibrary")
	static bool HasInstanceVertexColors(UStaticMeshComponent* StaticMeshComponent);

	/** Set Genrate Lightmap UVs for StaticMesh */
	/** Check whether a static mesh component has vertex colors */
	UFUNCTION(BlueprintCallable, Category = "StaticMeshUtilitiesLibrary")
	static bool SetGenerateLightmapUVs(UStaticMesh* StaticMesh, bool bGenerateLightmapUVs);

	/** Check whether a static mesh component has vertex colors */
	UFUNCTION(BlueprintPure, Category = "StaticMeshUtilitiesLibrary")
	static TEnumAsByte<ECollisionTraceFlag> GetCollisionComplexity(UStaticMesh* StaticMesh);

	/** Get number of StaticMesh verts for an LOD */
	UFUNCTION(BlueprintPure, Category = "StaticMeshUtilitiesLibrary")
	static int32 GetNumberVerts(UStaticMesh* StaticMesh, int32 LODIndex);

	/** Returns an array of LOD screen sizes for evaluation */
	UFUNCTION(BlueprintPure, Category = "StaticMeshUtilitiesLibrary")
	static TArray<float> GetLODScreenSizes(UStaticMesh* StaticMesh);

	/** Sets StaticMeshFlag bAllowCPUAccess  */
	UFUNCTION(BlueprintCallable, Category = "StaticMeshUtilitiesLibrary")
	static void SetAllowCPUAccess(UStaticMesh* StaticMesh, bool bAllowCPUAccess);
};
