// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraTypeCustomizations.h"
#include "CoreMinimal.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "NiagaraTypes.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "IDetailChildrenBuilder.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraEmitter.h"
#include "SGraphActionMenu.h"
#include "Framework/Application/SlateApplication.h"
#include "ScopedTransaction.h"
#include "NiagaraParameterMapHistory.h"
#include "NiagaraScriptSource.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraConstants.h"
#include "NiagaraNodeParameterMapBase.h"
#include "Widgets/Input/STextComboBox.h"

#define LOCTEXT_NAMESPACE "FNiagaraVariableAttributeBindingCustomization"


void FNiagaraNumericCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TSharedPtr<IPropertyHandle> ValueHandle = PropertyHandle->GetChildHandle(TEXT("Value"));

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(ValueHandle.IsValid() ? 125.f : 200.f)
		[
			// Some Niagara numeric types have no value so in that case just display their type name
			ValueHandle.IsValid()
			? ValueHandle->CreatePropertyValueWidget()
			: SNew(STextBlock)
			  .Text(FText::FromString(FName::NameToDisplayString(Cast<UStructProperty>(PropertyHandle->GetProperty())->Struct->GetName(), false)))
			  .Font(IDetailLayoutBuilder::GetDetailFont())
		];
}


void FNiagaraBoolCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	ValueHandle = PropertyHandle->GetChildHandle(TEXT("Value"));

	static const FName DefaultForegroundName("DefaultForeground");

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &FNiagaraBoolCustomization::OnCheckStateChanged)
			.IsChecked(this, &FNiagaraBoolCustomization::OnGetCheckState)
			.ForegroundColor(FEditorStyle::GetSlateColor(DefaultForegroundName))
			.Padding(0.0f)
		];
}

ECheckBoxState FNiagaraBoolCustomization::OnGetCheckState() const
{
	ECheckBoxState CheckState = ECheckBoxState::Undetermined;
	int32 Value;
	FPropertyAccess::Result Result = ValueHandle->GetValue(Value);
	if (Result == FPropertyAccess::Success)
	{
		CheckState = Value == FNiagaraBool::True ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return CheckState;
}

void FNiagaraBoolCustomization::OnCheckStateChanged(ECheckBoxState InNewState)
{
	if (InNewState == ECheckBoxState::Checked)
	{
		ValueHandle->SetValue(FNiagaraBool::True);
	}
	else
	{
		ValueHandle->SetValue(FNiagaraBool::False);
	}
}

void FNiagaraMatrixCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);

	for (uint32 ChildNum = 0; ChildNum < NumChildren; ++ChildNum)
	{
		ChildBuilder.AddProperty(PropertyHandle->GetChildHandle(ChildNum).ToSharedRef());
	}
}

FText FNiagaraVariableAttributeBindingCustomization::GetCurrentText() const
{
	if (BaseEmitter && TargetVariableBinding)
	{
		return FText::FromName(TargetVariableBinding->BoundVariable.GetName());
	}
	return FText::FromString(TEXT("Missing"));
}

FText FNiagaraVariableAttributeBindingCustomization::GetTooltipText() const
{
	if (BaseEmitter && TargetVariableBinding)
	{
		FString DefaultValueStr = TargetVariableBinding->DefaultValueIfNonExistent.GetName().ToString();
		
		if (!TargetVariableBinding->DefaultValueIfNonExistent.GetName().IsValid() || TargetVariableBinding->DefaultValueIfNonExistent.IsDataAllocated() == true)
		{
			DefaultValueStr = TargetVariableBinding->DefaultValueIfNonExistent.GetType().ToString(TargetVariableBinding->DefaultValueIfNonExistent.GetData());
			DefaultValueStr.TrimEndInline();
		}

		FText TooltipDesc = FText::Format(LOCTEXT("BindingTooltip", "Use the variable \"{0}\" if it exists, otherwise use the default \"{1}\" "), FText::FromName(TargetVariableBinding->BoundVariable.GetName()),
			FText::FromString(DefaultValueStr));
		return TooltipDesc;
	}
	return FText::FromString(TEXT("Missing"));
}

TSharedRef<SWidget> FNiagaraVariableAttributeBindingCustomization::OnGetMenuContent() const
{
	FGraphActionMenuBuilder MenuBuilder;

	return SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		.Padding(5)
		[
			SNew(SBox)
			[
				SNew(SGraphActionMenu)
				.OnActionSelected(this, &FNiagaraVariableAttributeBindingCustomization::OnActionSelected)
				.OnCreateWidgetForAction(SGraphActionMenu::FOnCreateWidgetForAction::CreateSP(this, &FNiagaraVariableAttributeBindingCustomization::OnCreateWidgetForAction))
				.OnCollectAllActions(this, &FNiagaraVariableAttributeBindingCustomization::CollectAllActions)
				.AutoExpandActionMenu(false)
				.ShowFilterTextBox(true)
			]
		];
}

