// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
BEGIN_UNIFORM_BUFFER_STRUCT(FSceneTexturesUniformParameters, RENDERER_API)
	// Scene Color / Depth
	UNIFORM_MEMBER_TEXTURE(Texture2D, SceneColorTexture)
	UNIFORM_MEMBER_SAMPLER(SamplerState, SceneColorTextureSampler)
	UNIFORM_MEMBER_TEXTURE(Texture2D, SceneDepthTexture)
	UNIFORM_MEMBER_SAMPLER(SamplerState, SceneDepthTextureSampler)
	UNIFORM_MEMBER_TEXTURE(Texture2D<float>, SceneDepthTextureNonMS)

	// GBuffer
	UNIFORM_MEMBER_TEXTURE(Texture2D, GBufferATexture)
	UNIFORM_MEMBER_TEXTURE(Texture2D, GBufferBTexture)
	UNIFORM_MEMBER_TEXTURE(Texture2D, GBufferCTexture)
	UNIFORM_MEMBER_TEXTURE(Texture2D, GBufferDTexture)
	UNIFORM_MEMBER_TEXTURE(Texture2D, GBufferETexture)
	UNIFORM_MEMBER_TEXTURE(Texture2D, GBufferVelocityTexture)
	UNIFORM_MEMBER_TEXTURE(Texture2D<float4>, GBufferATextureNonMS)
	UNIFORM_MEMBER_TEXTURE(Texture2D<float4>, GBufferBTextureNonMS)
	UNIFORM_MEMBER_TEXTURE(Texture2D<float4>, GBufferCTextureNonMS)
	UNIFORM_MEMBER_TEXTURE(Texture2D<float4>, GBufferDTextureNonMS)
	UNIFORM_MEMBER_TEXTURE(Texture2D<float4>, GBufferETextureNonMS)
	UNIFORM_MEMBER_TEXTURE(Texture2D<float4>, GBufferVelocityTextureNonMS)
	UNIFORM_MEMBER_SAMPLER(SamplerState, GBufferATextureSampler)
	UNIFORM_MEMBER_SAMPLER(SamplerState, GBufferBTextureSampler)
	UNIFORM_MEMBER_SAMPLER(SamplerState, GBufferCTextureSampler)
	UNIFORM_MEMBER_SAMPLER(SamplerState, GBufferDTextureSampler)
	UNIFORM_MEMBER_SAMPLER(SamplerState, GBufferETextureSampler)
	UNIFORM_MEMBER_SAMPLER(SamplerState, GBufferVelocityTextureSampler)

	// SSAO
	UNIFORM_MEMBER_TEXTURE(Texture2D, ScreenSpaceAOTexture)
	UNIFORM_MEMBER_SAMPLER(SamplerState, ScreenSpaceAOTextureSampler)

	// Custom Depth / Stencil
	UNIFORM_MEMBER_TEXTURE(Texture2D<float>, CustomDepthTextureNonMS)
	UNIFORM_MEMBER_TEXTURE(Texture2D, CustomDepthTexture)
	UNIFORM_MEMBER_SAMPLER(SamplerState, CustomDepthTextureSampler)
	UNIFORM_MEMBER_SRV(Texture2D<uint2>, CustomStencilTexture)
	UNIFORM_MEMBER_SRV(Texture2D<uint2>, SceneStencilTexture)

	// Misc
	UNIFORM_MEMBER_TEXTURE(Texture2D, EyeAdaptation)
	UNIFORM_MEMBER_TEXTURE(Texture2D, SceneColorCopyTexture)
	UNIFORM_MEMBER_SAMPLER(SamplerState, SceneColorCopyTextureSampler)
END_UNIFORM_BUFFER_STRUCT(FSceneTexturesUniformParameters)

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

template< typename TRHICmdList >
RENDERER_API TUniformBufferRef<FSceneTexturesUniformParameters> CreateSceneTextureUniformBufferSingleDraw(TRHICmdList& RHICmdList, ESceneTextureSetupMode SceneTextureSetupMode, ERHIFeatureLevel::Type FeatureLevel);

BEGIN_UNIFORM_BUFFER_STRUCT(FMobileSceneTextureUniformParameters, RENDERER_API)
	UNIFORM_MEMBER_TEXTURE(Texture2D, SceneColorTexture)
	UNIFORM_MEMBER_SAMPLER(SamplerState, SceneColorTextureSampler)
	UNIFORM_MEMBER_TEXTURE(Texture2D, SceneDepthTexture)
	UNIFORM_MEMBER_SAMPLER(SamplerState, SceneDepthTextureSampler)
	UNIFORM_MEMBER_TEXTURE(Texture2D, SceneAlphaCopyTexture)
	UNIFORM_MEMBER_SAMPLER(SamplerState, SceneAlphaCopyTextureSampler)
	UNIFORM_MEMBER_TEXTURE(Texture2D, CustomDepthTexture)
	UNIFORM_MEMBER_SAMPLER(SamplerState, CustomDepthTextureSampler)
	UNIFORM_MEMBER_TEXTURE(Texture2D, MobileCustomStencilTexture)
	UNIFORM_MEMBER_SAMPLER(SamplerState, MobileCustomStencilTextureSampler)
END_UNIFORM_BUFFER_STRUCT(FMobileSceneTextureUniformParameters)

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
