// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NiagaraScript.h"
#include "Modules/ModuleManager.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitter.h"
#include "UObject/Package.h"
#include "UObject/Linker.h"
#include "NiagaraModule.h"
#include "NiagaraCustomVersion.h"
#include "NiagaraShaderCompilationManager.h"
#include "Serialization/MemoryReader.h"

#include "Stats/Stats.h"
#include "UObject/Linker.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "UObject/EditorObjectVersion.h"
#include "UObject/ReleaseObjectVersion.h"


#if WITH_EDITOR
	#include "NiagaraScriptDerivedData.h"
	#include "DerivedDataCacheInterface.h"
	#include "Interfaces/ITargetPlatform.h"
#endif

#include "UObject/FortniteMainBranchObjectVersion.h"
#include "UObject/RenderingObjectVersion.h"

DECLARE_STATS_GROUP(TEXT("Niagara Detailed"), STATGROUP_NiagaraDetailed, STATCAT_Advanced);

FNiagaraScriptDebuggerInfo::FNiagaraScriptDebuggerInfo() : bWaitForGPU(false), FrameLastWriteId(-1), bWritten(false)
{
}


FNiagaraScriptDebuggerInfo::FNiagaraScriptDebuggerInfo(FName InName, ENiagaraScriptUsage InUsage, const FGuid& InUsageId) : HandleName(InName), Usage(InUsage), UsageId(InUsageId), FrameLastWriteId(-1), bWritten(false)
{
	if (InUsage == ENiagaraScriptUsage::ParticleGPUComputeScript)
	{
		bWaitForGPU = true;
	}
	else
	{
		bWaitForGPU = false;
	}
}


UNiagaraScriptSourceBase::UNiagaraScriptSourceBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}


FNiagaraVMExecutableData::FNiagaraVMExecutableData() 
	: NumUserPtrs(0)
	, LastOpCount(0)
	, LastCompileStatus(ENiagaraScriptCompileStatus::NCS_Unknown)
	, bReadsAttributeData(false)
	, CompileTime(0.0f)
{
}


bool FNiagaraVMExecutableData::IsValid() const
{
	return LastCompileStatus != ENiagaraScriptCompileStatus::NCS_Unknown;
}

void FNiagaraVMExecutableData::Reset() 
{
	*this = FNiagaraVMExecutableData();
}

void FNiagaraVMExecutableData::SerializeData(FArchive& Ar, bool bDDCData)
{
	FNiagaraVMExecutableData::StaticStruct()->SerializeBin(Ar, this);
}

bool FNiagaraVMExecutableDataId::IsValid() const
{
	return BaseScriptID.IsValid();
}

void FNiagaraVMExecutableDataId::Invalidate()
{
	*this = FNiagaraVMExecutableDataId();
}

bool FNiagaraVMExecutableDataId::HasInterpolatedParameters() const
{
	return AdditionalDefines.Contains("InterpolatedSpawn");
}

bool FNiagaraVMExecutableDataId::RequiresPersistentIDs() const
{
	return AdditionalDefines.Contains("RequiresPersistentIDs");
}

/**
* Tests this set against another for equality, disregarding override settings.
*
* @param ReferenceSet	The set to compare against
* @return				true if the sets are equal
*/
bool FNiagaraVMExecutableDataId::operator==(const FNiagaraVMExecutableDataId& ReferenceSet) const
{
	if (CompilerVersionID != ReferenceSet.CompilerVersionID ||
		ScriptUsageType != ReferenceSet.ScriptUsageType || 
		ScriptUsageTypeID != ReferenceSet.ScriptUsageTypeID ||
		BaseScriptID != ReferenceSet.BaseScriptID)
	{
		return false;
	}
	
	if (ReferencedDependencyIds.Num() != ReferenceSet.ReferencedDependencyIds.Num())
	{
		return false;
	}

	for (int32 RefFunctionIndex = 0; RefFunctionIndex < ReferenceSet.ReferencedDependencyIds.Num(); RefFunctionIndex++)
	{
		const FGuid& ReferenceGuid = ReferenceSet.ReferencedDependencyIds[RefFunctionIndex];

		if (ReferencedDependencyIds[RefFunctionIndex] != ReferenceGuid)
		{
			return false;
		}
	}

	if (AdditionalDefines.Num() != ReferenceSet.AdditionalDefines.Num())
	{
		return false;
	}

	for (int32 Idx = 0; Idx < ReferenceSet.AdditionalDefines.Num(); Idx++)
	{
		const FString& ReferenceStr = ReferenceSet.AdditionalDefines[Idx];

		if (AdditionalDefines[Idx] != ReferenceStr)
		{
			return false;
		}
	}


	return true;
}

void FNiagaraVMExecutableDataId::AppendKeyString(FString& KeyString) const
{
	KeyString += FString::Printf(TEXT("%d_"), (int32)ScriptUsageType);
	KeyString += ScriptUsageTypeID.ToString();
	KeyString += TEXT("_");
	KeyString += CompilerVersionID.ToString();
	KeyString += TEXT("_");
	KeyString += BaseScriptID.ToString();
	KeyString += TEXT("_");

	for (int32 Idx = 0; Idx < AdditionalDefines.Num(); Idx++)
	{
		KeyString += AdditionalDefines[Idx];

		if (Idx < AdditionalDefines.Num() - 1)
		{
			KeyString += TEXT("_");
		}
	}
	
	// Add any referenced functions to the key so that we will recompile when they are changed
	for (int32 FunctionIndex = 0; FunctionIndex < ReferencedDependencyIds.Num(); FunctionIndex++)
	{
		KeyString += ReferencedDependencyIds[FunctionIndex].ToString();

		if (FunctionIndex < ReferencedDependencyIds.Num() - 1)
		{
			KeyString += TEXT("_");
		}
	}	
}

UNiagaraScript::UNiagaraScript(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Usage(ENiagaraScriptUsage::Function)
#if WITH_EDITORONLY_DATA
	, UsageIndex_DEPRECATED(0)
#endif
	, ModuleUsageBitmask( (1 << (int32)ENiagaraScriptUsage::ParticleSpawnScript) | (1 << (int32)ENiagaraScriptUsage::ParticleSpawnScriptInterpolated) | (1 << (int32)ENiagaraScriptUsage::ParticleUpdateScript) | (1 << (int32)ENiagaraScriptUsage::ParticleEventScript) )
	, NumericOutputTypeSelectionMode(ENiagaraNumericOutputTypeSelectionMode::Largest)
{
#if WITH_EDITORONLY_DATA
	ScriptResource.OnCompilationComplete().AddUniqueDynamic(this, &UNiagaraScript::OnCompilationComplete);

	RapidIterationParameters.DebugName = *GetFullName();
#endif	
}

UNiagaraScript::~UNiagaraScript()
{
}

#if WITH_EDITORONLY_DATA
class UNiagaraSystem* UNiagaraScript::FindRootSystem()
{
	UObject* Obj = GetOuter();
	if (UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(Obj))
	{
		Obj = Emitter->GetOuter();
	}

	if (UNiagaraSystem* Sys = Cast<UNiagaraSystem>(Obj))
	{
		return Sys;
	}

	return nullptr;
}

void UNiagaraScript::ComputeVMCompilationId(FNiagaraVMExecutableDataId& Id) const
{
	Id = FNiagaraVMExecutableDataId();
	
	// Ideally we wouldn't want to do this but rather than push the data down
	// from the emitter.
	UObject* Obj = GetOuter();
	if (UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(Obj))
	{
		if ((Emitter->bInterpolatedSpawning && Usage == ENiagaraScriptUsage::ParticleGPUComputeScript) || 
			(Emitter->bInterpolatedSpawning && Usage == ENiagaraScriptUsage::ParticleSpawnScript) ||
			Usage == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated)
		{
			Id.AdditionalDefines.Add(TEXT("InterpolatedSpawn"));
		}
		if (Emitter->RequiresPersistantIDs())
		{
			Id.AdditionalDefines.Add(TEXT("RequiresPersistentIDs"));
		}
		if (Emitter->bLocalSpace)
		{
			Id.AdditionalDefines.Add(TEXT("Emitter.Localspace"));
		}
		if (Emitter->bDeterminism)
		{
			Id.AdditionalDefines.Add(TEXT("Emitter.Determinism"));
		}
	}

	if (UNiagaraSystem* System = Cast<UNiagaraSystem>(Obj))
	{
		for (const FNiagaraEmitterHandle& EmitterHandle: System->GetEmitterHandles())
		{
			UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(EmitterHandle.GetInstance());
			if (Emitter)
			{
				if (Emitter->bLocalSpace)
				{
					Id.AdditionalDefines.Add(Emitter->GetUniqueEmitterName() + TEXT(".Localspace"));
				}
				if (Emitter->bDeterminism)
				{
					Id.AdditionalDefines.Add(Emitter->GetUniqueEmitterName() + TEXT(".Determinism"));
				}
			}
		}
	}

	Source->ComputeVMCompilationId(Id, Usage, UsageId);
	
	LastGeneratedVMId = Id;
}
#endif

bool UNiagaraScript::ContainsUsage(ENiagaraScriptUsage InUsage) const
{
	if (IsEquivalentUsage(InUsage))
	{
		return true;
	}

	if (Usage == ENiagaraScriptUsage::ParticleGPUComputeScript && IsParticleScript(InUsage))
	{
		return true;
	}

	if (InUsage == ENiagaraScriptUsage::ParticleUpdateScript && Usage == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated)
	{
		return true;
	}

	if (InUsage == ENiagaraScriptUsage::EmitterSpawnScript && Usage == ENiagaraScriptUsage::SystemSpawnScript)
	{
		return true;
	}

	if (InUsage == ENiagaraScriptUsage::EmitterUpdateScript && Usage == ENiagaraScriptUsage::SystemUpdateScript)
	{
		return true;
	}

	return false;
}

FNiagaraScriptExecutionParameterStore* UNiagaraScript::GetExecutionReadyParameterStore(ENiagaraSimTarget SimTarget)
{
	if (SimTarget == ENiagaraSimTarget::CPUSim && IsReadyToRun(ENiagaraSimTarget::CPUSim))
	{
		if (ScriptExecutionParamStoreCPU.IsInitialized() == false)
		{
			ScriptExecutionParamStoreCPU.InitFromOwningScript(this, SimTarget, false);
		}
		return &ScriptExecutionParamStoreCPU;
	}
	else if (SimTarget == ENiagaraSimTarget::GPUComputeSim)
	{
		if (ScriptExecutionParamStoreGPU.IsInitialized() == false)
		{
			ScriptExecutionParamStoreGPU.InitFromOwningScript(this, SimTarget, false);
		}
		return &ScriptExecutionParamStoreGPU;
	}
	else
	{
		return nullptr;
	}
}


void UNiagaraScript::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FNiagaraCustomVersion::GUID);		// only changes version if not loading
	const int32 NiagaraVer = Ar.CustomVer(FNiagaraCustomVersion::GUID);
	
	bool IsValidShaderScript = false;
	if (NiagaraVer < FNiagaraCustomVersion::DontCompileGPUWhenNotNeeded)
	{
		IsValidShaderScript = Usage != ENiagaraScriptUsage::Module && Usage != ENiagaraScriptUsage::Function && Usage != ENiagaraScriptUsage::DynamicInput
			&& (NiagaraVer < FNiagaraCustomVersion::NiagaraShaderMapCooking2 || (Usage != ENiagaraScriptUsage::SystemSpawnScript && Usage != ENiagaraScriptUsage::SystemUpdateScript))
			&& (NiagaraVer < FNiagaraCustomVersion::NiagaraCombinedGPUSpawnUpdate || (Usage != ENiagaraScriptUsage::ParticleUpdateScript && Usage != ENiagaraScriptUsage::EmitterSpawnScript && Usage != ENiagaraScriptUsage::EmitterUpdateScript));
	}
	else if (NiagaraVer < FNiagaraCustomVersion::MovedToDerivedDataCache)
	{
		IsValidShaderScript = LegacyCanBeRunOnGpu();
	}
	else
	{
		IsValidShaderScript = CanBeRunOnGpu();
	}

	if ( (!Ar.IsLoading() && IsValidShaderScript)		// saving shader maps only for particle sim and spawn scripts
		|| (Ar.IsLoading() && NiagaraVer >= FNiagaraCustomVersion::NiagaraShaderMaps && (NiagaraVer < FNiagaraCustomVersion::NiagaraShaderMapCooking || IsValidShaderScript))  // load only if we know shader map is presen
		)
	{
#if WITH_EDITOR
		SerializeNiagaraShaderMaps(&CachedScriptResourcesForCooking, Ar, LoadedScriptResources);
#else
		SerializeNiagaraShaderMaps(nullptr, Ar, LoadedScriptResources);
#endif
	}
}

