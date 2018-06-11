// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeParameterMapSet.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraEditorUtilities.h"
#include "SNiagaraGraphNodeConvert.h"
#include "NiagaraHlslTranslator.h"
#include "Templates/SharedPointer.h"
#include "NiagaraGraph.h"
#include "NiagaraConstants.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "EdGraph/EdGraphNode.h"

#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeParameterMapSet"

UNiagaraNodeParameterMapSet::UNiagaraNodeParameterMapSet() : UNiagaraNodeParameterMapBase()
{

}

void UNiagaraNodeParameterMapSet::AllocateDefaultPins()
{
	PinPendingRename = nullptr;
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(FNiagaraTypeDefinition::GetParameterMapDef()), *UNiagaraNodeParameterMapBase::SourcePinName.ToString());
	CreatePin(EGPD_Output, Schema->TypeDefinitionToPinType(FNiagaraTypeDefinition::GetParameterMapDef()), *UNiagaraNodeParameterMapBase::DestPinName.ToString());
	CreateAddPin(EGPD_Input);
}

bool UNiagaraNodeParameterMapSet::IsPinNameEditable(const UEdGraphPin* GraphPinObj) const
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	FNiagaraTypeDefinition TypeDef = Schema->PinToTypeDefinition(GraphPinObj);
	if (TypeDef.IsValid() && GraphPinObj && GraphPinObj->Direction == EGPD_Input && CanRenamePin(GraphPinObj))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool UNiagaraNodeParameterMapSet::IsPinNameEditableUponCreation(const UEdGraphPin* GraphPinObj) const
{
	if (GraphPinObj == PinPendingRename)
	{
		return true;
	}
	else
	{
		return false;
	}
}


bool UNiagaraNodeParameterMapSet::VerifyEditablePinName(const FText& InName, FText& OutErrorMessage, const UEdGraphPin* InGraphPinObj) const
{
	if (InName.IsEmptyOrWhitespace())
	{
		OutErrorMessage = LOCTEXT("InvalidName", "Invalid pin name");
		return false;
	}
	return true;
}

void UNiagaraNodeParameterMapSet::OnNewTypedPinAdded(UEdGraphPin* NewPin)
{
	if (HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedInitialization))
	{
		return;
	}

	if (NewPin->Direction == EEdGraphPinDirection::EGPD_Input)
	{
		TArray<UEdGraphPin*> InputPins;
		GetInputPins(InputPins);

		TSet<FName> Names;
		for (const UEdGraphPin* Pin : InputPins)
		{
			if (Pin != NewPin)
			{
				Names.Add(Pin->GetFName());
			}
		}
		const FName NewUniqueName = FNiagaraUtilities::GetUniqueName(*NewPin->GetName(), Names);
		NewPin->PinName = NewUniqueName;
		NewPin->PinType.PinSubCategory = UNiagaraNodeParameterMapBase::ParameterPinSubCategory;
	}

	if (!NewPin->PersistentGuid.IsValid())
	{
		NewPin->PersistentGuid = FGuid::NewGuid();
	}

	PinPendingRename = NewPin;
}

void UNiagaraNodeParameterMapSet::OnPinRenamed(UEdGraphPin* RenamedPin, const FString& OldName)
{
	UNiagaraNodeParameterMapBase::OnPinRenamed(RenamedPin, OldName);
	MarkNodeRequiresSynchronization(__FUNCTION__, true);
}

void UNiagaraNodeParameterMapSet::SetPinName(UEdGraphPin* InPin, const FName& InName)
{
	FName OldName = InPin->PinName;
	InPin->PinName = InName;
	OnPinRenamed(InPin, OldName.ToString());
}

bool UNiagaraNodeParameterMapSet::CancelEditablePinName(const FText& InName, UEdGraphPin* InGraphPinObj)
{
	if (InGraphPinObj == PinPendingRename)
	{
		PinPendingRename = nullptr;
	}
	return true;
}

bool UNiagaraNodeParameterMapSet::CommitEditablePinName(const FText& InName, UEdGraphPin* InGraphPinObj) 
{
	if (Pins.Contains(InGraphPinObj))
	{
		FScopedTransaction AddNewPinTransaction(LOCTEXT("Rename Pin", "Renamed pin"));
		Modify();
		InGraphPinObj->Modify();
		
		FString OldPinName = InGraphPinObj->PinName.ToString();
		InGraphPinObj->PinName = *InName.ToString();
		OnPinRenamed(InGraphPinObj, OldPinName);

		return true;
	}
	return false;
}

