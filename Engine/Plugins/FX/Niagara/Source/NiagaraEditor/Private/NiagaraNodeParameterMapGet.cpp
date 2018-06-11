// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeParameterMapGet.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraEditorUtilities.h"
#include "SNiagaraGraphNodeConvert.h"
#include "NiagaraGraph.h"
#include "NiagaraHlslTranslator.h"
#include "ScopedTransaction.h"
#include "SNiagaraGraphParameterMapGetNode.h"
#include "NiagaraEditorModule.h"
#include "INiagaraEditorTypeUtilities.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"
#include "NiagaraConstants.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "EdGraph/EdGraphNode.h"
#include "NiagaraParameterCollection.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeParameterMapGet"

UNiagaraNodeParameterMapGet::UNiagaraNodeParameterMapGet() : UNiagaraNodeParameterMapBase()
{

}


void UNiagaraNodeParameterMapGet::AllocateDefaultPins()
{
	PinPendingRename = nullptr;
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(FNiagaraTypeDefinition::GetParameterMapDef()), *UNiagaraNodeParameterMapBase::SourcePinName.ToString());
	CreateAddPin(EGPD_Output);
}


TSharedPtr<SGraphNode> UNiagaraNodeParameterMapGet::CreateVisualWidget()
{
	return SNew(SNiagaraGraphParameterMapGetNode, this);
}

bool UNiagaraNodeParameterMapGet::IsPinNameEditable(const UEdGraphPin* GraphPinObj) const
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	FNiagaraTypeDefinition TypeDef = Schema->PinToTypeDefinition(GraphPinObj);
	if (TypeDef.IsValid() && GraphPinObj && GraphPinObj->Direction == EGPD_Output && CanRenamePin(GraphPinObj))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool UNiagaraNodeParameterMapGet::IsPinNameEditableUponCreation(const UEdGraphPin* GraphPinObj) const
{
	if (GraphPinObj == PinPendingRename && GraphPinObj->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		return true;
	}
	else
	{
		return false;
	}
}


bool UNiagaraNodeParameterMapGet::VerifyEditablePinName(const FText& InName, FText& OutErrorMessage, const UEdGraphPin* InGraphPinObj) const
{
	if (InName.IsEmptyOrWhitespace() && InGraphPinObj->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		OutErrorMessage = LOCTEXT("InvalidName", "Invalid pin name");
		return false;
	}
	return true;
}

UEdGraphPin* UNiagaraNodeParameterMapGet::CreateDefaultPin(UEdGraphPin* OutputPin)
{
	if (OutputPin == nullptr)
	{
		return nullptr;
	}

	UEdGraphPin* DefaultPin = CreatePin(EEdGraphPinDirection::EGPD_Input, OutputPin->PinType, TEXT(""));

	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	FNiagaraTypeDefinition NiagaraType = Schema->PinToTypeDefinition(OutputPin);
	bool bNeedsValue = NiagaraType.IsDataInterface() == false;
	FNiagaraVariable Var = Schema->PinToNiagaraVariable(OutputPin, bNeedsValue);

	FString PinDefaultValue;
	if (Schema->TryGetPinDefaultValueFromNiagaraVariable(Var, PinDefaultValue))
	{
		DefaultPin->DefaultValue = PinDefaultValue;
	}
	
	if (!OutputPin->PersistentGuid.IsValid())
	{
		OutputPin->PersistentGuid = FGuid::NewGuid();
	}
	if (!DefaultPin->PersistentGuid.IsValid())
	{
		DefaultPin->PersistentGuid = FGuid::NewGuid();
	}
	PinOutputToPinDefaultPersistentId.Add(OutputPin->PersistentGuid, DefaultPin->PersistentGuid);

	SynchronizeDefaultInputPin(DefaultPin, OutputPin);
	return DefaultPin;
}

void UNiagaraNodeParameterMapGet::OnPinRenamed(UEdGraphPin* RenamedPin, const FString& OldName)
{
	UNiagaraNodeParameterMapBase::OnPinRenamed(RenamedPin, OldName);

	UEdGraphPin* DefaultPin = GetDefaultPin(RenamedPin);
	if (DefaultPin)
	{
		DefaultPin->Modify();
		SynchronizeDefaultInputPin(DefaultPin, RenamedPin);
	}

	MarkNodeRequiresSynchronization(__FUNCTION__, true);
}


