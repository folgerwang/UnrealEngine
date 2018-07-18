// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayNodes/SequencerFolderNode.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Views/STableRow.h"
#include "EditorStyleSet.h"
#include "Engine/Engine.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EngineGlobals.h"
#include "MovieScene.h"
#include "ISequencer.h"
#include "DisplayNodes/SequencerObjectBindingNode.h"
#include "DisplayNodes/SequencerTrackNode.h"
#include "SSequencer.h"
#include "MovieSceneFolder.h"
#include "SequencerUtilities.h"
#include "MovieSceneSequence.h"
#include "SequencerDisplayNodeDragDropOp.h"
#include "ScopedTransaction.h"
#include "Widgets/Colors/SColorPicker.h"
#include "SequencerNodeTree.h"
#include "SequencerNodeSortingMethods.h"

#define LOCTEXT_NAMESPACE "SequencerFolderNode"

FSequencerFolderNode::FSequencerFolderNode( UMovieSceneFolder& InMovieSceneFolder, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree )
	: FSequencerDisplayNode( InMovieSceneFolder.GetFolderName(), InParentNode, InParentTree )
	, MovieSceneFolder(InMovieSceneFolder)
{
	FolderOpenBrush = FEditorStyle::GetBrush( "ContentBrowser.AssetTreeFolderOpen" );
	FolderClosedBrush = FEditorStyle::GetBrush( "ContentBrowser.AssetTreeFolderClosed" );
}


ESequencerNode::Type FSequencerFolderNode::GetType() const
{
	return ESequencerNode::Folder;
}


float FSequencerFolderNode::GetNodeHeight() const
{
	// TODO: Add another constant.
	return SequencerLayoutConstants::FolderNodeHeight;
}


FNodePadding FSequencerFolderNode::GetNodePadding() const
{
	return FNodePadding(4, 4);
}


bool FSequencerFolderNode::CanRenameNode() const
{
	return true;
}


FText FSequencerFolderNode::GetDisplayName() const
{
	return FText::FromName( MovieSceneFolder.GetFolderName() );
}


void FSequencerFolderNode::SetDisplayName( const FText& NewDisplayName )
{
	if ( MovieSceneFolder.GetFolderName() != FName( *NewDisplayName.ToString() ) )
	{
		TArray<FName> SiblingNames;
		TSharedPtr<FSequencerDisplayNode> ParentSeqNode = GetParent();
		if ( ParentSeqNode.IsValid() )
		{
			for ( TSharedRef<FSequencerDisplayNode> SiblingNode : ParentSeqNode->GetChildNodes() )
			{
				if ( SiblingNode != AsShared() )
				{
					SiblingNames.Add( FName(*SiblingNode->GetDisplayName().ToString()) );
				}
			}
		}
		else
		{
			for (TSharedRef<FSequencerDisplayNode> RootNode : GetParentTree().GetRootNodes())
			{
				if ( RootNode != AsShared() )
				{
					SiblingNames.Add( FName(*RootNode->GetDisplayName().ToString()) );
				}
			}
		}

		FName UniqueName = FSequencerUtilities::GetUniqueName( FName( *NewDisplayName.ToString() ), SiblingNames );

		const FScopedTransaction Transaction( NSLOCTEXT( "SequencerFolderNode", "RenameFolder", "Rename folder." ) );
		MovieSceneFolder.Modify();
		MovieSceneFolder.SetFolderName( UniqueName );
	}
}


const FSlateBrush* FSequencerFolderNode::GetIconBrush() const
{
	return IsExpanded()
		? FolderOpenBrush
		: FolderClosedBrush;
}


FSlateColor FSequencerFolderNode::GetIconColor() const
{
	return FSlateColor(MovieSceneFolder.GetFolderColor());
}

bool FSequencerFolderNode::CanDrag() const
{
	return true;
}


