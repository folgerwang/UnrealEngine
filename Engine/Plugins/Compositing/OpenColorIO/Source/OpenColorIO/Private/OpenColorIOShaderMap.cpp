// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOShader.h"

#include "Engine/VolumeTexture.h"
#include "OpenColorIOShared.h"
#include "OpenColorIODerivedDataVersion.h"
#include "OpenColorIOShaderCompilationManager.h"
#include "ProfilingDebugging/CookStats.h"
#include "ProfilingDebugging/DiagnosticTable.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "ShaderCompiler.h"
#include "Stats/StatsMisc.h"
#include "UObject/UObjectGlobals.h"

#if WITH_EDITOR
	#include "DerivedDataCacheInterface.h"
	#include "Interfaces/ITargetPlatformManagerModule.h"
	#include "TickableEditorObject.h"

#if WITH_OCIO
	#include "OpenColorIO/OpenColorIO.h"
#endif //WITH_OCIO

#endif


#if ENABLE_COOK_STATS
namespace OpenColorIOShaderCookStats
{
	static FCookStats::FDDCResourceUsageStats UsageStats;
	static int32 ShadersCompiled = 0;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("OpenColorIOShader.Usage"), TEXT(""));
		AddStat(TEXT("OpenColorIOShader.Misc"), FCookStatsManager::CreateKeyValueArray(
			TEXT("ShadersCompiled"), ShadersCompiled
		));
	});
}
#endif


//
// Globals
//
TMap<FOpenColorIOShaderMapId, FOpenColorIOShaderMap*> FOpenColorIOShaderMap::GIdToOpenColorIOShaderMap[SP_NumPlatforms];
TArray<FOpenColorIOShaderMap*> FOpenColorIOShaderMap::AllOpenColorIOShaderMaps;

// The Id of 0 is reserved for global shaders
uint32 FOpenColorIOShaderMap::NextCompilingId = 2;


/** 
 * Tracks FOpenColorIOTransformResource and their shader maps that are being compiled.
 * Uses a TRefCountPtr as this will be the only reference to a shader map while it is being compiled.
 */
TMap<TRefCountPtr<FOpenColorIOShaderMap>, TArray<FOpenColorIOTransformResource*> > FOpenColorIOShaderMap::OpenColorIOShaderMapsBeingCompiled;



static inline bool ShouldCacheOpenColorIOShader(const FOpenColorIOShaderType* InShaderType, EShaderPlatform InPlatform, const FOpenColorIOTransformResource* InColorTransformShader)
{
	return InShaderType->ShouldCache(InPlatform, InColorTransformShader) && InColorTransformShader->ShouldCache(InPlatform, InShaderType);
}



/** Called for every color transform shader to update the appropriate stats. */
void UpdateOpenColorIOShaderCompilingStats(const FOpenColorIOTransformResource* InShader)
{
	INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumTotalOpenColorIOShaders,1);
}


void FOpenColorIOShaderMapId::Serialize(FArchive& Ar)
{
	// You must bump OPENCOLORIO_DERIVEDDATA_VER if changing the serialization of FOpenColorIOShaderMapId.

	Ar << ShaderCodeHash;
	Ar << (int32&)FeatureLevel;
	Ar << ShaderTypeDependencies;
}

/** Hashes the color transform specific part of this shader map Id. */
void FOpenColorIOShaderMapId::GetOpenColorIOHash(FSHAHash& OutHash) const
{
	FSHA1 HashState;

	HashState.UpdateWithString(*ShaderCodeHash, ShaderCodeHash.Len());
	HashState.Update((const uint8*)&FeatureLevel, sizeof(FeatureLevel));

	HashState.Final();
	HashState.GetHash(&OutHash.Hash[0]);
}

