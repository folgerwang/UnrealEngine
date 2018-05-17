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
		UNiagaraStackInputCategory* InputCategory = FindCurrentChildOfTypeByPredicate<UNiagaraStackInputCategory>(NewChildren,
			[&](UNiagaraStackInputCategory* CurrentCategory) { return CurrentCategory->GetCategoryName().CompareTo(InputData.Category) == 0; });

		if (InputCategory == nullptr)
		{
			InputCategory = FindCurrentChildOfTypeByPredicate<UNiagaraStackInputCategory>(CurrentChildren,
				[&](UNiagaraStackInputCategory* CurrentCategory) { return CurrentCategory->GetCategoryName().CompareTo(InputData.Category) == 0; });
			if (InputCategory == nullptr)
			{
				InputCategory = NewObject<UNiagaraStackInputCategory>(this);
				InputCategory->Initialize(CreateDefaultChildRequiredData(), *ModuleNode, *InputFunctionCallNode, InputData.Category, GetOwnerStackItemEditorDataKey());
			}
			else
			{
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
	RefreshIssues(NewIssues);
}

void UNiagaraStackFunctionInputCollection::RefreshIssues(TArray<FStackIssue>& NewIssues)
{
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
				UNiagaraStackEntry::FStackIssue InvalidInputError;
				InvalidInputError.ShortDescription = FText::Format(LOCTEXT("InvalidInputSummaryFormat", "Invalid Input: {0}"), FText::FromString(OverridePin->PinName.ToString()));
				InvalidInputError.LongDescription = FText::Format(LOCTEXT("InvalidInputFormat", "The input {0} was previously overriden but is no longer exposed by the function {1}.\nPress the fix button to remove this unused override data,\nor check the function definition to see why this input is no longer exposed."),
					FText::FromString(OverridePin->PinName.ToString()), FText::FromString(InputFunctionCallNode->GetFunctionName()));
				InvalidInputError.UniqueIdentifier = FName(*FString::Printf(TEXT("%s-InvalidInput-%s"), *GetStackEditorDataKey(), *OverridePin->PinName.ToString()));
				UNiagaraStackEntry::FStackIssueFix Fix;
				Fix.Description = LOCTEXT("RemoveInvalidInputTransaction", "Remove invalid input override.");
				Fix.FixDelegate.BindLambda([=]()
				{
					FScopedTransaction ScopedTransaction(Fix.Description);
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
				});
				InvalidInputError.Fixes.Add(Fix);
				NewIssues.Add(InvalidInputError);
			}
		}
	}

	for (const FName& DuplicateInputName : DuplicateInputNames)
	{
		UNiagaraStackEntry::FStackIssue DuplicateInputError;

		FString DuplicateInputNameString = DuplicateInputName.ToString();

		DuplicateInputError.ShortDescription = FText::Format(LOCTEXT("DuplicateInputSummaryFormat", "Duplicate Input: {0}"), FText::FromString(DuplicateInputNameString));
		DuplicateInputError.LongDescription = FText::Format(LOCTEXT("DuplicateInputFormat", "There are multiple inputs with the same name {0}, but different types exposed by the function {1}.\nThis is not suppored and must be fixed in the script that defines this function."),
			FText::FromString(DuplicateInputNameString), FText::FromString(InputFunctionCallNode->GetFunctionName()));
		DuplicateInputError.UniqueIdentifier = FName(*FString::Printf(TEXT("%s-DuplicateInput-%s"), *GetStackEditorDataKey(), *DuplicateInputNameString));
		NewIssues.Add(DuplicateInputError);
	}

	for (const UEdGraphPin* PinWithInvalidType : PinsWithInvalidTypes)
	{
		UNiagaraStackEntry::FStackIssue InputWithInvalidTypeError;
		InputWithInvalidTypeError.ShortDescription = FText::Format(LOCTEXT("InputWithInvalidTypeSummaryFormat", "Input has an invalid type: {0}"), FText::FromString(PinWithInvalidType->PinName.ToString()));
		InputWithInvalidTypeError.LongDescription = FText::Format(LOCTEXT("InputWithInvalidTypeFormat", "The input {0} on function {1} has a type which is invalid.\nThe type of this input likely doesn't exist anymore.\nThis input must be fixed in the script before this module can be used."),
			FText::FromString(PinWithInvalidType->PinName.ToString()), FText::FromString(InputFunctionCallNode->GetFunctionName()));
		NewIssues.Add(InputWithInvalidTypeError);
	}
}

void UNiagaraStackFunctionInputCollection::OnFunctionInputsChanged()
{
	if (IsValid())
	{
		RefreshChildren();
	}
}

#undef LOCTEXT_NAMESPACE
