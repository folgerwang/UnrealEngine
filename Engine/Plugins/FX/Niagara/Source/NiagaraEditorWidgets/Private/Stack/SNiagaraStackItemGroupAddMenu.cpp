// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "Stack/SNiagaraStackItemGroupAddMenu.h"
#include "EditorStyleSet.h"
#include "NiagaraActions.h"
#include "ViewModels/Stack/INiagaraStackItemGroupAddUtilities.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "EdGraph/EdGraph.h"
#include "SGraphActionMenu.h"
#include "Framework/Application/SlateApplication.h"

void SNiagaraStackItemGroupAddMenu::Construct(const FArguments& InArgs, INiagaraStackItemGroupAddUtilities* InAddUtilities, int32 InInsertIndex)
{
	AddUtilities = InAddUtilities;
	InsertIndex = InInsertIndex;
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		.Padding(5)
		[
			SNew(SBox)
			.WidthOverride(300)
			.HeightOverride(400)
			[
				SAssignNew(AddMenu, SGraphActionMenu)
				.OnActionSelected(this, &SNiagaraStackItemGroupAddMenu::OnActionSelected)
				.OnCollectAllActions(this, &SNiagaraStackItemGroupAddMenu::CollectAllAddActions)
				.AutoExpandActionMenu(AddUtilities->GetAutoExpandAddActions())
				.ShowFilterTextBox(true)
			]
		]
	];
}

TSharedPtr<SEditableTextBox> SNiagaraStackItemGroupAddMenu::GetFilterTextBox()
{
	return AddMenu->GetFilterTextBox();
}

void SNiagaraStackItemGroupAddMenu::CollectAllAddActions(FGraphActionListBuilderBase& OutAllActions)
{
	if (OutAllActions.OwnerOfTemporaries == nullptr)
	{
		OutAllActions.OwnerOfTemporaries = NewObject<UEdGraph>((UObject*)GetTransientPackage());
	}

	TArray<TSharedRef<INiagaraStackItemGroupAddAction>> AddActions;
	AddUtilities->GenerateAddActions(AddActions);

	for (TSharedRef<INiagaraStackItemGroupAddAction> AddAction : AddActions)
	{
		TSharedPtr<FNiagaraMenuAction> NewNodeAction(
			new FNiagaraMenuAction(AddAction->GetCategory(), AddAction->GetDisplayName(), AddAction->GetDescription(), 0, AddAction->GetKeywords(),
				FNiagaraMenuAction::FOnExecuteStackAction::CreateRaw(AddUtilities, &INiagaraStackItemGroupAddUtilities::ExecuteAddAction, AddAction, InsertIndex)));
		OutAllActions.AddAction(NewNodeAction);
	}
}

void SNiagaraStackItemGroupAddMenu::OnActionSelected(const TArray< TSharedPtr<FEdGraphSchemaAction> >& SelectedActions, ESelectInfo::Type InSelectionType)
{
	if (InSelectionType == ESelectInfo::OnMouseClick || InSelectionType == ESelectInfo::OnKeyPress || SelectedActions.Num() == 0)
	{
		for (int32 ActionIndex = 0; ActionIndex < SelectedActions.Num(); ActionIndex++)
		{
			TSharedPtr<FNiagaraMenuAction> CurrentAction = StaticCastSharedPtr<FNiagaraMenuAction>(SelectedActions[ActionIndex]);

			if (CurrentAction.IsValid())
			{
				FSlateApplication::Get().DismissAllMenus();
				CurrentAction->ExecuteAction();
			}
		}
	}
}