/** Is usage A dependent on Usage B?*/
bool UNiagaraScript::IsUsageDependentOn(ENiagaraScriptUsage InUsageA, ENiagaraScriptUsage InUsageB)
{
	if (InUsageA == InUsageB)
	{
		return false;
	}

	// Usages of the same phase are interdependent because we copy the attributes from one to the other and if those got 
	// out of sync, there could be problems.

	if ((InUsageA == ENiagaraScriptUsage::ParticleSpawnScript || InUsageA == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated || InUsageA == ENiagaraScriptUsage::ParticleUpdateScript || InUsageA == ENiagaraScriptUsage::ParticleEventScript)
		&& (InUsageB == ENiagaraScriptUsage::ParticleSpawnScript || InUsageB == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated || InUsageB == ENiagaraScriptUsage::ParticleUpdateScript || InUsageB == ENiagaraScriptUsage::ParticleEventScript))
	{
		return true;
	}

	// The GPU compute script is always dependent on the other particle scripts.
	if ((InUsageA == ENiagaraScriptUsage::ParticleGPUComputeScript)
		&& (InUsageB == ENiagaraScriptUsage::ParticleSpawnScript || InUsageB == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated || InUsageB == ENiagaraScriptUsage::ParticleUpdateScript || InUsageB == ENiagaraScriptUsage::ParticleEventScript))
	{
		return true;
	}

	if ((InUsageA == ENiagaraScriptUsage::EmitterSpawnScript || InUsageA == ENiagaraScriptUsage::EmitterUpdateScript)
		&& (InUsageB == ENiagaraScriptUsage::EmitterSpawnScript || InUsageB == ENiagaraScriptUsage::EmitterUpdateScript))
	{
		return true;
	}

	if ((InUsageA == ENiagaraScriptUsage::SystemSpawnScript || InUsageA == ENiagaraScriptUsage::SystemUpdateScript)
		&& (InUsageB == ENiagaraScriptUsage::SystemSpawnScript || InUsageB == ENiagaraScriptUsage::SystemUpdateScript))
	{
		return true;
	}

	return false;
}

bool UNiagaraScript::ConvertUsageToGroup(ENiagaraScriptUsage InUsage, ENiagaraScriptGroup& OutGroup)
{
	if (IsParticleScript(InUsage) || IsStandaloneScript(InUsage))
	{
		OutGroup = ENiagaraScriptGroup::Particle;
		return true;
	}
	else if (IsEmitterSpawnScript(InUsage) || IsEmitterUpdateScript(InUsage))
	{
		OutGroup = ENiagaraScriptGroup::Emitter;
		return true;
	}
	else if (IsSystemSpawnScript(InUsage) || IsSystemUpdateScript(InUsage))
	{
		OutGroup = ENiagaraScriptGroup::System;
		return true;
	}

	return false;
}

void UNiagaraScript::PostLoad()
{
	Super::PostLoad();
	
	bool bNeedsRecompile = false;
	const int32 NiagaraVer = GetLinkerCustomVersion(FNiagaraCustomVersion::GUID);
	
	// Resources can be processed / registered now that we're back on the main thread
	ProcessSerializedShaderMaps(this, LoadedScriptResources, ScriptResource, ScriptResourcesByFeatureLevel);

	if (GIsEditor)
	{
		
#if WITH_EDITORONLY_DATA
		// Since we're about to check the synchronized state, we need to make sure that it has been post-loaded (which 
		// can affect the results of that call).
		if (Source != nullptr)
		{	
			Source->ConditionalPostLoad();
		}

#endif
	}

	// for now, force recompile until we can be sure everything is working
	//bNeedsRecompile = true;
#if WITH_EDITORONLY_DATA
	CacheResourceShadersForRendering(false, bNeedsRecompile);
#endif
#if STATS
	GenerateStatScopeIDs();
#endif

}


bool UNiagaraScript::IsReadyToRun(ENiagaraSimTarget SimTarget) const
{
	if (SimTarget == ENiagaraSimTarget::CPUSim)
	{
		if (CachedScriptVM.IsValid())
		{
			return true;
		}
	}
	else if (SimTarget == ENiagaraSimTarget::GPUComputeSim)
	{
		return CanBeRunOnGpu();
	}

	return false;
}



#if STATS
void UNiagaraScript::GenerateStatScopeIDs()
{
	StatScopesIDs.Empty();
	if (IsReadyToRun(ENiagaraSimTarget::CPUSim))
	{
		for (FNiagaraStatScope& StatScope : CachedScriptVM.StatScopes)
		{
			StatScopesIDs.Add(FDynamicStats::CreateStatId<FStatGroup_STATGROUP_NiagaraDetailed>(StatScope.FriendlyName.ToString()));
		}
	}
}
#endif

#if WITH_EDITOR

void UNiagaraScript::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName;
	if (PropertyChangedEvent.Property)
	{
		PropertyName = PropertyChangedEvent.Property->GetFName();
	}

	CacheResourceShadersForRendering(true);

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UNiagaraScript, bDeprecated) || PropertyName == GET_MEMBER_NAME_CHECKED(UNiagaraScript, DeprecationRecommendation))
	{
		if (Source)
		{
			Source->MarkNotSynchronized(TEXT("Deprecation changed."));
		}
	}
}

#endif

#if WITH_EDITORONLY_DATA
bool UNiagaraScript::AreScriptAndSourceSynchronized() const
{
	if (Source)
	{
		FNiagaraVMExecutableDataId NewId;
		ComputeVMCompilationId(NewId);
		bool bSynchronized = (NewId.IsValid() && NewId == CachedScriptVMId);
		if (!bSynchronized && NewId.IsValid() && CachedScriptVMId.IsValid() && CachedScriptVM.IsValid())
		{
			if (NewId != LastReportedVMId)
			{
				if (NewId.BaseScriptID != CachedScriptVMId.BaseScriptID)
				{
					UE_LOG(LogNiagara, Log, TEXT("AreScriptAndSourceSynchronized base script id's don't match. %s != %s"), *NewId.BaseScriptID.ToString(), *CachedScriptVMId.BaseScriptID.ToString());
				}
				if (NewId.ReferencedDependencyIds.Num() != CachedScriptVMId.ReferencedDependencyIds.Num())
				{
					UE_LOG(LogNiagara, Log, TEXT("AreScriptAndSourceSynchronized num dependencies don't match. %d != %d"), NewId.ReferencedDependencyIds.Num(), CachedScriptVMId.ReferencedDependencyIds.Num());
				}
				else
				{
					for (int32 i = 0; i < NewId.ReferencedDependencyIds.Num(); i++)
					{
						if (NewId.ReferencedDependencyIds[i] != CachedScriptVMId.ReferencedDependencyIds[i])
						{
							UE_LOG(LogNiagara, Log, TEXT("AreScriptAndSourceSynchronized reference id %d doesn't match. %s != %s, source %s"), i, *NewId.ReferencedDependencyIds[i].ToString(), *CachedScriptVMId.ReferencedDependencyIds[i].ToString(),
								NewId.ReferencedObjects[i] != nullptr ? *NewId.ReferencedObjects[i]->GetPathName() : TEXT("nullptr"));
						}
					}
				}
				LastReportedVMId = NewId;
			}
		}
		return bSynchronized;
	}
	else
	{
		return false;
	}
}

void UNiagaraScript::MarkScriptAndSourceDesynchronized(FString Reason)
{
	if (Source)
	{
		Source->MarkNotSynchronized(Reason);
	}
}

