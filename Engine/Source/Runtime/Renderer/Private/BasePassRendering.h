// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BasePassRendering.h: Base pass rendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "HitProxies.h"
#include "RHIStaticStates.h"
#include "SceneManagement.h"
#include "Materials/Material.h"
#include "DrawingPolicy.h"
#include "PostProcess/SceneRenderTargets.h"
#include "LightMapRendering.h"
#include "VelocityRendering.h"
#include "MeshMaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "DebugViewModeRendering.h"
#include "FogRendering.h"
#include "PlanarReflectionRendering.h"
#include "UnrealEngine.h"
#include "ReflectionEnvironment.h"

class FScene;

template<typename TBufferStruct> class TUniformBufferRef;

class FViewInfo;

/** Whether to allow the indirect lighting cache to be applied to dynamic objects. */
extern int32 GIndirectLightingCache;

/** Whether some GBuffer targets are optional. */
extern bool UseSelectiveBasePassOutputs();

class FForwardLocalLightData
{
public:
	FVector4 LightPositionAndInvRadius;
	FVector4 LightColorAndFalloffExponent;
	FVector4 LightDirectionAndShadowMapChannelMask;
	FVector4 SpotAnglesAndSourceRadiusPacked;
	FVector4 LightTangentAndSoftSourceRadius;
};

BEGIN_UNIFORM_BUFFER_STRUCT(FSharedBasePassUniformParameters,)
	UNIFORM_MEMBER_STRUCT(FForwardLightData, Forward)
	UNIFORM_MEMBER_STRUCT(FForwardLightData, ForwardISR)
	UNIFORM_MEMBER_STRUCT(FReflectionUniformParameters, Reflection)
	UNIFORM_MEMBER_STRUCT(FFogUniformParameters, Fog)
	UNIFORM_MEMBER_TEXTURE(Texture2D, SSProfilesTexture)
END_UNIFORM_BUFFER_STRUCT(FSharedBasePassUniformParameters)

BEGIN_UNIFORM_BUFFER_STRUCT(FOpaqueBasePassUniformParameters,)
	UNIFORM_MEMBER_STRUCT(FSharedBasePassUniformParameters, Shared)
	// Forward shading 
	UNIFORM_MEMBER_TEXTURE(Texture2D, ForwardScreenSpaceShadowMaskTexture)
	UNIFORM_MEMBER_TEXTURE(Texture2D, IndirectOcclusionTexture)
	UNIFORM_MEMBER_TEXTURE(Texture2D, ResolvedSceneDepthTexture)
	// DBuffer decals
	UNIFORM_MEMBER_TEXTURE(Texture2D, DBufferATexture)
	UNIFORM_MEMBER_SAMPLER(SamplerState, DBufferATextureSampler)
	UNIFORM_MEMBER_TEXTURE(Texture2D, DBufferBTexture)
	UNIFORM_MEMBER_SAMPLER(SamplerState, DBufferBTextureSampler)
	UNIFORM_MEMBER_TEXTURE(Texture2D, DBufferCTexture)
	UNIFORM_MEMBER_SAMPLER(SamplerState, DBufferCTextureSampler)
	UNIFORM_MEMBER_TEXTURE(Texture2D<uint>, DBufferRenderMask)
	// Misc
	UNIFORM_MEMBER_TEXTURE(Texture2D, EyeAdaptation)
END_UNIFORM_BUFFER_STRUCT(FOpaqueBasePassUniformParameters)

BEGIN_UNIFORM_BUFFER_STRUCT(FTranslucentBasePassUniformParameters,)
	UNIFORM_MEMBER_STRUCT(FSharedBasePassUniformParameters, Shared)
	UNIFORM_MEMBER_STRUCT(FSceneTexturesUniformParameters, SceneTextures)
	// Material SSR
	UNIFORM_MEMBER(FVector4, HZBUvFactorAndInvFactor)
	UNIFORM_MEMBER(FVector4, PrevScreenPositionScaleBias)
	UNIFORM_MEMBER(float, PrevSceneColorPreExposureInv)
	UNIFORM_MEMBER_TEXTURE(Texture2D, HZBTexture)
	UNIFORM_MEMBER_SAMPLER(SamplerState, HZBSampler)
	UNIFORM_MEMBER_TEXTURE(Texture2D, PrevSceneColor)
	UNIFORM_MEMBER_SAMPLER(SamplerState, PrevSceneColorSampler)
	// Translucency Lighting Volume
	UNIFORM_MEMBER_TEXTURE(Texture3D, TranslucencyLightingVolumeAmbientInner)
	UNIFORM_MEMBER_SAMPLER(SamplerState, TranslucencyLightingVolumeAmbientInnerSampler)
	UNIFORM_MEMBER_TEXTURE(Texture3D, TranslucencyLightingVolumeAmbientOuter)
	UNIFORM_MEMBER_SAMPLER(SamplerState, TranslucencyLightingVolumeAmbientOuterSampler)
	UNIFORM_MEMBER_TEXTURE(Texture3D, TranslucencyLightingVolumeDirectionalInner)
	UNIFORM_MEMBER_SAMPLER(SamplerState, TranslucencyLightingVolumeDirectionalInnerSampler)
	UNIFORM_MEMBER_TEXTURE(Texture3D, TranslucencyLightingVolumeDirectionalOuter)
	UNIFORM_MEMBER_SAMPLER(SamplerState, TranslucencyLightingVolumeDirectionalOuterSampler)
END_UNIFORM_BUFFER_STRUCT(FTranslucentBasePassUniformParameters)

extern FTextureRHIRef& GetEyeAdaptation(const FViewInfo& View);

extern void SetupSharedBasePassParameters(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	FSceneRenderTargets& SceneRenderTargets,
	FSharedBasePassUniformParameters& BasePassParameters);

extern void CreateOpaqueBasePassUniformBuffer(
	FRHICommandListImmediate& RHICmdList, 
	const FViewInfo& View,
	IPooledRenderTarget* ForwardScreenSpaceShadowMask, 
	TUniformBufferRef<FOpaqueBasePassUniformParameters>& BasePassUniformBuffer);

extern void CreateTranslucentBasePassUniformBuffer(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	IPooledRenderTarget* SceneColorCopy,
	ESceneTextureSetupMode SceneTextureSetupMode,
	TUniformBufferRef<class FTranslucentBasePassUniformParameters>& BasePassUniformBuffer,
	const int32 ViewIndex);

