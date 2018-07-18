// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeIf.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraHlslTranslator.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeIf"

const FString UNiagaraNodeIf::InputAPinSuffix(" A");
const FString UNiagaraNodeIf::InputBPinSuffix(" B");

UNiagaraNodeIf::UNiagaraNodeIf(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNiagaraNodeIf::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// @TODO why do we need to have this post-change property here at all? 
	// Doing a null check b/c otherwise if doing a Duplicate via Ctrl-W, we die inside AllocateDefaultPins due to 
	// the point where we get this call not being completely formed.
	if (PropertyChangedEvent.Property != nullptr)
	{
		ReallocatePins();
	}
}

void UNiagaraNodeIf::PostLoad()
{
	Super::PostLoad();

	if (OutputVars.Num() != OutputVarGuids.Num())
	{
		OutputVarGuids.SetNum(OutputVars.Num());
	}

	if (OutputVars.Num() != InputAVarGuids.Num())
	{
		InputAVarGuids.SetNum(OutputVars.Num());
	}

	if (OutputVars.Num() != InputBVarGuids.Num())
	{
		InputBVarGuids.SetNum(OutputVars.Num());
	}

	auto LoadGuid = [&](FGuid& Guid, const FString& Name, const EEdGraphPinDirection Direction)
	{
		UEdGraphPin* Pin = FindPin(Name, Direction);
		if (Pin)
		{
			if (!Pin->PersistentGuid.IsValid())
			{
				Pin->PersistentGuid = FGuid::NewGuid();
			}
			Guid = Pin->PersistentGuid;
		}
		else
		{
			UE_LOG(LogNiagaraEditor, Error, TEXT("Unable to find output pin named %s"), *Name);
		}
	};

	for (int32 i = 0; i < OutputVars.Num(); i++)
	{
		const FString VarName = OutputVars[i].GetName().ToString();
		LoadGuid(OutputVarGuids[i], VarName, EGPD_Output);
		const FString InputAName = VarName + InputAPinSuffix;
		LoadGuid(InputAVarGuids[i], InputAName, EGPD_Input);
		const FString InputBName = VarName + InputBPinSuffix;
		LoadGuid(InputBVarGuids[i], InputBName, EGPD_Input);
	}
}

bool UNiagaraNodeIf::AllowNiagaraTypeForAddPin(const FNiagaraTypeDefinition& InType)
{
	return Super::AllowNiagaraTypeForAddPin(InType) && InType != FNiagaraTypeDefinition::GetParameterMapDef();
}

void UNiagaraNodeIf::AllocateDefaultPins()
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	//Add the condition pin.
	CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(FNiagaraTypeDefinition::GetBoolDef()), TEXT("Condition"));

	//Create the inputs for each path.
	for (int32 Index = 0; Index < OutputVars.Num(); Index++)
	{
		const FNiagaraVariable& Var = OutputVars[Index];
		UEdGraphPin* NewPin = CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(Var.GetType()), *(Var.GetName().ToString() + InputAPinSuffix));
		NewPin->PersistentGuid = InputAVarGuids[Index];
	}

	for (int32 Index = 0; Index < OutputVars.Num(); Index++)
	{
		const FNiagaraVariable& Var = OutputVars[Index];
		UEdGraphPin* NewPin = CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(Var.GetType()), *(Var.GetName().ToString() + InputBPinSuffix));
		NewPin->PersistentGuid = InputBVarGuids[Index];
	}

	for (int32 Index = 0; Index < OutputVars.Num(); Index++)
	{
		const FNiagaraVariable& Var = OutputVars[Index];
		UEdGraphPin* NewPin = CreatePin(EGPD_Output, Schema->TypeDefinitionToPinType(Var.GetType()), Var.GetName());
		NewPin->PersistentGuid = OutputVarGuids[Index];
	}

	CreateAddPin(EGPD_Output);
}

void UNiagaraNodeIf::Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs)
{
	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());

	int32 PinIdx = 0;
	int32 Condition = Translator->CompilePin(Pins[PinIdx++]);
	TArray<int32> PathA;
	PathA.Reserve(OutputVars.Num());
	for (int32 i = 0; i < OutputVars.Num(); ++i)
	{
		if (Schema->PinToTypeDefinition(Pins[PinIdx]) == FNiagaraTypeDefinition::GetParameterMapDef())
		{
			Translator->Error(LOCTEXT("UnsupportedParamMapInIf","Parameter maps are not supported in if nodes."), this, Pins[PinIdx]);
		}

		PathA.Add(Translator->CompilePin(Pins[PinIdx++]));
	}
	TArray<int32> PathB;
	PathB.Reserve(OutputVars.Num());
	for (int32 i = 0; i < OutputVars.Num(); ++i)
	{
		if (Schema->PinToTypeDefinition(Pins[PinIdx]) == FNiagaraTypeDefinition::GetParameterMapDef())
		{
			Translator->Error(LOCTEXT("UnsupportedParamMapInIf", "Parameter maps are not supported in if nodes."), this, Pins[PinIdx]);
		}

		PathB.Add(Translator->CompilePin(Pins[PinIdx++]));
	}
	Translator->If(OutputVars, Condition, PathA, PathB, Outputs);
}

