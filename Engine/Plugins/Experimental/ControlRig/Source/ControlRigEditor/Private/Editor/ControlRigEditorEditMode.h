// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ControlRigEditMode.h"
#include "IPersonaEditMode.h"

#pragma once

class FControlRigEditorEditMode : public FControlRigEditMode
{
public:
	static FName ModeName;

	/** FControlRigEditMode interface */
	virtual bool IsInLevelEditor() const { return false; }

	/* IPersonaEditMode interface */
	virtual bool GetCameraTarget(FSphere& OutTarget) const override;
	virtual class IPersonaPreviewScene& GetAnimPreviewScene() const override;
	virtual void GetOnScreenDebugInfo(TArray<FText>& OutDebugInfo) const override;
};