/** Parameters for computing forward lighting. */
class FForwardLightingParameters
{
public:

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("LOCAL_LIGHT_DATA_STRIDE"), FMath::DivideAndRoundUp<int32>(sizeof(FForwardLocalLightData), sizeof(FVector4)));
		extern int32 NumCulledLightsGridStride;
		OutEnvironment.SetDefine(TEXT("NUM_CULLED_LIGHTS_GRID_STRIDE"), NumCulledLightsGridStride);
		extern int32 NumCulledGridPrimitiveTypes;
		OutEnvironment.SetDefine(TEXT("NUM_CULLED_GRID_PRIMITIVE_TYPES"), NumCulledGridPrimitiveTypes);
	}
};

inline void BindBasePassUniformBuffer(const FShaderParameterMap& ParameterMap, FShaderUniformBufferParameter& BasePassUniformBuffer)
{
	TArray<const FUniformBufferStruct*> NestedStructs;
	FOpaqueBasePassUniformParameters::StaticStruct.GetNestedStructs(NestedStructs);
	FTranslucentBasePassUniformParameters::StaticStruct.GetNestedStructs(NestedStructs);

	for (int32 StructIndex = 0; StructIndex < NestedStructs.Num(); StructIndex++)
	{
		const TCHAR* StructVariableName = NestedStructs[StructIndex]->GetShaderVariableName();
		checkfSlow(!ParameterMap.ContainsParameterAllocation(StructVariableName), TEXT("%s found bound in the base pass.  Base Pass uniform buffer nested structs should not be bound separately"), StructVariableName);
	}
	
	const bool bNeedsOpaqueBasePass = ParameterMap.ContainsParameterAllocation(FOpaqueBasePassUniformParameters::StaticStruct.GetShaderVariableName());
	const bool bNeedsTransparentBasePass = ParameterMap.ContainsParameterAllocation(FTranslucentBasePassUniformParameters::StaticStruct.GetShaderVariableName());

	checkSlow(!(bNeedsOpaqueBasePass && bNeedsTransparentBasePass));

	BasePassUniformBuffer.Bind(ParameterMap, FOpaqueBasePassUniformParameters::StaticStruct.GetShaderVariableName());

	if (!BasePassUniformBuffer.IsBound())
	{
		BasePassUniformBuffer.Bind(ParameterMap, FTranslucentBasePassUniformParameters::StaticStruct.GetShaderVariableName());
	}
}

/**
 * The base shader type for vertex shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 * The base type is shared between the versions with and without atmospheric fog.
 */
template<typename VertexParametersType>
class TBasePassVertexShaderPolicyParamType : public FMeshMaterialShader, public VertexParametersType
{
protected:

	TBasePassVertexShaderPolicyParamType() {}
	TBasePassVertexShaderPolicyParamType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		VertexParametersType::Bind(Initializer.ParameterMap);
		BindBasePassUniformBuffer(Initializer.ParameterMap, PassUniformBuffer);
		ReflectionCaptureBuffer.Bind(Initializer.ParameterMap, TEXT("ReflectionCapture"));
		PreviousLocalToWorldParameter.Bind(Initializer.ParameterMap, TEXT("PreviousLocalToWorld"));
		//@todo-rco: Move to pixel shader
		SkipOutputVelocityParameter.Bind(Initializer.ParameterMap, TEXT("SkipOutputVelocity"));

		InstancedEyeIndexParameter.Bind(Initializer.ParameterMap, TEXT("InstancedEyeIndex"));
		IsInstancedStereoParameter.Bind(Initializer.ParameterMap, TEXT("bIsInstancedStereo"));
	}

public:

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		FForwardLightingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		VertexParametersType::Serialize(Ar);
		Ar << ReflectionCaptureBuffer;
		Ar << PreviousLocalToWorldParameter;
		Ar << SkipOutputVelocityParameter;
		Ar << InstancedEyeIndexParameter;
		Ar << IsInstancedStereoParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FVertexFactory* VertexFactory,
		const FMaterial& InMaterialResource,
		const FViewInfo& View,
		const FDrawingPolicyRenderState& DrawRenderState,
		bool bIsInstancedStereo
		)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FMeshMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialRenderProxy, InMaterialResource, View, DrawRenderState.GetViewUniformBuffer(), DrawRenderState.GetPassUniformBuffer());

		SetUniformBufferParameter(RHICmdList, ShaderRHI, ReflectionCaptureBuffer, View.ReflectionCaptureUniformBuffer);

		if (IsInstancedStereoParameter.IsBound())
		{
			SetShaderValue(RHICmdList, ShaderRHI, IsInstancedStereoParameter, bIsInstancedStereo);
		}

		if (InstancedEyeIndexParameter.IsBound())
		{
			SetShaderValue(RHICmdList, ShaderRHI, InstancedEyeIndexParameter, 0);
		}
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy, const FMeshBatch& Mesh, const FMeshBatchElement& BatchElement, const FDrawingPolicyRenderState& DrawRenderState);

	void SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex);

private:
	
	FShaderUniformBufferParameter ReflectionCaptureBuffer;
	// When outputting from base pass, the previous transform
	FShaderParameter PreviousLocalToWorldParameter;
	FShaderParameter SkipOutputVelocityParameter;
	FShaderParameter InstancedEyeIndexParameter;
	FShaderParameter IsInstancedStereoParameter;
};




/**
 * The base shader type for vertex shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 * The base type is shared between the versions with and without atmospheric fog.
 */

template<typename LightMapPolicyType>
class TBasePassVertexShaderBaseType : public TBasePassVertexShaderPolicyParamType<typename LightMapPolicyType::VertexParametersType>
{
	typedef TBasePassVertexShaderPolicyParamType<typename LightMapPolicyType::VertexParametersType> Super;

protected:

	TBasePassVertexShaderBaseType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : Super(Initializer) {}

	TBasePassVertexShaderBaseType() {}

public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return LightMapPolicyType::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
};

template<typename LightMapPolicyType, bool bEnableAtmosphericFog>
class TBasePassVS : public TBasePassVertexShaderBaseType<LightMapPolicyType>
{
	DECLARE_SHADER_TYPE(TBasePassVS,MeshMaterial);
	typedef TBasePassVertexShaderBaseType<LightMapPolicyType> Super;

protected:

