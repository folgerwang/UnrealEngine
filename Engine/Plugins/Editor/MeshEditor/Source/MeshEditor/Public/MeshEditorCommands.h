// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshElement.h"
#include "UIAction.h"
#include "MeshEditorCommands.generated.h"


UCLASS( abstract )
class MESHEDITOR_API UMeshEditorCommand : public UObject
{
	GENERATED_BODY()

public:

	/** What type of mesh element is this command for? */
	virtual EEditableMeshElementType GetElementType() const PURE_VIRTUAL(,return EEditableMeshElementType::Invalid;);

	/** Registers the UI command for this mesh editor command */
	virtual void RegisterUICommand( class FBindingContext* BindingContext ) PURE_VIRTUAL(,);

	/** Runs this command */
	virtual void Execute( class IMeshEditorModeEditingContract& MeshEditorMode )
	{
	}

	/** Applies this command every frame while dragging */
	virtual void ApplyDuringDrag( class IMeshEditorModeEditingContract& MeshEditorMode, bool& bOutShouldDeselectAllFirst, TArray<FMeshElement>& OutMeshElementsToSelect )
	{
	}

	/** Allows this command to directly add a button to the VR Mode's radial menu */
	virtual void AddToVRRadialMenuActionsMenu( class IMeshEditorModeUIContract& MeshEditorMode, class FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode )
	{
	}

	/** Gets the name of this command.  This is not to display to a user, but instead used to uniquely identify this command */
	FName GetCommandName() const
	{
		return UICommandInfo->GetCommandName();
	}

	/** Gets the text to send to the transaction system when creating an undo/redo event for this action */
	FText GetUndoText() const
	{
		check( !bIsMode || !UndoText.IsEmpty() );	// All mode-based commands must have supplied UndoText.  Instantaneous commands handle their own undo/redo.
		return UndoText;
	}

	/** Returns true if this is a mesh editing 'Mode' that the user will stay in to perform the action multiple times, or false
		if the action applies instantly */
	bool IsMode() const
	{
		return bIsMode;
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

	/** Gets the UI command info for this command */
	const TSharedPtr<class FUICommandInfo>& GetUICommandInfo() const
	{
		return UICommandInfo;
	}

	/** Creates an UI action for this command */
	FUIAction MakeUIAction( class IMeshEditorModeUIContract& MeshEditorMode );


protected:

	/** The text to send to the transaction system when creating an undo / redo event for this action */
	UPROPERTY( EditAnywhere, Category=MeshEditor )
	FText UndoText;

	/** True if this is a mesh editing 'Mode' that the user will stay in to perform the action multiple times, or false
	    if the action applies instantly */
	UPROPERTY( EditAnywhere, Category=MeshEditor )
	bool bIsMode;

	/** Whether this command will kick off regular free translation of the selected mesh elements when dragging starts */
	UPROPERTY( EditAnywhere, Category=MeshEditor )
	bool bNeedsDraggingInitiated;

	/** Whether we rely on a hover location under the interactor being updated as we drag during this action */
	UPROPERTY( EditAnywhere, Category=MeshEditor )
	bool bNeedsHoverLocation;

	/** Our UI command for this action */
	TSharedPtr<FUICommandInfo> UICommandInfo;
};


UCLASS( abstract )
class MESHEDITOR_API UMeshEditorVertexCommand : public UMeshEditorCommand
{
	GENERATED_BODY()

public:

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Vertex;
	}

};


UCLASS( abstract )
class MESHEDITOR_API UMeshEditorEdgeCommand : public UMeshEditorCommand
{
	GENERATED_BODY()

public:

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Edge;
	}

};


UCLASS( abstract )
class MESHEDITOR_API UMeshEditorPolygonCommand : public UMeshEditorCommand
{
	GENERATED_BODY()

public:

	// Overrides
	virtual EEditableMeshElementType GetElementType() const override
	{
		return EEditableMeshElementType::Polygon;
	}

};



