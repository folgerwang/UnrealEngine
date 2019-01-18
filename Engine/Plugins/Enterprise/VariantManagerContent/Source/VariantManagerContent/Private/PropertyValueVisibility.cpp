// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PropertyValueVisibility.h"

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "HAL/UnrealMemory.h"
#include "VariantObjectBinding.h"

#define LOCTEXT_NAMESPACE "PropertyValueVisibility"


UPropertyValueVisibility::UPropertyValueVisibility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPropertyValueVisibility::SetVisibility(bool bVisible)
{
	SetRecordedData((uint8*)&bVisible, sizeof(bool));
}

bool UPropertyValueVisibility::GetVisibility()
{
	if (!HasRecordedData())
	{
		return false;
	}

	uint8 Value = *(ValueBytes.GetData());
	return Value != 0;
}

void UPropertyValueVisibility::ApplyDataToResolvedObject()
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
		UE_LOG(LogVariantContent, Error, TEXT("UPropertyValueVisibility '%s' does not have a USceneComponent as parent address!"), *GetFullDisplayString());
		return;
	}

	ContainerObject->SetFlags(RF_Transactional);
	ContainerObject->Modify();

	uint8 Value = *(ValueBytes.GetData());
	ContainerObject->SetVisibility(Value != 0, true);

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
