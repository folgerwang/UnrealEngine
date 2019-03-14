// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraph.h"


class FViewInfo;
struct FPreviousViewInfo;
class FLightSceneInfo;
class FSceneViewFamilyBlackboard;


/** Interface for denoiser to have all hook in the renderer. */
class RENDERER_API IScreenSpaceDenoiser
{
public:
	/** Maximum number a denoiser might be able to denoise at the same time. */
	static const int32 kMaxBatchSize = 4;


	/** What the shadow ray tracing needs to output */
	enum class EShadowRequirements
	{
		// Denoiser is unable to denoise that configuration.
		Bailout,

		// Denoiser only need ray hit distance for 1spp.
		// FShadowPenumbraInputs::Penumbra: non generated
		// FShadowPenumbraInputs::ClosestOccluder:
		//   -2: invalid sample,
		//   -1: miss
		//   >0: hit distance of occluding geometry
		ClosestOccluder,

		// Denoiser only need ray hit distance and the diffuse mask of the penumbra.
		// FShadowPenumbraInputs::Penumbra: average diffuse penumbra mask in [0; 1]
		// FShadowPenumbraInputs::ClosestOccluder:
		//   -1: invalid sample
		//   >0: average hit distance of occluding geometry
		PenumbraAndAvgOccluder,

		PenumbraAndClosestOccluder,
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
	struct FShadowRayTracingConfig
	{
		// Number of rays per pixels.
		int32 RayCountPerPixel = 1;
	};

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
	virtual EShadowRequirements GetShadowRequirements(
		const FViewInfo& View,
		const FLightSceneInfo& LightSceneInfo,
		const FShadowRayTracingConfig& RayTracingConfig) const = 0;

	/** Structure that contains all the parameters the denoiser needs to denoise one shadow. */
	struct FShadowParameters
	{
		const FLightSceneInfo* LightSceneInfo = nullptr;
		FShadowRayTracingConfig RayTracingConfig;
		FShadowPenumbraInputs InputTextures;
	};

	/** Entry point to denoise the diffuse mask of a shadow. */
	// TODO: correct specular.
	virtual void DenoiseShadows(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneViewFamilyBlackboard& SceneBlackboard,
		const TStaticArray<FShadowParameters, IScreenSpaceDenoiser::kMaxBatchSize>& InputParameters,
		const int32 InputParameterCount,
		TStaticArray<FShadowPenumbraOutputs, IScreenSpaceDenoiser::kMaxBatchSize>& Outputs) const = 0;
	
	/** All the inputs of the reflection denoiser. */
	BEGIN_SHADER_PARAMETER_STRUCT(FReflectionsInputs, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Color)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RayHitDistance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RayImaginaryDepth)
	END_SHADER_PARAMETER_STRUCT()

	/** All the outputs of the reflection denoiser may generate. */
	BEGIN_SHADER_PARAMETER_STRUCT(FReflectionsOutputs, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Color)
	END_SHADER_PARAMETER_STRUCT()
		
	/** Entry point to denoise reflections. */
	virtual FReflectionsOutputs DenoiseReflections(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneViewFamilyBlackboard& SceneBlackboard,
		const FReflectionsInputs& ReflectionInputs,
		const FReflectionsRayTracingConfig RayTracingConfig) const = 0;
	
	/** All the inputs of the AO denoisers. */
	BEGIN_SHADER_PARAMETER_STRUCT(FAmbientOcclusionInputs, )
		// TODO: Merge this back to MaskAndRayHitDistance into RG texture for performance improvement of denoiser's reconstruction pass. May also support RayDistanceOnly for 1spp AO ray tracing.
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Mask)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RayHitDistance)
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
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneViewFamilyBlackboard& SceneBlackboard,
		const FAmbientOcclusionInputs& ReflectionInputs,
		const FAmbientOcclusionRayTracingConfig RayTracingConfig) const = 0;

	/** All the inputs of the GI denoisers. */
	BEGIN_SHADER_PARAMETER_STRUCT(FGlobalIlluminationInputs, )
		// Irradiance in RGB, AO mask in alpha.
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Color)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RayHitDistance)
	END_SHADER_PARAMETER_STRUCT()

	/** All the outputs of the GI denoiser may generate. */
	BEGIN_SHADER_PARAMETER_STRUCT(FGlobalIlluminationOutputs, )
		// Irradiance in RGB, AO mask in alpha.
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Color)
	END_SHADER_PARAMETER_STRUCT()

	/** Entry point to denoise reflections. */
	virtual FGlobalIlluminationOutputs DenoiseGlobalIllumination(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneViewFamilyBlackboard& SceneBlackboard,
		const FGlobalIlluminationInputs& Inputs,
		const FAmbientOcclusionRayTracingConfig Config) const = 0;

	/** Entry point to denoise SkyLight. */
	virtual FGlobalIlluminationOutputs DenoiseSkyLight(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneViewFamilyBlackboard& SceneBlackboard,
		const FGlobalIlluminationInputs& Inputs,
		const FAmbientOcclusionRayTracingConfig Config) const = 0;

	/** Returns the interface of the default denoiser of the renderer. */
	static const IScreenSpaceDenoiser* GetDefaultDenoiser();
}; // class FScreenSpaceDenoisingInterface


// The interface for the renderer to denoise what it needs, Plugins can come and point this to custom interface.
extern RENDERER_API const IScreenSpaceDenoiser* GScreenSpaceDenoiser;
