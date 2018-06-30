// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "NiagaraCommon.h"
#include "NiagaraDataSet.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraSystem.generated.h"

#if WITH_EDITORONLY_DATA
class UNiagaraEditorDataBase;
#endif

USTRUCT()
struct FNiagaraEmitterSpawnAttributes
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FName> SpawnAttributes;
};



USTRUCT()
struct FEmitterCompiledScriptPair
{
	GENERATED_USTRUCT_BODY()
	
	bool bResultsReady;
	UNiagaraEmitter* Emitter;
	UNiagaraScript* CompiledScript;
	uint32 PendingDDCID;
	FNiagaraVMExecutableDataId CompileId;
	TSharedPtr<FNiagaraVMExecutableData> CompileResults;
};

USTRUCT()
struct FNiagaraSystemCompileRequest
{
	GENERATED_USTRUCT_BODY()
		
	double StartTime;

	UPROPERTY()
	TArray<UObject*> RootObjects;

	TArray<FEmitterCompiledScriptPair> EmitterCompiledScriptPairs;
	
	TMap<UNiagaraScript*, TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> > MappedData;
};

/** Container for multiple emitters that combine together to create a particle system effect.*/
UCLASS(BlueprintType)
class NIAGARA_API UNiagaraSystem : public UObject
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSystemCompiled, UNiagaraSystem*);
#endif

	//~ UObject interface
	void PostInitProperties();
	void Serialize(FArchive& Ar)override;
	virtual void PostLoad() override; 
	virtual void BeginDestroy() override;
	virtual void PreSave(const class ITargetPlatform * TargetPlatform) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override; 
	virtual void BeginCacheForCookedPlatformData(const ITargetPlatform *TargetPlatform) override;
#endif

	/** Gets an array of the emitter handles. */
	const TArray<FNiagaraEmitterHandle>& GetEmitterHandles();
	const TArray<FNiagaraEmitterHandle>& GetEmitterHandles()const;

	/** Returns true if this system is valid and can be instanced. False otherwise. */
	bool IsValid()const;

#if WITH_EDITORONLY_DATA
	/** Adds a new emitter handle to this System.  The new handle exposes an Instance value which is a copy of the
		original asset. */
	FNiagaraEmitterHandle AddEmitterHandle(UNiagaraEmitter& SourceEmitter, FName EmitterName);

	/** Adds a new emitter handle to this System.  The new handle will not copy the emitter and any changes made to it's
		Instance value will modify the original asset.  This should only be used in the emitter toolkit for simulation
		purposes. */
	FNiagaraEmitterHandle AddEmitterHandleWithoutCopying(UNiagaraEmitter& Emitter);

	/** Duplicates an existing emitter handle and adds it to the System.  The new handle will reference the same source asset,
		but will have a copy of the duplicated Instance value. */
	FNiagaraEmitterHandle DuplicateEmitterHandle(const FNiagaraEmitterHandle& EmitterHandleToDuplicate, FName EmitterName);

	/** Removes the provided emitter handle. */
	void RemoveEmitterHandle(const FNiagaraEmitterHandle& EmitterHandleToDelete);

	/** Removes the emitter handles which have an Id in the supplied set. */
	void RemoveEmitterHandlesById(const TSet<FGuid>& HandlesToRemove);
#endif


	FNiagaraEmitterHandle& GetEmitterHandle(int Idx)
	{
		check(Idx < EmitterHandles.Num());
		return EmitterHandles[Idx];
	};

	const FNiagaraEmitterHandle& GetEmitterHandle(int Idx) const
	{
		check(Idx < EmitterHandles.Num());
		return EmitterHandles[Idx];
	};

	int GetNumEmitters()
	{
		return EmitterHandles.Num();
	}

	/** From the last compile, what are the variables that were exported out of the system for external use?*/
	const FNiagaraParameterStore& GetExposedParameters() const {	return ExposedParameters; }
	FNiagaraParameterStore& GetExposedParameters()  { return ExposedParameters; }


	/** Gets the System script which is used to populate the System parameters and parameter bindings. */
	UNiagaraScript* GetSystemSpawnScript();
	UNiagaraScript* GetSystemUpdateScript();

	bool IsReadyToRun() const;

	/** Are there any pending compile requests?*/
	bool HasOutstandingCompilationRequests() const;

	/** Returns whether this system has to be run in solo or not. */
	bool IsSolo()const;

	FORCEINLINE bool NeedsWarmup()const { return WarmupTickCount > 0 && WarmupTickDelta > SMALL_NUMBER; }
	FORCEINLINE float GetWarmupTime()const { return WarmupTime; }
	FORCEINLINE int32 GetWarmupTickCount()const { return WarmupTickCount; }
	FORCEINLINE float GetWarmupTickDelta()const { return WarmupTickDelta; }

