// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#include "NiagaraSystem.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraRenderer.h"
#include "NiagaraConstants.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraCustomVersion.h"
#include "NiagaraModule.h"
#include "Modules/ModuleManager.h"
#include "NiagaraEmitter.h"
#include "UObject/Package.h"
#include "NiagaraEmitterHandle.h"
#include "AssetData.h"
#include "NiagaraStats.h"
#include "NiagaraEditorDataBase.h"

#if WITH_EDITOR
#include "NiagaraScriptDerivedData.h"
#include "DerivedDataCacheInterface.h"
#endif

DECLARE_CYCLE_STAT(TEXT("Niagara - System - Precompile"), STAT_Niagara_System_Precompile, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Niagara - System - CompileScript"), STAT_Niagara_System_CompileScript, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Niagara - System - CompileScript_ResetAfter"), STAT_Niagara_System_CompileScriptResetAfter, STATGROUP_Niagara);

// UNiagaraSystemCategory::UNiagaraSystemCategory(const FObjectInitializer& ObjectInitializer)
// 	: Super(ObjectInitializer)
// {
// }

//////////////////////////////////////////////////////////////////////////

UNiagaraSystem::UNiagaraSystem(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
, ExposedParameters(this)
#if WITH_EDITORONLY_DATA
, bIsolateEnabled(false)
#endif
, bAutoDeactivate(true)
, WarmupTime(0.0f)
, WarmupTickCount(0)
, WarmupTickDelta(1.0f / 15.0f)
{
}

void UNiagaraSystem::BeginDestroy()
{
	Super::BeginDestroy();
#if WITH_EDITORONLY_DATA
	while (ActiveCompilations.Num() > 0)
	{
		QueryCompileComplete(true, false, true);
	}
#endif
}

void UNiagaraSystem::PreSave(const class ITargetPlatform * TargetPlatform)
{
	Super::PreSave(TargetPlatform);
#if WITH_EDITORONLY_DATA
	WaitForCompilationComplete();
#endif
}

#if WITH_EDITOR
void UNiagaraSystem::BeginCacheForCookedPlatformData(const ITargetPlatform *TargetPlatform)
{
	Super::BeginCacheForCookedPlatformData(TargetPlatform);
#if WITH_EDITORONLY_DATA
	WaitForCompilationComplete();
#endif
}
#endif

void UNiagaraSystem::PostInitProperties()
{
	Super::PostInitProperties();
#if WITH_EDITORONLY_DATA
	ThumbnailImageOutOfDate = true;
#endif
	if (HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad) == false)
	{
		SystemSpawnScript = NewObject<UNiagaraScript>(this, "SystemSpawnScript", RF_Transactional);
		SystemSpawnScript->SetUsage(ENiagaraScriptUsage::SystemSpawnScript);

		SystemUpdateScript = NewObject<UNiagaraScript>(this, "SystemUpdateScript", RF_Transactional);
		SystemUpdateScript->SetUsage(ENiagaraScriptUsage::SystemUpdateScript);
	}
}

bool UNiagaraSystem::IsLooping() const
{ 
	return false; 
} //sckime todo fix this!

bool UNiagaraSystem::UsesCollection(const UNiagaraParameterCollection* Collection)const
{
	if (SystemSpawnScript->UsesCollection(Collection) ||
		SystemUpdateScript->UsesCollection(Collection))
	{
		return true;
	}

	for (const FNiagaraEmitterHandle& EmitterHandle : GetEmitterHandles())
	{
		if (EmitterHandle.GetInstance()->UsesCollection(Collection))
		{
			return true;
		}
	}

	return false;
}

#if WITH_EDITORONLY_DATA
bool UNiagaraSystem::UsesScript(const UNiagaraScript* Script)const
{
	if (SystemSpawnScript == Script ||
		SystemUpdateScript == Script)
	{
		return true;
	}

	for (FNiagaraEmitterHandle EmitterHandle : GetEmitterHandles())
	{
		if ((EmitterHandle.GetSource() && EmitterHandle.GetSource()->UsesScript(Script)) || (EmitterHandle.GetInstance() && EmitterHandle.GetInstance()->UsesScript(Script)))
		{
			return true;
		}
	}
	
	return false;
}

bool UNiagaraSystem::UsesEmitter(const UNiagaraEmitter* Emitter) const
{
	for (FNiagaraEmitterHandle EmitterHandle : GetEmitterHandles())
	{
		if (Emitter == EmitterHandle.GetSource() || Emitter == EmitterHandle.GetInstance())
		{
			return true;
		}
	}
	return false;
}

void UNiagaraSystem::RequestCompileForEmitter(UNiagaraEmitter* InEmitter)
{
	for (TObjectIterator<UNiagaraSystem> It; It; ++It)
	{
		UNiagaraSystem* Sys = *It;
		if (Sys && Sys->UsesEmitter(InEmitter))
		{
			Sys->RequestCompile(false);
		}
	}
}

#endif

void UNiagaraSystem::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FNiagaraCustomVersion::GUID);
}

#if WITH_EDITOR
void UNiagaraSystem::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	FNiagaraSystemUpdateContext(this, true);

	ThumbnailImageOutOfDate = true;

	DetermineIfSolo();
	
	if (PropertyChangedEvent.Property != nullptr)
	{
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraSystem, WarmupTickCount))
		{
			//Set the WarmupTime to feed back to the user.
			WarmupTime = WarmupTickCount * WarmupTickDelta;
		}
		else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraSystem, WarmupTime))
		{
			//Set the WarmupTickCount to feed back to the user.
			if (FMath::IsNearlyZero(WarmupTickDelta))
			{
				WarmupTickDelta = 0.0f;
			}
			else
			{
				WarmupTickCount = WarmupTime / WarmupTickDelta;
				WarmupTime = WarmupTickDelta * WarmupTickCount;
			}
		}
	}
}
#endif 