/** 
* Tests this set against another for equality, disregarding override settings.
* 
* @param ReferenceSet	The set to compare against
* @return				true if the sets are equal
*/
bool FOpenColorIOShaderMapId::operator==(const FOpenColorIOShaderMapId& InReferenceSet) const
{
	if (  ShaderCodeHash != InReferenceSet.ShaderCodeHash
		|| FeatureLevel != InReferenceSet.FeatureLevel)
	{
		return false;
	}

	if (ShaderTypeDependencies.Num() != InReferenceSet.ShaderTypeDependencies.Num())
	{
		return false;
	}

	for (int32 ShaderIndex = 0; ShaderIndex < ShaderTypeDependencies.Num(); ShaderIndex++)
	{
		const FShaderTypeDependency& ShaderTypeDependency = ShaderTypeDependencies[ShaderIndex];

		if (ShaderTypeDependency.ShaderType != InReferenceSet.ShaderTypeDependencies[ShaderIndex].ShaderType
			|| ShaderTypeDependency.SourceHash != InReferenceSet.ShaderTypeDependencies[ShaderIndex].SourceHash)
		{
			return false;
		}
	}

	return true;
}

void FOpenColorIOShaderMapId::AppendKeyString(FString& OutKeyString) const
{
#if WITH_EDITOR
	OutKeyString += ShaderCodeHash;
	OutKeyString += TEXT("_");

	FString FeatureLevelString;
	GetFeatureLevelName(FeatureLevel, FeatureLevelString);

	TMap<const TCHAR*, FCachedUniformBufferDeclaration> ReferencedUniformBuffers;

	// Add the inputs for any shaders that are stored inline in the shader map
	for (const FShaderTypeDependency& ShaderTypeDependency : ShaderTypeDependencies)
	{
		OutKeyString += TEXT("_");
		OutKeyString += ShaderTypeDependency.ShaderType->GetName();
		OutKeyString += ShaderTypeDependency.SourceHash.ToString();
		ShaderTypeDependency.ShaderType->GetSerializationHistory().AppendKeyString(OutKeyString);

		const TMap<const TCHAR*, FCachedUniformBufferDeclaration>& ReferencedUniformBufferStructsCache = ShaderTypeDependency.ShaderType->GetReferencedUniformBufferStructsCache();

		for (TMap<const TCHAR*, FCachedUniformBufferDeclaration>::TConstIterator It(ReferencedUniformBufferStructsCache); It; ++It)
		{
			ReferencedUniformBuffers.Add(It.Key(), It.Value());
		}
	}

	{
		TArray<uint8> TempData;
		FSerializationHistory SerializationHistory;
		FMemoryWriter Ar(TempData, true);
		FShaderSaveArchive SaveArchive(Ar, SerializationHistory);

		// Save uniform buffer member info so we can detect when layout has changed
		SerializeUniformBufferInfo(SaveArchive, ReferencedUniformBuffers);

		SerializationHistory.AppendKeyString(OutKeyString);
	}
#endif //WITH_EDITOR
}

/**
 * Enqueues a compilation for a new shader of this type.
 * @param InColorTransform - The ColorTransform to link the shader with.
 */
FShaderCompileJob* FOpenColorIOShaderType::BeginCompileShader(
	uint32 InShaderMapId,
	const FOpenColorIOTransformResource* InColorTransform,
	FShaderCompilerEnvironment* InCompilationEnvironment,
	EShaderPlatform InPlatform,
	TArray<FShaderCommonCompileJob*>& OutNewJobs,
	FShaderTarget InTarget
	)
{
	FShaderCompileJob* NewJob = new FShaderCompileJob(InShaderMapId, nullptr, this, /* PermutationId = */ 0);

	NewJob->Input.SharedEnvironment = InCompilationEnvironment;
	NewJob->Input.Target = InTarget;
	NewJob->Input.ShaderFormat = LegacyShaderPlatformToShaderFormat(InPlatform);
	NewJob->Input.VirtualSourceFilePath = TEXT("/Engine/Plugins/Compositing/OpenColorIO/Shaders/Private/OpenColorIOShader.usf");
	NewJob->Input.EntryPointName = TEXT("MainPS");
	NewJob->Input.Environment.IncludeVirtualPathToContentsMap.Add(TEXT("/Engine/Generated/OpenColorIOTransformShader.ush"), InColorTransform->ShaderCode);
	UE_LOG(LogShaders, Verbose, TEXT("%s"), *InColorTransform->ShaderCode);
	
	FShaderCompilerEnvironment& ShaderEnvironment = NewJob->Input.Environment;

	UE_LOG(LogShaders, Verbose, TEXT("			%s"), GetName());
	COOK_STAT(OpenColorIOShaderCookStats::ShadersCompiled++);

	//update ColorTransform shader stats
	UpdateOpenColorIOShaderCompilingStats(InColorTransform);

	// Allow the shader type to modify the compile environment.
	SetupCompileEnvironment(InPlatform, InColorTransform, ShaderEnvironment);

	::GlobalBeginCompileShader(
		InColorTransform->GetFriendlyName(),
		nullptr,
		this,
		nullptr,//ShaderPipeline,
		TEXT("/Plugin/OpenColorIO/Private/OpenColorIOShader.usf"),
		TEXT("MainPS"),
		FShaderTarget(GetFrequency(), InPlatform),
		NewJob,
		OutNewJobs
	);

	return NewJob;
}

/**
 * Either creates a new instance of this type or returns an equivalent existing shader.
 * @param InShaderMapHash - Precomputed hash of the shader map 
 * @param InCurrentJob - Compile job that was enqueued by BeginCompileShader.
 */
FShader* FOpenColorIOShaderType::FinishCompileShader(
	const FSHAHash& InShaderMapHash,
	const FShaderCompileJob& InCurrentJob,
	const FString& InDebugDescription
	)
{
	check(InCurrentJob.bSucceeded);

	FShaderType* SpecificType = InCurrentJob.ShaderType->LimitShaderResourceToThisType() ? InCurrentJob.ShaderType : nullptr;

	// Reuse an existing resource with the same key or create a new one based on the compile output
	// This allows FShaders to share compiled bytecode and RHI shader references
	FShaderResource* Resource = FShaderResource::FindOrCreateShaderResource(InCurrentJob.Output, SpecificType, /* SpecificPermutationId = */ 0);

	// Find a shader with the same key in memory
	FShader* Shader = InCurrentJob.ShaderType->FindShaderById(FShaderId(InShaderMapHash, nullptr, nullptr, InCurrentJob.ShaderType, /* SpecificPermutationId = */ 0, InCurrentJob.Input.Target));

	// There was no shader with the same key so create a new one with the compile output, which will bind shader parameters
	if (!Shader)
	{
		const int32 PermutationId = 0;
		Shader = (*ConstructCompiledRef)(FOpenColorIOShaderType::CompiledShaderInitializerType(this, PermutationId, InCurrentJob.Output, Resource, InShaderMapHash, InDebugDescription));
		InCurrentJob.Output.ParameterMap.VerifyBindingsAreComplete(GetName(), InCurrentJob.Output.Target, InCurrentJob.VFType);
	}

	return Shader;
}

/**
 * Finds the shader map for a color transform.
 * @param InShaderMapId - The color transform id and static parameter set identifying the shader map
 * @param InPlatform - The platform to lookup for
 * @return nullptr if no cached shader map was found.
 */
FOpenColorIOShaderMap* FOpenColorIOShaderMap::FindId(const FOpenColorIOShaderMapId& InShaderMapId, EShaderPlatform InPlatform)
{
	check(!InShaderMapId.ShaderCodeHash.IsEmpty());
	return GIdToOpenColorIOShaderMap[InPlatform].FindRef(InShaderMapId);
}

void OpenColorIOShaderMapAppendKeyString(EShaderPlatform InPlatform, FString& OutKeyString)
{
#if WITH_EDITOR && WITH_OCIO
	//Keep library version in the DDC key to invalidate it once we move to a new library
	OutKeyString += TEXT("OCIOVersion");
	OutKeyString += TEXT(OCIO_VERSION);
	OutKeyString += TEXT("_");
#endif //WITH_EDITOR && WITH_OCIO
}

/** Creates a string key for the derived data cache given a shader map id. */
static FString GetOpenColorIOShaderMapKeyString(const FOpenColorIOShaderMapId& InShaderMapId, EShaderPlatform InPlatform)
{
#if WITH_EDITOR
	const FName Format = LegacyShaderPlatformToShaderFormat(InPlatform);
	FString ShaderMapKeyString = Format.ToString() + TEXT("_") + FString(FString::FromInt(GetTargetPlatformManagerRef().ShaderFormatVersion(Format))) + TEXT("_");
	OpenColorIOShaderMapAppendKeyString(InPlatform, ShaderMapKeyString);
	ShaderMapAppendKeyString(InPlatform, ShaderMapKeyString);
	InShaderMapId.AppendKeyString(ShaderMapKeyString);
	return FDerivedDataCacheInterface::BuildCacheKey(TEXT("OCIOSM"), OPENCOLORIO_DERIVEDDATA_VER, *ShaderMapKeyString);
#else
	return FString();
#endif
}

void FOpenColorIOShaderMap::LoadFromDerivedDataCache(const FOpenColorIOTransformResource* InColorTransform, const FOpenColorIOShaderMapId& InShaderMapId, EShaderPlatform InPlatform, TRefCountPtr<FOpenColorIOShaderMap>& InOutShaderMap)
{
#if WITH_EDITOR
	if (InOutShaderMap != nullptr)
	{
		check(InOutShaderMap->Platform == InPlatform);
		// If the shader map was non-NULL then it was found in memory but is incomplete, attempt to load the missing entries from memory
		InOutShaderMap->LoadMissingShadersFromMemory(InColorTransform);
	}
	else
	{
		// Shader map was not found in memory, try to load it from the DDC
		STAT(double OpenColorIOShaderDDCTime = 0);
		{
			SCOPE_SECONDS_COUNTER(OpenColorIOShaderDDCTime);
			COOK_STAT(auto Timer = OpenColorIOShaderCookStats::UsageStats.TimeSyncWork());

			TArray<uint8> CachedData;
			const FString DataKey = GetOpenColorIOShaderMapKeyString(InShaderMapId, InPlatform);

			if (GetDerivedDataCacheRef().GetSynchronous(*DataKey, CachedData))
			{
				COOK_STAT(Timer.AddHit(CachedData.Num()));
				InOutShaderMap = new FOpenColorIOShaderMap();
				FMemoryReader Ar(CachedData, true);

				// Deserialize from the cached data
				InOutShaderMap->Serialize(Ar);
				InOutShaderMap->RegisterSerializedShaders(false);

				checkSlow(InOutShaderMap->GetShaderMapId() == InShaderMapId);

				// Register in the global map
				InOutShaderMap->Register(InPlatform);
			}
			else
			{
				// We should be build the data later, and we can track that the resource was built there when we push it to the DDC.
				COOK_STAT(Timer.TrackCyclesOnly());
				InOutShaderMap = nullptr;
			}
		}
		INC_FLOAT_STAT_BY(STAT_ShaderCompiling_DDCLoading,(float)OpenColorIOShaderDDCTime);
	}
#endif
}

void FOpenColorIOShaderMap::SaveToDerivedDataCache()
{
#if WITH_EDITOR
	COOK_STAT(auto Timer = OpenColorIOShaderCookStats::UsageStats.TimeSyncWork());
	TArray<uint8> SaveData;
	FMemoryWriter Ar(SaveData, true);
	Serialize(Ar);

	GetDerivedDataCacheRef().Put(*GetOpenColorIOShaderMapKeyString(ShaderMapId, Platform), SaveData);
	COOK_STAT(Timer.AddMiss(SaveData.Num()));
#endif
}

