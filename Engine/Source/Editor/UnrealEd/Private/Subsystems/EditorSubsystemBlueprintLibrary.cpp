// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Subsystems/EditorSubsystemBlueprintLibrary.h"

#include "Editor.h"

/*static*/ UEditorSubsystem* UEditorSubsystemBlueprintLibrary::GetEditorSubsystem(TSubclassOf<UEditorSubsystem> Class)
{
	return GEditor->GetEditorSubsystemBase(Class);
}
