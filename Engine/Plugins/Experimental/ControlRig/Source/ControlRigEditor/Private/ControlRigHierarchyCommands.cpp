// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigHierarchyCommands.h"

#define LOCTEXT_NAMESPACE "ControlRigHierarchyCommands"

void FControlRigHierarchyCommands::RegisterCommands()
{
	UI_COMMAND(AddItem, "New", "Add new item at the origin (0, 0, 0) to the hierarchy.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DuplicateItem, "Duplicate", "Duplicate the selected items in the hierarchy.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DeleteItem, "Delete", "Deletes the selected items from the hierarchy.", EUserInterfaceActionType::Button, FInputChord(EKeys::Delete));
	UI_COMMAND(RenameItem, "Rename", "Rename the selected item.", EUserInterfaceActionType::Button, FInputChord(EKeys::F2));
}

#undef LOCTEXT_NAMESPACE
