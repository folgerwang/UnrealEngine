// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "ControlRigPickerWidget.h"

class SEditorUserWidgetHost : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SEditorUserWidgetHost) {}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UWorld* InWorld);

	/** FGCObject interface */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	/** Set a new user widget class */
	void SetUserWidgetClass(TSubclassOf<UControlRigPickerWidget> InUserWidgetClass);

private:
	/** The world we create widgets with */
	TWeakObjectPtr<UWorld> World;

	/** The user widget we are hosting */
	UControlRigPickerWidget* UserWidget;
};