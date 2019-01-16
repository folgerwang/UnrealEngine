// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorDetailCustomization_ProceduralBrushStack.h"
#include "IDetailChildrenBuilder.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Brushes/SlateColorBrush.h"
#include "Layout/WidgetPath.h"
#include "SlateOptMacros.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"

#include "ScopedTransaction.h"

#include "LandscapeEditorDetailCustomization_TargetLayers.h"
#include "Widgets/Input/SEditableText.h"
#include "LandscapeBPCustomBrush.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor.Layers"

TSharedRef<IDetailCustomization> FLandscapeEditorDetailCustomization_ProceduralBrushStack::MakeInstance()
{
	return MakeShareable(new FLandscapeEditorDetailCustomization_ProceduralBrushStack);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FLandscapeEditorDetailCustomization_ProceduralBrushStack::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& LayerCategory = DetailBuilder.EditCategory("Current Layer Brushes");

	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentToolMode != nullptr)
	{
		const FName CurrentToolName = LandscapeEdMode->CurrentTool->GetToolName();

		if (LandscapeEdMode->CurrentToolMode->SupportedTargetTypes != 0 && CurrentToolName == TEXT("BPCustom"))
		{
			LayerCategory.AddCustomBuilder(MakeShareable(new FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack(DetailBuilder.GetThumbnailPool().ToSharedRef())));
		}
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

//////////////////////////////////////////////////////////////////////////

FEdModeLandscape* FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::GetEditorMode()
{
	return (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
}

FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack(TSharedRef<FAssetThumbnailPool> InThumbnailPool)
	: ThumbnailPool(InThumbnailPool)
{
}

FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::~FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack()
{
	
}

void FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
{
}

void FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	
	if (LandscapeEdMode == NULL)
	{
		return;	
	}

	NodeRow.NameWidget
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(FText::FromString(TEXT("Stack")))
		];
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		TSharedPtr<SDragAndDropVerticalBox> BrushesList = SNew(SDragAndDropVerticalBox)
			.OnCanAcceptDrop(this, &FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::HandleCanAcceptDrop)
			.OnAcceptDrop(this, &FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::HandleAcceptDrop)
			.OnDragDetected(this, &FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::HandleDragDetected);

		BrushesList->SetDropIndicator_Above(*FEditorStyle::GetBrush("LandscapeEditor.TargetList.DropZone.Above"));
		BrushesList->SetDropIndicator_Below(*FEditorStyle::GetBrush("LandscapeEditor.TargetList.DropZone.Below"));

		ChildrenBuilder.AddCustomRow(FText::FromString(FString(TEXT("Brush Stack"))))
			.Visibility(EVisibility::Visible)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Center)
				.Padding(0, 2)
				[
					BrushesList.ToSharedRef()
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Center)
				.Padding(0, 2)
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					//.Padding(4, 0)
					[
						SNew(SButton)
						.Text(this, &FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::GetCommitBrushesButtonText)
						.OnClicked(this, &FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::ToggleCommitBrushes)
						.IsEnabled(this, &FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::IsCommitBrushesButtonEnabled)
					]
				]
			];

		if (LandscapeEdMode->CurrentToolMode != nullptr)
		{
			const TArray<int8>& BrushOrderStack = LandscapeEdMode->GetBrushesOrderForCurrentProceduralLayer(LandscapeEdMode->CurrentToolTarget.TargetType);

			for (int32 i = 0; i < BrushOrderStack.Num(); ++i)
			{
				TSharedPtr<SWidget> GeneratedRowWidget = GenerateRow(i);

				if (GeneratedRowWidget.IsValid())
				{
					BrushesList->AddSlot()
						.AutoHeight()
						[
							GeneratedRowWidget.ToSharedRef()
						];
				}
			}
		}
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedPtr<SWidget> FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::GenerateRow(int32 InBrushIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	TSharedPtr<SWidget> RowWidget = SNew(SLandscapeEditorSelectableBorder)
		.Padding(0)
		.VAlign(VAlign_Center)
		.OnSelected(this, &FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::OnBrushSelectionChanged, InBrushIndex)
		.IsSelected(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::IsBrushSelected, InBrushIndex)))
		[	
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(4, 0)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Center)
				.Padding(0, 2)
				[
					SNew(STextBlock)
					.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::GetBrushTextColor, InBrushIndex)))
					.Text(this, &FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::GetBrushText, InBrushIndex)
				]
			]
		];
	
	return RowWidget;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