void UNiagaraSystem::PostLoad()
{
	Super::PostLoad();

	if (GIsEditor)
	{
		SetFlags(RF_Transactional);
	}

	// Previously added emitters didn't have their stand alone and public flags cleared so
	// they 'leak' into the system package.  Clear the flags here so they can be collected
	// during the next save.
	UPackage* PackageOuter = Cast<UPackage>(GetOuter());
	if (PackageOuter != nullptr && HasAnyFlags(RF_Public | RF_Standalone))
	{
		TArray<UObject*> ObjectsInPackage;
		GetObjectsWithOuter((UObject*)PackageOuter, ObjectsInPackage);
		for (UObject* ObjectInPackage : ObjectsInPackage)
		{
			UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(ObjectInPackage);
			if (Emitter != nullptr)
			{
				Emitter->ConditionalPostLoad();
				Emitter->ClearFlags(RF_Standalone | RF_Public);
			}
		}
	}

#if WITH_EDITORONLY_DATA
	TArray<UNiagaraScript*> AllSystemScripts;
	
	UNiagaraScriptSourceBase* SystemScriptSource = nullptr;
	if (SystemSpawnScript == nullptr)
	{
		SystemSpawnScript = NewObject<UNiagaraScript>(this, "SystemSpawnScript", RF_Transactional);
		SystemSpawnScript->SetUsage(ENiagaraScriptUsage::SystemSpawnScript);
		INiagaraModule& NiagaraModule = FModuleManager::GetModuleChecked<INiagaraModule>("Niagara");
		SystemScriptSource = NiagaraModule.CreateDefaultScriptSource(this);
		SystemSpawnScript->SetSource(SystemScriptSource);
	}
	else
	{
		SystemSpawnScript->ConditionalPostLoad();
		SystemScriptSource = SystemSpawnScript->GetSource();
	}
	AllSystemScripts.Add(SystemSpawnScript);

	if (SystemUpdateScript == nullptr)
	{
		SystemUpdateScript = NewObject<UNiagaraScript>(this, "SystemUpdateScript", RF_Transactional);
		SystemUpdateScript->SetUsage(ENiagaraScriptUsage::SystemUpdateScript);
		SystemUpdateScript->SetSource(SystemScriptSource);
	}
	else
	{
		SystemUpdateScript->ConditionalPostLoad();
	}
	AllSystemScripts.Add(SystemUpdateScript);

	const int32 NiagaraVer = GetLinkerCustomVersion(FNiagaraCustomVersion::GUID);

	//TODO: This causes a crash becuase the script source ptr is null? Fix
	//For existing emitters before the lifecylce rework, ensure they have the system lifecycle module.
	if (NiagaraVer < FNiagaraCustomVersion::LifeCycleRework)
	{
		/*UNiagaraScriptSourceBase* SystemScriptSource = SystemUpdateScript->GetSource();
		if (SystemScriptSource)
		{
			bool bFoundModule;
			if (SystemScriptSource->AddModuleIfMissing(TEXT("/Niagara/Modules/System/SystemLifeCycle.SystemLifeCycle"), ENiagaraScriptUsage::SystemUpdateScript, bFoundModule))
			{
				bNeedsRecompile = true;
			}
		}*/
	}

	bool bSystemScriptsAreSynchronized = true;
	for (UNiagaraScript* SystemScript : AllSystemScripts)
	{
		bSystemScriptsAreSynchronized &= SystemScript->AreScriptAndSourceSynchronized();
	}

	bool bEmitterGraphChangedFromMerge = false;
	bool bEmitterScriptsAreSynchronized = true;

#if 0
	UE_LOG(LogNiagara, Log, TEXT("PreMerger"));
	for (FNiagaraEmitterHandle& EmitterHandle : EmitterHandles)
	{
		UE_LOG(LogNiagara, Log, TEXT("Emitter Handle: %s"), *EmitterHandle.GetUniqueInstanceName());
		UNiagaraScript* UpdateScript = EmitterHandle.GetInstance()->GetScript(ENiagaraScriptUsage::ParticleUpdateScript, FGuid());
		UNiagaraScript* SpawnScript = EmitterHandle.GetInstance()->GetScript(ENiagaraScriptUsage::ParticleSpawnScript, FGuid());
		UE_LOG(LogNiagara, Log, TEXT("Spawn Parameters"));
		SpawnScript->GetVMExecutableData().Parameters.DumpParameters();
		UE_LOG(LogNiagara, Log, TEXT("Spawn RI Parameters"));
		SpawnScript->RapidIterationParameters.DumpParameters();
		UE_LOG(LogNiagara, Log, TEXT("Update Parameters"));
		UpdateScript->GetVMExecutableData().Parameters.DumpParameters();
		UE_LOG(LogNiagara, Log, TEXT("Update RI Parameters"));
		UpdateScript->RapidIterationParameters.DumpParameters();
	}
#endif

	for (FNiagaraEmitterHandle& EmitterHandle : EmitterHandles)
	{
		EmitterHandle.ConditionalPostLoad();
		if (EmitterHandle.IsSynchronizedWithSource() == false)
		{
			INiagaraModule::FMergeEmitterResults Results = MergeChangesForEmitterHandle(EmitterHandle);
			if (Results.bSucceeded)
			{
				bEmitterGraphChangedFromMerge |= Results.bModifiedGraph;
			}
		}
		if (bEmitterScriptsAreSynchronized)
		{
			if (!EmitterHandle.GetInstance()->AreAllScriptAndSourcesSynchronized())
			{
				bEmitterScriptsAreSynchronized = false;
			}
		}
	}

	if (EditorData != nullptr)
	{
		EditorData->PostLoadFromOwner(this);
	}

	if (UNiagaraEmitter::GetForceCompileOnLoad())
	{
		InvalidateCachedCompileIds();
		UE_LOG(LogNiagara, Log, TEXT("System %s being rebuilt because UNiagaraEmitter::GetForceCompileOnLoad() == true."), *GetPathName());
	}

	if (bSystemScriptsAreSynchronized == false)
	{
		UE_LOG(LogNiagara, Log, TEXT("System %s being compiled because there were changes to a system script Change ID."), *GetPathName());
	}

	if (bEmitterScriptsAreSynchronized == false)
	{
		UE_LOG(LogNiagara, Log, TEXT("System %s being compiled because there were changes to an emitter script Change ID."), *GetPathName());
	}

	if (bEmitterGraphChangedFromMerge)
	{
		UE_LOG(LogNiagara, Log, TEXT("System %s being compiled because graph changes were merged for a base emitter."), *GetPathName());
	}

#if 0
	UE_LOG(LogNiagara, Log, TEXT("Before"));
	for (FNiagaraEmitterHandle& EmitterHandle : EmitterHandles)
	{
		UE_LOG(LogNiagara, Log, TEXT("Emitter Handle: %s"), *EmitterHandle.GetUniqueInstanceName());
		UNiagaraScript* UpdateScript = EmitterHandle.GetInstance()->GetScript(ENiagaraScriptUsage::ParticleUpdateScript, FGuid());
		UNiagaraScript* SpawnScript = EmitterHandle.GetInstance()->GetScript(ENiagaraScriptUsage::ParticleSpawnScript, FGuid());
		UE_LOG(LogNiagara, Log, TEXT("Spawn Parameters"));
		SpawnScript->GetVMExecutableData().Parameters.DumpParameters();
		UE_LOG(LogNiagara, Log, TEXT("Spawn RI Parameters"));
		SpawnScript->RapidIterationParameters.DumpParameters();
		UE_LOG(LogNiagara, Log, TEXT("Update Parameters"));
		UpdateScript->GetVMExecutableData().Parameters.DumpParameters();
		UE_LOG(LogNiagara, Log, TEXT("Update RI Parameters"));
		UpdateScript->RapidIterationParameters.DumpParameters();
	}
#endif

	if (bSystemScriptsAreSynchronized == false || bEmitterScriptsAreSynchronized == false || bEmitterGraphChangedFromMerge)
	{
		RequestCompile(false);
	}

#if 0
	UE_LOG(LogNiagara, Log, TEXT("After"));
	for (FNiagaraEmitterHandle& EmitterHandle : EmitterHandles)
	{
		UE_LOG(LogNiagara, Log, TEXT("Emitter Handle: %s"), *EmitterHandle.GetUniqueInstanceName());
		UNiagaraScript* UpdateScript = EmitterHandle.GetInstance()->GetScript(ENiagaraScriptUsage::ParticleUpdateScript, FGuid());
		UNiagaraScript* SpawnScript = EmitterHandle.GetInstance()->GetScript(ENiagaraScriptUsage::ParticleSpawnScript, FGuid());
		UE_LOG(LogNiagara, Log, TEXT("Spawn Parameters"));
		SpawnScript->GetVMExecutableData().Parameters.DumpParameters();
		UE_LOG(LogNiagara, Log, TEXT("Spawn RI Parameters"));
		SpawnScript->RapidIterationParameters.DumpParameters();
		UE_LOG(LogNiagara, Log, TEXT("Update Parameters"));
		UpdateScript->GetVMExecutableData().Parameters.DumpParameters();
		UE_LOG(LogNiagara, Log, TEXT("Update RI Parameters"));
		UpdateScript->RapidIterationParameters.DumpParameters();
	}
#endif
#endif

	DetermineIfSolo();
}

