// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeSimTargetSelector.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraHlslTranslator.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeSimTargetSelector"

UNiagaraNodeSimTargetSelector::UNiagaraNodeSimTargetSelector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNiagaraNodeSimTargetSelector::AllocateDefaultPins()
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	// create all the cpu input pins
	for (FNiagaraVariable& Var : OutputVars)
	{
		FEdGraphPinType PinType = Schema->TypeDefinitionToPinType(Var.GetType());
		CreatePin(EGPD_Input, PinType, *(Var.GetName().ToString() + TEXT(" if CPU VM")));
	}

	// create all the gpu input pins
	for (FNiagaraVariable& Var : OutputVars)
	{
		FEdGraphPinType PinType = Schema->TypeDefinitionToPinType(Var.GetType());
		CreatePin(EGPD_Input, PinType, *(Var.GetName().ToString() + TEXT(" if GPU Shader")));
	}

	// create the output pins
	for (int32 Index = 0; Index < OutputVars.Num(); Index++)
	{
		const FNiagaraVariable& Var = OutputVars[Index];
		UEdGraphPin* NewPin = CreatePin(EGPD_Output, Schema->TypeDefinitionToPinType(Var.GetType()), Var.GetName());
		NewPin->PersistentGuid = OutputVarGuids[Index];
	}

	CreateAddPin(EGPD_Output);
}

void UNiagaraNodeSimTargetSelector::InsertInputPinsFor(const FNiagaraVariable& Var)
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	TArray<UEdGraphPin*> OldPins(Pins);
	Pins.Reset(Pins.Num() + 2);

	// Create the inputs for each simulation target.
	for (int64 i = 0; i < 2; i++)
	{
		// Add the previous input pins
		for (int32 k = 0; k < OutputVars.Num() - 1; k++)
		{
			Pins.Add(OldPins[k]);
		}
		OldPins.RemoveAt(0, OutputVars.Num() - 1);

		// Add the new input pin
		const FString PathSuffix = i == 0 ? TEXT(" if CPU VM") : TEXT(" if GPU Shader");
		CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(Var.GetType()), *(Var.GetName().ToString() + PathSuffix));
	}

	// Move the rest of the old pins over
	Pins.Append(OldPins);
}

void UNiagaraNodeSimTargetSelector::Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs)
{
	TArray<UEdGraphPin*> InputPins;
	GetInputPins(InputPins);
	TArray<UEdGraphPin*> OutputPins;
	GetOutputPins(OutputPins);

	ENiagaraSimTarget SimulationTarget = Translator->GetSimulationTarget();
	int32 VarIdx;
	if (SimulationTarget == ENiagaraSimTarget::CPUSim)
	{
		VarIdx = 0;
	}
	else if (SimulationTarget == ENiagaraSimTarget::GPUComputeSim)
	{
		VarIdx = InputPins.Num() / 2;
	}
	else
	{
		Translator->Error(LOCTEXT("InvalidSimTarget", "Unknown simulation target"), this, nullptr);
		return;
	}

	Outputs.SetNumUninitialized(OutputPins.Num());
	for (int32 i = 0; i < OutputVars.Num(); i++)
	{
		int32 InputIdx = Translator->CompilePin(InputPins[VarIdx + i]);
		Outputs[i] = InputIdx;
	}
	check(this->IsAddPin(OutputPins[OutputPins.Num() - 1]));
	Outputs[OutputPins.Num() - 1] = INDEX_NONE;
}

UEdGraphPin* UNiagaraNodeSimTargetSelector::GetPassThroughPin(const UEdGraphPin* LocallyOwnedOutputPin, ENiagaraScriptUsage MasterUsage) const 
{
	return nullptr;
}

void UNiagaraNodeSimTargetSelector::BuildParameterMapHistory(FNiagaraParameterMapHistoryBuilder& OutHistory, bool bRecursive)
{
	UNiagaraNode::BuildParameterMapHistory(OutHistory, bRecursive);
}

FText UNiagaraNodeSimTargetSelector::GetTooltipText() const
{
	return LOCTEXT("SimTargetSelectorDesc", "If the simulation target matches, then the traversal will follow that path.");
}

FText UNiagaraNodeSimTargetSelector::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("SimTargetSelectorTitle", "Select by Simulation Target");
}

#undef LOCTEXT_NAMESPACE
