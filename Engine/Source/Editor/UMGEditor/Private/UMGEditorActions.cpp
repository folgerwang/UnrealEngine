// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorActions.h"

#define LOCTEXT_NAMESPACE "UMGEditorCommands"

void FUMGEditorCommands::RegisterCommands()
{
	UI_COMMAND(CreateNativeBaseClass, "Create Native Base Class", "Create a native base class for this widget, using the current parent as the parent of the native class.", EUserInterfaceActionType::Button, FInputChord() );
}

#undef LOCTEXT_NAMESPACE