#if WITH_EDITORONLY_DATA

UNiagaraEditorDataBase* UNiagaraSystem::GetEditorData()
{
	return EditorData;
}

const UNiagaraEditorDataBase* UNiagaraSystem::GetEditorData() const
{
	return EditorData;
}

void UNiagaraSystem::SetEditorData(UNiagaraEditorDataBase* InEditorData)
{
	EditorData = InEditorData;
}

INiagaraModule::FMergeEmitterResults UNiagaraSystem::MergeChangesForEmitterHandle(FNiagaraEmitterHandle& EmitterHandle)
{
	INiagaraModule::FMergeEmitterResults Results = EmitterHandle.MergeSourceChanges();
	if (Results.bSucceeded)
	{
		UNiagaraEmitter* Instance = EmitterHandle.GetInstance();
		RefreshSystemParametersFromEmitter(EmitterHandle);
		if (Instance->bInterpolatedSpawning)
		{
			Instance->UpdateScriptProps.Script->RapidIterationParameters.CopyParametersTo(
				Instance->SpawnScriptProps.Script->RapidIterationParameters, false, FNiagaraParameterStore::EDataInterfaceCopyMethod::None);
		}
	}
	else
	{
		UE_LOG(LogNiagara, Warning, TEXT("Failed to merge changes for base emitter.  System: %s  Emitter: %s  Error Message: %s"), 
			*GetPathName(), *EmitterHandle.GetName().ToString(), *Results.GetErrorMessagesString());
	}
	return Results;
}

