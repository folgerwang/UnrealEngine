// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VariantSet.h"

#include "LevelVariantSets.h"
#include "Variant.h"
#include "CoreMinimal.h"
#include "VariantManagerObjectVersion.h"

#define LOCTEXT_NAMESPACE "VariantManagerVariantSet"


UVariantSet::UVariantSet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayText = FText::FromString(TEXT("Variant Set"));
	bExpanded = true;
}

ULevelVariantSets* UVariantSet::GetParent()
{
	return Cast<ULevelVariantSets>(GetOuter());
}

void UVariantSet::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FVariantManagerObjectVersion::GUID);
	int32 CustomVersion = Ar.CustomVer(FVariantManagerObjectVersion::GUID);

	if (CustomVersion < FVariantManagerObjectVersion::CategoryFlagsAndManualDisplayText)
	{
		// Recover name from back when it was an UPROPERTY
		if (Ar.IsLoading())
		{
			if (!DisplayText_DEPRECATED.IsEmpty())
			{
				DisplayText = DisplayText_DEPRECATED;
				DisplayText_DEPRECATED = FText();
			}
		}
	}
	else
	{
		Ar << DisplayText;
	}
}

bool UVariantSet::IsExpanded()
{
	return bExpanded;
}

void UVariantSet::SetExpanded(bool bInExpanded)
{
	bExpanded = bInExpanded;
}

void UVariantSet::SetDisplayText(const FText& NewDisplayText)
{
	Modify();

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

	if (!UniqueNames.Contains(InPrefix))
	{
		return InPrefix;
	}

	FString VarName = FString(InPrefix);

	// Remove potentially existing suffix numbers
	FString LastChar = VarName.Right(1);
	while (LastChar.IsNumeric())
	{
		VarName = VarName.LeftChop(1);
		LastChar = VarName.Right(1);
	}

	// Add a numbered suffix
	if (UniqueNames.Contains(VarName) || VarName.IsEmpty())
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

	TSet<FString> OldNames;
	for (UVariant* Var : Variants)
	{
		OldNames.Add(Var->GetDisplayText().ToString());
	}

	// Inserting first ensures we preserve the target order
	Variants.Insert(NewVariants, Index);

	bool bIsMoveOperation = false;
	TSet<UVariantSet*> ParentsModified;
	for (UVariant* NewVariant : NewVariants)
	{
		UVariantSet* OldParent = NewVariant->GetParent();

		// We can't just RemoveBinding since that might remove the wrong item
		if (OldParent)
		{
			if (OldParent != this)
			{
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
		NewVariant->Rename(nullptr, this, REN_DontCreateRedirectors);  // Change parents

		// Update name if we're from a different parent but our names collide
		FString IncomingName = NewVariant->GetDisplayText().ToString();
		if (OldParent != this && OldNames.Contains(IncomingName))
		{
			NewVariant->SetDisplayText(FText::FromString(GetUniqueVariantName(IncomingName)));
		}
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
}

int32 UVariantSet::GetVariantIndex(UVariant* Var)
{
	return Variants.Find(Var);
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

UVariant* UVariantSet::GetVariantByName(FString VariantName)
{
	UVariant** VarPtr = Variants.FindByPredicate([VariantName](const UVariant* Var)
	{
		return Var->GetDisplayText().ToString() == VariantName;
	});

	if (VarPtr)
	{
		return *VarPtr;
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE