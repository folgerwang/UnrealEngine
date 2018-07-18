// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigControl.h"

AControlRigControl::AControlRigControl(const FObjectInitializer& ObjectInitializer)
	: Transform(FTransform::Identity)
	, bEnabled(true)
	, bSelected(false)
	, bHovered(false)
	, bManipulating(false)
{
}

void AControlRigControl::SetPropertyPath(const FString& InPropertyPath)
{
	PropertyPath = InPropertyPath;
}

const FString& AControlRigControl::GetPropertyPath() const
{
	return PropertyPath;
}

void AControlRigControl::SetTransform(const FTransform& InTransform)
{
	FTransform OldTransform = Transform;

	Transform = InTransform;

	if(!Transform.Equals(OldTransform))
	{
		FEditorScriptExecutionGuard Guard;
		OnTransformChanged(Transform);
	}
}

const FTransform& AControlRigControl::GetTransform() const
{
	return Transform;
}

void AControlRigControl::SetEnabled(bool bInEnabled)
{
	bool bOldEnabled = bEnabled;

	bEnabled = bInEnabled;

	if(bEnabled != bOldEnabled)
	{
		FEditorScriptExecutionGuard Guard;
		OnEnabledChanged(bEnabled);
	}
}

bool AControlRigControl::IsEnabled() const
{
	return bEnabled;
}

void AControlRigControl::SetSelected(bool bInSelected)
{
	bool bOldSelected = bSelected;

	bSelected = bInSelected;

	if(bSelected != bOldSelected)
	{
		FEditorScriptExecutionGuard Guard;
		OnSelectionChanged(bSelected);
	}
}

bool AControlRigControl::IsSelected() const
{
	return bSelected;
}

void AControlRigControl::SetHovered(bool bInHovered)
{
	bool bOldHovered = bHovered;

	bHovered = bInHovered;

	if(bHovered != bOldHovered)
	{
		FEditorScriptExecutionGuard Guard;
		OnHoveredChanged(bHovered);
	}
}

bool AControlRigControl::IsHovered() const
{
	return bHovered;
}

void AControlRigControl::SetManipulating(bool bInManipulating)
{
	bool bOldManipulating = bManipulating;

	bManipulating = bInManipulating;

	if(bManipulating != bOldManipulating)
	{
		FEditorScriptExecutionGuard Guard;
		OnManipulatingChanged(bManipulating);
	}
}

bool AControlRigControl::IsManipulating() const
{
	return bManipulating;
}

void AControlRigControl::TickControl(float InDeltaSeconds, FRigUnit_Control& InRigUnit, UScriptStruct* InRigUnitStruct)
{

}