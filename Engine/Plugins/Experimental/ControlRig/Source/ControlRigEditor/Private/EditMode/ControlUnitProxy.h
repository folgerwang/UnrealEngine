// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyPathHelpers.h"
#include "ControlRigControl.h"

class UControlRig;

/** Proxy struct used to managing controls */
struct FControlUnitProxy
{
	FControlUnitProxy()
		: Control(nullptr)
		, bSelected(false)
		, bEnabled(true)
		, bHovered(false)
		, bManipulating(false)
	{}

	/** Set the control to be enabled/disabled */
	void SetEnabled(bool bInEnabled);

	/** Get whether the control is enabled/disabled */
	bool IsEnabled() const;

	/** Set the control to be selected/unselected */
	void SetSelected(bool bInSelected);

	/** Get whether the control is selected/unselected */
	bool IsSelected() const;

	/** Set the control to be hovered/non-hovered */
	void SetHovered(bool bInHovered);

	/** Get whether the control is hovered/non-hovered */
	bool IsHovered() const;

	/** Set whether the control is being manipulated */
	void SetManipulating(bool bInManipulating);

	/** Get whether the control is being manipulated */
	bool IsManipulating() const;

	/** Notify when the property is about to change */
	void NotifyPreEditChangeProperty(UControlRig* InControlRig);

	/** Notify when the property has changed */
	void NotifyPostEditChangeProperty(UControlRig* InControlRig);

	/** Property path to this unit in the rig */
	FCachedPropertyPath PropertyPath;

	/** Property path to this unit's transform in the rig */
	FCachedPropertyPath TransformPropertyPath;

	/** Property path as string */
	FString PropertyPathString;

	/** Transform property path as string */
	FString TransformPropertyPathString;

	/** The control actor used to visualize the unit */
	AControlRigControl* Control;

private:
	/** Whether the unit is selected */
	bool bSelected;

	/** Whether the unit is enabled */
	bool bEnabled;

	/** Whether the unit is hovered */
	bool bHovered;

	/** Whether the unit is manipulating */
	bool bManipulating;
};