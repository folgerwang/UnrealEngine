// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
DebugViewModeMaterialManager.cpp : Implements debug shader management.
=============================================================================*/

#include "DebugViewModeMaterialManager.h"
#include "DebugViewModeInterface.h"
#include "SceneInterface.h"
#include "EngineModule.h"
#include "RendererInterface.h"
#include "UnrealEngine.h"

ENGINE_API bool HasMissingDebugViewModeShaders(bool bClearFlag)
{
	return false;
}

ENGINE_API bool GetDebugViewMaterial(const UMaterialInterface* InMaterialInterface, EDebugViewShaderMode InDebugViewMode, ERHIFeatureLevel::Type InFeatureLevel, const FMaterialRenderProxy*& OutMaterialRenderProxy, const FMaterial*& OutMaterial)
{
#if WITH_EDITORONLY_DATA
	return GDebugViewModeMaterialManager.GetShader_RenderThread(InMaterialInterface, InDebugViewMode, InFeatureLevel, OutMaterialRenderProxy, OutMaterial);
#else
	return false;
#endif
}

ENGINE_API void UpdateDebugViewModeShaders()
{
#if WITH_EDITORONLY_DATA
	GDebugViewModeMaterialManager.Update();
#endif
}

ENGINE_API void ClearDebugViewMaterials(UMaterialInterface* InMaterialInterface)
{
#if WITH_EDITORONLY_DATA
	GDebugViewModeMaterialManager.RemoveShaders(InMaterialInterface);
#endif
}

float GViewModeShaderTimeSlice = 0.02f;
static FAutoConsoleVariableRef CVarViewModeShaderTimeSlice(
	TEXT("r.ViewMode.ShaderTimeSlice"),
	GViewModeShaderTimeSlice,
	TEXT("Max time to allocate each frame to generate new shaders. 0 disables (default=.02"),
	ECVF_Default
);

extern double GViewModeShaderMissingTime;
extern int32 GNumViewModeShaderMissing;
double GViewModeShaderMissingTime = 0;
int32 GNumViewModeShaderMissing = 0;

#if WITH_EDITORONLY_DATA

FDebugViewModeMaterialManager GDebugViewModeMaterialManager;

// Trigger compilation of a specific viewmode shader.
void FDebugViewModeMaterialManager::AddShader(
	UMaterialInterface* MaterialInterface, 
	EDebugViewShaderMode DebugViewMode,
	EMaterialQualityLevel::Type QualityLevel, 
	ERHIFeatureLevel::Type FeatureLevel, 
	bool bSynchronousCompilation)
{
	FScopeLock SL(&CS);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_DebugViewModeMaterialManager_AddShader);

	check(MaterialInterface);

	const FMaterial* Material = MaterialInterface->GetMaterialResource(FeatureLevel);
	const FDebugViewModeInterface* DebugViewModeInterface = FDebugViewModeInterface::GetInterface(DebugViewMode);
	if (Material && DebugViewModeInterface)
	{
		if (!DebugViewModeInterface->bNeedsMaterialProperties && FDebugViewModeInterface::AllowFallbackToDefaultMaterial(Material))
		{
			MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		FMaterialKey Key(MaterialInterface, DebugViewMode, FeatureLevel);
		FMaterialInfo& Info = MaterialInfos.FindOrAdd(Key);
		if (Info.ShaderState == EShaderState::New)
		{
			MaterialKeys.FindOrAdd(MaterialInterface).Add(Key);
			Info.DebugProxy = new FDebugViewModeMaterialProxy(MaterialInterface, QualityLevel, FeatureLevel, bSynchronousCompilation, DebugViewMode);
			Info.ShaderState = EShaderState::Compiling;
		}
	}
}

