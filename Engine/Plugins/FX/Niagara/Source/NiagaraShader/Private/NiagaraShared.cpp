// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	NiagaraShared.cpp: Shared Niagara compute shader implementation.
=============================================================================*/

#include "NiagaraShared.h"
#include "NiagaraShaderModule.h"
#include "NiagaraShaderType.h"
#include "NiagaraShader.h"
#include "Stats/StatsMisc.h"
#include "UObject/CoreObjectVersion.h"
#include "Misc/App.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "ShaderCompiler.h"
#include "NiagaraShaderCompilationManager.h"
#include "RendererInterface.h"
#include "Modules/ModuleManager.h"
#include "NiagaraCustomVersion.h"

#if WITH_EDITOR
	NIAGARASHADER_API FNiagaraCompilationQueue* FNiagaraCompilationQueue::Singleton = nullptr;
#endif

FNiagaraShaderScript::~FNiagaraShaderScript()
{
#if WITH_EDITOR
	if (IsInGameThread())
	{
		FNiagaraCompilationQueue::Get()->RemovePending(this);
	}
#endif
}

/** Populates OutEnvironment with defines needed to compile shaders for this script. */
void FNiagaraShaderScript::SetupShaderCompilationEnvironment(
	EShaderPlatform Platform,
	FShaderCompilerEnvironment& OutEnvironment
	) const
{
	OutEnvironment.SetDefine(TEXT("GPU_SIMULATION_SHADER"), TEXT("1"));
}


NIAGARASHADER_API bool FNiagaraShaderScript::ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType) const
{
	check(ShaderType->GetNiagaraShaderType() )
	return true;
}

NIAGARASHADER_API void FNiagaraShaderScript::NotifyCompilationFinished()
{
	OnCompilationCompleteDelegate.Broadcast();
}

NIAGARASHADER_API void FNiagaraShaderScript::CancelCompilation()
{
#if WITH_EDITOR
	if (IsInGameThread())
	{
		FNiagaraShaderMap::RemovePendingScript(this);
		FNiagaraCompilationQueue::Get()->RemovePending(this);

		UE_LOG(LogShaders, Log, TEXT("CancelCompilation %p."), this);
		OutstandingCompileShaderMapIds.Empty();
	}
#endif
}

NIAGARASHADER_API void FNiagaraShaderScript::RemoveOutstandingCompileId(const int32 OldOutstandingCompileShaderMapId)
{
	if (0 <= OutstandingCompileShaderMapIds.Remove(OldOutstandingCompileShaderMapId))
	{
		UE_LOG(LogShaders, Log, TEXT("RemoveOutstandingCompileId %p %d"), this, OldOutstandingCompileShaderMapId);
	}
}

NIAGARASHADER_API void FNiagaraShaderScript::Invalidate()
{
	CancelCompilation();
	ReleaseShaderMap();
}


NIAGARASHADER_API void FNiagaraShaderScript::LegacySerialize(FArchive& Ar)
{
}

bool FNiagaraShaderScript::IsSame(const FNiagaraShaderMapId& InId) const
{
	return InId.BaseScriptID == BaseScriptId && InId.ReferencedDependencyIds == ReferencedDependencyIds && InId.CompilerVersionID == CompilerVersionId;
}


void FNiagaraShaderScript::GetDependentShaderTypes(EShaderPlatform Platform, TArray<FShaderType*>& OutShaderTypes) const
{
	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList()); ShaderTypeIt; ShaderTypeIt.Next())
	{
		FNiagaraShaderType* ShaderType = ShaderTypeIt->GetNiagaraShaderType();

		if ( ShaderType && ShaderType->ShouldCache(Platform, this) && ShouldCache(Platform, ShaderType) )
		{
			OutShaderTypes.Add(ShaderType);
		}
	}
}



NIAGARASHADER_API void FNiagaraShaderScript::GetShaderMapId(EShaderPlatform Platform, FNiagaraShaderMapId& OutId) const
{
	if (bLoadedCookedShaderMapId)
	{
		OutId = CookedShaderMapId;
	}
	else
	{
		TArray<FShaderType*> ShaderTypes;
		GetDependentShaderTypes(Platform, ShaderTypes);
		OutId.FeatureLevel = GetFeatureLevel();
		OutId.BaseScriptID = BaseScriptId;
		OutId.ReferencedDependencyIds = ReferencedDependencyIds;
		OutId.CompilerVersionID = FNiagaraCustomVersion::LatestScriptCompileVersion;
	}
}



void FNiagaraShaderScript::AddReferencedObjects(FReferenceCollector& Collector)
{
}


