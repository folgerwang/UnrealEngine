// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
	class UMaterialInterface* HoveredGeometryMaterial;

	UPROPERTY(EditAnywhere, Category = Material)
	class UMaterialInterface* HoveredFaceMaterial;

	UPROPERTY(EditAnywhere, Category = Material)
	class UMaterialInterface* WireMaterial;

	UPROPERTY(EditAnywhere, Category = Material)
	class UMaterialInterface* SubdividedMeshWireMaterial;

	UPROPERTY(EditAnywhere, Category = Material)
	class UMaterialInterface* OverlayLineMaterial;

	UPROPERTY(EditAnywhere, Category = Material)
	class UMaterialInterface* OverlayPointMaterial;

	UPROPERTY(EditAnywhere, Category = Sound)
	class USoundBase* DefaultSound;
};