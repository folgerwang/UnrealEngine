// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprintCommands.h"

#define LOCTEXT_NAMESPACE "ControlRigBlueprintCommands"

void FControlRigBlueprintCommands::RegisterCommands()
{
	UI_COMMAND(DeleteItem, "Delete", "Deletes the selected items and removes their nodes from the graph.", EUserInterfaceActionType::Button, FInputChord(EKeys::Delete));
}

#undef LOCTEXT_NAMESPACE
