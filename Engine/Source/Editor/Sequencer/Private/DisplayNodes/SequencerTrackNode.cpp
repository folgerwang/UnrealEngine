// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DisplayNodes/SequencerTrackNode.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Views/STableRow.h"
#include "DisplayNodes/SequencerObjectBindingNode.h"
#include "Framework/Commands/UIAction.h"
#include "Textures/SlateIcon.h"
#include "UObject/UnrealType.h"
#include "MovieSceneTrack.h"
#include "SSequencer.h"
#include "MovieSceneNameableTrack.h"
#include "ISequencerTrackEditor.h"
#include "ScopedTransaction.h"
#include "SequencerUtilities.h"
#include "SKeyNavigationButtons.h"
#include "Compilation/MovieSceneSegmentCompiler.h"
#include "SequencerDisplayNodeDragDropOp.h"
#include "SequencerFolderNode.h"
#include "SequencerNodeSortingMethods.h"
#include "SequencerNodeTree.h"
#include "DisplayNodes/SequencerFolderNode.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "MovieSceneFolder.h"

#define LOCTEXT_NAMESPACE "SequencerTrackNode"

namespace SequencerNodeConstants
{
	extern const float CommonPadding;
}

bool ContainsKeyableArea(TSharedRef<FSequencerSectionKeyAreaNode> InKeyAreaNode)
{
	for (const TSharedRef<IKeyArea>& KeyArea : InKeyAreaNode->GetAllKeyAreas())
	{
		if (KeyArea->CanCreateKeyEditor())
		{
			return true;
		}
	}
	return false;
}

/* FTrackNode structors
 *****************************************************************************/

FSequencerTrackNode::FSequencerTrackNode(UMovieSceneTrack& InAssociatedTrack, ISequencerTrackEditor& InAssociatedEditor, bool bInCanBeDragged, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree)
	: FSequencerDisplayNode(InAssociatedTrack.GetFName(), InParentNode, InParentTree)
	, AssociatedEditor(InAssociatedEditor)
	, AssociatedTrack(&InAssociatedTrack)
	, bCanBeDragged(bInCanBeDragged)
	, SubTrackMode(ESubTrackMode::None)
{ }


/* FTrackNode interface
 *****************************************************************************/

void FSequencerTrackNode::SetSectionAsKeyArea(TSharedRef<IKeyArea>& KeyArea)
{
	if( !TopLevelKeyNode.IsValid() )
	{
		bool bTopLevel = true;
		TopLevelKeyNode = MakeShareable( new FSequencerSectionKeyAreaNode( GetNodeName(), FText::GetEmpty(), SharedThis(this), ParentTree, bTopLevel ) );
	}

	TopLevelKeyNode->AddKeyArea( KeyArea );
}

void FSequencerTrackNode::AddKey(const FGuid& ObjectGuid)
{
	AssociatedEditor.AddKey(ObjectGuid);
}

FSequencerTrackNode::ESubTrackMode FSequencerTrackNode::GetSubTrackMode() const
{
	return SubTrackMode;
}

void FSequencerTrackNode::SetSubTrackMode(FSequencerTrackNode::ESubTrackMode InSubTrackMode)
{
	SubTrackMode = InSubTrackMode;
}

int32 FSequencerTrackNode::GetRowIndex() const
{
	return RowIndex;
}

void FSequencerTrackNode::SetRowIndex(int32 InRowIndex)
{
	RowIndex = InRowIndex;
	NodeName.SetNumber(RowIndex);
}


/* FSequencerDisplayNode interface
 *****************************************************************************/

void FSequencerTrackNode::BuildContextMenu(FMenuBuilder& MenuBuilder)
{
	AssociatedEditor.BuildTrackContextMenu(MenuBuilder, AssociatedTrack.Get());
	UMovieSceneTrack* Track = AssociatedTrack.Get();
	if (Track && Track->GetSupportedBlendTypes().Num() > 0)
	{
		int32 NewRowIndex = SubTrackMode == ESubTrackMode::SubTrack ? GetRowIndex() : Track->GetMaxRowIndex() + 1;
		TWeakPtr<ISequencer> WeakSequencer = GetSequencer().AsShared();

		MenuBuilder.AddSubMenu(
			LOCTEXT("AddSection", "Add Section"),
			FText(),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){
				FSequencerUtilities::PopulateMenu_CreateNewSection(SubMenuBuilder, NewRowIndex, Track, WeakSequencer);
			})
		);
		
	}
	FSequencerDisplayNode::BuildContextMenu(MenuBuilder );
}


bool FSequencerTrackNode::CanRenameNode() const
{
	auto NameableTrack = Cast<UMovieSceneNameableTrack>(AssociatedTrack.Get());

	if (NameableTrack != nullptr)
	{
		return NameableTrack->CanRename();
	}
	return false;
}


