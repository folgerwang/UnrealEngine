// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackModuleItemOutputCollection.h"
#include "ViewModels/Stack/NiagaraStackModuleItemOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeParameterMapSet.h"
#include "NiagaraEmitterEditorData.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

#include "EdGraph/EdGraphPin.h"

UNiagaraStackModuleItemOutputCollection::UNiagaraStackModuleItemOutputCollection()
	: FunctionCallNode(nullptr)
{
}

void UNiagaraStackModuleItemOutputCollection::Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraNodeFunctionCall& InFunctionCallNode)
{
	checkf(FunctionCallNode == nullptr, TEXT("Can not set the node more than once."));
	FString OutputCollectionStackEditorDataKey = FString::Printf(TEXT("%s-Outputs"), *InFunctionCallNode.NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
	Super::Initialize(InRequiredEntryData, OutputCollectionStackEditorDataKey);
	FunctionCallNode = &InFunctionCallNode;
}

FText UNiagaraStackModuleItemOutputCollection::GetDisplayName() const
{
	return NSLOCTEXT("StackModuleItemOutputCollection", "OutputsLabel", "Outputs");
}

bool UNiagaraStackModuleItemOutputCollection::IsExpandedByDefault() const
{
	return false;
}

bool UNiagaraStackModuleItemOutputCollection::GetIsEnabled() const
{
	return FunctionCallNode->GetDesiredEnabledState() == ENodeEnabledState::Enabled;
}

UNiagaraStackEntry::EStackRowStyle UNiagaraStackModuleItemOutputCollection::GetStackRowStyle() const
{
	return EStackRowStyle::ItemContent;
}

void UNiagaraStackModuleItemOutputCollection::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	UEdGraphPin* OutputParameterMapPin = FNiagaraStackGraphUtilities::GetParameterMapOutputPin(*FunctionCallNode);
	if (ensureMsgf(OutputParameterMapPin != nullptr, TEXT("Invalid Stack Graph - Function call node has no output pin.")))
	{
		FNiagaraParameterMapHistoryBuilder Builder;
		Builder.SetIgnoreDisabled(false);
		FunctionCallNode->BuildParameterMapHistory(Builder, false);

		if (ensureMsgf(Builder.Histories.Num() == 1, TEXT("Invalid Stack Graph - Function call node has invalid history count!")))
		{
			for (int32 i = 0; i < Builder.Histories[0].Variables.Num(); i++)
			{
				FNiagaraVariable& Variable = Builder.Histories[0].Variables[i];
				TArray<const UEdGraphPin*>& WriteHistory = Builder.Histories[0].PerVariableWriteHistory[i];

				for (const UEdGraphPin* WritePin : WriteHistory)
				{
					if (Cast<UNiagaraNodeParameterMapSet>(WritePin->GetOwningNode()) != nullptr)
					{
						UNiagaraStackModuleItemOutput* Output = FindCurrentChildOfTypeByPredicate<UNiagaraStackModuleItemOutput>(CurrentChildren,
							[&](UNiagaraStackModuleItemOutput* CurrentOutput) { return CurrentOutput->GetOutputParameterHandle().GetParameterHandleString() == Variable.GetName(); });

						if (Output == nullptr)
						{
							Output = NewObject<UNiagaraStackModuleItemOutput>(this);
							Output->Initialize(CreateDefaultChildRequiredData(), *FunctionCallNode, Variable.GetName(), Variable.GetType());
						}

						NewChildren.Add(Output);
						break;
					}
				}
			}
		}
	}
}