void FNiagaraShaderScript::RegisterShaderMap()
{
	if (GameThreadShaderMap)
	{
		GameThreadShaderMap->RegisterSerializedShaders(false);
	}
}

void  FNiagaraShaderScript::DiscardShaderMap()
{
	if (GameThreadShaderMap)
	{
		GameThreadShaderMap->DiscardSerializedShaders();
	}
}

void FNiagaraShaderScript::ReleaseShaderMap()
{
	if (GameThreadShaderMap)
	{
		GameThreadShaderMap = nullptr;

		FNiagaraShaderScript* Script = this;
		ENQUEUE_RENDER_COMMAND(ReleaseShaderMap)(
			[Script](FRHICommandListImmediate& RHICmdList)
			{
				Script->SetRenderingThreadShaderMap(nullptr);
			});
	}
}

void FNiagaraShaderScript::SerializeShaderMap(FArchive& Ar)
{
	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	if (FPlatformProperties::RequiresCookedData() && !bCooked && Ar.IsLoading())
	{
		UE_LOG(LogShaders, Fatal, TEXT("This platform requires cooked packages, and shaders were not cooked into this Niagara script %s."), *GetFriendlyName());
	}

	if (bCooked)
	{
		if (Ar.IsCooking())
		{
#if WITH_EDITOR
			FinishCompilation();

			bool bValid = GameThreadShaderMap != nullptr && GameThreadShaderMap->CompiledSuccessfully();
			Ar << bValid;

			if (bValid)
			{
				GameThreadShaderMap->Serialize(Ar);
			}
			//else if (GameThreadShaderMap != nullptr && !GameThreadShaderMap->CompiledSuccessfully())
			//{
			//	FString Name;
			//	UE_LOG(LogShaders, Error, TEXT("Failed to compile Niagara shader %s."), *GetFriendlyName());
			//}
#endif
		}
		else
		{
			bool bValid = false;
			Ar << bValid;

			if (bValid)
			{
				TRefCountPtr<FNiagaraShaderMap> LoadedShaderMap = new FNiagaraShaderMap();
				LoadedShaderMap->Serialize(Ar);

				// Toss the loaded shader data if this is a server only instance
				//@todo - don't cook it in the first place
				if (FApp::CanEverRender())
				{
					GameThreadShaderMap = RenderingThreadShaderMap = LoadedShaderMap;
				}
				else
				{
					LoadedShaderMap->DiscardSerializedShaders();
				}
			}
		}
	}
}

void FNiagaraShaderScript::SetScript(UNiagaraScript *InScript, ERHIFeatureLevel::Type InFeatureLevel, const FGuid& InCompilerVersionID, const FGuid& InBaseScriptID, const TArray<FGuid>& InReferencedDependencyIds, FString InFriendlyName)
{
	BaseVMScript = InScript;
	BaseScriptId = InBaseScriptID;
	CompilerVersionId = InCompilerVersionID;
	ReferencedDependencyIds = InReferencedDependencyIds;
	FriendlyName = InFriendlyName;
	SetFeatureLevel(InFeatureLevel);
}

NIAGARASHADER_API  void FNiagaraShaderScript::SetRenderingThreadShaderMap(FNiagaraShaderMap* InShaderMap)
{
	check(IsInRenderingThread());
	RenderingThreadShaderMap = InShaderMap;
}

NIAGARASHADER_API  bool FNiagaraShaderScript::IsCompilationFinished() const
{
	bool bRet = GameThreadShaderMap && GameThreadShaderMap.IsValid() && GameThreadShaderMap->IsCompilationFinalized();
	if (OutstandingCompileShaderMapIds.Num() == 0)
	{
		return true;
	}
	return bRet;
}

/**
* Cache the script's shaders
*/
#if WITH_EDITOR

bool FNiagaraShaderScript::CacheShaders(EShaderPlatform Platform, bool bApplyCompletedShaderMapForRendering, bool bForceRecompile, bool bSynchronous)
{
	FNiagaraShaderMapId NoStaticParametersId;
	GetShaderMapId(Platform, NoStaticParametersId);
	return CacheShaders(NoStaticParametersId, Platform, bApplyCompletedShaderMapForRendering, bForceRecompile, bSynchronous);
}

