// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigTabSummoner.h"
#include "SControlRig.h"
#include "ControlRigEditorStyle.h"
#include "ControlRigEditor.h"

#define LOCTEXT_NAMESPACE "ControlRigTabSummoner"

const FName FControlRigTabSummoner::TabID(TEXT("Rig"));

FControlRigTabSummoner::FControlRigTabSummoner(const TSharedRef<FControlRigEditor>& InControlRigEditor)
	: FWorkflowTabFactory(TabID, InControlRigEditor)
	, ControlRigEditor(InControlRigEditor)
{
	TabLabel = LOCTEXT("ControlRigTabLabel", "Rig");
	TabIcon = FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "ControlRig.TabIcon");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("Rig_ViewMenu_Desc", "Rig");
	ViewMenuTooltip = LOCTEXT("Rig_ViewMenu_ToolTip", "Show the Rig tab");
}

TSharedRef<SWidget> FControlRigTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SControlRig, ControlRigEditor.Pin().ToSharedRef());
}

#undef LOCTEXT_NAMESPACE 
