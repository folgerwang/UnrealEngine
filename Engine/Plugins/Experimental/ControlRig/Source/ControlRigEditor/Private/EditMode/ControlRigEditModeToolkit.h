// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "EditorModes.h"
#include "Toolkits/BaseToolkit.h"
#include "EditorModeManager.h"
#include "SControlRigEditModeTools.h"
#include "ControlRigEditMode.h"

class FControlRigEditModeToolkit : public FModeToolkit
{
public:

	FControlRigEditModeToolkit(FControlRigEditMode& InEditMode)
		: EditMode(InEditMode)
	{
		SAssignNew(ModeTools, SControlRigEditModeTools, EditMode.GetWorld());
	}

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override { return FName("AnimationMode"); }
	virtual FText GetBaseToolkitName() const override { return NSLOCTEXT("AnimationModeToolkit", "DisplayName", "Animation"); }
	virtual class FEdMode* GetEditorMode() const override { return &EditMode; }
	virtual TSharedPtr<class SWidget> GetInlineContent() const override { return ModeTools; }

private:
	/** The edit mode we are bound to */
	FControlRigEditMode& EditMode;

	/** The tools widget */
	TSharedPtr<SControlRigEditModeTools> ModeTools;
};
