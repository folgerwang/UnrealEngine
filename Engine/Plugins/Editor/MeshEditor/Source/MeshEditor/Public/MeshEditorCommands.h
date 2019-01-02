// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshElement.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/Commands.h"
#include "MeshEditorCommands.generated.h"


UCLASS( abstract )
class MESHEDITOR_API UMeshEditorCommand : public UObject
{
	GENERATED_BODY()

public:

	/** Which mesh element does this command apply to? */
	virtual EEditableMeshElementType GetElementType() const PURE_VIRTUAL(,return EEditableMeshElementType::Invalid;);

	/** Registers the UI command for this mesh editor command */
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) PURE_VIRTUAL(,);

	/** Creates an UI action for this command */
	virtual FUIAction MakeUIAction( class IMeshEditorModeUIContract& MeshEditorMode ) PURE_VIRTUAL(,return FUIAction(););

	/** Allows this command to directly add a button to the VR Mode's radial menu */
	virtual void AddToVRRadialMenuActionsMenu( class IMeshEditorModeUIContract& MeshEditorMode, class FMenuBuilder& MenuBuilder, TSharedPtr<class FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode )
	{
	}

	/** Gets the name of this command.  This is not to display to a user, but instead used to uniquely identify this command */
	FName GetCommandName() const
	{
		return UICommandInfo->GetCommandName();
	}

	/** Gets the UI command info for this command */
	const TSharedPtr<FUICommandInfo>& GetUICommandInfo() const
	{
		return UICommandInfo;
	}


protected:

	/** Our UI command for this action */
	TSharedPtr<FUICommandInfo> UICommandInfo;
};


UCLASS( abstract )
class MESHEDITOR_API UMeshEditorInstantCommand : public UMeshEditorCommand
{
	GENERATED_BODY()

public:

	/** Runs this command */
	virtual void Execute( class IMeshEditorModeEditingContract& MeshEditorMode )
	{
	}

	// UMeshEditorCommand overrides
	virtual FUIAction MakeUIAction( class IMeshEditorModeUIContract& MeshEditorMode ) override;

protected:

};



UCLASS( abstract )
class MESHEDITOR_API UMeshEditorEditCommand : public UMeshEditorCommand
{
	GENERATED_BODY()

public:

	// UMeshEditorCommand overrides
	virtual FUIAction MakeUIAction( class IMeshEditorModeUIContract& MeshEditorMode ) override;

	/** Called when the user starts to drag on an element.  If this returns true, then the action will begin and ApplyDuringDrag() will be called each frame until the user releases the button. */
	virtual bool TryStartingToDrag( IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor )
	{
		return true;
	}

	/** Applies this command every frame while dragging */
	virtual void ApplyDuringDrag( class IMeshEditorModeEditingContract& MeshEditorMode, class UViewportInteractor* ViewportInteractor )
	{
	}

	/** Gets the text to send to the transaction system when creating an undo/redo event for this action */
	FText GetUndoText() const
	{
		check( !UndoText.IsEmpty() );
		return UndoText;
	}

	/** Returns whether we rely on a hover location under the interactor being updated as we drag during this action */
	bool NeedsHoverLocation() const
	{
		return bNeedsHoverLocation;
	}

	/** Returns whether this command will kick off regular free translation of the selected mesh elements when dragging starts */
	bool NeedsDraggingInitiated() const
	{
		return bNeedsDraggingInitiated;
	}


protected:

	/** The text to send to the transaction system when creating an undo / redo event for this action */
	UPROPERTY( EditAnywhere, Category=MeshEditor )
	FText UndoText;

	/** Whether this command will kick off regular free translation of the selected mesh elements when dragging starts */
	UPROPERTY( EditAnywhere, Category=MeshEditor )
	bool bNeedsDraggingInitiated;

	/** Whether we rely on a hover location under the interactor being updated as we drag during this action */
	UPROPERTY( EditAnywhere, Category=MeshEditor )
	bool bNeedsHoverLocation;
};



// Actions that can be invoked from this mode as long as at least one mesh is selected
class MESHEDITOR_API FMeshEditorCommonCommands : public TCommands<FMeshEditorCommonCommands>
{
public:
	FMeshEditorCommonCommands();

