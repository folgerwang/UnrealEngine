// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraCustomVersion.h"
#include "SNiagaraGraphNodeCustomHlsl.h"
#include "EdGraphSchema_Niagara.h"
#include "ScopedTransaction.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraGraph.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeCustomHlsl"

UNiagaraNodeCustomHlsl::UNiagaraNodeCustomHlsl(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCanRenameNode = true;
	ScriptUsage = ENiagaraScriptUsage::Function;

	Signature.Name = TEXT("Custom Hlsl");
	FunctionDisplayName = Signature.Name.ToString();
}

TSharedPtr<SGraphNode> UNiagaraNodeCustomHlsl::CreateVisualWidget()
{
	return SNew(SNiagaraGraphNodeCustomHlsl, this);
}

void UNiagaraNodeCustomHlsl::OnRenameNode(const FString& NewName)
{
	Signature.Name = *NewName;
	FunctionDisplayName = NewName;
}

FText UNiagaraNodeCustomHlsl::GetHlslText() const
{
	return FText::FromString(CustomHlsl);
}

void UNiagaraNodeCustomHlsl::OnCustomHlslTextCommitted(const FText& InText, ETextCommit::Type InType)
{
	FString NewValue = InText.ToString();
	if (!NewValue.Equals(CustomHlsl, ESearchCase::CaseSensitive))
	{
		FScopedTransaction Transaction(LOCTEXT("CustomHlslCommit", "Edited Custom Hlsl"));
		Modify();
		CustomHlsl = NewValue;
		RefreshFromExternalChanges();			
		MarkNodeRequiresSynchronization(__FUNCTION__, true);
	}
}

FLinearColor UNiagaraNodeCustomHlsl::GetNodeTitleColor() const
{
	return UEdGraphSchema_Niagara::NodeTitleColor_CustomHlsl;
}

bool UNiagaraNodeCustomHlsl::GetTokens(TArray<FString>& OutTokens) const
{
	FString HlslData = *CustomHlsl;

	if (HlslData.Len() == 0)
	{
		return false;
	}
	
	FString InputVars = TEXT(";/*+-)(?:, \t\n");
	int32 LastValidIdx = INDEX_NONE;
	bool bComment = false;
	int32 TargetLength = HlslData.Len();
	for (int32 i = 0; i < TargetLength; )
	{
		int32 Index = INDEX_NONE;

		// Determine if we are a splitter character or a regular character.
		if (InputVars.FindChar(HlslData[i], Index) && Index != INDEX_NONE)
		{
			if (HlslData[i] == '/' && (i + 1 != TargetLength) && HlslData[i+1] == '/') // We are a comment, go to end of line.
			{
				int32 FoundEndIdx = HlslData.Find("\n", ESearchCase::IgnoreCase, ESearchDir::FromStart, i + 2);
				if (FoundEndIdx != INDEX_NONE)
				{
					// do nothing // FoundEndIdx = FoundEndIdx;
				}
				else
				{
					FoundEndIdx = TargetLength - 1;
				}

				OutTokens.Add(HlslData.Mid(i, FoundEndIdx - i));
				OutTokens.Add("\n");
				i = FoundEndIdx + 1;
			}
			else if (HlslData[i] == '/' && (i + 1 != TargetLength) && HlslData[i+1] == '*') // We are a multiline comment, go to end comment. Nested comments are not supported currently.
			{
				int32 FoundEndIdx = HlslData.Find("*/", ESearchCase::IgnoreCase, ESearchDir::FromStart, i + 2);
				if (FoundEndIdx != INDEX_NONE)
				{
					FoundEndIdx = FoundEndIdx + 1;
				}
				else
				{
					FoundEndIdx = TargetLength - 1;
				}

				OutTokens.Add(HlslData.Mid(i, FoundEndIdx - i));
				i = FoundEndIdx + 1;
			}
			else if (LastValidIdx != INDEX_NONE) // We encountered a non-splitter character in the past that hasn't been recorded yet but we are a splitter.
			{
				OutTokens.Add(HlslData.Mid(LastValidIdx, i - LastValidIdx));
				OutTokens.Add(FString(1, &HlslData[i]));
				i++;
			}
			else //if (LastValidIdx == INDEX_NONE) // We are a splitter with no known unrecorded tokens prior.
			{
				OutTokens.Add(FString(1, &HlslData[i]));
				i++;
			}

			LastValidIdx = INDEX_NONE;
		}
		else if (LastValidIdx == INDEX_NONE)
		{
			LastValidIdx = i; // Record that this is where we encountered the first non-splitter character that has yet to be recorded.
			i++;
		}		
		else
		{
			i++;
		}
	}

	// We may need to pull in the last chars from the end.
	if (LastValidIdx != INDEX_NONE)
	{
		OutTokens.Add(HlslData.Mid(LastValidIdx));
	}

	return true;
}


