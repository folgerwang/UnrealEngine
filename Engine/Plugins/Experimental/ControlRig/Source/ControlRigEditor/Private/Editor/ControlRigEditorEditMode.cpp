// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigEditorEditMode.h"
#include "IPersonaPreviewScene.h"
#include "AssetEditorModeManager.h"

FName FControlRigEditorEditMode::ModeName("EditMode.ControlRigEditor");

bool FControlRigEditorEditMode::GetCameraTarget(FSphere& OutTarget) const
{
	FBox Box(ForceInit);

	for(const FControlUnitProxy& UnitProxy : ControlUnits)
	{
		if(UnitProxy.IsSelected() && UnitProxy.Control)
		{
			FBox ActorBox = UnitProxy.Control->GetComponentsBoundingBox(true);
			Box += ActorBox;
		}
	}

	if (AreJointSelected())
	{
		for (int32 Index = 0; Index < SelectedJoints.Num(); ++Index)
		{
			FTransform Transform = OnGetJointTransformDelegate.Execute(SelectedJoints[Index], false);
			Box += Transform.GetLocation();
		}
	}

	if(Box.IsValid)
	{
		OutTarget.Center = Box.GetCenter();
		OutTarget.W = Box.GetExtent().GetAbsMax();
		return true;
	}

	return false;
}

IPersonaPreviewScene& FControlRigEditorEditMode::GetAnimPreviewScene() const
{
	return *static_cast<IPersonaPreviewScene*>(static_cast<FAssetEditorModeManager*>(Owner)->GetPreviewScene());
}

void FControlRigEditorEditMode::GetOnScreenDebugInfo(TArray<FText>& OutDebugInfo) const
{

}