	TBasePassVS() {}
	TBasePassVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		Super(Initializer)
	{
	}

public:
	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static const auto SupportAtmosphericFog = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportAtmosphericFog"));
		static const auto SupportAllShaderPermutations = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportAllShaderPermutations"));
		const bool bForceAllPermutations = SupportAllShaderPermutations && SupportAllShaderPermutations->GetValueOnAnyThread() != 0;

		const bool bProjectAllowsAtmosphericFog = !SupportAtmosphericFog || SupportAtmosphericFog->GetValueOnAnyThread() != 0 || bForceAllPermutations;

		bool bShouldCache = Super::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
		bShouldCache &= (bEnableAtmosphericFog && bProjectAllowsAtmosphericFog && IsTranslucentBlendMode(Material->GetBlendMode())) || !bEnableAtmosphericFog;

		return bShouldCache
			&& (IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4));
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		// @todo MetalMRT: Remove this hack and implement proper atmospheric-fog solution for Metal MRT...
		OutEnvironment.SetDefine(TEXT("BASEPASS_ATMOSPHERIC_FOG"), (Platform != SP_METAL_MRT && Platform != SP_METAL_MRT_MAC) ? bEnableAtmosphericFog : 0);
	}
};

/**
 * The base shader type for hull shaders.
 */
template<typename LightMapPolicyType, bool bEnableAtmosphericFog>
class TBasePassHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(TBasePassHS,MeshMaterial);

protected:

	TBasePassHS() {}

	TBasePassHS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FBaseHS(Initializer)
	{
		BindBasePassUniformBuffer(Initializer.ParameterMap, PassUniformBuffer);
	}

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Re-use vertex shader gating
		// Metal requires matching permutations, but no other platform should worry about this complication.
		return (bEnableAtmosphericFog == false || IsMetalPlatform(Platform))
			&& FBaseHS::ShouldCompilePermutation(Platform, Material, VertexFactoryType)
			&& TBasePassVS<LightMapPolicyType,bEnableAtmosphericFog>::ShouldCompilePermutation(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Re-use vertex shader compilation environment
		TBasePassVS<LightMapPolicyType,bEnableAtmosphericFog>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	// Don't implement : void SetParameters(...) or SetMesh() unless changing the shader reference in TBasePassDrawingPolicy
};

/**
 * The base shader type for Domain shaders.
 */
template<typename LightMapPolicyType>
class TBasePassDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(TBasePassDS,MeshMaterial);

protected:

	TBasePassDS() {}

	TBasePassDS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FBaseDS(Initializer)
	{
		BindBasePassUniformBuffer(Initializer.ParameterMap, PassUniformBuffer);
	}

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Re-use vertex shader gating
		return FBaseDS::ShouldCompilePermutation(Platform, Material, VertexFactoryType)
			&& TBasePassVS<LightMapPolicyType,false>::ShouldCompilePermutation(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Re-use vertex shader compilation environment
		TBasePassVS<LightMapPolicyType,false>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

public:
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FBaseDS::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	// Don't implement : void SetParameters(...) or SetMesh() unless changing the shader reference in TBasePassDrawingPolicy
};

/** Parameters needed for reflections, shared by multiple shaders. */
class FBasePassReflectionParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		PlanarReflectionParameters.Bind(ParameterMap);
		SingleCubemapArrayIndex.Bind(ParameterMap, TEXT("SingleCubemapArrayIndex"));	
		SingleCaptureOffsetAndAverageBrightness.Bind(ParameterMap, TEXT("SingleCaptureOffsetAndAverageBrightness"));
		SingleCapturePositionAndRadius.Bind(ParameterMap, TEXT("SingleCapturePositionAndRadius"));
		SingleCaptureBrightness.Bind(ParameterMap, TEXT("SingleCaptureBrightness"));
	}

	void SetMesh(FRHICommandList& RHICmdList, FPixelShaderRHIParamRef PixelShaderRH, const FSceneView& View, const FPrimitiveSceneProxy* Proxy, ERHIFeatureLevel::Type FeatureLevel);

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FBasePassReflectionParameters& P)
	{
		Ar << P.PlanarReflectionParameters;
		Ar << P.SingleCubemapArrayIndex;
		Ar << P.SingleCaptureOffsetAndAverageBrightness;
		Ar << P.SingleCapturePositionAndRadius;
		Ar << P.SingleCaptureBrightness;
		return Ar;
	}

private:

	FPlanarReflectionParameters PlanarReflectionParameters;

	FShaderParameter SingleCubemapArrayIndex;
	FShaderParameter SingleCaptureOffsetAndAverageBrightness;
	FShaderParameter SingleCapturePositionAndRadius;
	FShaderParameter SingleCaptureBrightness;
};

/**
 * The base type for pixel shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 * The base type is shared between the versions with and without sky light.
 */
template<typename PixelParametersType>
class TBasePassPixelShaderPolicyParamType : public FMeshMaterialShader, public PixelParametersType
{
public:

	// static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);

		const bool bOutputVelocity = FVelocityRendering::BasePassCanOutputVelocity(Platform);
		if (bOutputVelocity)
		{
			const int32 VelocityIndex = 4; // As defined in BasePassPixelShader.usf
			OutEnvironment.SetRenderTargetOutputFormat(VelocityIndex, PF_G16R16);
		}

		FForwardLightingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	static bool ValidateCompiledResult(EShaderPlatform Platform, const TArray<FMaterial*>& Materials, const FVertexFactoryType* VertexFactoryType, const FShaderParameterMap& ParameterMap, TArray<FString>& OutError)
	{
		if (ParameterMap.ContainsParameterAllocation(FSceneTexturesUniformParameters::StaticStruct.GetShaderVariableName()))
		{
			OutError.Add(TEXT("Base pass shaders cannot read from the SceneTexturesStruct."));
			return false;
		}

		return true;
	}

	/** Initialization constructor. */
	TBasePassPixelShaderPolicyParamType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		PixelParametersType::Bind(Initializer.ParameterMap);
		BindBasePassUniformBuffer(Initializer.ParameterMap, PassUniformBuffer);
		ReflectionParameters.Bind(Initializer.ParameterMap);
		ReflectionCaptureBuffer.Bind(Initializer.ParameterMap, TEXT("ReflectionCapture"));

