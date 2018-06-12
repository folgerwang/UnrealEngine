// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraNodeWithDynamicPins.h"
#include "SGraphPin.h"
#include "NiagaraNodeParameterMapBase.generated.h"

class UEdGraphPin;


/** A node which allows the user to build a set of arbitrary output types from an arbitrary set of input types by connecting their inner components. */
UCLASS()
class UNiagaraNodeParameterMapBase : public UNiagaraNodeWithDynamicPins
{
public:
	GENERATED_BODY()
	UNiagaraNodeParameterMapBase();

	/** Traverse the graph looking for the history of the parameter map specified by the input pin. This will return the list of variables discovered, any per-variable warnings (type mismatches, etc)
		encountered per variable, and an array of pins encountered in order of traversal outward from the input pin.
	*/
	static TArray<FNiagaraParameterMapHistory> GetParameterMaps(UNiagaraNodeOutput* InGraphEnd, bool bLimitToOutputScriptType = false, FString EmitterNameOverride = TEXT(""), const TArray<FNiagaraVariable>& EncounterableVariables = TArray<FNiagaraVariable>());
	static TArray<FNiagaraParameterMapHistory> GetParameterMaps(UNiagaraGraph* InGraph, FString EmitterNameOverride = TEXT(""), const TArray<FNiagaraVariable>& EncounterableVariables = TArray<FNiagaraVariable>());
	static TArray<FNiagaraParameterMapHistory> GetParameterMaps(class UNiagaraScriptSourceBase* InSource, FString EmitterNameOverride = TEXT(""), const TArray<FNiagaraVariable>& EncounterableVariables = TArray<FNiagaraVariable>());

	virtual bool AllowNiagaraTypeForAddPin(const FNiagaraTypeDefinition& InType) override;
	
	/** Gets the description text for a pin. */
	FText GetPinDescriptionText(UEdGraphPin* Pin) const;

	/** Called when a pin's description text is committed. */
	void PinDescriptionTextCommitted(const FText& Text, ETextCommit::Type CommitType, UEdGraphPin* Pin);

	virtual void CollectAddPinActions(FGraphActionListBuilderBase& OutActions, bool& bOutCreateRemainingActions, UEdGraphPin* Pin) override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;

protected:
	virtual void OnPinRenamed(UEdGraphPin* RenamedPin, const FString& OldName) override;

	UEdGraphPin* PinPendingRename;

public:
	/** The sub category for parameter pins. */
	static const FName ParameterPinSubCategory;

	static const FName SourcePinName;
	static const FName DestPinName;
};
