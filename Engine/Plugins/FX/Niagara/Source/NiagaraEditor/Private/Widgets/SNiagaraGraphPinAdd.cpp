// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraGraphPinAdd.h"
#include "NiagaraNodeWithDynamicPins.h"

#include "ScopedTransaction.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "SNiagaraParameterMapView.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "NiagaraGraphPinAdd"

void SNiagaraGraphPinAdd::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SetShowLabel(false);
	OwningNode = Cast<UNiagaraNodeWithDynamicPins>(InGraphPinObj->GetOwningNode());
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
	TSharedPtr<SHorizontalBox> PinBox = GetFullPinHorizontalRowWidget().Pin();
	if (PinBox.IsValid())
	{
		if (InGraphPinObj->Direction == EGPD_Input)
		{
			PinBox->AddSlot()
			[
				ConstructAddButton()
			];
		}
		else
		{
			PinBox->InsertSlot(0)
			[
				ConstructAddButton()
			];
		}
	}
}

TSharedRef<SWidget>	SNiagaraGraphPinAdd::ConstructAddButton()
{
	AddButton = SNew(SComboButton)
		.HasDownArrow(false)
		.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
		.ForegroundColor(FSlateColor::UseForeground())
		.OnGetMenuContent(this, &SNiagaraGraphPinAdd::OnGetAddButtonMenuContent)
		.ContentPadding(FMargin(2))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.ToolTipText(LOCTEXT("AddPinButtonToolTip", "Connect this pin to add a new typed pin, or choose from the drop-down."))
		.ButtonContent()
		[
			SNew(SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
			.Image(FEditorStyle::GetBrush("Plus"))
		];

	return AddButton.ToSharedRef();
}

TSharedRef<SWidget> SNiagaraGraphPinAdd::OnGetAddButtonMenuContent()
{
	if (OwningNode != nullptr)
	{
		TArray<TWeakObjectPtr<UNiagaraGraph>> Graphs;
		Graphs.Add(OwningNode->GetNiagaraGraph());
		TSharedRef<SNiagaraAddParameterMenu> MenuWidget = SNew(SNiagaraAddParameterMenu, Graphs)
			.OnAddParameter_UObject(OwningNode, &UNiagaraNodeWithDynamicPins::AddParameter, GetPinObj()) // For non custom actions
			.OnCollectCustomActions_UObject(OwningNode, &UNiagaraNodeWithDynamicPins::CollectAddPinActions, GetPinObj())
			.OnAllowMakeType_UObject(OwningNode, &UNiagaraNodeWithDynamicPins::AllowNiagaraTypeForAddPin)
			.IsParameterRead(GraphPinObj ? GraphPinObj->Direction == EGPD_Output : true);
		
		AddButton->SetMenuContentWidgetToFocus(MenuWidget->GetSearchBox());
		return MenuWidget;
	}
	else
	{
		return SNullWidget::NullWidget;
	}

}

#undef LOCTEXT_NAMESPACE
