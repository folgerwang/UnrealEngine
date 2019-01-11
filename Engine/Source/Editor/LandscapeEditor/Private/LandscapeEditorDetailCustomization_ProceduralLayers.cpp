// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorDetailCustomization_ProceduralLayers.h"
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
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SErrorText.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "LandscapeEditorModule.h"
#include "LandscapeEditorObject.h"

#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"

#include "SLandscapeEditor.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "DesktopPlatformModule.h"
#include "AssetRegistryModule.h"

#include "LandscapeRender.h"
#include "Materials/MaterialExpressionLandscapeVisibilityMask.h"
#include "LandscapeEdit.h"
#include "IDetailGroup.h"
#include "Widgets/SBoxPanel.h"
#include "Editor/EditorStyle/Private/SlateEditorStyle.h"
#include "LandscapeEditorDetailCustomization_TargetLayers.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor.Layers"

TSharedRef<IDetailCustomization> FLandscapeEditorDetailCustomization_ProceduralLayers::MakeInstance()
{
	return MakeShareable(new FLandscapeEditorDetailCustomization_ProceduralLayers);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FLandscapeEditorDetailCustomization_ProceduralLayers::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& LayerCategory = DetailBuilder.EditCategory("Procedural Layers");

	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentToolMode != nullptr)
	{
		const FName CurrentToolName = LandscapeEdMode->CurrentTool->GetToolName();

		if (LandscapeEdMode->CurrentToolMode->SupportedTargetTypes != 0)
		{
			LayerCategory.AddCustomBuilder(MakeShareable(new FLandscapeEditorCustomNodeBuilder_ProceduralLayers(DetailBuilder.GetThumbnailPool().ToSharedRef())));
		}
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

//////////////////////////////////////////////////////////////////////////

FEdModeLandscape* FLandscapeEditorCustomNodeBuilder_ProceduralLayers::GetEditorMode()
{
	return (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
}

FLandscapeEditorCustomNodeBuilder_ProceduralLayers::FLandscapeEditorCustomNodeBuilder_ProceduralLayers(TSharedRef<FAssetThumbnailPool> InThumbnailPool)
	: ThumbnailPool(InThumbnailPool)
{
}

FLandscapeEditorCustomNodeBuilder_ProceduralLayers::~FLandscapeEditorCustomNodeBuilder_ProceduralLayers()
{
}

void FLandscapeEditorCustomNodeBuilder_ProceduralLayers::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
{
}

void FLandscapeEditorCustomNodeBuilder_ProceduralLayers::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
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
			.Text(FText::FromString(TEXT("")))
		];
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FLandscapeEditorCustomNodeBuilder_ProceduralLayers::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		TSharedPtr<SDragAndDropVerticalBox> LayerList = SNew(SDragAndDropVerticalBox)
			.OnCanAcceptDrop(this, &FLandscapeEditorCustomNodeBuilder_ProceduralLayers::HandleCanAcceptDrop)
			.OnAcceptDrop(this, &FLandscapeEditorCustomNodeBuilder_ProceduralLayers::HandleAcceptDrop)
			.OnDragDetected(this, &FLandscapeEditorCustomNodeBuilder_ProceduralLayers::HandleDragDetected);

		LayerList->SetDropIndicator_Above(*FEditorStyle::GetBrush("LandscapeEditor.TargetList.DropZone.Above"));
		LayerList->SetDropIndicator_Below(*FEditorStyle::GetBrush("LandscapeEditor.TargetList.DropZone.Below"));

		ChildrenBuilder.AddCustomRow(FText::FromString(FString(TEXT("Procedural Layers"))))
			.Visibility(EVisibility::Visible)
			[
				LayerList.ToSharedRef()
			];

		for (int32 i = 0; i < LandscapeEdMode->GetProceduralLayerCount(); ++i)
		{
			TSharedPtr<SWidget> GeneratedRowWidget = GenerateRow(i);

			if (GeneratedRowWidget.IsValid())
			{
				LayerList->AddSlot()
					.AutoHeight()
					[
						GeneratedRowWidget.ToSharedRef()
					];
			}
		}
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedPtr<SWidget> FLandscapeEditorCustomNodeBuilder_ProceduralLayers::GenerateRow(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	TSharedPtr<SWidget> RowWidget = SNew(SLandscapeEditorSelectableBorder)
		.Padding(0)
		.VAlign(VAlign_Center)
		//.OnContextMenuOpening_Static(&FLandscapeEditorCustomNodeBuilder_Layers::OnTargetLayerContextMenuOpening, Target)
		.OnSelected(this, &FLandscapeEditorCustomNodeBuilder_ProceduralLayers::OnLayerSelectionChanged, InLayerIndex)
		.IsSelected(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_ProceduralLayers::IsLayerSelected, InLayerIndex)))
		.Visibility(EVisibility::Visible)
		[
			SNew(SHorizontalBox)
			
			/*+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(2))
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush(TEXT("LandscapeEditor.Target_Heightmap")))
			]
			*/

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			.Padding(4, 0)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Center)
				.Padding(0, 2)
				.HAlign(HAlign_Left)				
				[
					SNew(SEditableText)
					.SelectAllTextWhenFocused(true)
					.IsReadOnly(true)
					.Text(this, &FLandscapeEditorCustomNodeBuilder_ProceduralLayers::GetLayerText, InLayerIndex)
					.ToolTipText(LOCTEXT("FLandscapeEditorCustomNodeBuilder_ProceduralLayers_tooltip", "Name of the Layer"))
					.OnTextCommitted(FOnTextCommitted::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_ProceduralLayers::OnLayerTextCommitted, InLayerIndex))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(0, 2)
			.HAlign(HAlign_Center)				
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &FLandscapeEditorCustomNodeBuilder_ProceduralLayers::OnLayerVisibilityChanged, InLayerIndex)
				.IsChecked(TAttribute<ECheckBoxState>::Create(TAttribute<ECheckBoxState>::FGetter::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_ProceduralLayers::IsLayerVisible, InLayerIndex)))
				.ToolTipText(LOCTEXT("FLandscapeEditorCustomNodeBuilder_ProceduralLayerVisibility_Tooltips", "Is layer visible"))
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FLandscapeEditorCustomNodeBuilder_ProceduralLayerVisibility", "Visibility"))
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(0)
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("FLandscapeEditorCustomNodeBuilder_ProceduralLayerWeight", "Weight"))
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(0, 2)
			.HAlign(HAlign_Left)
			.FillWidth(1.0f)
			[
				SNew(SNumericEntryBox<float>)
				.AllowSpin(true)
				.MinValue(0.0f)
				.MaxValue(65536.0f)
				.MaxSliderValue(65536.0f)
				.MinDesiredValueWidth(25.0f)
				.Value(this, &FLandscapeEditorCustomNodeBuilder_ProceduralLayers::GetLayerWeight, InLayerIndex)
				.OnValueChanged(this, &FLandscapeEditorCustomNodeBuilder_ProceduralLayers::SetLayerWeight, InLayerIndex)
				.IsEnabled(true)
			]			
		];	

	return RowWidget;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void FLandscapeEditorCustomNodeBuilder_ProceduralLayers::OnLayerTextCommitted(const FText& InText, ETextCommit::Type InCommitType, int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();

	if (LandscapeEdMode != nullptr)
	{
		LandscapeEdMode->SetProceduralLayerName(InLayerIndex, *InText.ToString());
	}
}

FText FLandscapeEditorCustomNodeBuilder_ProceduralLayers::GetLayerText(int32 InLayerIndex) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();

	if (LandscapeEdMode != nullptr)
	{
		return FText::FromName(LandscapeEdMode->GetProceduralLayerName(InLayerIndex));
	}

	return FText::FromString(TEXT("None"));
}

bool FLandscapeEditorCustomNodeBuilder_ProceduralLayers::IsLayerSelected(int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		return LandscapeEdMode->GetCurrentProceduralLayerIndex() == InLayerIndex;
	}

	return false;
}

void FLandscapeEditorCustomNodeBuilder_ProceduralLayers::OnLayerSelectionChanged(int32 InLayerIndex)
{	
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		LandscapeEdMode->SetCurrentProceduralLayer(InLayerIndex);
		LandscapeEdMode->UpdateTargetList();
	}
}

TOptional<float> FLandscapeEditorCustomNodeBuilder_ProceduralLayers::GetLayerWeight(int32 InLayerIndex) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();

	if (LandscapeEdMode)
	{
		return LandscapeEdMode->GetProceduralLayerWeight(InLayerIndex);
	}

	return 1.0f;
}

void FLandscapeEditorCustomNodeBuilder_ProceduralLayers::SetLayerWeight(float InWeight, int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();

	if (LandscapeEdMode)
	{
		LandscapeEdMode->SetProceduralLayerWeight(InWeight, InLayerIndex);
	}
}

void FLandscapeEditorCustomNodeBuilder_ProceduralLayers::OnLayerVisibilityChanged(ECheckBoxState NewState, int32 InLayerIndex)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();

	if (LandscapeEdMode)
	{
		LandscapeEdMode->SetProceduralLayerVisibility(NewState == ECheckBoxState::Checked, InLayerIndex);
	}
}

ECheckBoxState FLandscapeEditorCustomNodeBuilder_ProceduralLayers::IsLayerVisible(int32 InLayerIndex) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();

	if (LandscapeEdMode)
	{
		return LandscapeEdMode->IsProceduralLayerVisible(InLayerIndex) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Unchecked;
}

FReply FLandscapeEditorCustomNodeBuilder_ProceduralLayers::HandleDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, int32 SlotIndex, SVerticalBox::FSlot* Slot)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();

	if (LandscapeEdMode != nullptr)
	{
		// TODO: handle drag & drop
	}

	return FReply::Unhandled();
}

TOptional<SDragAndDropVerticalBox::EItemDropZone> FLandscapeEditorCustomNodeBuilder_ProceduralLayers::HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, SDragAndDropVerticalBox::EItemDropZone DropZone, SVerticalBox::FSlot* Slot)
{
	// TODO: handle drag & drop
	return TOptional<SDragAndDropVerticalBox::EItemDropZone>();
}

FReply FLandscapeEditorCustomNodeBuilder_ProceduralLayers::HandleAcceptDrop(FDragDropEvent const& DragDropEvent, SDragAndDropVerticalBox::EItemDropZone DropZone, int32 SlotIndex, SVerticalBox::FSlot* Slot)
{
	// TODO: handle drag & drop
	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
