// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeUsageSelector.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraHlslTranslator.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeUsageSelector"

UNiagaraNodeUsageSelector::UNiagaraNodeUsageSelector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNiagaraNodeUsageSelector::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// @TODO why do we need to have this post-change property here at all? 
	// Doing a null check b/c otherwise if doing a Duplicate via Ctrl-W, we die inside AllocateDefaultPins due to 
	// the point where we get this call not being completely formed.
	if (PropertyChangedEvent.Property != nullptr)
	{
		ReallocatePins();
	}
}

void UNiagaraNodeUsageSelector::PostLoad()
{
	Super::PostLoad();
}

bool UNiagaraNodeUsageSelector::AllowNiagaraTypeForAddPin(const FNiagaraTypeDefinition& InType)
{
	return Super::AllowNiagaraTypeForAddPin(InType) && InType != FNiagaraTypeDefinition::GetParameterMapDef();
}

void UNiagaraNodeUsageSelector::InsertInputPinsFor(const FNiagaraVariable& Var)
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	UEnum* ENiagaraScriptGroupEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ENiagaraScriptGroup"), true);
	int64 GroupCount = (int64)ENiagaraScriptGroup::Max;

	TArray<UEdGraphPin*> OldPins(Pins);
	Pins.Reset(Pins.Num() + GroupCount);

	// Create the inputs for each path.
	for (int64 i = 0; i < GroupCount; i++)
	{
		// Add the previous input pins
		for (int32 k = 0; k < OutputVars.Num() - 1; k++)
		{
			Pins.Add(OldPins[k]);
		}
		OldPins.RemoveAt(0, OutputVars.Num() - 1);

		// Add the new input pin
		const FString PathSuffix = ENiagaraScriptGroupEnum ? (FString::Printf(TEXT(" if %s"), *ENiagaraScriptGroupEnum->GetNameStringByValue((int64)i))) : TEXT("Error Unknown!");
		CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(Var.GetType()), *(Var.GetName().ToString() + PathSuffix));
	}

	// Move the rest of the old pins over
	Pins.Append(OldPins);
}

void UNiagaraNodeUsageSelector::AllocateDefaultPins()
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	UEnum* ENiagaraScriptGroupEnum = StaticEnum<ENiagaraScriptGroup>();

	//Create the inputs for each path.
	for (int64 i = 0; i < (int64)ENiagaraScriptGroup::Max; i++)
	{
		const FString PathSuffix = ENiagaraScriptGroupEnum ? ( FString::Printf(TEXT(" if %s"), *ENiagaraScriptGroupEnum->GetNameStringByValue((int64)i))) : TEXT("Error Unknown!");
		for (FNiagaraVariable& Var : OutputVars)
		{
			CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(Var.GetType()), *(Var.GetName().ToString() + PathSuffix));
		}
	}

	for (int32 Index = 0; Index < OutputVars.Num(); Index++)
	{
		const FNiagaraVariable& Var = OutputVars[Index];
		UEdGraphPin* NewPin = CreatePin(EGPD_Output, Schema->TypeDefinitionToPinType(Var.GetType()), Var.GetName());
		NewPin->PersistentGuid = OutputVarGuids[Index];
	}

	CreateAddPin(EGPD_Output);
}

bool UNiagaraNodeUsageSelector::RefreshFromExternalChanges()
{
	ReallocatePins();
	return true;
}


