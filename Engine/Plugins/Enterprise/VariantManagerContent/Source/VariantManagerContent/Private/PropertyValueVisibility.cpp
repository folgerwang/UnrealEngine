// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PropertyValueVisibility.h"

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "HAL/UnrealMemory.h"
#include "VariantObjectBinding.h"

#define LOCTEXT_NAMESPACE "PropertyValueVisibility"


UPropertyValueVisibility::UPropertyValueVisibility(const FObjectInitializer& Init)
	: Super(Init)
{
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

	const bool bVisibilityValue = *(ValueBytes.GetData()) != 0;
	ContainerObject->SetVisibility(bVisibilityValue, true);

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