FReply FSequencerTrackNode::CreateNewSection() const
{
	UMovieSceneTrack* Track = GetTrack();
	if (!Track)
	{
		return FReply::Handled();
	}

	const int32 InsertAtIndex = SubTrackMode == ESubTrackMode::SubTrack ? GetRowIndex() : Track->GetMaxRowIndex() + 1;
	const FQualifiedFrameTime CurrentTime = GetSequencer().GetLocalTime();

	FScopedTransaction Transaction(LOCTEXT("AddSectionText", "Add Section"));
	UMovieSceneSection* Section = Track->CreateNewSection();
	if (Section)
	{
		Track->Modify();

		FFrameNumber Duration = (10.f * CurrentTime.Rate).RoundToFrame();
		Section->SetRange(TRange<FFrameNumber>(CurrentTime.Time.FrameNumber, CurrentTime.Time.FrameNumber + Duration));
		Section->SetRowIndex(InsertAtIndex);

		Track->AddSection(*Section);

		GetSequencer().NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	}
	else
	{
		Transaction.Cancel();
	}
	return FReply::Handled();
}


TSharedRef<SWidget> FSequencerTrackNode::GetCustomOutlinerContent()
{
	TSharedPtr<FSequencerSectionKeyAreaNode> KeyAreaNode = GetTopLevelKeyNode();
	TSharedPtr<SWidget> KeyEditorWidget;
	if (KeyAreaNode.IsValid())
	{
		KeyEditorWidget = KeyAreaNode->GetOrCreateKeyAreaEditorSwitcher();
	}

	TAttribute<bool> NodeIsHovered = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FSequencerDisplayNode::IsHovered));

	TSharedRef<SHorizontalBox> BoxPanel = SNew(SHorizontalBox);

	FGuid ObjectBinding;
	TSharedPtr<FSequencerDisplayNode> ParentSeqNode = GetParent();

	if (ParentSeqNode.IsValid() && (ParentSeqNode->GetType() == ESequencerNode::Object))
	{
		ObjectBinding = StaticCastSharedPtr<FSequencerObjectBindingNode>(ParentSeqNode)->GetObjectBinding();
	}

	UMovieSceneTrack* Track = AssociatedTrack.Get();

	FBuildEditWidgetParams Params;
	Params.NodeIsHovered = NodeIsHovered;
	if (SubTrackMode == ESubTrackMode::SubTrack)
	{
		Params.TrackInsertRowIndex = GetRowIndex();
	}
	else if (Track->SupportsMultipleRows())
	{
		Params.TrackInsertRowIndex = Track->GetMaxRowIndex()+1;
	}

	TSharedPtr<SWidget> CustomWidget = AssociatedEditor.BuildOutlinerEditWidget(ObjectBinding, Track, Params);

	if (KeyEditorWidget.IsValid())
	{
		TSharedRef<SOverlay> Overlay = SNew(SOverlay);

		Overlay->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			KeyEditorWidget.ToSharedRef()
		];

		if (CustomWidget.IsValid())
		{
			Overlay->AddSlot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				CustomWidget.ToSharedRef()
			];
		}

		BoxPanel->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			Overlay
		];

		BoxPanel->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SKeyNavigationButtons, KeyAreaNode.ToSharedRef())
		];
	}

	else
	{
		if (CustomWidget.IsValid())
		{
			BoxPanel->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				CustomWidget.ToSharedRef()
			];
		}

		TArray<TSharedRef<FSequencerSectionKeyAreaNode>> ChildKeyAreaNodes;
		FSequencerDisplayNode::GetChildKeyAreaNodesRecursively(ChildKeyAreaNodes);

		if (ChildKeyAreaNodes.FindByPredicate(ContainsKeyableArea))
		{
			BoxPanel->AddSlot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SKeyNavigationButtons, AsShared())
			];
		}
	}

	return SNew(SBox)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		[
			BoxPanel
		];
}


const FSlateBrush* FSequencerTrackNode::GetIconBrush() const
{
	return AssociatedEditor.GetIconBrush();
}


bool FSequencerTrackNode::CanDrag() const
{
	return bCanBeDragged && SubTrackMode != ESubTrackMode::SubTrack;
}

