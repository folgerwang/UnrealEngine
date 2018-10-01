// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "VariantObjectBinding.h"

#include "UObject/LazyObjectPtr.h"
#include "PropertyValue.h"
#include "Variant.h"

#define LOCTEXT_NAMESPACE "VariantObjectBinding"


UVariantObjectBinding::UVariantObjectBinding(const FObjectInitializer& Init)
{
}

void UVariantObjectBinding::Init(UObject* InObject)
{
	ObjectPtr = InObject;
	LazyObjectPtr = InObject;
}

UVariant* UVariantObjectBinding::GetParent()
{
	return Cast<UVariant>(GetOuter());
}

FText UVariantObjectBinding::GetDisplayText() const
{
	UObject* Obj = GetObject();
	if (Obj)
	{
		return FText::FromName(Obj->GetFName());
	}
	else
	{
		return FText::FromString(TEXT("<Unloaded binding>"));
	}
}

FString UVariantObjectBinding::GetObjectPath() const
{
	return ObjectPtr.ToString();
}

UObject* UVariantObjectBinding::GetObject() const
{
	if (ObjectPtr.IsValid())
	{
		UObject* Obj = ObjectPtr.ResolveObject();
		if (Obj)
		{
			return Obj;
		}

		// Last resort: When an actor switches levels (e.g. new level -> create variant manager ->
		// -> add one of the temp actors -> save level) the FSoftObjectPath might lose track of it
		// We will then use our TLazyObjectPtr to try and find the object since it uses FGuids and
		// it might still have a valid link
		if (!Obj)
		{
			UObject* LazyObject = LazyObjectPtr.Get();
			if (LazyObject)
			{
				UE_LOG(LogVariantContent, Log, TEXT("Actor '%s' switched path. Binding updating from path '%s' to '%s'"), *LazyObject->GetName(), *ObjectPtr.ToString(), *LazyObject->GetFullName());
				ObjectPtr = LazyObject;
			}
		}
	}

	return nullptr;
}

void UVariantObjectBinding::AddCapturedProperties(const TArray<UPropertyValue*>& NewProperties, int32 Index)
{
	Modify();

	if (Index == INDEX_NONE)
	{
		Index = CapturedProperties.Num();
	}

	// Inserting first ensures we preserve the target order
	CapturedProperties.Insert(NewProperties, Index);

	bool bIsMoveOperation = false;
	TSet<UVariantObjectBinding*> ParentsModified;
	for (UPropertyValue* NewProp : NewProperties)
	{
		UVariantObjectBinding* OldParent = NewProp->GetParent();

		// We can't just RemoveCapturedProperty since that might remove the wrong item in case
		// we're moving bindings around within this Variant
		if (OldParent)
		{
			if (OldParent != this)
			{
				// Don't call RemoveProperty here so that we get the entire thing in a single transaction
				if (!ParentsModified.Contains(OldParent))
				{
					OldParent->Modify();
					ParentsModified.Add(OldParent);
				}
				OldParent->CapturedProperties.RemoveSingle(NewProp);
			}
			else
			{
				bIsMoveOperation = true;
			}
		}

		NewProp->Modify();
		NewProp->Rename(nullptr, this);
	}

	// If it's a move operation, we'll have to manually clear the old pointers from the array
	if (!bIsMoveOperation)
	{
		return;
	}

	TSet<FString> NewPropertyPaths = TSet<FString>();
	for (UPropertyValue* NewProp : NewProperties)
	{
		NewPropertyPaths.Add(NewProp->GetFullDisplayString());
	}

	// Sweep back from insertion point nulling old bindings with the same GUID
	for (int32 SweepIndex = Index-1; SweepIndex >= 0; SweepIndex--)
	{
		if (NewPropertyPaths.Contains(CapturedProperties[SweepIndex]->GetFullDisplayString()))
		{
			CapturedProperties[SweepIndex] = nullptr;
		}
	}
	// Sweep forward from the end of the inserted segment nulling old bindings with the same GUID
	for (int32 SweepIndex = Index + NewProperties.Num(); SweepIndex < CapturedProperties.Num(); SweepIndex++)
	{
		if (NewPropertyPaths.Contains(CapturedProperties[SweepIndex]->GetFullDisplayString()))
		{
			CapturedProperties[SweepIndex] = nullptr;
		}
	}

	// Finally remove null entries
	for (int32 IterIndex = CapturedProperties.Num() - 1; IterIndex >= 0; IterIndex--)
	{
		if (CapturedProperties[IterIndex] == nullptr)
		{
			CapturedProperties.RemoveAt(IterIndex);
		}
	}
}

const TArray<UPropertyValue*>& UVariantObjectBinding::GetCapturedProperties() const
{
	return CapturedProperties;
}

void UVariantObjectBinding::RemoveCapturedProperties(const TArray<UPropertyValue*>& Properties)
{
	Modify();

	for (UPropertyValue* Prop : Properties)
	{
		CapturedProperties.RemoveSingle(Prop);
	}
}

#undef LOCTEXT_NAMESPACE
