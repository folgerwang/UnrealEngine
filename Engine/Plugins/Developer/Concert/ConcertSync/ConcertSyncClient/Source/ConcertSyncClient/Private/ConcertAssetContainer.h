// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "ConcertAssetContainer.generated.h"


// Forward declarations
class USoundBase;
class USoundCue;
class UStaticMesh;
class UMaterial;
class UMaterialInterface;
class UMaterialInstance;
class UFont;

/**
 * Asset container for VREditor.
 */
UCLASS()
class CONCERTSYNCCLIENT_API UConcertAssetContainer : public UDataAsset
{
	GENERATED_BODY()

public:

	//
	// Meshes
	//

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* GenericDesktopMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* GenericHMDMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* VivePreControllerMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* OculusControllerMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* GenericControllerMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* LaserPointerMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* LaserPointerEndMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* LaserPointerStartMesh;

	//
	// Materials
	//

	UPROPERTY(EditAnywhere, Category = Material)
	UMaterialInterface* PresenceMaterial;

	UPROPERTY(EditAnywhere, Category = Material)
	UMaterialInterface* TextMaterial;

	UPROPERTY(EditAnywhere, Category = Desktop)
	UMaterialInterface* HeadMaterial;

	UPROPERTY(EditAnywhere, Category = Material)
	UMaterialInterface* LaserCoreMaterial;

	UPROPERTY(EditAnywhere, Category = Material)
	UMaterialInterface* LaserMaterial;

	UPROPERTY(EditAnywhere, Category = Material)
	UMaterialInterface* PresenceFadeMaterial;
};
