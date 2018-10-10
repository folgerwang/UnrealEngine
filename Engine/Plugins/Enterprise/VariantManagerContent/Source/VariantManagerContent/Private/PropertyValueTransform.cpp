// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PropertyValueTransform.h"

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "HAL/UnrealMemory.h"
#include "VariantObjectBinding.h"

#define LOCTEXT_NAMESPACE "PropertyValueTransform"


UPropertyValueTransform::UPropertyValueTransform(const FObjectInitializer& Init)
	: Super(Init)
{
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
