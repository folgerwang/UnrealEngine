// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PropertyValueMaterial.h"

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "HAL/UnrealMemory.h"
#include "VariantObjectBinding.h"

#define LOCTEXT_NAMESPACE "PropertyValueMaterial"


UPropertyValueMaterial::UPropertyValueMaterial(const FObjectInitializer& Init)
	: Super(Init)
{
}

bool UPropertyValueMaterial::Resolve()
{
	UObject* Object = nullptr;
	UVariantObjectBinding* Parent = GetParent();
	if (Parent)
	{
		Object = Parent->GetObject();
	}

	if (Object == nullptr)
	{
		return false;
	}

	// Remove the last couple of links in the chain because they might not resolve
	// These will be UArrayProperty OverrideMaterials and its Inner
	UProperty* OverrideInner = Properties.Pop();
	UProperty* OverrideOuter = Properties.Pop();
	int32 OverrideInnerIndex = PropertyIndices.Pop();
	int32 OverrideOuterIndex = PropertyIndices.Pop();

	if (!ResolvePropertiesRecursive(Object->GetClass(), Object, 0))
	{
		UE_LOG(LogVariantContent, Error, TEXT("Failed to resolve UPropertyValueMaterial '%s' on UObject '%s'"), *GetFullDisplayString(), *Object->GetName());
		return false;
	}

	Properties.Add(OverrideOuter);
	Properties.Add(OverrideInner);
	PropertyIndices.Add(OverrideOuterIndex);
	PropertyIndices.Add(OverrideInnerIndex);

	if (PropertyValuePtr == nullptr)
	{
		UE_LOG(LogVariantContent, Error, TEXT("UPropertyValueMaterial '%s' does not target a valid UStaticMeshComponent!"), *GetFullDisplayString());
		return false;
	}

	ParentContainerAddress = *((UObject**)PropertyValuePtr);
	ParentContainerClass = ((UObject*)ParentContainerAddress)->GetClass();

	// We don't want anything trying to access this property by itself
	PropertyValuePtr = nullptr;
	LeafProperty = Properties.Num() > 0 ? Properties[Properties.Num()-1] : nullptr;
	return true;
}

void UPropertyValueMaterial::RecordDataFromResolvedObject()
{
	if (!Resolve())
	{
		return;
	}

	int32 PropertySizeBytes = GetValueSizeInBytes();

	UMeshComponent* ContainerObject = (UMeshComponent*) ParentContainerAddress;
	if (!ContainerObject)
	{
		UE_LOG(LogVariantContent, Error, TEXT("UPropertyValueMaterial '%s' does not have a UMeshComponent as parent address!"), *GetFullDisplayString());
		return;
	}

	int32 NumIndices = PropertyIndices.Num();
	if (NumIndices < 1)
	{
		return;
	}

	int32 MatIndex = PropertyIndices[NumIndices-1];
	UMaterialInterface* Mat = ContainerObject->GetMaterial(MatIndex);

	if (Mat && Mat->IsValidLowLevel())
	{
		SetRecordedData((uint8*)&Mat, PropertySizeBytes);
	}

	OnPropertyRecorded.Broadcast();
}

void UPropertyValueMaterial::ApplyDataToResolvedObject()
{
	if (!HasRecordedData() || !Resolve())
	{
		return;
	}

	// Ready to transact
	UObject* ContainerOwnerObject = nullptr;
	UVariantObjectBinding* Parent = GetParent();
	if (Parent)
	{
		ContainerOwnerObject = Parent->GetObject();
		if (ContainerOwnerObject)
		{
			ContainerOwnerObject->SetFlags(RF_Transactional);
			ContainerOwnerObject->Modify();
		}
	}

	UMeshComponent* ContainerObject = (UMeshComponent*) ParentContainerAddress;
	if (!ContainerObject)
	{
		UE_LOG(LogVariantContent, Error, TEXT("UPropertyValueMaterial '%s' does not have a UMeshComponent as parent address!"), *GetFullDisplayString());
		return;
	}

	ContainerObject->SetFlags(RF_Transactional);
	ContainerObject->Modify();

	UMaterialInterface* Mat = *((UMaterialInterface**)GetRecordedData().GetData());

	int32 NumIndices = PropertyIndices.Num();
	if (Mat && Mat->IsValidLowLevel() && NumIndices > 0)
	{
		int32 MatIndex = PropertyIndices[NumIndices-1];
		ContainerObject->SetMaterial(MatIndex, Mat);
	}

	// Update object on viewport
#if WITH_EDITOR
	ContainerObject->PostEditChange();
	if (ContainerOwnerObject)
	{
		ContainerOwnerObject->PostEditChange();
	}
#endif

	OnPropertyApplied.Broadcast();
}

#undef LOCTEXT_NAMESPACE