bool UNiagaraScript::HandleVariableRenames(const TMap<FNiagaraVariable, FNiagaraVariable>& OldToNewVars, const FString& UniqueEmitterName)
{
	bool bConvertedAnything = false;
	auto Iter = OldToNewVars.CreateConstIterator();
	while (Iter)
	{
		// Sometimes the script is under the generic name, other times it has been converted to the unique emitter name. Handle both cases below...
		FNiagaraVariable RISrcVarA = FNiagaraUtilities::ConvertVariableToRapidIterationConstantName(Iter->Key, !UniqueEmitterName.IsEmpty() ? TEXT("Emitter") : nullptr , GetUsage());
		FNiagaraVariable RISrcVarB = FNiagaraUtilities::ConvertVariableToRapidIterationConstantName(Iter->Key, !UniqueEmitterName.IsEmpty() ? *UniqueEmitterName : nullptr, GetUsage());
		FNiagaraVariable RIDestVarA = FNiagaraUtilities::ConvertVariableToRapidIterationConstantName(Iter->Value, !UniqueEmitterName.IsEmpty() ? TEXT("Emitter") : nullptr, GetUsage());
		FNiagaraVariable RIDestVarB = FNiagaraUtilities::ConvertVariableToRapidIterationConstantName(Iter->Value, !UniqueEmitterName.IsEmpty() ? *UniqueEmitterName : nullptr, GetUsage());

		{
			if (nullptr != RapidIterationParameters.FindParameterOffset(RISrcVarA))
			{
				RapidIterationParameters.RenameParameter(RISrcVarA, RIDestVarA.GetName());
				UE_LOG(LogNiagara, Log, TEXT("Converted RI variable \"%s\" to \"%s\" in Script \"%s\""), *RISrcVarA.GetName().ToString(), *RIDestVarA.GetName().ToString(), *GetFullName());
				bConvertedAnything = true;
			}
			else if (nullptr != RapidIterationParameters.FindParameterOffset(RISrcVarB))
			{
				RapidIterationParameters.RenameParameter(RISrcVarB, RIDestVarB.GetName());
				UE_LOG(LogNiagara, Log, TEXT("Converted RI variable \"%s\" to \"%s\" in Script \"%s\""), *RISrcVarB.GetName().ToString(), *RIDestVarB.GetName().ToString(), *GetFullName());
				bConvertedAnything = true;
			}
		}

		{
			// Go ahead and convert the stored VM executable data too. I'm not 100% sure why this is necessary, since we should be recompiling.
			int32 VarIdx = GetVMExecutableData().Parameters.Parameters.IndexOfByKey(RISrcVarA);
			if (VarIdx != INDEX_NONE)
			{
				GetVMExecutableData().Parameters.Parameters[VarIdx].SetName(RIDestVarA.GetName());
				UE_LOG(LogNiagara, Log, TEXT("Converted exec param variable \"%s\" to \"%s\" in Script \"%s\""), *RISrcVarA.GetName().ToString(), *RIDestVarA.GetName().ToString(), *GetFullName());
				bConvertedAnything = true;
			}

			VarIdx = GetVMExecutableData().Parameters.Parameters.IndexOfByKey(RISrcVarB);
			if (VarIdx != INDEX_NONE)
			{
				GetVMExecutableData().Parameters.Parameters[VarIdx].SetName(RIDestVarB.GetName());
				UE_LOG(LogNiagara, Log, TEXT("Converted exec param  variable \"%s\" to \"%s\" in Script \"%s\""), *RISrcVarB.GetName().ToString(), *RIDestVarB.GetName().ToString(), *GetFullName());
				bConvertedAnything = true;
			}
		}

		{
			// Also handle any data set mappings...
			auto DS2PIterator = GetVMExecutableData().DataSetToParameters.CreateIterator();
			while (DS2PIterator)
			{
				for (int32 i = 0; i < DS2PIterator.Value().Parameters.Num(); i++)
				{
					FNiagaraVariable Var = DS2PIterator.Value().Parameters[i];
					if (Var == RISrcVarA)
					{
						DS2PIterator.Value().Parameters[i].SetName(RIDestVarA.GetName());
						bConvertedAnything = true;
					}
					else if (Var == RISrcVarB)
					{
						DS2PIterator.Value().Parameters[i].SetName(RIDestVarB.GetName());
						bConvertedAnything = true;
					}
				}
				++DS2PIterator;
			}
		}
		++Iter;
	}

	if (bConvertedAnything)
	{
		InvalidateExecutionReadyParameterStores();
	}

	return bConvertedAnything;
}

UNiagaraScript* UNiagaraScript::MakeRecursiveDeepCopy(UObject* DestOuter, TMap<const UObject*, UObject*>& ExistingConversions) const
{
	check(GetOuter() != DestOuter);

	bool bSourceConvertedAlready = ExistingConversions.Contains(Source);

	ResetLoaders(GetTransientPackage()); // Make sure that we're not going to get invalid version number linkers into the transient package. 
	GetTransientPackage()->LinkerCustomVersion.Empty();

	// For some reason, the default parameters of FObjectDuplicationParameters aren't the same as
	// StaticDuplicateObject uses internally. These are copied from Static Duplicate Object...
	EObjectFlags FlagMask = RF_AllFlags & ~RF_Standalone & ~RF_Public; // Remove Standalone and Public flags.
	EDuplicateMode::Type DuplicateMode = EDuplicateMode::Normal;
	EInternalObjectFlags InternalFlagsMask = EInternalObjectFlags::AllFlags;

	FObjectDuplicationParameters ObjParameters((UObject*)this, GetTransientPackage());
	ObjParameters.DestName = NAME_None;
	if (this->GetOuter() != DestOuter)
	{
		// try to keep the object name consistent if possible
		if (FindObjectFast<UObject>(DestOuter, GetFName()) == nullptr)
		{
			ObjParameters.DestName = GetFName();
		}
	}

	ObjParameters.DestClass = GetClass();
	ObjParameters.FlagMask = FlagMask;
	ObjParameters.InternalFlagMask = InternalFlagsMask;
	ObjParameters.DuplicateMode = DuplicateMode;
	
	// Make sure that we don't duplicate objects that we've already converted...
	TMap<const UObject*, UObject*>::TConstIterator It = ExistingConversions.CreateConstIterator();
	while (It)
	{
		ObjParameters.DuplicationSeed.Add(const_cast<UObject*>(It.Key()), It.Value());
		++It;
	}

	UNiagaraScript*	Script = CastChecked<UNiagaraScript>(StaticDuplicateObjectEx(ObjParameters));

	check(Script->HasAnyFlags(RF_Standalone) == false);
	check(Script->HasAnyFlags(RF_Public) == false);

	if (bSourceConvertedAlready)
	{
		// Confirm that we've converted these properly..
		check(Script->Source == ExistingConversions[Source]);
	}

	if (DestOuter != nullptr)
	{
		Script->Rename(nullptr, DestOuter, REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
	}
	UE_LOG(LogNiagara, Warning, TEXT("MakeRecursiveDeepCopy %s"), *Script->GetFullName());
	ExistingConversions.Add(const_cast<UNiagaraScript*>(this), Script);

	// Since the Source is the only thing we subsume from UNiagaraScripts, only do the subsume if 
	// we haven't already converted it.
	if (bSourceConvertedAlready == false)
	{
		Script->SubsumeExternalDependencies(ExistingConversions);
	}
	return Script;
}

void UNiagaraScript::SubsumeExternalDependencies(TMap<const UObject*, UObject*>& ExistingConversions)
{
	Source->SubsumeExternalDependencies(ExistingConversions);
}

void WriteTextFileToDisk(FString SaveDirectory, FString FileName, FString TextToSave, bool bAllowOverwriting)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// CreateDirectoryTree returns true if the destination
	// directory existed prior to call or has been created
	// during the call.
	if (PlatformFile.CreateDirectoryTree(*SaveDirectory))
	{
		// Get absolute file path
		FString AbsoluteFilePath = SaveDirectory + "/" + FileName;

		// Allow overwriting or file doesn't already exist
		if (bAllowOverwriting || !PlatformFile.FileExists(*AbsoluteFilePath))
		{
			if (FFileHelper::SaveStringToFile(TextToSave, *AbsoluteFilePath))
			{
				UE_LOG(LogNiagara, Log, TEXT("Wrote file to %s"), *AbsoluteFilePath);
				return;
			}

		}
	}
}