void UNiagaraNodeParameterMapGet::OnNewTypedPinAdded(UEdGraphPin* NewPin)
{
	if (NewPin->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		TArray<UEdGraphPin*> OutputPins;
		GetOutputPins(OutputPins);

		TSet<FName> Names;
		for (const UEdGraphPin* Pin : OutputPins)
		{
			if (Pin != NewPin)
			{
				Names.Add(Pin->GetFName());
			}
		}
		const FName NewUniqueName = FNiagaraUtilities::GetUniqueName(*NewPin->GetName(), Names);
		NewPin->PinName = NewUniqueName;

		const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
		FNiagaraTypeDefinition TypeDef = Schema->PinToTypeDefinition(NewPin);

		UEdGraphPin* MatchingDefault = GetDefaultPin(NewPin);
		if (MatchingDefault == nullptr)
		{
			UEdGraphPin* DefaultPin = CreateDefaultPin(NewPin);
		}

		NewPin->PinType.PinSubCategory = UNiagaraNodeParameterMapBase::ParameterPinSubCategory;
	}

	if (HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedInitialization))
	{
		return;
	}

	if (NewPin->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		PinPendingRename = NewPin;
	}

}

void UNiagaraNodeParameterMapGet::RemoveDynamicPin(UEdGraphPin* Pin)
{
	UEdGraphPin* DefaultPin = nullptr;
	if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		DefaultPin = GetDefaultPin(Pin);

		FNiagaraVariable Var = CastChecked<UEdGraphSchema_Niagara>(GetSchema())->PinToNiagaraVariable(Pin);

		UNiagaraGraph* Graph = GetNiagaraGraph();
		FNiagaraVariableMetaData* OldMetaData = Graph->GetMetaData(Var);

		if (OldMetaData)
		{
			Graph->Modify();
			OldMetaData->ReferencerNodes.Remove(TWeakObjectPtr<UObject>(this));
		}
	}

	RemovePin(Pin);

	if (DefaultPin != nullptr)
	{
		RemovePin(DefaultPin);
	}

	MarkNodeRequiresSynchronization(__FUNCTION__, true);
	//GetGraph()->NotifyGraphChanged();
}

UEdGraphPin* UNiagaraNodeParameterMapGet::GetDefaultPin(UEdGraphPin* OutputPin) const
{
	TArray<UEdGraphPin*> InputPins;
	GetInputPins(InputPins);

	const FGuid* InputGuid = PinOutputToPinDefaultPersistentId.Find(OutputPin->PersistentGuid);

	if (InputGuid != nullptr)
	{
		for (UEdGraphPin* InputPin : InputPins)
		{
			if ((*InputGuid) == InputPin->PersistentGuid)
			{
				return InputPin;
			}
		}
	}

	return nullptr;
}


UEdGraphPin* UNiagaraNodeParameterMapGet::GetOutputPinForDefault(const UEdGraphPin* DefaultPin) const
{
	TArray<UEdGraphPin*> OutputPins;
	GetOutputPins(OutputPins);

	FGuid OutputGuid;
	for (auto It : PinOutputToPinDefaultPersistentId)
	{
		if (It.Value == DefaultPin->PersistentGuid)
		{
			OutputGuid = It.Key;
			break;
		}
	}

	if (OutputGuid.IsValid())
	{
		for (UEdGraphPin* OutputPin : OutputPins)
		{
			if (OutputGuid == OutputPin->PersistentGuid)
			{
				return OutputPin;
			}
		}
	}

	return nullptr;
}

void UNiagaraNodeParameterMapGet::PostLoad()
{
	Super::PostLoad();
	TArray<UEdGraphPin*> OutputPins;
	GetOutputPins(OutputPins);
	for (int32 i = 0; i < OutputPins.Num(); i++)
	{
		UEdGraphPin* OutputPin = OutputPins[i];
		if (IsAddPin(OutputPin))
		{
			continue;
		}

		UEdGraphPin* InputPin = GetDefaultPin(OutputPin);
		if (InputPin == nullptr)
		{
			CreateDefaultPin(OutputPin);
		}
		else
		{
			SynchronizeDefaultInputPin(InputPin, OutputPin);
		}

		OutputPin->PinType.PinSubCategory = UNiagaraNodeParameterMapBase::ParameterPinSubCategory;
	}
}


