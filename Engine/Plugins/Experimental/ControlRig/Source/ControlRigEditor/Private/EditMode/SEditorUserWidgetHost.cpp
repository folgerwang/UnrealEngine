// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SEditorUserWidgetHost.h"

void SEditorUserWidgetHost::Construct(const FArguments& InArgs, UWorld* InWorld)
{
	check(InWorld);
	World = InWorld;
	UserWidget = nullptr;
	
	ChildSlot
	[
		SNullWidget::NullWidget
	];
}

void SEditorUserWidgetHost::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(UserWidget);
}

void SEditorUserWidgetHost::SetUserWidgetClass(TSubclassOf<UControlRigPickerWidget> InUserWidgetClass)
{
	TSharedPtr<SWidget> Widget = SNullWidget::NullWidget;
	// world is weak object ptr, if you want to make it strong, make sure it gets cleaned up 
	// before level shut down
	if(InUserWidgetClass.Get() != nullptr && World.IsValid())
	{
		UserWidget = CreateWidget<UControlRigPickerWidget>(World.Get(), InUserWidgetClass);
		Widget = UserWidget->TakeWidget();
	}
	else
	{
		UserWidget = nullptr;
	}

	ChildSlot
	[
		Widget.ToSharedRef()
	];
}