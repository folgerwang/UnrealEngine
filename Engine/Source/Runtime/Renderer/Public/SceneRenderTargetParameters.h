// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneRenderTargetParameters.h: Shader base classes
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "MaterialShared.h"

class FSceneView;
class FShaderParameterMap;

// Convenience parameters used by the material graph or many global shaders
//@todo - replace with rendergraph inputs and outputs, whose lifetimes can be validated (eg GBuffers not available in BasePass)
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FSceneTexturesUniformParameters, RENDERER_API)
	// Scene Color / Depth
	SHADER_PARAMETER_TEXTURE(Texture2D, SceneColorTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, SceneDepthTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SceneDepthTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D<float>, SceneDepthTextureNonMS)

	// GBuffer
	SHADER_PARAMETER_TEXTURE(Texture2D, GBufferATexture)
	SHADER_PARAMETER_TEXTURE(Texture2D, GBufferBTexture)
	SHADER_PARAMETER_TEXTURE(Texture2D, GBufferCTexture)
	SHADER_PARAMETER_TEXTURE(Texture2D, GBufferDTexture)
	SHADER_PARAMETER_TEXTURE(Texture2D, GBufferETexture)
	SHADER_PARAMETER_TEXTURE(Texture2D, GBufferVelocityTexture)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, GBufferATextureNonMS)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, GBufferBTextureNonMS)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, GBufferCTextureNonMS)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, GBufferDTextureNonMS)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, GBufferETextureNonMS)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, GBufferVelocityTextureNonMS)
	SHADER_PARAMETER_SAMPLER(SamplerState, GBufferATextureSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, GBufferBTextureSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, GBufferCTextureSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, GBufferDTextureSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, GBufferETextureSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, GBufferVelocityTextureSampler)

	// SSAO
	SHADER_PARAMETER_TEXTURE(Texture2D, ScreenSpaceAOTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, ScreenSpaceAOTextureSampler)

	// Custom Depth / Stencil
	SHADER_PARAMETER_TEXTURE(Texture2D<float>, CustomDepthTextureNonMS)
	SHADER_PARAMETER_TEXTURE(Texture2D, CustomDepthTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, CustomDepthTextureSampler)
	SHADER_PARAMETER_SRV(Texture2D<uint2>, CustomStencilTexture)
	SHADER_PARAMETER_SRV(Texture2D<uint2>, SceneStencilTexture)

	// Misc
	SHADER_PARAMETER_TEXTURE(Texture2D, EyeAdaptation)
	SHADER_PARAMETER_TEXTURE(Texture2D, SceneColorCopyTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorCopyTextureSampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

enum class ESceneTextureSetupMode : uint32
{
	None = 0,
	SceneDepth = 1,
	GBuffers = 2,
	SSAO = 4,
	CustomDepth = 8,
	All = SceneDepth | GBuffers | SSAO | CustomDepth
};

inline ESceneTextureSetupMode operator |(ESceneTextureSetupMode lhs, ESceneTextureSetupMode rhs)  
{
	return static_cast<ESceneTextureSetupMode> (
		static_cast<uint32>(lhs) |
		static_cast<uint32>(rhs)
	);
} 

inline ESceneTextureSetupMode operator &(ESceneTextureSetupMode lhs, ESceneTextureSetupMode rhs)  
{
	return static_cast<ESceneTextureSetupMode> (
		static_cast<uint32>(lhs) &
		static_cast<uint32>(rhs)
	);
} 

extern RENDERER_API void SetupSceneTextureUniformParameters(
	FSceneRenderTargets& SceneContext,
	ERHIFeatureLevel::Type FeatureLevel,
	ESceneTextureSetupMode SetupMode,
	FSceneTexturesUniformParameters& OutParameters);

extern RENDERER_API TUniformBufferRef<FSceneTexturesUniformParameters> CreateSceneTextureUniformBuffer(
	FSceneRenderTargets& SceneContext,
	ERHIFeatureLevel::Type FeatureLevel,
	ESceneTextureSetupMode SetupMode,
	EUniformBufferUsage Usage);

template< typename TRHICmdList >
RENDERER_API TUniformBufferRef<FSceneTexturesUniformParameters> CreateSceneTextureUniformBufferSingleDraw(TRHICmdList& RHICmdList, ESceneTextureSetupMode SceneTextureSetupMode, ERHIFeatureLevel::Type FeatureLevel);

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FMobileSceneTextureUniformParameters, RENDERER_API)
	SHADER_PARAMETER_TEXTURE(Texture2D, SceneColorTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, SceneDepthTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SceneDepthTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, SceneAlphaCopyTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SceneAlphaCopyTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, CustomDepthTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, CustomDepthTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, MobileCustomStencilTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, MobileCustomStencilTextureSampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

extern void SetupMobileSceneTextureUniformParameters(
	FSceneRenderTargets& SceneContext,
	ERHIFeatureLevel::Type FeatureLevel,
	bool bSceneTexturesValid,
	FMobileSceneTextureUniformParameters& SceneTextureParameters);

template< typename TRHICmdList >
RENDERER_API TUniformBufferRef<FMobileSceneTextureUniformParameters> CreateMobileSceneTextureUniformBufferSingleDraw(TRHICmdList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel);

extern RENDERER_API void BindSceneTextureUniformBufferDependentOnShadingPath(
	const FShader::CompiledShaderInitializerType& Initializer,
	FShaderUniformBufferParameter& SceneTexturesUniformBuffer,
	FShaderUniformBufferParameter& MobileSceneTexturesUniformBuffer);

/** Encapsulates scene texture shader parameter bindings. */
class RENDERER_API FSceneTextureShaderParameters
{
public:
	/** Binds the parameters using a compiled shader's parameter map. */
	void Bind(const FShader::CompiledShaderInitializerType& Initializer)
	{
		BindSceneTextureUniformBufferDependentOnShadingPath(Initializer, SceneTexturesUniformBuffer, MobileSceneTexturesUniformBuffer);
	}

	template< typename ShaderRHIParamRef, typename TRHICmdList >
	void Set(TRHICmdList& RHICmdList, const ShaderRHIParamRef& ShaderRHI, ERHIFeatureLevel::Type FeatureLevel, ESceneTextureSetupMode SetupMode) const
	{
		if (FSceneInterface::GetShadingPath(FeatureLevel) == EShadingPath::Deferred && SceneTexturesUniformBuffer.IsBound())
		{
			TUniformBufferRef<FSceneTexturesUniformParameters> UniformBuffer = CreateSceneTextureUniformBufferSingleDraw(RHICmdList, SetupMode, FeatureLevel);
			SetUniformBufferParameter(RHICmdList, ShaderRHI, SceneTexturesUniformBuffer, UniformBuffer);
		}

		if (FSceneInterface::GetShadingPath(FeatureLevel) == EShadingPath::Mobile && MobileSceneTexturesUniformBuffer.IsBound())
		{
			TUniformBufferRef<FMobileSceneTextureUniformParameters> UniformBuffer = CreateMobileSceneTextureUniformBufferSingleDraw(RHICmdList, FeatureLevel);
			SetUniformBufferParameter(RHICmdList, ShaderRHI, MobileSceneTexturesUniformBuffer, UniformBuffer);
		}
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FSceneTextureShaderParameters& P)
	{
		Ar << P.SceneTexturesUniformBuffer;
		Ar << P.MobileSceneTexturesUniformBuffer;
		return Ar;
	}

	inline bool IsBound() const 
	{ 
		return SceneTexturesUniformBuffer.IsBound() || MobileSceneTexturesUniformBuffer.IsBound(); 
	}

	bool IsSameUniformParameter(const FShaderUniformBufferParameter& Parameter)
	{
		if (Parameter.IsBound())
		{
			if (SceneTexturesUniformBuffer.IsBound() && SceneTexturesUniformBuffer.GetBaseIndex() == Parameter.GetBaseIndex())
			{
				return true;
			}

			if (MobileSceneTexturesUniformBuffer.IsBound() && MobileSceneTexturesUniformBuffer.GetBaseIndex() == Parameter.GetBaseIndex())
			{
				return true;
			}
		}

		return false;
	}

private:
	TShaderUniformBufferParameter<FSceneTexturesUniformParameters> SceneTexturesUniformBuffer;
	TShaderUniformBufferParameter<FMobileSceneTextureUniformParameters> MobileSceneTexturesUniformBuffer;
};
