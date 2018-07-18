// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "NiagaraCommon.h"
#include "NiagaraScriptSourceBase.generated.h"

struct EditorExposedVectorConstant
{
	FName ConstName;
	FVector4 Value;
};

struct EditorExposedVectorCurveConstant
{
	FName ConstName;
	class UCurveVector *Value;
};

/** External reference to the compile request data generated.*/
class FNiagaraCompileRequestDataBase
{
public:
	virtual ~FNiagaraCompileRequestDataBase() {}
	virtual bool GatherPreCompiledVariables(const FString& InNamespaceFilter, TArray<FNiagaraVariable>& OutVars) = 0;
	virtual void GetReferencedObjects(TArray<UObject*>& Objects) = 0;
	virtual const TMap<FName, UNiagaraDataInterface*>& GetObjectNameMap() = 0;
	virtual int32 GetDependentRequestCount() const = 0;
	virtual TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> GetDependentRequest(int32 Index) = 0;
	virtual FName ResolveEmitterAlias(FName VariableName) const = 0;
};

class FNiagaraCompileOptions
{
public:
	FNiagaraCompileOptions() : TargetUsage(ENiagaraScriptUsage::Function), TargetUsageBitmask(0)
	{
	}

	FNiagaraCompileOptions(ENiagaraScriptUsage InTargetUsage, FGuid InTargetUsageId, int32 InTargetUsageBitmask,  const FString& InPathName, const FString& InFullName, const FString& InName)
		: TargetUsage(InTargetUsage), TargetUsageId(InTargetUsageId), PathName(InPathName), FullName(InFullName), Name(InName), TargetUsageBitmask(InTargetUsageBitmask)
	{
	}

	const FString& GetFullName() const { return FullName; }
	const FString& GetName() const { return Name; }
	const FString& GetPathName() const { return PathName; }
	int32 GetTargetUsageBitmask() const { return TargetUsageBitmask; }

	ENiagaraScriptUsage TargetUsage;
	FGuid TargetUsageId;
	FString PathName;
	FString FullName;
	FString Name;
	int32 TargetUsageBitmask;
	TArray<FString> AdditionalDefines;
};

struct FNiagaraParameterStore;

/** Runtime data for a Niagara system */
UCLASS(MinimalAPI)
class UNiagaraScriptSourceBase : public UObject
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE(FOnChanged);
#endif

	TArray<TSharedPtr<EditorExposedVectorConstant> > ExposedVectorConstants;
	TArray<TSharedPtr<EditorExposedVectorCurveConstant> > ExposedVectorCurveConstants;

	/** Determines if the input change id is equal to the current source graph's change id.*/
	virtual bool IsSynchronized(const FGuid& InChangeId) { return true; }

	virtual UNiagaraScriptSourceBase* MakeRecursiveDeepCopy(UObject* DestOuter, TMap<const UObject*, UObject*>& ExistingConversions) const { return nullptr; }

	/** Determine if there are any external dependencies wrt to scripts and ensure that those dependencies are sucked into the existing package.*/
	virtual void SubsumeExternalDependencies(TMap<const UObject*, UObject*>& ExistingConversions) {}

	/** Enforce that the source graph is now out of sync with the script.*/
	virtual void MarkNotSynchronized(FString Reason) {}

	virtual FGuid GetChangeID() { return FGuid(); };

	virtual void ComputeVMCompilationId(struct FNiagaraVMExecutableDataId& Id, ENiagaraScriptUsage InUsage, const FGuid& InUsageId) const {};
	
	/** Cause the source to build up any internal variables that will be useful in the compilation process.*/
	virtual TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> PreCompile(UNiagaraEmitter* Emitter, const TArray<FNiagaraVariable>& EncounterableVariables, TArray<TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe>>& ReferencedCompileRequests, bool bClearErrors = true) { return nullptr; }
	
	/** 
	 * Allows the derived editor only script source to handle a post load requested by an owning emitter. 
	 * @param OwningEmitter The emitter requesting the post load.
	 */
	virtual void PostLoadFromEmitter(UNiagaraEmitter& OwningEmitter) { }

	/** Adds a module if it isn't already in the graph. If the module isn't found bOutFoundModule will be false. If it is found and it did need to be added, the function returns true. If it already exists, it returns false. */
	NIAGARA_API virtual bool AddModuleIfMissing(FString ModulePath, ENiagaraScriptUsage Usage, bool& bOutFoundModule) { bOutFoundModule = false; return false; }

#if WITH_EDITOR
	virtual void CleanUpOldAndInitializeNewRapidIterationParameters(FString UniqueEmitterName, ENiagaraScriptUsage ScriptUsage, FGuid ScriptUsageId, FNiagaraParameterStore& RapidIterationParameters) const { checkf(false, TEXT("Not implemented")); }

	FOnChanged& OnChanged() { return OnChangedDelegate; }

	virtual void InvalidateCachedCompileIds() {}

protected:
	FOnChanged OnChangedDelegate;
#endif
};
