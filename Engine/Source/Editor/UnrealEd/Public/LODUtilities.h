// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/Commands/UIAction.h"

//////////////////////////////////////////////////////////////////////////
// FSkeletalMeshUpdateContext


struct FSkeletalMeshUpdateContext
{
	USkeletalMesh*				SkeletalMesh;
	TArray<UActorComponent*>	AssociatedComponents;

	FExecuteAction				OnLODChanged;
};

//////////////////////////////////////////////////////////////////////////
// FLODUtilities

class UNREALED_API FLODUtilities
{
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
	static bool RegenerateLOD(USkeletalMesh* SkeletalMesh, int32 NewLODCount = 0, bool bRegenerateEvenIfImported = false);

	/** Removes a particular LOD from the SkeletalMesh. 
	*
	* @param UpdateContext - The skeletal mesh and actor components to operate on.
	* @param DesiredLOD   - The LOD index to remove the LOD from.
	*/
	static void RemoveLOD( FSkeletalMeshUpdateContext& UpdateContext, int32 DesiredLOD );

	/**
	*	Simplifies the static mesh based upon various user settings for DesiredLOD.
	*
	* @param UpdateContext - The skeletal mesh and actor components to operate on.
	* @param DesiredLOD - The LOD to simplify
	*/
	static void SimplifySkeletalMeshLOD(FSkeletalMeshUpdateContext& UpdateContext, int32 DesiredLOD, bool bReregisterComponent = true);

	/**
	 * Refresh LOD Change
	 * 
	 * LOD has changed, it will have to notify all SMC that uses this SM
	 * and ask them to refresh LOD
	 *
	 * @param	InSkeletalMesh	SkeletalMesh that LOD has been changed for
	 */
	static void RefreshLODChange(const USkeletalMesh* SkeletalMesh);

private:
	FLODUtilities() {}

	/**
	 *	Simplifies the static mesh based upon various user settings for DesiredLOD
	 *  This is private function that gets called by SimplifySkeletalMesh
	 *
	 * @param SkeletalMesh - The skeletal mesh and actor components to operate on.
	 * @param DesiredLOD - Desired LOD
	 */
	static void SimplifySkeletalMeshLOD(USkeletalMesh* SkeletalMesh, int32 DesiredLOD, bool bReregisterComponent = true);
	
	/**
	*  Remap the morph targets of the base LOD onto the desired LOD.
	*
	* @param SkeletalMesh - The skeletal mesh and actor components to operate on.
	* @param InSetting   - The user settings passed from the Simplification tool.
	* @param DesiredLOD - Desired LOD
	*/
	static void ApplyMorphTargetsToLOD(USkeletalMesh* SkeletalMesh, const FSkeletalMeshOptimizationSettings& InSetting, int32 DesiredLOD);

	/**
	*  Clear generated morphtargets for the given LODs
	*
	* @param SkeletalMesh - The skeletal mesh and actor components to operate on.
	* @param DesiredLOD - Desired LOD
	*/
	static void ClearGeneratedMorphTarget(USkeletalMesh* SkeletalMesh, int32 DesiredLOD);
};
