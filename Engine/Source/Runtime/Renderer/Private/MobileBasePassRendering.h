// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
#include "DebugViewModeRendering.h"
#include "FogRendering.h"
#include "PlanarReflectionRendering.h"
#include "BasePassRendering.h"

BEGIN_UNIFORM_BUFFER_STRUCT(FMobileBasePassUniformParameters, )
	UNIFORM_MEMBER_STRUCT(FFogUniformParameters, Fog)
	UNIFORM_MEMBER_STRUCT(FMobileSceneTextureUniformParameters, SceneTextures)
END_UNIFORM_BUFFER_STRUCT(FMobileBasePassUniformParameters)

extern void CreateMobileBasePassUniformBuffer(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	bool bTranslucentPass,
	TUniformBufferRef<FMobileBasePassUniformParameters>& BasePassUniformBuffer);

class FPlanarReflectionSceneProxy;
class FScene;

enum EOutputFormat
{
	LDR_GAMMA_32,
	HDR_LINEAR_64,
};

#define MAX_BASEPASS_DYNAMIC_POINT_LIGHTS 4

const FLightSceneInfo* GetSceneMobileDirectionalLights(FScene const* Scene, uint32 LightChannel);

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

/**
 * The base shader type for vertex shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */

template<typename VertexParametersType>
class TMobileBasePassVSPolicyParamType : public FMeshMaterialShader, public VertexParametersType
{
protected:

	TMobileBasePassVSPolicyParamType() {}
	TMobileBasePassVSPolicyParamType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		VertexParametersType::Bind(Initializer.ParameterMap);
		PassUniformBuffer.Bind(Initializer.ParameterMap, FMobileBasePassUniformParameters::StaticStruct.GetShaderVariableName());
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
		VertexParametersType::Serialize(Ar);
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
};

template<typename LightMapPolicyType>
class TMobileBasePassVSBaseType : public TMobileBasePassVSPolicyParamType<typename LightMapPolicyType::VertexParametersType>
{
	typedef TMobileBasePassVSPolicyParamType<typename LightMapPolicyType::VertexParametersType> Super;

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

// If no reflection captures are available then attempt to use sky light's texture.
bool UseSkyReflectionCapture(const FScene* RenderScene);
void GetSkyTextureParams(const FScene* Scene, float& AverageBrightnessOUT, FTexture*& ReflectionTextureOUT, float& OutSkyMaxMipIndex);

/**
 * The base type for pixel shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */

template<typename PixelParametersType>
class TMobileBasePassPSPolicyParamType : public FMeshMaterialShader, public PixelParametersType
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
		PixelParametersType::Bind(Initializer.ParameterMap);
		PassUniformBuffer.Bind(Initializer.ParameterMap, FMobileBasePassUniformParameters::StaticStruct.GetShaderVariableName());
		ReflectionCubemap.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemap"));
		ReflectionSampler.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemapSampler"));
		InvReflectionCubemapAverageBrightness.Bind(Initializer.ParameterMap, TEXT("InvReflectionCubemapAverageBrightness"));
		LightPositionAndInvRadiusParameter.Bind(Initializer.ParameterMap, TEXT("LightPositionAndInvRadius"));
		LightColorAndFalloffExponentParameter.Bind(Initializer.ParameterMap, TEXT("LightColorAndFalloffExponent"));
			NumDynamicPointLightsParameter.Bind(Initializer.ParameterMap, TEXT("NumDynamicPointLights"));
		ReflectionPositionsAndRadii.Bind(Initializer.ParameterMap, TEXT("ReflectionPositionsAndRadii"));
		ReflectionCubemap1.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemap1"));
		ReflectionSampler1.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemapSampler1"));
		ReflectionCubemap2.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemap2"));
		ReflectionSampler2.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemapSampler2"));

		MobileSkyReflectionParam.Bind(Initializer.ParameterMap, TEXT("MobileSkyReflectionParams"));

		CSMDebugHintParams.Bind(Initializer.ParameterMap, TEXT("CSMDebugHint"));