		// These parameters should only be used nested in the base pass uniform buffer
		check(!Initializer.ParameterMap.ContainsParameterAllocation(FFogUniformParameters::StaticStruct.GetShaderVariableName()));
		check(!Initializer.ParameterMap.ContainsParameterAllocation(FReflectionUniformParameters::StaticStruct.GetShaderVariableName()));
	}
	TBasePassPixelShaderPolicyParamType() {}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy, 
		const FMaterial& MaterialResource, 
		const FViewInfo* View, 
		const FDrawingPolicyRenderState& DrawRenderState,
		EBlendMode BlendMode)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FMeshMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialRenderProxy, MaterialResource, *View, DrawRenderState.GetViewUniformBuffer(), DrawRenderState.GetPassUniformBuffer());

		SetUniformBufferParameter(RHICmdList, ShaderRHI, ReflectionCaptureBuffer, View->ReflectionCaptureUniformBuffer);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement, const FDrawingPolicyRenderState& DrawRenderState, EBlendMode BlendMode);

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		PixelParametersType::Serialize(Ar);
		Ar << ReflectionParameters;
		Ar << ReflectionCaptureBuffer;
		return bShaderHasOutdatedParameters;
	}

private:
	FBasePassReflectionParameters ReflectionParameters;
	FShaderUniformBufferParameter ReflectionCaptureBuffer;
};

/**
 * The base type for pixel shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 * The base type is shared between the versions with and without sky light.
 */
template<typename LightMapPolicyType>
class TBasePassPixelShaderBaseType : public TBasePassPixelShaderPolicyParamType<typename LightMapPolicyType::PixelParametersType>
{
	typedef TBasePassPixelShaderPolicyParamType<typename LightMapPolicyType::PixelParametersType> Super;

public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return LightMapPolicyType::ShouldCompilePermutation(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	TBasePassPixelShaderBaseType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : Super(Initializer) {}

	TBasePassPixelShaderBaseType() {}
};

/** The concrete base pass pixel shader type. */
template<typename LightMapPolicyType, bool bEnableSkyLight>
class TBasePassPS : public TBasePassPixelShaderBaseType<LightMapPolicyType>
{
	DECLARE_SHADER_TYPE(TBasePassPS,MeshMaterial);
public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Only compile skylight version for lit materials, and if the project allows them.
		static const auto SupportStationarySkylight = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportStationarySkylight"));
		static const auto SupportAllShaderPermutations = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportAllShaderPermutations"));

		const bool bTranslucent = IsTranslucentBlendMode(Material->GetBlendMode());
		const bool bForceAllPermutations = SupportAllShaderPermutations && SupportAllShaderPermutations->GetValueOnAnyThread() != 0;
		const bool bProjectSupportsStationarySkylight = !SupportStationarySkylight || SupportStationarySkylight->GetValueOnAnyThread() != 0 || bForceAllPermutations;

		const bool bCacheShaders = !bEnableSkyLight
			//translucent materials need to compile skylight support to support MOVABLE skylights also.
			|| bTranslucent
			// Some lightmap policies (eg Simple Forward) always require skylight support
			|| LightMapPolicyType::RequiresSkylight()
			|| ((bProjectSupportsStationarySkylight || IsForwardShadingEnabled(Platform)) && (Material->GetShadingModel() != MSM_Unlit));
		return bCacheShaders
			&& (IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4))
			&& TBasePassPixelShaderBaseType<LightMapPolicyType>::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		// For deferred decals, the shader class used is FDeferredDecalPS. the TBasePassPS is only used in the material editor and will read wrong values.
		OutEnvironment.SetDefine(TEXT("SCENE_TEXTURES_DISABLED"), Material->GetMaterialDomain() != MD_Surface); 

		OutEnvironment.SetDefine(TEXT("ENABLE_SKY_LIGHT"), bEnableSkyLight);
		TBasePassPixelShaderBaseType<LightMapPolicyType>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
	
	/** Initialization constructor. */
	TBasePassPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		TBasePassPixelShaderBaseType<LightMapPolicyType>(Initializer)
	{}

	/** Default constructor. */
	TBasePassPS() {}
};

/**
 * Get shader templates allowing to redirect between compatible shaders.
 */

template <typename LightMapPolicyType>
void GetBasePassShaders(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	LightMapPolicyType LightMapPolicy, 
	bool bNeedsHSDS,
	bool bEnableAtmosphericFog,
	bool bEnableSkyLight,
	FBaseHS*& HullShader,
	FBaseDS*& DomainShader,
	TBasePassVertexShaderPolicyParamType<typename LightMapPolicyType::VertexParametersType>*& VertexShader,
	TBasePassPixelShaderPolicyParamType<typename LightMapPolicyType::PixelParametersType>*& PixelShader
	)
{
	if (bNeedsHSDS)
	{
		DomainShader = Material.GetShader<TBasePassDS<LightMapPolicyType > >(VertexFactoryType);
		
		// Metal requires matching permutations, but no other platform should worry about this complication.
		if (bEnableAtmosphericFog && DomainShader && IsMetalPlatform(EShaderPlatform(DomainShader->GetTarget().Platform)))
		{
			HullShader = Material.GetShader<TBasePassHS<LightMapPolicyType, true > >(VertexFactoryType);
		}
		else
		{
			HullShader = Material.GetShader<TBasePassHS<LightMapPolicyType, false > >(VertexFactoryType);
		}
	}

	if (bEnableAtmosphericFog)
	{
		VertexShader = Material.GetShader<TBasePassVS<LightMapPolicyType, true> >(VertexFactoryType);
	}
	else
	{
		VertexShader = Material.GetShader<TBasePassVS<LightMapPolicyType, false> >(VertexFactoryType);
	}
	if (bEnableSkyLight)
	{
		PixelShader = Material.GetShader<TBasePassPS<LightMapPolicyType, true> >(VertexFactoryType);
	}
	else
	{
		PixelShader = Material.GetShader<TBasePassPS<LightMapPolicyType, false> >(VertexFactoryType);
	}
}

