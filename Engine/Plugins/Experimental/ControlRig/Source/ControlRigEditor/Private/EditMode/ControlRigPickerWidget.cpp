// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigPickerWidget.h"
#include "ControlRigEditMode.h"

UControlRigPickerWidget::UControlRigPickerWidget(const FObjectInitializer& ObjectInitializer)
	: UUserWidget(ObjectInitializer)
	, EditMode(nullptr)
{
}

void UControlRigPickerWidget::SelectControl(const FString& ControlPropertyPath, bool bSelected)
{
	if(EditMode)
	{
		EditMode->SetControlSelection(ControlPropertyPath, bSelected);
	}
}

bool UControlRigPickerWidget::IsControlSelected(const FString& ControlPropertyPath)
{
	if(EditMode)
	{
		return EditMode->IsControlSelected(ControlPropertyPath);
	}

	return false;
}

void UControlRigPickerWidget::EnableControl(const FString& ControlPropertyPath, bool bEnabled)
{
	if(EditMode)
	{
		EditMode->SetControlEnabled(ControlPropertyPath, bEnabled);
	}
}

bool UControlRigPickerWidget::IsControlEnabled(const FString& ControlPropertyPath)
{
	if(EditMode)
	{
		return EditMode->IsControlEnabled(ControlPropertyPath);
	}

	return false;
}