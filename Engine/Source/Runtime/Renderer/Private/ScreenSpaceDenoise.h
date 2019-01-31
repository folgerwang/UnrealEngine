// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraph.h"


class FViewInfo;
class FLightSceneInfo;
class FSceneViewFamilyBlackboard;


/** Interface for denoiser to have all hook in the renderer. */
class RENDERER_API IScreenSpaceDenoiser
{
public:
	/** What the shadow ray tracing needs to output */
	enum class EShadowRayTracingOutputs
	{
		ClosestOccluder,
		PenumbraAndClosestOccluder
	};

	/** What the denoiser would like to have as an input. */
	struct FShadowRayTracingConfig
	{
		// The output the shadow denoiser needs.
		EShadowRayTracingOutputs Requirements;
	};

	/** All the inputs of the shadow denoiser. */
	BEGIN_SHADER_PARAMETER_STRUCT(FShadowPenumbraInputs, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Penumbra)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ClosestOccluder)
	END_SHADER_PARAMETER_STRUCT()

	/** All the outputs of the shadow denoiser may generate. */
	BEGIN_SHADER_PARAMETER_STRUCT(FShadowPenumbraOutputs, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DiffusePenumbra)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SpecularPenumbra)
	END_SHADER_PARAMETER_STRUCT()

	/** The configuration of the reflection ray tracing. */
	struct FReflectionsRayTracingConfig
	{
		// Resolution fraction the ray tracing is being traced at.
		float ResolutionFraction;
	};
	
	/** The configuration of the reflection ray tracing. */
	struct FAmbientOcclusionRayTracingConfig
	{
		// Resolution fraction the ray tracing is being traced at.
		float ResolutionFraction = 1.0f;

		// Number of rays per pixels.
		float RayCountPerPixel = 1.0f;
	};


	virtual ~IScreenSpaceDenoiser() {};

	/** Debug name of the denoiser for draw event. */
	virtual const TCHAR* GetDebugName() const = 0;

	/** Returns the ray tracing configuration that should be done for denoiser. */
	virtual FShadowRayTracingConfig GetShadowRayTracingConfig(
		const FViewInfo& View,
		const FLightSceneInfo& LightSceneInfo) const = 0;

	/** Entry point to denoise a shadow. */
	virtual FShadowPenumbraOutputs DenoiseShadowPenumbra(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		const FLightSceneInfo& LightSceneInfo,
		const FSceneViewFamilyBlackboard& SceneBlackboard,
		const FShadowPenumbraInputs& ShadowInputs) const = 0;
	
	/** All the inputs of the reflection denoiser. */
	BEGIN_SHADER_PARAMETER_STRUCT(FReflectionsInputs, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Color)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RayHitDistance)
	END_SHADER_PARAMETER_STRUCT()

	/** All the outputs of the reflection denoiser may generate. */
	BEGIN_SHADER_PARAMETER_STRUCT(FReflectionsOutputs, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Color)
	END_SHADER_PARAMETER_STRUCT()
		
	/** Entry point to denoise reflections. */
	virtual FReflectionsOutputs DenoiseReflections(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		const FSceneViewFamilyBlackboard& SceneBlackboard,
		const FReflectionsInputs& ReflectionInputs,
		const FReflectionsRayTracingConfig RayTracingConfig) const = 0;
	
	/** All the inputs of the AO denoisers. */
	BEGIN_SHADER_PARAMETER_STRUCT(FAmbientOcclusionInputs, )
		// G16R16F texture:
		//  R: mask as [0; 1].
		//  B: closest hit distance of the ray.
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MaskAndRayHitDistance)
	END_SHADER_PARAMETER_STRUCT()

	/** All the outputs of the AO denoiser may generate. */
	BEGIN_SHADER_PARAMETER_STRUCT(FAmbientOcclusionOutputs, )
		// Ambient occlusion mask stored in the red channel as [0; 1].
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AmbientOcclusionMask)
	END_SHADER_PARAMETER_STRUCT()
		
	/** Entry point to denoise reflections. */
	virtual FAmbientOcclusionOutputs DenoiseAmbientOcclusion(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		const FSceneViewFamilyBlackboard& SceneBlackboard,
		const FAmbientOcclusionInputs& ReflectionInputs,
		const FAmbientOcclusionRayTracingConfig RayTracingConfig) const = 0;

	/** Returns the interface of the default denoiser of the renderer. */
	static const IScreenSpaceDenoiser* GetDefaultDenoiser();
}; // class FScreenSpaceDenoisingInterface


// The interface for the renderer to denoise what it needs, Plugins can come and point this to custom interface.
extern RENDERER_API const IScreenSpaceDenoiser* GScreenSpaceDenoiser;
