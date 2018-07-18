// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Animation/ControlRigInterface.h"
#include "Engine/EngineBaseTypes.h"
#include "Templates/SubclassOf.h"
#include "ControlRigDefines.h"
#include "Hierarchy.h"
#include "Units/RigUnitContext.h"
#include "Animation/NodeMappingProviderInterface.h"
#include "Units/RigUnit.h"
#include "Units/RigUnit_Control.h"
#include "ControlRig.generated.h"

class IControlRigObjectBinding;
struct FRigUnit;
class UControlRig;

/** Delegate used to optionally gather inputs before evaluating a ControlRig */
DECLARE_DELEGATE_OneParam(FPreEvaluateGatherInput, UControlRig*);
DECLARE_DELEGATE_OneParam(FPostEvaluateQueryOutput, UControlRig*);

/** Runs logic for mapping input data to transforms (the "Rig") */
UCLASS(Blueprintable, Abstract, editinlinenew)
class CONTROLRIG_API UControlRig : public UObject, public IControlRigInterface, public INodeMappingProviderInterface
{
	GENERATED_BODY()

	friend class UControlRigComponent;

public:
	static const FName InputMetaName;
	static const FName OutputMetaName;
	static const FName AbstractMetaName;
	static const FName DisplayNameMetaName;
	static const FName ShowVariableNameInTitleMetaName;

private:
	/** Current delta time */
	float DeltaTime;

public:
	UControlRig();

	/** Get the current delta time */
	UFUNCTION(BlueprintPure, Category = "Animation")
	float GetDeltaTime() const;

	/** Set the current delta time */
	void SetDeltaTime(float InDeltaTime);

#if WITH_EDITOR
	/** Get the category of this ControlRig (for display in menus) */
	virtual FText GetCategory() const;

	/** Get the tooltip text to display for this node (displayed in graphs and from context menus) */
	virtual FText GetTooltipText() const;
#endif

	/** UObject interface */
	virtual UWorld* GetWorld() const override;

	/** Initialize things for the ControlRig */
	virtual void Initialize();

	/** IControlRigInterface implementation */
	virtual void PreEvaluate_GameThread() override;
	virtual void Evaluate_AnyThread() override;
	virtual void PostEvaluate_GameThread() override;

	/** Setup bindings to a runtime object (or clear by passing in nullptr). */
	void SetObjectBinding(TSharedPtr<IControlRigObjectBinding> InObjectBinding)
	{
		ObjectBinding = InObjectBinding;
	}

	/** Get bindings to a runtime object */
	TSharedPtr<IControlRigObjectBinding> GetObjectBinding() const
	{
		return ObjectBinding;
	}

	/** Evaluate another animation ControlRig */
	UFUNCTION(BlueprintPure, Category = "Hierarchy")
	FTransform GetGlobalTransform(const FName JointName) const;

	/** Evaluate another animation ControlRig */
	UFUNCTION(BlueprintPure, Category = "Hierarchy")
	void SetGlobalTransform(const FName JointName, const FTransform& InTransform) ;

	/** Returns base hierarchy */
	const FRigHierarchy& GetBaseHierarchy() const
	{
		return Hierarchy.BaseHierarchy;
	}

	void SetPreEvaluateGatherInputDelegate(const FPreEvaluateGatherInput& Delegate)
	{
		OnPreEvaluateGatherInput = Delegate;
	}

	void ClearPreEvaluateGatherInputDelegate()
	{
		OnPreEvaluateGatherInput.Unbind();
	}

	void SetPostEvaluateQueryOutputDelegate(const FPostEvaluateQueryOutput& Delegate)
	{
		OnPostEvaluateQueryOutput = Delegate;
	}

	void ClearPostEvaluateQueryOutputDelegate()
	{
		OnPostEvaluateQueryOutput.Unbind();
	}

#if WITH_EDITOR
	// get class name of rig unit that is owned by this rig
	FName GetRigClassNameFromRigUnit(const FRigUnit* InRigUnit) const;
	FRigUnit_Control* GetControlRigUnitFromName(const FName& PropertyName);
	FRigUnit* GetRigUnitFromName(const FName& PropertyName);
	
	// called after post reinstance when compilng blueprint by Sequencer
	void PostReinstanceCallback(const UControlRig* Old);


#endif // WITH_EDITOR
	// BEGIN UObject interface
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// END UObject interface

#if WITH_EDITORONLY_DATA
	// only editor feature that stops execution
	// whether we're executing the graph or not
	bool bExecutionOn;
#endif // #if WITH_EDITORONLY_DATA

	UPROPERTY(transient)
	ERigExecutionType ExecutionType;

private:
	UPROPERTY(VisibleDefaultsOnly, Category = "Hierarchy")
	FRigHierarchyContainer Hierarchy;

#if WITH_EDITORONLY_DATA
	/** The properties of source accessible <target, source local path> when source -> target
	 * For example, if you have property RigUnitA.B->RigUnitB.C, this will save as <RigUnitB.C, RigUnitA.B> */
	UPROPERTY()
	TMap<FName, FString> AllowSourceAccessProperties;

	/** Cached editor object reference by rig unit */
	TMap<FRigUnit*, UObject*> RigUnitEditorObjects;
#endif // WITH_EDITOR

	/** list of operators. */
	UPROPERTY()
	TArray<FControlRigOperator> Operators;

	/** Execution form from Operators. Used for Execute function */
	TArray<FRigExecutor> Executors;

	/** Runtime object binding */
	TSharedPtr<IControlRigObjectBinding> ObjectBinding;

	FPreEvaluateGatherInput OnPreEvaluateGatherInput;
	FPostEvaluateQueryOutput OnPostEvaluateQueryOutput;

	/** Instantiate Executor from Operators */
	void InstantiateExecutor();

	/** Execute the rig unit */
	void Execute(const EControlRigState State);

	/** INodeMappingInterface implementation */
	virtual void GetMappableNodeData(TArray<FName>& OutNames, TArray<FNodeItem>& OutNodeItems) const override;

	friend class FControlRigBlueprintCompilerContext;
	friend struct FRigHierarchyRef;
	friend class FControlRigDetails;
	friend class UControlRigEditorLibrary;
	friend class URigUnitEditor_Base;
	friend class FControlRigEditor;
	friend class SRigHierarchy;
};