	// TCommands<> interface
	virtual void RegisterCommands() override;
	// End of TCommands<> interface

public:

	/** Increases the number of subdivision levels for the selected mesh. */
	TSharedPtr<FUICommandInfo> AddSubdivisionLevel;

	/** Decreases the number of subdivision levels for the selected mesh. */
	TSharedPtr<FUICommandInfo> RemoveSubdivisionLevel;

	/** Shows vertex normals */
	TSharedPtr<FUICommandInfo> ShowVertexNormals;

	/** Marquee select actions */
	TSharedPtr<FUICommandInfo> MarqueeSelectVertices;
	TSharedPtr<FUICommandInfo> MarqueeSelectEdges;
	TSharedPtr<FUICommandInfo> MarqueeSelectPolygons;

	/** Draw vertices */
	TSharedPtr<FUICommandInfo> DrawVertices;

	/** Frame selected elements */
	TSharedPtr<FUICommandInfo> FrameSelectedElements;

	/** Set mesh element selection modes */
	TSharedPtr<FUICommandInfo> SetVertexSelectionMode;
	TSharedPtr<FUICommandInfo> SetEdgeSelectionMode;
	TSharedPtr<FUICommandInfo> SetPolygonSelectionMode;
	TSharedPtr<FUICommandInfo> SetAnySelectionMode;
	TSharedPtr<FUICommandInfo> SetFractureSelectionMode;
};


// Actions that can be invoked from this mode for any type of selected element
class MESHEDITOR_API FMeshEditorAnyElementCommands : public TCommands<FMeshEditorAnyElementCommands>
{
public:
	FMeshEditorAnyElementCommands();

	// TCommands<> interface
	virtual void RegisterCommands() override;
	// End of TCommands<> interface
};


// Actions that can be invoked from this mode when vertices are selected
class MESHEDITOR_API FMeshEditorVertexCommands : public TCommands<FMeshEditorVertexCommands>
{
public:
	FMeshEditorVertexCommands();

	// TCommands<> interface
	virtual void RegisterCommands() override;
	// End of TCommands<> interface

public:

	/** Sets the primary action to move vertices */
	TSharedPtr<FUICommandInfo> MoveVertex;

	/** Welds the selected vertices */
	TSharedPtr<FUICommandInfo> WeldVertices;
};


// Actions that can be invoked from this mode when edges are selected
class MESHEDITOR_API FMeshEditorEdgeCommands : public TCommands<FMeshEditorEdgeCommands>
{
public:
	FMeshEditorEdgeCommands();

	// TCommands<> interface
	virtual void RegisterCommands() override;
	// End of TCommands<> interface

public:

	/** Sets the primary action to move edges */
	TSharedPtr<FUICommandInfo> MoveEdge;

	/** Select edge loop */
	TSharedPtr<FUICommandInfo> SelectEdgeLoop;
};


// Actions that can be invoked from this mode when polygons are selected
class MESHEDITOR_API FMeshEditorPolygonCommands : public TCommands<FMeshEditorPolygonCommands>
{
public:
	FMeshEditorPolygonCommands();

	// TCommands<> interface
	virtual void RegisterCommands() override;
	// End of TCommands<> interface

public:

	/** Sets the primary action to move polygons */
	TSharedPtr<FUICommandInfo> MovePolygon;

	/** Triangulates the currently selected polygon(s) */
	TSharedPtr<FUICommandInfo> TriangulatePolygon;
};

// Mesh Fracture Tools
class MESHEDITOR_API FMeshEditorFractureCommands : public TCommands<FMeshEditorFractureCommands>
{
public:
	FMeshEditorFractureCommands();

	// TCommands<> interface
	virtual void RegisterCommands() override;
	// End of TCommands<> interface
};


UCLASS()
class MESHEDITOR_API UMeshEditorCommandList : public UObject
{
	GENERATED_BODY()

public:

	void HarvestMeshEditorCommands();

	/** All of the mesh editor commands that were registered at startup */
	UPROPERTY()
	TArray<UMeshEditorCommand*> MeshEditorCommands;
};


namespace MeshEditorCommands
{
	MESHEDITOR_API const TArray<UMeshEditorCommand*>& Get();
}



