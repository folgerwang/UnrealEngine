// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SControlRigGraphNode.h"
#include "ControlRigGraphNode.h"
#include "SGraphPin.h"
#include "ControlRigGraphSchema.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScrollBar.h"
#include "GraphEditorSettings.h"
#include "ControlRigEditorStyle.h"
#include "Widgets/Layout/SWrapBox.h"

#define LOCTEXT_NAMESPACE "SControlRigGraphNode"

void SControlRigGraphNode::Construct( const FArguments& InArgs )
{
	check(InArgs._GraphNodeObj);
	this->GraphNode = InArgs._GraphNodeObj;

	this->SetCursor( EMouseCursor::CardinalCross );

 	UControlRigGraphNode* ControlRigGraphNode = InArgs._GraphNodeObj;

	// Re-cache variable info here (unit structure could have changed since last reconstruction, e.g. array add/remove)
	// and also create missing pins if it hasn't created yet
	ControlRigGraphNode->CreateVariablePins(false);

	this->UpdateGraphNode();

	SetIsEditable(ControlRigGraphNode->IsPropertyAccessor());

	ScrollBar = SNew(SScrollBar);

	// create pin-collapse areas
	LeftNodeBox->AddSlot()
		.AutoHeight()
		[
			SAssignNew(InputTree, STreeView<TSharedRef<FControlRigField>>)
			.Visibility(this, &SControlRigGraphNode::GetInputTreeVisibility)
			.TreeItemsSource(&ControlRigGraphNode->GetInputVariableInfo())
			.SelectionMode(ESelectionMode::None)
			.OnGenerateRow(this, &SControlRigGraphNode::MakeTableRowWidget)
			.OnGetChildren(this, &SControlRigGraphNode::HandleGetChildrenForTree)
			.OnExpansionChanged(this, &SControlRigGraphNode::HandleExpansionChanged)
			.ExternalScrollbar(ScrollBar)
			.ItemHeight(20.0f)
		];

	LeftNodeBox->AddSlot()
		.AutoHeight()
		[
			SAssignNew(InputOutputTree, STreeView<TSharedRef<FControlRigField>>)
			.Visibility(this, &SControlRigGraphNode::GetInputOutputTreeVisibility)
			.TreeItemsSource(&ControlRigGraphNode->GetInputOutputVariableInfo())
			.SelectionMode(ESelectionMode::None)
			.OnGenerateRow(this, &SControlRigGraphNode::MakeTableRowWidget)
			.OnGetChildren(this, &SControlRigGraphNode::HandleGetChildrenForTree)
			.OnExpansionChanged(this, &SControlRigGraphNode::HandleExpansionChanged)
			.ExternalScrollbar(ScrollBar)
			.ItemHeight(20.0f)
		];

	LeftNodeBox->AddSlot()
		.AutoHeight()
		[
			SAssignNew(OutputTree, STreeView<TSharedRef<FControlRigField>>)
			.Visibility(this, &SControlRigGraphNode::GetOutputTreeVisibility)
			.TreeItemsSource(&ControlRigGraphNode->GetOutputVariableInfo())
			.SelectionMode(ESelectionMode::None)
			.OnGenerateRow(this, &SControlRigGraphNode::MakeTableRowWidget)
			.OnGetChildren(this, &SControlRigGraphNode::HandleGetChildrenForTree)
			.OnExpansionChanged(this, &SControlRigGraphNode::HandleExpansionChanged)
			.ExternalScrollbar(ScrollBar)
			.ItemHeight(20.0f)
		];

	struct Local
	{
		static void SetItemExpansion_Recursive(UControlRigGraphNode* InControlRigGraphNode, TSharedPtr<STreeView<TSharedRef<FControlRigField>>>& TreeWidget, const TArray<TSharedRef<FControlRigField>>& InItems)
		{
			for(const TSharedRef<FControlRigField>& Field : InItems)
			{
				if(InControlRigGraphNode->IsPinExpanded(Field->GetPropertyPath()))
				{
					TreeWidget->SetItemExpansion(Field, true);

					SetItemExpansion_Recursive(InControlRigGraphNode, TreeWidget, Field->Children);
				}
			}
		}
	};

	Local::SetItemExpansion_Recursive(ControlRigGraphNode, InputTree, ControlRigGraphNode->GetInputVariableInfo());
	Local::SetItemExpansion_Recursive(ControlRigGraphNode, InputOutputTree, ControlRigGraphNode->GetInputOutputVariableInfo());
	Local::SetItemExpansion_Recursive(ControlRigGraphNode, OutputTree, ControlRigGraphNode->GetOutputVariableInfo());
}

TSharedRef<SWidget> SControlRigGraphNode::CreateNodeContentArea()
{
	// We only use the LeftNodeBox
	return SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("NoBorder") )
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(0,3))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.FillWidth(1.0f)
			[
				SAssignNew(LeftNodeBox, SVerticalBox)
			]
		];
}

TSharedPtr<SGraphPin> SControlRigGraphNode::GetHoveredPin(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
{
	TSharedPtr<SGraphPin> HoveredPin = SGraphNode::GetHoveredPin(MyGeometry, MouseEvent);
	if(!HoveredPin.IsValid())
	{
 		TArray<TSharedRef<SWidget>> ExtraWidgetArray;
 		ExtraWidgetToPinMap.GenerateKeyArray(ExtraWidgetArray);
 		TSet<TSharedRef<SWidget>> ExtraWidgets(ExtraWidgetArray);
 		
 		TMap<TSharedRef<SWidget>, FArrangedWidget> Result;
 		FindChildGeometries(MyGeometry, ExtraWidgets, Result);

		if (Result.Num() > 0)
		{
			FArrangedChildren ArrangedWidgets(EVisibility::Visible);
			Result.GenerateValueArray(ArrangedWidgets.GetInternalArray());
			int32 HoveredWidgetIndex = SWidget::FindChildUnderMouse(ArrangedWidgets, MouseEvent);
			if (HoveredWidgetIndex != INDEX_NONE)
			{
				return *ExtraWidgetToPinMap.Find(ArrangedWidgets[HoveredWidgetIndex].Widget);
			}
		}
	}
	return HoveredPin;
}

void SControlRigGraphNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd) 
{
	// We show our own label
	PinToAdd->SetShowLabel(false);

	const UEdGraphPin* PinObj = PinToAdd->GetPinObj();

	// Remove value widget from combined pin content
	TSharedPtr<SWrapBox> LabelAndValueWidget = PinToAdd->GetLabelAndValue();
	TSharedPtr<SHorizontalBox> FullPinHorizontalRowWidget = PinToAdd->GetFullPinHorizontalRowWidget().Pin();
	if(LabelAndValueWidget.IsValid() && FullPinHorizontalRowWidget.IsValid())
	{
		FullPinHorizontalRowWidget->RemoveSlot(LabelAndValueWidget.ToSharedRef());
	}

	PinToAdd->SetOwner(SharedThis(this));
	PinWidgetMap.Add(PinObj, PinToAdd);
	if(PinObj->Direction == EGPD_Input)
	{
		InputPins.Add(PinToAdd);
	}
	else
	{
		OutputPins.Add(PinToAdd);
	}
}

bool SControlRigGraphNode::UseLowDetailNodeTitles() const
{
	return ParentUseLowDetailNodeTitles();
}

EVisibility SControlRigGraphNode::GetTitleVisibility() const
{
	return ParentUseLowDetailNodeTitles() ? EVisibility::Hidden : EVisibility::Visible;
}

