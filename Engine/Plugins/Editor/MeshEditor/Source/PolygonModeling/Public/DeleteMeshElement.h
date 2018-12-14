// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshEditorCommands.h"

#include "DeleteMeshElement.generated.h"


UCLASS()
class POLYGONMODELING_API UDeleteMeshElementCommand : public UMeshEditorInstantCommand
{
	GENERATED_BODY()

public:

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override { return EEditableMeshElementType::Any; }

	virtual void RegisterUICommand( class FBindingContext* BindingContext ) override;
	virtual void Execute( class IMeshEditorModeEditingContract& MeshEditorMode ) override;
	virtual void AddToVRRadialMenuActionsMenu( class IMeshEditorModeUIContract& MeshEditorMode, class FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode ) override;
};