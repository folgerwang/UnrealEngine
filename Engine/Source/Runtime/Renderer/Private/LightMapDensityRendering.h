// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightMapDensityRendering.h: Definitions for rendering lightmap density.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "EngineGlobals.h"
#include "RHI.h"
#include "HitProxies.h"
#include "ShaderParameters.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "UnrealEngine.h"
#include "MeshMaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "SceneRendering.h"
#include "Engine/LightMapTexture2D.h"
#include "Runtime/Engine/Classes/VT/VirtualTexture.h"
#include "Runtime/Engine/Classes/VT/VirtualTextureSpace.h"

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FLightmapDensityPassUniformParameters, )
	SHADER_PARAMETER_STRUCT(FSceneTexturesUniformParameters, SceneTextures)
	SHADER_PARAMETER(FVector4, LightMapDensity)
	SHADER_PARAMETER(FVector4, DensitySelectedColor) // The color to apply to selected objects.
	SHADER_PARAMETER(FVector4, VertexMappedColor) // The color to apply to vertex mapped objects.
	SHADER_PARAMETER_TEXTURE(Texture2D, GridTexture) // The "Grid" texture to visualize resolution.
	SHADER_PARAMETER_SAMPLER(SamplerState, GridTextureSampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

template<typename LightMapPolicyType>
class TLightMapDensityElementData : public FMeshMaterialShaderElementData
{
public:
	TLightMapDensityElementData(const typename LightMapPolicyType::ElementDataType& InLightMapPolicyElementData) :
		LightMapPolicyElementData(InLightMapPolicyElementData)
	{}

	typename LightMapPolicyType::ElementDataType LightMapPolicyElementData;

	FVector BuiltLightingAndSelectedFlags;
	FVector2D LightMapResolutionScale; 
	bool bTextureMapped;
};

/**
 * The base shader type for vertex shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */
template<typename LightMapPolicyType>
class TLightMapDensityVS : public FMeshMaterialShader, public LightMapPolicyType::VertexParametersType
{
	DECLARE_SHADER_TYPE(TLightMapDensityVS,MeshMaterial);

public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return  AllowDebugViewmodes(Platform) 
				&& (Material->IsSpecialEngineMaterial() || Material->IsMasked() || Material->MaterialMayModifyMeshPosition())
				&& LightMapPolicyType::ShouldCompilePermutation(Platform,Material,VertexFactoryType)
				&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	TLightMapDensityVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		LightMapPolicyType::VertexParametersType::Bind(Initializer.ParameterMap);
		PassUniformBuffer.Bind(Initializer.ParameterMap, FLightmapDensityPassUniformParameters::StaticStructMetadata.GetShaderVariableName());
	}
	TLightMapDensityVS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		LightMapPolicyType::VertexParametersType::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const TLightMapDensityElementData<LightMapPolicyType>& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);

		LightMapPolicyType::GetVertexShaderBindings(
			PrimitiveSceneProxy,
			ShaderElementData.LightMapPolicyElementData,
			this,
			ShaderBindings);
	}
};

/**
 * The base shader type for hull shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */
template<typename LightMapPolicyType>
class TLightMapDensityHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(TLightMapDensityHS,MeshMaterial);

public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return AllowDebugViewmodes(Platform) 
			&& FBaseHS::ShouldCompilePermutation(Platform, Material, VertexFactoryType)
			&& TLightMapDensityVS<LightMapPolicyType>::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	TLightMapDensityHS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FBaseHS(Initializer)
	{}

	TLightMapDensityHS() {}
};

/**
 * The base shader type for domain shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */
template<typename LightMapPolicyType>
class TLightMapDensityDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(TLightMapDensityDS,MeshMaterial);

public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return AllowDebugViewmodes(Platform) 
			&& FBaseDS::ShouldCompilePermutation(Platform, Material, VertexFactoryType)
			&& TLightMapDensityVS<LightMapPolicyType>::ShouldCompilePermutation(Platform, Material, VertexFactoryType);		
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	TLightMapDensityDS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FBaseDS(Initializer)
	{}

	TLightMapDensityDS() {}
};

/**
 * The base type for pixel shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 * The base type is shared between the versions with and without sky light.
 */
template<typename LightMapPolicyType>
class TLightMapDensityPS : public FMeshMaterialShader, public LightMapPolicyType::PixelParametersType
{
	DECLARE_SHADER_TYPE(TLightMapDensityPS,MeshMaterial);

public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return	AllowDebugViewmodes(Platform) 
				&& (Material->IsSpecialEngineMaterial() || Material->IsMasked() || Material->MaterialMayModifyMeshPosition())
				&& LightMapPolicyType::ShouldCompilePermutation(Platform,Material,VertexFactoryType)
				&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	TLightMapDensityPS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		LightMapPolicyType::PixelParametersType::Bind(Initializer.ParameterMap);
		BuiltLightingAndSelectedFlags.Bind(Initializer.ParameterMap,TEXT("BuiltLightingAndSelectedFlags"));
		LightMapResolutionScale.Bind(Initializer.ParameterMap,TEXT("LightMapResolutionScale"));
		LightMapDensityDisplayOptions.Bind(Initializer.ParameterMap,TEXT("LightMapDensityDisplayOptions"));
		PassUniformBuffer.Bind(Initializer.ParameterMap, FLightmapDensityPassUniformParameters::StaticStructMetadata.GetShaderVariableName());
	}
	TLightMapDensityPS() {}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const TLightMapDensityElementData<LightMapPolicyType>& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);

		LightMapPolicyType::GetPixelShaderBindings(
			PrimitiveSceneProxy,
			ShaderElementData.LightMapPolicyElementData,
			this,
			ShaderBindings);

		ShaderBindings.Add(BuiltLightingAndSelectedFlags, ShaderElementData.BuiltLightingAndSelectedFlags);
		ShaderBindings.Add(LightMapResolutionScale, ShaderElementData.LightMapResolutionScale);

		FVector4 OptionsParameter(
			GEngine->bRenderLightMapDensityGrayscale ? GEngine->RenderLightMapDensityGrayscaleScale : 0.0f,
			GEngine->bRenderLightMapDensityGrayscale ? 0.0f : GEngine->RenderLightMapDensityColorScale,
			(ShaderElementData.bTextureMapped == true) ? 1.0f : 0.0f,
			(ShaderElementData.bTextureMapped == false) ? 1.0f : 0.0f
			);
		ShaderBindings.Add(LightMapDensityDisplayOptions, OptionsParameter);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		LightMapPolicyType::PixelParametersType::Serialize(Ar);
		Ar << BuiltLightingAndSelectedFlags;
		Ar << LightMapResolutionScale;
		Ar << LightMapDensityDisplayOptions;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter BuiltLightingAndSelectedFlags;
	FShaderParameter LightMapResolutionScale;
	FShaderParameter LightMapDensityDisplayOptions;
};


class FLightmapDensityMeshProcessor : public FMeshPassProcessor
{
public:

	FLightmapDensityMeshProcessor(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;


private:

	template<typename LightMapPolicyType>
	void Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		const LightMapPolicyType& RESTRICT LightMapPolicy,
		const typename LightMapPolicyType::ElementDataType& RESTRICT LightMapElementData,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);

	FMeshPassProcessorRenderState PassDrawRenderState;
};