#if WITH_EDITOR

void UNiagaraNodeCustomHlsl::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraNodeCustomHlsl, CustomHlsl))
	{
		RefreshFromExternalChanges();
		GetNiagaraGraph()->NotifyGraphNeedsRecompile();
	}
}

void UNiagaraNodeCustomHlsl::InitAsCustomHlslDynamicInput(const FNiagaraTypeDefinition& OutputType)
{
	Modify();
	ReallocatePins();
	RequestNewTypedPin(EGPD_Input, FNiagaraTypeDefinition::GetParameterMapDef(), FName("Map"));
	RequestNewTypedPin(EGPD_Output, OutputType, FName("Output"));
	ScriptUsage = ENiagaraScriptUsage::DynamicInput;
}

#endif

/** Called when a new typed pin is added by the user. */
void UNiagaraNodeCustomHlsl::OnNewTypedPinAdded(UEdGraphPin* NewPin)
{
	UNiagaraNodeWithDynamicPins::OnNewTypedPinAdded(NewPin);
	RebuildSignatureFromPins();
}

/** Called when a pin is renamed. */
void UNiagaraNodeCustomHlsl::OnPinRenamed(UEdGraphPin* RenamedPin, const FString& OldPinName)
{
	UNiagaraNodeWithDynamicPins::OnPinRenamed(RenamedPin, OldPinName);
	RebuildSignatureFromPins();
}

/** Removes a pin from this node with a transaction. */
void UNiagaraNodeCustomHlsl::RemoveDynamicPin(UEdGraphPin* Pin)
{
	UNiagaraNodeWithDynamicPins::RemoveDynamicPin(Pin);
	RebuildSignatureFromPins();
}

void UNiagaraNodeCustomHlsl::MoveDynamicPin(UEdGraphPin* Pin, int32 DirectionToMove)
{
	UNiagaraNodeWithDynamicPins::MoveDynamicPin(Pin, DirectionToMove);
	RebuildSignatureFromPins();
}

