// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Variant.h"

#include "PropertyValue.h"
#include "VariantSet.h"
#include "VariantObjectBinding.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "Variant"


UVariant::UVariant(const FObjectInitializer& Init)
{
	DisplayText = FText::FromString(TEXT("Variant"));
}

UVariantSet* UVariant::GetParent()
{
	return Cast<UVariantSet>(GetOuter());
}

void UVariant::SetDisplayText(const FText& NewDisplayText)
{
	DisplayText = NewDisplayText;
}

FText UVariant::GetDisplayText() const
{
	return DisplayText;
}

void UVariant::AddBindings(const TArray<UVariantObjectBinding*>& NewBindings, int32 Index)
{
	Modify();

	if (Index == INDEX_NONE)
	{
		Index = ObjectBindings.Num();
	}

	// Inserting first ensures we preserve the target order
	ObjectBindings.Insert(NewBindings, Index);

	bool bIsMoveOperation = false;
	TSet<UVariant*> ParentsModified;
	for (UVariantObjectBinding* NewBinding : NewBindings)
	{
		UVariant* OldParent = NewBinding->GetParent();

		// We can't just RemoveBinding since that might remove the wrong item in case
		// we're moving bindings around within this Variant
		if (OldParent)
		{
			if (OldParent != this)
			{
				// Don't call RemoveBinding here so that we get the entire thing in a single transaction
				if (!ParentsModified.Contains(OldParent))
				{
					OldParent->Modify();
					ParentsModified.Add(OldParent);
				}
				OldParent->ObjectBindings.RemoveSingle(NewBinding);
			}
			else
			{
				bIsMoveOperation = true;
			}
		}

		NewBinding->Modify();
		NewBinding->Rename(nullptr, this);
	}

	// If it's a move operation, we'll have to manually clear the old pointers from the array
	if (!bIsMoveOperation)
	{
		return;
	}

	TSet<FString> NewBindingPaths = TSet<FString>();
	for (UVariantObjectBinding* NewBinding : NewBindings)
	{
		NewBindingPaths.Add(NewBinding->GetObjectPath());
	}

	// Sweep back from insertion point nulling old bindings with the same GUID
	for (int32 SweepIndex = Index-1; SweepIndex >= 0; SweepIndex--)
	{
		if (NewBindingPaths.Contains(ObjectBindings[SweepIndex]->GetObjectPath()))
		{
			ObjectBindings[SweepIndex] = nullptr;
		}
	}
	// Sweep forward from the end of the inserted segment nulling old bindings with the same GUID
	for (int32 SweepIndex = Index + NewBindings.Num(); SweepIndex < ObjectBindings.Num(); SweepIndex++)
	{
		if (NewBindingPaths.Contains(ObjectBindings[SweepIndex]->GetObjectPath()))
		{
			ObjectBindings[SweepIndex] = nullptr;
		}
	}

	// Finally remove null entries
	for (int32 IterIndex = ObjectBindings.Num() - 1; IterIndex >= 0; IterIndex--)
	{
		if (ObjectBindings[IterIndex] == nullptr)
		{
			ObjectBindings.RemoveAt(IterIndex);
		}
	}
}

const TArray<UVariantObjectBinding*>& UVariant::GetBindings()
{
	return ObjectBindings;
}

void UVariant::RemoveBindings(const TArray<UVariantObjectBinding*>& Bindings)
{
	Modify();

	for (UVariantObjectBinding* Binding : Bindings)
	{
		ObjectBindings.RemoveSingle(Binding);
	}
}

int32 UVariant::GetNumActors()
{
	return ObjectBindings.Num();
}

AActor* UVariant::GetActor(int32 ActorIndex)
{
	if (ObjectBindings.IsValidIndex(ActorIndex))
	{
		UVariantObjectBinding* Binding = ObjectBindings[ActorIndex];
		UObject* Obj = Binding->GetObject();
		if (AActor* Actor = Cast<AActor>(Obj))
		{
			return Actor;
		}
	}

	return nullptr;
}

void UVariant::SwitchOn()
{
	for (UVariantObjectBinding* Binding : ObjectBindings)
	{
		for (UPropertyValue* PropCapture : Binding->GetCapturedProperties())
		{
			PropCapture->ApplyDataToResolvedObject();
		}
	}
}

#undef LOCTEXT_NAMESPACE