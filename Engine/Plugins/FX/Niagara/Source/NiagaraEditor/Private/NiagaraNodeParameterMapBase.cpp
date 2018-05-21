// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeParameterMapBase.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraEditorUtilities.h"
#include "SNiagaraGraphNodeConvert.h"
#include "INiagaraCompiler.h"
#include "NiagaraNodeOutput.h"
#include "ScopedTransaction.h"
#include "SNiagaraGraphPinAdd.h"
#include "NiagaraGraph.h"
#include "NiagaraComponent.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraConstants.h"
#include "NiagaraParameterCollection.h"
#include "Widgets/SNiagaraParameterMapView.h"
#include "Widgets/Input/SEditableTextBox.h"

#include "IAssetTools.h"
#include "AssetRegistryModule.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeParameterMapBase"

const FName UNiagaraNodeParameterMapBase::ParameterPinSubCategory("ParameterPin");
const FName UNiagaraNodeParameterMapBase::SourcePinName("Source");
const FName UNiagaraNodeParameterMapBase::DestPinName("Dest");

UNiagaraNodeParameterMapBase::UNiagaraNodeParameterMapBase() 
	: UNiagaraNodeWithDynamicPins()
	, PinPendingRename(nullptr)
{

}

TArray<FNiagaraParameterMapHistory> UNiagaraNodeParameterMapBase::GetParameterMaps(UNiagaraScriptSourceBase* InSource, FString EmitterNameOverride, const TArray<FNiagaraVariable>& EncounterableVariables)
{
	TArray<FNiagaraParameterMapHistory> OutputParameterMapHistories;
	UNiagaraScriptSource* Base = Cast<UNiagaraScriptSource>(InSource);
	if (Base != nullptr)
	{
		OutputParameterMapHistories = GetParameterMaps(Base->NodeGraph, EmitterNameOverride, EncounterableVariables);
	}
	return OutputParameterMapHistories;

}

TArray<FNiagaraParameterMapHistory> UNiagaraNodeParameterMapBase::GetParameterMaps(UNiagaraGraph* InGraph, FString EmitterNameOverride, const TArray<FNiagaraVariable>& EncounterableVariables)
{
	TArray<UNiagaraNodeOutput*> OutputNodes;
	InGraph->FindOutputNodes(OutputNodes);
	TArray<FNiagaraParameterMapHistory> OutputParameterMapHistories;

	for (UNiagaraNodeOutput* FoundOutputNode : OutputNodes)
	{
		OutputParameterMapHistories.Append(GetParameterMaps(FoundOutputNode, false, EmitterNameOverride,EncounterableVariables));
	}

	return OutputParameterMapHistories;
}

TArray<FNiagaraParameterMapHistory> UNiagaraNodeParameterMapBase::GetParameterMaps(UNiagaraNodeOutput* InGraphEnd, bool bLimitToOutputScriptType, FString EmitterNameOverride, const TArray<FNiagaraVariable>& EncounterableVariables)
{
	const UEdGraphSchema_Niagara* Schema = Cast<UEdGraphSchema_Niagara>(InGraphEnd->GetSchema());

	FNiagaraParameterMapHistoryBuilder Builder;
	Builder.RegisterEncounterableVariables(EncounterableVariables);
	if (!EmitterNameOverride.IsEmpty())
	{
		Builder.EnterEmitter(EmitterNameOverride, nullptr);
	}

	if (bLimitToOutputScriptType)
	{
		Builder.EnableScriptWhitelist(true, InGraphEnd->GetUsage());
	}
	
	Builder.BuildParameterMaps(InGraphEnd);
	
	if (!EmitterNameOverride.IsEmpty())
	{
		Builder.ExitEmitter(EmitterNameOverride, nullptr);
	}
	
	return Builder.Histories;
}


bool UNiagaraNodeParameterMapBase::AllowNiagaraTypeForAddPin(const FNiagaraTypeDefinition& InType)
{
	return InType != FNiagaraTypeDefinition::GetGenericNumericDef();
}

FText UNiagaraNodeParameterMapBase::GetPinDescriptionText(UEdGraphPin* Pin) const
{
	FNiagaraVariable Var = CastChecked<UEdGraphSchema_Niagara>(GetSchema())->PinToNiagaraVariable(Pin);

	const UNiagaraGraph* Graph = GetNiagaraGraph();
	const FNiagaraVariableMetaData* OldMetaData = Graph->GetMetaData(Var);
	if (OldMetaData)
	{
		ensure(OldMetaData->ReferencerNodes.Contains(MakeWeakObjectPtr<UObject>(const_cast<UNiagaraNodeParameterMapBase*>(this))));
		return OldMetaData->Description;
	}
	return FText::GetEmpty();
}