bool UNiagaraSystem::ReferencesSourceEmitter(UNiagaraEmitter& Emitter)
{
	for (FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (&Emitter == Handle.GetSource())
		{
			return true;
		}
	}
	return false;
}

bool UNiagaraSystem::ReferencesInstanceEmitter(UNiagaraEmitter& Emitter)
{
	for (FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (&Emitter == Handle.GetInstance())
		{
			return true;
		}
	}
	return false;
}

void UNiagaraSystem::UpdateFromEmitterChanges(UNiagaraEmitter& ChangedSourceEmitter)
{
	bool bNeedsCompile = false;
	for(FNiagaraEmitterHandle& EmitterHandle : EmitterHandles)
	{
		if (EmitterHandle.GetSource() == &ChangedSourceEmitter)
		{
			INiagaraModule::FMergeEmitterResults Results = MergeChangesForEmitterHandle(EmitterHandle);
			bNeedsCompile |= Results.bSucceeded && Results.bModifiedGraph;
		}
	}

	if (bNeedsCompile)
	{
		RequestCompile(false);
	}
}

void UNiagaraSystem::RefreshSystemParametersFromEmitter(const FNiagaraEmitterHandle& EmitterHandle)
{
	InitEmitterSpawnAttributes();
	if (ensureMsgf(EmitterHandles.ContainsByPredicate([=](const FNiagaraEmitterHandle& OwnedEmitterHandle) { return OwnedEmitterHandle.GetId() == EmitterHandle.GetId(); }),
		TEXT("Can't refresh parameters from an emitter handle this system doesn't own.")))
	{
		EmitterHandle.GetInstance()->EmitterSpawnScriptProps.Script->RapidIterationParameters.CopyParametersTo(
			SystemSpawnScript->RapidIterationParameters, false, FNiagaraParameterStore::EDataInterfaceCopyMethod::None);
		EmitterHandle.GetInstance()->EmitterUpdateScriptProps.Script->RapidIterationParameters.CopyParametersTo(
			SystemUpdateScript->RapidIterationParameters, false, FNiagaraParameterStore::EDataInterfaceCopyMethod::None);
	}
}

void UNiagaraSystem::RemoveSystemParametersForEmitter(const FNiagaraEmitterHandle& EmitterHandle)
{
	InitEmitterSpawnAttributes();
	if (ensureMsgf(EmitterHandles.ContainsByPredicate([=](const FNiagaraEmitterHandle& OwnedEmitterHandle) { return OwnedEmitterHandle.GetId() == EmitterHandle.GetId(); }),
		TEXT("Can't remove parameters for an emitter handle this system doesn't own.")))
	{
		EmitterHandle.GetInstance()->EmitterSpawnScriptProps.Script->RapidIterationParameters.RemoveParameters(SystemSpawnScript->RapidIterationParameters);
		EmitterHandle.GetInstance()->EmitterUpdateScriptProps.Script->RapidIterationParameters.RemoveParameters(SystemUpdateScript->RapidIterationParameters);
	}
}
#endif


const TArray<FNiagaraEmitterHandle>& UNiagaraSystem::GetEmitterHandles()
{
	return EmitterHandles;
}

const TArray<FNiagaraEmitterHandle>& UNiagaraSystem::GetEmitterHandles()const
{
	return EmitterHandles;
}

bool UNiagaraSystem::IsReadyToRun() const
{
	if (!SystemSpawnScript || !SystemUpdateScript)
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	if (HasOutstandingCompilationRequests())
	{
		return false;
	}
#endif

	if (SystemSpawnScript->IsScriptCompilationPending(false) || 
		SystemUpdateScript->IsScriptCompilationPending(false))
	{
		return false;
	}

	for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (!Handle.GetInstance()->IsReadyToRun())
		{
			return false;
		}
	}
	return true;
}

bool UNiagaraSystem::HasOutstandingCompilationRequests() const
{
	return ActiveCompilations.Num() > 0;
}


bool UNiagaraSystem::IsSolo()const
{
	return bSolo;
}

void UNiagaraSystem::DetermineIfSolo()
{
	//Determine if we can update normally or have to update solo.
	bSolo = false;
	//If our scripts have any interfaces that require instance data.
	UNiagaraScript* SystemSpawn = GetSystemSpawnScript();
	if (SystemSpawn->GetVMExecutableData().IsValid())
	{
		for (int32 i = 0; !bSolo && i < SystemSpawn->GetVMExecutableData().DataInterfaceInfo.Num(); ++i)
		{
			FNiagaraScriptDataInterfaceCompileInfo& Info = SystemSpawn->GetVMExecutableData().DataInterfaceInfo[i];
			if (Info.IsSystemSolo())//Temp hack to force solo on any systems with system scrips needing user (aka per instance) interfaces.
			{
				bSolo = true;
				break;
			}
		}
	}

	UNiagaraScript* SystemUpdate = GetSystemUpdateScript();
	if (SystemUpdate->GetVMExecutableData().IsValid())
	{
		for (int32 i = 0; !bSolo && i < SystemUpdate->GetVMExecutableData().DataInterfaceInfo.Num(); ++i)
		{
			FNiagaraScriptDataInterfaceCompileInfo& Info = SystemUpdate->GetVMExecutableData().DataInterfaceInfo[i];
			if (Info.IsSystemSolo())//Temp hack to force solo on any systems with system scrips needing user (aka per instance) interfaces.
			{
				bSolo = true;
				break;
			}
		}
	}
}

