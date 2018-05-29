// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackFunctionInputCollection.h"
#include "ViewModels/Stack/NiagaraStackFunctionInput.h"
#include "ViewModels/Stack/NiagaraStackInputCategory.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraEmitterEditorData.h"
#include "EdGraphSchema_Niagara.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraNodeParameterMapSet.h"
#include "NiagaraDataInterface.h"
#include "NiagaraConstants.h"
#include "NiagaraGraph.h"
#include "NiagaraStackEditorData.h"

#include "EdGraph/EdGraphPin.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "UNiagaraStackFunctionInputCollection"

UNiagaraStackFunctionInputCollection::UNiagaraStackFunctionInputCollection()
	: ModuleNode(nullptr)
	, InputFunctionCallNode(nullptr)
	, bShouldShowInStack(true)
{
}

UNiagaraNodeFunctionCall* UNiagaraStackFunctionInputCollection::GetModuleNode() const
{
	return ModuleNode;
}

UNiagaraNodeFunctionCall* UNiagaraStackFunctionInputCollection::GetInputFunctionCallNode() const
{
	return InputFunctionCallNode;
}

void UNiagaraStackFunctionInputCollection::Initialize(
	FRequiredEntryData InRequiredEntryData,
	UNiagaraNodeFunctionCall& InModuleNode,
	UNiagaraNodeFunctionCall& InInputFunctionCallNode,
	FString InOwnerStackItemEditorDataKey)
{
	checkf(ModuleNode == nullptr && InputFunctionCallNode == nullptr, TEXT("Can not set the node more than once."));
	FString InputCollectionStackEditorDataKey = FString::Printf(TEXT("%s-Inputs"), *InInputFunctionCallNode.NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
	Super::Initialize(InRequiredEntryData, false, InOwnerStackItemEditorDataKey, InputCollectionStackEditorDataKey);
	ModuleNode = &InModuleNode;
	InputFunctionCallNode = &InInputFunctionCallNode;
	InputFunctionCallNode->OnInputsChanged().AddUObject(this, &UNiagaraStackFunctionInputCollection::OnFunctionInputsChanged);
}

void UNiagaraStackFunctionInputCollection::FinalizeInternal()
{
	InputFunctionCallNode->OnInputsChanged().RemoveAll(this);
	Super::FinalizeInternal();
}

FText UNiagaraStackFunctionInputCollection::GetDisplayName() const
{
	return LOCTEXT("InputCollectionDisplayName", "Inputs");
}

bool UNiagaraStackFunctionInputCollection::GetShouldShowInStack() const
{
	return bShouldShowInStack;
}

bool UNiagaraStackFunctionInputCollection::GetIsEnabled() const
{
	return InputFunctionCallNode->GetDesiredEnabledState() == ENodeEnabledState::Enabled;
}

void UNiagaraStackFunctionInputCollection::SetShouldShowInStack(bool bInShouldShowInStack)
{
	bShouldShowInStack = bInShouldShowInStack;
}

void UNiagaraStackFunctionInputCollection::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	TArray<const UEdGraphPin*> InputPins;
	FNiagaraStackGraphUtilities::GetStackFunctionInputPins(*InputFunctionCallNode, InputPins, FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly);

	const UEdGraphSchema_Niagara* NiagaraSchema = GetDefault<UEdGraphSchema_Niagara>();

	TArray<FName> ProcessedInputNames;
	TArray<FName> DuplicateInputNames;
	TArray<FName> ValidAliasedInputNames;
	TArray<const UEdGraphPin*> PinsWithInvalidTypes;

	FText UncategorizedName = LOCTEXT("Uncategorized", "Uncategorized");

	UNiagaraGraph* InputFunctionGraph = InputFunctionCallNode->FunctionScript != nullptr
		? CastChecked<UNiagaraScriptSource>(InputFunctionCallNode->FunctionScript->GetSource())->NodeGraph
		: nullptr;

	TArray<FInputData> InputDataCollection;
	
	// Gather input data
	for (const UEdGraphPin* InputPin : InputPins)
	{
		if (ProcessedInputNames.Contains(InputPin->PinName) == false)
		{
			ProcessedInputNames.Add(InputPin->PinName);

			FNiagaraVariable InputVariable = NiagaraSchema->PinToNiagaraVariable(InputPin);
			if (InputVariable.GetType().IsValid() == false)
			{
				PinsWithInvalidTypes.Add(InputPin);
				continue;
			}
			else
			{
				ValidAliasedInputNames.Add(FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(
					FNiagaraParameterHandle(InputPin->PinName), InputFunctionCallNode).GetParameterHandleString());
			}

			FNiagaraVariableMetaData* InputMetaData = nullptr;
			if (InputFunctionGraph != nullptr)
			{
				InputMetaData = InputFunctionGraph->GetMetaData(InputVariable);
			}

			FText InputCategory = InputMetaData != nullptr && InputMetaData->CategoryName.IsEmptyOrWhitespace() == false
				? InputMetaData->CategoryName
				: UncategorizedName;

			FInputData InputData = { InputPin, InputVariable.GetType(), InputMetaData ? InputMetaData->EditorSortPriority : 0, InputCategory };
			InputDataCollection.Add(InputData);
		}
		else
		{
			DuplicateInputNames.AddUnique(InputPin->PinName);
		}
	}

	// Sort data and keep the uncategorized first
	InputDataCollection.Sort([UncategorizedName](const FInputData& A, const FInputData& B)
	{
		if (A.Category.CompareTo(UncategorizedName) == 0 && B.Category.CompareTo(UncategorizedName) != 0)
		{
			return true;
		}
		if (A.Category.CompareTo(UncategorizedName) != 0 && B.Category.CompareTo(UncategorizedName) == 0)
		{
			return false;
		}
		if (A.SortKey != B.SortKey)
		{
			return A.SortKey < B.SortKey;
		}
		else
		{
			return A.Pin->PinName < B.Pin->PinName;
		}
	});

	// Populate the category children
	for (const FInputData InputData : InputDataCollection)
	{
		// Try to find an existing category in the already processed children.
		UNiagaraStackInputCategory* InputCategory = FindCurrentChildOfTypeByPredicate<UNiagaraStackInputCategory>(NewChildren,
			[&](UNiagaraStackInputCategory* CurrentCategory) { return CurrentCategory->GetCategoryName().CompareTo(InputData.Category) == 0; });

		if (InputCategory == nullptr)
		{
			// If we haven't added any children to this category yet see if there is one that can be reused from the current children.
			InputCategory = FindCurrentChildOfTypeByPredicate<UNiagaraStackInputCategory>(CurrentChildren,
				[&](UNiagaraStackInputCategory* CurrentCategory) { return CurrentCategory->GetCategoryName().CompareTo(InputData.Category) == 0; });
			if (InputCategory == nullptr)
			{
				// If we don't have a current child for this category make a new one.
				InputCategory = NewObject<UNiagaraStackInputCategory>(this);
				InputCategory->Initialize(CreateDefaultChildRequiredData(), *ModuleNode, *InputFunctionCallNode, InputData.Category, GetOwnerStackItemEditorDataKey());
			}
			else
			{
				// We found a category to reuse, but we need to reset the inputs before we can start adding the current set of inputs.
				InputCategory->ResetInputs();
			}

			if (InputData.Category.CompareTo(UncategorizedName) == 0)
			{
				InputCategory->SetShouldShowInStack(false);
			}
			NewChildren.Add(InputCategory);
		}
		InputCategory->AddInput(InputData.Pin->PinName, InputData.Type);
	}
	RefreshIssues(DuplicateInputNames, ValidAliasedInputNames, PinsWithInvalidTypes, NewIssues);
}

