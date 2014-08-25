// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "IntroTutorialsPrivatePCH.h"
#include "STutorialOverlay.h"
#include "STutorialContent.h"
#include "EditorTutorial.h"
#include "IntroTutorials.h"
#include "LevelEditor.h"

void STutorialOverlay::Construct(const FArguments& InArgs, FTutorialStage* const InStage)
{
	ParentWindow = InArgs._ParentWindow;
	bIsStandalone = InArgs._IsStandalone;
	OnClosed = InArgs._OnClosed;

	// Setup the map for opening of closed tabs by highlighted widgets
	AddTabInfo();

	TSharedPtr<SOverlay> Overlay;

	ChildSlot
	[
		SAssignNew(Overlay, SOverlay)
		+SOverlay::Slot()
		[
			SAssignNew(OverlayCanvas, SCanvas)
		]
	];

	if(InStage != nullptr)
	{
		// add non-widget content, if any
		if(InArgs._AllowNonWidgetContent && InStage->Content.Type != ETutorialContent::None)
		{
			Overlay->AddSlot()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(STutorialContent, InStage->Content)
					.OnClosed(InArgs._OnClosed)
					.IsStandalone(InArgs._IsStandalone)
					.WrapTextAt(600.0f)
				]
			];
		}

		if(InStage->WidgetContent.Num() > 0)
		{
			FIntroTutorials& IntroTutorials = FModuleManager::Get().GetModuleChecked<FIntroTutorials>("IntroTutorials");

			// now add canvas slots for widget-bound content
			for(const FTutorialWidgetContent& WidgetContent : InStage->WidgetContent)
			{
				if(WidgetContent.Content.Type != ETutorialContent::None )
				{
					const bool bEmptyText = (WidgetContent.Content.Type == ETutorialContent::Text || WidgetContent.Content.Type == ETutorialContent::RichText) && WidgetContent.Content.Text.IsEmpty();
					if(!bEmptyText)
					{
						TSharedPtr<STutorialContent> ContentWidget = 
							SNew(STutorialContent, WidgetContent.Content)
							.HAlign(WidgetContent.HorizontalAlignment)
							.VAlign(WidgetContent.VerticalAlignment)
							.Offset(WidgetContent.Offset)
							.IsStandalone(bIsStandalone)
							.OnClosed(OnClosed)
							.WrapTextAt(WidgetContent.ContentWidth)
							.Anchor(WidgetContent.WidgetAnchor);
						OpenBrowserForWidgetAnchor(WidgetContent);

						OverlayCanvas->AddSlot()
						.Position(TAttribute<FVector2D>::Create(TAttribute<FVector2D>::FGetter::CreateSP(ContentWidget.Get(), &STutorialContent::GetPosition)))
						.Size(TAttribute<FVector2D>::Create(TAttribute<FVector2D>::FGetter::CreateSP(ContentWidget.Get(), &STutorialContent::GetSize)))
						[
							ContentWidget.ToSharedRef()
						];

						OnPaintNamedWidget.AddSP(ContentWidget.Get(), &STutorialContent::HandlePaintNamedWidget);
						OnResetNamedWidget.AddSP(ContentWidget.Get(), &STutorialContent::HandleResetNamedWidget);
						OnCacheWindowSize.AddSP(ContentWidget.Get(), &STutorialContent::HandleCacheWindowSize);
					}
				}
			}
		}
	}
}

int32 STutorialOverlay::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	if(ParentWindow.IsValid())
	{
		TSharedPtr<SWindow> PinnedWindow = ParentWindow.Pin();
		OnResetNamedWidget.Broadcast();
		OnCacheWindowSize.Broadcast(PinnedWindow->GetWindowGeometryInWindow().Size);
		LayerId = TraverseWidgets(PinnedWindow.ToSharedRef(), PinnedWindow->GetWindowGeometryInWindow(), MyClippingRect, OutDrawElements, LayerId);
	}
	
	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

int32 STutorialOverlay::TraverseWidgets(TSharedRef<SWidget> InWidget, const FGeometry& InGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	const FName Tag = InWidget->GetTag();
	if(Tag != NAME_None)
	{
		// we are a named widget - ask it to draw
		OnPaintNamedWidget.Broadcast(InWidget, InGeometry);

		// if we are picking, we need to draw an outline here
		FName WidgetNameToHighlight = NAME_None;
		bool bIsPicking = false;
		FIntroTutorials& IntroTutorials = FModuleManager::Get().GetModuleChecked<FIntroTutorials>("IntroTutorials");
		if(IntroTutorials.OnIsPicking().IsBound())
		{
			bIsPicking = IntroTutorials.OnIsPicking().Execute(WidgetNameToHighlight);
		}
	
		if(WidgetNameToHighlight != NAME_None || bIsPicking)
		{
			const bool bHighlight = WidgetNameToHighlight != NAME_None && WidgetNameToHighlight == Tag;
			if(bIsPicking || (!bIsPicking && bHighlight))
			{
				const FLinearColor Color = bIsPicking && bHighlight ? FLinearColor::Green : FLinearColor::White;
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId++, InGeometry.ToPaintGeometry(), FCoreStyle::Get().GetBrush(TEXT("Debug.Border")), MyClippingRect, ESlateDrawEffect::None, Color);
			}
		}	
	}

	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	InWidget->ArrangeChildren(InGeometry, ArrangedChildren);
	for(int32 ChildIndex = 0; ChildIndex < ArrangedChildren.Num(); ChildIndex++)
	{
		const FArrangedWidget& ArrangedWidget = ArrangedChildren[ChildIndex];
		LayerId = TraverseWidgets(ArrangedWidget.Widget, ArrangedWidget.Geometry, MyClippingRect, OutDrawElements, LayerId);
	}

	return LayerId;
}

void STutorialOverlay::OpenBrowserForWidgetAnchor(const FTutorialWidgetContent &WidgetContent)
{
	FString IdentString = WidgetContent.WidgetAnchor.WrapperIdentifier.ToString();
	FString TabString = "";
		
	// See if we have a mapping for the widget ident
	for (TMap<FString, FString>::TConstIterator It(BrowserTabMap); It; ++It)
	{
		if (It.Key().StartsWith(IdentString) == true)
		{
			TabString = It.Value();
		}
	}

	// Open the required tab if we found it in the map
	if (TabString.IsEmpty() == false)
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
		LevelEditorTabManager->InvokeTab(FName(*TabString));
	}
	
}

void STutorialOverlay::AddTabInfo()
{
	BrowserTabMap.Empty();
	BrowserTabMap.Add(FString(TEXT("ActorDetails")), FString(TEXT("LevelEditorSelectionDetails")));
	BrowserTabMap.Add(FString(TEXT("SceneOutliner")), FString(TEXT("LevelEditorSceneOutliner")));
	BrowserTabMap.Add(FString(TEXT("ContentBrowser")), FString(TEXT("ContentBrowserTab1")));
	BrowserTabMap.Add(FString(TEXT("ToolsPanel")), FString(TEXT("LevelEditorToolBox")));
	BrowserTabMap.Add(FString(TEXT("WorldSettings")), FString(TEXT("WorldSettingsTab")));
	BrowserTabMap.Add(FString(TEXT("EditorViewports")), FString(TEXT("LevelEditorViewport")));	 
	BrowserTabMap.Add(FString(TEXT("LayerBrowser")), FString(TEXT("LevelEditorLayerBrowser")));	
}