void UNiagaraScript::SetVMCompilationResults(const FNiagaraVMExecutableDataId& InCompileId, FNiagaraVMExecutableData& InScriptVM, FNiagaraCompileRequestDataBase* InRequestData)
{
	check(InRequestData != nullptr);

	CachedScriptVMId = InCompileId;
	CachedScriptVM = InScriptVM;
	CachedParameterCollectionReferences.Empty();
	
	if (CachedScriptVM.LastCompileStatus == ENiagaraScriptCompileStatus::NCS_Error)
	{
		UE_LOG(LogNiagara, Error, TEXT("%s"), *CachedScriptVM.ErrorMsg);
	}
	else if (CachedScriptVM.LastCompileStatus == ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings)
	{
		UE_LOG(LogNiagara, Warning, TEXT("%s"), *CachedScriptVM.ErrorMsg);
	}

	// The compilation process only references via soft references any parameter collections. This resolves those 
	// soft references to real references.
	for (FString& Path : CachedScriptVM.ParameterCollectionPaths)
	{
		FSoftObjectPath SoftPath(Path);
		UObject* Obj = SoftPath.TryLoad();
		UNiagaraParameterCollection* ParamCollection = Cast<UNiagaraParameterCollection>(Obj);
		if (ParamCollection != nullptr)
		{
			CachedParameterCollectionReferences.Add(ParamCollection);
		}
	}

	CachedDefaultDataInterfaces.Empty(CachedScriptVM.DataInterfaceInfo.Num());
	for (FNiagaraScriptDataInterfaceCompileInfo Info : CachedScriptVM.DataInterfaceInfo)
	{
		int32 Idx = CachedDefaultDataInterfaces.AddDefaulted();
		CachedDefaultDataInterfaces[Idx].UserPtrIdx = Info.UserPtrIdx;
		CachedDefaultDataInterfaces[Idx].Name = Info.Name;
		CachedDefaultDataInterfaces[Idx].Type = Info.Type;
		CachedDefaultDataInterfaces[Idx].RegisteredParameterMapRead = InRequestData->ResolveEmitterAlias(Info.RegisteredParameterMapRead);
		CachedDefaultDataInterfaces[Idx].RegisteredParameterMapWrite = InRequestData->ResolveEmitterAlias(Info.RegisteredParameterMapWrite);

		// We compiled it just a bit ago, so we should be able to resolve it from the table that we passed in.
		UNiagaraDataInterface*const* FindDIById = InRequestData->GetObjectNameMap().Find(Info.Name);
		if (FindDIById != nullptr && *(FindDIById) != nullptr)
		{
			CachedDefaultDataInterfaces[Idx].DataInterface = DuplicateObject<UNiagaraDataInterface>(*(FindDIById), this);
			check(CachedDefaultDataInterfaces[Idx].DataInterface != nullptr);
		}			
		
		if (CachedDefaultDataInterfaces[Idx].DataInterface == nullptr)
		{
			// Use the CDO since we didn't have a default..
			UObject* Obj = const_cast<UClass*>(Info.Type.GetClass())->GetDefaultObject(true);
			CachedDefaultDataInterfaces[Idx].DataInterface = Cast<UNiagaraDataInterface>(DuplicateObject(Obj, this));

			if (Info.bIsPlaceholder == false)
			{
				UE_LOG(LogNiagara, Error, TEXT("We somehow ended up with a data interface that we couldn't match post compile. This shouldn't happen. Creating a dummy to prevent crashes. %s"), *Info.Name.ToString());
			}
		}
		check(CachedDefaultDataInterfaces[Idx].DataInterface != nullptr);
	}

	GenerateStatScopeIDs();

	// Now go ahead and trigger the GPU script compile now that we have a compiled GPU hlsl script.
	if (Usage == ENiagaraScriptUsage::ParticleGPUComputeScript)
	{
		CacheResourceShadersForRendering(false, true);
	}

	InvalidateExecutionReadyParameterStores();
	
	OnVMScriptCompiled().Broadcast(this);
}

void UNiagaraScript::InvalidateExecutionReadyParameterStores()
{
	// Make sure that we regenerate any parameter stores, since they must be kept in sync with the layout from script compilation.
	ScriptExecutionParamStoreCPU.Empty();
	ScriptExecutionParamStoreGPU.Empty();
}

void UNiagaraScript::InvalidateCachedCompileIds()
{
	GetSource()->InvalidateCachedCompileIds();
}