TOptional<EItemDropZone> FSequencerFolderNode::CanDrop( FSequencerDisplayNodeDragDropOp& DragDropOp, EItemDropZone ItemDropZone ) const
{
	DragDropOp.ResetToDefaultToolTip();

	// Prevent taking any parent that's part of the dragged node hierarchy from being put inside a child of itself
	// This is done first before the other checks so that the UI stays consistent as you move between them.
	TSharedPtr<FSequencerDisplayNode> CurrentNode = SharedThis((FSequencerDisplayNode*)this);
	while (CurrentNode.IsValid())
	{
		if (DragDropOp.GetDraggedNodes().Contains(CurrentNode))
		{
			DragDropOp.CurrentHoverText = NSLOCTEXT("SequencerFolderNode", "ParentIntoChildDragErrorFormat", "Can't drag a parent node into one of it's children.");
			return TOptional<EItemDropZone>();
		}
		CurrentNode = CurrentNode->GetParent();
	}

	// Don't allow "Drop Below" as a target as this causes a confusing UI. Instead, users should try to use the Drop Above of the item after this, or, onto of this (to put it at the end).
	if (ItemDropZone == EItemDropZone::BelowItem)
	{
		ItemDropZone = EItemDropZone::OntoItem;
	}

	// If they're trying to move an item above/below or into us. This item may or may not be a sibling; If it is not already a sibling then
	// we need to check if it has a conflicting name with a sibling that we already have (for folders).
	TArray<UMovieSceneFolder*> AdjacentFolders;
	TSharedPtr<FSequencerDisplayNode> ChildParent;

	if (ItemDropZone == EItemDropZone::OntoItem)
	{
		// If the item is being dropped onto us, we check our own children for name conflicts
		ChildParent = SharedThis((FSequencerDisplayNode*)this);
	}
	else
	{
		// Otherwise, we try to check our siblings
		ChildParent = GetParent();
	}

	if (ChildParent.IsValid())
	{
		for (TSharedRef <FSequencerDisplayNode> Child : ChildParent->GetChildNodes())
		{
			if (Child->GetType() == ESequencerNode::Folder)
			{
				TSharedRef<FSequencerFolderNode> FolderNode = StaticCastSharedRef<FSequencerFolderNode>(Child);
				AdjacentFolders.Add(&FolderNode->GetFolder());
			}
		}
	}
	else
	{
		// If this folder has no parent then this is a root level folder, so we need to check the Movie Scene's child list for conflicting children names.
		UMovieScene* FocusedMovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();
		AdjacentFolders.Append(FocusedMovieScene->GetRootFolders());
	}

	// Check each node we're dragging to see if any of them have a name conflict - if so, block the whole drag/drop operation.
	for (TSharedRef<FSequencerDisplayNode> DraggedNode : DragDropOp.GetDraggedNodes())
	{
		if (DraggedNode->GetType() == ESequencerNode::Folder)
		{
			TSharedRef<FSequencerFolderNode> DraggedFolder = StaticCastSharedRef<FSequencerFolderNode>(DraggedNode);

			// Name Conflicts are only an issue on folders.
			bool bHasNameConflict = false;
			for (UMovieSceneFolder* Folder : AdjacentFolders)
			{

				// We don't allow a folder with the same name to become a sibling, but we need to not check the dragged node if it is already at that
				// hierarchy depth so that we can rearrange them by triggering EItemDropZone::AboveItem / EItemDropZone::BelowItem on the same hierarchy.
				if (&DraggedFolder->GetFolder() != Folder && DraggedFolder->GetFolder().GetFolderName() == Folder->GetFolderName())
				{
					bHasNameConflict = true;
					break;
				}
			}

			if (bHasNameConflict)
			{
				DragDropOp.CurrentHoverText = FText::Format(
					NSLOCTEXT("SequencerFolderNode", "DuplicateFolderDragErrorFormat", "Folder with name '{0}' already exists."),
					FText::FromName(DraggedFolder->GetFolder().GetFolderName()));

				return TOptional<EItemDropZone>();
			}
		}
	}

	return ItemDropZone;
}

void FSequencerFolderNode::MoveDisplayNodeToFolder(TSharedRef<FSequencerDisplayNode>& Node)
{
	GetFolder().Modify();
	TSharedPtr<FSequencerDisplayNode> ParentSeqNode = Node->GetParent();
	switch (Node->GetType())
	{
		case ESequencerNode::Folder:
		{
			TSharedRef<FSequencerFolderNode> DraggedFolderNode = StaticCastSharedRef<FSequencerFolderNode>(Node);
			UMovieScene* FocusedMovieScene = GetParentTree().GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();

			// Remove the folder from where it currently resides, and then we'll add it to it's new location later.
			// We remove it before adding so that when you move a folder within the same hierarchy it doesn't end up
			// removing it after changing order.
			if (ParentSeqNode.IsValid())
			{
				checkf(ParentSeqNode->GetType() == ESequencerNode::Folder, TEXT("Can not remove from unsupported parent node."));
				TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>(ParentSeqNode);
				ParentFolder->GetFolder().Modify();
				ParentFolder->GetFolder().RemoveChildFolder(&DraggedFolderNode->GetFolder());
			}
			else
			{
				FocusedMovieScene->Modify();
				FocusedMovieScene->GetRootFolders().Remove(&DraggedFolderNode->GetFolder());
			}

			// Add this as a child of ourself now
			GetFolder().AddChildFolder(&DraggedFolderNode->GetFolder());
			break;
		}
		case ESequencerNode::Track:
		{
			TSharedRef<FSequencerTrackNode> DraggedTrackNode = StaticCastSharedRef<FSequencerTrackNode>(Node);

			if (ParentSeqNode.IsValid())
			{
				checkf(ParentSeqNode->GetType() == ESequencerNode::Folder, TEXT("Can not remove from unsupported parent node."));
				TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>(ParentSeqNode);
				ParentFolder->GetFolder().Modify();
				ParentFolder->GetFolder().RemoveChildMasterTrack(DraggedTrackNode->GetTrack());
			}
			
			GetFolder().AddChildMasterTrack(DraggedTrackNode->GetTrack());
			break;
		}
		case ESequencerNode::Object:
		{
			TSharedRef<FSequencerObjectBindingNode> DraggedObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>(Node);

			if (ParentSeqNode.IsValid())
			{
				checkf(ParentSeqNode->GetType() == ESequencerNode::Folder, TEXT("Can not remove from unsupported parent node."));
				TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>(ParentSeqNode);
				ParentFolder->GetFolder().Modify();
				ParentFolder->GetFolder().RemoveChildObjectBinding(DraggedObjectBindingNode->GetObjectBinding());
			}

			GetFolder().AddChildObjectBinding(DraggedObjectBindingNode->GetObjectBinding());
			break;
		}
	}

	// Update the node's parent so that request for the NodePath reflect the new path
	// instead of waiting until all nodes are regenerated by the subsequent Refresh call.
	AddChildAndSetParent(Node);

	// Update the expansion state using our new path.
	Node->Traverse_ParentFirst([](FSequencerDisplayNode& TraversalNode)
	{
		TraversalNode.GetParentTree().SaveExpansionState(TraversalNode, TraversalNode.IsExpanded());
		return true;
	}, true);
}

void FSequencerFolderNode::Drop( const TArray<TSharedRef<FSequencerDisplayNode>>& DraggedNodes, EItemDropZone ItemDropZone )
{
	const FScopedTransaction Transaction(NSLOCTEXT("SequencerFolderNode", "MoveItems", "Move items."));
	GetFolder().SetFlags(RF_Transactional);
	GetFolder().Modify();

	// Open this folder if an item was dropped into the folder
	if (ItemDropZone == EItemDropZone::OntoItem)
	{
		SetExpansionState(true);
	}

	for (TSharedRef<FSequencerDisplayNode> DraggedNode : DraggedNodes)
	{
		TSharedPtr<FSequencerDisplayNode> DraggedSeqNodeParent = DraggedNode->GetParent();

		if (ItemDropZone == EItemDropZone::OntoItem)
		{
			// Remove the node from it's old parent and put it as a child of ourself
			MoveDisplayNodeToFolder(DraggedNode);
		}
		else
		{
			if (GetParent().IsValid())
			{
				checkf(GetParent()->GetType() == ESequencerNode::Folder, TEXT("Cannot reorder when parent is not a folder."));
				TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>(GetParent());

				// Let the folder we're going into remove us from our old parent and put us as a child of it first.
				ParentFolder->MoveDisplayNodeToFolder(DraggedNode);
			}
			else
			{
				// We're at root and they're placing above or below us
				ParentTree.MoveDisplayNodeToRoot(DraggedNode);
			}
		}
	}

	if (ItemDropZone == EItemDropZone::OntoItem)
	{
		// Sort our dragged nodes relative to our children
		SortAndSetSortingOrder(DraggedNodes, GetChildNodes(), ItemDropZone, FDisplayNodeTreePositionSorter(), SharedThis(this));
	}
	else
	{
		// If it's above or below us, then we need to check our parent and have them sort the nodes relative to our siblings.
		if (GetParent().IsValid())
		{
			checkf(GetParent()->GetType() == ESequencerNode::Folder, TEXT("Cannot reorder when parent is not a folder."));
			TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>(GetParent());

			// Sort our dragged nodes relative to our siblings.
			SortAndSetSortingOrder(DraggedNodes, ParentFolder->GetChildNodes(), ItemDropZone, FDisplayNodeTreePositionSorter(), SharedThis(this));
		}
		else
		{
			// We're at root
			SortAndSetSortingOrder(DraggedNodes, GetSequencer().GetNodeTree()->GetRootNodes(), ItemDropZone, FDisplayNodeTreePositionSorter(), SharedThis(this));
		}
	}

	ParentTree.GetSequencer().NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
}