template <>
void GetBasePassShaders<FUniformLightMapPolicy>(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	FUniformLightMapPolicy LightMapPolicy, 
	bool bNeedsHSDS,
	bool bEnableAtmosphericFog,
	bool bEnableSkyLight,
	FBaseHS*& HullShader,
	FBaseDS*& DomainShader,
	TBasePassVertexShaderPolicyParamType<FUniformLightMapPolicyShaderParametersType>*& VertexShader,
	TBasePassPixelShaderPolicyParamType<FUniformLightMapPolicyShaderParametersType>*& PixelShader
	);

class FBasePassDrawingPolicy : public FMeshDrawingPolicy
{
public:
	FBasePassDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy, 
		const FMaterial& InMaterialResource, 
		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
		EDebugViewShaderMode InDebugViewShaderMode,
		bool bInEnableReceiveDecalOutput) : FMeshDrawingPolicy(InVertexFactory, InMaterialRenderProxy, InMaterialResource, InOverrideSettings, InDebugViewShaderMode), bEnableReceiveDecalOutput(bInEnableReceiveDecalOutput)
	{}

	void ApplyDitheredLODTransitionState(FDrawingPolicyRenderState& DrawRenderState, const FViewInfo& ViewInfo, const FStaticMesh& Mesh, const bool InAllowStencilDither);

protected:

	/** Whether or not outputing the receive decal boolean */
	uint32 bEnableReceiveDecalOutput : 1;
};

/**
 * Draws the emissive color and the light-map of a mesh.
 */
template<typename LightMapPolicyType>
class TBasePassDrawingPolicy : public FBasePassDrawingPolicy
{
public:

	/** The data the drawing policy uses for each mesh element. */
	class ElementDataType
	{
	public:

		/** The element's light-map data. */
		typename LightMapPolicyType::ElementDataType LightMapElementData;

		/** Default constructor. */
		ElementDataType()
		{}

		/** Initialization constructor. */
		ElementDataType(const typename LightMapPolicyType::ElementDataType& InLightMapElementData)
		:	LightMapElementData(InLightMapElementData)
		{}
	};

	/** Initialization constructor. */
	TBasePassDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		ERHIFeatureLevel::Type InFeatureLevel,
		LightMapPolicyType InLightMapPolicy,
		EBlendMode InBlendMode,
		bool bInEnableSkyLight,
		bool bInEnableAtmosphericFog,
		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
		EDebugViewShaderMode InDebugViewShaderMode = DVSM_None,
		bool bInEnableReceiveDecalOutput = false
		):
		FBasePassDrawingPolicy(InVertexFactory, InMaterialRenderProxy, InMaterialResource, InOverrideSettings, InDebugViewShaderMode, bInEnableReceiveDecalOutput),
		LightMapPolicy(InLightMapPolicy),
		BlendMode(InBlendMode), 
		bEnableSkyLight(bInEnableSkyLight),
		bEnableAtmosphericFog(bInEnableAtmosphericFog)
	{
		HullShader = NULL;
		DomainShader = NULL;
	
		const EMaterialTessellationMode MaterialTessellationMode = InMaterialResource.GetTessellationMode();

		const bool bNeedsHSDS = RHISupportsTessellation(GShaderPlatformForFeatureLevel[InFeatureLevel])
								&& InVertexFactory->GetType()->SupportsTessellationShaders() 
								&& MaterialTessellationMode != MTM_NoTessellation;

		GetBasePassShaders<LightMapPolicyType>(
			InMaterialResource, 
			VertexFactory->GetType(), 
			InLightMapPolicy, 
			bNeedsHSDS,
			bEnableAtmosphericFog,
			bEnableSkyLight,
			HullShader,
			DomainShader,
			VertexShader,
			PixelShader
			);

		BaseVertexShader = VertexShader;
	}

	// FMeshDrawingPolicy interface.

	FDrawingPolicyMatchResult Matches(const TBasePassDrawingPolicy& Other, bool bForReals = false) const
	{
		DRAWING_POLICY_MATCH_BEGIN
			DRAWING_POLICY_MATCH(FMeshDrawingPolicy::Matches(Other, bForReals)) &&
			DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader) &&
			DRAWING_POLICY_MATCH(PixelShader == Other.PixelShader) &&
			DRAWING_POLICY_MATCH(HullShader == Other.HullShader) &&
			DRAWING_POLICY_MATCH(DomainShader == Other.DomainShader) &&
			DRAWING_POLICY_MATCH(bEnableSkyLight == Other.bEnableSkyLight) && 
			DRAWING_POLICY_MATCH(LightMapPolicy == Other.LightMapPolicy) &&
			DRAWING_POLICY_MATCH(bEnableReceiveDecalOutput == Other.bEnableReceiveDecalOutput) &&
			DRAWING_POLICY_MATCH(UseDebugViewPS() == Other.UseDebugViewPS());
		DRAWING_POLICY_MATCH_END
	}

	void SetupPipelineState(FDrawingPolicyRenderState& DrawRenderState, const FSceneView& View) const
	{
		if (UseDebugViewPS())
		{
			if (IsTranslucentBlendMode(BlendMode))
			{
				if (View.Family->EngineShowFlags.ShaderComplexity)
				{	// If we are in the translucent pass then override the blend mode, otherwise maintain additive blending.
					DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI());
				}
				else if (View.Family->GetDebugViewShaderMode() != DVSM_OutputMaterialTextureScales)
				{	// Otherwise, force translucent blend mode (shaders will use an hardcoded alpha).
					DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
				}
			}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// If we are in the translucent pass or rendering a masked material then override the blend mode, otherwise maintain opaque blending
			if (View.Family->EngineShowFlags.ShaderComplexity && BlendMode != BLEND_Opaque)
			{
				// Add complexity to existing, keep alpha
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI());
			}