TOptional<EItemDropZone> FSequencerTrackNode::CanDrop(FSequencerDisplayNodeDragDropOp& DragDropOp, EItemDropZone ItemDropZone) const
{
	DragDropOp.ResetToDefaultToolTip();

	// Prevent taking any parent that's part of the dragged node hierarchy from being put inside a child of itself
	// This is done first before the other checks so that the UI stays consistent as you move between them, otherwise
	// when you are above/below a node it reports this error, but if you were on top of a node it would do the standard
	// no-drag-drop due to OntoItem being blocked. 
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

	// If we can't be dragged, then we don't allow reordering things above or below us.
	if (!CanDrag() || SubTrackMode == ESubTrackMode::SubTrack)
	{
		return TOptional<EItemDropZone>();
	}

	// Since tracks can't have children (via the UI) any attempts to drop below or onto them get rerouted into
	// attempts to drop them above to help with the insert marker drawing in a confusing manner for multi-row children.
	if (ItemDropZone == EItemDropZone::BelowItem || ItemDropZone == EItemDropZone::OntoItem)
	{
		ItemDropZone = EItemDropZone::AboveItem;
	}

	TArray<UMovieSceneFolder*> AdjacentFolders;
	if (GetParent().IsValid())
	{
		// We are either trying to drop adjacent to ourself (when nestled), or as a child of ourself, so we add either our siblings or our children
		// to the list of possibly conflicting names.
		for (TSharedRef <FSequencerDisplayNode> Child : GetParent()->GetChildNodes())
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

	TSharedPtr<FSequencerDisplayNode> ParentSeqNode = GetParent();
	if (ParentSeqNode.IsValid())
	{
		if (ParentSeqNode->GetType() == ESequencerNode::Folder)
		{
			return TOptional<EItemDropZone>(ItemDropZone);
		}
		else
		{
			// If we have a parent who is not a folder (ie: This is a component track on an actor)
			// then it can't be rearranged.
			return TOptional<EItemDropZone>();
		}
	}
	else
	{
		// We're at the root level and thus a Master track, so they can re-arrange.
		return TOptional<EItemDropZone>(ItemDropZone);
	}
}

void FSequencerTrackNode::Drop(const TArray<TSharedRef<FSequencerDisplayNode>>& DraggedNodes, EItemDropZone ItemDropZone)
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

bool FSequencerTrackNode::IsResizable() const
{
	UMovieSceneTrack* Track = GetTrack();
	return Track && AssociatedEditor.IsResizable(Track);
}

void FSequencerTrackNode::Resize(float NewSize)
{
	UMovieSceneTrack* Track = GetTrack();

	float PaddingAmount = 2 * SequencerNodeConstants::CommonPadding;
	if (Track && Sections.Num())
	{
		PaddingAmount *= (Track->GetMaxRowIndex() + 1);
	}
	
	NewSize -= PaddingAmount;

	if (Track && AssociatedEditor.IsResizable(Track))
	{
		AssociatedEditor.Resize(NewSize, Track);
	}
}

void FSequencerTrackNode::GetChildKeyAreaNodesRecursively(TArray<TSharedRef<FSequencerSectionKeyAreaNode>>& OutNodes) const
{
	FSequencerDisplayNode::GetChildKeyAreaNodesRecursively(OutNodes);

	if (TopLevelKeyNode.IsValid())
	{
		OutNodes.Add(TopLevelKeyNode.ToSharedRef());
	}
}


FText FSequencerTrackNode::GetDisplayName() const
{
	return AssociatedTrack.IsValid() ? AssociatedTrack->GetDisplayName() : FText::GetEmpty();
}


float FSequencerTrackNode::GetNodeHeight() const
{
	float SectionHeight = Sections.Num() > 0
		? Sections[0]->GetSectionHeight()
		: SequencerLayoutConstants::SectionAreaDefaultHeight;
	float PaddedSectionHeight = SectionHeight + (2 * SequencerNodeConstants::CommonPadding);

	if (SubTrackMode == ESubTrackMode::None && AssociatedTrack.IsValid())
	{
		return PaddedSectionHeight * (AssociatedTrack->GetMaxRowIndex() + 1);
	}
	else
	{
		return PaddedSectionHeight;
	}
}


FNodePadding FSequencerTrackNode::GetNodePadding() const
{
	return FNodePadding(0.f);
}


ESequencerNode::Type FSequencerTrackNode::GetType() const
{
	return ESequencerNode::Track;
}


void FSequencerTrackNode::SetDisplayName(const FText& NewDisplayName)
{
	auto NameableTrack = Cast<UMovieSceneNameableTrack>(AssociatedTrack.Get());

	if (NameableTrack != nullptr && !NameableTrack->GetDisplayName().EqualTo(NewDisplayName))
	{
		const FScopedTransaction Transaction(NSLOCTEXT("SequencerTrackNode", "RenameTrack", "Rename Track"));

		NameableTrack->SetDisplayName(NewDisplayName);
		GetSequencer().NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
	}
}

TArray<FSequencerOverlapRange> FSequencerTrackNode::GetUnderlappingSections(UMovieSceneSection* InSection)
{
	TRange<FFrameNumber> InSectionRange = InSection->GetRange();

	TMovieSceneEvaluationTree<int32> SectionIndexTree;

	// Iterate all other sections on the same row with <= overlap priority
	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		UMovieSceneSection* SectionObj = Sections[SectionIndex]->GetSectionObject();
		if (!SectionObj || SectionObj == InSection || SectionObj->GetRowIndex() != InSection->GetRowIndex() || SectionObj->GetOverlapPriority() > InSection->GetOverlapPriority())
		{
			continue;
		}

		TRange<FFrameNumber> OtherSectionRange = SectionObj->GetRange();
		TRange<FFrameNumber> Intersection = TRange<FFrameNumber>::Intersection(OtherSectionRange, InSectionRange);
		if (!Intersection.IsEmpty())
		{
			SectionIndexTree.Add(Intersection, SectionIndex);
		}
	}

	TSharedRef<FSequencerTrackNode> TrackNode = SharedThis(this);;

	TArray<FSequencerOverlapRange> Result;
	for (FMovieSceneEvaluationTreeRangeIterator It(SectionIndexTree); It; ++It)
	{
		FSequencerOverlapRange NewRange;

		NewRange.Range = It.Range();

		for (int32 SectionIndex : SectionIndexTree.GetAllData(It.Node()))
		{
			NewRange.Sections.Add(FSectionHandle(TrackNode, SectionIndex));
		}

		if (!NewRange.Sections.Num())
		{
			continue;
		}

		// Sort lowest to highest
		NewRange.Sections.Sort([](const FSectionHandle& A, const FSectionHandle& B){
			return A.GetSectionObject()->GetOverlapPriority() < B.GetSectionObject()->GetOverlapPriority();
		});

		Result.Add(MoveTemp(NewRange));
	}

	return Result;
}

