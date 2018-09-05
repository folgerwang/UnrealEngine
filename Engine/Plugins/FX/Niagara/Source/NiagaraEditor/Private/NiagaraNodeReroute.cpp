// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeReroute.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraHlslTranslator.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeReroute"

const char* PC_Wildcard_Niagara = "wildcard";

UNiagaraNodeReroute::UNiagaraNodeReroute(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNiagaraNodeReroute::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// @TODO why do we need to have this post-change property here at all? 
	// Doing a null check b/c otherwise if doing a Duplicate via Ctrl-W, we die inside AllocateDefaultPins due to 
	// the point where we get this call not being completely formed.
	if (PropertyChangedEvent.Property != nullptr)
	{
		ReallocatePins();
	}
}

void UNiagaraNodeReroute::PostLoad()
{
	Super::PostLoad();
}

void UNiagaraNodeReroute::AllocateDefaultPins()
{
	const FName InputPinName(TEXT("InputPin"));
	UEdGraphPin* MyInputPin = CreatePin(EGPD_Input, PC_Wildcard_Niagara, InputPinName);
	MyInputPin->bDefaultValueIsIgnored = true;

	const FName OutputPinName(TEXT("OutputPin"));
	CreatePin(EGPD_Output, PC_Wildcard_Niagara, OutputPinName);
}

FText UNiagaraNodeReroute::GetTooltipText() const
{
	return FText::GetEmpty();
}

FText UNiagaraNodeReroute::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::EditableTitle)
	{
		return FText::FromString(NodeComment);
	}

	return LOCTEXT("RerouteNodeTitle", "Reroute Node");
}

bool UNiagaraNodeReroute::ShouldOverridePinNames() const
{
	return true;
}

FText UNiagaraNodeReroute::GetPinNameOverride(const UEdGraphPin& Pin) const
{
	// Keep the pin size tiny
	return FText::GetEmpty();
}

void UNiagaraNodeReroute::OnRenameNode(const FString& NewName)
{
	NodeComment = NewName;
}


bool UNiagaraNodeReroute::CanSplitPin(const UEdGraphPin* Pin) const
{
	return false;
}

UEdGraphPin* UNiagaraNodeReroute::GetPassThroughPin(const UEdGraphPin* FromPin) const
{
	if (FromPin && Pins.Contains(FromPin))
	{
		return FromPin == Pins[0] ? Pins[1] : Pins[0];
	}

	return nullptr;
}

bool UNiagaraNodeReroute::ShouldDrawNodeAsControlPointOnly(int32& OutInputPinIndex, int32& OutOutputPinIndex) const
{
	OutInputPinIndex = 0;
	OutOutputPinIndex = 1;
	return true;
}

void UNiagaraNodeReroute::Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs)
{
	Super::Compile(Translator, Outputs);
}

bool UNiagaraNodeReroute::RefreshFromExternalChanges()
{
	ReallocatePins();
	PropagatePinType();
	return true;
}

void UNiagaraNodeReroute::PinConnectionListChanged(UEdGraphPin* Pin)
{
	PropagatePinType();
	Super::PinConnectionListChanged(Pin);
}

void UNiagaraNodeReroute::BuildParameterMapHistory(FNiagaraParameterMapHistoryBuilder& OutHistory, bool bRecursive)
{
	// Should never hit here...
	check(false);
}

/** Traces one of this node's output pins to its source output pin if it is a reroute node output pin.*/
UEdGraphPin* UNiagaraNodeReroute::GetTracedOutputPin(UEdGraphPin* LocallyOwnedOutputPin) const
{
	check(Pins.Contains(LocallyOwnedOutputPin) && LocallyOwnedOutputPin->Direction == EGPD_Output);
	UEdGraphPin* InputPin = GetInputPin(0);
	if (InputPin && InputPin->LinkedTo.Num() == 1 && InputPin->LinkedTo[0] != nullptr)
	{
		UEdGraphPin* LinkedPin = InputPin->LinkedTo[0];
		UNiagaraNode* LinkedNode = CastChecked<UNiagaraNode>(LinkedPin->GetOwningNode());
		return LinkedNode->GetTracedOutputPin(LinkedPin);
	}
	return nullptr;
}

void UNiagaraNodeReroute::PropagatePinType()
{
	UEdGraphPin* MyInputPin = GetInputPin(0);
	UEdGraphPin* MyOutputPin = GetOutputPin(0);

	for (UEdGraphPin* Inputs : MyInputPin->LinkedTo)
	{
		if (Inputs->PinType.PinCategory != PC_Wildcard_Niagara)
		{
			PropagatePinTypeFromDirection(true);
			return;
		}
	}

	for (UEdGraphPin* Outputs : MyOutputPin->LinkedTo)
	{
		if (Outputs->PinType.PinCategory != PC_Wildcard_Niagara)
		{
			PropagatePinTypeFromDirection(false);
			return;
		}
	}

	// if all inputs/outputs are wildcards, still favor the inputs first (propagate array/reference/etc. state)
	if (MyInputPin->LinkedTo.Num() > 0)
	{
		// If we can't mirror from output type, we should at least get the type information from the input connection chain
		PropagatePinTypeFromDirection(true);
	}
	else if (MyOutputPin->LinkedTo.Num() > 0)
	{
		// Try to mirror from output first to make sure we get appropriate member references
		PropagatePinTypeFromDirection(false);
	}
	else
	{
		// Revert to wildcard
		MyInputPin->BreakAllPinLinks();
		MyInputPin->PinType.ResetToDefaults();
		MyInputPin->PinType.PinCategory = PC_Wildcard_Niagara;

		MyOutputPin->BreakAllPinLinks();
		MyOutputPin->PinType.ResetToDefaults();
		MyOutputPin->PinType.PinCategory = PC_Wildcard_Niagara;
	}
}

void UNiagaraNodeReroute::PropagatePinTypeFromDirection(bool bFromInput)
{
	if (bRecursionGuard)
	{
		return;
	}
	// Set the type of the pin based on the source connection, and then percolate
	// that type information up until we no longer reach another Reroute node
	UEdGraphPin* MySourcePin = bFromInput ? GetInputPin(0) : GetOutputPin(0);
	UEdGraphPin* MyDestinationPin = bFromInput ? GetOutputPin(0) : GetInputPin(0);

	TGuardValue<bool> RecursionGuard(bRecursionGuard, true);

	// Make sure any source knot pins compute their type, this will try to call back
	// into this function but the recursion guard will stop it
	for (UEdGraphPin* InPin : MySourcePin->LinkedTo)
	{
		if (UNiagaraNodeReroute* KnotNode = Cast<UNiagaraNodeReroute>(InPin->GetOwningNode()))
		{
			KnotNode->PropagatePinTypeFromDirection(bFromInput);
		}
	}

	UEdGraphPin* TypeSource = MySourcePin->LinkedTo.Num() ? MySourcePin->LinkedTo[0] : nullptr;
	if (TypeSource)
	{
		MySourcePin->PinType = TypeSource->PinType;
		MyDestinationPin->PinType = TypeSource->PinType;

		for (UEdGraphPin* LinkPin : MyDestinationPin->LinkedTo)
		{
			if (UNiagaraNodeReroute* OwningNode = Cast<UNiagaraNodeReroute>(LinkPin->GetOwningNode()))
			{
				// Notify any pins in the destination direction
				if (UNiagaraNodeReroute* RerouteNode = Cast<UNiagaraNodeReroute>(OwningNode))
				{
					RerouteNode->PropagatePinTypeFromDirection(bFromInput);
				}
				else
				{
					OwningNode->PinConnectionListChanged(LinkPin);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
