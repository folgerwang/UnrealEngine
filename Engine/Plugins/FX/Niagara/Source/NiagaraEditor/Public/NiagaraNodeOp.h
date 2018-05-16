// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "NiagaraEditorCommon.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SOverlay.h"
#include "NiagaraNode.h"
#include "NiagaraNodeOp.generated.h"

class SGraphNode;
class SGraphPin;
class SVerticalBox;

UCLASS(MinimalAPI)
class UNiagaraNodeOp : public UNiagaraNode
{
	GENERATED_UCLASS_BODY()

public:

	/** Name of operation */
	UPROPERTY()
	FName OpName;

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
};



