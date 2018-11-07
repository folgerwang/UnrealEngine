// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LevelVariantSets.h"

#include "VariantSet.h"

#define LOCTEXT_NAMESPACE "LevelVariantSets"


ULevelVariantSets::ULevelVariantSets(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULevelVariantSets::AddVariantSets(const TArray<UVariantSet*>& NewVariantSets, int32 Index)
{
	Modify();

	if (Index == INDEX_NONE)
	{
		Index = VariantSets.Num();
	}

	// Inserting first ensures we preserve the target order
	VariantSets.Insert(NewVariantSets, Index);

	bool bIsMoveOperation = false;
	TSet<ULevelVariantSets*> ParentsModified;
	for (UVariantSet* NewVarSet : NewVariantSets)
	{
		ULevelVariantSets* OldParent = NewVarSet->GetParent();

		// We can't just RemoveBinding since that might remove the wrong item in case
		// we're moving bindings around within this Variant
		if (OldParent)
		{
			if (OldParent != this)
			{
				// Don't call RemoveVariantSets here so that we get the entire thing in a single transaction
				if (!ParentsModified.Contains(OldParent))
				{
					OldParent->Modify();
					ParentsModified.Add(OldParent);
				}
				OldParent->VariantSets.RemoveSingle(NewVarSet);
			}
			else
			{
				bIsMoveOperation = true;
			}
		}

		NewVarSet->Modify();
		NewVarSet->Rename(nullptr, this);  // We'll only actually rename them at the end of this function
	}

	// If it's a move operation, we'll have to manually clear the old pointers from the array
	if (bIsMoveOperation)
	{
		TSet<UVariantSet*> SetOfNewVariantSets = TSet<UVariantSet*>(NewVariantSets);

		// Sweep back from insertion point nulling old bindings with the same GUID
		for (int32 SweepIndex = Index-1; SweepIndex >= 0; SweepIndex--)
		{
			if (SetOfNewVariantSets.Contains(VariantSets[SweepIndex]))
			{
				VariantSets[SweepIndex] = nullptr;
			}
		}
		// Sweep forward from the end of the inserted segment nulling old bindings with the same GUID
		for (int32 SweepIndex = Index + NewVariantSets.Num(); SweepIndex < VariantSets.Num(); SweepIndex++)
		{
			if (SetOfNewVariantSets.Contains(VariantSets[SweepIndex]))
			{
				VariantSets[SweepIndex] = nullptr;
			}
		}

		// Finally remove null entries
		for (int32 IterIndex = VariantSets.Num() - 1; IterIndex >= 0; IterIndex--)
		{
			if (VariantSets[IterIndex] == nullptr)
			{
				VariantSets.RemoveAt(IterIndex);
			}
		}
	}

	// Go over new added variants and get them unique display names if they don't have them yet
	// Can only do this now that we know for sure we deleted the old versions, if we're moving
	for (UVariantSet* NewVarSet : NewVariantSets)
	{
		// Don't transact to keep all of this in a single transaction
		NewVarSet->SetDisplayText(FText::FromString(GetUniqueVariantSetName(NewVarSet->GetDisplayText().ToString())));
	}
}

const TArray<UVariantSet*>& ULevelVariantSets::GetVariantSets() const
{
	return VariantSets;
}

void ULevelVariantSets::RemoveVariantSets(const TArray<UVariantSet*> InVariantSets)
{
	Modify();

	for (UVariantSet* VariantSet : InVariantSets)
	{
		VariantSets.RemoveSingle(VariantSet);
	}
}

FString ULevelVariantSets::GetUniqueVariantSetName(const FString& InPrefix)
{
	TSet<FString> UniqueNames;
	for (UVariantSet* VariantSet : VariantSets)
	{
		UniqueNames.Add(VariantSet->GetDisplayText().ToString());
	}

	FString VarSetName = FString(InPrefix);

	// Add a numbered suffix
	if (UniqueNames.Contains(VarSetName))
	{
		int32 Suffix = 0;
		while (UniqueNames.Contains(VarSetName + FString::FromInt(Suffix)))
		{
			Suffix += 1;
		}

		VarSetName = VarSetName + FString::FromInt(Suffix);
	}

	return VarSetName;
}

void ULevelVariantSets::SaveExpansionState(UVariantSet* VarSetOfNode, bool bExpanded)
{
	DisplayNodeExpansionStates.Add(VarSetOfNode, bExpanded);
}

bool ULevelVariantSets::GetExpansionState(UVariantSet* VarSetOfNode)
{
	if (DisplayNodeExpansionStates.Contains(VarSetOfNode))
	{
		return DisplayNodeExpansionStates[VarSetOfNode];
	}
	return false;
}

int32 ULevelVariantSets::GetNumVariantSets()
{
	return VariantSets.Num();
}

UVariantSet* ULevelVariantSets::GetVariantSet(int32 VariantSetIndex)
{
	if (VariantSets.IsValidIndex(VariantSetIndex))
	{
		return VariantSets[VariantSetIndex];
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
