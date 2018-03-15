// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBox.h"

class SSequencerTreeViewRow;

/**
 * A node that displays a category for other nodes
 */
class FSequencerSpacerNode
	: public FSequencerDisplayNode
{
public:

	/**
	 * Create and initialize a new instance.
	 * 
	 * @param InParentNode The parent of this node, or nullptr if this is a root node.
	 * @param InParentTree The tree this node is in.
	 * @param InIsDragAndDropTarget Determines whether or not this spacer will allow dropping items above it at all.
	 */
	FSequencerSpacerNode(float InSize, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree, bool InIsDragAndDropTarget)
		: FSequencerDisplayNode(NAME_None, InParentNode, InParentTree)
		, Size(InSize)
		, bIsDragAndDropTarget(InIsDragAndDropTarget)
	{ }

public:

	// FSequencerDisplayNode interface

	virtual bool CanRenameNode() const override { return false; }
	virtual FText GetDisplayName() const override { return FText(); }
	virtual float GetNodeHeight() const override { return Size; }
	virtual FNodePadding GetNodePadding() const override { return FNodePadding(0.f); }
	virtual ESequencerNode::Type GetType() const override { return ESequencerNode::Spacer; }
	virtual void SetDisplayName(const FText& NewDisplayName) override { }
	virtual TSharedRef<SWidget> GenerateContainerWidgetForOutliner(const TSharedRef<SSequencerTreeViewRow>& InRow) override { return SNew(SBox).HeightOverride(Size); }
	virtual bool IsSelectable() const override { return false; }
	virtual TOptional<EItemDropZone> CanDrop(FSequencerDisplayNodeDragDropOp& DragDropOp, EItemDropZone ItemDropZone) const override;
	virtual void Drop(const TArray<TSharedRef<FSequencerDisplayNode>>& DraggedNodes, EItemDropZone ItemDropZone) override;
private:

	/** The size of the spacer */
	float Size;

	/** Does this spacer allow drag and drop operations. Spacers that go between individual rows shouldn't allow them. */
	bool bIsDragAndDropTarget;
};