void UNiagaraNodeCustomHlsl::BuildParameterMapHistory(FNiagaraParameterMapHistoryBuilder& OutHistory, bool bRecursive)
{
	Super::BuildParameterMapHistory(OutHistory, bRecursive);
	if (!IsNodeEnabled() && OutHistory.GetIgnoreDisabled())
	{
		RouteParameterMapAroundMe(OutHistory, bRecursive);
		return;
	}

	TArray<FString> Tokens;
	GetTokens(Tokens);

	TArray<UEdGraphPin*> InputPins;
	GetInputPins(InputPins);

	TArray<UEdGraphPin*> OutputPins;
	GetOutputPins(OutputPins);

	int32 ParamMapIdx = INDEX_NONE;
	TArray<FNiagaraVariable> LocalVars;
	// This only works currently if the input pins are in the same order as the signature pins.
	if (InputPins.Num() == Signature.Inputs.Num() + 1)// the add pin is extra
	{
		bool bHasParamMapInput = false;
		bool bHasParamMapOutput = false;
		for (int32 i = 0; i < InputPins.Num(); i++)
		{
			if (IsAddPin(InputPins[i]))
				continue;

			FNiagaraVariable Input = Signature.Inputs[i];
			if (Input.GetType() == FNiagaraTypeDefinition::GetParameterMapDef())
			{
				bHasParamMapInput = true;
				FString ReplaceSrc = Input.GetName().ToString() + TEXT(".");
				ReplaceExactMatchTokens(Tokens, ReplaceSrc, TEXT(""), false);
				if (InputPins[i]->LinkedTo.Num() != 0)
				{
					ParamMapIdx = OutHistory.TraceParameterMapOutputPin(InputPins[i]->LinkedTo[0]);
				}
			}
			else
			{
				LocalVars.Add(Input);
			}
		}

		for (int32 i = 0; i < OutputPins.Num(); i++)
		{
			if (IsAddPin(OutputPins[i]))
				continue;

			FNiagaraVariable Output = Signature.Outputs[i];
			if (Output.GetType() == FNiagaraTypeDefinition::GetParameterMapDef())
			{
				bHasParamMapOutput = true;
				FString ReplaceSrc = Output.GetName().ToString() + TEXT(".");
				ReplaceExactMatchTokens(Tokens, ReplaceSrc, TEXT(""), false);
			}
			else
			{
				LocalVars.Add(Output);
			}
		}

		TArray<FString> PossibleNamespaces;
		FNiagaraParameterMapHistory::GetValidNamespacesForReading(OutHistory.GetBaseUsageContext(), 0, PossibleNamespaces);

		if ((bHasParamMapOutput || bHasParamMapInput) && ParamMapIdx != INDEX_NONE)
		{
			for (int32 i = 0; i < Tokens.Num(); i++)
			{
				bool bFoundLocal = false;
				if (INDEX_NONE != FNiagaraVariable::SearchArrayForPartialNameMatch(LocalVars, *Tokens[i]))
				{
					bFoundLocal = true;
				}

				if (!bFoundLocal && Tokens[i].Contains(TEXT("."))) // Only check tokens with namespaces in them..
				{
					for (const FString& ValidNamespace : PossibleNamespaces)
					{
						// There is one possible path here, one where we're using the namespace as-is from the valid list.
						if (Tokens[i].StartsWith(ValidNamespace, ESearchCase::CaseSensitive))
						{
							bool bUsedDefault = false;
							OutHistory.HandleExternalVariableRead(ParamMapIdx, *Tokens[i]);
						}
					}
				}
			}
		}
	}
}

// Replace items in the tokens array if they start with the src string or optionally src string and a namespace delimiter
void UNiagaraNodeCustomHlsl::ReplaceExactMatchTokens(TArray<FString>& Tokens, const FString& SrcString, const FString& ReplaceString, bool bAllowNamespaceSeparation)
{
	for (int32 i = 0; i < Tokens.Num(); i++)
	{
		if (Tokens[i].StartsWith(SrcString, ESearchCase::CaseSensitive))
		{
			if (Tokens[i].Len() > SrcString.Len() && Tokens[i][SrcString.Len()] == '.' && bAllowNamespaceSeparation)
			{
				Tokens[i] = ReplaceString + Tokens[i].Mid(SrcString.Len());
			}
			else if (Tokens[i].Len() == SrcString.Len())
			{
				Tokens[i] = ReplaceString;
			}
		}
	}
}


void UNiagaraNodeCustomHlsl::RebuildSignatureFromPins()
{
	Modify();
	FNiagaraFunctionSignature Sig = Signature;
	Sig.Inputs.Empty();
	Sig.Outputs.Empty();

	TArray<UEdGraphPin*> InputPins;
	TArray<UEdGraphPin*> OutputPins;
	GetInputPins(InputPins);
	GetOutputPins(OutputPins);

	const UEdGraphSchema_Niagara* Schema = Cast<UEdGraphSchema_Niagara>(GetSchema());

	for (UEdGraphPin* Pin : InputPins)
	{
		if (IsAddPin(Pin))
		{
			continue;
		}
		Sig.Inputs.Add(Schema->PinToNiagaraVariable(Pin, true));
	}

	for (UEdGraphPin* Pin : OutputPins)
	{
		if (IsAddPin(Pin))
		{
			continue;
		}
		Sig.Outputs.Add(Schema->PinToNiagaraVariable(Pin, false));
	}

	Signature = Sig;

	RefreshFromExternalChanges();
	MarkNodeRequiresSynchronization(__FUNCTION__, true);
}

#undef LOCTEXT_NAMESPACE

