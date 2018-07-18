// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackModuleItemOutput.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraConstants.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "EdGraphSchema_Niagara.h"


#define LOCTEXT_NAMESPACE "NiagaraStackViewModel"

UNiagaraStackModuleItemOutput::UNiagaraStackModuleItemOutput()
	: FunctionCallNode(nullptr)
{
}

void UNiagaraStackModuleItemOutput::Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraNodeFunctionCall& InFunctionCallNode, FName InOutputParameterHandle,
	FNiagaraTypeDefinition InOutputType)
{
	checkf(FunctionCallNode.Get() == nullptr, TEXT("Can only set the Output once."));
	FString OutputStackEditorDataKey = FString::Printf(TEXT("%s-Output-%s"), *InFunctionCallNode.NodeGuid.ToString(EGuidFormats::DigitsWithHyphens), *InOutputParameterHandle.ToString());
	Super::Initialize(InRequiredEntryData, OutputStackEditorDataKey);
	FunctionCallNode = &InFunctionCallNode;
	OutputType = InOutputType;
	OutputParameterHandle = FNiagaraParameterHandle(InOutputParameterHandle);
	DisplayName = FText::FromName(OutputParameterHandle.GetName());
}

FText UNiagaraStackModuleItemOutput::GetDisplayName() const
{
	return DisplayName;
}

FText UNiagaraStackModuleItemOutput::GetTooltipText() const
{
	FNiagaraVariable ValueVariable(OutputType, OutputParameterHandle.GetParameterHandleString());
	if (FunctionCallNode.IsValid() && FunctionCallNode->FunctionScript != nullptr)
	{
		UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(FunctionCallNode->FunctionScript->GetSource());
		const UEdGraphSchema_Niagara* NiagaraSchema = GetDefault<UEdGraphSchema_Niagara>();
		const FNiagaraVariableMetaData* MetaData = nullptr;
		if (FNiagaraConstants::IsNiagaraConstant(ValueVariable))
		{
			MetaData = FNiagaraConstants::GetConstantMetaData(ValueVariable);
		}
		else if (Source->NodeGraph != nullptr)
		{
			MetaData = Source->NodeGraph->GetMetaData(ValueVariable);
		}

		if (MetaData != nullptr)
		{
			return MetaData->Description;
		}
	}
	return FText::FromName(ValueVariable.GetName());
}

bool UNiagaraStackModuleItemOutput::GetIsEnabled() const
{
	return FunctionCallNode->GetDesiredEnabledState() == ENodeEnabledState::Enabled;
}

UNiagaraStackEntry::EStackRowStyle UNiagaraStackModuleItemOutput::GetStackRowStyle() const
{
	return EStackRowStyle::ItemContent;
}

const FNiagaraParameterHandle& UNiagaraStackModuleItemOutput::GetOutputParameterHandle() const
{
	return OutputParameterHandle;
}

FText UNiagaraStackModuleItemOutput::GetOutputParameterHandleText() const
{
	return FText::FromName(OutputParameterHandle.GetParameterHandleString());
}

#undef LOCTEXT_NAMESPACE