TArray<FSequencerOverlapRange> FSequencerTrackNode::GetEasingSegmentsForSection(UMovieSceneSection* InSection)
{
	TRange<FFrameNumber> InSectionRange = InSection->GetRange();

	TArray<FMovieSceneSectionData> CompileData;

	TMovieSceneEvaluationTree<int32> SectionIndexTree;

	// Iterate all active sections on the same row with <= overlap priority
	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		UMovieSceneSection* SectionObj = Sections[SectionIndex]->GetSectionObject();
		if (!SectionObj || !SectionObj->IsActive() || SectionObj->GetRowIndex() != InSection->GetRowIndex() || SectionObj->GetOverlapPriority() > InSection->GetOverlapPriority())
		{
			continue;
		}

		TRange<FFrameNumber> Intersection = TRange<FFrameNumber>::Intersection(SectionObj->GetEaseInRange(), InSectionRange);
		if (!Intersection.IsEmpty())
		{
			SectionIndexTree.Add(Intersection, SectionIndex);
		}

		Intersection = TRange<FFrameNumber>::Intersection(SectionObj->GetEaseOutRange(), InSectionRange);
		if (!Intersection.IsEmpty())
		{
			SectionIndexTree.Add(Intersection, SectionIndex);
		}
	}

	TSharedRef<FSequencerTrackNode> TrackNode = SharedThis(this);;

	TArray<FSequencerOverlapRange> Result;
	for (FMovieSceneEvaluationTreeRangeIterator It(SectionIndexTree); It; ++It)
	{
		FSequencerOverlapRange NewRange;

		NewRange.Range = It.Range();

		for (int32 SectionIndex : SectionIndexTree.GetAllData(It.Node()))
		{
			NewRange.Sections.Add(FSectionHandle(TrackNode, SectionIndex));
		}

		if (!NewRange.Sections.Num())
		{
			continue;
		}

		// Sort lowest to highest
		NewRange.Sections.Sort([=](const FSectionHandle& A, const FSectionHandle& B){
			return A.GetSectionObject()->GetOverlapPriority() < B.GetSectionObject()->GetOverlapPriority();
		});

		Result.Add(MoveTemp(NewRange));
	}

	return Result;
}

int32 FSequencerTrackNode::GetSortingOrder() const
{
	const UMovieSceneTrack* MovieSceneTrack = AssociatedTrack.Get();
	if (MovieSceneTrack)
	{
		return MovieSceneTrack->GetSortingOrder();
	}

	return 0;
}

void FSequencerTrackNode::SetSortingOrder(const int32 InSortingOrder)
{
	UMovieSceneTrack* MovieSceneTrack = AssociatedTrack.Get();
	if (MovieSceneTrack)
	{
		MovieSceneTrack->SetSortingOrder(InSortingOrder);
	}
}

void FSequencerTrackNode::ModifyAndSetSortingOrder(const int32 InSortingOrder)
{
	UMovieSceneTrack* MovieSceneTrack = AssociatedTrack.Get();
	if (MovieSceneTrack)
	{
		MovieSceneTrack->Modify();
		SetSortingOrder(InSortingOrder);
	}
}
#undef LOCTEXT_NAMESPACE