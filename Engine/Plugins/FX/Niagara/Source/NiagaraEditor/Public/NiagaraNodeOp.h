// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "NiagaraEditorCommon.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SOverlay.h"
#include "NiagaraNode.h"
#include "NiagaraNodeWithDynamicPins.h"
#include "NiagaraNodeOp.generated.h"

class SGraphNode;
class SGraphPin;
class SVerticalBox;

USTRUCT()
struct FAddedPinData
{
	GENERATED_BODY()
		
	/** The data type of the added pin */
	UPROPERTY()
	FEdGraphPinType PinType;

	/** The name type of the added pin */
	UPROPERTY()
	FName PinName;
};

UCLASS(MinimalAPI)
class UNiagaraNodeOp : public UNiagaraNodeWithDynamicPins
{
	GENERATED_UCLASS_BODY()

public:

	/** Name of operation */
	UPROPERTY()
	FName OpName;

	UPROPERTY()
	TArray<FAddedPinData> AddedPins;

	//~ Begin UObject interface
	virtual void PostLoad() override;
	//~ End UObject interface

	//~ Begin EdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	//~ End EdGraphNode Interface

	//~ Begin UNiagaraNode Interface
	virtual void Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs) override;
	virtual bool RefreshFromExternalChanges() override;
	virtual ENiagaraNumericOutputTypeSelectionMode GetNumericOutputTypeSelectionMode() const override;
	//~ End UNiagaraNode Interface

	//~ Begin UNiagaraNodeWithDynamicPins Interface
	virtual bool AllowNiagaraTypeForAddPin(const FNiagaraTypeDefinition& InType) override;
	//~ End UNiagaraNodeWithDynamicPins Interface

protected:
	//~ Begin EdGraphNode Interface
	virtual void OnPinRemoved(UEdGraphPin* PinToRemove) override;
	//~ End EdGraphNode Interface

	//~ Begin UNiagaraNodeWithDynamicPins Interface
	virtual bool AllowDynamicPins() const override;
	virtual bool CanMovePin(const UEdGraphPin* Pin) const override { return false; }
	virtual void OnNewTypedPinAdded(UEdGraphPin* NewPin) override;
	virtual void OnPinRenamed(UEdGraphPin* RenamedPin, const FString& OldName) override;
	virtual bool CanRemovePin(const UEdGraphPin* Pin) const override;
	//~ End UNiagaraNodeWithDynamicPins Interface
	
private:
	FName GetUniqueAdditionalPinName() const;
};



