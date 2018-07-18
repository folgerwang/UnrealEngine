// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Stack/SNiagaraStackFunctionInputValue.h"
#include "ViewModels/Stack/NiagaraStackFunctionInput.h"
#include "NiagaraEditorModule.h"
#include "INiagaraEditorTypeUtilities.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEditorWidgetsStyle.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraActions.h"
#include "SNiagaraParameterEditor.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Framework/Application/SlateApplication.h"
#include "IStructureDetailsView.h"
#include "SDropTarget.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "NiagaraStackFunctionInputValue"

const float TextIconSize = 16;

void SNiagaraStackFunctionInputValue::Construct(const FArguments& InArgs, UNiagaraStackFunctionInput* InFunctionInput)
{
	FunctionInput = InFunctionInput;

	FunctionInput->OnValueChanged().AddSP(this, &SNiagaraStackFunctionInputValue::OnInputValueChanged);
	DisplayedLocalValueStruct = FunctionInput->GetLocalValueStruct();

	FMargin ItemPadding = FMargin(0);
	ChildSlot
	[
		SNew(SDropTarget)
		.OnAllowDrop(this, &SNiagaraStackFunctionInputValue::OnFunctionInputAllowDrop)
		.OnDrop(this, &SNiagaraStackFunctionInputValue::OnFunctionInputDrop)
		.Content()
		[
			// Values
			SNew(SHorizontalBox)
			.IsEnabled(this, &SNiagaraStackFunctionInputValue::GetInputEnabled)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(0, 0, 3, 0)
			[
				// Value Icon
				SNew(SBox)
				.WidthOverride(TextIconSize)
				.VAlign(VAlign_Center)
				.Visibility(this, &SNiagaraStackFunctionInputValue::GetInputIconVisibility)
				[
					SNew(STextBlock)
					.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
					.Text(this, &SNiagaraStackFunctionInputValue::GetInputIconText)
					.ToolTipText(this, &SNiagaraStackFunctionInputValue::GetInputIconToolTip)
					.ColorAndOpacity(this, &SNiagaraStackFunctionInputValue::GetInputIconColor)
				]
			]
			+ SHorizontalBox::Slot()
			[
				// TODO Don't generate all of these widgets for every input, only generate the ones that are used based on the value type.

				// Local struct
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SAssignNew(LocalValueStructContainer, SBox)
					.Visibility(this, &SNiagaraStackFunctionInputValue::GetValueWidgetVisibility, UNiagaraStackFunctionInput::EValueMode::Local)
					[
						ConstructLocalValueStructWidget()
					]
				]

				// Linked handle
				+ SOverlay::Slot()
				.Padding(0.0f, 0.0f, 0.0f, 2.0f)
				[
					SNew(SBox)
					.Visibility(this, &SNiagaraStackFunctionInputValue::GetValueWidgetVisibility, UNiagaraStackFunctionInput::EValueMode::Linked)
					.ToolTipText_UObject(InFunctionInput, &UNiagaraStackFunctionInput::GetTooltipText, UNiagaraStackFunctionInput::EValueMode::Linked)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
						.Text(this, &SNiagaraStackFunctionInputValue::GetLinkedValueHandleText)
					]
				]

				// Data Object
				+ SOverlay::Slot()
				.Padding(0.0f, 0.0f, 0.0f, 2.0f)
				[
					SNew(SBox)
					.Visibility(this, &SNiagaraStackFunctionInputValue::GetValueWidgetVisibility, UNiagaraStackFunctionInput::EValueMode::Data)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
						.Text(this, &SNiagaraStackFunctionInputValue::GetDataValueText)
					]
				]

				// Dynamic input name
				+ SOverlay::Slot()
				[
					SNew(SBox)
					.Visibility(this, &SNiagaraStackFunctionInputValue::GetValueWidgetVisibility, UNiagaraStackFunctionInput::EValueMode::Dynamic)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
						.Text(this, &SNiagaraStackFunctionInputValue::GetDynamicValueText)
						.OnDoubleClicked(this, &SNiagaraStackFunctionInputValue::DynamicInputTextDoubleClicked)
					]
				]

				// Expression input
				+ SOverlay::Slot()
				[
					SNew(SBox)
					.Visibility(this, &SNiagaraStackFunctionInputValue::GetValueWidgetVisibility, UNiagaraStackFunctionInput::EValueMode::Expression)
					.VAlign(VAlign_Center)
					[
						//SNew(SInlineEditableTextBlock)
						//.Style(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterInlineEditableText")
						//.Text(this, &SNiagaraStackFunctionInputValue::GetExpressionValueText)
						//.OnTextCommitted(this, &SNiagaraStackFunctionInputValue::OnExpressionTextCommitted)
						// SNew(SMultiLineEditableTextBox)
						//.AutoWrapText(false)
						SNew(SEditableTextBox)
						.IsReadOnly(false)
						.Text(this, &SNiagaraStackFunctionInputValue::GetExpressionValueText)
						.OnTextCommitted(this, &SNiagaraStackFunctionInputValue::OnExpressionTextCommitted)
					]
				]

				// Invalid input
				+ SOverlay::Slot()
				[
					SNew(SBox)
					.Visibility(this, &SNiagaraStackFunctionInputValue::GetValueWidgetVisibility, UNiagaraStackFunctionInput::EValueMode::Invalid)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
						.Text(this, &SNiagaraStackFunctionInputValue::GetInvalidValueText)
						.ToolTipText(this, &SNiagaraStackFunctionInputValue::GetInvalidValueToolTipText)
					]
				]
			]

			// Handle drop-down button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(3, 0, 0, 0)
			[
				SAssignNew(SetFunctionInputButton, SComboButton)
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.ForegroundColor(FSlateColor::UseForeground())
				.OnGetMenuContent(this, &SNiagaraStackFunctionInputValue::OnGetAvailableHandleMenu)
				.ContentPadding(FMargin(2))
				.MenuPlacement(MenuPlacement_BelowRightAnchor)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
			]

			// Reset Button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(3, 0, 0, 0)
			[
				SNew(SButton)
				.IsFocusable(false)
				.ToolTipText(LOCTEXT("ResetToolTip", "Reset to the default value"))
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.ContentPadding(0)
				.Visibility(this, &SNiagaraStackFunctionInputValue::GetResetButtonVisibility)
				.OnClicked(this, &SNiagaraStackFunctionInputValue::ResetButtonPressed)
				.Content()
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				]
			]

			// Reset to base Button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(3, 0, 0, 0)
			[
				SNew(SButton)
				.IsFocusable(false)
				.ToolTipText(LOCTEXT("ResetToBaseToolTip", "Reset this input to the value defined by the parent emitter"))
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.ContentPadding(0)
				.Visibility(this, &SNiagaraStackFunctionInputValue::GetResetToBaseButtonVisibility)
				.OnClicked(this, &SNiagaraStackFunctionInputValue::ResetToBaseButtonPressed)
				.Content()
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
					.ColorAndOpacity(FSlateColor(FLinearColor::Green))
				]
			]
		]
	];
}