void UNiagaraScript::RequestCompile()
{
	if (!AreScriptAndSourceSynchronized())
	{
		if (IsCompilable() == false)
		{
			CachedScriptVM.LastCompileStatus = ENiagaraScriptCompileStatus::NCS_Unknown;
			CachedScriptVMId = LastGeneratedVMId;
			return;
		}

		CachedScriptVM.LastCompileStatus = ENiagaraScriptCompileStatus::NCS_BeingCreated;

		TArray<TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe>> DependentRequests;
		TArray<uint8> OutData;
		INiagaraModule& NiagaraModule = FModuleManager::Get().LoadModuleChecked<INiagaraModule>(TEXT("Niagara"));
		TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> RequestData = NiagaraModule.Precompile(this);

		ActiveCompileRoots.Empty();
		RequestData->GetReferencedObjects(ActiveCompileRoots);

		FNiagaraCompileOptions Options(GetUsage(), GetUsageId(), ModuleUsageBitmask, GetPathName(), GetFullName(), GetName());

		FNiagaraScriptDerivedData* CompileTask = new FNiagaraScriptDerivedData(GetFullName(), RequestData, Options, LastGeneratedVMId, false);

		// For debugging DDC/Compression issues		
		const bool bSkipDDC = false;
		if (bSkipDDC)
		{
			CompileTask->Build(OutData);

			delete CompileTask;
			CompileTask = nullptr;
		}
		else
		{
			if (CompileTask->CanBuild())
			{
				GetDerivedDataCacheRef().GetSynchronous(CompileTask, OutData);
				// Assume that once given over to the derived cache, the compile task is going to be killed by it.
				CompileTask = nullptr;
			}
			else
			{
				delete CompileTask;
				CompileTask = nullptr;
			}
		}

		if (OutData.Num() > 0)
		{
			FNiagaraVMExecutableData ExeData;
			FNiagaraScriptDerivedData::BinaryToExecData(OutData, ExeData);
			SetVMCompilationResults(LastGeneratedVMId, ExeData, RequestData.Get());
		}
		else
		{
			check(false);
		}

		ActiveCompileRoots.Empty();
	}
	else
	{
		UE_LOG(LogNiagara, Log, TEXT("Script '%s' is in-sync skipping compile.."), *GetFullName());
	}
}

bool UNiagaraScript::RequestExternallyManagedAsyncCompile(const TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe>& RequestData, FNiagaraVMExecutableDataId& OutCompileId, uint32& OutAsyncHandle, bool bTrulyAsync)
{
	if (!AreScriptAndSourceSynchronized())
	{
		if (IsCompilable() == false)
		{
			OutCompileId = LastGeneratedVMId;
			OutAsyncHandle = (uint32)INDEX_NONE;
			CachedScriptVM.LastCompileStatus = ENiagaraScriptCompileStatus::NCS_Unknown;
			CachedScriptVMId = LastGeneratedVMId;
			return false;
		}

		CachedScriptVM.LastCompileStatus = ENiagaraScriptCompileStatus::NCS_BeingCreated;

		OutCompileId = LastGeneratedVMId;
		FNiagaraCompileOptions Options(GetUsage(), GetUsageId(), ModuleUsageBitmask, GetPathName(), GetFullName(), GetName());
		FNiagaraScriptDerivedData* CompileTask = new FNiagaraScriptDerivedData(GetFullName(), RequestData, Options, LastGeneratedVMId, bTrulyAsync);
	
		check(CompileTask->CanBuild());
		OutAsyncHandle = GetDerivedDataCacheRef().GetAsynchronous(CompileTask);

		return true;
	}
	else
	{
		OutCompileId = LastGeneratedVMId;
		OutAsyncHandle = (uint32)INDEX_NONE;
		UE_LOG(LogNiagara, Log, TEXT("Script '%s' is in-sync skipping compile.."), *GetFullName());
		return false;
	}
}
#endif

void UNiagaraScript::OnCompilationComplete()
{
#if WITH_EDITORONLY_DATA
	FNiagaraSystemUpdateContext(this, true);
#endif
}

void UNiagaraScript::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
#if WITH_EDITORONLY_DATA
	FName TagName = GET_MEMBER_NAME_CHECKED(UNiagaraScript, ProvidedDependencies);
	FString DependenciesProvidedString;
	for (FName DependencyProvided : ProvidedDependencies)
	{
		DependenciesProvidedString.Append(DependencyProvided.ToString() + ",");
	}
	if (ProvidedDependencies.Num() > 0)
	{
		OutTags.Add(FAssetRegistryTag(TagName, DependenciesProvidedString, UObject::FAssetRegistryTag::TT_Hidden));
	}
#endif
}

#if WITH_EDITOR

void UNiagaraScript::BeginCacheForCookedPlatformData(const ITargetPlatform *TargetPlatform)
{
	if (CanBeRunOnGpu())
	{
		// Commandlets like DerivedDataCacheCommandlet call BeginCacheForCookedPlatformData directly on objects. This may mean that
		// we have not properly gotten the HLSL script generated by the time that we get here. This does the awkward work of 
		// waiting on the parent system to finish generating the HLSL before we can begin compiling it for the GPU.
		UNiagaraSystem* SystemOwner = FindRootSystem();
		if (SystemOwner)
		{
			SystemOwner->WaitForCompilationComplete();
		}

		TArray<FName> DesiredShaderFormats;
		TargetPlatform->GetAllTargetedShaderFormats(DesiredShaderFormats);

		TArray<FNiagaraShaderScript*>& CachedScriptResourcesForPlatform = CachedScriptResourcesForCooking.FindOrAdd(TargetPlatform);

		// Cache for all the shader formats that the cooking target requires
		for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
		{
			const EShaderPlatform LegacyShaderPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);
			if (FNiagaraUtilities::SupportsGPUParticles(LegacyShaderPlatform))
			{
				CacheResourceShadersForCooking(LegacyShaderPlatform, CachedScriptResourcesForPlatform);
			}
		}
	}
}

void UNiagaraScript::CacheResourceShadersForCooking(EShaderPlatform ShaderPlatform, TArray<FNiagaraShaderScript*>& InOutCachedResources)
{
	if (CanBeRunOnGpu())
	{
		// spawn and update are combined on GPU, so we only compile spawn scripts
		if (Usage == ENiagaraScriptUsage::ParticleGPUComputeScript)
		{
			FNiagaraShaderScript *ResourceToCache = nullptr;
			ERHIFeatureLevel::Type TargetFeatureLevel = GetMaxSupportedFeatureLevel(ShaderPlatform);

			FNiagaraShaderScript* NewResource = AllocateResource();
			check(CachedScriptVMId.CompilerVersionID != FGuid());
			check(CachedScriptVMId.BaseScriptID != FGuid());

			NewResource->SetScript(this, (ERHIFeatureLevel::Type)TargetFeatureLevel, CachedScriptVMId.CompilerVersionID, CachedScriptVMId.BaseScriptID, CachedScriptVMId.ReferencedDependencyIds, GetName());
			ResourceToCache = NewResource;

			check(ResourceToCache);

			CacheShadersForResources(ShaderPlatform, ResourceToCache, false, false, true);

			INiagaraModule NiagaraModule = FModuleManager::GetModuleChecked<INiagaraModule>(TEXT("Niagara"));
			NiagaraModule.ProcessShaderCompilationQueue();

			InOutCachedResources.Add(ResourceToCache);
		}
	}
}



