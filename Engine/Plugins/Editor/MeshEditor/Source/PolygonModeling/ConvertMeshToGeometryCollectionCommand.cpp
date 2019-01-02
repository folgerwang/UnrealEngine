// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConvertMeshToGeometryCollectionCommand.h"
#include "IMeshEditorModeEditingContract.h"
#include "GeometryCollection/GeometryCollectionConversion.h"

#define LOCTEXT_NAMESPACE "ConvertMeshToGeometryCollectionCommand"

void UConvertMeshToGeometryCollectionCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "GenerateAsset", "Generate Asset", "Converts selected mesh to a Geometry Collection.", EUserInterfaceActionType::Button, FInputChord() );
}

void UConvertMeshToGeometryCollectionCommand::Execute(IMeshEditorModeEditingContract& MeshEditorMode)
{
	if (MeshEditorMode.GetActiveAction() != NAME_None)
	{
		return;
	}

	FGeometryCollectionConversion::CreateGeometryCollectionCommand(GetWorld());
}

#undef LOCTEXT_NAMESPACE