bool SNiagaraStackFunctionInputValue::GetInputEnabled() const
{
	return FunctionInput->GetHasEditCondition() == false || FunctionInput->GetEditConditionEnabled();
}

EVisibility SNiagaraStackFunctionInputValue::GetValueWidgetVisibility(UNiagaraStackFunctionInput::EValueMode ValidMode) const
{
	return FunctionInput->GetValueMode() == ValidMode ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> SNiagaraStackFunctionInputValue::ConstructLocalValueStructWidget()
{
	LocalValueStructParameterEditor.Reset();
	LocalValueStructDetailsView.Reset();
	if (DisplayedLocalValueStruct.IsValid())
	{
		FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
		TSharedPtr<INiagaraEditorTypeUtilities, ESPMode::ThreadSafe> TypeEditorUtilities = NiagaraEditorModule.GetTypeUtilities(FunctionInput->GetInputType());
		if (TypeEditorUtilities.IsValid() && TypeEditorUtilities->CanCreateParameterEditor())
		{
			TSharedPtr<SNiagaraParameterEditor> ParameterEditor = TypeEditorUtilities->CreateParameterEditor(FunctionInput->GetInputType());
			ParameterEditor->UpdateInternalValueFromStruct(DisplayedLocalValueStruct.ToSharedRef());
			ParameterEditor->SetOnBeginValueChange(SNiagaraParameterEditor::FOnValueChange::CreateSP(
				this, &SNiagaraStackFunctionInputValue::ParameterBeginValueChange));
			ParameterEditor->SetOnEndValueChange(SNiagaraParameterEditor::FOnValueChange::CreateSP(
				this, &SNiagaraStackFunctionInputValue::ParameterEndValueChange));
			ParameterEditor->SetOnValueChanged(SNiagaraParameterEditor::FOnValueChange::CreateSP(
				this, &SNiagaraStackFunctionInputValue::ParameterValueChanged, TWeakPtr<SNiagaraParameterEditor>(ParameterEditor)));

			LocalValueStructParameterEditor = ParameterEditor;

			return SNew(SBox)
				.HAlign(ParameterEditor->GetHorizontalAlignment())
				.VAlign(ParameterEditor->GetVerticalAlignment())
				[
					ParameterEditor.ToSharedRef()
				];
		}
		else
		{
			FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

			TSharedRef<IStructureDetailsView> StructureDetailsView = PropertyEditorModule.CreateStructureDetailView(
				FDetailsViewArgs(false, false, false, FDetailsViewArgs::HideNameArea, true),
				FStructureDetailsViewArgs(),
				nullptr);

			StructureDetailsView->SetStructureData(DisplayedLocalValueStruct);
			StructureDetailsView->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &SNiagaraStackFunctionInputValue::ParameterPropertyValueChanged);

			LocalValueStructDetailsView = StructureDetailsView;
			return StructureDetailsView->GetWidget().ToSharedRef();
		}
	}
	else
	{
		return SNullWidget::NullWidget;
	}
}

