// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlUnitProxy.h"
#include "ControlRigControl.h"
#include "ControlRig.h"

void FControlUnitProxy::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;
	if(Control != nullptr)
	{
		Control->SetEnabled(bEnabled);
	}
}

bool FControlUnitProxy::IsEnabled() const
{
	return bEnabled;
}

void FControlUnitProxy::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	if(Control != nullptr)
	{
		Control->SetSelected(bSelected);
	}
}

bool FControlUnitProxy::IsSelected() const
{
	return bSelected;
}

void FControlUnitProxy::SetHovered(bool bInHovered)
{
	bHovered = bInHovered;
	if(Control != nullptr)
	{
		Control->SetHovered(bHovered);
	}
}

bool FControlUnitProxy::IsHovered() const
{
	return bHovered;
}

void FControlUnitProxy::SetManipulating(bool bInManipulating)
{
	bManipulating = bInManipulating;
	if(Control != nullptr)
	{
		Control->SetManipulating(bManipulating);
	}
}

bool FControlUnitProxy::IsManipulating() const
{
	return bManipulating;
}

void FControlUnitProxy::NotifyPreEditChangeProperty(UControlRig* InControlRig)
{
	if (InControlRig)
	{
		if(!TransformPropertyPath.IsResolved())
		{
			TransformPropertyPath.Resolve(InControlRig);
		}

		FEditPropertyChain EditPropertyChain;
		TransformPropertyPath.ToEditPropertyChain(EditPropertyChain);
		InControlRig->PreEditChange(EditPropertyChain);
	}
}

void FControlUnitProxy::NotifyPostEditChangeProperty(UControlRig* InControlRig)
{
	if (InControlRig)
	{
		if(!TransformPropertyPath.IsResolved())
		{
			TransformPropertyPath.Resolve(InControlRig);
		}

		FPropertyChangedEvent PropertyChangedEvent = TransformPropertyPath.ToPropertyChangedEvent(bManipulating ? EPropertyChangeType::Interactive : EPropertyChangeType::ValueSet);
		InControlRig->PostEditChangeProperty(PropertyChangedEvent);
	}
}