void UNiagaraScript::CacheShadersForResources(EShaderPlatform ShaderPlatform, FNiagaraShaderScript *ResourceToCache, bool bApplyCompletedShaderMapForRendering, bool bForceRecompile, bool bCooking)
{
	if (CanBeRunOnGpu())
	{
		// When not running in the editor, the shaders are created in-sync (in the postload) to avoid update issues.
		const bool bSuccess = ResourceToCache->CacheShaders(ShaderPlatform, bApplyCompletedShaderMapForRendering, bForceRecompile, bCooking || !GIsEditor);

#if defined(NIAGARA_SCRIPT_COMPILE_LOGGING_MEDIUM)
		if (!bSuccess)
		{
			UE_LOG(LogNiagara, Warning, TEXT("Failed to compile Niagara shader %s for platform %s."),
				*GetPathName(),
				*LegacyShaderPlatformToShaderFormat(ShaderPlatform).ToString());

			const TArray<FString>& CompileErrors = ResourceToCache->GetCompileErrors();
			for (int32 ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++)
			{
				UE_LOG(LogNiagara, Warning, TEXT("	%s"), *CompileErrors[ErrorIndex]);
			}
		}
#endif
	}
}

void UNiagaraScript::CacheResourceShadersForRendering(bool bRegenerateId, bool bForceRecompile)
{
	if (bRegenerateId)
	{
		// Regenerate this script's Id if requested
		for (int32 Idx = 0; Idx < ERHIFeatureLevel::Num; Idx++)
		{
			if (ScriptResourcesByFeatureLevel[Idx])
			{
				ScriptResourcesByFeatureLevel[Idx]->ReleaseShaderMap();
				ScriptResourcesByFeatureLevel[Idx] = nullptr;
			}
		}
	}

	//UpdateResourceAllocations();

	if (FApp::CanEverRender() && CanBeRunOnGpu())
	{
		if (Source)
		{
			FNiagaraShaderScript* ResourceToCache;
			ERHIFeatureLevel::Type CacheFeatureLevel = GMaxRHIFeatureLevel;
			ScriptResource.SetScript(this, FeatureLevel, CachedScriptVMId.CompilerVersionID, CachedScriptVMId.BaseScriptID, CachedScriptVMId.ReferencedDependencyIds, GetName());

			//if (ScriptResourcesByFeatureLevel[FeatureLevel])
			{
				if (FNiagaraUtilities::SupportsGPUParticles(CacheFeatureLevel))
				{
					EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[CacheFeatureLevel];
					ResourceToCache = ScriptResourcesByFeatureLevel[CacheFeatureLevel];
					CacheShadersForResources(ShaderPlatform, &ScriptResource, true);
					ScriptResourcesByFeatureLevel[CacheFeatureLevel] = &ScriptResource;
				}
			}
		}
	}
}

void UNiagaraScript::SyncAliases(const TMap<FString, FString>& RenameMap)
{
	// First handle any rapid iteration parameters...
	{
		TArray<FNiagaraVariable> Params;
		RapidIterationParameters.GetParameters(Params);
		for (FNiagaraVariable Var : Params)
		{
			FNiagaraVariable NewVar = FNiagaraVariable::ResolveAliases(Var, RenameMap);
			if (NewVar.GetName() != Var.GetName())
			{
				RapidIterationParameters.RenameParameter(Var, NewVar.GetName());
			}
		}
	}

	InvalidateExecutionReadyParameterStores();

	// Now handle any Parameters overall..
	for (int32 i = 0; i < GetVMExecutableData().Parameters.Parameters.Num(); i++)
	{
		if (GetVMExecutableData().Parameters.Parameters[i].IsValid() == false)
		{
			const FNiagaraVariable& InvalidParameter = GetVMExecutableData().Parameters.Parameters[i];
			UE_LOG(LogNiagara, Error, TEXT("Invalid parameter found while syncing script aliases.  Script: %s Parameter Name: %s Parameter Type: %s"),
				*GetPathName(), *InvalidParameter.GetName().ToString(), InvalidParameter.GetType().IsValid() ? *InvalidParameter.GetType().GetName() : TEXT("Unknown"));
			continue;
		}

		FNiagaraVariable Var = GetVMExecutableData().Parameters.Parameters[i];
		FNiagaraVariable NewVar = FNiagaraVariable::ResolveAliases(Var, RenameMap);
		if (NewVar.GetName() != Var.GetName())
		{
			GetVMExecutableData().Parameters.Parameters[i] = NewVar;
		}
	}

	// Also handle any data set mappings...
	auto Iterator = GetVMExecutableData().DataSetToParameters.CreateIterator();
	while (Iterator)
	{
		for (int32 i = 0; i < Iterator.Value().Parameters.Num(); i++)
		{
			FNiagaraVariable Var = Iterator.Value().Parameters[i];
			FNiagaraVariable NewVar = FNiagaraVariable::ResolveAliases(Var, RenameMap);
			if (NewVar.GetName() != Var.GetName())
			{
				Iterator.Value().Parameters[i] = NewVar;
			}
		}
		++Iterator;
	}
}

bool UNiagaraScript::SynchronizeExecutablesWithMaster(const UNiagaraScript* Script, const TMap<FString, FString>& RenameMap)
{
	FNiagaraVMExecutableDataId Id;
	ComputeVMCompilationId(Id);

#if 1 // TODO Shaun... turn this on...
	if (Id == Script->GetVMExecutableDataCompilationId())
	{
		CachedScriptVM.Reset();
		ScriptResource.Invalidate();

		CachedScriptVM = Script->CachedScriptVM;
		CachedScriptVMId = Script->CachedScriptVMId;
		CachedParameterCollectionReferences = Script->CachedParameterCollectionReferences;
		CachedDefaultDataInterfaces.Empty();
		for (const FNiagaraScriptDataInterfaceInfo& Info : Script->CachedDefaultDataInterfaces)
		{
			FNiagaraScriptDataInterfaceInfo AddInfo;
			AddInfo = Info;
			AddInfo.DataInterface = DuplicateObject<UNiagaraDataInterface>(Info.DataInterface, this);
			CachedDefaultDataInterfaces.Add(AddInfo);
		}

		GenerateStatScopeIDs();

		//SyncAliases(RenameMap);

		// Now go ahead and trigger the GPU script compile now that we have a compiled GPU hlsl script.
		if (Usage == ENiagaraScriptUsage::ParticleGPUComputeScript)
		{
			CacheResourceShadersForRendering(false, true);
		}

		OnVMScriptCompiled().Broadcast(this);
		return true;
	}
#endif

	return false;
}

void UNiagaraScript::InvalidateCompileResults()
{
	UE_LOG(LogNiagara, Log, TEXT("InvalidateCompileResults %s"), *GetPathName());
	CachedScriptVM.Reset();
	ScriptResource.Invalidate();
	CachedScriptVMId.Invalidate();
	LastGeneratedVMId.Invalidate();
}


UNiagaraScript::FOnScriptCompiled& UNiagaraScript::OnVMScriptCompiled()
{
	return OnVMScriptCompiledDelegate;
}



#endif


