// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "ControlRigEditorStyle.h"

class FControlRigHierarchyCommands : public TCommands<FControlRigHierarchyCommands>
{
public:
	FControlRigHierarchyCommands() : TCommands<FControlRigHierarchyCommands>
	(
		"ControlRigHierarchy",
		NSLOCTEXT("Contexts", "RigHierarchy", "Rig Hierarchy"),
		NAME_None, // "MainFrame" // @todo Fix this crash
		FControlRigEditorStyle::Get().GetStyleSetName() // Icon Style Set
	)
	{}
	
	/** Add Item at origin */
	TSharedPtr< FUICommandInfo > AddItem;

	/** Duplicate currently selected items */
	TSharedPtr< FUICommandInfo > DuplicateItem;

	/** Delete currently selected items */
	TSharedPtr< FUICommandInfo > DeleteItem;

	/** Rename selected item */
	TSharedPtr< FUICommandInfo > RenameItem;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;
};
