// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
DebugViewModeMaterialManager.h : Implements debug shader management.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "DebugViewModeMaterialProxy.h"

#if WITH_EDITORONLY_DATA

class FDebugViewModeMaterialManager
{
public:

	// Trigger compilation of a specific viewmode shader.
	void AddShader(
		UMaterialInterface* MaterialInterface, 
		EDebugViewShaderMode DebugViewMode,
		EMaterialQualityLevel::Type QualityLevel, 
		ERHIFeatureLevel::Type FeatureLevel, 
		bool bSynchronousCompilation);

	// Gamethread update that trigger missing shader compilation and validate shader that are ready to be used.
	void Update();

	// Validate shaders refering to a specific material (or all shaders that could be pending).
	void ValidateShaders(bool bAllShadersReady);

	// Clear shaders refering to a specific material (or all shaders if null).
	void RemoveShaders(UMaterialInterface* InMaterialInterface = nullptr);

	// Get a shader to be used for rendering.
	bool GetShader_RenderThread(
		const UMaterialInterface* MaterialInterface, 
		EDebugViewShaderMode DebugViewMode, 
		ERHIFeatureLevel::Type FeatureLevel, 
		const FMaterialRenderProxy*& OutMaterialRenderProxy, 
		const FMaterial*& OutMaterial);

private:

	struct FMaterialKey
	{
		FMaterialKey(const UMaterialInterface* InMaterialInterface, EDebugViewShaderMode InDebugViewMode, ERHIFeatureLevel::Type InFeatureLevel) : MaterialInterface(InMaterialInterface), DebugViewMode((uint32)InDebugViewMode), FeatureLevel((uint32)InFeatureLevel) {}

		const UMaterialInterface* MaterialInterface;
		uint32 DebugViewMode;
		uint32 FeatureLevel;

		friend bool operator==(const FMaterialKey& Lhs, const FMaterialKey& Rhs)
		{
			return Lhs.MaterialInterface == Rhs.MaterialInterface
				&& Lhs.DebugViewMode == Rhs.DebugViewMode
				&& Lhs.FeatureLevel == Rhs.FeatureLevel;
		}

		friend uint32 GetTypeHash(const FMaterialKey& Key)
		{
			const uint32 DebugViewModeShift = 6;
			static_assert((2 ^ DebugViewModeShift) < DVSM_MAX, "Bit shift too small!");

			return GetTypeHash(Key.MaterialInterface) ^ GetTypeHash((Key.FeatureLevel << DebugViewModeShift) ^ Key.DebugViewMode);
		}
	};

	enum class EShaderState
	{
		New,				// Entry was just created using default constructor.
		Missing,			// Entry was just created and we will create the debug proxy in the next call to update (has an entry in MissingShaderKeys).
		Compiling,			// Debug proxy was created and compilation shader is not finished.
		PendingValidation,	// Shader compilation is finished but can't be used before validation.
		PendingUpdateUEC,	// Valid but material needs a call to UpdateUniformExpressionCacheIfNeeded before being used in rendering.
		Valid,				// Shader can be used.
		Invalid,			// Shader validation has failed, don't reattempt compilation or use debug proxy.
	};

	struct FMaterialInfo
	{
		// The debug proxy created to generate the shader. If null then shader compilation has not started or been validated.
		FDebugViewModeMaterialProxy* DebugProxy = nullptr;
		EShaderState ShaderState = EShaderState::New;
	};

	// A critical section used to control access of the shader maps.
	FCriticalSection CS;

	// List of debug shader we have generated. Once the shader is compiled, it needs to be validated before being used.
	// If the validation fails, we keep an entry in the map to prevent attemption a second recompile.
	TMap<FMaterialKey, FMaterialInfo> MaterialInfos;

	// List of all keys used for a shader, used to speed up RemoveShaders()
	TMap<const UMaterialInterface*, TArray<FMaterialKey>> MaterialKeys;

	// List of all entries in EShaderState::Missing state.
	TSet<FMaterialKey> MissingShaderKeys;

	// List of all entries in EShaderState::PendingValidation state.
	TSet<FMaterialKey> PendingValidationShaderKeys;
};

extern FDebugViewModeMaterialManager GDebugViewModeMaterialManager;

#endif // WITH_EDITORONLY_DATA