bool FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::IsBrushSelected(int32 InBrushIndex) const
{
	ALandscapeBlueprintCustomBrush* Brush = GetBrush(InBrushIndex);

	return Brush != nullptr ? Brush->IsSelected() : false;
}

void FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::OnBrushSelectionChanged(int32 InBrushIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();

	if (LandscapeEdMode != nullptr && LandscapeEdMode->AreAllBrushesCommitedToCurrentProceduralLayer(LandscapeEdMode->CurrentToolTarget.TargetType))
	{
		return;
	}

	ALandscapeBlueprintCustomBrush* Brush = GetBrush(InBrushIndex);

	if (Brush != nullptr && !Brush->IsCommited())
	{
		GEditor->SelectNone(true, true);
		GEditor->SelectActor(Brush, true, true);
	}
}

FText FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::GetBrushText(int32 InBrushIndex) const
{
	ALandscapeBlueprintCustomBrush* Brush = GetBrush(InBrushIndex);

	if (Brush != nullptr)
	{
		return FText::FromString(Brush->GetActorLabel());
	}

	return FText::FromName(NAME_None);
}

FSlateColor FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::GetBrushTextColor(int32 InBrushIndex) const
{
	ALandscapeBlueprintCustomBrush* Brush = GetBrush(InBrushIndex);

	if (Brush != nullptr)
	{
		return Brush->IsCommited() ? FSlateColor::UseSubduedForeground() : FSlateColor::UseForeground();
	}

	return FSlateColor::UseSubduedForeground();
}

ALandscapeBlueprintCustomBrush* FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::GetBrush(int32 InBrushIndex) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();

	if (LandscapeEdMode != nullptr)
	{
		return LandscapeEdMode->GetBrushForCurrentProceduralLayer(LandscapeEdMode->CurrentToolTarget.TargetType, InBrushIndex);
	}

	return nullptr;
}

FReply FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::ToggleCommitBrushes()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();

	if (LandscapeEdMode != nullptr)
	{
		bool CommitBrushes = !LandscapeEdMode->AreAllBrushesCommitedToCurrentProceduralLayer(LandscapeEdMode->CurrentToolTarget.TargetType);

		if (CommitBrushes)
		{
			TArray<ALandscapeBlueprintCustomBrush*> BrushStack = LandscapeEdMode->GetBrushesForCurrentProceduralLayer(LandscapeEdMode->CurrentToolTarget.TargetType);

			for (ALandscapeBlueprintCustomBrush* Brush : BrushStack)
			{
				GEditor->SelectActor(Brush, false, true);
			}
		}

		LandscapeEdMode->SetCurrentProceduralLayerBrushesCommitState(LandscapeEdMode->CurrentToolTarget.TargetType, CommitBrushes);
	}

	return FReply::Handled();
}

bool FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::IsCommitBrushesButtonEnabled() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();

	if (LandscapeEdMode != nullptr)
	{
		TArray<ALandscapeBlueprintCustomBrush*> BrushStack = LandscapeEdMode->GetBrushesForCurrentProceduralLayer(LandscapeEdMode->CurrentToolTarget.TargetType);

		return BrushStack.Num() > 0;
	}

	return false;
}

FText FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::GetCommitBrushesButtonText() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();

	if (LandscapeEdMode != nullptr)
	{
		return LandscapeEdMode->AreAllBrushesCommitedToCurrentProceduralLayer(LandscapeEdMode->CurrentToolTarget.TargetType) ? LOCTEXT("UnCommitBrushesText", "Uncommit") : LOCTEXT("CommitBrushesText", "Commit");
	}

	return FText::FromName(NAME_None);
}

