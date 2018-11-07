// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "VariantSet.h"

#include "LevelVariantSets.h"
#include "Variant.h"
#include "CoreMinimal.h"

#define LOCTEXT_NAMESPACE "VariantManagerVariantSet"


UVariantSet::UVariantSet(const FObjectInitializer& Init)
{
	DisplayText = FText::FromString(TEXT("VariantSet"));
}

ULevelVariantSets* UVariantSet::GetParent()
{
	return Cast<ULevelVariantSets>(GetOuter());
}

void UVariantSet::SetDisplayText(const FText& NewDisplayText)
{
	DisplayText = NewDisplayText;
}

FText UVariantSet::GetDisplayText() const
{
	return DisplayText;
}

FString UVariantSet::GetUniqueVariantName(const FString& InPrefix)
{
	TSet<FString> UniqueNames;
	for (UVariant* Variant : Variants)
	{
		UniqueNames.Add(Variant->GetDisplayText().ToString());
	}

	FString VarName = FString(InPrefix);

	// Add a numbered suffix
	if (UniqueNames.Contains(VarName))
	{
		int32 Suffix = 0;
		while (UniqueNames.Contains(VarName + FString::FromInt(Suffix)))
		{
			Suffix += 1;
		}

		VarName = VarName + FString::FromInt(Suffix);
	}

	return VarName;
}

void UVariantSet::AddVariants(const TArray<UVariant*>& NewVariants, int32 Index)
{
	Modify();

	if (Index == INDEX_NONE)
	{
		Index = Variants.Num();
	}

	// Inserting first ensures we preserve the target order
	Variants.Insert(NewVariants, Index);

	bool bIsMoveOperation = false;
	TSet<UVariantSet*> ParentsModified;
	for (UVariant* NewVariant : NewVariants)
	{
		UVariantSet* OldParent = NewVariant->GetParent();

		// We can't just RemoveBinding since that might remove the wrong item in case
		// we're moving bindings around within this Variant
		if (OldParent)
		{
			if (OldParent != this)
			{
				// Don't call RemoveVariants here so that we get the entire thing in a single transaction
				if (!ParentsModified.Contains(OldParent))
				{
					OldParent->Modify();
					ParentsModified.Add(OldParent);
				}
				OldParent->Variants.RemoveSingle(NewVariant);
			}
			else
			{
				bIsMoveOperation = true;
			}
		}

		NewVariant->Modify();
		NewVariant->Rename(nullptr, this);  // We'll only actually rename them at the end of this function
	}

	// If it's a move operation, we'll have to manually clear the old pointers from the array
	if (bIsMoveOperation)
	{
		TSet<UVariant*> SetOfNewVariants = TSet<UVariant*>(NewVariants);

		// Sweep back from insertion point nulling old bindings with the same GUID
		for (int32 SweepIndex = Index-1; SweepIndex >= 0; SweepIndex--)
		{
			if (SetOfNewVariants.Contains(Variants[SweepIndex]))
			{
				Variants[SweepIndex] = nullptr;
			}
		}
		// Sweep forward from the end of the inserted segment nulling old bindings with the same GUID
		for (int32 SweepIndex = Index + NewVariants.Num(); SweepIndex < Variants.Num(); SweepIndex++)
		{
			if (SetOfNewVariants.Contains(Variants[SweepIndex]))
			{
				Variants[SweepIndex] = nullptr;
			}
		}

		// Finally remove null entries
		for (int32 IterIndex = Variants.Num() - 1; IterIndex >= 0; IterIndex--)
		{
			if (Variants[IterIndex] == nullptr)
			{
				Variants.RemoveAt(IterIndex);
			}
		}
	}

	// Go over new added variants and get them unique display names if they don't have them yet
	// Can only do this now that we know for sure we deleted the old versions, if we're moving
	for (UVariant* NewVar : NewVariants)
	{
		// Don't transact to keep all of this in a single transaction
		NewVar->SetDisplayText(FText::FromString(GetUniqueVariantName(NewVar->GetDisplayText().ToString())));
	}
}

const TArray<UVariant*>& UVariantSet::GetVariants() const
{
	return Variants;
}

void UVariantSet::RemoveVariants(const TArray<UVariant*>& InVariants)
{
	Modify();

	for (UVariant* Variant : InVariants)
	{
		Variants.RemoveSingle(Variant);
	}
}

int32 UVariantSet::GetNumVariants()
{
	return Variants.Num();
}

UVariant* UVariantSet::GetVariant(int32 VariantIndex)
{
	if (Variants.IsValidIndex(VariantIndex))
	{
		return Variants[VariantIndex];
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE