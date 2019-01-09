// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MobileBasePassRendering.h: base pass rendering definitions.
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
#include "PrimitiveSceneInfo.h"
#include "DrawingPolicy.h"
#include "PostProcess/SceneRenderTargets.h"
#include "LightMapRendering.h"
#include "MeshMaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "FogRendering.h"
#include "PlanarReflectionRendering.h"
#include "BasePassRendering.h"

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FMobileBasePassUniformParameters, )
	SHADER_PARAMETER_STRUCT(FFogUniformParameters, Fog)
	SHADER_PARAMETER_STRUCT(FPlanarReflectionUniformParameters, PlanarReflection) // Single global planar reflection for the forward pass.
	SHADER_PARAMETER_STRUCT(FMobileSceneTextureUniformParameters, SceneTextures)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

extern void SetupMobileBasePassUniformParameters(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	bool bTranslucentPass,
	FMobileBasePassUniformParameters& BasePassParameters);

extern void CreateMobileBasePassUniformBuffer(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	bool bTranslucentPass,
	TUniformBufferRef<FMobileBasePassUniformParameters>& BasePassUniformBuffer);

extern void SetupMobileDirectionalLightUniformParameters(
	const FScene& Scene,
	const FViewInfo& View,
	const TArray<FVisibleLightInfo,SceneRenderingAllocator> VisibleLightInfos,
	int32 ChannelIdx,
	bool bDynamicShadows,
	FMobileDirectionalLightShaderParameters& Parameters);

class FPlanarReflectionSceneProxy;
class FScene;

enum EOutputFormat
{
	LDR_GAMMA_32,
	HDR_LINEAR_64,
};

#define MAX_BASEPASS_DYNAMIC_POINT_LIGHTS 4

namespace MobileBasePass
{
	// If no reflection captures are available then attempt to use sky light's texture.
	bool UseSkyReflectionCapture(const FScene* RenderScene);
	void GetSkyTextureParams(const FScene* Scene, float& AverageBrightnessOUT, FTexture*& ReflectionTextureOUT, float& OutSkyMaxMipIndex);
};


/* Info for dynamic point lights rendered in base pass */
class FMobileBasePassMovablePointLightInfo
{
public:
	FMobileBasePassMovablePointLightInfo(const FPrimitiveSceneProxy* InSceneProxy);

	int32 NumMovablePointLights;
	FVector4 LightPositionAndInvRadius[MAX_BASEPASS_DYNAMIC_POINT_LIGHTS];
	FVector4 LightColorAndFalloffExponent[MAX_BASEPASS_DYNAMIC_POINT_LIGHTS];
};

static bool ShouldCacheShaderByPlatformAndOutputFormat(EShaderPlatform Platform, EOutputFormat OutputFormat)
{
	bool bSupportsMobileHDR = IsMobileHDR();
	bool bShaderUsesLDR = (OutputFormat == LDR_GAMMA_32);

	// only cache this shader if the LDR/HDR output matches what we currently support.  IsMobileHDR can't change, so we don't need
	// the LDR shaders if we are doing HDR, and vice-versa.
	return (bShaderUsesLDR && !bSupportsMobileHDR) || (!bShaderUsesLDR && bSupportsMobileHDR);
}

template<typename LightMapPolicyType>
class TMobileBasePassShaderElementData : public FMeshMaterialShaderElementData
{
public:
	TMobileBasePassShaderElementData(const typename LightMapPolicyType::ElementDataType& InLightMapPolicyElementData) :
		LightMapPolicyElementData(InLightMapPolicyElementData)
	{}

	typename LightMapPolicyType::ElementDataType LightMapPolicyElementData;
};

/**
 * The base shader type for vertex shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */

template<typename LightMapPolicyType>
class TMobileBasePassVSPolicyParamType : public FMeshMaterialShader, public LightMapPolicyType::VertexParametersType
{
protected:

	TMobileBasePassVSPolicyParamType() {}
	TMobileBasePassVSPolicyParamType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		LightMapPolicyType::VertexParametersType::Bind(Initializer.ParameterMap);
		PassUniformBuffer.Bind(Initializer.ParameterMap, FMobileBasePassUniformParameters::StaticStructMetadata.GetShaderVariableName());
	}

public:

	// static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		LightMapPolicyType::VertexParametersType::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		const FViewInfo* View,
		const FDrawingPolicyRenderState& DrawRenderState)
	{

		FMaterialShader::SetViewParameters(RHICmdList, GetVertexShader(), *View, DrawRenderState.GetViewUniformBuffer());
		FMeshMaterialShader::SetPassUniformBuffer(RHICmdList, GetVertexShader(), DrawRenderState.GetPassUniformBuffer());
	}

	// Set parameters specific to mesh
	void SetMesh(
		FRHICommandList& RHICmdList,
		const FMaterial& InMaterialResource,
		const FSceneView& View,
		const FVertexFactory* InVertexFactory, 
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FPrimitiveSceneProxy* Proxy,
		const FMeshBatchElement& BatchElement, 
		const FDrawingPolicyRenderState& DrawRenderState)
	{
		FMaterialShader::SetParametersInner(RHICmdList, GetVertexShader(), InMaterialRenderProxy, InMaterialResource, View);
		uint32 DataFlags = 0;
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(), InVertexFactory, View, Proxy, BatchElement, DrawRenderState, DataFlags);
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
		FUniformBufferRHIParamRef PassUniformBufferValue,
		const TMobileBasePassShaderElementData<LightMapPolicyType>& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, ViewUniformBuffer, PassUniformBufferValue, ShaderElementData, ShaderBindings);

		LightMapPolicyType::GetVertexShaderBindings(
			PrimitiveSceneProxy,
			ShaderElementData.LightMapPolicyElementData,
			this,
			ShaderBindings);
	}
};

template<typename LightMapPolicyType>
class TMobileBasePassVSBaseType : public TMobileBasePassVSPolicyParamType<LightMapPolicyType>
{
	typedef TMobileBasePassVSPolicyParamType<LightMapPolicyType> Super;

protected:

	TMobileBasePassVSBaseType() {}
	TMobileBasePassVSBaseType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : Super(Initializer) {}

public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return IsMobilePlatform(Platform) && LightMapPolicyType::ShouldCompilePermutation(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
};

template< typename LightMapPolicyType, EOutputFormat OutputFormat >
class TMobileBasePassVS : public TMobileBasePassVSBaseType<LightMapPolicyType>
{
	DECLARE_SHADER_TYPE(TMobileBasePassVS,MeshMaterial);
public:
	
	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{		
		return TMobileBasePassVSBaseType<LightMapPolicyType>::ShouldCompilePermutation(Platform, Material, VertexFactoryType) && ShouldCacheShaderByPlatformAndOutputFormat(Platform,OutputFormat);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		TMobileBasePassVSBaseType<LightMapPolicyType>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine( TEXT("OUTPUT_GAMMA_SPACE"), OutputFormat == LDR_GAMMA_32 ? 1u : 0u );
	}
	
	/** Initialization constructor. */
	TMobileBasePassVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		TMobileBasePassVSBaseType<LightMapPolicyType>(Initializer)
	{}

	/** Default constructor. */
	TMobileBasePassVS() {}
};

/**
 * The base type for pixel shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */

template<typename LightMapPolicyType>
class TMobileBasePassPSPolicyParamType : public FMeshMaterialShader, public LightMapPolicyType::PixelParametersType
{
public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return IsMobilePlatform(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		// Modify compilation environment depending upon material shader quality level settings.
		ModifyCompilationEnvironmentForQualityLevel(Platform, Material->GetQualityLevel(), OutEnvironment);
	}

	/** Initialization constructor. */
	TMobileBasePassPSPolicyParamType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		LightMapPolicyType::PixelParametersType::Bind(Initializer.ParameterMap);
		PassUniformBuffer.Bind(Initializer.ParameterMap, FMobileBasePassUniformParameters::StaticStructMetadata.GetShaderVariableName());
		
		MobileDirectionLightBufferParam.Bind(Initializer.ParameterMap, FMobileDirectionalLightShaderParameters::StaticStructMetadata.GetShaderVariableName());

		ReflectionCubemap.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemap"));
		ReflectionSampler.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemapSampler"));
		ReflectionCubemap1.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemap1"));
		ReflectionSampler1.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemapSampler1"));
		ReflectionCubemap2.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemap2"));
		ReflectionSampler2.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemapSampler2"));
		MobileReflectionParams.Bind(Initializer.ParameterMap, TEXT("MobileReflectionParams"));
		ReflectionPositionsAndRadii.Bind(Initializer.ParameterMap, TEXT("ReflectionPositionsAndRadii"));

		LightPositionAndInvRadiusParameter.Bind(Initializer.ParameterMap, TEXT("LightPositionAndInvRadius"));
		LightColorAndFalloffExponentParameter.Bind(Initializer.ParameterMap, TEXT("LightColorAndFalloffExponent"));
		NumDynamicPointLightsParameter.Bind(Initializer.ParameterMap, TEXT("NumDynamicPointLights"));
				
		CSMDebugHintParams.Bind(Initializer.ParameterMap, TEXT("CSMDebugHint"));
	}
	TMobileBasePassPSPolicyParamType() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		LightMapPolicyType::PixelParametersType::Serialize(Ar);

		Ar << MobileDirectionLightBufferParam;
		Ar << ReflectionCubemap;
		Ar << ReflectionSampler;
		Ar << ReflectionCubemap1;
		Ar << ReflectionCubemap2;
		Ar << ReflectionSampler1;
		Ar << ReflectionSampler2;
		Ar << MobileReflectionParams;
		Ar << ReflectionPositionsAndRadii;

		Ar << LightPositionAndInvRadiusParameter;
		Ar << LightColorAndFalloffExponentParameter;
		Ar << NumDynamicPointLightsParameter;
	
		Ar << CSMDebugHintParams;

		return bShaderHasOutdatedParameters;
	}

private:
	FShaderUniformBufferParameter MobileDirectionLightBufferParam;

	FShaderResourceParameter ReflectionCubemap;
	FShaderResourceParameter ReflectionSampler;
	FShaderResourceParameter ReflectionCubemap1;
	FShaderResourceParameter ReflectionSampler1;
	FShaderResourceParameter ReflectionCubemap2;
	FShaderResourceParameter ReflectionSampler2;
	FShaderParameter MobileReflectionParams;
	FShaderParameter ReflectionPositionsAndRadii;
	
	FShaderParameter LightPositionAndInvRadiusParameter;
	FShaderParameter LightColorAndFalloffExponentParameter;
	FShaderParameter NumDynamicPointLightsParameter;

	FShaderParameter CSMDebugHintParams;

public:
	// Set parameters specific to PSO
	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo* View, 
		const FDrawingPolicyRenderState& DrawRenderState)
	{
		// If we're using only the sky for reflection then set it once here.
		FScene* RenderScene = View->Family->Scene->GetRenderScene();
		if (MobileBasePass::UseSkyReflectionCapture(RenderScene))
		{
			// MobileReflectionValues.x == max sky cube mip.
			// if >0 this will disable shader's RGBM decoding and enable sky light tinting of this envmap.
			FTexture* ReflectionTexture = GBlackTextureCube;
			float AverageBrightness = 1.0f;
			FVector4 MobileSkyReflectionValues(ForceInit);
			if (View->GetFeatureLevel() > ERHIFeatureLevel::ES2) // not-supported on ES2 at the moment
			{
				MobileBasePass::GetSkyTextureParams(RenderScene, AverageBrightness, ReflectionTexture, MobileSkyReflectionValues.W);
			}
			MobileSkyReflectionValues.X = 1.0f / AverageBrightness;

			FRHIPixelShader* PixelShader = GetPixelShader();
			// Set the reflection cubemap
			SetTextureParameter(RHICmdList, PixelShader, ReflectionCubemap, ReflectionSampler, ReflectionTexture);
			SetShaderValue(RHICmdList, PixelShader, MobileReflectionParams, MobileSkyReflectionValues);
		}

		FMaterialShader::SetViewParameters(RHICmdList, GetPixelShader(), *View, DrawRenderState.GetViewUniformBuffer());
		FMeshMaterialShader::SetPassUniformBuffer(RHICmdList, GetPixelShader(), DrawRenderState.GetPassUniformBuffer());
	}

	// Set parameters specific to Mesh
	void SetMesh(
		FRHICommandList& RHICmdList, 
		const FMaterial& InMaterialResource,
		const FSceneView& View,
		const FVertexFactory* InVertexFactory, 
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FPrimitiveSceneProxy* Proxy,
		const FMeshBatchElement& BatchElement, 
		const FDrawingPolicyRenderState& DrawRenderState,
		int32 NumMovablePointLights)
	{
		FRHIPixelShader* PixelShader = GetPixelShader();
		FPrimitiveSceneInfo* PrimitiveSceneInfo = Proxy ? Proxy->GetPrimitiveSceneInfo() : NULL;
		// test for HQ reflection parameter existence
		if (ReflectionCubemap1.IsBound() || ReflectionCubemap2.IsBound() || ReflectionPositionsAndRadii.IsBound())
		{
			static const int32 MaxNumReflections = FPrimitiveSceneInfo::MaxCachedReflectionCaptureProxies;
			static_assert(MaxNumReflections == 3, "Update reflection array initializations to match MaxCachedReflectionCaptureProxies");

			// set high quality reflection parameters.
			const FShaderResourceParameter* ReflectionTextureParameters[MaxNumReflections] = { &ReflectionCubemap, &ReflectionCubemap1, &ReflectionCubemap2 };
			const FShaderResourceParameter* ReflectionSamplerParameters[MaxNumReflections] = { &ReflectionSampler, &ReflectionSampler1, &ReflectionSampler2 };
			FTexture* ReflectionCubemapTextures[MaxNumReflections] = { GBlackTextureCube, GBlackTextureCube, GBlackTextureCube };
			FVector4 CapturePositions[MaxNumReflections] = { FVector4(0, 0, 0, 0), FVector4(0, 0, 0, 0), FVector4(0, 0, 0, 0) };
			FVector AverageBrightness(1, 1, 1);

			if (PrimitiveSceneInfo)
			{
				for (int32 i = 0; i < MaxNumReflections; i++)
				{
					const FReflectionCaptureProxy* ReflectionProxy = PrimitiveSceneInfo->CachedReflectionCaptureProxies[i];
					if (ReflectionProxy)
					{
						CapturePositions[i] = ReflectionProxy->Position;
						CapturePositions[i].W = ReflectionProxy->InfluenceRadius;
						if (ReflectionProxy->EncodedHDRCubemap && ReflectionProxy->EncodedHDRCubemap->IsInitialized())
						{
							ReflectionCubemapTextures[i] = PrimitiveSceneInfo->CachedReflectionCaptureProxies[i]->EncodedHDRCubemap;
						}
						AverageBrightness[i] = ReflectionProxy->EncodedHDRAverageBrightness;
					}
				}
			}

			for (int32 i = 0; i < MaxNumReflections; i++)
			{
				if (ReflectionTextureParameters[i]->IsBound())
				{
					SetTextureParameter(RHICmdList, PixelShader, *ReflectionTextureParameters[i], *ReflectionSamplerParameters[i], ReflectionCubemapTextures[i]);
				}
			}

			if (ReflectionPositionsAndRadii.IsBound())
			{
				SetShaderValueArray(RHICmdList, PixelShader, ReflectionPositionsAndRadii, CapturePositions, MaxNumReflections);
			}

			SetShaderValue(RHICmdList, PixelShader, MobileReflectionParams, FVector4(1.0f / AverageBrightness.X, 1.0f / AverageBrightness.Y, 1.0f / AverageBrightness.Z, 0));
		}
		else if (ReflectionCubemap.IsBound() && (!PrimitiveSceneInfo || !MobileBasePass::UseSkyReflectionCapture(PrimitiveSceneInfo->Scene)))
		{
			FTexture* ReflectionTexture = GBlackTextureCube;
			float AverageBrightness = 1.0f;

			if (PrimitiveSceneInfo 
				&& PrimitiveSceneInfo->CachedReflectionCaptureProxy
				&& PrimitiveSceneInfo->CachedReflectionCaptureProxy->EncodedHDRCubemap
				&& PrimitiveSceneInfo->CachedReflectionCaptureProxy->EncodedHDRCubemap->IsInitialized())
			{
				AverageBrightness = PrimitiveSceneInfo->CachedReflectionCaptureProxy->EncodedHDRAverageBrightness;
				ReflectionTexture = PrimitiveSceneInfo->CachedReflectionCaptureProxy->EncodedHDRCubemap;
			}

			// Set the reflection cubemap
			SetTextureParameter(RHICmdList, PixelShader, ReflectionCubemap, ReflectionSampler, ReflectionTexture);
			SetShaderValue(RHICmdList, PixelShader, MobileReflectionParams, FVector4(1.0f / AverageBrightness, 0, 0, 0));
		}

		if (NumMovablePointLights > 0)
		{
			FMobileBasePassMovablePointLightInfo LightInfo(Proxy);

			if (NumMovablePointLights == INT32_MAX)
			{
				SetShaderValue(RHICmdList, PixelShader, NumDynamicPointLightsParameter, LightInfo.NumMovablePointLights);
			}

			// Set dynamic point lights
			SetShaderValueArray(RHICmdList, PixelShader, LightPositionAndInvRadiusParameter, LightInfo.LightPositionAndInvRadius, LightInfo.NumMovablePointLights);
			SetShaderValueArray(RHICmdList, PixelShader, LightColorAndFalloffExponentParameter, LightInfo.LightColorAndFalloffExponent, LightInfo.NumMovablePointLights);
		}

		if (CSMDebugHintParams.IsBound())
		{
			static const auto CVarsCSMDebugHint = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Mobile.Shadow.CSMDebugHint"));
			
			float CSMDebugValue = CVarsCSMDebugHint->GetValueOnRenderThread();
			SetShaderValue(RHICmdList, PixelShader, CSMDebugHintParams, CSMDebugValue);
		}

		FMaterialShader::SetParametersInner(RHICmdList, PixelShader, InMaterialRenderProxy, InMaterialResource, View);
		uint32 DataFlags = 0;
		FMeshMaterialShader::SetMesh(RHICmdList, PixelShader, InVertexFactory, View, Proxy, BatchElement, DrawRenderState, DataFlags);
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
		FUniformBufferRHIParamRef PassUniformBufferValue,
		const TMobileBasePassShaderElementData<LightMapPolicyType>& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const;

private:
	static bool ModifyCompilationEnvironmentForQualityLevel(EShaderPlatform Platform, EMaterialQualityLevel::Type QualityLevel, FShaderCompilerEnvironment& OutEnvironment);
};

template<typename LightMapPolicyType>
class TMobileBasePassPSBaseType : public TMobileBasePassPSPolicyParamType<LightMapPolicyType>
{
	typedef TMobileBasePassPSPolicyParamType<LightMapPolicyType> Super;

public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return LightMapPolicyType::ShouldCompilePermutation(Platform, Material, VertexFactoryType) 
			&& Super::ShouldCompilePermutation(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	TMobileBasePassPSBaseType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : Super(Initializer) {}
	TMobileBasePassPSBaseType() {}
};


namespace MobileBasePass
{
	ELightMapPolicyType SelectMeshLightmapPolicy(
		const FScene* Scene, 
		const FMeshBatch& MeshBatch, 
		const FPrimitiveSceneProxy* PrimitiveSceneProxy, 
		const FLightSceneInfo* MobileDirectionalLight, 
		EMaterialShadingModel ShadingModel, 
		bool bPrimReceivesCSM, 
		ERHIFeatureLevel::Type FeatureLevel);

	void GetShaders(
		ELightMapPolicyType LightMapPolicyType,
		int32 NumMovablePointLights, 
		const FMaterial& MaterialResource,
		FVertexFactoryType* VertexFactoryType,
		bool bEnableSkyLight, 
		TMobileBasePassVSPolicyParamType<FUniformLightMapPolicy>*& VertexShader,
		TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>*& PixelShader);

	const FLightSceneInfo* GetDirectionalLightInfo(const FScene* Scene, const FPrimitiveSceneProxy* PrimitiveSceneProxy);
	int32 CalcNumMovablePointLights(const FMaterial& InMaterial, const FPrimitiveSceneProxy* InPrimitiveSceneProxy);
		
	bool StaticCanReceiveCSM(const FLightSceneInfo* LightSceneInfo, const FPrimitiveSceneProxy* PrimitiveSceneProxy);

	void SetOpaqueRenderState(FDrawingPolicyRenderState& DrawRenderState, const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FMaterial& Material, bool bEnableReceiveDecalOutput);
	void SetTranslucentRenderState(FDrawingPolicyRenderState& DrawRenderState, const FMaterial& Material);

	void ComputeBasePassSortKeys(const FScene& Scene, const FViewInfo& View, FMeshCommandOneFrameArray& VisibleMeshCommands);
};


inline bool UseSkylightPermutation(bool bEnableSkyLight, int32 MobileSkyLightPermutationOptions)
{
	if (bEnableSkyLight)
	{
		return MobileSkyLightPermutationOptions == 0 || MobileSkyLightPermutationOptions == 2;
	}
	else
	{
		return MobileSkyLightPermutationOptions == 0 || MobileSkyLightPermutationOptions == 1;
	}
}

template< typename LightMapPolicyType, EOutputFormat OutputFormat, bool bEnableSkyLight, int32 NumMovablePointLights>
class TMobileBasePassPS : public TMobileBasePassPSBaseType<LightMapPolicyType>
{
	DECLARE_SHADER_TYPE(TMobileBasePassPS,MeshMaterial);
public:

	static bool ShouldCompilePermutation(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{		
		// We compile the point light shader combinations based on the project settings
		static auto* MobileDynamicPointLightsUseStaticBranchCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileDynamicPointLightsUseStaticBranch"));
		static auto* MobileNumDynamicPointLightsCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileNumDynamicPointLights"));
		static auto* MobileSkyLightPermutationCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.SkyLightPermutation"));
		const bool bMobileDynamicPointLightsUseStaticBranch = (MobileDynamicPointLightsUseStaticBranchCVar->GetValueOnAnyThread() == 1);
		const int32 MobileNumDynamicPointLights = MobileNumDynamicPointLightsCVar->GetValueOnAnyThread();
		const int32 MobileSkyLightPermutationOptions = MobileSkyLightPermutationCVar->GetValueOnAnyThread();
		const bool bIsUnlit = Material->GetShadingModel() == MSM_Unlit;

		// Only compile skylight version for lit materials on ES2 (Metal) or higher
		const bool bShouldCacheBySkylight = !bEnableSkyLight || !bIsUnlit;

		// Only compile skylight permutations when they are enabled
		if (!bIsUnlit && !UseSkylightPermutation(bEnableSkyLight, MobileSkyLightPermutationOptions))
		{
			return false;
		}

		const bool bShouldCacheByNumDynamicPointLights =
			(NumMovablePointLights == 0 ||
			(!bIsUnlit && NumMovablePointLights == INT32_MAX && bMobileDynamicPointLightsUseStaticBranch && MobileNumDynamicPointLights > 0) ||	// single shader for variable number of point lights
				(!bIsUnlit && NumMovablePointLights <= MobileNumDynamicPointLights && !bMobileDynamicPointLightsUseStaticBranch));				// unique 1...N point light shaders

		return TMobileBasePassPSBaseType<LightMapPolicyType>::ShouldCompilePermutation(Platform, Material, VertexFactoryType) && ShouldCacheShaderByPlatformAndOutputFormat(Platform, OutputFormat) && bShouldCacheBySkylight && bShouldCacheByNumDynamicPointLights;
	}
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{		
		TMobileBasePassPSBaseType<LightMapPolicyType>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_SKY_LIGHT"), bEnableSkyLight);
		OutEnvironment.SetDefine(TEXT("OUTPUT_GAMMA_SPACE"), OutputFormat == LDR_GAMMA_32);
		if (NumMovablePointLights == INT32_MAX)
		{
			OutEnvironment.SetDefine(TEXT("MAX_DYNAMIC_POINT_LIGHTS"), (uint32)MAX_BASEPASS_DYNAMIC_POINT_LIGHTS);
			OutEnvironment.SetDefine(TEXT("VARIABLE_NUM_DYNAMIC_POINT_LIGHTS"), (uint32)1);
		}
		else
		{
			OutEnvironment.SetDefine(TEXT("MAX_DYNAMIC_POINT_LIGHTS"), (uint32)NumMovablePointLights);
			OutEnvironment.SetDefine(TEXT("VARIABLE_NUM_DYNAMIC_POINT_LIGHTS"), (uint32)0);
			OutEnvironment.SetDefine(TEXT("NUM_DYNAMIC_POINT_LIGHTS"), (uint32)NumMovablePointLights);
		}
	}
	
	/** Initialization constructor. */
	TMobileBasePassPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		TMobileBasePassPSBaseType<LightMapPolicyType>(Initializer)
	{}

	/** Default constructor. */
	TMobileBasePassPS() {}
};

/**
 * Draws the emissive color and the light-map of a mesh.
 */
class FMobileBasePassUniformDrawingPolicy : public FMeshDrawingPolicy
{
public:
	/** The data the drawing policy uses for each mesh element. */
	class ElementDataType
	{
	public:
		/** The element's light-map data. */
		typename FUniformLightMapPolicy::ElementDataType LightMapElementData;

		/** Default constructor. */
		ElementDataType()
		{}

		/** Initialization constructor. */
		ElementDataType(const FUniformLightMapPolicy::ElementDataType& InLightMapElementData)
		:	LightMapElementData(InLightMapElementData)
		{}
	};

	/** Initialization constructor. */
	FMobileBasePassUniformDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		ELightMapPolicyType InLightMapPolicyType,
		int32 InNumMovablePointLights,
		EBlendMode InBlendMode,
		bool bInEnableSkyLight,
		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
		ERHIFeatureLevel::Type FeatureLevel,
		bool bInEnableReceiveDecalOutput = false
		):
		FMeshDrawingPolicy(nullptr, nullptr, InMaterialResource, InOverrideSettings),
		VertexDeclaration(InVertexFactory->GetDeclaration()),
		UniformLightMapPolicy(InLightMapPolicyType),
		NumMovablePointLights(InNumMovablePointLights),
		BlendMode(InBlendMode),
		bEnableReceiveDecalOutput(bInEnableReceiveDecalOutput)
	{
		static_assert(MAX_BASEPASS_DYNAMIC_POINT_LIGHTS == 4, "If you change MAX_BASEPASS_DYNAMIC_POINT_LIGHTS, you need to change the switch statement below");

		if (InMaterialRenderProxy != nullptr)
		{
			ImmutableSamplerState = InMaterialRenderProxy->ImmutableSamplerState;
		}
				
		MobileBasePass::GetShaders(InLightMapPolicyType, InNumMovablePointLights, InMaterialResource, InVertexFactory->GetType(), bInEnableSkyLight, VertexShader, PixelShader);
		BaseVertexShader = VertexShader;
	}

	// FMeshDrawingPolicy interface.

	FDrawingPolicyMatchResult Matches(const FMobileBasePassUniformDrawingPolicy& Other, bool bForReals = false) const
	{
		DRAWING_POLICY_MATCH_BEGIN
			DRAWING_POLICY_MATCH(MaterialResource == Other.MaterialResource) &&
			DRAWING_POLICY_MATCH(VertexDeclaration == Other.VertexDeclaration) &&
			DRAWING_POLICY_MATCH(bIsDitheredLODTransitionMaterial == Other.bIsDitheredLODTransitionMaterial) &&
			DRAWING_POLICY_MATCH(bUsePositionOnlyVS == Other.bUsePositionOnlyVS) &&
			DRAWING_POLICY_MATCH(MeshFillMode == Other.MeshFillMode) &&
			DRAWING_POLICY_MATCH(MeshCullMode == Other.MeshCullMode) &&
			DRAWING_POLICY_MATCH(MeshPrimitiveType == Other.MeshPrimitiveType) &&
			DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader) &&
			DRAWING_POLICY_MATCH(PixelShader == Other.PixelShader) &&
			DRAWING_POLICY_MATCH(UniformLightMapPolicy.GetIndirectPolicy() == Other.UniformLightMapPolicy.GetIndirectPolicy()) &&
			DRAWING_POLICY_MATCH(NumMovablePointLights == Other.NumMovablePointLights) &&
			DRAWING_POLICY_MATCH(bEnableReceiveDecalOutput == Other.bEnableReceiveDecalOutput) &&
			DRAWING_POLICY_MATCH(ImmutableSamplerState == Other.ImmutableSamplerState);
		DRAWING_POLICY_MATCH_END 
	}

	uint32 GetTypeHash() const
	{
		return PointerHash(VertexDeclaration, PointerHash(MaterialResource));
	}

	const FMaterialRenderProxy* GetPipelineMaterialRenderProxy(const FMaterialRenderProxy* ElementMaterialRenderProxy)
	{
		return ElementMaterialRenderProxy;
	}

	friend int32 CompareDrawingPolicy(const FMobileBasePassUniformDrawingPolicy& A, const FMobileBasePassUniformDrawingPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(MaterialResource);
		COMPAREDRAWINGPOLICYMEMBERS(NumMovablePointLights);
		return CompareDrawingPolicy(A.UniformLightMapPolicy, B.UniformLightMapPolicy);
	}

	void SetupPipelineState(FDrawingPolicyRenderState& DrawRenderState, const FViewInfo& View) const
	{
		bool bEncodedHDR = GetMobileHDRMode() == EMobileHDRMode::EnabledRGBE && MaterialResource->GetMaterialDomain() != MD_UI;;

		static const auto CVarMonoscopicFarField = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MonoscopicFarField"));
		const bool bIsMobileMonoscopic = CVarMonoscopicFarField && (CVarMonoscopicFarField->GetValueOnRenderThread() != 0);

		if (bEncodedHDR == false)
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
				if (MaterialResource->ShouldWriteOnlyAlpha())
				{
					DrawRenderState.SetBlendState(TStaticBlendState<CW_ALPHA, BO_Add, BF_Zero, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI());
				} 
				else if (bIsMobileMonoscopic)
				{
					DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_One>::GetRHI());
				}
				else
				{
					DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
				}
				break;
			case BLEND_Additive:
				// Add to the existing scene color
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
				break;
			case BLEND_Modulate:
				// Modulate with the existing scene color
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI());
				break;
			case BLEND_AlphaComposite:
				// Blend with existing scene color. New color is already pre-multiplied by alpha.
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
				break;
			};
		}
		else
		{
			DrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
		}

		if (bEnableReceiveDecalOutput && View.bSceneHasDecals)
		{
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<
				true, CF_GreaterEqual,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
				0x00, GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1)>::GetRHI());
		}
	}

	void SetSharedState(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, const FViewInfo* View, const ContextDataType PolicyContext) const
	{
		VertexShader->SetParameters(RHICmdList, View, DrawRenderState);
		PixelShader->SetParameters(RHICmdList, View, DrawRenderState);
	}

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @return new bound shader state object
	*/
	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		return FBoundShaderStateInput(
			VertexDeclaration, 
			VertexShader->GetVertexShader(),
			FHullShaderRHIRef(), 
			FDomainShaderRHIRef(), 
			PixelShader->GetPixelShader(),
			FGeometryShaderRHIRef());
	}

	void SetMeshRenderState(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& Mesh,
		int32 BatchElementIndex,
		const FDrawingPolicyRenderState& DrawRenderState,
		const ElementDataType& ElementData,
		const ContextDataType PolicyContext
		) const
	{
		const FMaterialRenderProxy* MeshMaterialRenderProxy = Mesh.MaterialRenderProxy;
		const FVertexFactory* MeshVertexFactory = Mesh.VertexFactory;

		check(MeshVertexFactory && MeshVertexFactory->IsInitialized());
		MeshVertexFactory->SetStreams(View.FeatureLevel, RHICmdList);

		// Set the light-map policy's mesh-specific settings.
		UniformLightMapPolicy.SetMesh(
			RHICmdList, 
			View,
			PrimitiveSceneProxy,
			VertexShader,
			PixelShader,
			VertexShader,
			PixelShader,
			MeshVertexFactory,
			MeshMaterialRenderProxy,
			ElementData.LightMapElementData);

		const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];
		VertexShader->SetMesh(RHICmdList, *MaterialResource, View, MeshVertexFactory, MeshMaterialRenderProxy, PrimitiveSceneProxy, BatchElement, DrawRenderState);
		PixelShader->SetMesh(RHICmdList, *MaterialResource, View, MeshVertexFactory, MeshMaterialRenderProxy, PrimitiveSceneProxy, BatchElement, DrawRenderState, NumMovablePointLights);

		// Set directional light UB
		const TShaderUniformBufferParameter<FMobileDirectionalLightShaderParameters>& MobileDirectionalLightParam = PixelShader->template GetUniformBufferParameter<FMobileDirectionalLightShaderParameters>();
		if (MobileDirectionalLightParam.IsBound())
		{
			int32 UniformBufferIndex = PrimitiveSceneProxy ? GetFirstLightingChannelFromMask(PrimitiveSceneProxy->GetLightingChannelMask()) + 1 : 0;
			SetUniformBufferParameter(RHICmdList, PixelShader->GetPixelShader(), MobileDirectionalLightParam, View.MobileDirectionalLightUniformBuffers[UniformBufferIndex]);
		}

		if (bEnableReceiveDecalOutput && View.bSceneHasDecals)
		{
			const uint8 StencilValue = (PrimitiveSceneProxy && !PrimitiveSceneProxy->ReceivesDecals() ? 0x01 : 0x00);
			RHICmdList.SetStencilRef(GET_STENCIL_BIT_MASK(RECEIVE_DECAL, StencilValue)); // we hash the stencil group because we only have 6 bits.
		}
	}

public:
	TMobileBasePassVSPolicyParamType<FUniformLightMapPolicy>* VertexShader;
	TMobileBasePassPSPolicyParamType<FUniformLightMapPolicy>* PixelShader;
protected:
	FVertexDeclarationRHIRef VertexDeclaration;
	FUniformLightMapPolicy UniformLightMapPolicy;
	int32 NumMovablePointLights;
	FImmutableSamplerState ImmutableSamplerState;
	EBlendMode BlendMode;
	uint32 bEnableReceiveDecalOutput : 1;
};

/**
 * A drawing policy factory for the base pass drawing policy.
 */
class FMobileBasePassOpaqueDrawingPolicyFactory
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
		FHitProxyId HitProxyId
		);
};


/** The parameters used to process a mobile base pass mesh. */
class FMobileProcessBasePassMeshParameters
{
public:
	const FMeshBatch& Mesh;
	const uint64 BatchElementMask;
	const FMaterial& Material;
	const FPrimitiveSceneProxy* PrimitiveSceneProxy;
	int32 NumMovablePointLights;
	EBlendMode BlendMode;
	EMaterialShadingModel ShadingModel;
	const bool bAllowFog;
	ERHIFeatureLevel::Type FeatureLevel;
	const bool bIsInstancedStereo;

	/** Initialization constructor. */
	FMobileProcessBasePassMeshParameters(
		const FMeshBatch& InMesh,
		const FMaterial& InMaterial,
		const FPrimitiveSceneProxy* InPrimitiveSceneProxy,
		bool InbAllowFog,
		ERHIFeatureLevel::Type InFeatureLevel,
		const bool InbIsInstancedStereo = false
	) :
		Mesh(InMesh),
		BatchElementMask(Mesh.Elements.Num() == 1 ? 1 : (1 << Mesh.Elements.Num()) - 1), // 1 bit set for each mesh element
		Material(InMaterial),
		PrimitiveSceneProxy(InPrimitiveSceneProxy),
		NumMovablePointLights(MobileBasePass::CalcNumMovablePointLights(InMaterial, InPrimitiveSceneProxy)),
		BlendMode(InMaterial.GetBlendMode()),
		ShadingModel(InMaterial.GetShadingModel()),
		bAllowFog(InbAllowFog),
		FeatureLevel(InFeatureLevel),
		bIsInstancedStereo(InbIsInstancedStereo)
	{
	}

	/** Initialization constructor. */
	FMobileProcessBasePassMeshParameters(
		const FMeshBatch& InMesh,
		const uint64& InBatchElementMask,
		const FMaterial& InMaterial,
		const FPrimitiveSceneProxy* InPrimitiveSceneProxy,
		bool InbAllowFog,
		ERHIFeatureLevel::Type InFeatureLevel,
		bool InbIsInstancedStereo = false
	) :
		Mesh(InMesh),
		BatchElementMask(InBatchElementMask),
		Material(InMaterial),
		PrimitiveSceneProxy(InPrimitiveSceneProxy),
		NumMovablePointLights(MobileBasePass::CalcNumMovablePointLights(InMaterial, InPrimitiveSceneProxy)),
		BlendMode(InMaterial.GetBlendMode()),
		ShadingModel(InMaterial.GetShadingModel()),
		bAllowFog(InbAllowFog),
		FeatureLevel(InFeatureLevel),
		bIsInstancedStereo(InbIsInstancedStereo)
	{
	}
};


class FMobileBasePassMeshProcessor : public FMeshPassProcessor
{
public:
	FMobileBasePassMeshProcessor(
		const FScene* InScene, 
		ERHIFeatureLevel::Type InFeatureLevel, 
		const FSceneView* InViewIfDynamicMeshCommand, 
		const FDrawingPolicyRenderState& InDrawRenderState, 
		FMeshPassDrawListContext& InDrawListContext,
		bool bInCanReceiveCSM,
		ETranslucencyPass::Type InTranslucencyPassType = ETranslucencyPass::TPT_MAX);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;

	FDrawingPolicyRenderState PassDrawRenderState;

private:
	void Process(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		EBlendMode BlendMode,
		EMaterialShadingModel ShadingModel,
		const ELightMapPolicyType LightMapPolicyType,
		const FUniformLightMapPolicy::ElementDataType& RESTRICT LightMapElementData);
			
	const ETranslucencyPass::Type TranslucencyPassType;
	const bool bTranslucentBasePass;
	const bool bCanReceiveCSM;
};