/**
* Compiles the shaders for a color transform and caches them in this shader map.
* @param InColorTransform - The ColorTransform to compile shaders for.
* @param InShaderMapId - the color transform id and set of static parameters to compile for
* @param InPlatform - The platform to compile to
*/
void FOpenColorIOShaderMap::Compile(FOpenColorIOTransformResource* InColorTransform
									, const FOpenColorIOShaderMapId& InShaderMapId
									, TRefCountPtr<FShaderCompilerEnvironment> InCompilationEnvironment
									, const FOpenColorIOCompilationOutput& InOpenColorIOCompilationOutput
									, EShaderPlatform InPlatform
									, bool bSynchronousCompile
									, bool bApplyCompletedShaderMapForRendering)
{
	if (FPlatformProperties::RequiresCookedData())
	{
		UE_LOG(LogShaders, Fatal, TEXT("Trying to compile OpenColorIO shader %s at run-time, which is not supported on consoles!"), *InColorTransform->GetFriendlyName() );
	}
	else
	{
		// Make sure we are operating on a referenced shader map or the below Find will cause this shader map to be deleted,
		// Since it creates a temporary ref counted pointer.
		check(NumRefs > 0);
  
		// Add this shader map and to OpenColorIOShaderMapsBeingCompiled
		TArray<FOpenColorIOTransformResource*>* CorrespondingTransform = OpenColorIOShaderMapsBeingCompiled.Find(this);
  
		if (CorrespondingTransform)
		{
			check(!bSynchronousCompile);
			CorrespondingTransform->AddUnique(InColorTransform);
		}
		else
		{
			// Assign a unique identifier so that shaders from this shader map can be associated with it after a deferred compile
			CompilingId = NextCompilingId;
			UE_LOG(LogShaders, Log, TEXT("CompilingId = %p %d"), InColorTransform, CompilingId);
			InColorTransform->AddCompileId(CompilingId);

			check(NextCompilingId < UINT_MAX);
			NextCompilingId++;
  
			TArray<FOpenColorIOTransformResource*> NewCorrespondingTransforms;
			NewCorrespondingTransforms.Add(InColorTransform);
			OpenColorIOShaderMapsBeingCompiled.Add(this, NewCorrespondingTransforms);
#if DEBUG_INFINITESHADERCOMPILE
			UE_LOG(LogTemp, Display, TEXT("Added OpenColorIO ShaderMap 0x%08X%08X with ColorTransform 0x%08X%08X to OpenColorIOShaderMapsBeingCompiled"), (int)((int64)(this) >> 32), (int)((int64)(this)), (int)((int64)(InColorTransform) >> 32), (int)((int64)(InColorTransform)));
#endif  
			// Setup the compilation environment.
			InColorTransform->SetupShaderCompilationEnvironment(InPlatform, *InCompilationEnvironment);
  
			// Store the ColorTransform name for debugging purposes.
			FriendlyName = InColorTransform->GetFriendlyName();
			OpenColorIOCompilationOutput = InOpenColorIOCompilationOutput;
			ShaderMapId = InShaderMapId;
			Platform = InPlatform;

			uint32 NumShaders = 0;
			TArray<FShaderCommonCompileJob*> NewJobs;
	
			// Iterate over all shader types.
			TMap<FShaderType*, FShaderCompileJob*> SharedShaderJobs;
			for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
			{
				FOpenColorIOShaderType* ShaderType = ShaderTypeIt->GetOpenColorIOShaderType();
				if (ShaderType && ShouldCacheOpenColorIOShader(ShaderType, InPlatform, InColorTransform))
				{
					// Verify that the shader map Id contains inputs for any shaders that will be put into this shader map
					check(InShaderMapId.ContainsShaderType(ShaderType));
					
					// Compile this OpenColorIO shader .
					TArray<FString> ShaderErrors;
  
					// Only compile the shader if we don't already have it
					if (!HasShader(ShaderType, /* PermutationId = */ 0))
					{
						auto* Job = ShaderType->BeginCompileShader(
							CompilingId,
							InColorTransform,
							InCompilationEnvironment,
							InPlatform,
							NewJobs,
							FShaderTarget(ShaderType->GetFrequency(), Platform)
							);
						check(!SharedShaderJobs.Find(ShaderType));
						SharedShaderJobs.Add(ShaderType, Job);
					}
					NumShaders++;
				}
				else if (ShaderType)
				{
					UE_LOG(LogShaders, Display, TEXT("Skipping compilation of %s as it isn't supported on this target type."), *InColorTransform->GetFriendlyName());
					InColorTransform->RemoveOutstandingCompileId(CompilingId);
					InColorTransform->NotifyCompilationFinished();
				}
			}
  
			if (!CorrespondingTransform)
			{
				UE_LOG(LogShaders, Log, TEXT("		%u Shaders"), NumShaders);
			}

			// Register this shader map in the global ColorTransform->shadermap map
			Register(InPlatform);
  
			// Mark the shader map as not having been finalized with ProcessCompilationResults
			bCompilationFinalized = false;
  
			// Mark as not having been compiled
			bCompiledSuccessfully = false;
  
			GOpenColorIOShaderCompilationManager.AddJobs(NewJobs);
  
			// Compile the shaders for this shader map now if not deferring and deferred compiles are not enabled globally
			if (bSynchronousCompile)
			{
				TArray<int32> CurrentShaderMapId;
				CurrentShaderMapId.Add(CompilingId);
				GOpenColorIOShaderCompilationManager.FinishCompilation(*FriendlyName, CurrentShaderMapId);
			}
		}
	}
}