void SNiagaraStackFunctionInputValue::OnInputValueChanged()
{
	TSharedPtr<FStructOnScope> NewLocalValueStruct = FunctionInput->GetLocalValueStruct();
	if (DisplayedLocalValueStruct == NewLocalValueStruct)
	{
		if (LocalValueStructParameterEditor.IsValid())
		{
			LocalValueStructParameterEditor->UpdateInternalValueFromStruct(DisplayedLocalValueStruct.ToSharedRef());
		}
		if (LocalValueStructDetailsView.IsValid())
		{
			LocalValueStructDetailsView->SetStructureData(TSharedPtr<FStructOnScope>());
			LocalValueStructDetailsView->SetStructureData(DisplayedLocalValueStruct);
		}
	}
	else
	{
		DisplayedLocalValueStruct = NewLocalValueStruct;
		LocalValueStructContainer->SetContent(ConstructLocalValueStructWidget());
	}
}

void SNiagaraStackFunctionInputValue::ParameterBeginValueChange()
{
	FunctionInput->NotifyBeginLocalValueChange();
}

void SNiagaraStackFunctionInputValue::ParameterEndValueChange()
{
	FunctionInput->NotifyEndLocalValueChange();
}

void SNiagaraStackFunctionInputValue::ParameterValueChanged(TWeakPtr<SNiagaraParameterEditor> ParameterEditor)
{
	TSharedPtr<SNiagaraParameterEditor> ParameterEditorPinned = ParameterEditor.Pin();
	if (ParameterEditorPinned.IsValid())
	{
		ParameterEditorPinned->UpdateStructFromInternalValue(DisplayedLocalValueStruct.ToSharedRef());
		FunctionInput->SetLocalValue(DisplayedLocalValueStruct.ToSharedRef());
	}
}

void SNiagaraStackFunctionInputValue::ParameterPropertyValueChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
	FunctionInput->SetLocalValue(DisplayedLocalValueStruct.ToSharedRef());
}