void UNiagaraStackFunctionInputCollection::RefreshIssues(TArray<FName> DuplicateInputNames, TArray<FName> ValidAliasedInputNames, TArray<const UEdGraphPin*> PinsWithInvalidTypes, TArray<FStackIssue>& NewIssues)
{
	if (!GetIsEnabled())
	{
		NewIssues.Empty();
		return;
	}
	// Try to find function input overrides which are no longer valid so we can generate errors for them.
	UNiagaraNodeParameterMapSet* OverrideNode = FNiagaraStackGraphUtilities::GetStackFunctionOverrideNode(*InputFunctionCallNode);
	if (OverrideNode != nullptr)
	{
		TArray<UEdGraphPin*> OverridePins;
		OverrideNode->GetInputPins(OverridePins);
		for (UEdGraphPin* OverridePin : OverridePins)
		{
			// If the pin isn't in the misc category for the add pin, and not the parameter map pin, and it's for this function call,
			// check to see if it's in the list of valid input names, and if not generate an error.
			if (OverridePin->PinType.PinCategory != UEdGraphSchema_Niagara::PinCategoryMisc &&
				OverridePin->PinType.PinSubCategoryObject != FNiagaraTypeDefinition::GetParameterMapStruct() &&
				FNiagaraParameterHandle(OverridePin->PinName).GetNamespace().ToString() == InputFunctionCallNode->GetFunctionName() &&
				ValidAliasedInputNames.Contains(OverridePin->PinName) == false)
			{
				FText FixDescription = LOCTEXT("RemoveInvalidInputTransaction", "Remove invalid input override.");
				FStackIssueFix RemoveInputOverrideFix(
					FixDescription,
					UNiagaraStackEntry::FStackIssueFixDelegate::CreateLambda([=]()
				{
					FScopedTransaction ScopedTransaction(FixDescription);
					TArray<TWeakObjectPtr<UNiagaraDataInterface>> RemovedDataObjects;
					FNiagaraStackGraphUtilities::RemoveNodesForStackFunctionInputOverridePin(*OverridePin, RemovedDataObjects);
					for (TWeakObjectPtr<UNiagaraDataInterface> RemovedDataObject : RemovedDataObjects)
					{
						if (RemovedDataObject.IsValid())
						{
							OnDataObjectModified().Broadcast(RemovedDataObject.Get());
						}
					}
					OverridePin->GetOwningNode()->RemovePin(OverridePin);
				}));

				FStackIssue InvalidInputOverrideError(
					EStackIssueSeverity::Error,
					FText::Format(LOCTEXT("InvalidInputSummaryFormat", "Invalid Input Override: {0}"), FText::FromString(OverridePin->PinName.ToString())),
					FText::Format(LOCTEXT("InvalidInputFormat", "The input {0} was previously overriden but is no longer exposed by the function {1}.\nPress the fix button to remove this unused override data,\nor check the function definition to see why this input is no longer exposed."),
						FText::FromString(OverridePin->PinName.ToString()), FText::FromString(InputFunctionCallNode->GetFunctionName())),
					GetStackEditorDataKey(),
					false,
					RemoveInputOverrideFix);

				NewIssues.Add(InvalidInputOverrideError);
			}
		}
	}

	// Generate issues for duplicate input names.
	for (const FName& DuplicateInputName : DuplicateInputNames)
	{
		FStackIssue DuplicateInputError(
			EStackIssueSeverity::Error,
			FText::Format(LOCTEXT("DuplicateInputSummaryFormat", "Duplicate Input: {0}"), FText::FromName(DuplicateInputName)),
			FText::Format(LOCTEXT("DuplicateInputFormat", "There are multiple inputs with the same name {0}, but different types exposed by the function {1}.\nThis is not suppored and must be fixed in the script that defines this function."),
				FText::FromName(DuplicateInputName), FText::FromString(InputFunctionCallNode->GetFunctionName())),
			GetStackEditorDataKey(),
			false);
		NewIssues.Add(DuplicateInputError);
	}

	// Generate issues for invalid types.
	for (const UEdGraphPin* PinWithInvalidType : PinsWithInvalidTypes)
	{
		FStackIssue InputWithInvalidTypeError(
			EStackIssueSeverity::Error,
			FText::Format(LOCTEXT("InputWithInvalidTypeSummaryFormat", "Input has an invalid type: {0}"), FText::FromName(PinWithInvalidType->PinName)),
			FText::Format(LOCTEXT("InputWithInvalidTypeFormat", "The input {0} on function {1} has a type which is invalid.\nThe type of this input likely doesn't exist anymore.\nThis input must be fixed in the script before this module can be used."),
				FText::FromName(PinWithInvalidType->PinName), FText::FromString(InputFunctionCallNode->GetFunctionName())),
			GetStackEditorDataKey(),
			false);
		NewIssues.Add(InputWithInvalidTypeError);
	}
}

void UNiagaraStackFunctionInputCollection::OnFunctionInputsChanged()
{
	RefreshChildren();
}

#undef LOCTEXT_NAMESPACE
