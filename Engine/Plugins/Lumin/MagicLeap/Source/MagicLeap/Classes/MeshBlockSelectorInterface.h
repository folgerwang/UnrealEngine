// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/Engine.h"
#include "MeshTrackerTypes.h"
#include "MeshBlockSelectorInterface.generated.h"

UINTERFACE(BlueprintType, Blueprintable)
class MAGICLEAP_API UMeshBlockSelectorInterface : public UInterface
{
	GENERATED_BODY()
};

/** Interface to select blocks for a mesh request. */
class MAGICLEAP_API IMeshBlockSelectorInterface
{
	GENERATED_BODY()

public:
	/**
	 * Given the new mesh information, select the blocks you want to keep and the level of detail for each of those blocks. 
	 * @param NewMeshInfo Information on the latest mesh blocks available.
	 * @param RequestedMesh output array containing details of blocks for whom the mesh should be requested.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Meshing|MagicLeap")
	void SelectMeshBlocks(const FMLTrackingMeshInfo& NewMeshInfo, TArray<FMeshBlockRequest>& RequestedMesh);
};
