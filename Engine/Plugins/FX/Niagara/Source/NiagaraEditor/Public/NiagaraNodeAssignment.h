// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraEditorCommon.h"
#include "NiagaraNode.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeAssignment.generated.h"

class UNiagaraScript;

UCLASS()
class NIAGARAEDITOR_API UNiagaraNodeAssignment : public UNiagaraNodeFunctionCall
{
	GENERATED_BODY()

public:
	int32 NumTargets() const { return AssignmentTargets.Num(); }

	int32 FindAssignmentTarget(const FName& InName);
	int32 FindAssignmentTarget(const FName& InName, const FNiagaraTypeDefinition& TypeDef);

	/** Set the assignment target and default value. This does not call RefreshFromExternalChanges, which you will need to do to update the internal graph. Returns true if values were changed.*/
	bool SetAssignmentTarget(int32 Idx, const FNiagaraVariable& InVar, const FString* InDefaultValue = nullptr);
	int32 AddAssignmentTarget(const FNiagaraVariable& InVar, const FString* InDefaultValue = nullptr);

	FName GetAssignmentTargetName(int32 Idx) const { return AssignmentTargets[Idx].GetName(); }
	bool SetAssignmentTargetName(int32 Idx, const FName& InName);
	const FNiagaraVariable& GetAssignmentTarget(int32 Idx) const { return AssignmentTargets[Idx]; }

	const TArray<FNiagaraVariable>& GetAssignmentTargets() const { return AssignmentTargets; }
	const TArray<FString>& GetAssignmentDefaults() const { return AssignmentDefaultValues; }

	void MergeUp();
	void BuildParameterMenu(FMenuBuilder& MenuBuilder, ENiagaraScriptUsage InUsage, UNiagaraNodeOutput* InGraphOutputNode);

	//~ Begin EdGraphNode Interface
	virtual void PostLoad() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void AllocateDefaultPins() override;

	//~ UNiagaraNodeFunctionCall interface
	virtual bool RefreshFromExternalChanges() override;
	virtual void BuildParameterMapHistory(FNiagaraParameterMapHistoryBuilder& OutHistory, bool bRecursive = true) override;

	void AddParameter(FNiagaraVariable InVar, FString InDefaultValue);
	void RemoveParameter(const FNiagaraVariable& InVar);

	void UpdateUsageBitmaskFromOwningScript();

protected:

	UPROPERTY()
	FNiagaraVariable AssignmentTarget_DEPRECATED;

	UPROPERTY()
	FString AssignmentDefaultValue_DEPRECATED;

	UPROPERTY(EditAnywhere, Category = Assignment)
	TArray<FNiagaraVariable> AssignmentTargets;

	UPROPERTY(EditAnywhere, Category = Assignment)
	TArray<FString> AssignmentDefaultValues;

	UPROPERTY()
	FString OldFunctionCallName;

private:
	void GenerateScript();

	void InitializeScript(UNiagaraScript* NewScript);

	int32 CalculateScriptUsageBitmask();
};

