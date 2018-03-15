// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SequencerSpacerNode.h"
#include "Widgets/Views/STableRow.h"
#include "MovieScene.h"
#include "SequencerDisplayNodeDragDropOp.h"
#include "SequencerFolderNode.h"
#include "MovieSceneFolder.h"
#include "SequencerDisplayNode.h"
#include "Sequencer.h"
#include "MovieSceneSequence.h"
#include "SequencerNodeSortingMethods.h"
#include "ScopedTransaction.h"

TOptional<EItemDropZone> FSequencerSpacerNode::CanDrop(FSequencerDisplayNodeDragDropOp& DragDropOp, EItemDropZone ItemDropZone) const
{
	// Some spacers shouldn't allow dropping above or below at all as they're 1px tall between rows.
	if (!bIsDragAndDropTarget)
		return TOptional<EItemDropZone>();

	// The spacer node is currently always used at the bottom of the list and we want it to be a drop target that puts things
	// at the end of the root level. We still need to do a folder name collision check though as people could be moving out of
	// a nestled folder up to the root.
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

	// If there's no collision, we'll force them to drop above us so the UI draws where we want it to.
	return TOptional<EItemDropZone>(EItemDropZone::AboveItem);
}

void FSequencerSpacerNode::Drop(const TArray<TSharedRef<FSequencerDisplayNode>>& DraggedNodes, EItemDropZone ItemDropZone)
{
	const FScopedTransaction Transaction(NSLOCTEXT("SequencerTrackNode", "MoveItems", "Move items."));
	for (TSharedRef<FSequencerDisplayNode> DraggedNode : DraggedNodes)
	{
		TSharedPtr<FSequencerDisplayNode> DraggedSeqNodeParent = DraggedNode->GetParent();

		if (GetParent().IsValid())
		{
			// If the object is coming from the root or it's coming from another folder then we can allow it to move adjacent to us.
			if (!DraggedSeqNodeParent.IsValid() || (DraggedSeqNodeParent.IsValid() && DraggedSeqNodeParent->GetType() == ESequencerNode::Folder))
			{
				checkf(GetParent()->GetType() == ESequencerNode::Folder, TEXT("Cannot reorder when parent is not a folder."));
				TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>(GetParent());

				// Let the folder we're going into remove us from our old parent and put us as a child of it first.
				ParentFolder->MoveDisplayNodeToFolder(DraggedNode);
			}
		}
		else
		{
			// We're at root and they're placing above or below us
			ParentTree.MoveDisplayNodeToRoot(DraggedNode);
		}
	}

	if (GetParent().IsValid())
	{
		checkf(GetParent()->GetType() == ESequencerNode::Folder, TEXT("Cannot reorder when parent is not a folder."));
		TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>(GetParent());

		// Sort our dragged nodes relative to our siblings.
		SortAndSetSortingOrder(DraggedNodes, ParentFolder->GetChildNodes(), ItemDropZone, FDisplayNodeTreePositionSorter(), SharedThis(this));
	}
	else
	{
		// We're at root and they're placing above or below us
		SortAndSetSortingOrder(DraggedNodes, GetSequencer().GetNodeTree()->GetRootNodes(), ItemDropZone, FDisplayNodeTreePositionSorter(), SharedThis(this));
	}

	ParentTree.GetSequencer().NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
}