FReply FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::HandleDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, int32 SlotIndex, SVerticalBox::FSlot* Slot)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();

	if (LandscapeEdMode != nullptr)
	{
		const TArray<int8>& BrushOrderStack = LandscapeEdMode->GetBrushesOrderForCurrentProceduralLayer(LandscapeEdMode->CurrentToolTarget.TargetType);

		if (BrushOrderStack.IsValidIndex(SlotIndex))
		{
			TSharedPtr<SWidget> Row = GenerateRow(SlotIndex);

			if (Row.IsValid())
			{
				return FReply::Handled().BeginDragDrop(FLandscapeBrushDragDropOp::New(SlotIndex, Slot, Row));
			}
		}
	}

	return FReply::Unhandled();
}

TOptional<SDragAndDropVerticalBox::EItemDropZone> FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, SDragAndDropVerticalBox::EItemDropZone DropZone, SVerticalBox::FSlot* Slot)
{
	TSharedPtr<FLandscapeBrushDragDropOp> DragDropOperation = DragDropEvent.GetOperationAs<FLandscapeBrushDragDropOp>();

	if (DragDropOperation.IsValid())
	{
		return DropZone;
	}

	return TOptional<SDragAndDropVerticalBox::EItemDropZone>();
}

FReply FLandscapeEditorCustomNodeBuilder_ProceduralBrushStack::HandleAcceptDrop(FDragDropEvent const& DragDropEvent, SDragAndDropVerticalBox::EItemDropZone DropZone, int32 SlotIndex, SVerticalBox::FSlot* Slot)
{
	TSharedPtr<FLandscapeBrushDragDropOp> DragDropOperation = DragDropEvent.GetOperationAs<FLandscapeBrushDragDropOp>();

	if (DragDropOperation.IsValid())
	{
		FEdModeLandscape* LandscapeEdMode = GetEditorMode();

		if (LandscapeEdMode != nullptr)
		{
			TArray<int8>& BrushOrderStack = LandscapeEdMode->GetBrushesOrderForCurrentProceduralLayer(LandscapeEdMode->CurrentToolTarget.TargetType);

			if (BrushOrderStack.IsValidIndex(DragDropOperation->SlotIndexBeingDragged) && BrushOrderStack.IsValidIndex(SlotIndex))
			{
				int32 StartingLayerIndex = DragDropOperation->SlotIndexBeingDragged;
				int32 DestinationLayerIndex = SlotIndex;

				if (StartingLayerIndex != INDEX_NONE && DestinationLayerIndex != INDEX_NONE)
				{
					int8 MovingBrushIndex = BrushOrderStack[StartingLayerIndex];
					 
					BrushOrderStack.RemoveAt(StartingLayerIndex);
					BrushOrderStack.Insert(MovingBrushIndex, DestinationLayerIndex);

					LandscapeEdMode->RefreshDetailPanel();
					LandscapeEdMode->RequestProceduralContentUpdate();

					return FReply::Handled();
				}
			}
		}
	}

	return FReply::Unhandled();
}

TSharedRef<FLandscapeBrushDragDropOp> FLandscapeBrushDragDropOp::New(int32 InSlotIndexBeingDragged, SVerticalBox::FSlot* InSlotBeingDragged, TSharedPtr<SWidget> WidgetToShow)
{
	TSharedRef<FLandscapeBrushDragDropOp> Operation = MakeShareable(new FLandscapeBrushDragDropOp);

	Operation->MouseCursor = EMouseCursor::GrabHandClosed;
	Operation->SlotIndexBeingDragged = InSlotIndexBeingDragged;
	Operation->SlotBeingDragged = InSlotBeingDragged;
	Operation->WidgetToShow = WidgetToShow;

	Operation->Construct();

	return Operation;
}

FLandscapeBrushDragDropOp::~FLandscapeBrushDragDropOp()
{
}

TSharedPtr<SWidget> FLandscapeBrushDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ContentBrowser.AssetDragDropTooltipBackground"))
		.Content()
		[
			WidgetToShow.ToSharedRef()
		];

}

#undef LOCTEXT_NAMESPACE