EVisibility SControlRigGraphNode::GetInputTreeVisibility() const
{
	UControlRigGraphNode* ControlRigGraphNode = CastChecked<UControlRigGraphNode>(GraphNode);
	return ControlRigGraphNode->GetInputVariableInfo().Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SControlRigGraphNode::GetInputOutputTreeVisibility() const
{
	UControlRigGraphNode* ControlRigGraphNode = CastChecked<UControlRigGraphNode>(GraphNode);
	return ControlRigGraphNode->GetInputOutputVariableInfo().Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SControlRigGraphNode::GetOutputTreeVisibility() const
{
	UControlRigGraphNode* ControlRigGraphNode = CastChecked<UControlRigGraphNode>(GraphNode);
	return ControlRigGraphNode->GetOutputVariableInfo().Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> SControlRigGraphNode::CreateTitleWidget(TSharedPtr<SNodeTitle> NodeTitle)
{
	auto WidgetRef = SGraphNode::CreateTitleWidget(NodeTitle);
	auto VisibilityAttribute = TAttribute<EVisibility>::Create(
		TAttribute<EVisibility>::FGetter::CreateSP(this, &SControlRigGraphNode::GetTitleVisibility));
	WidgetRef->SetVisibility(VisibilityAttribute);
	if (NodeTitle.IsValid())
	{
		NodeTitle->SetVisibility(VisibilityAttribute);
	}

	return SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding(0.0f)
		[
			WidgetRef
		];
}

class SControlRigExpanderArrow : public SExpanderArrow
{
	SLATE_BEGIN_ARGS(SControlRigExpanderArrow) {}

	SLATE_ARGUMENT(bool, LeftAligned)

	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedPtr<class ITableRow>& TableRow)
	{
		bLeftAligned = InArgs._LeftAligned;

		SExpanderArrow::Construct(
			SExpanderArrow::FArguments()
			.IndentAmount(8.0f),
			TableRow);

		// override padding
		ChildSlot.Padding(TAttribute<FMargin>(this, &SControlRigExpanderArrow::GetExpanderPadding_Extended));

		// override image
		ExpanderArrow->SetContent(
			SNew(SImage)
			.Image(this, &SControlRigExpanderArrow::GetExpanderImage_Extended)
			.ColorAndOpacity(FSlateColor::UseForeground()));
	}

	FMargin GetExpanderPadding_Extended() const
	{
		const int32 NestingDepth = FMath::Max(0, OwnerRowPtr.Pin()->GetIndentLevel() - BaseIndentLevel.Get());
		const float Indent = IndentAmount.Get(8.0f);
		return bLeftAligned ? FMargin( NestingDepth * Indent, 0,0,0 ) : FMargin( 0,0, NestingDepth * Indent,0 );
	}

	const FSlateBrush* GetExpanderImage_Extended() const
	{
		const bool bIsItemExpanded = OwnerRowPtr.Pin()->IsItemExpanded();

		FName ResourceName;
		if (bIsItemExpanded)
		{
			if ( ExpanderArrow->IsHovered() )
			{
				static FName ExpandedHoveredLeftName = "ControlRig.Node.PinTree.Arrow_Expanded_Hovered_Left";
				static FName ExpandedHoveredRightName = "ControlRig.Node.PinTree.Arrow_Expanded_Hovered_Right";
				ResourceName = bLeftAligned ? ExpandedHoveredLeftName : ExpandedHoveredRightName;
			}
			else
			{
				static FName ExpandedLeftName = "ControlRig.Node.PinTree.Arrow_Expanded_Left";
				static FName ExpandedRightName = "ControlRig.Node.PinTree.Arrow_Expanded_Right";
				ResourceName = bLeftAligned ? ExpandedLeftName : ExpandedRightName;
			}
		}
		else
		{
			if ( ExpanderArrow->IsHovered() )
			{
				static FName CollapsedHoveredLeftName = "ControlRig.Node.PinTree.Arrow_Collapsed_Hovered_Left";
				static FName CollapsedHoveredRightName = "ControlRig.Node.PinTree.Arrow_Collapsed_Hovered_Right";
				ResourceName = bLeftAligned ? CollapsedHoveredLeftName : CollapsedHoveredRightName;
			}
			else
			{
				static FName CollapsedLeftName = "ControlRig.Node.PinTree.Arrow_Collapsed_Left";
				static FName CollapsedRightName = "ControlRig.Node.PinTree.Arrow_Collapsed_Right";
				ResourceName = bLeftAligned ? CollapsedLeftName : CollapsedRightName;
			}
		}

		return FControlRigEditorStyle::Get().GetBrush(ResourceName);
	}

	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
	{
		return FCursorReply::Cursor(EMouseCursor::Default);
	}

	bool bLeftAligned;
};

class SControlRigPinTreeRow : public STableRow<TSharedRef<FControlRigField>>
{
public:
	SLATE_BEGIN_ARGS(SControlRigPinTreeRow) {}

	SLATE_ARGUMENT(bool, LeftAligned)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		bLeftAligned = InArgs._LeftAligned;

		STableRow<TSharedRef<FControlRigField>>::Construct(STableRow<TSharedRef<FControlRigField>>::FArguments(), InOwnerTableView);
	}

	virtual void ConstructChildren( ETableViewMode::Type InOwnerTableMode, const TAttribute<FMargin>& InPadding, const TSharedRef<SWidget>& InContent ) override
	{
		const UGraphEditorSettings* Settings = GetDefault<UGraphEditorSettings>();
		FMargin InputPadding = Settings->GetInputPinPadding();
		InputPadding.Top = InputPadding.Bottom = 3.0f;
		InputPadding.Right = 0.0f;

		FMargin OutputPadding = Settings->GetOutputPinPadding();
		OutputPadding.Top = OutputPadding.Bottom = 3.0f;
		OutputPadding.Left = 2.0f;

		this->Content = InContent;

		SHorizontalBox::FSlot* InnerContentSlotNativePtr = nullptr;

		TSharedRef<SHorizontalBox> ContentBox = SNew(SHorizontalBox);

		if(bLeftAligned)
		{
			ContentBox->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.Padding(InputPadding)
			[
				SAssignNew(LeftContentBox, SBox)
			];

			ContentBox->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				SNew(SControlRigExpanderArrow, SharedThis(this))
				.ToolTipText(LOCTEXT("ExpandSubPin", "Expand Pin"))
				.LeftAligned(bLeftAligned)
			];

			ContentBox->AddSlot()
			.FillWidth(1.0f)
			.Expose(InnerContentSlotNativePtr)
			[
				SAssignNew(RightContentBox, SBox)
				[
					InContent
				]
			];
		}
		else
		{
			ContentBox->AddSlot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			[
				SAssignNew(LeftContentBox, SBox)
			];

			ContentBox->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			[
				SNew(SControlRigExpanderArrow, SharedThis(this))
				.LeftAligned(bLeftAligned)
			];

			ContentBox->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.Expose(InnerContentSlotNativePtr)
			.Padding(OutputPadding)
			[
				SAssignNew(RightContentBox, SBox)
				[
					InContent
				]
			];
		}

		this->ChildSlot
		[
			ContentBox
		];

		InnerContentSlot = InnerContentSlotNativePtr;
	}

	/** Exposed boxes to slot pin widgets into */
	TSharedPtr<SBox> LeftContentBox;
	TSharedPtr<SBox> RightContentBox;

	/** Whether we align our content left or right */
	bool bLeftAligned;
};