		PlanarReflectionParams.Bind(Initializer.ParameterMap);
	}
	TMobileBasePassPSPolicyParamType() {}

	// Set parameters specific to PSO
	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo* View, 
		const FDrawingPolicyRenderState& DrawRenderState)
	{
		// If we're using only the sky for reflection then set it once here.
		FScene* RenderScene = View->Family->Scene->GetRenderScene();
		if (UseSkyReflectionCapture(RenderScene))
		{
			// MobileSkyReflectionValues.x == max sky cube mip.
			// if >0 this will disable shader's RGBM decoding and enable sky light tinting of this envmap.
			FTexture* ReflectionTexture = GBlackTextureCube;
			float AverageBrightness = 1.0f;
			FVector4 MobileSkyReflectionValues(ForceInit);
			if (View->GetFeatureLevel() > ERHIFeatureLevel::ES2) // not-supported on ES2 at the moment
			{
				GetSkyTextureParams(RenderScene, AverageBrightness, ReflectionTexture, MobileSkyReflectionValues.X);
			}
			FRHIPixelShader* PixelShader = GetPixelShader();
			// Set the reflection cubemap
			SetTextureParameter(RHICmdList, PixelShader, ReflectionCubemap, ReflectionSampler, ReflectionTexture);
			SetShaderValue(RHICmdList, PixelShader, InvReflectionCubemapAverageBrightness, FVector(1.0f / AverageBrightness, 0, 0));
			SetShaderValue(RHICmdList, PixelShader, MobileSkyReflectionParam, MobileSkyReflectionValues);
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

			SetShaderValue(RHICmdList, PixelShader, InvReflectionCubemapAverageBrightness, FVector(1.0f / AverageBrightness.X, 1.0f / AverageBrightness.Y, 1.0f / AverageBrightness.Z));
		}
		else if (ReflectionCubemap.IsBound() && (!PrimitiveSceneInfo || !UseSkyReflectionCapture(PrimitiveSceneInfo->Scene)))
		{
			FTexture* ReflectionTexture = GBlackTextureCube;
			float AverageBrightness = 1.0f;
			FVector4 MobileSkyReflectionValues(ForceInit);

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
			SetShaderValue(RHICmdList, PixelShader, InvReflectionCubemapAverageBrightness, FVector(1.0f / AverageBrightness, 0, 0));
			SetShaderValue(RHICmdList, PixelShader, MobileSkyReflectionParam, MobileSkyReflectionValues);
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

		const FPlanarReflectionSceneProxy* CachedPlanarReflectionProxy = PrimitiveSceneInfo ? PrimitiveSceneInfo->CachedPlanarReflectionProxy : nullptr;
		PlanarReflectionParams.SetParameters(RHICmdList, PixelShader, View, CachedPlanarReflectionProxy );

		FMaterialShader::SetParametersInner(RHICmdList, PixelShader, InMaterialRenderProxy, InMaterialResource, View);
		uint32 DataFlags = 0;
		FMeshMaterialShader::SetMesh(RHICmdList, PixelShader, InVertexFactory, View, Proxy, BatchElement, DrawRenderState, DataFlags);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		PixelParametersType::Serialize(Ar);
		Ar << BasePassUniformBuffer;
		Ar << ReflectionCubemap;
		Ar << ReflectionSampler;
		Ar << InvReflectionCubemapAverageBrightness;
		Ar << LightPositionAndInvRadiusParameter;
		Ar << LightColorAndFalloffExponentParameter;
		Ar << NumDynamicPointLightsParameter;
		Ar << MobileSkyReflectionParam;
		Ar << ReflectionCubemap1;
		Ar << ReflectionCubemap2;
		Ar << ReflectionPositionsAndRadii;
		Ar << ReflectionSampler1;
		Ar << ReflectionSampler2;
		Ar << PlanarReflectionParams;

		Ar << CSMDebugHintParams;

		return bShaderHasOutdatedParameters;
	}

private:

	static bool ModifyCompilationEnvironmentForQualityLevel(EShaderPlatform Platform, EMaterialQualityLevel::Type QualityLevel, FShaderCompilerEnvironment& OutEnvironment);

	FShaderUniformBufferParameter BasePassUniformBuffer;
	FShaderResourceParameter ReflectionCubemap;
	FShaderResourceParameter ReflectionSampler;
	FShaderParameter InvReflectionCubemapAverageBrightness;
	FShaderParameter LightPositionAndInvRadiusParameter;
	FShaderParameter MobileSkyReflectionParam;
	FShaderParameter LightColorAndFalloffExponentParameter;
	FShaderParameter NumDynamicPointLightsParameter;


	//////////////////////////////////////////////////////////////////////////
	FShaderResourceParameter ReflectionCubemap1;
	FShaderResourceParameter ReflectionSampler1;
	FShaderResourceParameter ReflectionCubemap2;
	FShaderResourceParameter ReflectionSampler2;
	FShaderParameter ReflectionPositionsAndRadii;
	//////////////////////////////////////////////////////////////////////////
	FPlanarReflectionParameters PlanarReflectionParams;

	FShaderParameter CSMDebugHintParams;
};