// Actions that can be invoked from this mode regardless of what type of elements are selected
class FMeshEditorCommonCommands : public TCommands<FMeshEditorCommonCommands>
{
public:
	FMeshEditorCommonCommands();

	// TCommands<> interface
	virtual void RegisterCommands() override;
	// End of TCommands<> interface

public:

	/** Deletes selected mesh elements, including polygons partly defined by selected elements */
	TSharedPtr<FUICommandInfo> DeleteMeshElement;

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

	/** Quadrangulate mesh */
	TSharedPtr<FUICommandInfo> QuadrangulateMesh;
};


// Actions that can be invoked from this mode when vertices are selected
class FMeshEditorVertexCommands : public TCommands<FMeshEditorVertexCommands>
{
public:
	FMeshEditorVertexCommands();

	// TCommands<> interface
	virtual void RegisterCommands() override;
	// End of TCommands<> interface

public:

	/** Sets the primary action to move vertices */
	TSharedPtr<FUICommandInfo> MoveVertex;

	/** Sets the primary action to extend vertices */
	TSharedPtr<FUICommandInfo> ExtendVertex;

	/** Sets the primary action to edit the vertex's corner sharpness */
	TSharedPtr<FUICommandInfo> EditVertexCornerSharpness;

	/** Removes the selected vertex if possible */
	TSharedPtr<FUICommandInfo> RemoveVertex;

	/** Welds the selected vertices */
	TSharedPtr<FUICommandInfo> WeldVertices;
};


// Actions that can be invoked from this mode when edges are selected
class FMeshEditorEdgeCommands : public TCommands<FMeshEditorEdgeCommands>
{
public:
	FMeshEditorEdgeCommands();

	// TCommands<> interface
	virtual void RegisterCommands() override;
	// End of TCommands<> interface

public:

	/** Sets the primary action to move edges */
	TSharedPtr<FUICommandInfo> MoveEdge;

	/** Sets the primary action to split edges */
	TSharedPtr<FUICommandInfo> SplitEdge;

	/** Sets the primary action to split edges and drag vertices */
	TSharedPtr<FUICommandInfo> SplitEdgeAndDragVertex;

	/** Sets the primary action to extend edges */
	TSharedPtr<FUICommandInfo> ExtendEdge;

	/** Sets the primary action to edit the edge's crease sharpness */
	TSharedPtr<FUICommandInfo> EditEdgeCreaseSharpness;

	/** Removes the selected edge if possible */
	TSharedPtr<FUICommandInfo> RemoveEdge;

	/** Soften edge */
	TSharedPtr<FUICommandInfo> SoftenEdge;

	/** Harden edge */
	TSharedPtr<FUICommandInfo> HardenEdge;

	/** Select edge loop */
	TSharedPtr<FUICommandInfo> SelectEdgeLoop;
};


// Actions that can be invoked from this mode when polygons are selected
class FMeshEditorPolygonCommands : public TCommands<FMeshEditorPolygonCommands>
{
public:
	FMeshEditorPolygonCommands();

	// TCommands<> interface
	virtual void RegisterCommands() override;
	// End of TCommands<> interface

public:

	/** Sets the primary action to move polygons */
	TSharedPtr<FUICommandInfo> MovePolygon;

	/** Sets the primary action to extrude polygons */
	TSharedPtr<FUICommandInfo> ExtrudePolygon;

	/** Sets the primary action to freely extrude polygons */
	TSharedPtr<FUICommandInfo> FreelyExtrudePolygon;

	/** Sets the primary action to inset polygons */
	TSharedPtr<FUICommandInfo> InsetPolygon;

	/** Sets the primary action to bevel polygons */
	TSharedPtr<FUICommandInfo> BevelPolygon;

	/** Flips the currently selected polygon(s) */
	TSharedPtr<FUICommandInfo> FlipPolygon;

	/** Triangulates the currently selected polygon(s) */
	TSharedPtr<FUICommandInfo> TriangulatePolygon;

	/** Assigns the highlighted material to the currently selected polygon(s) */
	TSharedPtr<FUICommandInfo> AssignMaterial;
};

