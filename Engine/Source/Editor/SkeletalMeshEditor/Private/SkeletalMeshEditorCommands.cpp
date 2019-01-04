// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "SkeletalMeshEditorCommands.h"

#define LOCTEXT_NAMESPACE "SkeletalMeshEditorCommands"

void FSkeletalMeshEditorCommands::RegisterCommands()
{
	UI_COMMAND(ReimportMesh, "Reimport Mesh", "Reimport the current mesh.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ReimportAllMesh, "Reimport All Mesh", "Reimport the current mesh and all the custom LODs.", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(MeshSectionSelection, "Section Selection", "Enables selecting Mesh Sections in the viewport (disables selecting bones using their physics shape).", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(UpdateRefPose, "Update Reference Pose", "Update Reference Pose with current pose", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