bool UNiagaraSystem::IsValid()const
{
	if (!SystemSpawnScript || !SystemUpdateScript)
	{
		return false;
	}

	if ((!SystemSpawnScript->IsScriptCompilationPending(false) && !SystemSpawnScript->DidScriptCompilationSucceed(false)) ||
		(!SystemUpdateScript->IsScriptCompilationPending(false) && !SystemUpdateScript->DidScriptCompilationSucceed(false)))
	{
		return false;
	}


	for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (!Handle.GetInstance()->IsValid())
		{
			return false;
		}
	}

	return true;
}
#if WITH_EDITORONLY_DATA

FNiagaraEmitterHandle UNiagaraSystem::AddEmitterHandle(UNiagaraEmitter& SourceEmitter, FName EmitterName)
{
	FNiagaraEmitterHandle EmitterHandle(SourceEmitter, EmitterName, *this);
	EmitterHandles.Add(EmitterHandle);
	RefreshSystemParametersFromEmitter(EmitterHandle);
	return EmitterHandle;
}

FNiagaraEmitterHandle UNiagaraSystem::AddEmitterHandleWithoutCopying(UNiagaraEmitter& Emitter)
{
	FNiagaraEmitterHandle EmitterHandle(Emitter);
	EmitterHandles.Add(EmitterHandle);
	RefreshSystemParametersFromEmitter(EmitterHandle);
	return EmitterHandle;
}

FNiagaraEmitterHandle UNiagaraSystem::DuplicateEmitterHandle(const FNiagaraEmitterHandle& EmitterHandleToDuplicate, FName EmitterName)
{
	FNiagaraEmitterHandle EmitterHandle(EmitterHandleToDuplicate, EmitterName, *this);
	EmitterHandles.Add(EmitterHandle);
	RefreshSystemParametersFromEmitter(EmitterHandle);
	return EmitterHandle;
}

void UNiagaraSystem::RemoveEmitterHandle(const FNiagaraEmitterHandle& EmitterHandleToDelete)
{
	UNiagaraEmitter* EditableEmitter = EmitterHandleToDelete.GetInstance();
	RemoveSystemParametersForEmitter(EmitterHandleToDelete);
	auto RemovePredicate = [&](const FNiagaraEmitterHandle& EmitterHandle) { return EmitterHandle.GetId() == EmitterHandleToDelete.GetId(); };
	EmitterHandles.RemoveAll(RemovePredicate);
}

void UNiagaraSystem::RemoveEmitterHandlesById(const TSet<FGuid>& HandlesToRemove)
{
	auto RemovePredicate = [&](const FNiagaraEmitterHandle& EmitterHandle)
	{
		return HandlesToRemove.Contains(EmitterHandle.GetId());
	};
	EmitterHandles.RemoveAll(RemovePredicate);

	InitEmitterSpawnAttributes();
}
#endif


UNiagaraScript* UNiagaraSystem::GetSystemSpawnScript()
{
	return SystemSpawnScript;
}

UNiagaraScript* UNiagaraSystem::GetSystemUpdateScript()
{
	return SystemUpdateScript;
}

#if WITH_EDITORONLY_DATA

bool UNiagaraSystem::GetIsolateEnabled() const
{
	return bIsolateEnabled;
}

void UNiagaraSystem::SetIsolateEnabled(bool bIsolate)
{
	bIsolateEnabled = bIsolate;
}

UNiagaraSystem::FOnSystemCompiled& UNiagaraSystem::OnSystemCompiled()
{
	return OnSystemCompiledDelegate;
}

void UNiagaraSystem::InvalidateCachedCompileIds()
{
	check(SystemSpawnScript->GetSource() == SystemUpdateScript->GetSource());
	SystemSpawnScript->GetSource()->InvalidateCachedCompileIds();

	for (FNiagaraEmitterHandle Handle : EmitterHandles)
	{
		UNiagaraScriptSourceBase* GraphSource = Handle.GetInstance()->GraphSource;
		GraphSource->InvalidateCachedCompileIds();
	}
}

void UNiagaraSystem::WaitForCompilationComplete()
{
	while (ActiveCompilations.Num() > 0)
	{
		QueryCompileComplete(true, ActiveCompilations.Num() == 1);
	}
}

bool UNiagaraSystem::PollForCompilationComplete()
{
	if (ActiveCompilations.Num() > 0)
	{
		return QueryCompileComplete(false, true);
	}
	return true;
}

