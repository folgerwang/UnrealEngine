// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackFunctionInputCondition.h"
#include "NiagaraEditorModule.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraTypes.h"
#include "INiagaraEditorTypeUtilities.h"

#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "NiagaraStackFunctionInputCondition"

void FNiagaraStackFunctionInputCondition::Initialize(UNiagaraScript* InScript,
	TArray<UNiagaraScript*> InDependentScripts,
	FString InOwningEmitterUniqueName,
	UNiagaraNodeFunctionCall* InFunctionCallNode)
{
	Script = InScript;
	DependentScripts = InDependentScripts;
	OwningEmitterUniqueName = InOwningEmitterUniqueName;
	FunctionCallNode = InFunctionCallNode;
}

void FNiagaraStackFunctionInputCondition::Refresh(const FString* Condition)
{
	TargetValueData.Empty();
	InputBinder.Reset();

	if (Condition == nullptr || Condition->IsEmpty())
	{
		return;
	}

	FString InputName;
	FString TargetValue;
	int32 EqualsIndex = Condition->Find("=");
	if (EqualsIndex == INDEX_NONE)
	{
		// If the condition doesn't have an equals sign the target value is assumed to be a bool value of true.
		InputName = *Condition;
		TargetValue = "true";
	}
	else
	{
		InputName = Condition->Left(EqualsIndex);
		TargetValue = Condition->RightChop(EqualsIndex + 1);
	}

	FText ErrorMessage;
	if (InputBinder.TryBind(Script, DependentScripts, OwningEmitterUniqueName, FunctionCallNode, *InputName, TOptional<FNiagaraTypeDefinition>(), true, ErrorMessage))
	{
		FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
		TSharedPtr<INiagaraEditorTypeUtilities, ESPMode::ThreadSafe> TypeEditorUtilities = NiagaraEditorModule.GetTypeUtilities(InputBinder.GetInputType());
		if (TypeEditorUtilities.IsValid())
		{
			FNiagaraVariable TempVariable(InputBinder.GetInputType(), "Temp");
			bool bValueParsed = false;

			if (TypeEditorUtilities->CanHandlePinDefaults())
			{
				bValueParsed = TypeEditorUtilities->SetValueFromPinDefaultString(TargetValue, TempVariable);
			}
			if (bValueParsed == false && TypeEditorUtilities->CanSetValueFromDisplayName())
			{
				bValueParsed = TypeEditorUtilities->SetValueFromDisplayName(FText::FromString(TargetValue), TempVariable);
			}

			if (bValueParsed)
			{
				TargetValueData.AddUninitialized(InputBinder.GetInputType().GetSize());
				TempVariable.CopyTo(TargetValueData.GetData());
			}
			else
			{
				ErrorMessage = FText::Format(LOCTEXT("ParseValueError", "Target value {0} is not a valid for type {1}"), FText::FromString(TargetValue), InputBinder.GetInputType().GetNameText());
				InputBinder.Reset();
			}
		}
	}

	if(ErrorMessage.IsEmpty() == false)
	{
		UE_LOG(LogNiagaraEditor, Warning, TEXT("Input condition failed to bind %s=%s.  Message: %s"), *InputName, *TargetValue, *ErrorMessage.ToString());
	}
}

bool FNiagaraStackFunctionInputCondition::IsValid() const
{
	return InputBinder.IsValid() && (TargetValueData.Num() > 0);
}

bool FNiagaraStackFunctionInputCondition::GetConditionIsEnabled() const
{
	return IsValid() && FMemory::Memcmp(TargetValueData.GetData(), InputBinder.GetData().GetData(), TargetValueData.Num()) == 0;
}

bool FNiagaraStackFunctionInputCondition::CanSetConditionIsEnabled() const
{
	return IsValid() && InputBinder.GetInputType() == FNiagaraTypeDefinition::GetBoolDef();
}

void FNiagaraStackFunctionInputCondition::SetConditionIsEnabled(bool bInIsEnabled)
{
	checkf(CanSetConditionIsEnabled(), TEXT("Can not set this condition"));
	InputBinder.SetValue(bInIsEnabled);
}

FName FNiagaraStackFunctionInputCondition::GetConditionInputName() const
{
	checkf(IsValid(), TEXT("Can not get the input name for an invalid input condition"));
	return InputBinder.GetInputName();
}

FNiagaraTypeDefinition FNiagaraStackFunctionInputCondition::GetConditionInputType() const
{
	checkf(IsValid(), TEXT("Can not get the input type for an invalid input condition"));
	return InputBinder.GetInputType();
}

FNiagaraVariableMetaData* FNiagaraStackFunctionInputCondition::GetConditionInputMetaData() const
{
	checkf(IsValid(), TEXT("Can not get the input metadata for an invalid input condition"));
	FNiagaraVariable InputVariable(InputBinder.GetInputType(), InputBinder.GetInputName());
	UNiagaraGraph* FunctionGraph = CastChecked<UNiagaraScriptSource>(FunctionCallNode->FunctionScript->GetSource())->NodeGraph;
	return FunctionGraph->GetMetaData(InputVariable);
}

#undef LOCTEXT_NAMESPACE