FShader* FOpenColorIOShaderMap::ProcessCompilationResultsForSingleJob(FShaderCommonCompileJob* InSingleJob, const FSHAHash& InShaderMapHash)
{
	check(InSingleJob);
	const FShaderCompileJob& CurrentJob = *((FShaderCompileJob*)InSingleJob);
	check(CurrentJob.Id == CompilingId);

	FShader* Shader = nullptr;

	FOpenColorIOShaderType* OpenColorIOShaderType = CurrentJob.ShaderType->GetOpenColorIOShaderType();
	check(OpenColorIOShaderType);
	Shader = OpenColorIOShaderType->FinishCompileShader(InShaderMapHash, CurrentJob, FriendlyName);
	bCompiledSuccessfully = CurrentJob.bSucceeded;

	FOpenColorIOPixelShader *OpenColorIOShader = static_cast<FOpenColorIOPixelShader*>(Shader);
	check(Shader);
	check(!HasShader(OpenColorIOShaderType, /* PermutationId = */ 0));
	AddShader(OpenColorIOShaderType, /* PermutationId = */ 0, Shader);

	return Shader;
}

bool FOpenColorIOShaderMap::ProcessCompilationResults(const TArray<FShaderCommonCompileJob*>& InCompilationResults, int32& InOutJobIndex, float& InOutTimeBudget)
{
	check(InOutJobIndex < InCompilationResults.Num());

	double StartTime = FPlatformTime::Seconds();

	FSHAHash ShaderMapHash;
	ShaderMapId.GetOpenColorIOHash(ShaderMapHash);

	do
	{
		FShaderCommonCompileJob* SingleJob = InCompilationResults[InOutJobIndex];
		ensure(SingleJob);

		{
			ProcessCompilationResultsForSingleJob(SingleJob, ShaderMapHash);
		}

		InOutJobIndex++;
		
		double NewStartTime = FPlatformTime::Seconds();
		InOutTimeBudget -= NewStartTime - StartTime;
		StartTime = NewStartTime;
	}
	while ((InOutTimeBudget > 0.0f) && (InOutJobIndex < InCompilationResults.Num()));

	if (InOutJobIndex == InCompilationResults.Num())
	{
		SaveToDerivedDataCache();
		// The shader map can now be used on the rendering thread
		bCompilationFinalized = true;
		return true;
	}

	return false;
}

bool FOpenColorIOShaderMap::TryToAddToExistingCompilationTask(FOpenColorIOTransformResource* InColorTransform)
{
	check(NumRefs > 0);
	TArray<FOpenColorIOTransformResource*>* CorrespondingColorTransforms = FOpenColorIOShaderMap::OpenColorIOShaderMapsBeingCompiled.Find(this);

	if (CorrespondingColorTransforms)
	{
		CorrespondingColorTransforms->AddUnique(InColorTransform);

		UE_LOG(LogShaders, Log, TEXT("TryToAddToExistingCompilationTask %p %d"), InColorTransform, GetCompilingId());

#if DEBUG_INFINITESHADERCOMPILE
		UE_LOG(LogTemp, Display, TEXT("Added shader map 0x%08X%08X from OpenColorIO transform 0x%08X%08X"), (int)((int64)(this) >> 32), (int)((int64)(this)), (int)((int64)(InColorTransform) >> 32), (int)((int64)(InColorTransform)));
#endif
		return true;
	}

	return false;
}

bool FOpenColorIOShaderMap::IsOpenColorIOShaderComplete(const FOpenColorIOTransformResource* InColorTransform, const FOpenColorIOShaderType* InShaderType, bool bSilent)
{
	// If we should cache this color transform, it's incomplete if the shader is missing
	if (ShouldCacheOpenColorIOShader(InShaderType, Platform, InColorTransform) &&	!HasShader((FShaderType*)InShaderType, /* PermutationId = */ 0))
	{
		if (!bSilent)
		{
			UE_LOG(LogShaders, Warning, TEXT("Incomplete shader %s, missing FOpenColorIOShader %s."), *InColorTransform->GetFriendlyName(), InShaderType->GetName());
		}
		return false;
	}

	return true;
}