void UNiagaraNodeParameterMapSet::Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs)
{
	TArray<UEdGraphPin*> InputPins;
	GetInputPins(InputPins);

	TArray<UEdGraphPin*> OutputPins;
	GetOutputPins(OutputPins);

	// Initialize the outputs to invalid values.
	check(Outputs.Num() == 0);
	for (int32 i = 0; i < OutputPins.Num(); i++)
	{
		Outputs.Add(INDEX_NONE);
	}

	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());

	// First compile fully down the hierarchy for our predecessors..
	TArray<int32> CompileInputs;
	for (UEdGraphPin* InputPin : InputPins)
	{
		if (IsAddPin(InputPin))
		{
			continue;
		}

		if (IsNodeEnabled() == false && Schema->PinToTypeDefinition(InputPin) != FNiagaraTypeDefinition::GetParameterMapDef())
		{
			continue;
		}

		int32 CompiledInput = Translator->CompilePin(InputPin);
		if (CompiledInput == INDEX_NONE)
		{
			Translator->Error(LOCTEXT("InputError", "Error compiling input for set node."), this, InputPin);
		}
		CompileInputs.Add(CompiledInput);
	}

	if (GetInputPin(0) != nullptr && GetInputPin(0)->LinkedTo.Num() > 0)
	{
		Translator->ParameterMapSet(this, CompileInputs, Outputs);
	}
}

FText UNiagaraNodeParameterMapSet::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UNiagaraNodeParameterMapSetName", "Map Set");
}


void UNiagaraNodeParameterMapSet::BuildParameterMapHistory(FNiagaraParameterMapHistoryBuilder& OutHistory, bool bRecursive)
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	TArray<UEdGraphPin*> InputPins;
	GetInputPins(InputPins);
	
	int32 ParamMapIdx = INDEX_NONE;
	uint32 NodeIdx = INDEX_NONE;

	for (int32 i = 0; i < InputPins.Num(); i++)
	{
		if (IsAddPin(InputPins[i]))
		{
			continue;
		}

		OutHistory.VisitInputPin(InputPins[i], this);


		if (!IsNodeEnabled() && OutHistory.GetIgnoreDisabled())
		{
			continue;
		}

		FNiagaraTypeDefinition VarTypeDef = Schema->PinToTypeDefinition(InputPins[i]);
		if (i == 0 && InputPins[i] != nullptr && VarTypeDef == FNiagaraTypeDefinition::GetParameterMapDef())
		{
			UEdGraphPin* PriorParamPin = nullptr;
			if (InputPins[i]->LinkedTo.Num() > 0)
			{
				PriorParamPin = InputPins[i]->LinkedTo[0];
			}

			// Now plow into our ancestor node
			if (PriorParamPin)
			{
				ParamMapIdx = OutHistory.TraceParameterMapOutputPin(PriorParamPin);
				NodeIdx = OutHistory.BeginNodeVisitation(ParamMapIdx, this);
			}
		}
		else if (i > 0 && InputPins[i] != nullptr && ParamMapIdx != INDEX_NONE)
		{
			OutHistory.HandleVariableWrite(ParamMapIdx, InputPins[i]);
		}
	}

	if (!IsNodeEnabled() && OutHistory.GetIgnoreDisabled())
	{
		RouteParameterMapAroundMe(OutHistory, bRecursive);
		return;
	}

	if (ParamMapIdx != INDEX_NONE)
	{
		OutHistory.EndNodeVisitation(ParamMapIdx, NodeIdx);
	}

	OutHistory.RegisterParameterMapPin(ParamMapIdx, GetOutputPin(0));
}

void UNiagaraNodeParameterMapSet::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	UNiagaraNodeParameterMapBase::GetContextMenuActions(Context);

	UEdGraphPin* Pin = const_cast<UEdGraphPin*>(Context.Pin);
	if (Pin && Pin->Direction == EEdGraphPinDirection::EGPD_Input)
	{
		FNiagaraVariable Var = CastChecked<UEdGraphSchema_Niagara>(GetSchema())->PinToNiagaraVariable(Pin);
		const UNiagaraGraph* Graph = GetNiagaraGraph();

		if (!FNiagaraConstants::IsNiagaraConstant(Var))
		{
			Context.MenuBuilder->BeginSection("EdGraphSchema_NiagaraMetaDataActions", LOCTEXT("EditPinMenuHeader", "Meta-Data"));
			TSharedRef<SWidget> RenameWidget =
				SNew(SBox)
				.WidthOverride(100)
				.Padding(FMargin(5, 0, 0, 0))
				[
					SNew(SEditableTextBox)
					.Text_UObject(this, &UNiagaraNodeParameterMapBase::GetPinDescriptionText, Pin)
					.OnTextCommitted_UObject(this, &UNiagaraNodeParameterMapBase::PinDescriptionTextCommitted, Pin)
				];
			Context.MenuBuilder->AddWidget(RenameWidget, LOCTEXT("DescMenuItem", "Description"));

			Context.MenuBuilder->EndSection();
		}
	}
}

void UNiagaraNodeParameterMapSet::PostLoad()
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (!Pin->PersistentGuid.IsValid())
		{
			Pin->PersistentGuid = FGuid::NewGuid();
		}

		if (Pin->Direction == EEdGraphPinDirection::EGPD_Input && Pin->GetFName() != UNiagaraNodeParameterMapBase::SourcePinName && !IsAddPin(Pin))
		{
			Pin->PinType.PinSubCategory = UNiagaraNodeParameterMapBase::ParameterPinSubCategory;
		}
	}
	Super::PostLoad();
}

#undef LOCTEXT_NAMESPACE