TSharedRef<SWidget> SControlRigGraphNode::AddContainerPinContent(TSharedRef<FControlRigField> InItem, FText InTooltipText)
{
	TWeakPtr<FControlRigField> WeakItem = InItem;

	return SNew(SButton)
	.ContentPadding(0.0f)
	.ButtonStyle(FEditorStyle::Get(), "NoBorder")
	.OnClicked(this, &SControlRigGraphNode::HandleAddArrayElement, WeakItem)
	.IsEnabled(this, &SGraphNode::IsNodeEditable)
	.ToolTipText(InTooltipText)
	.Cursor(EMouseCursor::Default)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(7,0,0,0)
		[
			SNew(SImage)
			.Image(FEditorStyle::GetBrush(TEXT("PropertyWindow.Button_AddToArray")))
		]
	];
}

TSharedRef<ITableRow> SControlRigGraphNode::MakeTableRowWidget(TSharedRef<FControlRigField> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	const bool bLeaf = InItem->Children.Num() == 0;
	const bool bIsContainer = InItem->PinType.IsContainer();

	TSharedPtr<SGraphPin> InputPinWidget;
	TSharedPtr<SGraphPin> OutputPinWidget;
	TSharedPtr<SWidget> InputPinValueWidget;

	if(InItem->InputPin)
	{
		TSharedPtr<SGraphPin>* InputGraphPinPtr = PinWidgetMap.Find(InItem->InputPin);
		if(InputGraphPinPtr != nullptr)
		{
			InputPinWidget = *InputGraphPinPtr;

			// Only leaf pins have value widgets, but not containers
			if(bLeaf && !bIsContainer)
			{
				InputPinValueWidget = (*InputGraphPinPtr)->GetValueWidget();
			}
			else if(bIsContainer)
			{
				// Add a 'new item' widget for containers
				InputPinValueWidget = AddContainerPinContent(InItem, LOCTEXT("AddToContainer", "Add a new value to this container"));
			}
		}
	}

	if(InItem->OutputPin)
	{
		TSharedPtr<SGraphPin>* OutputGraphPinPtr = PinWidgetMap.Find(InItem->OutputPin);
		if(OutputGraphPinPtr != nullptr)
		{
			OutputPinWidget = *OutputGraphPinPtr;
		}
	}

	TSharedRef<SControlRigPinTreeRow> ControlRigPinTreeRow = SNew(SControlRigPinTreeRow, OwnerTable)
		.LeftAligned(!(OutputPinWidget.IsValid() && !InputPinWidget.IsValid()))
		.ToolTipText(InItem->GetTooltipText());

	if(InputPinWidget.IsValid() || OutputPinWidget.IsValid())
	{
		TWeakPtr<SGraphPin> WeakPin = InputPinWidget.IsValid() ? InputPinWidget : OutputPinWidget;

		TSharedRef<SWidget> LabelWidget = SNew(STextBlock)
		.Text(this, &SControlRigGraphNode::GetPinLabel, WeakPin)
		.TextStyle(FEditorStyle::Get(), NAME_DefaultPinLabelStyle)
		.ColorAndOpacity(this, &SControlRigGraphNode::GetPinTextColor, WeakPin);

		// add to mapping that allows labels to act as hover widgets
		if(InputPinWidget.IsValid())
		{
			ExtraWidgetToPinMap.Emplace(LabelWidget, InputPinWidget.ToSharedRef());
		}
		else if(OutputPinWidget.IsValid())
		{
			ExtraWidgetToPinMap.Emplace(LabelWidget, OutputPinWidget.ToSharedRef());
		}

		FMargin OutputPadding = Settings->GetOutputPinPadding();
		OutputPadding.Top = OutputPadding.Bottom = 3.0f;
		OutputPadding.Left = 2.0f;

		if(OutputPinWidget.IsValid() && !InputPinWidget.IsValid())
		{
			TSharedRef<SWidget> InputWidget = 
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.FillWidth(1.0f)
				.Padding(50.0f, 2.0f, 2.0f, 2.0f)
				[
					LabelWidget
				];

			TSharedRef<SWidget> OutputWidget = 
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.MinDesiredWidth(24.0f)
					[
						OutputPinWidget.IsValid() ? StaticCastSharedRef<SWidget>(OutputPinWidget.ToSharedRef()) : StaticCastSharedRef<SWidget>(SNew(SSpacer).Size(FVector2D(16.0f, 14.0f)))
					]
				];

			ControlRigPinTreeRow->LeftContentBox->SetContent(InputWidget);
			ControlRigPinTreeRow->RightContentBox->SetContent(OutputWidget);
		}
		else
		{
			TSharedRef<SWidget> InputWidget = 
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.MinDesiredWidth(24.0f)
					[
						InputPinWidget.IsValid() ? StaticCastSharedRef<SWidget>(InputPinWidget.ToSharedRef()) : StaticCastSharedRef<SWidget>(SNew(SSpacer).Size(FVector2D(16.0f, 14.0f)))
					]
				];
				
			TSharedRef<SWidget> OutputWidget = 
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(2.0f)
				[
					LabelWidget
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(2.0f, 2.0f, 18.0f, 2.0f)
				[
					InputPinValueWidget.IsValid() ? InputPinValueWidget.ToSharedRef() : SNew(SSpacer)
				]
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(OutputPadding)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.MinDesiredWidth(24.0f)
					[
						OutputPinWidget.IsValid() ? StaticCastSharedRef<SWidget>(OutputPinWidget.ToSharedRef()) : StaticCastSharedRef<SWidget>(SNew(SSpacer).Size(FVector2D(16.0f, 14.0f)))
					]
				];

			ControlRigPinTreeRow->LeftContentBox->SetContent(InputWidget);
			ControlRigPinTreeRow->RightContentBox->SetContent(OutputWidget);
		}
	}

	return ControlRigPinTreeRow;
}
	