void UNiagaraNodeParameterMapGet::SynchronizeDefaultInputPin(UEdGraphPin* DefaultPin, UEdGraphPin* OutputPin)
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	if (!DefaultPin)
	{
		return;
	}

	if (FNiagaraParameterMapHistory::IsEngineParameter(Schema->PinToNiagaraVariable(OutputPin)))
	{
		DefaultPin->bDefaultValueIsIgnored = true;
		DefaultPin->bNotConnectable = true;
		DefaultPin->bHidden = true;
		DefaultPin->PinToolTip = FText::Format(LOCTEXT("DefaultValueTooltip", "Default value for {0}. Disabled for Engine Parameters."), FText::FromName(OutputPin->PinName)).ToString();
	}
	else
	{
		DefaultPin->bDefaultValueIsIgnored = false;
		DefaultPin->bNotConnectable = false;
		DefaultPin->bHidden = false;
		DefaultPin->PinToolTip = FText::Format(LOCTEXT("DefaultValueTooltip", "Default value for {0} if no other module has set it previously in the stack."), FText::FromName(OutputPin->PinName)).ToString();
	}
}


FText UNiagaraNodeParameterMapGet::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UNiagaraNodeParameterMapGetName", "Map Get");
}

void UNiagaraNodeParameterMapGet::BuildParameterMapHistory(FNiagaraParameterMapHistoryBuilder& OutHistory, bool bRecursive)
{
	if (bRecursive)
	{
		OutHistory.VisitInputPin(GetInputPin(0), this);
	}

	if (!IsNodeEnabled() && OutHistory.GetIgnoreDisabled())
	{
		RouteParameterMapAroundMe(OutHistory, bRecursive);
		return;
	}

	int32 ParamMapIdx = INDEX_NONE;
	if (GetInputPin(0)->LinkedTo.Num() != 0)
	{
		ParamMapIdx = OutHistory.TraceParameterMapOutputPin(UNiagaraNode::TraceOutputPin(GetInputPin(0)->LinkedTo[0]));
	}

	if (ParamMapIdx != INDEX_NONE)
	{
		uint32 NodeIdx = OutHistory.BeginNodeVisitation(ParamMapIdx, this);
		TArray<UEdGraphPin*> OutputPins;
		GetOutputPins(OutputPins);
		for (int32 i = 0; i < OutputPins.Num(); i++)
		{
			if (IsAddPin(OutputPins[i]))
			{
				continue;
			}

			bool bUsedDefaults = false;
			if (bRecursive)
			{
				OutHistory.HandleVariableRead(ParamMapIdx, OutputPins[i], true, GetDefaultPin(OutputPins[i]), bUsedDefaults);
			}
			else
			{
				OutHistory.HandleVariableRead(ParamMapIdx, OutputPins[i], true, nullptr, bUsedDefaults);
			}
		}
		OutHistory.EndNodeVisitation(ParamMapIdx, NodeIdx);
	}
}

void UNiagaraNodeParameterMapGet::Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs)
{
	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());

	TArray<UEdGraphPin*> InputPins;
	GetInputPins(InputPins);
	TArray<UEdGraphPin*> OutputPins;
	GetOutputPins(OutputPins);

	// Initialize the outputs to invalid values.
	check(Outputs.Num() == 0);
	for (int32 i = 0; i < OutputPins.Num(); i++)
	{
		if (IsAddPin(OutputPins[i]))
		{
			continue;
		}
		Outputs.Add(INDEX_NONE);
	}

	// First compile fully down the hierarchy for our predecessors..
	TArray<int32> CompileInputs;

	for (int32 i = 0; i < InputPins.Num(); i++)
	{
		UEdGraphPin* InputPin = InputPins[i];

		if (InputPin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryType || 
			InputPin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryEnum)
		{
			int32 CompiledInput = INDEX_NONE;
			if (i == 0) // Only the zeroth item is not an default value pin.
			{
				CompiledInput = Translator->CompilePin(InputPin);
				if (CompiledInput == INDEX_NONE)
				{
					Translator->Error(LOCTEXT("InputError", "Error compiling input for param map get node."), this, InputPin);
				}
			}
			CompileInputs.Add(CompiledInput);
		}
	}

	UNiagaraGraph* Graph = Cast<UNiagaraGraph>(GetGraph());
	// By this point, we've visited all of our child nodes in the call graph. We can mine them to find out everyone contributing to the parameter map (and when).
	if (GetInputPin(0) != nullptr && GetInputPin(0)->LinkedTo.Num() > 0)
	{
		Translator->ParameterMapGet(this, CompileInputs, Outputs);
	}
}