void FDebugViewModeMaterialManager::Update()
{
	FScopeLock SL(&CS);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_DebugViewModeMaterialManager_Update);

	const EMaterialQualityLevel::Type QualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
	int32 NumShaderCompiling = 0;

	if (MissingShaderKeys.Num())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_DebugViewModeMaterialManager_AddMissingShaders);
		const double StartTime = FPlatformTime::Seconds();

		// Update viewport warning
		GViewModeShaderMissingTime = FApp::GetCurrentTime();
		GNumViewModeShaderMissing = MissingShaderKeys.Num();

		TSet<FMaterialKey> NextMissingShaderKeys;

		// First create a proxy for every missing shader.
		for (const FMaterialKey& Key : MissingShaderKeys)
		{
			FMaterialInfo& Info = MaterialInfos.FindChecked(Key);
			check(!Info.DebugProxy && Info.ShaderState == EShaderState::Missing);

			if (GViewModeShaderTimeSlice <= 0 || (float)(FPlatformTime::Seconds() - StartTime) < GViewModeShaderTimeSlice)
			{
				Info.DebugProxy = new FDebugViewModeMaterialProxy(const_cast<UMaterialInterface*>(Key.MaterialInterface), QualityLevel, (ERHIFeatureLevel::Type)Key.FeatureLevel, false, (EDebugViewShaderMode)Key.DebugViewMode);
				Info.ShaderState = EShaderState::Compiling;
			}
			else
			{
				NextMissingShaderKeys.Add(Key);
			}
		}
		FMemory::Memswap(&NextMissingShaderKeys, &MissingShaderKeys, sizeof(NextMissingShaderKeys));
	}

	ValidateShaders(false);
}

void FDebugViewModeMaterialManager::ValidateShaders(bool bAllShadersReady)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_DebugViewModeMaterialManager_ValidateShaders);

	// Check every shader that could be missing validation. Otherwise shader first have to be requested before their validation is done.
	if (bAllShadersReady)
	{
		FScopeLock SL(&CS);

		for (TMap<FMaterialKey, FMaterialInfo>::TIterator It(MaterialInfos); It; ++It)
		{
			FMaterialInfo& Info = It.Value();
			if (Info.DebugProxy && Info.ShaderState == EShaderState::Compiling)
			{
				PendingValidationShaderKeys.Add(It.Key());
				Info.ShaderState = EShaderState::PendingValidation;
			}
		}
	}

	TArray<FMaterialKey> KeysToUpdateUEC;
	TArray<FDebugViewModeMaterialProxy*, TInlineAllocator<4> > DebugMaterialsToDelete;
	{
		FScopeLock SL(&CS);

		for (const FMaterialKey& Key : PendingValidationShaderKeys)
		{
			check(Key.MaterialInterface);
			FMaterialInfo& Info = MaterialInfos.FindChecked(Key);
			check(Info.DebugProxy && Info.ShaderState == EShaderState::PendingValidation);

			// It doesn't look relevant anymore to ensure that the uniform expression set are compatible.
			// const FMaterial* ReferenceMaterial = Key.MaterialInterface->GetMaterialResource((ERHIFeatureLevel::Type)Key.FeatureLevel);

			if (Info.DebugProxy->IsValid() && Info.DebugProxy->GetGameThreadShaderMap())
				// && ReferenceMaterial
				// && ReferenceMaterial->GetGameThreadShaderMap() 
				// && ReferenceMaterial->GetGameThreadShaderMap()->GetUniformExpressionSet() == Info.DebugProxy->GetGameThreadShaderMap()->GetUniformExpressionSet()
			{
				KeysToUpdateUEC.Add(Key);
				Info.ShaderState = EShaderState::PendingUpdateUEC;
			}
			else
			{
				Info.DebugProxy->MarkAsInvalid();
				DebugMaterialsToDelete.Add(Info.DebugProxy);
				Info.DebugProxy = nullptr;
				Info.ShaderState = EShaderState::Invalid;
			}
		}
		PendingValidationShaderKeys.Empty();
	}

	if (KeysToUpdateUEC.Num())
	{
		ENQUEUE_RENDER_COMMAND(DebugViewModeMaterialsUpdateUEC)([KeysToUpdateUEC, this](FRHICommandList& RHICmdList)
		{
			FScopeLock SL(&CS);
			QUICK_SCOPE_CYCLE_COUNTER(STAT_DebugViewModeMaterialManager_DebugViewModeMaterialsUpdateUEC);

			for (const FMaterialKey& Key : KeysToUpdateUEC)
			{
				FMaterialInfo* Info = MaterialInfos.Find(Key);

				// Could be missing if there was a call to RemoveShaders().
				if (Info)
				{
					check(Info->DebugProxy && Info->ShaderState == EShaderState::PendingUpdateUEC);
					Info->DebugProxy->UpdateUniformExpressionCacheIfNeeded((ERHIFeatureLevel::Type)Key.FeatureLevel);
					Info->ShaderState = EShaderState::Valid;
				}
			}
		});
	}

	if (DebugMaterialsToDelete.Num())
	{
		ENQUEUE_RENDER_COMMAND(DeleteDebugViewModeMaterials)([DebugMaterialsToDelete](FRHICommandList& RHICmdList)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_DebugViewModeMaterialManager_DeleteDebugViewModeMaterials);

			for (FDebugViewModeMaterialProxy* MaterialToDelete : DebugMaterialsToDelete)
			{
				check(MaterialToDelete);
				delete MaterialToDelete;
			}
		});
	}

	if (bAllShadersReady)
	{
		FlushRenderingCommands();
	}
}