FText SNiagaraStackFunctionInputValue::GetLinkedValueHandleText() const
{
	return FText::FromName(FunctionInput->GetLinkedValueHandle().GetParameterHandleString());
}

FText SNiagaraStackFunctionInputValue::GetDataValueText() const
{
	if (FunctionInput->GetDataValueObject() != nullptr)
	{
		return FunctionInput->GetInputType().GetClass()->GetDisplayNameText();
	}
	else
	{
		return FText::Format(LOCTEXT("InvalidDataObjectFormat", "{0} (Invalid)"), FunctionInput->GetInputType().GetClass()->GetDisplayNameText());
	}
}

FText SNiagaraStackFunctionInputValue::GetDynamicValueText() const
{
	if (FunctionInput->GetDynamicInputNode() != nullptr)
	{
		return FText::FromString(FName::NameToDisplayString(FunctionInput->GetDynamicInputNode()->GetFunctionName(), false));
	}
	else
	{
		return LOCTEXT("InvalidDynamicDisplayName", "(Invalid)");
	}
}

FText SNiagaraStackFunctionInputValue::GetExpressionValueText() const
{
	if (FunctionInput->GetExpressionNode() != nullptr)
	{
		return FunctionInput->GetExpressionNode()->GetHlslText();
	}
	else
	{
		return LOCTEXT("InvalidDynamicDisplayName", "(Invalid)");
	}
}

void SNiagaraStackFunctionInputValue::OnExpressionTextCommitted(const FText& Name, ETextCommit::Type CommitInfo)
{
	if (FunctionInput->GetExpressionNode() != nullptr)
	{
		FunctionInput->GetExpressionNode()->OnCustomHlslTextCommitted(Name, CommitInfo);
	}
}

FText SNiagaraStackFunctionInputValue::GetInvalidValueText() const
{
	if (FunctionInput->CanReset())
	{
		return LOCTEXT("InvalidResetLabel", "Unsupported value - Reset to fix.");
	}
	else
	{
		return LOCTEXT("InvalidLabel", "Custom value");
	}
}

FText SNiagaraStackFunctionInputValue::GetInvalidValueToolTipText() const
{
	if (FunctionInput->CanReset())
	{
		return LOCTEXT("InvalidResetToolTip", "This input has an unsupported value assigned in the stack.\nUse the reset button to remove the unsupported value.");
	}
	else
	{
		return LOCTEXT("InvalidToolTip", "The script that defines the source of this input has\n a custom default value that can not be displayed in the stack view.\nYou can set a local override value using the drop down menu.");
	}
}