template<typename LightMapPolicyType>
class TMobileBasePassPSBaseType : public TMobileBasePassPSPolicyParamType<typename LightMapPolicyType::PixelParametersType>
{
	typedef TMobileBasePassPSPolicyParamType<typename LightMapPolicyType::PixelParametersType> Super;

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
 * Get shader templates allowing to redirect between compatible shaders.
 */

template <typename LightMapPolicyType, int32 NumMovablePointLights>
struct GetMobileBasePassShaders
{
	GetMobileBasePassShaders(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	LightMapPolicyType LightMapPolicy, 
	bool bEnableSkyLight,
	TMobileBasePassVSPolicyParamType<typename LightMapPolicyType::VertexParametersType>*& VertexShader,
	TMobileBasePassPSPolicyParamType<typename LightMapPolicyType::PixelParametersType>*& PixelShader
	)
	{
		if (IsMobileHDR())
		{
			VertexShader = Material.GetShader<TMobileBasePassVS<LightMapPolicyType, HDR_LINEAR_64> >(VertexFactoryType);

			if (bEnableSkyLight)
			{
				PixelShader = Material.GetShader< TMobileBasePassPS<LightMapPolicyType, HDR_LINEAR_64, true, NumMovablePointLights> >(VertexFactoryType);
			}
			else
			{
				PixelShader = Material.GetShader< TMobileBasePassPS<LightMapPolicyType, HDR_LINEAR_64, false, NumMovablePointLights> >(VertexFactoryType);
			}
		}
		else
		{
			VertexShader = Material.GetShader<TMobileBasePassVS<LightMapPolicyType, LDR_GAMMA_32> >(VertexFactoryType);

			if (bEnableSkyLight)
			{
				PixelShader = Material.GetShader< TMobileBasePassPS<LightMapPolicyType, LDR_GAMMA_32, true, NumMovablePointLights> >(VertexFactoryType);
			}
			else
			{
				PixelShader = Material.GetShader< TMobileBasePassPS<LightMapPolicyType, LDR_GAMMA_32, false, NumMovablePointLights> >(VertexFactoryType);
			}			
		}
	}
};

// Only using a struct/class allows partial specialisation here
template <int32 NumMovablePointLights>
struct GetMobileBasePassShaders<FUniformLightMapPolicy, NumMovablePointLights>
{
	GetMobileBasePassShaders(
		const FMaterial& Material, 
		FVertexFactoryType* VertexFactoryType, 
		FUniformLightMapPolicy LightMapPolicy, 
		bool bEnableSkyLight,
		TMobileBasePassVSPolicyParamType<typename FUniformLightMapPolicy::VertexParametersType>*& VertexShader,
		TMobileBasePassPSPolicyParamType<typename FUniformLightMapPolicy::PixelParametersType>*& PixelShader
		);
};

template <ELightMapPolicyType Policy, int32 NumMovablePointLights>
void GetUniformMobileBasePassShaders(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	bool bEnableSkyLight,
	TMobileBasePassVSPolicyParamType<FUniformLightMapPolicyShaderParametersType>*& VertexShader,
	TMobileBasePassPSPolicyParamType<FUniformLightMapPolicyShaderParametersType>*& PixelShader
	)
{
	if (IsMobileHDR())
	{
		VertexShader = Material.GetShader<TMobileBasePassVS<TUniformLightMapPolicy<Policy>, HDR_LINEAR_64> >(VertexFactoryType);

		if (bEnableSkyLight)
		{
			PixelShader = Material.GetShader< TMobileBasePassPS<TUniformLightMapPolicy<Policy>, HDR_LINEAR_64, true, NumMovablePointLights> >(VertexFactoryType);
		}
		else
		{
			PixelShader = Material.GetShader< TMobileBasePassPS<TUniformLightMapPolicy<Policy>, HDR_LINEAR_64, false, NumMovablePointLights> >(VertexFactoryType);
		}	
	}
	else
	{
		VertexShader = Material.GetShader<TMobileBasePassVS<TUniformLightMapPolicy<Policy>, LDR_GAMMA_32> >(VertexFactoryType);

		if (bEnableSkyLight)
		{
			PixelShader = Material.GetShader< TMobileBasePassPS<TUniformLightMapPolicy<Policy>, LDR_GAMMA_32, true, NumMovablePointLights> >(VertexFactoryType);
		}
		else
		{
			PixelShader = Material.GetShader< TMobileBasePassPS<TUniformLightMapPolicy<Policy>, LDR_GAMMA_32, false, NumMovablePointLights> >(VertexFactoryType);
		}			
	}
}

template <int32 NumMovablePointLights>
GetMobileBasePassShaders<FUniformLightMapPolicy, NumMovablePointLights>::GetMobileBasePassShaders(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	FUniformLightMapPolicy LightMapPolicy, 
	bool bEnableSkyLight,
	TMobileBasePassVSPolicyParamType<FUniformLightMapPolicyShaderParametersType>*& VertexShader,
	TMobileBasePassPSPolicyParamType<FUniformLightMapPolicyShaderParametersType>*& PixelShader
	)
{
	switch (LightMapPolicy.GetIndirectPolicy())
	{
	case LMP_LQ_LIGHTMAP:
		GetUniformMobileBasePassShaders<LMP_LQ_LIGHTMAP, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP:
			GetUniformMobileBasePassShaders<LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM:
			GetUniformMobileBasePassShaders<LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
			break;
	case LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT:
			GetUniformMobileBasePassShaders<LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT:
			GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT:
			GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT:
			GetUniformMobileBasePassShaders<LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT:
			GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM:
			GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP:
			GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP:
			GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	default:										
		check(false);
	case LMP_NO_LIGHTMAP:
		GetUniformMobileBasePassShaders<LMP_NO_LIGHTMAP, NumMovablePointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	}
}

/**
 * Draws the emissive color and the light-map of a mesh.
 */
template<typename LightMapPolicyType>
class TMobileBasePassDrawingPolicy : public FMeshDrawingPolicy
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
	TMobileBasePassDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		LightMapPolicyType InLightMapPolicy,
		int32 InNumMovablePointLights,
		EBlendMode InBlendMode,
		bool bInEnableSkyLight,
		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
		EDebugViewShaderMode InDebugViewShaderMode,
		ERHIFeatureLevel::Type FeatureLevel,
		bool bInEnableReceiveDecalOutput = false
		):
		FMeshDrawingPolicy(nullptr, nullptr, InMaterialResource, InOverrideSettings, InDebugViewShaderMode),
		VertexDeclaration(InVertexFactory->GetDeclaration()),
		LightMapPolicy(InLightMapPolicy),
		NumMovablePointLights(InNumMovablePointLights),
		BlendMode(InBlendMode),
		bEnableReceiveDecalOutput(bInEnableReceiveDecalOutput)
	{
		static_assert(MAX_BASEPASS_DYNAMIC_POINT_LIGHTS == 4, "If you change MAX_BASEPASS_DYNAMIC_POINT_LIGHTS, you need to change the switch statement below");

		if (InMaterialRenderProxy != nullptr)
		{
			ImmutableSamplerState = InMaterialRenderProxy->ImmutableSamplerState;
		}
		
		// use only existing sky-light permutation
		bool bIsLit = (InMaterialResource.GetShadingModel() != MSM_Unlit);
		if (bIsLit && !UseSkylightPermutation(bInEnableSkyLight, FReadOnlyCVARCache::Get().MobileSkyLightPermutation))	
		{
			bInEnableSkyLight = !bInEnableSkyLight;
		}
		
		switch (NumMovablePointLights)
		{
		case INT32_MAX:
			GetMobileBasePassShaders<LightMapPolicyType, INT32_MAX>(
				InMaterialResource,
				InVertexFactory->GetType(),
				InLightMapPolicy,
				bInEnableSkyLight,
				VertexShader,
				PixelShader
				);
			break;
		case 1:
			GetMobileBasePassShaders<LightMapPolicyType, 1>(
					InMaterialResource, 
					InVertexFactory->GetType(), 
					InLightMapPolicy, 
					bInEnableSkyLight, 
					VertexShader,
					PixelShader
					);
			break;
		case 2:
			GetMobileBasePassShaders<LightMapPolicyType, 2>(
				InMaterialResource,
				InVertexFactory->GetType(),
				InLightMapPolicy,
				bInEnableSkyLight,
				VertexShader,
				PixelShader
				);
			break;
		case 3:
			GetMobileBasePassShaders<LightMapPolicyType, 3>(
				InMaterialResource,
				InVertexFactory->GetType(),
				InLightMapPolicy,
				bInEnableSkyLight,
				VertexShader,
				PixelShader
				);
			break;
		case 4:
			GetMobileBasePassShaders<LightMapPolicyType, 4>(
				InMaterialResource,
				InVertexFactory->GetType(),
				InLightMapPolicy,
				bInEnableSkyLight,
				VertexShader,
				PixelShader
				);
			break;
		case 0:
		default:
			GetMobileBasePassShaders<LightMapPolicyType, 0>(
				InMaterialResource,
				InVertexFactory->GetType(),
				InLightMapPolicy,
				bInEnableSkyLight,
				VertexShader,
				PixelShader
				);
			break;
		}

		BaseVertexShader = VertexShader;
	}

	// FMeshDrawingPolicy interface.

	FDrawingPolicyMatchResult Matches(const TMobileBasePassDrawingPolicy& Other, bool bForReals = false) const
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
			DRAWING_POLICY_MATCH(LightMapPolicy == Other.LightMapPolicy) &&
			DRAWING_POLICY_MATCH(NumMovablePointLights == Other.NumMovablePointLights) &&
			DRAWING_POLICY_MATCH(bEnableReceiveDecalOutput == Other.bEnableReceiveDecalOutput) &&
			DRAWING_POLICY_MATCH(UseDebugViewPS() == Other.UseDebugViewPS()) &&
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

	friend int32 CompareDrawingPolicy(const TMobileBasePassDrawingPolicy& A,const TMobileBasePassDrawingPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(MaterialResource);
		COMPAREDRAWINGPOLICYMEMBERS(NumMovablePointLights);
		return CompareDrawingPolicy(A.LightMapPolicy,B.LightMapPolicy);
	}

	void SetupPipelineState(FDrawingPolicyRenderState& DrawRenderState, const FViewInfo& View) const
	{
		if (UseDebugViewPS())
		{
			if (View.Family->EngineShowFlags.ShaderComplexity)
			{
				if (BlendMode == BLEND_Opaque)
				{
					DrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA>::GetRHI());
				}
				else
				{
					// Add complexity to existing
					DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI());
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
					DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
					break;
				};
			}
			else
			{
				DrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
			}
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

		if (!UseDebugViewPS())
		{
			PixelShader->SetParameters(RHICmdList, View, DrawRenderState);
		}
	}

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @return new bound shader state object
	*/
	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		FPixelShaderRHIParamRef PixelShaderRHIRef = PixelShader->GetPixelShader();

		if (UseDebugViewPS())
		{
			PixelShaderRHIRef = FDebugViewMode::GetPSInterface(GetGlobalShaderMap(InFeatureLevel), MaterialResource, GetDebugViewShaderMode())->GetShader()->GetPixelShader();
		}

		return FBoundShaderStateInput(
			VertexDeclaration, 
			VertexShader->GetVertexShader(),
			FHullShaderRHIRef(), 
			FDomainShaderRHIRef(), 
			PixelShaderRHIRef,
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
		LightMapPolicy.SetMesh(
			RHICmdList, 
			View,
			PrimitiveSceneProxy,
			VertexShader,
			!UseDebugViewPS() ? PixelShader : nullptr,
			VertexShader,
			PixelShader,
			MeshVertexFactory,
			MeshMaterialRenderProxy,
			ElementData.LightMapElementData);

		const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];
		VertexShader->SetMesh(RHICmdList, *MaterialResource, View, MeshVertexFactory, MeshMaterialRenderProxy, PrimitiveSceneProxy, BatchElement, DrawRenderState);

		if (UseDebugViewPS())
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			FDebugViewMode::GetPSInterface(View.ShaderMap, MaterialResource, GetDebugViewShaderMode())->SetParameters(RHICmdList, VertexShader, PixelShader, MeshMaterialRenderProxy, *MaterialResource, View, DrawRenderState);
			FDebugViewMode::GetPSInterface(View.ShaderMap, MaterialResource, GetDebugViewShaderMode())->SetMesh(RHICmdList, MeshVertexFactory, View, PrimitiveSceneProxy, Mesh.VisualizeLODIndex, BatchElement, DrawRenderState);