void FDebugViewModeMaterialManager::RemoveShaders(UMaterialInterface* InMaterialInterface)
{
	TArray<FDebugViewModeMaterialProxy*, TInlineAllocator<4> > DebugMaterialsToDelete;

	if (InMaterialInterface)
	{
		FScopeLock SL(&CS);
		QUICK_SCOPE_CYCLE_COUNTER(STAT_DebugViewModeMaterialManager_RemoveShaders);

		const TArray<FMaterialKey>* Keys = MaterialKeys.Find(InMaterialInterface);
		if (Keys)
		{
			for (const FMaterialKey& Key : *Keys)
			{
				FMaterialInfo Info = MaterialInfos.FindAndRemoveChecked(Key);
				if (Info.DebugProxy)
				{
					DebugMaterialsToDelete.Add(Info.DebugProxy);
				}
				if (Info.ShaderState == EShaderState::Missing)
				{
					MissingShaderKeys.Remove(Key);
				}
				else if (Info.ShaderState == EShaderState::PendingValidation)
				{
					PendingValidationShaderKeys.Remove(Key);
				}
			}
			MaterialKeys.Remove(InMaterialInterface);
		}
	}
	else // Otherwise clear everything.
	{
		FScopeLock SL(&CS);
		QUICK_SCOPE_CYCLE_COUNTER(STAT_DebugViewModeMaterialManager_RemoveAllShaders);

		for (TMap<FMaterialKey, FMaterialInfo>::TIterator It(MaterialInfos); It; ++It)
		{
			const FMaterialInfo& Info = It.Value();
			if (Info.DebugProxy)
			{
				DebugMaterialsToDelete.Add(Info.DebugProxy);
			}
		}
		MaterialInfos.Empty();
		MaterialKeys.Empty();
		MissingShaderKeys.Empty();
		PendingValidationShaderKeys.Empty();
	}
	
	if (DebugMaterialsToDelete.Num())
	{
		ENQUEUE_RENDER_COMMAND(DeleteDebugViewModeMaterials)([DebugMaterialsToDelete](FRHICommandList& RHICmdList)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_DebugViewModeMaterialManager_DeleteDebugViewModeMaterials);

			for (FDebugViewModeMaterialProxy* MaterialToDelete : DebugMaterialsToDelete)
			{
				check(MaterialToDelete);
				delete MaterialToDelete;
			}
		});
	}
}

bool FDebugViewModeMaterialManager::GetShader_RenderThread(
	const UMaterialInterface* MaterialInterface, 
	EDebugViewShaderMode DebugViewMode, 
	ERHIFeatureLevel::Type FeatureLevel, 
	const FMaterialRenderProxy*& OutMaterialRenderProxy, 
	const FMaterial*& OutMaterial)
{
	FScopeLock SL(&CS);

	FMaterialKey Key(MaterialInterface, DebugViewMode, FeatureLevel);
	FMaterialInfo& Info = MaterialInfos.FindOrAdd(Key);

	if (Info.ShaderState == EShaderState::Valid)
	{
		OutMaterialRenderProxy = Info.DebugProxy;
		OutMaterial = Info.DebugProxy;
		return true;
	}
	else if (Info.ShaderState == EShaderState::New)
	{
		MaterialKeys.FindOrAdd(MaterialInterface).Add(Key);
		MissingShaderKeys.Add(Key);
		Info.ShaderState = EShaderState::Missing;
	}
	else if (Info.ShaderState == EShaderState::Compiling && Info.DebugProxy->GetRenderingThreadShaderMap())
	{
		PendingValidationShaderKeys.Add(Key);
		Info.ShaderState = EShaderState::PendingValidation;
	}

	return false;
}

#endif // WITH_EDITORONLY_DATA