#if WITH_EDITORONLY_DATA
	/** Called to query whether or not this emitter is referenced as the source to any emitter handles for this System.*/
	bool ReferencesSourceEmitter(UNiagaraEmitter& Emitter);

	/** Determines if this system has the supplied emitter as an editable and simulating emitter instance. */
	bool ReferencesInstanceEmitter(UNiagaraEmitter& Emitter);

	/** Updates all handles which use this emitter as their source. */
	void UpdateFromEmitterChanges(UNiagaraEmitter& ChangedSourceEmitter);

	/** Updates the system's rapid iteration parameters from a specific emitter. */
	void RefreshSystemParametersFromEmitter(const FNiagaraEmitterHandle& EmitterHandle);

	/** Removes the system's rapid iteration parameters for a specific emitter. */
	void RemoveSystemParametersForEmitter(const FNiagaraEmitterHandle& EmitterHandle);

	/** Request that any dirty scripts referenced by this system be compiled.*/
	bool RequestCompile(bool bForce);

	/** If we have a pending compile request, is it done with yet? */
	bool PollForCompilationComplete();

	/** */
	void WaitForCompilationComplete();

	/** Delegate called when the system's dependencies have all been compiled.*/
	FOnSystemCompiled& OnSystemCompiled();

	/** Gets editor specific data stored with this system. */
	UNiagaraEditorDataBase* GetEditorData();

	/** Gets editor specific data stored with this system. */
	const UNiagaraEditorDataBase* GetEditorData() const;

	/** Sets editor specific data stored with this system. */
	void SetEditorData(UNiagaraEditorDataBase* InEditorData);

	/** Internal: The thumbnail image.*/
	UPROPERTY()
	class UTexture2D* ThumbnailImage;

	/** Internal: Indicates the thumbnail image is out of date.*/
	UPROPERTY()
	uint32 ThumbnailImageOutOfDate : 1;

	bool GetIsolateEnabled() const;
	void SetIsolateEnabled(bool bIsolate);
#endif

	bool ShouldAutoDeactivate() const { return bAutoDeactivate; }
	bool IsLooping() const;

	const TArray<FNiagaraEmitterSpawnAttributes>& GetEmitterSpawnAttributes()const {	return EmitterSpawnAttributes;	};

	bool UsesCollection(const UNiagaraParameterCollection* Collection)const;
#if WITH_EDITORONLY_DATA
	bool UsesEmitter(const UNiagaraEmitter* Emitter) const;
	bool UsesScript(const UNiagaraScript* Script)const; 
	void InvalidateCachedCompileIds();

	static void RequestCompileForEmitter(UNiagaraEmitter* InEmitter);
#endif

	FORCEINLINE UNiagaraParameterCollectionInstance* GetParameterCollectionOverride(UNiagaraParameterCollection* Collection)
	{
		UNiagaraParameterCollectionInstance** Found = ParameterCollectionOverrides.FindByPredicate(
			[&](const UNiagaraParameterCollectionInstance* CheckInst)
		{
			return CheckInst && Collection == CheckInst->Collection;
		});

		return Found ? *Found : nullptr;
	}
	
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDumpDebugSystemInfo;

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDumpDebugEmitterInfo;

private:
#if WITH_EDITORONLY_DATA
	INiagaraModule::FMergeEmitterResults MergeChangesForEmitterHandle(FNiagaraEmitterHandle& EmitterHandle);
	bool QueryCompileComplete(bool bWait, bool bDoPost, bool bDoNotApply = false);
#endif

	void DetermineIfSolo();
protected:

	/** Handles to the emitter this System will simulate. */
	UPROPERTY(VisibleAnywhere, Category = "Emitters")
	TArray<FNiagaraEmitterHandle> EmitterHandles;

	UPROPERTY(EditAnywhere, Category="System")
	TArray<UNiagaraParameterCollectionInstance*> ParameterCollectionOverrides;

	UPROPERTY(Transient)
	TArray<FNiagaraSystemCompileRequest> ActiveCompilations;

// 	/** Category of this system. */
// 	UPROPERTY(EditAnywhere, Category = System)
// 	UNiagaraSystemCategory* Category;

	/** The script which defines the System parameters, and which generates the bindings from System
		parameter to emitter parameter. */
	UPROPERTY()
	UNiagaraScript* SystemSpawnScript;

	/** The script which defines the System parameters, and which generates the bindings from System
	parameter to emitter parameter. */
	UPROPERTY()
	UNiagaraScript* SystemUpdateScript;

	/** Attribute names in the data set that are driving each emitter's spawning. */
	UPROPERTY()
	TArray<FNiagaraEmitterSpawnAttributes> EmitterSpawnAttributes;

	/** Variables exposed to the outside work for tweaking*/
	UPROPERTY()
	FNiagaraParameterStore ExposedParameters;

#if WITH_EDITORONLY_DATA	

	/** Data used by the editor to maintain UI state etc.. */
	UPROPERTY()
	UNiagaraEditorDataBase* EditorData;

	bool bIsolateEnabled;

	/** A multicast delegate which is called whenever the script has been compiled (successfully or not). */
	FOnSystemCompiled OnSystemCompiledDelegate;
#endif

	UPROPERTY(EditAnywhere, Category = Performance, meta = (ToolTip = "Auto-deactivate system if all emitters are determined to not spawn particles again, regardless of lifetime."))
	bool bAutoDeactivate;

	/** Warm up time in seconds. Used to calculate WarmupTickCount. Rounds down to the nearest multiple of WarmupTickDelta. */
	UPROPERTY(EditAnywhere, Category = Warmup)
	float WarmupTime;

	/** Number of ticks to process for warmup. You can set by this or by time via WarmupTime. */
	UPROPERTY(EditAnywhere, Category = Warmup)
	int32 WarmupTickCount;

	/** Delta time to use for warmup ticks. */
	UPROPERTY(EditAnywhere, Category = Warmup)
	float WarmupTickDelta;


	UPROPERTY()
	uint32 bSolo : 1;

	void InitEmitterSpawnAttributes();
};
