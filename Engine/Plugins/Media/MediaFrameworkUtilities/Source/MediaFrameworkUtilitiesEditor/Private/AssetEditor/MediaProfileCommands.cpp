// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaProfileCommands.h"

#define LOCTEXT_NAMESPACE "MediaProfileCommands"

void FMediaProfileCommands::RegisterCommands()
{
	UI_COMMAND(Apply, "Apply", "Apply changes to the media profile.", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE