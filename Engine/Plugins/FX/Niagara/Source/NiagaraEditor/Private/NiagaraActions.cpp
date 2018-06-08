// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraActions.h"
#include "NiagaraNodeParameterMapGet.h"
#include "NiagaraNodeParameterMapSet.h"
#include "EdGraphSchema_Niagara.h"
#include "Widgets/SWidget.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"

#define LOCTEXT_NAMESPACE "NiagaraActions"

/************************************************************************/
/* FNiagaraMenuAction													*/
/************************************************************************/
FNiagaraMenuAction::FNiagaraMenuAction(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping, FText InKeywords, FOnExecuteStackAction InAction, int32 InSectionID)
	: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping, MoveTemp(InKeywords), InSectionID)
	, Action(InAction)
{}

FNiagaraMenuAction::FNiagaraMenuAction(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping, FText InKeywords, FOnExecuteStackAction InAction, FCanExecuteStackAction InCanPerformAction, int32 InSectionID)
	: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping, MoveTemp(InKeywords), InSectionID)
	, Action(InAction)
	, CanPerformAction(InCanPerformAction)
{}

/************************************************************************/
/* FNiagaraParameterAction												*/
/************************************************************************/
FNiagaraParameterAction::FNiagaraParameterAction(const FNiagaraVariable& InParameter,
	const TArray<FNiagaraGraphParameterReferenceCollection>& InReferenceCollection,
	FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping, FText InKeywords, int32 InSectionID)
	: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping, MoveTemp(InKeywords), InSectionID)
	, Parameter(InParameter)
	, ReferenceCollection(InReferenceCollection)
{
}

/************************************************************************/
/* FNiagaraParameterGraphDragOperation									*/
/************************************************************************/
FNiagaraParameterGraphDragOperation::FNiagaraParameterGraphDragOperation()
	: bControlDrag(false)
	, bAltDrag(false)
{

}

TSharedRef<FNiagaraParameterGraphDragOperation> FNiagaraParameterGraphDragOperation::New(const TSharedPtr<FEdGraphSchemaAction>& InActionNode)
{
	TSharedRef<FNiagaraParameterGraphDragOperation> Operation = MakeShareable(new FNiagaraParameterGraphDragOperation);
	Operation->SourceAction = InActionNode;
	Operation->Construct();
	return Operation;
}

void FNiagaraParameterGraphDragOperation::HoverTargetChanged()
{
	if (SourceAction.IsValid())
	{
		if (!HoveredCategoryName.IsEmpty())
		{
			return;
		}
		else if (HoveredAction.IsValid())
		{
			const FSlateBrush* StatusSymbol = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
			TSharedPtr<FNiagaraParameterAction> ParameterAction = StaticCastSharedPtr<FNiagaraParameterAction>(SourceAction);
			if (ParameterAction.IsValid())
			{
				const FLinearColor TypeColor = UEdGraphSchema_Niagara::GetTypeColor(ParameterAction->GetParameter().GetType());
				SetSimpleFeedbackMessage(StatusSymbol, TypeColor, SourceAction->GetMenuDescription());
			}
			return;
		}
	}

	FGraphSchemaActionDragDropAction::HoverTargetChanged();
}

FReply FNiagaraParameterGraphDragOperation::DroppedOnNode(FVector2D ScreenPosition, FVector2D GraphPosition)
{
	FNiagaraParameterAction* ParameterAction = (FNiagaraParameterAction*)SourceAction.Get();
	if (ParameterAction)
	{
		const FNiagaraVariable& Parameter = ParameterAction->GetParameter();
		if (UNiagaraNodeParameterMapGet* GetMapNode = Cast<UNiagaraNodeParameterMapGet>(GetHoveredNode()))
		{
			GetMapNode->RequestNewTypedPin(EGPD_Output, Parameter.GetType(), Parameter.GetName());
		} 
		else if (UNiagaraNodeParameterMapSet* SetMapNode = Cast<UNiagaraNodeParameterMapSet>(GetHoveredNode()))
		{
			SetMapNode->RequestNewTypedPin(EGPD_Input, Parameter.GetType(), Parameter.GetName());
		}
	}

	return FReply::Handled();
}

FReply FNiagaraParameterGraphDragOperation::DroppedOnPanel(const TSharedRef<SWidget>& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph)
{
	if (Graph.GetSchema()->IsA<UEdGraphSchema_Niagara>())
	{
		FNiagaraParameterAction* ParameterAction = (FNiagaraParameterAction*)SourceAction.Get();
		if (ParameterAction)
		{
			FNiagaraParameterNodeConstructionParams NewNodeParams;
			NewNodeParams.Graph = &Graph;
			NewNodeParams.GraphPosition = GraphPosition;
			NewNodeParams.Parameter = ParameterAction->GetParameter();

			// Take into account current state of modifier keys in case the user changed his mind
			FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
			const bool bModifiedKeysActive = ModifierKeys.IsControlDown() || ModifierKeys.IsAltDown();
			const bool bAutoCreateGetter = bModifiedKeysActive ? ModifierKeys.IsControlDown() : bControlDrag;
			const bool bAutoCreateSetter = bModifiedKeysActive ? ModifierKeys.IsAltDown() : bAltDrag;
			// Handle Getter/Setters
			if (bAutoCreateGetter || bAutoCreateSetter)
			{
				if (bAutoCreateGetter)
				{
					MakeGetMap(NewNodeParams);
				}
				if (bAutoCreateSetter)
				{
					MakeSetMap(NewNodeParams);
				}
			}
			// Show selection menu
			else
			{
				FMenuBuilder MenuBuilder(true, NULL);
				const FText ParameterNameText = FText::FromName(NewNodeParams.Parameter.GetName());

				MenuBuilder.BeginSection("NiagaraParameterDroppedOnPanel", ParameterNameText);
				MenuBuilder.AddMenuEntry(
					FText::Format(LOCTEXT("CreateGetMap", "Get Map including {0}"), ParameterNameText),
					FText::Format(LOCTEXT("CreateGetMapToolTip", "Create Getter for variable '{0}'\n(Ctrl-drag to automatically create a getter)"), ParameterNameText),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateStatic(&FNiagaraParameterGraphDragOperation::MakeGetMap, NewNodeParams), 
						FCanExecuteAction())
				);

				MenuBuilder.AddMenuEntry(
					FText::Format(LOCTEXT("CreateSetMap", "Set Map including {0}"), ParameterNameText),
					FText::Format(LOCTEXT("CreateSetMapToolTip", "Create Set Map for parameter '{0}'\n(Alt-drag to automatically create a setter)"), ParameterNameText),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateStatic(&FNiagaraParameterGraphDragOperation::MakeSetMap, NewNodeParams),
						FCanExecuteAction())
				);

				TSharedRef< SWidget > PanelWidget = Panel;
				// Show dialog to choose getter vs setter
				FSlateApplication::Get().PushMenu(
					PanelWidget,
					FWidgetPath(),
					MenuBuilder.MakeWidget(),
					ScreenPosition,
					FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
				);

				MenuBuilder.EndSection();
			}
		}
	}

	return FReply::Handled();
}


void FNiagaraParameterGraphDragOperation::MakeGetMap(FNiagaraParameterNodeConstructionParams InParams)
{
	check(InParams.Graph);
	FGraphNodeCreator<UNiagaraNodeParameterMapGet> GetNodeCreator(*InParams.Graph);
	UNiagaraNodeParameterMapGet* GetNode = GetNodeCreator.CreateNode();
	GetNode->NodePosX = InParams.GraphPosition.X;
	GetNode->NodePosY = InParams.GraphPosition.Y;
	GetNodeCreator.Finalize();
	GetNode->RequestNewTypedPin(EGPD_Output, InParams.Parameter.GetType(), InParams.Parameter.GetName());
}

void FNiagaraParameterGraphDragOperation::MakeSetMap(FNiagaraParameterNodeConstructionParams InParams)
{
	check(InParams.Graph);
	FGraphNodeCreator<UNiagaraNodeParameterMapSet> SetNodeCreator(*InParams.Graph);
	UNiagaraNodeParameterMapSet* SetNode = SetNodeCreator.CreateNode();
	SetNode->NodePosX = InParams.GraphPosition.X;
	SetNode->NodePosY = InParams.GraphPosition.Y;
	SetNodeCreator.Finalize();
	SetNode->RequestNewTypedPin(EGPD_Input, InParams.Parameter.GetType(), InParams.Parameter.GetName());
}

EVisibility FNiagaraParameterGraphDragOperation::GetIconVisible() const
{
	return EVisibility::Collapsed;
}

EVisibility FNiagaraParameterGraphDragOperation::GetErrorIconVisible() const
{
	return EVisibility::Collapsed;
}

/************************************************************************/
/* FNiagaraStackDragOperation											*/
/************************************************************************/
TSharedRef<FNiagaraStackDragOperation> FNiagaraStackDragOperation::New(TSharedPtr<FEdGraphSchemaAction> InActionNode)
{
	TSharedRef<FNiagaraStackDragOperation> Operation = MakeShareable(new FNiagaraStackDragOperation);
	Operation->SourceAction = InActionNode;
	Operation->Construct();
	return Operation;
}

void FNiagaraStackDragOperation::OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent)
{
	FDragDropOperation::OnDrop(bDropWasHandled, MouseEvent);
}

void FNiagaraStackDragOperation::OnDragged(const class FDragDropEvent& DragDropEvent)
{
	if (SourceAction.IsValid())
	{
		SetSimpleFeedbackMessage(SourceAction->GetMenuDescription());
	}

	FDragDropOperation::OnDragged(DragDropEvent);
}

void FNiagaraStackDragOperation::Construct()
{
	// Create the drag-drop decorator window
	CursorDecoratorWindow = SWindow::MakeCursorDecorator();
	const bool bShowImmediately = false;
	FSlateApplication::Get().AddWindow(CursorDecoratorWindow.ToSharedRef(), bShowImmediately);
}

FNiagaraStackDragOperation::FNiagaraStackDragOperation()
	: bControlDrag(false)
	, bAltDrag(false)
{

}

bool FNiagaraStackDragOperation::HasFeedbackMessage()
{
	return CursorDecoratorWindow->GetContent() != SNullWidget::NullWidget;
}

void FNiagaraStackDragOperation::SetFeedbackMessage(const TSharedPtr<SWidget>& Message)
{
	if (Message.IsValid())
	{
		CursorDecoratorWindow->ShowWindow();
		CursorDecoratorWindow->SetContent
		(
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("Graph.ConnectorFeedback.Border"))
			[
				Message.ToSharedRef()
			]
		);
	}
	else
	{
		CursorDecoratorWindow->HideWindow();
		CursorDecoratorWindow->SetContent(SNullWidget::NullWidget);
	}
}

void FNiagaraStackDragOperation::SetSimpleFeedbackMessage(const FText& Message)
{
	SetFeedbackMessage(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.MaxWidth(500)
		.Padding(3.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(Message)
		]
	);
}

#undef LOCTEXT_NAMESPACE