NIAGARA_API bool UNiagaraScript::IsScriptCompilationPending(bool bGPUScript) const
{
	if (bGPUScript)
	{
		FNiagaraShader *Shader = ScriptResource.GetShaderGameThread();
		if (Shader)
		{
			return false;
		}
		return !ScriptResource.IsCompilationFinished();
	}
	else
	{
		if (CachedScriptVM.IsValid())
		{
			return CachedScriptVM.ByteCode.Num() == 0 && (CachedScriptVM.LastCompileStatus == ENiagaraScriptCompileStatus::NCS_BeingCreated || CachedScriptVM.LastCompileStatus == ENiagaraScriptCompileStatus::NCS_Unknown);
		}
		return false;
	}
}

NIAGARA_API bool UNiagaraScript::DidScriptCompilationSucceed(bool bGPUScript) const
{
	if (bGPUScript)
	{
		FNiagaraShader *Shader = ScriptResource.GetShaderGameThread();
		if (Shader)
		{
			return true;
		}

		if (ScriptResource.IsCompilationFinished())
		{
			// If we failed compilation, it would be finished and Shader would be null.
			return false;
		}
	}
	else
	{
		if (CachedScriptVM.IsValid())
		{
			return CachedScriptVM.ByteCode.Num() != 0;
		}
	}

	return false;
}

void SerializeNiagaraShaderMaps(const TMap<const ITargetPlatform*, TArray<FNiagaraShaderScript*>>* PlatformScriptResourcesToSave, FArchive& Ar, TArray<FNiagaraShaderScript>& OutLoadedResources)
{
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);
	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);
	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);

//	SCOPED_LOADTIMER(SerializeInlineShaderMaps);
	if (Ar.IsSaving())
	{
		int32 NumResourcesToSave = 0;
		const TArray<FNiagaraShaderScript*>* ScriptResourcesToSavePtr = nullptr;

		if (Ar.IsCooking())
		{
			checkf(PlatformScriptResourcesToSave != nullptr, TEXT("PlatformScriptResourcesToSave must be supplied when cooking"));
			ScriptResourcesToSavePtr = PlatformScriptResourcesToSave->Find(Ar.CookingTarget());
			if (ScriptResourcesToSavePtr != nullptr)
			{
				NumResourcesToSave = ScriptResourcesToSavePtr->Num();
			}
		}

		Ar << NumResourcesToSave;

		if (ScriptResourcesToSavePtr != nullptr)
		{
			for (FNiagaraShaderScript* ScriptResourceToSave : (*ScriptResourcesToSavePtr))
			{
				checkf(ScriptResourceToSave != nullptr, TEXT("Invalid script resource was cached"));
				ScriptResourceToSave->SerializeShaderMap(Ar);
			}
		}
	}
	else if (Ar.IsLoading())
	{
		int32 NumLoadedResources = 0;
		Ar << NumLoadedResources;
		for (int32 i = 0; i < NumLoadedResources; i++)
		{
			FNiagaraShaderScript LoadedResource;
			LoadedResource.SerializeShaderMap(Ar);
			OutLoadedResources.Add(LoadedResource);
		}
	}
}

void ProcessSerializedShaderMaps(UNiagaraScript* Owner, TArray<FNiagaraShaderScript>& LoadedResources, FNiagaraShaderScript& OutResourceForCurrentPlatform, FNiagaraShaderScript* (&OutScriptResourcesLoaded)[ERHIFeatureLevel::Num])
{
	check(IsInGameThread());

	for (FNiagaraShaderScript& LoadedResource : LoadedResources)
	{
		LoadedResource.RegisterShaderMap();

		FNiagaraShaderMap* LoadedShaderMap = LoadedResource.GetGameThreadShaderMap();
		if (LoadedShaderMap && LoadedShaderMap->GetShaderPlatform() == GMaxRHIShaderPlatform)
		{
			OutResourceForCurrentPlatform = LoadedResource;

			ERHIFeatureLevel::Type LoadedFeatureLevel = LoadedShaderMap->GetShaderMapId().FeatureLevel;
			if (!OutScriptResourcesLoaded[LoadedFeatureLevel])
			{
				OutScriptResourcesLoaded[LoadedFeatureLevel] = Owner->AllocateResource();
			}

			OutScriptResourcesLoaded[LoadedFeatureLevel]->SetShaderMap(LoadedShaderMap);
			OutResourceForCurrentPlatform.SetDataInterfaceParamInfo(LoadedResource.GetShaderGameThread()->GetDIParameters());

			break;
		}
		else
		{
			LoadedResource.DiscardShaderMap();
		}
	}
}

FNiagaraShaderScript* UNiagaraScript::AllocateResource()
{
	return new FNiagaraShaderScript();
}

TArray<ENiagaraScriptUsage> UNiagaraScript::GetSupportedUsageContexts() const
{
	return GetSupportedUsageContextsForBitmask(ModuleUsageBitmask);
}

TArray<ENiagaraScriptUsage> UNiagaraScript::GetSupportedUsageContextsForBitmask(int32 InModuleUsageBitmask)
{
	TArray<ENiagaraScriptUsage> Supported;
	for (int32 i = 0; i <= (int32)ENiagaraScriptUsage::SystemUpdateScript; i++)
	{
		int32 TargetBit = (InModuleUsageBitmask >> (int32)i) & 1;
		if (TargetBit == 1)
		{
			Supported.Add((ENiagaraScriptUsage)i);
		}
	}
	return Supported;
}

bool UNiagaraScript::CanBeRunOnGpu()const
{

	if (Usage != ENiagaraScriptUsage::ParticleGPUComputeScript)
	{
		return false;
	}
	if (!CachedScriptVM.IsValid())
	{
		return false;
	}
	for (const FNiagaraScriptDataInterfaceCompileInfo& InterfaceInfo : CachedScriptVM.DataInterfaceInfo)
	{
		if (!InterfaceInfo.CanExecuteOnTarget(ENiagaraSimTarget::GPUComputeSim))
		{
			return false;
		}
	}
	return true;
}


bool UNiagaraScript::LegacyCanBeRunOnGpu() const
{
	if (UNiagaraEmitter* Emitter = GetTypedOuter<UNiagaraEmitter>())
	{
		if (Emitter->SimTarget == ENiagaraSimTarget::CPUSim)
		{
			return false;
		}

		if (!IsParticleSpawnScript())
		{
			return false;
		}

		return true;
	}
	return false;
}


#if WITH_EDITORONLY_DATA
FGuid UNiagaraScript::GetBaseChangeID() const
{
	return Source->GetChangeID(); 
}

ENiagaraScriptCompileStatus UNiagaraScript::GetLastCompileStatus() const
{
	if (CachedScriptVM.IsValid())
	{
		return CachedScriptVM.LastCompileStatus;
	}
	return ENiagaraScriptCompileStatus::NCS_Unknown;
}
#endif

bool UNiagaraScript::UsesCollection(const UNiagaraParameterCollection* Collection)const
{
	if (CachedScriptVM.IsValid())
	{
		return CachedParameterCollectionReferences.FindByPredicate([&](const UNiagaraParameterCollection* CheckCollection)
		{
			return CheckCollection == Collection;
		}) != NULL;
	}
	return false;
}