#endif
		}
		else
		{
			switch (BlendMode)
			{
			default:
			case BLEND_Opaque:
				// Opaque materials are rendered together in the base pass, where the blend state is set at a higher level
				break;
			case BLEND_Masked:
				// Masked materials are rendered together in the base pass, where the blend state is set at a higher level
				break;
			case BLEND_Translucent:
				// Note: alpha channel used by separate translucency, storing how much of the background should be added when doing the final composite
				// The Alpha channel is also used by non-separate translucency when rendering to scene captures, which store the final opacity
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
				break;
			case BLEND_Additive:
				// Add to the existing scene color
				// Note: alpha channel used by separate translucency, storing how much of the background should be added when doing the final composite
				// The Alpha channel is also used by non-separate translucency when rendering to scene captures, which store the final opacity
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
				break;
			case BLEND_Modulate:
				// Modulate with the existing scene color, preserve destination alpha.
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI());
				break;
			case BLEND_AlphaComposite:
				// Blend with existing scene color. New color is already pre-multiplied by alpha.
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
				break;
			};
		}
	}

	void SetSharedState(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, const FViewInfo* View, const ContextDataType PolicyContext) const
	{
		// If the current debug view shader modes are allowed, different VS/DS/HS must be used (with only SV_POSITION as PS interpolant).
		if (View->Family->UseDebugViewVSDSHS())
		{
			FDebugViewMode::SetParametersVSHSDS(RHICmdList, MaterialRenderProxy, MaterialResource, *View, VertexFactory, HullShader && DomainShader, DrawRenderState);
		}
		else
		{
			check(VertexFactory && VertexFactory->IsInitialized());
			VertexFactory->SetStreams(View->FeatureLevel, RHICmdList);

			VertexShader->SetParameters(RHICmdList, MaterialRenderProxy, VertexFactory, *MaterialResource, *View, DrawRenderState, PolicyContext.bIsInstancedStereo);

			if(HullShader)
			{
				HullShader->SetParameters(RHICmdList, MaterialRenderProxy, *View, DrawRenderState.GetViewUniformBuffer(), DrawRenderState.GetPassUniformBuffer());
			}

			if (DomainShader)
			{
				DomainShader->SetParameters(RHICmdList, MaterialRenderProxy, *View, DrawRenderState.GetViewUniformBuffer(), DrawRenderState.GetPassUniformBuffer());
			}
		}

		if (UseDebugViewPS())
		{
			FDebugViewMode::GetPSInterface(View->ShaderMap, MaterialResource, GetDebugViewShaderMode())->SetParameters(RHICmdList, VertexShader, PixelShader, MaterialRenderProxy, *MaterialResource, *View, DrawRenderState);
		}
		else
		{
			PixelShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, View, DrawRenderState, BlendMode);
		}
	}

	void SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex) const
	{
		VertexShader->SetInstancedEyeIndex(RHICmdList, EyeIndex);
	}

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @param DynamicStride - optional stride for dynamic vertex data
	* @return new bound shader state object
	*/
	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		FBoundShaderStateInput BoundShaderStateInput(
			FMeshDrawingPolicy::GetVertexDeclaration(), 
			VertexShader->GetVertexShader(),
			GETSAFERHISHADER_HULL(HullShader), 
			GETSAFERHISHADER_DOMAIN(DomainShader), 
			PixelShader->GetPixelShader(),
			FGeometryShaderRHIRef()
			);

		if (UseDebugViewPS())
		{
			FDebugViewMode::PatchBoundShaderState(BoundShaderStateInput, MaterialResource, VertexFactory, InFeatureLevel, GetDebugViewShaderMode());
		}
		return BoundShaderStateInput;
	}

	void SetMeshRenderState(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& Mesh,
		int32 BatchElementIndex,
		FDrawingPolicyRenderState& DrawRenderState,
		const ElementDataType& ElementData,
		const ContextDataType PolicyContext
		) const
	{
		const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];

		// If debug view shader mode are allowed, different VS/DS/HS must be used (with only SV_POSITION as PS interpolant).
		if (View.Family->UseDebugViewVSDSHS())
		{
			FDebugViewMode::SetMeshVSHSDS(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState, MaterialResource, HullShader && DomainShader);
		}
		else
		{
			// Set the light-map policy's mesh-specific settings.
			LightMapPolicy.SetMesh(
				RHICmdList, 
				View,
				PrimitiveSceneProxy,
				VertexShader,
				!UseDebugViewPS() ? PixelShader : nullptr,
				VertexShader,
				PixelShader,
				VertexFactory,
				MaterialRenderProxy,
				ElementData.LightMapElementData);

			VertexShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy, Mesh,BatchElement,DrawRenderState);
		
			if(HullShader && DomainShader)
			{
				HullShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
				DomainShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
			}
		}

		if (UseDebugViewPS())
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			FDebugViewMode::GetPSInterface(View.ShaderMap, MaterialResource, GetDebugViewShaderMode())->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, Mesh.VisualizeLODIndex, BatchElement, DrawRenderState);
#endif
		}
		else
		{
			PixelShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState,BlendMode);
		}
	}

	friend int32 CompareDrawingPolicy(const TBasePassDrawingPolicy& A,const TBasePassDrawingPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
		COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
		COMPAREDRAWINGPOLICYMEMBERS(HullShader);
		COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
		COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
		COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
		COMPAREDRAWINGPOLICYMEMBERS(bEnableSkyLight);
		COMPAREDRAWINGPOLICYMEMBERS(bEnableReceiveDecalOutput);

		return CompareDrawingPolicy(A.LightMapPolicy,B.LightMapPolicy);
	}

protected:

	// Here we don't store the most derived type of shaders, for instance TBasePassVertexShaderBaseType<LightMapPolicyType>.
	// This is to allow any shader using the same parameters to be used, and is required to allow FUniformLightMapPolicy to use shaders derived from TUniformLightMapPolicy.
	TBasePassVertexShaderPolicyParamType<typename LightMapPolicyType::VertexParametersType>* VertexShader;
	FBaseHS* HullShader; // Does not depend on LightMapPolicyType
	FBaseDS* DomainShader; // Does not depend on LightMapPolicyType
	TBasePassPixelShaderPolicyParamType<typename LightMapPolicyType::PixelParametersType>* PixelShader;

	LightMapPolicyType LightMapPolicy;
	EBlendMode BlendMode;

	uint32 bEnableSkyLight : 1;

	/** Whether or not this policy enables atmospheric fog */
	uint32 bEnableAtmosphericFog : 1;
};

/**
 * A drawing policy factory for the base pass drawing policy.
 */
class FBasePassOpaqueDrawingPolicyFactory
{
public:

	enum { bAllowSimpleElements = true };
	struct ContextType 
	{
		ContextType()
		{}
	};

	static void AddStaticMesh(FRHICommandList& RHICmdList, FScene* Scene, FStaticMesh* StaticMesh);
	static bool DrawDynamicMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		bool bPreFog,
		const FDrawingPolicyRenderState& DrawRenderState,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId, 
		const bool bIsInstancedStereo = false
		);
};