bool UNiagaraSystem::QueryCompileComplete(bool bWait, bool bDoPost, bool bDoNotApply)
{
	if (ActiveCompilations.Num() > 0)
	{
		int32 ActiveCompileIdx = 0;

		bool bAreWeWaitingForAnyResults = false;

		// Check to see if ALL of the sub-requests have resolved. 
		for (FEmitterCompiledScriptPair& EmitterCompiledScriptPair : ActiveCompilations[ActiveCompileIdx].EmitterCompiledScriptPairs)
		{
			if ((uint32)INDEX_NONE == EmitterCompiledScriptPair.PendingDDCID || EmitterCompiledScriptPair.bResultsReady)
			{
				continue;
			}
			if (bWait)
			{
				GetDerivedDataCacheRef().WaitAsynchronousCompletion(EmitterCompiledScriptPair.PendingDDCID);
				EmitterCompiledScriptPair.bResultsReady = true;
			}
			else
			{
				EmitterCompiledScriptPair.bResultsReady = GetDerivedDataCacheRef().PollAsynchronousCompletion(EmitterCompiledScriptPair.PendingDDCID);
				if (!EmitterCompiledScriptPair.bResultsReady)
				{
					bAreWeWaitingForAnyResults = true;
				}
			}

			// If the results are ready, go ahead and cache them so that the pending task isn't removed prematurely...
			if (EmitterCompiledScriptPair.bResultsReady)
			{
				TArray<uint8> OutData;
				bool bBuiltLocally = false;
				if (GetDerivedDataCacheRef().GetAsynchronousResults(EmitterCompiledScriptPair.PendingDDCID, OutData, &bBuiltLocally))
				{
					if (bBuiltLocally)
					{
						UE_LOG(LogNiagara, Log, TEXT("UNiagraScript \'%s\' was built locally.."), *EmitterCompiledScriptPair.CompiledScript->GetFullName());
					}
					else
					{
						UE_LOG(LogNiagara, Log, TEXT("UNiagraScript \'%s\' was pulled from DDC."), *EmitterCompiledScriptPair.CompiledScript->GetFullName());
					}

					TSharedPtr<FNiagaraVMExecutableData> ExeData = MakeShared<FNiagaraVMExecutableData>();
					EmitterCompiledScriptPair.CompileResults = ExeData;
					if (!bDoNotApply)
					{
						FNiagaraScriptDerivedData::BinaryToExecData(OutData, *(ExeData.Get()));
					}
				}
			}
		}

		check(bWait ? (bAreWeWaitingForAnyResults == false) : true);

		// Make sure that we aren't waiting for any results to come back.
		if (bAreWeWaitingForAnyResults && !bWait)
		{
			return false;
		}

		// In the world of do not apply, we're exiting the system completely so let's just kill any active compilations altogether.
		if (bDoNotApply)
		{
			ActiveCompilations[ActiveCompileIdx].RootObjects.Empty();
			ActiveCompilations.RemoveAt(ActiveCompileIdx);
			return true;
		}


		SCOPE_CYCLE_COUNTER(STAT_Niagara_System_CompileScript);

		// Now that the above code says they are all complete, go ahead and resolve them all at once.
		float CombinedCompileTime = 0.0f;
		for (FEmitterCompiledScriptPair& EmitterCompiledScriptPair : ActiveCompilations[ActiveCompileIdx].EmitterCompiledScriptPairs)
		{
			if ((uint32)INDEX_NONE == EmitterCompiledScriptPair.PendingDDCID )
			{
				continue;
			}
			CombinedCompileTime += EmitterCompiledScriptPair.CompileResults->CompileTime;
			check(EmitterCompiledScriptPair.bResultsReady);

			TSharedPtr<FNiagaraVMExecutableData> ExeData = EmitterCompiledScriptPair.CompileResults;
			TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> PrecompData = ActiveCompilations[ActiveCompileIdx].MappedData.FindChecked(EmitterCompiledScriptPair.CompiledScript);
			EmitterCompiledScriptPair.CompiledScript->SetVMCompilationResults(EmitterCompiledScriptPair.CompileId, *(ExeData.Get()), PrecompData.Get());	
		}

		if (bDoPost)
		{
			for (FNiagaraEmitterHandle Handle : EmitterHandles)
			{
				Handle.GetInstance()->OnPostCompile();
			}
		}

		InitEmitterSpawnAttributes();

		// Prepare rapid iteration parameters for execution.
		TArray<UNiagaraScript*> Scripts;
		TMap<UNiagaraScript*, UNiagaraScript*> ScriptDependencyMap;
		TMap<UNiagaraScript*, FString> ScriptToEmitterNameMap;
		for (FEmitterCompiledScriptPair& EmitterCompiledScriptPair : ActiveCompilations[ActiveCompileIdx].EmitterCompiledScriptPairs)
		{
			UNiagaraEmitter* Emitter = EmitterCompiledScriptPair.Emitter;
			UNiagaraScript* CompiledScript = EmitterCompiledScriptPair.CompiledScript;

			Scripts.AddUnique(CompiledScript);
			ScriptToEmitterNameMap.Add(CompiledScript, Emitter != nullptr ? Emitter->GetUniqueEmitterName() : FString());

			if (UNiagaraScript::IsEquivalentUsage(CompiledScript->GetUsage(), ENiagaraScriptUsage::EmitterSpawnScript))
			{
				Scripts.AddUnique(SystemSpawnScript);
				ScriptDependencyMap.Add(CompiledScript, SystemSpawnScript);
				ScriptToEmitterNameMap.Add(SystemSpawnScript, FString());
			}

			if (UNiagaraScript::IsEquivalentUsage(CompiledScript->GetUsage(), ENiagaraScriptUsage::EmitterUpdateScript))
			{
				Scripts.AddUnique(SystemUpdateScript);
				ScriptDependencyMap.Add(CompiledScript, SystemUpdateScript);
				ScriptToEmitterNameMap.Add(SystemSpawnScript, FString());
			}

			if (UNiagaraScript::IsEquivalentUsage(CompiledScript->GetUsage(), ENiagaraScriptUsage::ParticleSpawnScript))
			{
				if (Emitter && Emitter->SimTarget == ENiagaraSimTarget::GPUComputeSim)
				{
					Scripts.AddUnique(Emitter->GetGPUComputeScript());
					ScriptDependencyMap.Add(CompiledScript, Emitter->GetGPUComputeScript());
					ScriptToEmitterNameMap.Add(Emitter->GetGPUComputeScript(), Emitter->GetUniqueEmitterName());
				}
			}

			if (UNiagaraScript::IsEquivalentUsage(CompiledScript->GetUsage(), ENiagaraScriptUsage::ParticleUpdateScript))
			{
				if (Emitter && Emitter->SimTarget == ENiagaraSimTarget::GPUComputeSim)
				{
					Scripts.AddUnique(Emitter->GetGPUComputeScript());
					ScriptDependencyMap.Add(CompiledScript, Emitter->GetGPUComputeScript());
					ScriptToEmitterNameMap.Add(Emitter->GetGPUComputeScript(), Emitter->GetUniqueEmitterName());
				}
				else if (Emitter && Emitter->bInterpolatedSpawning)
				{
					Scripts.AddUnique(Emitter->SpawnScriptProps.Script);
					ScriptDependencyMap.Add(CompiledScript, Emitter->SpawnScriptProps.Script);
					ScriptToEmitterNameMap.Add(Emitter->SpawnScriptProps.Script, Emitter->GetUniqueEmitterName());
				}
			}
		}

		FNiagaraUtilities::PrepareRapidIterationParameters(Scripts, ScriptDependencyMap, ScriptToEmitterNameMap);

		// HACK: This is a temporary hack to fix an issue where data interfaces used by modules and dynamic inputs in the
		// particle update script aren't being shared by the interpolated spawn script when accessed directly.  This works
		// properly if the data interface is assigned to a named particle parameter and then linked to an input.
		// TODO: Bind these data interfaces the same way parameter data interfaces are bound.
		for (FEmitterCompiledScriptPair& EmitterCompiledScriptPair : ActiveCompilations[ActiveCompileIdx].EmitterCompiledScriptPairs)
		{
			UNiagaraEmitter* Emitter = EmitterCompiledScriptPair.Emitter;
			UNiagaraScript* CompiledScript = EmitterCompiledScriptPair.CompiledScript;

			if (UNiagaraScript::IsEquivalentUsage(CompiledScript->GetUsage(), ENiagaraScriptUsage::ParticleUpdateScript))
			{
				UNiagaraScript* SpawnScript = Emitter->SpawnScriptProps.Script;
				for (const FNiagaraScriptDataInterfaceInfo& UpdateDataInterfaceInfo : CompiledScript->GetCachedDefaultDataInterfaces())
				{
					if (UpdateDataInterfaceInfo.RegisteredParameterMapRead == NAME_None && UpdateDataInterfaceInfo.RegisteredParameterMapWrite == NAME_None)
					{
						// If the data interface isn't being read or written to a parameter map then it won't be bound properly so we
						// assign the update scripts copy of the data interface to the spawn scripts copy by pointer so that they will share
						// the data interface at runtime and will both be updated in the editor.
						for (FNiagaraScriptDataInterfaceInfo& SpawnDataInterfaceInfo : SpawnScript->GetCachedDefaultDataInterfaces())
						{
							if (UpdateDataInterfaceInfo.Name == SpawnDataInterfaceInfo.Name)
							{
								SpawnDataInterfaceInfo.DataInterface = UpdateDataInterfaceInfo.DataInterface;
							}
						}
					}
				}
			}
		}

		ActiveCompilations[ActiveCompileIdx].RootObjects.Empty();

		DetermineIfSolo();

		UE_LOG(LogNiagara, Log, TEXT("Compiling System %s took %f sec (wall time), %f sec (combined time)."), *GetFullName(), (float)(FPlatformTime::Seconds() - ActiveCompilations[ActiveCompileIdx].StartTime),
			CombinedCompileTime);

		ActiveCompilations.RemoveAt(ActiveCompileIdx);

		if (bDoPost)
		{
			SCOPE_CYCLE_COUNTER(STAT_Niagara_System_CompileScriptResetAfter);

			OnSystemCompiled().Broadcast(this);
		}

		return true;
	}

	return false;
}