#endif
		}
		else
		{
			PixelShader->SetMesh(RHICmdList, *MaterialResource, View, MeshVertexFactory, MeshMaterialRenderProxy, PrimitiveSceneProxy, BatchElement, DrawRenderState, NumMovablePointLights);

			// Set directional light UB
			const TShaderUniformBufferParameter<FMobileDirectionalLightShaderParameters>& MobileDirectionalLightParam = PixelShader->template GetUniformBufferParameter<FMobileDirectionalLightShaderParameters>();
			if (MobileDirectionalLightParam.IsBound())
			{
				int32 UniformBufferIndex = PrimitiveSceneProxy ? GetFirstLightingChannelFromMask(PrimitiveSceneProxy->GetLightingChannelMask()) + 1 : 0;
				SetUniformBufferParameter(RHICmdList, PixelShader->GetPixelShader(), MobileDirectionalLightParam, View.MobileDirectionalLightUniformBuffers[UniformBufferIndex]);
			}
		}

		if (bEnableReceiveDecalOutput && View.bSceneHasDecals)
		{
			const uint8 StencilValue = (PrimitiveSceneProxy && !PrimitiveSceneProxy->ReceivesDecals() ? 0x01 : 0x00);
			RHICmdList.SetStencilRef(GET_STENCIL_BIT_MASK(RECEIVE_DECAL, StencilValue)); // we hash the stencil group because we only have 6 bits.
		}
	}

protected:
	TMobileBasePassVSPolicyParamType<FUniformLightMapPolicyShaderParametersType>* VertexShader;
	TMobileBasePassPSPolicyParamType<FUniformLightMapPolicyShaderParametersType>* PixelShader;

	FVertexDeclarationRHIRef VertexDeclaration;
	LightMapPolicyType LightMapPolicy;
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
	const FMaterial* Material;
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
		const FMaterial* InMaterial,
		const FPrimitiveSceneProxy* InPrimitiveSceneProxy,
		bool InbAllowFog,
		ERHIFeatureLevel::Type InFeatureLevel,
		const bool InbIsInstancedStereo = false
	) :
		Mesh(InMesh),
		BatchElementMask(Mesh.Elements.Num() == 1 ? 1 : (1 << Mesh.Elements.Num()) - 1), // 1 bit set for each mesh element
		Material(InMaterial),
		PrimitiveSceneProxy(InPrimitiveSceneProxy),
		NumMovablePointLights(CalcNumMovablePointLights(InMaterial, InPrimitiveSceneProxy)),
		BlendMode(InMaterial->GetBlendMode()),
		ShadingModel(InMaterial->GetShadingModel()),
		bAllowFog(InbAllowFog),
		FeatureLevel(InFeatureLevel),
		bIsInstancedStereo(InbIsInstancedStereo)
	{
	}

	/** Initialization constructor. */
	FMobileProcessBasePassMeshParameters(
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
		NumMovablePointLights(CalcNumMovablePointLights(InMaterial, InPrimitiveSceneProxy)),
		BlendMode(InMaterial->GetBlendMode()),
		ShadingModel(InMaterial->GetShadingModel()),
		bAllowFog(InbAllowFog),
		FeatureLevel(InFeatureLevel),
		bIsInstancedStereo(InbIsInstancedStereo)
	{
	}

private:
	static int32 CalcNumMovablePointLights(const FMaterial* InMaterial, const FPrimitiveSceneProxy* InPrimitiveSceneProxy)
	{
		const FReadOnlyCVARCache& ReadOnlyCVARCache = FReadOnlyCVARCache::Get();
		const bool bIsUnlit = InMaterial->GetShadingModel() == MSM_Unlit;
		int32 OutNumMovablePointLights = (InPrimitiveSceneProxy && !bIsUnlit) ? FMath::Min<int32>(InPrimitiveSceneProxy->GetPrimitiveSceneInfo()->NumMobileMovablePointLights, ReadOnlyCVARCache.NumMobileMovablePointLights) : 0;
		if (OutNumMovablePointLights > 0 && ReadOnlyCVARCache.bMobileMovablePointLightsUseStaticBranch)
		{
			OutNumMovablePointLights = INT32_MAX;
		}
		return OutNumMovablePointLights;
	}
};


