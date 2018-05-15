// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SGraphNode.h"
#include "Widgets/Views/STreeView.h"

class UControlRigGraphNode;
class STableViewBase;
class FControlRigField;
class SOverlay;
class SGraphPin;
class UEdGraphPin;
class SScrollBar;

class SControlRigGraphNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SControlRigGraphNode)
		: _GraphNodeObj(nullptr)
		{}

	SLATE_ARGUMENT(UControlRigGraphNode*, GraphNodeObj)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// SGraphNode interface
	virtual TSharedRef<SWidget> CreateTitleWidget(TSharedPtr<SNodeTitle> NodeTitle) override;
	virtual bool UseLowDetailNodeTitles() const override;
	virtual void AddPin( const TSharedRef<SGraphPin>& PinToAdd ) override;
	virtual void SetDefaultTitleAreaWidget(TSharedRef<SOverlay> DefaultTitleAreaWidget) override
	{
		TitleAreaWidget = DefaultTitleAreaWidget;
	}

	virtual TSharedRef<SWidget> CreateNodeContentArea() override;
	virtual TSharedPtr<SGraphPin> GetHoveredPin( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) const override;

private:
	bool ParentUseLowDetailNodeTitles() const
	{
		return SGraphNode::UseLowDetailNodeTitles();
	}

	EVisibility GetTitleVisibility() const;

	EVisibility GetInputTreeVisibility() const;

	EVisibility GetInputOutputTreeVisibility() const;

	EVisibility GetOutputTreeVisibility() const;

	TSharedRef<ITableRow> MakeTableRowWidget(TSharedRef<FControlRigField> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	void HandleGetChildrenForTree(TSharedRef<FControlRigField> InItem, TArray<TSharedRef<FControlRigField>>& OutChildren);

	void HandleExpansionChanged(TSharedRef<FControlRigField> InItem, bool bExpanded);

	FText GetPinLabel(TWeakPtr<SGraphPin> GraphPin) const;

	FSlateColor GetPinTextColor(TWeakPtr<SGraphPin> GraphPin) const;

	TSharedRef<SWidget> AddContainerPinContent(TSharedRef<FControlRigField> InItem, FText InTooltipText);

	FReply HandleAddArrayElement(TWeakPtr<FControlRigField> InWeakItem);

private:
	/** Cached widget title area */
	TSharedPtr<SOverlay> TitleAreaWidget;

	/** Widget representing collapsible input pins */
	TSharedPtr<STreeView<TSharedRef<FControlRigField>>> InputTree;

	/** Widget representing collapsible input-output pins */
	TSharedPtr<STreeView<TSharedRef<FControlRigField>>> InputOutputTree;

	/** Widget representing collapsible output pins */
	TSharedPtr<STreeView<TSharedRef<FControlRigField>>> OutputTree;

	/** Dummy scrollbar, as we cant create a tree view without one! */
	TSharedPtr<SScrollBar> ScrollBar;

	/** Map of pin->widget */
	TMap<const UEdGraphPin*, TSharedPtr<SGraphPin>> PinWidgetMap;

	/** Map of pin widgets to extra pin widgets */
	TMap<TSharedRef<SWidget>, TSharedRef<SGraphPin>> ExtraWidgetToPinMap;
};
