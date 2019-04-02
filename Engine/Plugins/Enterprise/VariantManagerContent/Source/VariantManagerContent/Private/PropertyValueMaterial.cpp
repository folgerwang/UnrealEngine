// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PropertyValueMaterial.h"

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "HAL/UnrealMemory.h"
#include "VariantObjectBinding.h"

#define LOCTEXT_NAMESPACE "PropertyValueMaterial"


UPropertyValueMaterial::UPropertyValueMaterial(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UMaterialInterface* UPropertyValueMaterial::GetMaterial()
{
	if (!HasRecordedData())
	{
		return nullptr;
	}

	return *(UMaterialInterface**)ValueBytes.GetData();
}

void UPropertyValueMaterial::SetMaterial(UMaterialInterface* Mat)
{
	if (Mat && Mat->IsValidLowLevel())
	{
		SetRecordedData((uint8*)&Mat, GetValueSizeInBytes());
	}
}

bool UPropertyValueMaterial::Resolve(UObject* Object)
{
	if (Object == nullptr)
	{
		UVariantObjectBinding* Parent = GetParent();
		if (Parent)
		{
			Object = Parent->GetObject();
		}
	}

	if (Object == nullptr)
	{
		return false;
	}

	if (CapturedPropSegments.Num() == 0)
	{
		return false;
	}

	// Remove an item so that we don't trip an early out in ResolvePropertiesRecursive below
	// (the if(SegmentIndex == CapturedPropSegments.Num() - 2) test). The point of this resolve
	// is just to get ParentContainerAddress pointing at the target UMeshComponent, as we
	// apply/record values by calling the respective functions instead
	FCapturedPropSegment OverrideInner = CapturedPropSegments.Pop();
	bool bResolveSucceeded = ResolvePropertiesRecursive(Object->GetClass(), Object, 0);
	CapturedPropSegments.Add(OverrideInner);

	if (!bResolveSucceeded)
	{
		//UE_LOG(LogVariantContent, Error, TEXT("Failed to resolve UPropertyValueMaterial '%s' on UObject '%s'"), *GetFullDisplayString(), *Object->GetName());
		return false;
	}

	// We don't want anything trying to access this property by itself
	PropertyValuePtr = nullptr;
	LeafProperty = nullptr;
	PropertySetter = nullptr;
	return true;
}

UStruct* UPropertyValueMaterial::GetPropertyParentContainerClass() const
{
	return UMeshComponent::StaticClass();
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

	int32 NumSegs = CapturedPropSegments.Num();
	if (NumSegs < 1)
	{
		return;
	}

	int32 MatIndex = CapturedPropSegments[NumSegs-1].PropertyIndex;
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

	// Go through GetRecordedData to resolve our path if we need to
	UMaterialInterface* Mat = *((UMaterialInterface**)GetRecordedData().GetData());

	int32 NumSegs = CapturedPropSegments.Num();
	if (Mat && Mat->IsValidLowLevel() && NumSegs > 0)
	{
		int32 MatIndex = CapturedPropSegments[NumSegs-1].PropertyIndex;
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

UClass* UPropertyValueMaterial::GetPropertyClass() const
{
	return UObjectProperty::StaticClass();
}

UClass* UPropertyValueMaterial::GetObjectPropertyObjectClass() const
{
	return UMaterialInterface::StaticClass();
}

int32 UPropertyValueMaterial::GetValueSizeInBytes() const
{
	return sizeof(UMaterialInterface*);
}

#undef LOCTEXT_NAMESPACE