bool UNiagaraSystem::RequestCompile(bool bForce)
{
	if (bForce)
	{
		InvalidateCachedCompileIds();
		bForce = false;
	}

	if (ActiveCompilations.Num() > 0)
	{
		PollForCompilationComplete();
	}
	
	int32 ActiveCompileIdx = ActiveCompilations.AddDefaulted();
	ActiveCompilations[ActiveCompileIdx].StartTime = FPlatformTime::Seconds();

	SCOPE_CYCLE_COUNTER(STAT_Niagara_System_Precompile);
	
	check(SystemSpawnScript->GetSource() == SystemUpdateScript->GetSource());
	TArray<FNiagaraVariable> OriginalExposedParams;
	GetExposedParameters().GetParameters(OriginalExposedParams);

	INiagaraModule& NiagaraModule = FModuleManager::Get().LoadModuleChecked<INiagaraModule>(TEXT("Niagara"));
	TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> SystemPrecompiledData = NiagaraModule.Precompile(this);

	SystemPrecompiledData->GetReferencedObjects(ActiveCompilations[ActiveCompileIdx].RootObjects);

	//Compile all emitters
	bool bTrulyAsync = true;
	bool bAnyUnsynchronized = false;	

	ActiveCompilations[ActiveCompileIdx].MappedData.Add(SystemSpawnScript, SystemPrecompiledData);
	ActiveCompilations[ActiveCompileIdx].MappedData.Add(SystemUpdateScript, SystemPrecompiledData);

	check(EmitterHandles.Num() == SystemPrecompiledData->GetDependentRequestCount());

	// Grab the list of user variables that were actually encountered so that we can add to them later.
	TArray<FNiagaraVariable> EncounteredExposedVars;
	SystemPrecompiledData->GatherPreCompiledVariables(TEXT("User"), EncounteredExposedVars);

	for (int32 i = 0; i < EmitterHandles.Num(); i++)
	{
		FNiagaraEmitterHandle Handle = EmitterHandles[i];

		UNiagaraScriptSourceBase* GraphSource = Handle.GetInstance()->GraphSource;
		TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> EmitterPrecompiledData = SystemPrecompiledData->GetDependentRequest(i);
		EmitterPrecompiledData->GetReferencedObjects(ActiveCompilations[ActiveCompileIdx].RootObjects);

		TArray<UNiagaraScript*> EmitterScripts;
		Handle.GetInstance()->GetScripts(EmitterScripts, false);
		check(EmitterScripts.Num() > 0);
		for (UNiagaraScript* EmitterScript : EmitterScripts)
		{
			ActiveCompilations[ActiveCompileIdx].MappedData.Add(EmitterScript, EmitterPrecompiledData);

			FEmitterCompiledScriptPair Pair;
			Pair.bResultsReady = false;
			Pair.Emitter = Handle.GetInstance();
			Pair.CompiledScript = EmitterScript;			
			if (EmitterScript->RequestExternallyManagedAsyncCompile(EmitterPrecompiledData, Pair.CompileId, Pair.PendingDDCID, bTrulyAsync))
			{
				bAnyUnsynchronized = true;
			}
			ActiveCompilations[ActiveCompileIdx].EmitterCompiledScriptPairs.Add(Pair);
		}	

		// Add the emitter's User variables to the encountered list to expose for later.
		EmitterPrecompiledData->GatherPreCompiledVariables(TEXT("User"), EncounteredExposedVars);
	}

	bool bForceSystems = bForce || bAnyUnsynchronized;
	bool bAnyCompiled = bAnyUnsynchronized || bForce;

	// Now add the system scripts for compilation...
	{
		FEmitterCompiledScriptPair Pair;
		Pair.bResultsReady = false;
		Pair.Emitter = nullptr;
		Pair.CompiledScript = SystemSpawnScript;
		if (SystemSpawnScript->RequestExternallyManagedAsyncCompile(SystemPrecompiledData, Pair.CompileId, Pair.PendingDDCID, bTrulyAsync))
		{
			bAnyCompiled = true;
		}
		ActiveCompilations[ActiveCompileIdx].EmitterCompiledScriptPairs.Add(Pair);
	}

	{
		FEmitterCompiledScriptPair Pair;
		Pair.bResultsReady = false;
		Pair.Emitter = nullptr;
		Pair.CompiledScript = SystemUpdateScript;
		if (SystemUpdateScript->RequestExternallyManagedAsyncCompile(SystemPrecompiledData, Pair.CompileId, Pair.PendingDDCID, bTrulyAsync))
		{
			bAnyCompiled = true;
		}
		ActiveCompilations[ActiveCompileIdx].EmitterCompiledScriptPairs.Add(Pair);
	}

	// Now let's synchronize the variables that we actually encountered during compile so that we can expose them to the end user.
	for (int32 i = 0; i < EncounteredExposedVars.Num(); i++)
	{
		if (OriginalExposedParams.Contains(EncounteredExposedVars[i]) == false)
		{
			// Just in case it wasn't added previously..
			ExposedParameters.AddParameter(EncounteredExposedVars[i]);
		}
	}

	FNiagaraSystemUpdateContext UpdateCtx(this, true);

	return bAnyCompiled;
}