void SControlRigGraphNode::HandleGetChildrenForTree(TSharedRef<FControlRigField> InItem, TArray<TSharedRef<FControlRigField>>& OutChildren)
{
	OutChildren.Append(InItem->Children);
}

void SControlRigGraphNode::HandleExpansionChanged(TSharedRef<FControlRigField> InItem, bool bExpanded)
{
	if (GraphNode)
	{
		CastChecked<UControlRigGraphNode>(GraphNode)->SetPinExpansion(InItem->GetPropertyPath(), bExpanded);
	}
}

FText SControlRigGraphNode::GetPinLabel(TWeakPtr<SGraphPin> GraphPin) const
{
	if(GraphPin.IsValid())
	{
		if (GraphNode)
		{
			return GraphNode->GetPinDisplayName(GraphPin.Pin()->GetPinObj());
		}
	}

	return FText();
}

FSlateColor SControlRigGraphNode::GetPinTextColor(TWeakPtr<SGraphPin> GraphPin) const
{
	if(GraphPin.IsValid())
	{
		// If there is no schema there is no owning node (or basically this is a deleted node)
		if (GraphNode)
		{
			if(!GraphNode->IsNodeEnabled() || GraphNode->IsDisplayAsDisabledForced() || !GraphPin.Pin()->IsEditingEnabled())
			{
				return FLinearColor(1.0f, 1.0f, 1.0f, 0.5f);
			}
		}
	}

	return FLinearColor::White;
}

FReply SControlRigGraphNode::HandleAddArrayElement(TWeakPtr<FControlRigField> InWeakItem)
{
	TSharedPtr<FControlRigField> Item = InWeakItem.Pin();
	if(Item.IsValid())
	{
		if (UControlRigGraphNode* ControlRigGraphNode = Cast<UControlRigGraphNode>(GraphNode))
		{
			ControlRigGraphNode->HandleAddArrayElement(Item->PropertyPath);
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE