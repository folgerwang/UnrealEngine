// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Layout/Margin.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Editor/LandscapeEditor/Private/LandscapeEdMode.h"
#include "IDetailCustomNodeBuilder.h"
#include "IDetailCustomization.h"
#include "AssetThumbnail.h"
#include "Framework/SlateDelegates.h"
#include "Editor/LandscapeEditor/Private/LandscapeEditorDetailCustomization_Base.h"

class FDetailWidgetRow;
class IDetailChildrenBuilder;
class IDetailLayoutBuilder;
class SDragAndDropVerticalBox;

/**
 * Slate widgets customizer for the layers list in the Landscape Editor
 */

class FLandscapeEditorDetailCustomization_ProceduralLayers : public FLandscapeEditorDetailCustomization_Base
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

class FLandscapeEditorCustomNodeBuilder_ProceduralLayers : public IDetailCustomNodeBuilder, public TSharedFromThis<FLandscapeEditorCustomNodeBuilder_ProceduralLayers>
{
public:
	FLandscapeEditorCustomNodeBuilder_ProceduralLayers(TSharedRef<FAssetThumbnailPool> ThumbnailPool);
	~FLandscapeEditorCustomNodeBuilder_ProceduralLayers();

	virtual void SetOnRebuildChildren( FSimpleDelegate InOnRegenerateChildren ) override;
	virtual void GenerateHeaderRowContent( FDetailWidgetRow& NodeRow ) override;
	virtual void GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder ) override;
	virtual void Tick( float DeltaTime ) override {}
	virtual bool RequiresTick() const override { return false; }
	virtual bool InitiallyCollapsed() const override { return false; }
	virtual FName GetName() const override { return "Layers"; }

protected:
	TSharedRef<FAssetThumbnailPool> ThumbnailPool;

	static class FEdModeLandscape* GetEditorMode();

	TSharedPtr<SWidget> GenerateRow(int32 InLayerIndex);

	// Drag/Drop handling
	FReply HandleDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, int32 SlotIndex, SVerticalBox::FSlot* Slot);
	TOptional<SDragAndDropVerticalBox::EItemDropZone> HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, SDragAndDropVerticalBox::EItemDropZone DropZone, SVerticalBox::FSlot* Slot);
	FReply HandleAcceptDrop(FDragDropEvent const& DragDropEvent, SDragAndDropVerticalBox::EItemDropZone DropZone, int32 SlotIndex, SVerticalBox::FSlot* Slot);

	bool IsLayerSelected(int32 LayerIndex);
	void OnLayerSelectionChanged(int32 LayerIndex);

	void OnLayerTextCommitted(const FText& InText, ETextCommit::Type InCommitType, int32 InLayerIndex);
	FText GetLayerText(int32 InLayerIndex) const;

	TOptional<float> GetLayerWeight(int32 InLayerIndex) const;
	void SetLayerWeight(float InWeight, int32 InLayerIndex);

	void OnLayerVisibilityChanged(ECheckBoxState NewState, int32 InLayerIndex);
	ECheckBoxState IsLayerVisible(int32 InLayerIndex) const;
};