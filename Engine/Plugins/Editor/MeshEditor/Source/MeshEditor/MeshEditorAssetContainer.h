// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/DataAsset.h"
#include "MeshEditorAssetContainer.generated.h"

/**
 * Asset container for the mesh editor
 */
UCLASS()
class MESHEDITOR_API UMeshEditorAssetContainer : public UDataAsset
{
    GENERATED_BODY()
 
public:
	UPROPERTY(EditAnywhere, Category = Material)
	class UMaterial* HoveredGeometryMaterial;

	UPROPERTY(EditAnywhere, Category = Material)
	class UMaterial* HoveredFaceMaterial;

	UPROPERTY(EditAnywhere, Category = Sound)
	class USoundBase* DefaultSound;
};