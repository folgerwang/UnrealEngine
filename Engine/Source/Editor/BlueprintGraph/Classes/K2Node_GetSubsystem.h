// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "K2Node.h"
#include "K2Node_GetSubsystem.generated.h"

class USubsystem;
class FKismetCompilerContext;

UCLASS()
class UK2Node_GetSubsystem : public UK2Node
{
	GENERATED_BODY()
public:

	virtual void Serialize(FArchive& Ar) override;

	void Initialize(UClass* NodeClass);
	//~Begin UEdGraphNode interface.
	virtual void AllocateDefaultPins() override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

	virtual FText GetTooltipText() const override;
	//~End UEdGraphNode interface.

	virtual bool IsNodePure() const { return true; }
	virtual bool IsNodeSafeToIgnore() const override { return true; }
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void GetNodeAttributes(TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes) const override;
	virtual FText GetMenuCategory() const override;

	/** Get the blueprint input pin */
	UEdGraphPin* GetClassPin(const TArray< UEdGraphPin* >* InPinsToSearch = nullptr) const;
	/** Get the world context input pin, can return NULL */
	UEdGraphPin* GetWorldContextPin() const;
	/** Get the result output pin */
	UEdGraphPin* GetResultPin() const;

	virtual bool ShouldDrawCompact() const override { return true; }

protected:
	UPROPERTY()
	TSubclassOf<USubsystem> CustomClass;
};

UCLASS()
class UK2Node_GetSubsystemFromPC : public UK2Node_GetSubsystem
{
	GENERATED_BODY()
public:

	//~Begin UEdGraphNode interface.
	void AllocateDefaultPins();
	void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph);
	FText GetTooltipText() const;
	//~End UEdGraphNode interface.

	void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const;
	FText GetMenuCategory() const;

	/** Get the world context input pin, can return NULL */
	UEdGraphPin* GetPlayerControllerPin() const;
	
};