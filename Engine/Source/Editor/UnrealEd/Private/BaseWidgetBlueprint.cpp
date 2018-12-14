// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "BaseWidgetBlueprint.h"
#include "Components/Widget.h"
#include "Blueprint/WidgetTree.h"
#include "UObject/UObjectHash.h"

UBaseWidgetBlueprint::UBaseWidgetBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WidgetTree = CreateDefaultSubobject<UWidgetTree>(TEXT("WidgetTree"));
	WidgetTree->SetFlags(RF_Transactional);
}

TArray<UWidget*> UBaseWidgetBlueprint::GetAllSourceWidgets()
{
	TArray<UWidget*> Ret;
	ForEachSourceWidgetImpl([&Ret](UWidget* Inner) { Ret.Push(Inner); });
	return Ret;
}

TArray<const UWidget*> UBaseWidgetBlueprint::GetAllSourceWidgets() const
{
	TArray<const UWidget*> Ret;
	ForEachSourceWidgetImpl([&Ret](UWidget* Inner) { Ret.Push(Inner); });
	return Ret;
}

void UBaseWidgetBlueprint::ForEachSourceWidget(TFunctionRef<void(UWidget*)> Fn)
{
	ForEachSourceWidgetImpl(Fn);
}

void UBaseWidgetBlueprint::ForEachSourceWidget(TFunctionRef<void(UWidget*)> Fn) const
{
	ForEachSourceWidgetImpl(Fn);
}

void UBaseWidgetBlueprint::ForEachSourceWidgetImpl(TFunctionRef<void(UWidget*)> Fn) const
{
	// This exists in order to facilitate working with collections of UWidgets wo/ 
	// relying on user implemented UWidget virtual functions. During blueprint compilation
	// it is bad practice to call those virtual functions until the class is fully formed
	// and reinstancing has finished. For instance, GetDefaultObject() calls in those user
	// functions may create a CDO before the class has been linked, or even before
	// all member variables have been generated:
	UWidgetTree* WidgetTreeForCapture = WidgetTree;
	ForEachObjectWithOuter(
		WidgetTree,
		[Fn, WidgetTreeForCapture](UObject* Inner)
	{
		if (UWidget* AsWidget = Cast<UWidget>(Inner))
		{
			// Widgets owned by another UWidgetBlueprint aren't really 'source' widgets, E.g. widgets
			// created by the user *in this blueprint*
			if (AsWidget->GetTypedOuter<UWidgetTree>() == WidgetTreeForCapture)
			{
				Fn(AsWidget);
			}
		}
	}
	);
}