bool FOpenColorIOShaderMap::IsComplete(const FOpenColorIOTransformResource* InColorTransform, bool bSilent)
{
	// Make sure we are operating on a referenced shader map or the below Find will cause this shader map to be deleted,
	// Since it creates a temporary ref counted pointer.
	check(NumRefs > 0);
	const TArray<FOpenColorIOTransformResource*>* CorrespondingColorTransforms = FOpenColorIOShaderMap::OpenColorIOShaderMapsBeingCompiled.Find(this);

	if (CorrespondingColorTransforms)
	{
		check(!bCompilationFinalized);
		return false;
	}

	// Iterate over all shader types.
	for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		// Find this shader type in the ColorTransform's shader map.
		const FOpenColorIOShaderType* ShaderType = ShaderTypeIt->GetOpenColorIOShaderType();
		if (ShaderType && !IsOpenColorIOShaderComplete(InColorTransform, ShaderType, bSilent))
		{
			return false;
		}
	}

	return true;
}

void FOpenColorIOShaderMap::LoadMissingShadersFromMemory(const FOpenColorIOTransformResource* InColorTransform)
{
	// Make sure we are operating on a referenced shader map or the below Find will cause this shader map to be deleted,
	// Since it creates a temporary ref counted pointer.
	check(NumRefs > 0);

	const TArray<FOpenColorIOTransformResource*>* CorrespondingColorTransforms = FOpenColorIOShaderMap::OpenColorIOShaderMapsBeingCompiled.Find(this);

	if (CorrespondingColorTransforms)
	{
		check(!bCompilationFinalized);
		return;
	}

	FSHAHash ShaderMapHash;
	ShaderMapId.GetOpenColorIOHash(ShaderMapHash);

	// Try to find necessary FOpenColorIOShaderType's in memory
	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		FOpenColorIOShaderType* ShaderType = ShaderTypeIt->GetOpenColorIOShaderType();
		if (ShaderType && ShouldCacheOpenColorIOShader(ShaderType, Platform, InColorTransform) && !HasShader(ShaderType, /* PermutationId = */ 0))
		{
			FShaderId ShaderId(ShaderMapHash, nullptr, nullptr, ShaderType, /** PermutationId = */ 0, FShaderTarget(ShaderType->GetFrequency(), Platform));
			FShader* FoundShader = ShaderType->FindShaderById(ShaderId);
			if (FoundShader)
			{
				AddShader(ShaderType, /* PermutationId = */ 0, FoundShader);
			}
		}
	}
}

void FOpenColorIOShaderMap::GetShaderList(TMap<FShaderId, FShader*>& OutShaders) const
{
	TShaderMap<FOpenColorIOShaderType>::GetShaderList(OutShaders);
}

/**
 * Registers a OpenColorIO shader map in the global map.
 */
void FOpenColorIOShaderMap::Register(EShaderPlatform InShaderPlatform)
{
	if (Platform == InShaderPlatform)
	{
		for (auto KeyValue : GetShaders())
		{
			FShader* Shader = KeyValue.Value;
			if (Shader)
			{
				Shader->BeginInitializeResources();
			}
		}
	}

	if (!bRegistered)
	{
		INC_DWORD_STAT(STAT_Shaders_NumShaderMaps);
		INC_DWORD_STAT_BY(STAT_Shaders_ShaderMapMemory, GetSizeBytes());
	}

	GIdToOpenColorIOShaderMap[Platform].Add(ShaderMapId,this);
	bRegistered = true;
}

void FOpenColorIOShaderMap::AddRef()
{
	check(!bDeletedThroughDeferredCleanup);
	++NumRefs;
}

void FOpenColorIOShaderMap::Release()
{
	check(NumRefs > 0);
	if(--NumRefs == 0)
	{
		if (bRegistered)
		{
			DEC_DWORD_STAT(STAT_Shaders_NumShaderMaps);
			DEC_DWORD_STAT_BY(STAT_Shaders_ShaderMapMemory, GetSizeBytes());

			GIdToOpenColorIOShaderMap[Platform].Remove(ShaderMapId);
			bRegistered = false;
		}

		check(!bDeletedThroughDeferredCleanup);
		bDeletedThroughDeferredCleanup = true;
		BeginCleanup(this);
	}
}

