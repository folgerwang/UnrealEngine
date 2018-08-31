// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayNodes/SequencerSectionKeyAreaNode.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "SSequencer.h"
#include "IKeyArea.h"
#include "SKeyNavigationButtons.h"
#include "SKeyAreaEditorSwitcher.h"


/* FSectionKeyAreaNode interface
 *****************************************************************************/

FSequencerSectionKeyAreaNode::FSequencerSectionKeyAreaNode(FName NodeName, const FText& InDisplayName, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree, bool bInTopLevel)
	: FSequencerDisplayNode(NodeName, InParentNode, InParentTree)
	, DisplayName(InDisplayName)
	, bTopLevel(bInTopLevel)
{
}

void FSequencerSectionKeyAreaNode::AddKeyArea(TSharedRef<IKeyArea> KeyArea)
{
	KeyAreas.Add(KeyArea);

	if (KeyEditorSwitcher.IsValid())
	{
		KeyEditorSwitcher->Rebuild();
	}
}

TSharedPtr<IKeyArea> FSequencerSectionKeyAreaNode::GetKeyArea(UMovieSceneSection* Section) const
{
	for (const TSharedRef<IKeyArea>& KeyArea : KeyAreas)
	{
		if (KeyArea->GetOwningSection() == Section)
		{
			return KeyArea;
		}
	}
	return nullptr;
}

/* FSequencerDisplayNode interface
 *****************************************************************************/

bool FSequencerSectionKeyAreaNode::CanRenameNode() const
{
	return false;
}

TSharedRef<SWidget> FSequencerSectionKeyAreaNode::GetOrCreateKeyAreaEditorSwitcher()
{
	if (!KeyEditorSwitcher.IsValid())
	{
		KeyEditorSwitcher = SNew(SKeyAreaEditorSwitcher, SharedThis(this));
	}
	return KeyEditorSwitcher.ToSharedRef();
}

TSharedRef<SWidget> FSequencerSectionKeyAreaNode::GetCustomOutlinerContent()
{
	if (GetAllKeyAreas().Num() > 0)
	{
		return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			GetOrCreateKeyAreaEditorSwitcher()
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SKeyNavigationButtons, SharedThis(this))
		];
	}

	return FSequencerDisplayNode::GetCustomOutlinerContent();
}


FText FSequencerSectionKeyAreaNode::GetDisplayName() const
{
	return DisplayName;
}


float FSequencerSectionKeyAreaNode::GetNodeHeight() const
{
	//@todo sequencer: should be defined by the key area probably
	return SequencerLayoutConstants::KeyAreaHeight;
}


FNodePadding FSequencerSectionKeyAreaNode::GetNodePadding() const
{
	return FNodePadding(0.f);//FNodePadding(0.f, 1.f);
}


ESequencerNode::Type FSequencerSectionKeyAreaNode::GetType() const
{
	return ESequencerNode::KeyArea;
}


void FSequencerSectionKeyAreaNode::SetDisplayName(const FText& NewDisplayName)
{
	check(false);
}
