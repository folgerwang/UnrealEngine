// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "USDImportOptions.h"
#include "UObject/UnrealType.h"

UUSDImportOptions::UUSDImportOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MeshImportType = EUsdMeshImportType::StaticMesh;
	bApplyWorldTransformToGeometry = true;
	Scale = 1.0;
}

void UUSDImportOptions::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		SaveConfig();
	}
}

UUSDSceneImportOptions::UUSDSceneImportOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bFlattenHierarchy = true;
	bImportMeshes = true;
	PathForAssets.Path = TEXT("/Game");
	bGenerateUniqueMeshes = true;
	bGenerateUniquePathPerUSDPrim = true;
	bApplyWorldTransformToGeometry = false;
}

void UUSDSceneImportOptions::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool UUSDSceneImportOptions::CanEditChange(const UProperty* InProperty) const
{
	bool bCanEdit = Super::CanEditChange(InProperty);

	FName PropertyName = InProperty ? InProperty->GetFName() : NAME_None;

	if (GET_MEMBER_NAME_CHECKED(UUSDImportOptions, MeshImportType) == PropertyName ||
		GET_MEMBER_NAME_CHECKED(UUSDImportOptions, bApplyWorldTransformToGeometry) == PropertyName || 
		GET_MEMBER_NAME_CHECKED(UUSDImportOptions, bGenerateUniquePathPerUSDPrim) == PropertyName)
	{
		bCanEdit &= bImportMeshes;
	}

	return bCanEdit;
}

UUSDBatchImportOptions::UUSDBatchImportOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bImportMeshes = true;
	PathForAssets.Path = TEXT("/Game");
	bGenerateUniqueMeshes = true;
	bGenerateUniquePathPerUSDPrim = true;
	bApplyWorldTransformToGeometry = false;
}

void UUSDBatchImportOptions::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool UUSDBatchImportOptions::CanEditChange(const UProperty* InProperty) const
{
	bool bCanEdit = Super::CanEditChange(InProperty);

	FName PropertyName = InProperty ? InProperty->GetFName() : NAME_None;

	if (GET_MEMBER_NAME_CHECKED(UUSDImportOptions, MeshImportType) == PropertyName ||
		GET_MEMBER_NAME_CHECKED(UUSDImportOptions, bApplyWorldTransformToGeometry) == PropertyName || 
		GET_MEMBER_NAME_CHECKED(UUSDImportOptions, bGenerateUniquePathPerUSDPrim) == PropertyName)
	{
		bCanEdit &= bImportMeshes;
	}

	return bCanEdit;
}

UUSDBatchImportOptionsSubTask::UUSDBatchImportOptionsSubTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}