/** The parameters used to process a base pass mesh. */
class FProcessBasePassMeshParameters
{
public:

	const FMeshBatch& Mesh;
	const uint64 BatchElementMask;
	const FMaterial* Material;
	const FPrimitiveSceneProxy* PrimitiveSceneProxy;
	EBlendMode BlendMode;
	EMaterialShadingModel ShadingModel;
	const bool bAllowFog;
	ERHIFeatureLevel::Type FeatureLevel;
	const bool bIsInstancedStereo;

	/** Initialization constructor. */
	FProcessBasePassMeshParameters(
		const FMeshBatch& InMesh,
		const FMaterial* InMaterial,
		const FPrimitiveSceneProxy* InPrimitiveSceneProxy,
		bool InbAllowFog,
		ERHIFeatureLevel::Type InFeatureLevel,
		const bool InbIsInstancedStereo = false
		):
		Mesh(InMesh),
		BatchElementMask(Mesh.Elements.Num()==1 ? 1 : (1<<Mesh.Elements.Num())-1), // 1 bit set for each mesh element
		Material(InMaterial),
		PrimitiveSceneProxy(InPrimitiveSceneProxy),
		BlendMode(InMaterial->GetBlendMode()),
		ShadingModel(InMaterial->GetShadingModel()),
		bAllowFog(InbAllowFog),
		FeatureLevel(InFeatureLevel), 
		bIsInstancedStereo(InbIsInstancedStereo)
	{
	}

	/** Initialization constructor. */
	FProcessBasePassMeshParameters(
		const FMeshBatch& InMesh,
		const uint64& InBatchElementMask,
		const FMaterial* InMaterial,
		const FPrimitiveSceneProxy* InPrimitiveSceneProxy,
		bool InbAllowFog,
		ERHIFeatureLevel::Type InFeatureLevel, 
		bool InbIsInstancedStereo = false
		) :
		Mesh(InMesh),
		BatchElementMask(InBatchElementMask),
		Material(InMaterial),
		PrimitiveSceneProxy(InPrimitiveSceneProxy),
		BlendMode(InMaterial->GetBlendMode()),
		ShadingModel(InMaterial->GetShadingModel()),
		bAllowFog(InbAllowFog),
		FeatureLevel(InFeatureLevel),
		bIsInstancedStereo(InbIsInstancedStereo)
	{
	}
};

template<typename ProcessActionType>
void ProcessBasePassMeshForSimpleForwardShading(
	FRHICommandList& RHICmdList,
	const FProcessBasePassMeshParameters& Parameters,
	ProcessActionType&& Action,
	const FLightMapInteraction& LightMapInteraction,
	bool bIsLitMaterial,
	bool bAllowStaticLighting
	)
{
	if (bAllowStaticLighting && LightMapInteraction.GetType() == LMIT_Texture)
	{
		const FShadowMapInteraction ShadowMapInteraction = (Parameters.Mesh.LCI && bIsLitMaterial) 
			? Parameters.Mesh.LCI->GetShadowMapInteraction() 
			: FShadowMapInteraction();

		if (ShadowMapInteraction.GetType() == SMIT_Texture)
		{
			Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_SIMPLE_STATIONARY_PRECOMPUTED_SHADOW_LIGHTING), Parameters.Mesh.LCI);
		}
		else
		{
			Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_SIMPLE_LIGHTMAP_ONLY_LIGHTING), Parameters.Mesh.LCI);
		}
	}
	else if (bIsLitMaterial
		&& bAllowStaticLighting
		&& Action.UseVolumetricLightmap()
		&& Parameters.PrimitiveSceneProxy)
	{
		Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_SIMPLE_STATIONARY_VOLUMETRICLIGHTMAP_SHADOW_LIGHTING), Parameters.Mesh.LCI);
	}
	else if (bIsLitMaterial
		&& IsIndirectLightingCacheAllowed(Parameters.FeatureLevel)
		&& Action.AllowIndirectLightingCache()
		&& Parameters.PrimitiveSceneProxy)
	{
		const FIndirectLightingCacheAllocation* IndirectLightingCacheAllocation = Parameters.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation;
		const bool bPrimitiveIsMovable = Parameters.PrimitiveSceneProxy->IsMovable();
		const bool bPrimitiveUsesILC = Parameters.PrimitiveSceneProxy->GetIndirectLightingCacheQuality() != ILCQ_Off;								

		// Use the indirect lighting cache shaders if the object has a cache allocation
		// This happens for objects with unbuilt lighting
		if (bPrimitiveUsesILC &&
			((IndirectLightingCacheAllocation && IndirectLightingCacheAllocation->IsValid())
			// Use the indirect lighting cache shaders if the object is movable, it may not have a cache allocation yet because that is done in InitViews
			// And movable objects are sometimes rendered in the static draw lists
			|| bPrimitiveIsMovable))
		{
			// Use a lightmap policy that supports reading indirect lighting from a single SH sample
			Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_SIMPLE_STATIONARY_SINGLESAMPLE_SHADOW_LIGHTING), Parameters.Mesh.LCI);
		}
		else
		{
			Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_SIMPLE_NO_LIGHTMAP), Parameters.Mesh.LCI);
		}
	}
	else if (bIsLitMaterial)
	{
		// Always choosing shaders to support dynamic directional even if one is not present
		Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_SIMPLE_DIRECTIONAL_LIGHT_LIGHTING), Parameters.Mesh.LCI);
	}
	else
	{
		Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_SIMPLE_NO_LIGHTMAP), Parameters.Mesh.LCI);
	}
}