/**
* Caches the shaders for this script
*/
bool FNiagaraShaderScript::CacheShaders(const FNiagaraShaderMapId& ShaderMapId, EShaderPlatform Platform, bool bApplyCompletedShaderMapForRendering, bool bForceRecompile, bool bSynchronous)
{
	bool bSucceeded = false;

	{
		// Find the script's cached shader map.
		GameThreadShaderMap = FNiagaraShaderMap::FindId(ShaderMapId, Platform);

		// Attempt to load from the derived data cache if we are uncooked
		if (!bForceRecompile && (!GameThreadShaderMap || !GameThreadShaderMap->IsComplete(this, true)) && !FPlatformProperties::RequiresCookedData())
		{
			FNiagaraShaderMap::LoadFromDerivedDataCache(this, ShaderMapId, Platform, GameThreadShaderMap);
			if (GameThreadShaderMap && GameThreadShaderMap->IsValid())
			{
				UE_LOG(LogTemp, Display, TEXT("Loaded shader %s for Niagara script %s from DDC"), *GameThreadShaderMap->GetFriendlyName(), *GetFriendlyName());
			}
			else
			{
				UE_LOG(LogTemp, Display, TEXT("Loading shader for Niagara script %s from DDC failed. Shader needs recompile."), *GetFriendlyName());
			}
		}
	}

	bool bAssumeShaderMapIsComplete = false;
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	bAssumeShaderMapIsComplete = FPlatformProperties::RequiresCookedData();
#endif

	if (GameThreadShaderMap && GameThreadShaderMap->TryToAddToExistingCompilationTask(this))
	{
		//FNiagaraShaderMap::ShaderMapsBeingCompiled.Find(GameThreadShaderMap);
#if DEBUG_INFINITESHADERCOMPILE
		UE_LOG(LogTemp, Display, TEXT("Found existing compiling shader for Niagara script %s, linking to other GameThreadShaderMap 0x%08X%08X"), *GetFriendlyName(), (int)((int64)(GameThreadShaderMap.GetReference()) >> 32), (int)((int64)(GameThreadShaderMap.GetReference())));
#endif
		OutstandingCompileShaderMapIds.AddUnique(GameThreadShaderMap->GetCompilingId());
		UE_LOG(LogShaders, Log, TEXT("CacheShaders AddUniqueExisting %p %d"), this, GameThreadShaderMap->GetCompilingId());

		// Reset the shader map so we fall back to CPU sim until the compile finishes.
		GameThreadShaderMap = nullptr;
		bSucceeded = true;
	}
	else if (bForceRecompile || !GameThreadShaderMap || !(bAssumeShaderMapIsComplete || GameThreadShaderMap->IsComplete(this, false)))
	{
		if (FPlatformProperties::RequiresCookedData())
		{
			UE_LOG(LogShaders, Log, TEXT("Can't compile %s with cooked content!"), *GetFriendlyName());
			// Reset the shader map so we fall back to CPU sim
			GameThreadShaderMap = nullptr;
		}
		else
		{
			UE_LOG(LogShaders, Log, TEXT("%s cached shader map for script %s, compiling."), GameThreadShaderMap? TEXT("Incomplete") : TEXT("Missing"), *GetFriendlyName());

			// If there's no cached shader map for this script compile a new one.
			// This is just kicking off the compile, GameThreadShaderMap will not be complete yet
			bSucceeded = BeginCompileShaderMap(ShaderMapId, Platform, GameThreadShaderMap, bApplyCompletedShaderMapForRendering, bSynchronous);

			if (!bSucceeded)
			{
				GameThreadShaderMap = nullptr;
			}
		}
	}
	else
	{
		bSucceeded = true;
	}

	FNiagaraShaderScript* Script = this;
	FNiagaraShaderMap* LoadedShaderMap = GameThreadShaderMap;
	ENQUEUE_RENDER_COMMAND(FSetShaderMapOnScriptResources)(
		[Script, LoadedShaderMap](FRHICommandListImmediate& RHICmdList)
		{
			Script->SetRenderingThreadShaderMap(LoadedShaderMap);
		});

	return bSucceeded;
}


void FNiagaraShaderScript::FinishCompilation()
{
	TArray<int32> ShaderMapIdsToFinish;
	GetShaderMapIDsWithUnfinishedCompilation(ShaderMapIdsToFinish);

	if (ShaderMapIdsToFinish.Num() > 0)
	{
		for (int32 i = 0; i < ShaderMapIdsToFinish.Num(); i++)
		{
			UE_LOG(LogShaders, Log, TEXT("FinishCompilation()[%d] %s id %d!"), i, *GetFriendlyName(), ShaderMapIdsToFinish[i]);
		}
		// Block until the shader maps that we will save have finished being compiled
		// NIAGARATODO: implement when async compile works
		GNiagaraShaderCompilationManager.FinishCompilation(*GetFriendlyName(), ShaderMapIdsToFinish);

		// Shouldn't have anything left to do...
		TArray<int32> ShaderMapIdsToFinish2;
		GetShaderMapIDsWithUnfinishedCompilation(ShaderMapIdsToFinish2);
		if (ShaderMapIdsToFinish2.Num() != 0)
		{
			UE_LOG(LogShaders, Warning, TEXT("Skipped multiple Niagara shader maps for compilation! May be indicative of no support for a given platform. Count: %d"), ShaderMapIdsToFinish2.Num());
		}
	}
}