void UNiagaraNodeUsageSelector::Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs)
{
	TArray<UEdGraphPin*> InputPins;
	GetInputPins(InputPins);
	TArray<UEdGraphPin*> OutputPins;
	GetOutputPins(OutputPins);

	ENiagaraScriptUsage CurrentUsage = Translator->GetCurrentUsage();
	ENiagaraScriptGroup UsageGroup = ENiagaraScriptGroup::Max;
	if (UNiagaraScript::ConvertUsageToGroup(CurrentUsage, UsageGroup))
	{
		int32 VarIdx = 0;
		for (int64 i = 0; i < (int64)ENiagaraScriptGroup::Max; i++)
		{
			if ((int64)UsageGroup == i)
			{
				break;
			}

			VarIdx += OutputVars.Num();
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
	else
	{
		Translator->Error(LOCTEXT("InvalidUsage", "Invalid script usage"), this, nullptr);
	}
}

UEdGraphPin* UNiagaraNodeUsageSelector::GetPassThroughPin(const UEdGraphPin* LocallyOwnedOutputPin, ENiagaraScriptUsage MasterUsage) const 
{
	check(Pins.Contains(LocallyOwnedOutputPin) && LocallyOwnedOutputPin->Direction == EGPD_Output);

	ENiagaraScriptGroup UsageGroup = ENiagaraScriptGroup::Max;
	if (UNiagaraScript::ConvertUsageToGroup(MasterUsage, UsageGroup))
	{
		int32 VarIdx = 0;
		for (int64 i = 0; i < (int64)ENiagaraScriptGroup::Max; i++)
		{
			if ((int64)UsageGroup == i)
			{
				for (int32 j = 0; j < OutputVars.Num(); j++)
				{
					UEdGraphPin* OutputPin = GetOutputPin(j);
					if (OutputPin == LocallyOwnedOutputPin)
					{
						VarIdx += j;
						break;
					}
				}
				break;
			}

			VarIdx += OutputVars.Num();
		}
		UEdGraphPin* InputPin = GetInputPin(VarIdx);
		if (InputPin)
		{
			return InputPin;
		}
	}
	return nullptr;
}

void UNiagaraNodeUsageSelector::AppendFunctionAliasForContext(const FNiagaraGraphFunctionAliasContext& InFunctionAliasContext, FString& InOutFunctionAlias)
{
	FString UsageString;
	switch (InFunctionAliasContext.CompileUsage)
	{
	case ENiagaraScriptUsage::SystemSpawnScript:
	case ENiagaraScriptUsage::SystemUpdateScript:
		UsageString = "System";
		break;
	case ENiagaraScriptUsage::EmitterSpawnScript:
	case ENiagaraScriptUsage::EmitterUpdateScript:
		UsageString = "Emitter";
		break;
	case ENiagaraScriptUsage::ParticleSpawnScript:
	case ENiagaraScriptUsage::ParticleUpdateScript:
	case ENiagaraScriptUsage::ParticleEventScript:
	case ENiagaraScriptUsage::ParticleGPUComputeScript:
		UsageString = "Particle";
		break;
	}

	if (UsageString.IsEmpty() == false)
	{
		InOutFunctionAlias += "_" + UsageString;
	}
}

void UNiagaraNodeUsageSelector::BuildParameterMapHistory(FNiagaraParameterMapHistoryBuilder& OutHistory, bool bRecursive)
{
	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());

	TArray<UEdGraphPin*> InputPins;
	GetInputPins(InputPins);
	TArray<UEdGraphPin*> OutputPins;
	GetOutputPins(OutputPins);

	ENiagaraScriptUsage BaseUsage = OutHistory.GetBaseUsageContext();
	ENiagaraScriptUsage CurrentUsage = OutHistory.GetCurrentUsageContext();	

	check(OutputPins.Num() - 1 == OutputVars.Num());

	ENiagaraScriptGroup UsageGroup = ENiagaraScriptGroup::Max;
	if (UNiagaraScript::ConvertUsageToGroup(CurrentUsage, UsageGroup))
	{
		if (bRecursive)
		{
			int32 VarIdx = 0;
			for (int64 i = 0; i < (int64)ENiagaraScriptGroup::Max; i++)
			{
				if ((int64)UsageGroup == i)
				{
					break;
				}

				VarIdx += OutputVars.Num();
			}

			for (int32 i = 0; i < OutputVars.Num(); i++)
			{
				OutHistory.VisitInputPin(InputPins[VarIdx + i], this);
			}
		}
	}
}

FGuid UNiagaraNodeUsageSelector::AddOutput(FNiagaraTypeDefinition Type, const FName& Name)
{
	FNiagaraVariable NewOutput(Type, Name);
	FGuid Guid = FGuid::NewGuid();
	OutputVars.Add(NewOutput);
	OutputVarGuids.Add(Guid);
	return Guid;
}

void UNiagaraNodeUsageSelector::OnPinRemoved(UEdGraphPin* PinToRemove)
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

void UNiagaraNodeUsageSelector::OnNewTypedPinAdded(UEdGraphPin* NewPin)
{
	Super::OnNewTypedPinAdded(NewPin);

	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	FNiagaraTypeDefinition OutputType = Schema->PinToTypeDefinition(NewPin);

	TSet<FName> OutputNames;
	for (const FNiagaraVariable& Output : OutputVars)
	{
		OutputNames.Add(Output.GetName());
	}
	FName OutputName = FNiagaraUtilities::GetUniqueName(*NewPin->GetName(), OutputNames);
	NewPin->PinName = OutputName;
	FGuid Guid = AddOutput(OutputType, OutputName);

	// Update the pin's data too so that it's connection is maintained after reallocating.
	NewPin->PersistentGuid = Guid;

	// We cannot just reallocate the pins here, because that invalidates all pins of this node (including
	// the NewPin parameter). If the calling method tries to access the provided new pin afterwards, it
	// runs into a nullptr error (e.g. when called by drag and drop).
	InsertInputPinsFor(OutputVars.Last());
}

void UNiagaraNodeUsageSelector::OnPinRenamed(UEdGraphPin* RenamedPin, const FString& OldName)
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

bool UNiagaraNodeUsageSelector::CanRenamePin(const UEdGraphPin* Pin) const
{
	return Super::CanRenamePin(Pin) && Pin->Direction == EGPD_Output;
}

bool UNiagaraNodeUsageSelector::CanRemovePin(const UEdGraphPin* Pin) const
{
	return Super::CanRemovePin(Pin) && Pin->Direction == EGPD_Output;
}


FText UNiagaraNodeUsageSelector::GetTooltipText() const
{
	return LOCTEXT("UsageSelectorDesc", "If the usage matches, then the traversal will follow that path.");
}

FText UNiagaraNodeUsageSelector::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UsageSelectorTitle", "Select by Use");
}

#undef LOCTEXT_NAMESPACE