/** Called when a pin's description text is committed. */
void UNiagaraNodeParameterMapBase::PinDescriptionTextCommitted(const FText& Text, ETextCommit::Type CommitType, UEdGraphPin* Pin)
{
	FNiagaraVariable Var = CastChecked<UEdGraphSchema_Niagara>(GetSchema())->PinToNiagaraVariable(Pin);
	UNiagaraGraph* Graph = GetNiagaraGraph();
	if (FNiagaraConstants::IsNiagaraConstant(Var))
	{
		UE_LOG(LogNiagaraEditor, Error, TEXT("You cannot set the description for a Niagara internal constant \"%s\""),*Var.GetName().ToString());
		return;
	}
	FNiagaraVariableMetaData* OldMetaData = Graph->GetMetaData(Var);
	bool bSet = !Text.IsEmptyOrWhitespace();
	if (OldMetaData && !OldMetaData->Description.IsEmptyOrWhitespace())
	{
		bSet = true;
	}

	FScopedTransaction AddNewPinTransaction(LOCTEXT("Rename Pin Desc", "Changed variable description"));
	Modify();
	Pin->Modify();

	if (OldMetaData)
	{
		ensure(OldMetaData->ReferencerNodes.Contains(TWeakObjectPtr<UObject>(this)));
		Graph->Modify();
		OldMetaData->Description = Text;
	}
	else
	{
		Graph->Modify();
		FNiagaraVariableMetaData& NewMetaData = Graph->FindOrAddMetaData(Var);
		NewMetaData.Description = Text;
		NewMetaData.ReferencerNodes.Add(TWeakObjectPtr<UObject>(this));
	}
}

void UNiagaraNodeParameterMapBase::CollectAddPinActions(FGraphActionListBuilderBase& OutActions, bool& bOutCreateRemainingActions, UEdGraphPin* Pin)
{
	bOutCreateRemainingActions = true;
}

void UNiagaraNodeParameterMapBase::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	// Get hover text from metadata description.
	const UNiagaraGraph* NiagaraGraph = GetNiagaraGraph();
	if (NiagaraGraph)
	{
		const UEdGraphSchema_Niagara* Schema = Cast<UEdGraphSchema_Niagara>(NiagaraGraph->GetSchema());
		if (Schema)
		{
			if (IsAddPin(&Pin))
			{
				HoverTextOut = LOCTEXT("ParameterMapAddString", "Request a new variable from the parameter map.").ToString();
				return;
			}

			FNiagaraTypeDefinition TypeDef = Schema->PinToTypeDefinition(&Pin);

			if (Pin.Direction == EEdGraphPinDirection::EGPD_Input)
			{
				if (&Pin == GetInputPin(0) && TypeDef == FNiagaraTypeDefinition::GetParameterMapDef())
				{
					HoverTextOut = LOCTEXT("ParameterMapInString", "The source parameter map where we pull the values from.").ToString();
					return;
				}
			}

			if (Pin.Direction == EEdGraphPinDirection::EGPD_Output)
			{
				if (&Pin == GetOutputPin(0) && TypeDef == FNiagaraTypeDefinition::GetParameterMapDef())
				{
					HoverTextOut = LOCTEXT("ParameterMapOutString", "The destination parameter map where we write the values to.").ToString();
					return;
				}
			}

			FNiagaraVariable Var = FNiagaraVariable(TypeDef, Pin.PinName);
			const FNiagaraVariableMetaData* Metadata = NiagaraGraph->GetMetaData(Var);
			if (Metadata)
			{
				FText Desc = FText::Format(LOCTEXT("GetVarTooltip", "Name: \"{0}\"\nType: {1}\nDesc: {2}"), FText::FromName(Pin.PinName),
					TypeDef.GetNameText(), Metadata->Description);
				HoverTextOut = Desc.ToString();
			}
			else
			{
				FText Desc = FText::Format(LOCTEXT("GetVarTooltip", "Name: \"{0}\"\nType: {1}\nDesc: None"), FText::FromName(Pin.PinName),
					TypeDef.GetNameText());
				HoverTextOut = Desc.ToString();
			}
		}
	}
}

void UNiagaraNodeParameterMapBase::OnPinRenamed(UEdGraphPin* RenamedPin, const FString& OldName)
{
	RenamedPin->PinFriendlyName = FText::FromName(RenamedPin->PinName);

	TArray<UEdGraphPin*> InOrOutPins;
	if (RenamedPin->Direction == EEdGraphPinDirection::EGPD_Input)
	{
		GetInputPins(InOrOutPins);
	}
	else
	{
		GetOutputPins(InOrOutPins);
	}

	TSet<FName> Names;
	for (const UEdGraphPin* Pin : InOrOutPins)
	{
		if (Pin != RenamedPin)
		{
			Names.Add(Pin->GetFName());
		}
	}
	const FName NewUniqueName = FNiagaraUtilities::GetUniqueName(*RenamedPin->GetName(), Names);

	FNiagaraTypeDefinition VarType = CastChecked<UEdGraphSchema_Niagara>(GetSchema())->PinToTypeDefinition(RenamedPin);
	FNiagaraVariable Var(VarType, *OldName);

	UNiagaraGraph* Graph = GetNiagaraGraph();
	Graph->RenameParameter(Var, NewUniqueName, false /*bInNotifyGraphChanged*/); // Notify graph changed in child.

	if (RenamedPin == PinPendingRename)
	{
		PinPendingRename = nullptr;
	}

}

#undef LOCTEXT_NAMESPACE