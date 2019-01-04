// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "SGraphPreviewer.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "SGraphPanel.h"

EActiveTimerReturnType SGraphPreviewer::RefreshGraphTimer(const double InCurrentTime, const float InDeltaTime)
{
	if (NeedsRefreshCounter > 0)
	{
		GraphPanel->ZoomToFit(false);
		NeedsRefreshCounter--;
		return EActiveTimerReturnType::Continue;
	}
	else
	{
		return EActiveTimerReturnType::Stop;
	}
}

void SGraphPreviewer::Construct( const FArguments& InArgs, UEdGraph* InGraphObj )
{
	EdGraphObj = InGraphObj;
	NeedsRefreshCounter = 2;

	TSharedPtr<SOverlay> DisplayStack;

	this->ChildSlot
	[
		SAssignNew(DisplayStack, SOverlay)

		// The graph panel
		+SOverlay::Slot()
		[
			SAssignNew(GraphPanel, SGraphPanel)
			.GraphObj( EdGraphObj )
			.IsEditable( false )
			.ShowGraphStateOverlay(InArgs._ShowGraphStateOverlay)
			.InitialZoomToFit( true )
		]

		// Bottom-right corner text indicating the type of tool
		+SOverlay::Slot()
		.Padding(4)
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Right)
		[
			SNew(STextBlock)
			.Visibility( EVisibility::HitTestInvisible )
			.TextStyle( FEditorStyle::Get(), "GraphPreview.CornerText" )
			.Text( InArgs._CornerOverlayText )
		]
	];

	GraphPanel->Update();

	// Add the title bar if specified
	if (InArgs._TitleBar.IsValid())
	{
		DisplayStack->AddSlot()
			.VAlign(VAlign_Top)
			[
				InArgs._TitleBar.ToSharedRef()
			];
	}

	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SGraphPreviewer::RefreshGraphTimer));
}