FOpenColorIOShaderMap::FOpenColorIOShaderMap() :
	TShaderMap<FOpenColorIOShaderType>(SP_NumPlatforms),
	Platform(SP_NumPlatforms),
	CompilingId(1),
	NumRefs(0),
	bDeletedThroughDeferredCleanup(false),
	bRegistered(false),
	bCompilationFinalized(true),
	bCompiledSuccessfully(true),
	bIsPersistent(true) 
{
	checkSlow(IsInGameThread() || IsAsyncLoading());
	AllOpenColorIOShaderMaps.Add(this);
}

FOpenColorIOShaderMap::~FOpenColorIOShaderMap()
{ 
	checkSlow(IsInGameThread() || IsAsyncLoading());
	check(bDeletedThroughDeferredCleanup);
	check(!bRegistered);
	AllOpenColorIOShaderMaps.RemoveSwap(this);
}

/**
 * Removes all entries in the cache with exceptions based on a shader type
 * @param ShaderType - The shader type to flush
 */
void FOpenColorIOShaderMap::FlushShadersByShaderType(FShaderType* InShaderType)
{
	if (InShaderType->GetOpenColorIOShaderType())
	{
		RemoveShaderTypePermutaion(InShaderType->GetOpenColorIOShaderType(), /* PermutationId = */ 0);	
	}
}

void FOpenColorIOShaderMap::Serialize(FArchive& Ar, bool bInlineShaderResources)
{
	// Note: This is saved to the DDC, not into packages (except when cooked)
	// Backwards compatibility therefore will not work based on the version of Ar
	// Instead, just bump OPENCOLORIO_DERIVEDDATA_VER

	ShaderMapId.Serialize(Ar);

	// serialize the platform enum as a uint8
	int32 TempPlatform = (int32)Platform;
	Ar << TempPlatform;
	Platform = (EShaderPlatform)TempPlatform;

	Ar << FriendlyName;

	OpenColorIOCompilationOutput.Serialize(Ar);

	if (Ar.IsSaving())
	{
		TShaderMap<FOpenColorIOShaderType>::SerializeInline(Ar, bInlineShaderResources, false, false);
		RegisterSerializedShaders(false);
	}

	if (Ar.IsLoading())
	{
		TShaderMap<FOpenColorIOShaderType>::SerializeInline(Ar, bInlineShaderResources, false, false);
	}
}

void FOpenColorIOShaderMap::RegisterSerializedShaders(bool bCooked)
{
	check(IsInGameThread());

	TShaderMap<FOpenColorIOShaderType>::RegisterSerializedShaders(bCooked);
}

void FOpenColorIOShaderMap::DiscardSerializedShaders()
{
	TShaderMap<FOpenColorIOShaderType>::DiscardSerializedShaders();
}

void FOpenColorIOShaderMap::RemovePendingColorTransform(FOpenColorIOTransformResource* InColorTransform)
{
	for (TMap<TRefCountPtr<FOpenColorIOShaderMap>, TArray<FOpenColorIOTransformResource*> >::TIterator It(OpenColorIOShaderMapsBeingCompiled); It; ++It)
	{
		TArray<FOpenColorIOTransformResource*>& ColorTranforms = It.Value();
		int32 Result = ColorTranforms.Remove(InColorTransform);
		if (Result)
		{
			InColorTransform->RemoveOutstandingCompileId(It.Key()->CompilingId);
			InColorTransform->NotifyCompilationFinished();
		}
#if DEBUG_INFINITESHADERCOMPILE
		if ( Result )
		{
			UE_LOG(LogTemp, Display, TEXT("Removed shader map 0x%08X%08X from color transform 0x%08X%08X"), (int)((int64)(It.Key().GetReference()) >> 32), (int)((int64)(It.Key().GetReference())), (int)((int64)(InColorTransform) >> 32), (int)((int64)(InColorTransform)));
		}
#endif
	}
}

