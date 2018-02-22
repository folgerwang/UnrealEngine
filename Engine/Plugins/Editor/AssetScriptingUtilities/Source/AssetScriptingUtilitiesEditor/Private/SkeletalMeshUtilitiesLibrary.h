// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "SkeletalMeshUtilitiesLibrary.generated.h"

/** Blueprint library for altering and analyzing static mesh data */
UCLASS()
class USkeletalMeshUtilitiesLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Regenerate LODs of the mesh 
	 * 
	 * @param SkeletalMesh : the mesh that will regenerate LOD
	 * @param NewLODCount : Set valid value (>0) if you want to change LOD count. 
	 *						Otherwise, it will use the current LOD and regenerate
	 * @param bRegenerateEvenIfImported : If this is true, it only regenerate even if this LOD was imported before
	 *									If false, it will regenerate for only previously auto generated ones
	 * 
	 * @return true if succeed. If mesh reduction is not available this will return false. 
	 */
	UFUNCTION(BlueprintCallable, Category = "SkeletalMeshUtilitiesLibrary")
	static bool RegenerateLOD(USkeletalMesh* SkeletalMesh, int32 NewLODCount = 0, bool bRegenerateEvenIfImported = false);
};