/** Processes a base pass mesh using an unknown light map policy, and unknown fog density policy. */
template<typename ProcessActionType>
void ProcessMobileBasePassMesh(
	FRHICommandList& RHICmdList,
	const FMobileProcessBasePassMeshParameters& Parameters,
	ProcessActionType&& Action
	)
{
	// Check for a cached light-map.
	const bool bIsLitMaterial = Parameters.ShadingModel != MSM_Unlit;
	if (bIsLitMaterial)
	{
		const FLightMapInteraction LightMapInteraction = (Parameters.Mesh.LCI && bIsLitMaterial)
			? Parameters.Mesh.LCI->GetLightMapInteraction(Parameters.FeatureLevel)
			: FLightMapInteraction();

		const FScene* Scene = Action.GetScene();
		const FLightSceneInfo* MobileDirectionalLight = nullptr;
		if (Parameters.PrimitiveSceneProxy && Scene)
		{
			const int32 LightChannel = GetFirstLightingChannelFromMask(Parameters.PrimitiveSceneProxy->GetLightingChannelMask());
			MobileDirectionalLight = LightChannel >= 0 ? GetSceneMobileDirectionalLights(Scene, LightChannel) : nullptr;
		}
	
		const FReadOnlyCVARCache& ReadOnlyCVARCache = FReadOnlyCVARCache::Get();

		const bool bPrimReceivesCSM = Action.CanReceiveCSM(MobileDirectionalLight, Parameters.PrimitiveSceneProxy);
		const bool bUseMovableLight = MobileDirectionalLight && !MobileDirectionalLight->Proxy->HasStaticShadowing() && ReadOnlyCVARCache.bMobileAllowMovableDirectionalLights;

		const bool bUseStaticAndCSM = MobileDirectionalLight && MobileDirectionalLight->Proxy->UseCSMForDynamicObjects()
			&& bPrimReceivesCSM
			&& ReadOnlyCVARCache.bMobileEnableStaticAndCSMShadowReceivers;

		const bool bMovableWithCSM = bUseMovableLight && MobileDirectionalLight->ShouldRenderViewIndependentWholeSceneShadows() && bPrimReceivesCSM;

		if (LightMapInteraction.GetType() == LMIT_Texture && ReadOnlyCVARCache.bAllowStaticLighting && ReadOnlyCVARCache.bEnableLowQualityLightmaps)
		{
			// Lightmap path
			const FShadowMapInteraction ShadowMapInteraction = (Parameters.Mesh.LCI && bIsLitMaterial)
				? Parameters.Mesh.LCI->GetShadowMapInteraction()
				: FShadowMapInteraction();

			if (bUseMovableLight)
			{
				// final determination of whether CSMs are rendered can be view dependent, thus we always need to clear the CSMs even if we're not going to render to them based on the condition below.
				if (MobileDirectionalLight->ShouldRenderViewIndependentWholeSceneShadows())
				{
					Action.Process(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP), Parameters.Mesh.LCI);
				}
				else
				{
					Action.Process(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP), Parameters.Mesh.LCI);
				}
			}
			else if (bUseStaticAndCSM)
			{
				if (ShadowMapInteraction.GetType() == SMIT_Texture && MobileDirectionalLight->ShouldRenderViewIndependentWholeSceneShadows() && ReadOnlyCVARCache.bMobileAllowDistanceFieldShadows)
				{
					Action.Process(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM), Parameters.Mesh.LCI);
				}
				else
				{
					Action.Process(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_LQ_LIGHTMAP), Parameters.Mesh.LCI);
				}
			}
			else
			{
				if (ShadowMapInteraction.GetType() == SMIT_Texture && ReadOnlyCVARCache.bMobileAllowDistanceFieldShadows)
				{
					Action.Process(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP), Parameters.Mesh.LCI);
				}
				else
				{
					Action.Process(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_LQ_LIGHTMAP), Parameters.Mesh.LCI);
				}
			}

			// Exit to avoid NoLightmapPolicy
			return;
		}
		else if (IsIndirectLightingCacheAllowed(Parameters.FeatureLevel) /* implies bAllowStaticLighting*/
			&& Parameters.PrimitiveSceneProxy
			// Movable objects need to get their GI from the indirect lighting cache
			&& Parameters.PrimitiveSceneProxy->IsMovable())
		{
			if (bUseMovableLight)
			{
				if (MobileDirectionalLight && MobileDirectionalLight->ShouldRenderViewIndependentWholeSceneShadows() && bMovableWithCSM)
				{
					Action.Process(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT), Parameters.Mesh.LCI);
				}
				else
				{
					Action.Process(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT), Parameters.Mesh.LCI);
				}
			}
			else
			{
				if (bUseStaticAndCSM)
				{
					Action.Process(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT), Parameters.Mesh.LCI);
				}
				else
				{
					Action.Process(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT), Parameters.Mesh.LCI);
				}
			}

			// Exit to avoid NoLightmapPolicy
			return;
		}
		else if (bUseMovableLight)
		{
			// final determination of whether CSMs are rendered can be view dependent, thus we always need to clear the CSMs even if we're not going to render to them based on the condition below.
			if (MobileDirectionalLight && bMovableWithCSM)
			{
				Action.Process(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM), Parameters.Mesh.LCI);
			}
			else
			{
				Action.Process(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT), Parameters.Mesh.LCI);
			}

			// Exit to avoid NoLightmapPolicy
			return;
		}
	}

	// Unlit uses NoLightmapPolicy with 0 point lights
	Action.Process(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_NO_LIGHTMAP), Parameters.Mesh.LCI);
}