bool UNiagaraNodeIf::RefreshFromExternalChanges()
{
	// TODO - Leverage code in reallocate pins to determine if any pins have changed...
	ReallocatePins();
	return true;
}

FGuid UNiagaraNodeIf::AddOutput(FNiagaraTypeDefinition Type, const FName& Name)
{
	FNiagaraVariable NewOutput(Type, Name);
	OutputVars.Add(NewOutput);
	FGuid Guid = FGuid::NewGuid();
	OutputVarGuids.Add(Guid);

	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
	FGuid PinAGuid = FGuid::NewGuid();
	InputAVarGuids.Add(PinAGuid);
	UEdGraphPin* PinA = CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(Type), *(Name.ToString() + InputAPinSuffix), InputAVarGuids.Num());
	PinA->PersistentGuid = PinAGuid;

	FGuid PinBGuid = FGuid::NewGuid();
	InputBVarGuids.Add(PinBGuid);
	UEdGraphPin* PinB = CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(Type), *(Name.ToString() + InputBPinSuffix), InputAVarGuids.Num() + InputBVarGuids.Num());
	PinB->PersistentGuid = PinBGuid;

	return Guid;
}

void UNiagaraNodeIf::OnPinRemoved(UEdGraphPin* PinToRemove)
{
	auto FindPredicate = [=](const FGuid& Guid) { return Guid == PinToRemove->PersistentGuid; };
	int32 FoundIndex = OutputVarGuids.IndexOfByPredicate(FindPredicate);
	if (FoundIndex != INDEX_NONE)
	{
		OutputVarGuids.RemoveAt(FoundIndex);
		OutputVars.RemoveAt(FoundIndex);
	}
	ReallocatePins();
}

void UNiagaraNodeIf::OnNewTypedPinAdded(UEdGraphPin* NewPin)
{
	Super::OnNewTypedPinAdded(NewPin);

	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	FNiagaraTypeDefinition OutputType = Schema->PinToTypeDefinition(NewPin);

	TSet<FName> OutputNames;
	for (const FNiagaraVariable& Output : OutputVars)
	{
		OutputNames.Add(Output.GetName());
	}
	FName OutputName = FNiagaraUtilities::GetUniqueName(*OutputType.GetNameText().ToString(), OutputNames);

	FGuid Guid = AddOutput(OutputType, OutputName);

	// Update the pin's data too so that it's connection is maintained after reallocating.
	NewPin->PinName = OutputName;
	NewPin->PersistentGuid = Guid;
}

void UNiagaraNodeIf::OnPinRenamed(UEdGraphPin* RenamedPin, const FString& OldName)
{
	auto FindPredicate = [=](const FGuid& Guid) { return Guid == RenamedPin->PersistentGuid; };
	int32 FoundIndex = OutputVarGuids.IndexOfByPredicate(FindPredicate);
	if(FoundIndex != INDEX_NONE)
	{
		TSet<FName> OutputNames;
		for (int32 Index = 0; Index < OutputVars.Num(); Index++)
		{
			if (FoundIndex != Index)
			{
				OutputNames.Add(OutputVars[Index].GetName());
			}
		}
		const FName OutputName = FNiagaraUtilities::GetUniqueName(RenamedPin->PinName, OutputNames);
		OutputVars[FoundIndex].SetName(OutputName);
	}
	ReallocatePins();
}

bool UNiagaraNodeIf::CanRenamePin(const UEdGraphPin* Pin) const
{
	return Super::CanRenamePin(Pin) && Pin->Direction == EGPD_Output;
}

bool UNiagaraNodeIf::CanRemovePin(const UEdGraphPin* Pin) const
{
	return Super::CanRemovePin(Pin) && Pin->Direction == EGPD_Output;
}


FText UNiagaraNodeIf::GetTooltipText() const
{
	return LOCTEXT("IfDesc", "If Condition is true, the output value is A, otherwise output B.");
}

FText UNiagaraNodeIf::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("IfTitle", "If");
}

#undef LOCTEXT_NAMESPACE