/** Processes a base pass mesh using an unknown light map policy, and unknown fog density policy. */
template<typename ProcessActionType>
void ProcessBasePassMesh(
	FRHICommandList& RHICmdList,
	const FProcessBasePassMeshParameters& Parameters,
	ProcessActionType&& Action
	)
{
	// Check for a cached light-map.
	const bool bIsLitMaterial = (Parameters.ShadingModel != MSM_Unlit);
	static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnRenderThread() != 0);
	
	
	const FLightMapInteraction LightMapInteraction = (bAllowStaticLighting && Parameters.Mesh.LCI && bIsLitMaterial) 
		? Parameters.Mesh.LCI->GetLightMapInteraction(Parameters.FeatureLevel) 
		: FLightMapInteraction();

	// force LQ lightmaps based on system settings
	const bool bPlatformAllowsHighQualityLightMaps = AllowHighQualityLightmaps(Parameters.FeatureLevel);
	const bool bAllowHighQualityLightMaps = bPlatformAllowsHighQualityLightMaps && LightMapInteraction.AllowsHighQualityLightmaps();

	if (IsSimpleForwardShadingEnabled(GetFeatureLevelShaderPlatform(Parameters.FeatureLevel)))
	{
		// Only compiling simple lighting shaders for HQ lightmaps to save on permutations
		check(bPlatformAllowsHighQualityLightMaps);
		ProcessBasePassMeshForSimpleForwardShading(RHICmdList, Parameters, Action, LightMapInteraction, bIsLitMaterial, bAllowStaticLighting);
	}
	// Render self-shadowing only for >= SM4 and fallback to non-shadowed for lesser shader models
	else if (bIsLitMaterial && Action.UseTranslucentSelfShadowing() && Parameters.FeatureLevel >= ERHIFeatureLevel::SM4)
	{
		if (bIsLitMaterial
			&& bAllowStaticLighting
			&& Action.UseVolumetricLightmap()
			&& Parameters.PrimitiveSceneProxy)
		{
			Action.template Process<FSelfShadowedVolumetricLightmapPolicy>(RHICmdList, Parameters, FSelfShadowedVolumetricLightmapPolicy(), FSelfShadowedTranslucencyPolicy::ElementDataType(Action.GetTranslucentSelfShadow()));
		}
		else if (IsIndirectLightingCacheAllowed(Parameters.FeatureLevel)
				&& Action.AllowIndirectLightingCache()
				&& Parameters.PrimitiveSceneProxy)
		{
			// Apply cached point indirect lighting as well as self shadowing if needed
			Action.template Process<FSelfShadowedCachedPointIndirectLightingPolicy>(RHICmdList, Parameters, FSelfShadowedCachedPointIndirectLightingPolicy(), FSelfShadowedTranslucencyPolicy::ElementDataType(Action.GetTranslucentSelfShadow()));
		}
		else
		{
			Action.template Process<FSelfShadowedTranslucencyPolicy>(RHICmdList, Parameters, FSelfShadowedTranslucencyPolicy(), FSelfShadowedTranslucencyPolicy::ElementDataType(Action.GetTranslucentSelfShadow()));
		}
	}
	else
	{
		static const auto CVarSupportLowQualityLightmap = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportLowQualityLightmaps"));
		const bool bAllowLowQualityLightMaps = (!CVarSupportLowQualityLightmap) || (CVarSupportLowQualityLightmap->GetValueOnAnyThread() != 0);

		switch (LightMapInteraction.GetType())
		{
		case LMIT_Texture:
			if (bAllowHighQualityLightMaps)
			{
				const FShadowMapInteraction ShadowMapInteraction = (bAllowStaticLighting && Parameters.Mesh.LCI && bIsLitMaterial)
					? Parameters.Mesh.LCI->GetShadowMapInteraction()
					: FShadowMapInteraction();

				if (ShadowMapInteraction.GetType() == SMIT_Texture)
				{
					Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP), Parameters.Mesh.LCI);
				}
				else
				{
					Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_HQ_LIGHTMAP), Parameters.Mesh.LCI);
				}
			}
			else if (bAllowLowQualityLightMaps)
			{
				Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_LQ_LIGHTMAP), Parameters.Mesh.LCI);
			}
			else
			{
				Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_NO_LIGHTMAP), Parameters.Mesh.LCI);
			}
			break;
		default:
			if (bIsLitMaterial
				&& bAllowStaticLighting
				&& Action.UseVolumetricLightmap()
				&& Parameters.PrimitiveSceneProxy
				&& (Parameters.PrimitiveSceneProxy->IsMovable() 
					|| Parameters.PrimitiveSceneProxy->NeedsUnbuiltPreviewLighting() 
					|| Parameters.PrimitiveSceneProxy->GetLightmapType() == ELightmapType::ForceVolumetric))
			{
				Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING), Parameters.Mesh.LCI);
			}
			else if (bIsLitMaterial
				&& IsIndirectLightingCacheAllowed(Parameters.FeatureLevel)
				&& Action.AllowIndirectLightingCache()
				&& Parameters.PrimitiveSceneProxy)
			{
				const FIndirectLightingCacheAllocation* IndirectLightingCacheAllocation = Parameters.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation;
				const bool bPrimitiveIsMovable = Parameters.PrimitiveSceneProxy->IsMovable();
				const bool bPrimitiveUsesILC = Parameters.PrimitiveSceneProxy->GetIndirectLightingCacheQuality() != ILCQ_Off;

				// Use the indirect lighting cache shaders if the object has a cache allocation
				// This happens for objects with unbuilt lighting
				if (bPrimitiveUsesILC &&
					((IndirectLightingCacheAllocation && IndirectLightingCacheAllocation->IsValid())
						// Use the indirect lighting cache shaders if the object is movable, it may not have a cache allocation yet because that is done in InitViews
						// And movable objects are sometimes rendered in the static draw lists
						|| bPrimitiveIsMovable))
				{
					if (CanIndirectLightingCacheUseVolumeTexture(Parameters.FeatureLevel)
						// Translucency forces point sample for pixel performance
						&& Action.AllowIndirectLightingCacheVolumeTexture()
						&& ((IndirectLightingCacheAllocation && !IndirectLightingCacheAllocation->bPointSample)
							|| (bPrimitiveIsMovable && Parameters.PrimitiveSceneProxy->GetIndirectLightingCacheQuality() == ILCQ_Volume)))
					{
						// Use a lightmap policy that supports reading indirect lighting from a volume texture for dynamic objects
						Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_CACHED_VOLUME_INDIRECT_LIGHTING), Parameters.Mesh.LCI);
					}
					else
					{
						// Use a lightmap policy that supports reading indirect lighting from a single SH sample
						Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_CACHED_POINT_INDIRECT_LIGHTING), Parameters.Mesh.LCI);
					}
				}
				else
				{
					Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_NO_LIGHTMAP), Parameters.Mesh.LCI);
				}
			}
			else
			{
				Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_NO_LIGHTMAP), Parameters.Mesh.LCI);
			}
			break;
		};
	}	
}
