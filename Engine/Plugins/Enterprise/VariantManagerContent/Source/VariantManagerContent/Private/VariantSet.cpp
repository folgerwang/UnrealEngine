// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "VariantSet.h"
#include "LevelVariantSets.h"
#include "Variant.h"

#include "CoreMinimal.h"

#define LOCTEXT_NAMESPACE "VariantManagerVariantSet"


UVariantSet::UVariantSet(const FObjectInitializer& Init)
{
}

UVariantSet* UVariantSet::Clone(UObject* ClonesOuter)
{
	if (ClonesOuter == INVALID_OBJECT)
	{
		ClonesOuter = GetOuter();
	}

	UVariantSet* NewVariantSet = DuplicateObject(this, GetOuter());

	for (UVariant* OurVariant : GetVariants())
	{
		NewVariantSet->AddVariant(OurVariant->Clone());
	}
	NewVariantSet->SetSortingOrder(GetSortingOrder() + 1);

	return NewVariantSet;
}

ULevelVariantSets* UVariantSet::GetParent()
{
	return Cast<ULevelVariantSets>(GetOuter());
}

FText UVariantSet::GetDisplayName() const
{
	if (DisplayName.IsEmpty())
	{
		return GetDefaultDisplayName();
	}

	return DisplayName;
}

void UVariantSet::SetDisplayName(const FText& NewDisplayName)
{
	if (NewDisplayName.EqualTo(DisplayName))
	{
		return;
	}

	SetFlags(RF_Transactional);
	Modify();

	DisplayName = NewDisplayName;
}

FText UVariantSet::GetDefaultDisplayName() const
{
	return LOCTEXT("UnnamedVariantSetName", "VariantSet");
}

void UVariantSet::AddVariant(UVariant* NewVariant)
{
	NewVariant->Rename(*NewVariant->GetName(), this);

	Variants.Add(NewVariant);
}

void UVariantSet::RemoveVariant(UVariant* ThisVariant)
{
	Variants.RemoveSingle(ThisVariant);
}

#undef LOCTEXT_NAMESPACE