// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Variant.h"

#include "PropertyValue.h"
#include "VariantSet.h"
#include "VariantObjectBinding.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VariantManagerObjectVersion.h"

#define LOCTEXT_NAMESPACE "Variant"

UVariant::UVariant(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayText = FText::FromString(TEXT("Variant"));
}

UVariantSet* UVariant::GetParent()
{
	return Cast<UVariantSet>(GetOuter());
}

void UVariant::Serialize(FArchive& Ar)
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

void UVariant::SetDisplayText(const FText& NewDisplayText)
{
	Modify();

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

		if (OldParent)
		{
			if (OldParent != this)
			{
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
		NewBinding->Rename(nullptr, this, REN_DontCreateRedirectors);
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

	// Sweep back from insertion point nulling old bindings with the same path
	for (int32 SweepIndex = Index-1; SweepIndex >= 0; SweepIndex--)
	{
		if (NewBindingPaths.Contains(ObjectBindings[SweepIndex]->GetObjectPath()))
		{
			ObjectBindings[SweepIndex] = nullptr;
		}
	}
	// Sweep forward from the end of the inserted segment nulling old bindings with the same path
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

int32 UVariant::GetBindingIndex(UVariantObjectBinding* Binding)
{
	return ObjectBindings.Find(Binding);
}

const TArray<UVariantObjectBinding*>& UVariant::GetBindings() const
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

UVariantObjectBinding* UVariant::GetBindingByName(const FString& ActorName)
{
	UVariantObjectBinding** FoundBindingPtr = ObjectBindings.FindByPredicate([&ActorName](const UVariantObjectBinding* Binding)
	{
		UObject* ThisActor = Binding->GetObject();
		return ThisActor && ThisActor->GetName() == ActorName;
	});

	if (FoundBindingPtr)
	{
		return *FoundBindingPtr;
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

		Binding->ExecuteAllTargetFunctions();
	}
}

#undef LOCTEXT_NAMESPACE