#endif

void FNiagaraShaderScript::SetDataInterfaceParamInfo(TArray< FNiagaraDataInterfaceGPUParamInfo >& InDIParamInfo)
{
	DIParamInfo = InDIParamInfo;
}

void FNiagaraShaderScript::SetDataInterfaceParamInfo(TArray<FNiagaraDataInterfaceParamRef>& InDIParamRefs)
{
	DIParamInfo.Empty();
	for (FNiagaraDataInterfaceParamRef& DIParam : InDIParamRefs)
	{
		DIParamInfo.Add(DIParam.ParameterInfo);
	}
}

NIAGARASHADER_API  FNiagaraShader* FNiagaraShaderScript::GetShader() const
{
	check(!GIsThreadedRendering || !IsInGameThread());
	if (!GIsEditor || RenderingThreadShaderMap /*&& RenderingThreadShaderMap->IsComplete(this, true)*/)
	{
		return RenderingThreadShaderMap->GetShader<FNiagaraShader>();
	}
	return nullptr;
};

NIAGARASHADER_API  FNiagaraShader* FNiagaraShaderScript::GetShaderGameThread() const
{
	if (GameThreadShaderMap)
	{
		return GameThreadShaderMap->GetShader<FNiagaraShader>();
	}

	return nullptr;
};


void FNiagaraShaderScript::GetShaderMapIDsWithUnfinishedCompilation(TArray<int32>& ShaderMapIds)
{
	// Build an array of the shader map Id's are not finished compiling.
	if (GameThreadShaderMap && GameThreadShaderMap.IsValid() && !GameThreadShaderMap->IsCompilationFinalized())
	{
		ShaderMapIds.Add(GameThreadShaderMap->GetCompilingId());
	}
	else if (OutstandingCompileShaderMapIds.Num() != 0)
	{
		ShaderMapIds.Append(OutstandingCompileShaderMapIds);
	}
}

#if WITH_EDITOR

/**
* Compiles this script for Platform, storing the result in OutShaderMap
*
* @param ShaderMapId - the set of static parameters to compile
* @param Platform - the platform to compile for
* @param OutShaderMap - the shader map to compile
* @return - true if compile succeeded or was not necessary (shader map for ShaderMapId was found and was complete)
*/
bool FNiagaraShaderScript::BeginCompileShaderMap(
	const FNiagaraShaderMapId& ShaderMapId,
	EShaderPlatform Platform,
	TRefCountPtr<FNiagaraShaderMap>& OutShaderMap,
	bool bApplyCompletedShaderMapForRendering,
	bool bSynchronous)
{
#if WITH_EDITORONLY_DATA
	bool bSuccess = false;

	STAT(double NiagaraCompileTime = 0);


	SCOPE_SECONDS_COUNTER(NiagaraCompileTime);

	// Queue hlsl generation and shader compilation - Unlike materials, we queue this here, and compilation happens from the editor module
	TRefCountPtr<FNiagaraShaderMap> NewShaderMap = new FNiagaraShaderMap();
	OutstandingCompileShaderMapIds.AddUnique(NewShaderMap->GetCompilingId());		
	UE_LOG(LogShaders, Log, TEXT("BeginCompileShaderMap AddUnique %p %d"), this, NewShaderMap->GetCompilingId());

	FNiagaraCompilationQueue::Get()->Queue(this, NewShaderMap, ShaderMapId, Platform, bApplyCompletedShaderMapForRendering);
	if (bSynchronous)
	{
		INiagaraShaderModule NiagaraShaderModule = FModuleManager::GetModuleChecked<INiagaraShaderModule>(TEXT("NiagaraShader"));
		NiagaraShaderModule.ProcessShaderCompilationQueue();
		OutShaderMap = NewShaderMap;
	}
	else
	{
		// For async compile, set to nullptr so that we fall back to CPU side simulation until shader compile is finished
		OutShaderMap = nullptr;
	}

	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_NiagaraShaders, (float)NiagaraCompileTime);

	return true;
#else
	UE_LOG(LogShaders, Fatal, TEXT("Compiling of shaders in a build without editordata is not supported."));
	return false;
#endif
}

#endif