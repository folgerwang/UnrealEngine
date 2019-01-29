// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Components/MeshComponent.h"
#include "OculusMR_BoundaryMeshComponent.generated.h"

class FPrimitiveSceneProxy;

UENUM(BlueprintType)
enum class EOculusMR_BoundaryType : uint8
{
	BT_OuterBoundary    UMETA(DisplayName = "OuterBoundary"),
	BT_PlayArea         UMETA(DisplayName = "PlayArea"),
};


/** Component that allows you to specify custom triangle mesh geometry */
UCLASS(hidecategories = (Object, LOD, Physics, Collision), editinlinenew, ClassGroup = Rendering, NotPlaceable, NotBlueprintable)
class UOculusMR_BoundaryMeshComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
	EOculusMR_BoundaryType BoundaryType;

	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
	float BottomZ;

	UPROPERTY(Category = OculusMR, EditAnywhere, BlueprintReadWrite)
	float TopZ;

	UPROPERTY()
	class UMaterial* WhiteMaterial;

	UPROPERTY()
	class AOculusMR_CastingCameraActor* CastingCameraActor;

	bool IsValid() const { return bIsValid; }

private:
	bool bIsValid;

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const;
	virtual int32 GetNumMaterials() const override;
	//~ End UMeshComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ Begin USceneComponent Interface.

	friend class FOculusMR_BoundaryMeshSceneProxy;
};