bool UNiagaraNodeParameterMapGet::CancelEditablePinName(const FText& InName, UEdGraphPin* InGraphPinObj)
{
	if (InGraphPinObj == PinPendingRename)
	{
		PinPendingRename = nullptr;
	}
	return true;
}

bool UNiagaraNodeParameterMapGet::CommitEditablePinName(const FText& InName, UEdGraphPin* InGraphPinObj) 
{
	if (Pins.Contains(InGraphPinObj) && InGraphPinObj->Direction == EEdGraphPinDirection::EGPD_Output)
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

void UNiagaraNodeParameterMapGet::GatherExternalDependencyIDs(ENiagaraScriptUsage InMasterUsage, const FGuid& InMasterUsageId, TArray<FGuid>& InReferencedIDs, TArray<UObject*>& InReferencedObjs) const
{
	// If we are referencing any parameter collections, we need to register them here... might want to speeed this up in the future 
	// by caching any parameter collections locally.
	TArray<UEdGraphPin*> OutputPins;
	GetOutputPins(OutputPins);
	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());

	for (int32 i = 0; i < OutputPins.Num(); i++)
	{
		if (IsAddPin(OutputPins[i]))
		{
			continue;
		}

		FNiagaraVariable Var = CastChecked<UEdGraphSchema_Niagara>(GetSchema())->PinToNiagaraVariable(OutputPins[i]);
		UNiagaraParameterCollection* Collection = Schema->VariableIsFromParameterCollection(Var);
		if (Collection)
		{
			InReferencedIDs.Add(Collection->GetCompileId());
			InReferencedObjs.Add(Collection);
		}

	}
}

void UNiagaraNodeParameterMapGet::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	// Get hover text from metadata description.
	const UNiagaraGraph* NiagaraGraph = GetNiagaraGraph();
	if (NiagaraGraph)
	{
		const UEdGraphSchema_Niagara* Schema = Cast<UEdGraphSchema_Niagara>(NiagaraGraph->GetSchema());

		if (Schema)
		{
			FNiagaraTypeDefinition TypeDef = Schema->PinToTypeDefinition(&Pin);
			if (IsAddPin(&Pin))
			{
				HoverTextOut = LOCTEXT("ParameterMapAddString", "Request a new variable from the parameter map.").ToString();
				return;
			}

			if (Pin.Direction == EEdGraphPinDirection::EGPD_Input)
			{
				if (&Pin == GetInputPin(0) && TypeDef == FNiagaraTypeDefinition::GetParameterMapDef())
				{
					HoverTextOut = LOCTEXT("ParameterMapInString", "The source parameter map where we pull the values from.").ToString();
					return;
				}
				const UEdGraphPin* OutputPin = GetOutputPinForDefault(&Pin);
				if (OutputPin)
				{
					TypeDef = Schema->PinToTypeDefinition(OutputPin);
					FNiagaraVariable Var = FNiagaraVariable(TypeDef, OutputPin->PinName);
					const FNiagaraVariableMetaData* Metadata = NiagaraGraph->GetMetaData(Var);
					if (Metadata)
					{
						FText Desc = FText::Format(LOCTEXT("DefaultValueTooltip", "Default value for \"{0}\" if no other module has set it previously in the stack."), FText::FromName(OutputPin->PinName));
						HoverTextOut = Desc.ToString();
						return;
					}
				}
				
			}
			else
			{
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
}

void UNiagaraNodeParameterMapGet::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	UNiagaraNodeParameterMapBase::GetContextMenuActions(Context);

	UEdGraphPin* Pin = const_cast<UEdGraphPin*>(Context.Pin);
	if (Pin && Pin->Direction == EEdGraphPinDirection::EGPD_Output)
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

#undef LOCTEXT_NAMESPACE