TArray<FName> FNiagaraVariableAttributeBindingCustomization::GetNames(UNiagaraEmitter* InEmitter) const
{
	TArray<FName> Names;

	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(InEmitter->GraphSource);
	if (Source)
	{
		TArray<FNiagaraParameterMapHistory> Histories =  UNiagaraNodeParameterMapBase::GetParameterMaps(Source->NodeGraph);
		for (const FNiagaraParameterMapHistory& History : Histories)
		{
			for (const FNiagaraVariable& Var : History.Variables)
			{
				if (FNiagaraParameterMapHistory::IsAttribute(Var) && Var.GetType() == TargetVariableBinding->BoundVariable.GetType())
				{
					Names.AddUnique(Var.GetName());
				}
			}
		}
		
	}
	return Names;
}

void FNiagaraVariableAttributeBindingCustomization::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
{
	TArray<FName> EventNames = GetNames(BaseEmitter);
	FName EmitterName = BaseEmitter->GetFName();
	for (FName EventName : EventNames)
	{
		FText CategoryName = FText();
		FString DisplayNameString = FName::NameToDisplayString(EventName.ToString(), false);
		const FText NameText = FText::FromString(DisplayNameString);
		const FText TooltipDesc = FText::Format(LOCTEXT("SetFunctionPopupTooltip", "Use the variable \"{0}\" "), FText::FromString(DisplayNameString));
		TSharedPtr<FNiagaraStackAssetAction_VarBind> NewNodeAction(new FNiagaraStackAssetAction_VarBind(EventName, CategoryName, NameText,
			TooltipDesc, 0, FText()));
		OutAllActions.AddAction(NewNodeAction);
	}
}

TSharedRef<SWidget> FNiagaraVariableAttributeBindingCustomization::OnCreateWidgetForAction(struct FCreateWidgetForActionData* const InCreateData)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(InCreateData->Action->GetMenuDescription())
			.ToolTipText(InCreateData->Action->GetTooltipDescription())
		];
}


void FNiagaraVariableAttributeBindingCustomization::OnActionSelected(const TArray< TSharedPtr<FEdGraphSchemaAction> >& SelectedActions, ESelectInfo::Type InSelectionType)
{
	if (InSelectionType == ESelectInfo::OnMouseClick || InSelectionType == ESelectInfo::OnKeyPress || SelectedActions.Num() == 0)
	{
		for (int32 ActionIndex = 0; ActionIndex < SelectedActions.Num(); ActionIndex++)
		{
			TSharedPtr<FEdGraphSchemaAction> CurrentAction = SelectedActions[ActionIndex];

			if (CurrentAction.IsValid())
			{
				FSlateApplication::Get().DismissAllMenus();
				FNiagaraStackAssetAction_VarBind* EventSourceAction = (FNiagaraStackAssetAction_VarBind*)CurrentAction.Get();
				ChangeSource(EventSourceAction->VarName);
			}
		}
	}
}

void FNiagaraVariableAttributeBindingCustomization::ChangeSource(FName InVarName)
{
	FScopedTransaction Transaction(FText::Format(LOCTEXT("ChangeSource", " Change Variable Source to \"{0}\" "), FText::FromName(InVarName)));
	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);
	for (UObject* Obj : Objects)
	{
		Obj->Modify();
	}

	PropertyHandle->NotifyPreChange();
	TargetVariableBinding->BoundVariable.SetName(InVarName);
	TargetVariableBinding->DataSetVariable = FNiagaraConstants::GetAttributeAsDataSetKey(TargetVariableBinding->BoundVariable);
	PropertyHandle->NotifyPostChange();
	PropertyHandle->NotifyFinishedChangingProperties();
}

void FNiagaraVariableAttributeBindingCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyHandle = InPropertyHandle;
	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);
	bool bAddDefault = true;
	if (Objects.Num() == 1)
	{
		UNiagaraRendererProperties* RenderProps = Cast<UNiagaraRendererProperties>(Objects[0]);
		if (RenderProps)
		{
			BaseEmitter = Cast<UNiagaraEmitter>(RenderProps->GetOuter());

			if (BaseEmitter)
			{
				TargetVariableBinding = (FNiagaraVariableAttributeBinding*)PropertyHandle->GetValueBaseAddress((uint8*)Objects[0]);
				
				HeaderRow
					.NameContent()
					[
						PropertyHandle->CreatePropertyNameWidget()
					]
					.ValueContent()
					.MaxDesiredWidth(200.f)
					[
						SNew(SComboButton)
						.OnGetMenuContent(this, &FNiagaraVariableAttributeBindingCustomization::OnGetMenuContent)
						.ContentPadding(1)
						.ToolTipText(this, &FNiagaraVariableAttributeBindingCustomization::GetTooltipText)
						.ButtonContent()
						[
							SNew(STextBlock)
							.Text(this, &FNiagaraVariableAttributeBindingCustomization::GetCurrentText)
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
					];
				bAddDefault = false;
			}
		}
	}
	

	if (bAddDefault)
	{
		HeaderRow
			.NameContent()
			[
				PropertyHandle->CreatePropertyNameWidget()
			]
		.ValueContent()
			.MaxDesiredWidth(200.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FName::NameToDisplayString(Cast<UStructProperty>(PropertyHandle->GetProperty())->Struct->GetName(), false)))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	}
}


#undef LOCTEXT_NAMESPACE