#endif

void UNiagaraSystem::InitEmitterSpawnAttributes()
{
	EmitterSpawnAttributes.Empty();
	EmitterSpawnAttributes.SetNum(EmitterHandles.Num());
	FNiagaraTypeDefinition SpawnInfoDef = FNiagaraTypeDefinition(FNiagaraSpawnInfo::StaticStruct());
	if (SystemSpawnScript->GetVMExecutableData().IsValid())
	{
		for (FNiagaraVariable& Var : SystemSpawnScript->GetVMExecutableData().Attributes)
		{
			for (int32 EmitterIdx = 0; EmitterIdx < EmitterHandles.Num(); ++EmitterIdx)
			{
				UNiagaraEmitter* Emitter = EmitterHandles[EmitterIdx].GetInstance();
				FString EmitterName = Emitter->GetUniqueEmitterName() + TEXT(".");
				if (Var.GetType() == SpawnInfoDef && Var.GetName().ToString().StartsWith(EmitterName))
				{
					EmitterSpawnAttributes[EmitterIdx].SpawnAttributes.AddUnique(Var.GetName());
				}
			}
		}
	}
	if (SystemUpdateScript->GetVMExecutableData().IsValid())
	{
		for (FNiagaraVariable& Var : SystemUpdateScript->GetVMExecutableData().Attributes)
		{
			for (int32 EmitterIdx = 0; EmitterIdx < EmitterHandles.Num(); ++EmitterIdx)
			{
				UNiagaraEmitter* Emitter = EmitterHandles[EmitterIdx].GetInstance();
				FString EmitterName = Emitter->GetUniqueEmitterName() + TEXT(".");
				if (Var.GetType() == SpawnInfoDef && Var.GetName().ToString().StartsWith(EmitterName))
				{
					EmitterSpawnAttributes[EmitterIdx].SpawnAttributes.AddUnique(Var.GetName());
				}
			}
		}
	}
}