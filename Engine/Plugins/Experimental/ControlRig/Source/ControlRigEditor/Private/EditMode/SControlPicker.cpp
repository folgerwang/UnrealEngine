// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SControlPicker.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SButton.h"
#include "ScopedTransaction.h"
#include "ControlRigEditMode.h"
#include "ControlRig.h"
#include "EditorModeManager.h"
#include "SEditorUserWidgetHost.h"
#include "ControlRigBlueprint.h"
#include "ControlRigBlueprintGeneratedClass.h"

#define LOCTEXT_NAMESPACE "ControlPicker"

void SControlPicker::Construct(const FArguments& InArgs, UWorld* InWorld)
{
	ChildSlot
	[
		SAssignNew(EditorUserWidgetHost, SEditorUserWidgetHost, InWorld)
		.Visibility(this, &SControlPicker::ShowWidgetHost)
	];
}

void SControlPicker::SetControlRig(UControlRig* InRig)
{
	if (InRig != RigPtr.Get())
	{
		RigPtr = InRig;
	}
}

UControlRig* SControlPicker::GetRig() const
{
	return RigPtr.Get();
}

EVisibility SControlPicker::ShowWidgetHost() const
{
	return (GetRig() != nullptr) ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