void FSequencerFolderNode::BuildContextMenu(FMenuBuilder& MenuBuilder)
{
	FSequencerDisplayNode::BuildContextMenu(MenuBuilder);

	TSharedRef<FSequencerFolderNode> ThisNode = SharedThis(this);

	MenuBuilder.BeginSection("Folder", LOCTEXT("FolderContextMenuSectionName", "Folder"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetColor", "Set Color"),
			LOCTEXT("SetColorTooltip", "Set the folder color"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FSequencerFolderNode::SetFolderColor))
		);
	}
	MenuBuilder.EndSection();
}

FColor InitialFolderColor;
bool bFolderPickerWasCancelled;

void FSequencerFolderNode::SetFolderColor()
{
	InitialFolderColor = MovieSceneFolder.GetFolderColor();
	bFolderPickerWasCancelled = false;

	FColorPickerArgs PickerArgs;
	PickerArgs.bUseAlpha = false;
	PickerArgs.DisplayGamma = TAttribute<float>::Create( TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma) );
	PickerArgs.InitialColorOverride = InitialFolderColor.ReinterpretAsLinear();
	PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP( this, &FSequencerFolderNode::OnColorPickerPicked);
	PickerArgs.OnColorPickerWindowClosed = FOnWindowClosed::CreateSP( this, &FSequencerFolderNode::OnColorPickerClosed);
	PickerArgs.OnColorPickerCancelled  = FOnColorPickerCancelled::CreateSP( this, &FSequencerFolderNode::OnColorPickerCancelled );

	OpenColorPicker(PickerArgs);
}

void FSequencerFolderNode::OnColorPickerPicked(FLinearColor NewFolderColor)
{			
	MovieSceneFolder.SetFolderColor(NewFolderColor.ToFColor(false));
}

void FSequencerFolderNode::OnColorPickerClosed(const TSharedRef<SWindow>& Window)
{	
	if (!bFolderPickerWasCancelled)
	{
		const FScopedTransaction Transaction( NSLOCTEXT( "SequencerFolderNode", "SetFolderColor", "Set Folder Color" ) );
		
		FColor CurrentColor = MovieSceneFolder.GetFolderColor();
		MovieSceneFolder.SetFolderColor(InitialFolderColor);
		MovieSceneFolder.Modify();
		MovieSceneFolder.SetFolderColor(CurrentColor);
	}
}

void FSequencerFolderNode::OnColorPickerCancelled(FLinearColor NewFolderColor)
{
	bFolderPickerWasCancelled = true;

	MovieSceneFolder.SetFolderColor(InitialFolderColor);
}

void FSequencerFolderNode::AddChildNode( TSharedRef<FSequencerDisplayNode> ChildNode )
{
	AddChildAndSetParent( ChildNode );
}


UMovieSceneFolder& FSequencerFolderNode::GetFolder() const
{
	return MovieSceneFolder;
}

int32 FSequencerFolderNode::GetSortingOrder() const
{
	return MovieSceneFolder.GetSortingOrder();
}

void FSequencerFolderNode::SetSortingOrder(const int32 InSortingOrder)
{
	MovieSceneFolder.SetSortingOrder(InSortingOrder);
}

void FSequencerFolderNode::ModifyAndSetSortingOrder(const int32 InSortingOrder)
{
	MovieSceneFolder.Modify();
	SetSortingOrder(InSortingOrder);
}
#undef LOCTEXT_NAMESPACE
