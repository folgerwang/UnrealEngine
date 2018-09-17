// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Variant.h"
#include "VariantSet.h"
#include "VariantObjectBinding.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "VariantManagerVariant"


UVariant::UVariant(const FObjectInitializer& Init)
{
}

UVariant* UVariant::Clone(UObject* ClonesOuter)
{
	if (ClonesOuter == INVALID_OBJECT)
	{
		ClonesOuter = GetOuter();
	}

	UVariant* NewVariant = DuplicateObject(this, ClonesOuter);

	for (UVariantObjectBinding* OurBinding : GetBindings())
	{
		NewVariant->AddBinding(OurBinding->Clone());
	}
	NewVariant->SetSortingOrder(GetSortingOrder() + 1);

	return NewVariant;
}

UVariantSet* UVariant::GetParent()
{
	return Cast<UVariantSet>(GetOuter());
}

FText UVariant::GetDisplayName() const
{
	if (DisplayName.IsEmpty())
	{
		return GetDefaultDisplayName();
	}

	return DisplayName;
}

void UVariant::SetDisplayName(const FText& NewDisplayName)
{
	if (NewDisplayName.EqualTo(DisplayName))
	{
		return;
	}

	SetFlags(RF_Transactional);
	Modify();

	DisplayName = NewDisplayName;
}

FText UVariant::GetDefaultDisplayName() const
{
	return LOCTEXT("UnnamedVariantName", "Variant");
}

void UVariant::AddBinding(UVariantObjectBinding* NewBinding)
{
	NewBinding->Rename(*NewBinding->GetName(), this);

	ObjectBindings.Add(NewBinding);
}

void UVariant::AddActors(TWeakObjectPtr<AActor> InActor)
{
	UVariantObjectBinding* NewBinding = NewObject<UVariantObjectBinding>(this);
	NewBinding->Init(InActor.Get());

	FGuid NewGuid = NewBinding->GetObjectGuid();
	for (UVariantObjectBinding* Binding : ObjectBindings)
	{
		if (Binding->GetObjectGuid() == NewGuid)
		{
			return;
		}
	}

	AddBinding(NewBinding);
}

void UVariant::AddActors(const TArray<TWeakObjectPtr<AActor>>& InActors)
{
	for (TWeakObjectPtr<AActor> InActor : InActors)
	{
		UVariantObjectBinding* NewBinding = NewObject<UVariantObjectBinding>(this);
		NewBinding->Init(InActor.Get());

		FGuid NewGuid = NewBinding->GetObjectGuid();

		bool bDuplicate = false;
		for (UVariantObjectBinding* Binding : ObjectBindings)
		{
			if (Binding->GetObjectGuid() == NewGuid)
			{
				bDuplicate = true;
				break;
			}
		}

		if (!bDuplicate)
		{
			AddBinding(NewBinding);
		}
	}
}

void UVariant::RemoveBinding(UVariantObjectBinding* ThisBinding)
{
	ObjectBindings.RemoveSingle(ThisBinding);
}

#undef LOCTEXT_NAMESPACE