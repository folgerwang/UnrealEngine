// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PropertyValueTransform.h"

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "HAL/UnrealMemory.h"
#include "VariantObjectBinding.h"

#define LOCTEXT_NAMESPACE "PropertyValueTransform"


UPropertyValueTransform::UPropertyValueTransform(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FVector UPropertyValueTransform::GetLocation()
{
	if (HasRecordedData() && PropCategory == EPropertyValueCategory::RelativeLocation)
	{
		FVector Value = *(FVector*)ValueBytes.GetData();
		return Value;
	}
	else
	{
		return FVector();
	}
}

FQuat UPropertyValueTransform::GetRotation()
{
	if (HasRecordedData() && PropCategory == EPropertyValueCategory::RelativeRotation)
	{
		FRotator Value = *(FRotator*)(ValueBytes.GetData());
		return Value.Quaternion();
	}
	else
	{
		return FQuat();
	}
}

FVector UPropertyValueTransform::GetScale3D()
{
	if (HasRecordedData() && PropCategory == EPropertyValueCategory::RelativeScale3D)
	{
		FVector Value = *(FVector*)ValueBytes.GetData();
		return Value;
	}
	else
	{
		return FVector();
	}
}

void UPropertyValueTransform::SetLocation(const FVector& NewValue)
{
	if (PropCategory == EPropertyValueCategory::RelativeLocation)
	{
		SetRecordedData((uint8*)&NewValue, sizeof(FVector));
	}
}

void UPropertyValueTransform::SetRotation(const FQuat& NewValue)
{
	if (PropCategory == EPropertyValueCategory::RelativeRotation)
	{
		FRotator Rot = NewValue.Rotator();
		SetRecordedData((uint8*)&Rot, sizeof(FRotator));
	}
}

void UPropertyValueTransform::SetScale3D(const FVector& NewValue)
{
	if (PropCategory == EPropertyValueCategory::RelativeScale3D)
	{
		SetRecordedData((uint8*)&NewValue, sizeof(FVector));
	}
}

void UPropertyValueTransform::ApplyDataToResolvedObject()
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

	USceneComponent* ContainerObject = (USceneComponent*) ParentContainerAddress;
	if (!ContainerObject)
	{
		UE_LOG(LogVariantContent, Error, TEXT("UPropertyValueTransform '%s' does not have a USceneComponent as parent address!"), *GetFullDisplayString());
		return;
	}

	ContainerObject->SetFlags(RF_Transactional);
	ContainerObject->Modify();

	switch (PropCategory)
	{
	case EPropertyValueCategory::RelativeLocation:
	{
		FVector BytesAsVec = *(FVector*)ValueBytes.GetData();
		ContainerObject->SetRelativeLocation(BytesAsVec);
		break;
	}
	case EPropertyValueCategory::RelativeRotation:
	{
		FRotator BytesAsRot = *(FRotator*)ValueBytes.GetData();
		ContainerObject->SetRelativeRotation(BytesAsRot);
		break;
	}
	case EPropertyValueCategory::RelativeScale3D:
	{
		FVector BytesAsVec = *(FVector*)ValueBytes.GetData();
		ContainerObject->SetRelativeScale3D(BytesAsVec);
		break;
	}
	default:
		check(false);
		break;
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
