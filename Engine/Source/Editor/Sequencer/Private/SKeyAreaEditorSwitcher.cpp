// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SKeyAreaEditorSwitcher.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Sequencer.h"
#include "SKeyNavigationButtons.h"
#include "IKeyArea.h"
#include "DisplayNodes/SequencerSectionKeyAreaNode.h"

void SKeyAreaEditorSwitcher::Construct(const FArguments& InArgs, TSharedRef<FSequencerSectionKeyAreaNode> InKeyAreaNode)
{
	WeakKeyAreaNode = InKeyAreaNode;
	ChildSlot
	[
		SAssignNew(Overlay, SOverlay)
		.IsEnabled(!InKeyAreaNode->GetSequencer().IsReadOnly())
	];

	Rebuild();
}

void SKeyAreaEditorSwitcher::Rebuild()
{
	Overlay->ClearChildren();
	VisibleIndex = -1;

	TSharedPtr<FSequencerSectionKeyAreaNode> KeyAreaNode = WeakKeyAreaNode.Pin();
	if (!KeyAreaNode.IsValid())
	{
		return;
	}

	TSharedPtr<FSequencerObjectBindingNode> ParentObjectBinding = KeyAreaNode->FindParentObjectBindingNode();
	FGuid ObjectBindingID = ParentObjectBinding.IsValid() ? ParentObjectBinding->GetObjectBinding() : FGuid();

	for (int32 Index = 0; Index < KeyAreaNode->GetAllKeyAreas().Num(); ++Index)
	{
		TSharedRef<IKeyArea> KeyArea = KeyAreaNode->GetAllKeyAreas()[Index];

		if (KeyArea->CanCreateKeyEditor())
		{
			Overlay->AddSlot()
			[
				SNew(SBox)
				.IsEnabled(!KeyAreaNode->GetSequencer().IsReadOnly())
				.WidthOverride(100)
				.HAlign(HAlign_Left)
				.Visibility(this, &SKeyAreaEditorSwitcher::GetWidgetVisibility, Index)
				[
					KeyArea->CreateKeyEditor(KeyAreaNode->GetSequencer().AsShared(), ObjectBindingID)
				]
			];
		}
	}
}

EVisibility SKeyAreaEditorSwitcher::GetWidgetVisibility(int32 Index) const
{
	return Index == VisibleIndex ? EVisibility::Visible : EVisibility::Collapsed;
}

void SKeyAreaEditorSwitcher::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	VisibleIndex = -1;

	TSharedPtr<FSequencerSectionKeyAreaNode> KeyAreaNode = WeakKeyAreaNode.Pin();
	if (!KeyAreaNode.IsValid() || KeyAreaNode->GetAllKeyAreas().Num() == 0)
	{
		return;
	}

	TArray<UMovieSceneSection*> AllSections;
	for (const TSharedRef<IKeyArea>& KeyArea : KeyAreaNode->GetAllKeyAreas())
	{
		AllSections.Add(KeyArea->GetOwningSection());
	}

	VisibleIndex = SequencerHelpers::GetSectionFromTime(AllSections, KeyAreaNode->GetSequencer().GetLocalTime().Time.FrameNumber);
}