FReply SNiagaraStackFunctionInputValue::DynamicInputTextDoubleClicked()
{
	UNiagaraNodeFunctionCall* DynamicInputNode = FunctionInput->GetDynamicInputNode();
	if (DynamicInputNode->FunctionScript != nullptr && DynamicInputNode->FunctionScript->IsAsset())
	{
		FAssetEditorManager::Get().OpenEditorForAsset(DynamicInputNode->FunctionScript);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

TSharedRef<SExpanderArrow> SNiagaraStackFunctionInputValue::CreateCustomNiagaraFunctionInputActionExpander(const FCustomExpanderData& ActionMenuData)
{
	return SNew(SNiagaraFunctionInputActionMenuExpander, ActionMenuData);
}

TSharedRef<SWidget> SNiagaraStackFunctionInputValue::OnGetAvailableHandleMenu()
{
	TSharedPtr<SGraphActionMenu> SelectInputFunctionMenu;
	TSharedRef<SBorder> MenuWidget = SNew(SBorder)
	.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
	.Padding(5)
	[
		SNew(SBox)
		.WidthOverride(300)
		.HeightOverride(400)
		[
			SAssignNew(SelectInputFunctionMenu, SGraphActionMenu)
			.OnActionSelected(this, &SNiagaraStackFunctionInputValue::OnActionSelected)
			.OnCollectAllActions(this, &SNiagaraStackFunctionInputValue::CollectAllActions)
			.AutoExpandActionMenu(false)
			.ShowFilterTextBox(true)
			.OnCreateCustomRowExpander_Static(&CreateCustomNiagaraFunctionInputActionExpander)
		]
	];

	SetFunctionInputButton->SetMenuContentWidgetToFocus(SelectInputFunctionMenu->GetFilterTextBox()->AsShared());
	return MenuWidget;
}

void SNiagaraStackFunctionInputValue::OnActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& SelectedActions, ESelectInfo::Type InSelectionType)
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

void SNiagaraStackFunctionInputValue::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
{
	// Set a local value
	{
		bool bCanSetLocalValue =
			FunctionInput->GetValueMode() != UNiagaraStackFunctionInput::EValueMode::Local &&
			FunctionInput->GetInputType().IsDataInterface() == false;

		const FText NameText = LOCTEXT("LocalValue", "Set a local value");
		const FText Tooltip = FText::Format(LOCTEXT("LocalValueToolTip", "Set a local editable value for this input."), NameText);
		const FText CategoryName = LOCTEXT("LocalValueCategory", "Local");
		TSharedPtr<FNiagaraMenuAction> SetLocalValueAction(
			new FNiagaraMenuAction(CategoryName, NameText, Tooltip, 0, FText(),
				FNiagaraMenuAction::FOnExecuteStackAction::CreateSP(this, &SNiagaraStackFunctionInputValue::SetToLocalValue),
				FNiagaraMenuAction::FCanExecuteStackAction::CreateLambda([=]() { return bCanSetLocalValue; })));
		OutAllActions.AddAction(SetLocalValueAction);
	}


	// Add a dynamic input
	{
		const FText CategoryName = LOCTEXT("DynamicInputValueCategory", "Dynamic Inputs");
		TArray<UNiagaraScript*> DynamicInputScripts;
		FunctionInput->GetAvailableDynamicInputs(DynamicInputScripts);
		for (UNiagaraScript* DynamicInputScript : DynamicInputScripts)
		{
			const FText DynamicInputText = FText::FromString(FName::NameToDisplayString(DynamicInputScript->GetName(), false));
			const FText Tooltip = FNiagaraEditorUtilities::FormatScriptAssetDescription(DynamicInputScript->Description, *DynamicInputScript->GetPathName());
			TSharedPtr<FNiagaraMenuAction> DynamicInputAction(new FNiagaraMenuAction(CategoryName, DynamicInputText, Tooltip, 0, DynamicInputScript->Keywords,
				FNiagaraMenuAction::FOnExecuteStackAction::CreateSP(this, &SNiagaraStackFunctionInputValue::DynamicInputScriptSelected, DynamicInputScript)));
			OutAllActions.AddAction(DynamicInputAction);
		}
	}

	// Link existing attribute
	TArray<FNiagaraParameterHandle> AvailableHandles;
	FunctionInput->GetAvailableParameterHandles(AvailableHandles);

	TArray<FNiagaraParameterHandle> UserHandles;
	TArray<FNiagaraParameterHandle> EngineHandles;
	TArray<FNiagaraParameterHandle> SystemHandles;
	TArray<FNiagaraParameterHandle> EmitterHandles;
	TArray<FNiagaraParameterHandle> ParticleAttributeHandles;
	TArray<FNiagaraParameterHandle> OtherHandles;
	for (const FNiagaraParameterHandle AvailableHandle : AvailableHandles)
	{
		if (AvailableHandle.IsUserHandle())
		{
			UserHandles.Add(AvailableHandle);
		}
		else if (AvailableHandle.IsEngineHandle())
		{
			EngineHandles.Add(AvailableHandle);
		}
		else if (AvailableHandle.IsSystemHandle())
		{
			SystemHandles.Add(AvailableHandle);
		}
		else if (AvailableHandle.IsEmitterHandle())
		{
			EmitterHandles.Add(AvailableHandle);
		}
		else if (AvailableHandle.IsParticleAttributeHandle())
		{
			ParticleAttributeHandles.Add(AvailableHandle);
		}
		else
		{
			OtherHandles.Add(AvailableHandle);
		}
	}

	{
		const FString RootCategoryName = FString("Link Inputs");
		auto AddMenuItemsForHandleList = [&](const TArray<FNiagaraParameterHandle>& Handles, const FText& SectionDisplay)
		{
			const FText MapInputFormat = LOCTEXT("LinkInputFormat", "Link this input to {0}");
			for (const FNiagaraParameterHandle& Handle : Handles)
			{
				const FText DisplayName = FText::FromString(FName::NameToDisplayString(Handle.GetName().ToString(), false));
				const FText Tooltip = FText::Format(MapInputFormat, FText::FromName(Handle.GetParameterHandleString()));
				TSharedPtr<FNiagaraMenuAction> LinkAction(new FNiagaraMenuAction(SectionDisplay, DisplayName, Tooltip, 0, FText(),
					FNiagaraMenuAction::FOnExecuteStackAction::CreateSP(this, &SNiagaraStackFunctionInputValue::ParameterHandleSelected, Handle)));
				OutAllActions.AddAction(LinkAction, RootCategoryName);
			}
		};

		AddMenuItemsForHandleList(UserHandles, LOCTEXT("UserSection", "User Exposed"));
		AddMenuItemsForHandleList(EngineHandles, LOCTEXT("EngineSection", "Engine"));
		AddMenuItemsForHandleList(SystemHandles, LOCTEXT("SystemSection", "System"));
		AddMenuItemsForHandleList(EmitterHandles, LOCTEXT("EmitterSection", "Emitter"));
		AddMenuItemsForHandleList(ParticleAttributeHandles, LOCTEXT("ParticleAttributeSection", "Particle Attribute"));
		AddMenuItemsForHandleList(OtherHandles, LOCTEXT("OtherSection", "Other"));
	}

	// Read from new attribute
	{
		const FText CategoryName = LOCTEXT("MakeCategory", "Make");

		TArray<FName> AvailableNamespaces;
		FunctionInput->GetNamespacesForNewParameters(AvailableNamespaces);

		TArray<FString> InputNames;
		for (int32 i = FunctionInput->GetInputParameterHandlePath().Num() - 1; i >= 0; i--)
		{
			InputNames.Add(FunctionInput->GetInputParameterHandlePath()[i].GetName().ToString());
		}
		FName InputName = *FString::Join(InputNames, TEXT("."));

		for (const FName AvailableNamespace : AvailableNamespaces)
		{
			FNiagaraParameterHandle HandleToRead(AvailableNamespace, InputName);
			bool bCanExecute = AvailableHandles.Contains(HandleToRead) == false;

			FFormatNamedArguments Args;
			Args.Add(TEXT("AvailableNamespace"), FText::FromName(AvailableNamespace));

			const FText DisplayName = FText::Format(LOCTEXT("ReadLabelFormat", "Read from new {AvailableNamespace} parameter"), Args);
			const FText Tooltip = FText::Format(LOCTEXT("ReadToolTipFormat", "Read this input from a new parameter in the {AvailableNamespace} namespace."), Args);
			TSharedPtr<FNiagaraMenuAction> MakeAction(
				new FNiagaraMenuAction(CategoryName, DisplayName, Tooltip, 0, FText(),
					FNiagaraMenuAction::FOnExecuteStackAction::CreateSP(this, &SNiagaraStackFunctionInputValue::ParameterHandleSelected, HandleToRead),
					FNiagaraMenuAction::FCanExecuteStackAction::CreateLambda([=]() { return bCanExecute; })));
			OutAllActions.AddAction(MakeAction);
		}
	}

	{
		const FText CategoryName = LOCTEXT("ExpressionCategory", "Expression");
		const FText DisplayName = LOCTEXT("ExpressionLabel", "Make new expression");
		const FText Tooltip = LOCTEXT("ExpressionToolTipl", "Resolve this variable with a custom expression.");
		TSharedPtr<FNiagaraMenuAction> ExpressionAction(new FNiagaraMenuAction(CategoryName, DisplayName, Tooltip, 0, FText(),
			FNiagaraMenuAction::FOnExecuteStackAction::CreateSP(this, &SNiagaraStackFunctionInputValue::CustomExpressionSelected)));
		OutAllActions.AddAction(ExpressionAction);
	}

	if (FunctionInput->CanDeleteInput())
	{
		const FText NameText = LOCTEXT("DeleteInput", "Remove");
		const FText Tooltip = FText::Format(LOCTEXT("DeleteInputTooltip", "Remove input from module."), NameText);
		TSharedPtr<FNiagaraMenuAction> SetLocalValueAction(
			new FNiagaraMenuAction(FText::GetEmpty(), NameText, Tooltip, 0, FText::GetEmpty(),
				FNiagaraMenuAction::FOnExecuteStackAction::CreateUObject(FunctionInput, &UNiagaraStackFunctionInput::DeleteInput),
				FNiagaraMenuAction::FCanExecuteStackAction::CreateUObject(FunctionInput, &UNiagaraStackFunctionInput::CanDeleteInput)));
		OutAllActions.AddAction(SetLocalValueAction);
	}
}

void SNiagaraStackFunctionInputValue::SetToLocalValue()
{
	const UScriptStruct* LocalValueStruct = FunctionInput->GetInputType().GetScriptStruct();
	if (LocalValueStruct != nullptr)
	{
		TSharedRef<FStructOnScope> LocalValue = MakeShared<FStructOnScope>(LocalValueStruct);
		TArray<uint8> DefaultValueData;
		FNiagaraEditorUtilities::GetTypeDefaultValue(FunctionInput->GetInputType(), DefaultValueData);
		if (DefaultValueData.Num() == LocalValueStruct->GetStructureSize())
		{
			FMemory::Memcpy(LocalValue->GetStructMemory(), DefaultValueData.GetData(), DefaultValueData.Num());
			FunctionInput->SetLocalValue(LocalValue);
		}
	}
}

void SNiagaraStackFunctionInputValue::DynamicInputScriptSelected(UNiagaraScript* DynamicInputScript)
{
	FunctionInput->SetDynamicInput(DynamicInputScript);
}

void SNiagaraStackFunctionInputValue::CustomExpressionSelected()
{
	FunctionInput->SetCustomExpression(TEXT("// Insert expression here"));
}

void SNiagaraStackFunctionInputValue::ParameterHandleSelected(FNiagaraParameterHandle Handle)
{
	FunctionInput->SetLinkedValueHandle(Handle);
}

EVisibility SNiagaraStackFunctionInputValue::GetResetButtonVisibility() const
{
	return FunctionInput->CanReset() ? EVisibility::Visible : EVisibility::Hidden;
}

FReply SNiagaraStackFunctionInputValue::ResetButtonPressed() const
{
	FunctionInput->Reset();
	return FReply::Handled();
}

EVisibility SNiagaraStackFunctionInputValue::GetResetToBaseButtonVisibility() const
{
	if (FunctionInput->EmitterHasBase())
	{
		return FunctionInput->CanResetToBase() ? EVisibility::Visible : EVisibility::Hidden;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

FReply SNiagaraStackFunctionInputValue::ResetToBaseButtonPressed() const
{
	FunctionInput->ResetToBase();
	return FReply::Handled();
}

EVisibility SNiagaraStackFunctionInputValue::GetInputIconVisibility() const
{
	return FunctionInput->GetValueMode() == UNiagaraStackFunctionInput::EValueMode::Local
		? EVisibility::Collapsed
		: EVisibility::Visible;
}

FText SNiagaraStackFunctionInputValue::GetInputIconText() const
{
	switch (FunctionInput->GetValueMode())
	{
	case UNiagaraStackFunctionInput::EValueMode::Linked:
		return FText::FromString(FString(TEXT("\xf0C1") /* fa-link */));
	case UNiagaraStackFunctionInput::EValueMode::Data:
		return FText::FromString(FString(TEXT("\xf1C0") /* fa-database */));
	case UNiagaraStackFunctionInput::EValueMode::Dynamic:
		return FText::FromString(FString(TEXT("\xf201") /* fa-line-chart */));
	case UNiagaraStackFunctionInput::EValueMode::Expression:
		return FText::FromString(FString(TEXT("\xf120") /* fa-terminal */));
	case UNiagaraStackFunctionInput::EValueMode::Invalid:
		return FunctionInput->CanReset()
			? FText::FromString(FString(TEXT("\xf128") /* fa-question */))
			: FText::FromString(FString(TEXT("\xf005") /* fa-star */));
	default:
		return FText::FromString(FString(TEXT("\xf128") /* fa-question */));
	}
}

FText SNiagaraStackFunctionInputValue::GetInputIconToolTip() const
{
	static const FText InvalidText = LOCTEXT("InvalidInputIconToolTip", "Unsupported value.  Check the graph for issues.");
	switch (FunctionInput->GetValueMode())
	{
	case UNiagaraStackFunctionInput::EValueMode::Linked:
		return LOCTEXT("LinkInputIconToolTip", "Linked Value");
	case UNiagaraStackFunctionInput::EValueMode::Data:
		return LOCTEXT("DataInterfaceInputIconToolTip", "Data Value");
	case UNiagaraStackFunctionInput::EValueMode::Dynamic:
		return LOCTEXT("DynamicInputIconToolTip", "Dynamic Value");
	case UNiagaraStackFunctionInput::EValueMode::Expression:
		return LOCTEXT("ExpressionInputIconToolTip", "Custom Expression");
	case UNiagaraStackFunctionInput::EValueMode::Invalid:
		return FunctionInput->CanReset()
			? InvalidText
			: LOCTEXT("CustomInputIconToolTip", "Custom value");
	default:
		return InvalidText;
	}
}

FSlateColor SNiagaraStackFunctionInputValue::GetInputIconColor() const
{
	switch (FunctionInput->GetValueMode())
	{
	case UNiagaraStackFunctionInput::EValueMode::Linked:
		return FLinearColor(FColor::Purple);
	case UNiagaraStackFunctionInput::EValueMode::Data:
		return FLinearColor(FColor::Yellow);
	case UNiagaraStackFunctionInput::EValueMode::Dynamic:
		return FLinearColor(FColor::Cyan);
	case UNiagaraStackFunctionInput::EValueMode::Expression:
		return FLinearColor(FColor::Green);
	case UNiagaraStackFunctionInput::EValueMode::Invalid:
	default:
		return FLinearColor(FColor::White);
	}
}

FReply SNiagaraStackFunctionInputValue::OnFunctionInputDrop(TSharedPtr<FDragDropOperation> DragDropOperation)
{
	if (DragDropOperation->IsOfType<FNiagaraStackDragOperation>())
	{
		TSharedPtr<FNiagaraStackDragOperation> InputDragDropOperation = StaticCastSharedPtr<FNiagaraStackDragOperation>(DragDropOperation);
		TSharedPtr<FNiagaraParameterAction> Action = StaticCastSharedPtr<FNiagaraParameterAction>(InputDragDropOperation->GetAction());
		if (Action.IsValid())
		{
			FunctionInput->SetLinkedValueHandle(FNiagaraParameterHandle(Action->GetParameter().GetName()));
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

bool SNiagaraStackFunctionInputValue::OnFunctionInputAllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation)
{
	if (FunctionInput && DragDropOperation->IsOfType<FNiagaraStackDragOperation>())
	{
		TSharedPtr<FNiagaraStackDragOperation> InputDragDropOperation = StaticCastSharedPtr<FNiagaraStackDragOperation>(DragDropOperation);
		TSharedPtr<FNiagaraParameterAction> Action = StaticCastSharedPtr<FNiagaraParameterAction>(InputDragDropOperation->GetAction());
		if (Action->GetParameter().GetType() == FunctionInput->GetInputType() && FNiagaraStackGraphUtilities::ParameterAllowedInExecutionCategory(Action->GetParameter().GetName(), FunctionInput->GetExecutionCategoryName()))
		{
			return true;
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
