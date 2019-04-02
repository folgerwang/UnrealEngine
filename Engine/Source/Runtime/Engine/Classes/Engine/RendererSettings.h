// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Engine/Scene.h"
#include "Engine/DeveloperSettings.h"
#include "PixelFormat.h"

#include "RendererSettings.generated.h"

struct FPropertyChangedEvent;

/**
 * Enumerates ways to clear a scene.
 */
UENUM()
namespace EClearSceneOptions
{
	enum Type
	{
		NoClear = 0 UMETA(DisplayName="Do not clear",ToolTip="This option is fastest but can cause artifacts unless you render to every pixel. Make sure to use a skybox with this option!"),
		HardwareClear = 1 UMETA(DisplayName="Hardware clear",ToolTip="Perform a full hardware clear before rendering. Most projects should use this option."),
		QuadAtMaxZ = 2 UMETA(DisplayName="Clear at far plane",ToolTip="Draws a quad to perform the clear at the far plane, this is faster than a hardware clear on some GPUs."),
	};
}


/**
 * Enumerates available compositing sample counts.
 */
UENUM()
namespace ECompositingSampleCount
{
	enum Type
	{
		One = 1 UMETA(DisplayName="No MSAA"),
		Two = 2 UMETA(DisplayName="2x MSAA"),
		Four = 4 UMETA(DisplayName="4x MSAA"),
		Eight = 8 UMETA(DisplayName="8x MSAA"),
	};
}


/**
* Enumerates available mobile MSAA sample counts.
*/
UENUM()
namespace EMobileMSAASampleCount
{
	enum Type
	{
		One = 1 UMETA(DisplayName = "No MSAA"),
		Two = 2 UMETA(DisplayName = "2x MSAA"),
		Four = 4 UMETA(DisplayName = "4x MSAA"),
		Eight = 8 UMETA(DisplayName = "8x MSAA"),
	};
}


/**
 * Enumerates available options for custom depth.
 */
UENUM()
namespace ECustomDepthStencil
{
	enum Type
	{
		Disabled = 0,
		Enabled = 1 UMETA(ToolTip="Depth buffer created immediately. Stencil disabled."),
		EnabledOnDemand = 2 UMETA(ToolTip="Depth buffer created on first use, can save memory but cause stalls. Stencil disabled."),
		EnabledWithStencil = 3 UMETA(ToolTip="Depth buffer created immediately. Stencil available for read/write."),
	};
}


/**
 * Enumerates available options for early Z-passes.
 */
UENUM()
namespace EEarlyZPass
{
	enum Type
	{
		None = 0 UMETA(DisplayName="None"),
		OpaqueOnly = 1 UMETA(DisplayName="Opaque meshes only"),
		OpaqueAndMasked = 2 UMETA(DisplayName="Opaque and masked meshes"),
		Auto = 3 UMETA(DisplayName="Decide automatically",ToolTip="Let the engine decide what to render in the early Z pass based on the features being used."),
	};
}

/**
 * Enumerates available options for alpha channel through post processing. The renderer will always generate premultiplied RGBA
 * with alpha as translucency (0 = fully opaque; 1 = fully translucent).
 */
UENUM()
namespace EAlphaChannelMode
{
	enum Type
	{
		/** Disabled, reducing GPU cost to the minimum. (default). */
		Disabled = 0 UMETA(DisplayName = "Disabled"),

		/** Maintain alpha channel only within linear color space. Tonemapper won't output alpha channel. */
		LinearColorSpaceOnly = 1 UMETA(DisplayName="Linear color space only"),

		/** Maintain alpha channel within linear color space, but also pass it through the tonemapper.
		 *
		 * CAUTION: Passing the alpha channel through the tonemapper can unevitably lead to pretty poor compositing quality as
		 * opposed to linear color space compositing, especially on purely additive pixels bloom can generate. This settings is
		 * exclusively targeting broadcast industry in case of hardware unable to do linear color space compositing and
		 * tonemapping.
		 */
		AllowThroughTonemapper = 2 UMETA(DisplayName="Allow through tonemapper"),
	};
}

namespace EAlphaChannelMode
{
	ENGINE_API EAlphaChannelMode::Type FromInt(int32 InAlphaChannelMode);
}

/** used by FPostProcessSettings AutoExposure*/
UENUM()
namespace EAutoExposureMethodUI
{
	enum Type
	{
		/** Not supported on mobile, requires compute shader to construct 64 bin histogram */
		AEM_Histogram  UMETA(DisplayName = "Auto Exposure Histogram"),
		/** Not supported on mobile, faster method that computes single value by downsampling */
		AEM_Basic      UMETA(DisplayName = "Auto Exposure Basic"),
		/** Uses camera settings. */
		AEM_Manual   UMETA(DisplayName = "Manual"),
		AEM_MAX,
	};
}

/** used by GetDefaultBackBufferPixelFormat*/
UENUM()
namespace EDefaultBackBufferPixelFormat
{
	enum Type
	{
		DBBPF_B8G8R8A8 = 0				UMETA(DisplayName = "8bit RGBA"),
		DBBPF_A16B16G16R16_DEPRECATED	UMETA(DisplayName = "DEPRECATED - 16bit RGBA", Hidden),
		DBBPF_FloatRGB_DEPRECATED		UMETA(DisplayName = "DEPRECATED - Float RGB", Hidden),
		DBBPF_FloatRGBA					UMETA(DisplayName = "Float RGBA"),
		DBBPF_A2B10G10R10				UMETA(DisplayName = "10bit RGB, 2bit Alpha"),
		DBBPF_MAX,
	};
}

namespace EDefaultBackBufferPixelFormat
{
	ENGINE_API EPixelFormat Convert2PixelFormat(EDefaultBackBufferPixelFormat::Type InDefaultBackBufferPixelFormat);
	ENGINE_API int32 NumberOfBitForAlpha(EDefaultBackBufferPixelFormat::Type InDefaultBackBufferPixelFormat);
	ENGINE_API EDefaultBackBufferPixelFormat::Type FromInt(int32 InDefaultBackBufferPixelFormat);
}

/**
 * Rendering settings.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Rendering"))
class ENGINE_API URendererSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(config, EditAnywhere, Category=Mobile, meta=(
		ConsoleVariable="r.MobileHDR",DisplayName="Mobile HDR",
		ToolTip="If true, mobile renders in full HDR. Disable this setting for games that do not require lighting features for better performance on slow devices. Changing this setting requires restarting the editor.",
		ConfigRestartRequired = true))
	uint32 bMobileHDR:1;

	UPROPERTY(config, EditAnywhere, Category = Mobile, meta = (
		ConsoleVariable = "r.Mobile.DisableVertexFog", DisplayName = "Disable vertex fogging in mobile shaders",
		ToolTip = "If true, vertex fog will be omitted from all mobile shaders. If your game does not use fog, you should choose this setting to increase shading performance.",
		ConfigRestartRequired = true))
		uint32 bMobileDisableVertexFog : 1;

	UPROPERTY(config, EditAnywhere, Category = Mobile, meta = (
		ConsoleVariable = "r.Shadow.CSM.MaxMobileCascades", DisplayName = "Maximum number of CSM cascades to render", ClampMin = 1, ClampMax = 4,
		ToolTip = "The maximum number of cascades with which to render dynamic directional light shadows when using the mobile renderer.",
		ConfigRestartRequired = true))
		int32 MaxMobileCascades;

	UPROPERTY(config, EditAnywhere, Category = Mobile, meta = (
		ConsoleVariable = "r.MobileMSAA", DisplayName = "Mobile MSAA",
		ToolTip = "Multi-sample anti-aliasing setting to use on mobile. MSAA is currently supported using Metal on iOS, and on Android devices with the required support using ES 2 or ES 3.1.\nIf MSAA is not available, the current default AA method will be used."))
	TEnumAsByte<EMobileMSAASampleCount::Type> MobileMSAASampleCount;

	UPROPERTY(config, EditAnywhere, Category = Mobile, meta = (
		ConsoleVariable = "r.Mobile.UseLegacyShadingModel", DisplayName = "Use legacy shading model",
		ToolTip = "If true then mobile shaders will use the cheaper but lower quality specular calculation found in versions prior to 4.20.",
		ConfigRestartRequired = true))
		uint32 bMobileUseLegacyShadingModel : 1;

	UPROPERTY(config, meta = (
		ConsoleVariable = "r.Mobile.UseHWsRGBEncoding", DisplayName = "Single-pass linear rendering",
		ToolTip = "If true then mobile single-pass (non mobile HDR) rendering will use HW accelerated sRGB encoding/decoding. Available only on Oculus for now."))
		uint32 bMobileUseHWsRGBEncoding : 1;

	UPROPERTY(config, EditAnywhere, Category = Mobile, meta=(
		ConsoleVariable="r.Mobile.AllowDitheredLODTransition", DisplayName="Allow Dithered LOD Transition",
		ToolTip="Whether to support 'Dithered LOD Transition' material option on mobile platforms. Enabling this may degrade performance as rendering will not benefit from Early-Z optimization.",
		ConfigRestartRequired=true))
	uint32 bMobileAllowDitheredLODTransition:1;

	UPROPERTY(config, EditAnywhere, Category = Mobile, meta=(
		ConsoleVariable="r.Mobile.AllowSoftwareOcclusion", DisplayName="Support Software Occlusion Culling",
		ToolTip="Whether to support 'Software Occlusion Culling' on mobile platforms. This will package occluder information and enable Software Occlusion Culling.",
		ConfigRestartRequired=false))
	uint32 bMobileAllowSoftwareOcclusionCulling:1;
	
	UPROPERTY(config, EditAnywhere, Category = Materials, meta = (
		ConsoleVariable = "r.DiscardUnusedQuality", DisplayName = "Game Discards Unused Material Quality Levels",
		ToolTip = "When running in game mode, whether to keep shaders for all quality levels in memory or only those needed for the current quality level.\nUnchecked: Keep all quality levels in memory allowing a runtime quality level change. (default)\nChecked: Discard unused quality levels when loading content for the game, saving some memory."))
	uint32 bDiscardUnusedQualityLevels : 1;

	UPROPERTY(config, EditAnywhere, Category=Culling, meta=(
		ConsoleVariable="r.AllowOcclusionQueries",DisplayName="Occlusion Culling",
		ToolTip="Allows occluded meshes to be culled and no rendered."))
	uint32 bOcclusionCulling:1;

	UPROPERTY(config, EditAnywhere, Category=Culling, meta=(
		ConsoleVariable="r.MinScreenRadiusForLights",DisplayName="Min Screen Radius for Lights",
		ToolTip="Screen radius at which lights are culled. Larger values can improve performance but causes lights to pop off when they affect a small area of the screen."))
	float MinScreenRadiusForLights;

	UPROPERTY(config, EditAnywhere, Category=Culling, meta=(
		ConsoleVariable="r.MinScreenRadiusForDepthPrepass",DisplayName="Min Screen Radius for Early Z Pass",
		ToolTip="Screen radius at which objects are culled for the early Z pass. Larger values can improve performance but very large values can degrade performance if large occluders are not rendered."))
	float MinScreenRadiusForEarlyZPass;

	UPROPERTY(config, EditAnywhere, Category=Culling, meta=(
		ConsoleVariable="r.MinScreenRadiusForCSMDepth",DisplayName="Min Screen Radius for Cascaded Shadow Maps",
		ToolTip="Screen radius at which objects are culled for cascaded shadow map depth passes. Larger values can improve performance but can cause artifacts as objects stop casting shadows."))
	float MinScreenRadiusForCSMdepth;
	
	UPROPERTY(config, EditAnywhere, Category=Culling, meta=(
		ConsoleVariable="r.PrecomputedVisibilityWarning",DisplayName="Warn about no precomputed visibility",
		ToolTip="Displays a warning when no precomputed visibility data is available for the current camera location. This can be helpful if you are making a game that relies on precomputed visibility, e.g. a first person mobile game."))
	uint32 bPrecomputedVisibilityWarning:1;

	UPROPERTY(config, EditAnywhere, Category=Textures, meta=(
		ConsoleVariable="r.TextureStreaming",DisplayName="Texture Streaming",
		ToolTip="When enabled textures will stream in based on what is visible on screen."))
	uint32 bTextureStreaming:1;

	UPROPERTY(config, EditAnywhere, Category=Textures, meta=(
		ConsoleVariable="Compat.UseDXT5NormalMaps",DisplayName="Use DXT5 Normal Maps",
		ToolTip="Whether to use DXT5 for normal maps, otherwise BC5 will be used, which is not supported on all hardware. Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bUseDXT5NormalMaps:1;

	UPROPERTY(config, EditAnywhere, Category = Materials, meta =(
		ConfigRestartRequired = true,
		ConsoleVariable = "r.ClearCoatNormal",
		ToolTip = "Use a separate normal map for the bottom layer of a clear coat material. This is a higher quality feature that is expensive."))
		uint32 bClearCoatEnableSecondNormal : 1;

	UPROPERTY(config, EditAnywhere, Category = Reflections, meta = (
		ConsoleVariable = "r.ReflectionCaptureResolution", DisplayName = "Reflection Capture Resolution",
		ToolTip = "The cubemap resolution for all reflection capture probes. Must be power of 2. Note that for very high values the memory and performance impact may be severe."))
	int32 ReflectionCaptureResolution;

	UPROPERTY(config, EditAnywhere, Category = Reflections, meta = (
		ConsoleVariable = "r.ReflectionEnvironmentLightmapMixBasedOnRoughness", DisplayName = "Reduce lightmap mixing on smooth surfaces",
		ToolTip = "Whether to reduce lightmap mixing with reflection captures for very smooth surfaces.  This is useful to make sure reflection captures match SSR / planar reflections in brightness."))
	uint32 ReflectionEnvironmentLightmapMixBasedOnRoughness : 1;

	UPROPERTY(config, EditAnywhere, Category=ForwardRenderer, meta=(
		ConsoleVariable="r.ForwardShading",
		DisplayName = "Forward Shading",
		ToolTip="Whether to use forward shading on desktop platforms, requires Shader Model 5 hardware.  Forward shading supports MSAA and has lower default cost, but fewer features supported overall.  Materials have to opt-in to more expensive features like high quality reflections.  Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bForwardShading:1;

	UPROPERTY(config, EditAnywhere, Category=ForwardRenderer, meta=(
		ConsoleVariable="r.VertexFoggingForOpaque",
		ToolTip="Causes opaque materials to use per-vertex fogging, which costs slightly less.  Only supported with forward shading. Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bVertexFoggingForOpaque:1;

	UPROPERTY(config, EditAnywhere, Category=Lighting, meta=(
		ConsoleVariable="r.AllowStaticLighting",
		ToolTip="Whether to allow any static lighting to be generated and used, like lightmaps and shadowmaps. Games that only use dynamic lighting should set this to 0 to save some static lighting overhead. Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bAllowStaticLighting:1;

	UPROPERTY(config, EditAnywhere, Category=Lighting, meta=(
		ConsoleVariable="r.NormalMapsForStaticLighting",
		ToolTip="Whether to allow any static lighting to use normal maps for lighting computations."))
	uint32 bUseNormalMapsForStaticLighting:1;

	UPROPERTY(config, EditAnywhere, Category=Lighting, meta=(
		ConsoleVariable="r.GenerateMeshDistanceFields",
		ToolTip="Whether to build distance fields of static meshes, needed for distance field AO, which is used to implement Movable SkyLight shadows, and ray traced distance field shadows on directional lights.  Enabling will increase mesh build times and memory usage.  Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bGenerateMeshDistanceFields:1;

	UPROPERTY(config, EditAnywhere, Category=Lighting, meta=(
		EditCondition = "bGenerateMeshDistanceFields",
		ConsoleVariable="r.DistanceFieldBuild.EightBit",
		ToolTip="Whether to store mesh distance fields in an 8 bit fixed point format instead of 16 bit floating point.  8 bit uses half the memory, but introduces artifacts for large meshes or thin meshes.  Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bEightBitMeshDistanceFields:1;

	UPROPERTY(config, EditAnywhere, Category = Lighting, meta = (
		EditCondition = "bGenerateMeshDistanceFields",
		ConsoleVariable = "r.GenerateLandscapeGIData", DisplayName = "Generate Landscape Real-time GI Data",
		ToolTip = "Whether to generate a low-resolution base color texture for landscapes for rendering real-time global illumination.  This feature requires GenerateMeshDistanceFields is also enabled, and will increase mesh build times and memory usage."))
	uint32 bGenerateLandscapeGIData : 1;

	UPROPERTY(config, EditAnywhere, Category=Lighting, meta=(
		EditCondition = "bGenerateMeshDistanceFields",
		ConsoleVariable="r.DistanceFieldBuild.Compress",
		ToolTip="Whether to store mesh distance fields compressed in memory, which reduces how much memory they take, but also causes serious hitches when making new levels visible.  Only enable if your project does not stream levels in-game.  Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bCompressMeshDistanceFields:1;

	UPROPERTY(config, EditAnywhere, Category=Tessellation, meta=(
		ConsoleVariable="r.TessellationAdaptivePixelsPerTriangle",DisplayName="Adaptive pixels per triangle",
		ToolTip="When adaptive tessellation is enabled it will try to tessellate a mesh so that each triangle contains the specified number of pixels. The tessellation multiplier specified in the material can increase or decrease the amount of tessellation."))
	float TessellationAdaptivePixelsPerTriangle;

	UPROPERTY(config, EditAnywhere, Category=Translucency, meta=(
		ConsoleVariable="r.SeparateTranslucency",
		ToolTip="Allow translucency to be rendered to a separate render targeted and composited after depth of field. Prevents translucency from appearing out of focus."))
	uint32 bSeparateTranslucency:1;

	UPROPERTY(config, EditAnywhere, Category=Translucency, meta=(
		ConsoleVariable="r.TranslucentSortPolicy",DisplayName="Translucent Sort Policy",
		ToolTip="The sort mode for translucent primitives, affecting how they are ordered and how they change order as the camera moves. Requires that Separate Translucency (under Postprocessing) is true."))
	TEnumAsByte<ETranslucentSortPolicy::Type> TranslucentSortPolicy;

	UPROPERTY(config, EditAnywhere, Category=Translucency, meta=(
		DisplayName="Translucent Sort Axis",
		ToolTip="The axis that sorting will occur along when Translucent Sort Policy is set to SortAlongAxis."))
	FVector TranslucentSortAxis;

	UPROPERTY(config, EditAnywhere, Category=Postprocessing, meta=(
		ConsoleVariable="r.CustomDepth",DisplayName="Custom Depth-Stencil Pass",
		ToolTip="Whether the custom depth pass for tagging primitives for postprocessing passes is enabled. Enabling it on demand can save memory but may cause a hitch the first time the feature is used."))
	TEnumAsByte<ECustomDepthStencil::Type> CustomDepthStencil;

	UPROPERTY(config, EditAnywhere, Category = Postprocessing, meta = (
		ConsoleVariable = "r.CustomDepthTemporalAAJitter", DisplayName = "Custom Depth with TemporalAA Jitter",
		ToolTip = "Whether the custom depth pass has the TemporalAA jitter enabled. Disabling this can be useful when the result of the CustomDepth Pass is used after TAA (e.g. after Tonemapping)"))
	uint32 bCustomDepthTaaJitter : 1;

	UPROPERTY(config, EditAnywhere, Category = Postprocessing, meta = (
		ConsoleVariable = "r.PostProcessing.PropagateAlpha", DisplayName = "Enable alpha channel support in post processing (experimental).",
		ToolTip = "Configures alpha channel support in renderer's post processing chain. Still experimental: works only with Temporal AA, Motion Blur, Circle Depth Of Field. This option also force disable the separate translucency.",
		ConfigRestartRequired = true))
	TEnumAsByte<EAlphaChannelMode::Type> bEnableAlphaChannelInPostProcessing;

	UPROPERTY(config, EditAnywhere, Category = Postprocessing, meta = (
		ConsoleVariable = "r.DOF.Algorithm", DisplayName = "Use new DOF algorithm",
		ToolTip = "Whether to use the new DOF implementation for Circle DOF method."))
	uint32 bUseNewAlgorithm : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.Bloom", DisplayName = "Bloom",
		ToolTip = "Whether the default for Bloom is enabled or not (postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	uint32 bDefaultFeatureBloom : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.AmbientOcclusion", DisplayName = "Ambient Occlusion",
		ToolTip = "Whether the default for AmbientOcclusion is enabled or not (postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	uint32 bDefaultFeatureAmbientOcclusion : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.AmbientOcclusionStaticFraction", DisplayName = "Ambient Occlusion Static Fraction (AO for baked lighting)",
		ToolTip = "Whether the default for AmbientOcclusionStaticFraction is enabled or not (only useful for baked lighting and if AO is on, allows to have SSAO affect baked lighting as well, costs performance, postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	uint32 bDefaultFeatureAmbientOcclusionStaticFraction : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.AutoExposure", DisplayName = "Auto Exposure",
		ToolTip = "Whether the default for AutoExposure is enabled or not (postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	uint32 bDefaultFeatureAutoExposure : 1;
	
	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.AutoExposure.Method", DisplayName = "Auto Exposure",
		ToolTip = "The default method for AutoExposure(postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	TEnumAsByte<EAutoExposureMethodUI::Type> DefaultFeatureAutoExposure; 

	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange", DisplayName = "Extend default luminance range in Auto Exposure settings",
		ToolTip = "Whether the default values for AutoExposure should support an extended range of scene luminance. Also changes the exposure settings to be expressed in EV100.",
		ConfigRestartRequired=true))
	uint32 bExtendDefaultLuminanceRangeInAutoExposureSettings: 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.UsePreExposure", DisplayName = "Apply Pre-exposure before writing to the scene color",
		ToolTip = "Whether to use pre-exposure to remap the range of the scene color around the camera exposure. This limits the render target range required to support HDR lighting value.",
		ConfigRestartRequired=true))
	uint32 bUsePreExposure : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.MotionBlur", DisplayName = "Motion Blur",
		ToolTip = "Whether the default for MotionBlur is enabled or not (postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	uint32 bDefaultFeatureMotionBlur : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.LensFlare", DisplayName = "Lens Flares (Image based)",
		ToolTip = "Whether the default for LensFlare is enabled or not (postprocess volume/camera/game setting can still override and enable or disable it independently)"))
	uint32 bDefaultFeatureLensFlare : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		EditCondition = "DefaultFeatureAntiAliasing == AAM_TemporalAA",
		ConsoleVariable = "r.TemporalAA.Upsampling", DisplayName = "Temporal Upsampling",
		ToolTip = "Whether to do primary screen percentage with temporal AA or not."))
	uint32 bTemporalUpsampling : 1;

	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.AntiAliasing", DisplayName = "Anti-Aliasing Method",
		ToolTip = "Which anti-aliasing mode is used by default"))
	TEnumAsByte<EAntiAliasingMethod> DefaultFeatureAntiAliasing;

	UPROPERTY(config, EditAnywhere, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultFeature.LightUnits", DisplayName = "Light Units",
		ToolTip = "Which units to use for newly placed point, spot and rect lights"))
	ELightUnits DefaultLightUnits;

	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = DefaultSettings, meta = (
		ConsoleVariable = "r.DefaultBackBufferPixelFormat",DisplayName = "Frame Buffer Pixel Format",
		ToolTip = "Pixel format used for back buffer, when not specified",
		ConfigRestartRequired=true))
	TEnumAsByte<EDefaultBackBufferPixelFormat::Type> DefaultBackBufferPixelFormat;

	UPROPERTY(config, EditAnywhere, Category=Optimizations, meta=(
		ConsoleVariable="r.Shadow.UnbuiltPreviewInGame",DisplayName="Render Unbuilt Preview Shadows in game",
		ToolTip="Whether to render unbuilt preview shadows in game.  When enabled and lighting is not built, expensive preview shadows will be rendered in game.  When disabled, lighting in game and editor won't match which can appear to be a bug."))
	uint32 bRenderUnbuiltPreviewShadowsInGame:1;

	UPROPERTY(config, EditAnywhere, Category=Optimizations, meta=(
		ConsoleVariable="r.StencilForLODDither",DisplayName="Use Stencil for LOD Dither Fading",
		ToolTip="Whether to use stencil for LOD dither fading.  This saves GPU time in the base pass for materials with dither fading enabled, but forces a full prepass. Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bStencilForLODDither:1;

	UPROPERTY(config, EditAnywhere, Category = Optimizations, meta = (
		ConsoleVariable="r.EarlyZPass",DisplayName="Early Z-pass",
		ToolTip="Whether to use a depth only pass to initialize Z culling for the base pass."))
	TEnumAsByte<EEarlyZPass::Type> EarlyZPass;

	UPROPERTY(config, EditAnywhere, Category = Optimizations, meta = (
		EditCondition = "EarlyZPass == OpaqueAndMasked && bEarlyZPassMovable",
		ConsoleVariable = "r.EarlyZPassOnlyMaterialMasking", DisplayName = "Mask material only in early Z-pass",
		ToolTip = "Whether to compute materials' mask opacity only in early Z pass. Changing this setting requires restarting the editor.",
		ConfigRestartRequired = true))
	uint32 bEarlyZPassOnlyMaterialMasking : 1;

	UPROPERTY(config, EditAnywhere, Category=Lighting, meta=(
		ConsoleVariable="r.DBuffer",DisplayName="DBuffer Decals",
		ToolTip="Whether to accumulate decal properties to a buffer before the base pass.  DBuffer decals correctly affect lightmap and sky lighting, unlike regular deferred decals.  DBuffer enabled forces a full prepass.  Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bDBuffer:1;

	UPROPERTY(config, EditAnywhere, Category=Optimizations, meta=(
		ConsoleVariable="r.ClearSceneMethod",DisplayName="Clear Scene",
		ToolTip="Select how the g-buffer is cleared in game mode (only affects deferred shading)."))
	TEnumAsByte<EClearSceneOptions::Type> ClearSceneMethod;

	UPROPERTY(config, EditAnywhere, Category=Optimizations, meta=(
		ConsoleVariable="r.BasePassOutputsVelocity", DisplayName="Accurate velocities from Vertex Deformation",
		ToolTip="Enables materials with time-based World Position Offset and/or World Displacement to output accurate velocities. This incurs a performance cost. If this is disabled, those materials will not output velocities. Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bBasePassOutputsVelocity:1;

	UPROPERTY(config, EditAnywhere, Category=Optimizations, meta=(
		ConsoleVariable="r.SelectiveBasePassOutputs", DisplayName="Selectively output to the GBuffer rendertargets",
		ToolTip="Enables not exporting to the GBuffer rendertargets that are not relevant. Changing this setting requires restarting the editor.",
		ConfigRestartRequired=true))
	uint32 bSelectiveBasePassOutputs:1;

	UPROPERTY(config, EditAnywhere, Category = Optimizations, meta = (
		DisplayName = "Enable Particle Cutouts by default",
		ToolTip = "When enabled, after changing the material on a Required particle module a Particle Cutout texture will be chosen automatically from the Opacity Mask texture if it exists, if not the Opacity Texture will be used if it exists."))
	uint32 bDefaultParticleCutouts : 1;

	UPROPERTY(config, EditAnywhere, Category = Optimizations, meta = (
		ConsoleVariable = "fx.GPUSimulationTextureSizeX",
		DisplayName = "GPU Particle simulation texture size - X",
		ToolTip = "The X size of the GPU simulation texture size. SizeX*SizeY determines the maximum number of GPU simulated particles in an emitter. Potentially overridden by CVar settings in BaseDeviceProfile.ini.",
		ConfigRestartRequired = true))
	int32 GPUSimulationTextureSizeX;
	
	UPROPERTY(config, EditAnywhere, Category = Optimizations, meta = (
		ConsoleVariable = "fx.GPUSimulationTextureSizeY",
		DisplayName = "GPU Particle simulation texture size - Y",
		ToolTip = "The Y size of the GPU simulation texture size. SizeX*SizeY determines the maximum number of GPU simulated particles in an emitter. Potentially overridden by CVar settings in BaseDeviceProfile.ini.",
		ConfigRestartRequired = true))
	int32 GPUSimulationTextureSizeY;

	UPROPERTY(config, EditAnywhere, Category = Lighting, meta = (
		ConsoleVariable = "r.AllowGlobalClipPlane", DisplayName = "Support global clip plane for Planar Reflections",
		ToolTip = "Whether to support the global clip plane needed for planar reflections.  Enabling this increases BasePass triangle cost by ~15% regardless of whether planar reflections are active. Changing this setting requires restarting the editor.",
		ConfigRestartRequired = true))
	uint32 bGlobalClipPlane : 1;

	UPROPERTY(config, EditAnywhere, Category = Optimizations, meta = (
		ConsoleVariable = "r.GBufferFormat", DisplayName = "GBuffer Format",
		ToolTip = "Selects which GBuffer format should be used. Affects performance primarily via how much GPU memory bandwidth used."))
	TEnumAsByte<EGBufferFormat::Type> GBufferFormat;

	UPROPERTY(config, EditAnywhere, Category = Optimizations, meta = (
		ConsoleVariable = "r.MorphTarget.Mode", DisplayName = "Use GPU for computing morph targets",
		ToolTip = "Whether to use original CPU method (loop per morph then by vertex) or use a GPU-based method on Shader Model 5 hardware."))
	uint32 bUseGPUMorphTargets : 1;
	
	UPROPERTY(config, EditAnywhere, Category = Debugging, meta = (
		ConsoleVariable = "r.GPUCrashDebugging", DisplayName = "Enable vendor specific GPU crash analysis tools",
		ToolTip = "Enables vendor specific GPU crash analysis tools.  Currently only supports NVIDIA Aftermath on DX11.",
		ConfigRestartRequired = true))
		uint32 bNvidiaAftermathEnabled : 1;

	UPROPERTY(config, EditAnywhere, Category=VR, meta=(
		ConsoleVariable="vr.InstancedStereo", DisplayName="Instanced Stereo",
		ToolTip="Enable instanced stereo rendering (only available for D3D SM5 or PS4).",
		ConfigRestartRequired=true))
	uint32 bInstancedStereo:1;

	UPROPERTY(config, EditAnywhere, Category = VR, meta = (
		EditCondition = "bInstancedStereo",
		ConsoleVariable = "vr.MultiView", DisplayName = "Multi-View",
		ToolTip = "Enable multi-view for instanced stereo rendering (only available on the PS4).",
		ConfigRestartRequired = true))
	uint32 bMultiView : 1;

	UPROPERTY(config, EditAnywhere, Category = VR, meta = (
		ConsoleVariable = "vr.MobileMultiView", DisplayName = "Mobile Multi-View",
		ToolTip = "Enable mobile multi-view rendering (only available on some Gear VR Android devices using OpenGL ES 2.0).",
		ConfigRestartRequired = true))
		uint32 bMobileMultiView : 1;

	UPROPERTY(config, EditAnywhere, Category = VR, meta = (
		EditCondition = "bMobileMultiView",
		ConsoleVariable = "vr.MobileMultiView.Direct", DisplayName = "Mobile Multi-View Direct",
		ToolTip = "Enable direct mobile multi-view rendering (only available on multi-view enabled Gear VR and Daydream Android devices).",
		ConfigRestartRequired = true))
		uint32 bMobileMultiViewDirect : 1;

	UPROPERTY(config, EditAnywhere, Category = VR, meta = (
		ConsoleVariable = "vr.RoundRobinOcclusion", DisplayName = "Round Robin Occlusion Queries",
		ToolTip = "Enable round-robin scheduling of occlusion queries for VR.",
		ConfigRestartRequired = false))
	uint32 bRoundRobinOcclusion : 1;

	UPROPERTY(config, EditAnywhere, Category = Experimental, meta = (
		ConsoleVariable = "vr.ODSCapture", DisplayName = "Omni-directional Stereo Capture",
		ToolTip = "Enable Omni-directional Stereo Capture.",
		ConfigRestartRequired = true))
		uint32 bODSCapture : 1;

	UPROPERTY(config, EditAnywhere, Category=Editor, meta=(
		ConsoleVariable="r.WireframeCullThreshold",DisplayName="Wireframe Cull Threshold",
		ToolTip="Screen radius at which wireframe objects are culled. Larger values can improve performance when viewing a scene in wireframe."))
	float WireframeCullThreshold;

	/**
	"Ray Tracing settings."
	*/
	UPROPERTY(config, EditAnywhere, Category = RayTracing, meta = (
		ConsoleVariable = "r.RayTracing", DisplayName = "Ray Tracing",
		ToolTip = "Enable Ray Tracing capabilities.  Requires 'Support Compute Skincache' before project is allowed to set this.",
		ConfigRestartRequired = true))
		uint32 bEnableRayTracing : 1;

	/**
	"Stationary skylight requires permutations of the basepass shaders.  Disabling will reduce the number of shader permutations required per material. Changing this setting requires restarting the editor."
	*/
	UPROPERTY(config, EditAnywhere, Category = ShaderPermutationReduction, meta = (
		ConsoleVariable = "r.SupportStationarySkylight", DisplayName = "Support Stationary Skylight",
		ConfigRestartRequired = true))
		uint32 bSupportStationarySkylight : 1;

	/**
	"Low quality lightmap requires permutations of the lightmap rendering shaders.  Disabling will reduce the number of shader permutations required per material. Note that the mobile renderer requires low quality lightmaps, so disabling this setting is not recommended for mobile titles using static lighting. Changing this setting requires restarting the editor."
	*/
	UPROPERTY(config, EditAnywhere, Category = ShaderPermutationReduction, meta = (
		ConsoleVariable = "r.SupportLowQualityLightmaps", DisplayName = "Support low quality lightmap shader permutations",
		ConfigRestartRequired = true))
		uint32 bSupportLowQualityLightmaps : 1;

	/**
	PointLight WholeSceneShadows requires many vertex and geometry shader permutations for cubemap rendering. Disabling will reduce the number of shader permutations required per material. Changing this setting requires restarting the editor."
	*/
	UPROPERTY(config, EditAnywhere, Category = ShaderPermutationReduction, meta = (
		ConsoleVariable = "r.SupportPointLightWholeSceneShadows", DisplayName = "Support PointLight WholeSceneShadows",
		ConfigRestartRequired = true))
		uint32 bSupportPointLightWholeSceneShadows : 1;

	/** 
	"Atmospheric fog requires permutations of the basepass shaders.  Disabling will reduce the number of shader permutations required per material. Changing this setting requires restarting the editor."
	*/
	UPROPERTY(config, EditAnywhere, Category = ShaderPermutationReduction, meta = (
		ConsoleVariable = "r.SupportAtmosphericFog", DisplayName = "Support Atmospheric Fog",	
		ConfigRestartRequired = true))
		uint32 bSupportAtmosphericFog : 1;

	/**
	"Skincache allows a compute shader to skin once each vertex, save those results into a new buffer and reuse those calculations when later running the depth, base and velocity passes. This also allows opting into the 'recompute tangents' for skinned mesh instance feature. Disabling will reduce the number of shader permutations required per material. Changing this setting requires restarting the editor."
	*/
	UPROPERTY(config, EditAnywhere, Category = Optimizations, meta = (
		ConsoleVariable = "r.SkinCache.CompileShaders", DisplayName = "Support Compute Skincache",
		ToolTip = "Cannot be disabled while Ray Tracing is enabled as it is then required.",
		ConfigRestartRequired = true))
		uint32 bSupportSkinCacheShaders : 1;

	UPROPERTY(config, EditAnywhere, Category = MobileShaderPermutationReduction, meta = (
		ConsoleVariable = "r.Mobile.EnableStaticAndCSMShadowReceivers", DisplayName = "Support Combined Static and CSM Shadowing",
		ToolTip = "Allow primitives to receive both static and CSM shadows from a stationary light. Disabling will free a mobile texture sampler and reduce shader permutations. Changing this setting requires restarting the editor.",
		ConfigRestartRequired = true))
		uint32 bMobileEnableStaticAndCSMShadowReceivers : 1;

	UPROPERTY(config, EditAnywhere, Category = Mobile, meta = (
		ConsoleVariable = "r.Mobile.EnableMovableLightCSMShaderCulling", DisplayName = "Support movable light CSM shader culling",
		ToolTip = "Primitives lit by a movable directional light will render with the CSM shader only when determined to be within CSM range. Changing this setting requires restarting the editor.",
		ConfigRestartRequired = true))
		uint32 bMobileEnableMovableLightCSMShaderCulling : 1;

	UPROPERTY(config, EditAnywhere, Category = MobileShaderPermutationReduction, meta = (
		ConsoleVariable = "r.Mobile.AllowDistanceFieldShadows",
		DisplayName = "Support Distance Field Shadows",
		ToolTip = "Generate shaders for primitives to receive distance field shadows from stationary directional lights. Changing this setting requires restarting the editor.",
		ConfigRestartRequired = true))
		uint32 bMobileAllowDistanceFieldShadows : 1;

	UPROPERTY(config, EditAnywhere, Category = MobileShaderPermutationReduction, meta = (
		ConsoleVariable = "r.Mobile.AllowMovableDirectionalLights",
		DisplayName = "Support Movable Directional Lights",
		ToolTip = "Generate shaders for primitives to receive movable directional lights. Changing this setting requires restarting the editor.",
		ConfigRestartRequired = true))
		uint32 bMobileAllowMovableDirectionalLights : 1;

	UPROPERTY(config, EditAnywhere, Category = MobileShaderPermutationReduction, meta = (
		ConsoleVariable = "r.MobileNumDynamicPointLights", DisplayName = "Max Movable Spotlights / Point Lights", ClampMax = 4,
		ToolTip = "The number of dynamic spotlights or point lights to support on mobile devices. Setting this to 0 for games which do not require dynamic spotlights or point lights will reduce the number of shaders generated. Changing this setting requires restarting the editor.",
		ConfigRestartRequired = true))
		uint32 MobileNumDynamicPointLights;

	UPROPERTY(config, EditAnywhere, Category = MobileShaderPermutationReduction, meta = (
		ConsoleVariable = "r.MobileDynamicPointLightsUseStaticBranch", DisplayName = "Use Shared Movable Spotlight / Point Light Shaders",
		ToolTip = "If this setting is enabled, the same shader will be used for any number of dynamic spotlights or point lights (up to the maximum specified above) hitting a surface. This is slightly slower but reduces the number of shaders generated. Changing this setting requires restarting the editor.",
		ConfigRestartRequired = true))
		uint32 bMobileDynamicPointLightsUseStaticBranch : 1;

	UPROPERTY(config, EditAnywhere, Category = MobileShaderPermutationReduction, meta = (
		ConsoleVariable = "r.Mobile.EnableMovableSpotlights",
		DisplayName = "Support Movable Spotlights",
		ToolTip = "Generate shaders for primitives to receive lighting from movable spotlights. This incurs an additional cost when processing movable lights. Changing this setting requires restarting the editor.",
		ConfigRestartRequired = true))
		uint32 bMobileAllowMovableSpotlights : 1;

	UPROPERTY(config, EditAnywhere, Category = Optimizations, meta = (
		ConsoleVariable = "r.SkinCache.SceneMemoryLimitInMB", DisplayName = "Maximum memory for Compute Skincache per world (MB)",
		ToolTip = "Maximum amount of memory (in MB) per world/scene allowed for the Compute Skincache to generate output vertex data and recompute tangents."))
		float SkinCacheSceneMemoryLimitInMB;

	UPROPERTY(config, EditAnywhere, Category = Optimizations, meta = (
		ConsoleVariable = "r.GPUSkin.Limit2BoneInfluences", DisplayName = "Limit GPU skinning to 2 bones influence",
		ToolTip = "Whether to use 2 bone influences instead of the default of 4 for GPU skinning. This does not change skeletal mesh assets but reduces the number of instructions required by the GPU skin vertex shaders. Changing this setting requires restarting the editor.",
		ConfigRestartRequired = true))
		uint32 bGPUSkinLimit2BoneInfluences : 1;

	UPROPERTY(config, EditAnywhere, Category = Optimizations, meta = (
		ConsoleVariable = "r.SupportDepthOnlyIndexBuffers", DisplayName = "Support depth-only index buffers",
		ToolTip = "Support depth-only index buffers, which provide a minor rendering speedup at the expense of using twice the index buffer memory.",
		ConfigRestartRequired = true))
		uint32 bSupportDepthOnlyIndexBuffers : 1;

	UPROPERTY(config, EditAnywhere, Category = Optimizations, meta = (
		ConsoleVariable = "r.SupportReversedIndexBuffers", DisplayName = "Support reversed index buffers",
		ToolTip = "Support reversed index buffers, which provide a minor rendering speedup at the expense of using twice the index buffer memory.",
		ConfigRestartRequired = true))
		uint32 bSupportReversedIndexBuffers : 1;

	UPROPERTY(config, EditAnywhere, Category = Experimental, meta = (
		ConsoleVariable = "r.SupportMaterialLayers", Tooltip = "Support new material layering system. Disabling it reduces some overhead in place to support the experimental feature",
		ConfigRestartRequired = true))
		uint32 bSupportMaterialLayers : 1;

public:

	//~ Begin UObject Interface

	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif

	//~ End UObject Interface

private:
	void SanatizeReflectionCaptureResolution();
};

UCLASS(config = Engine, globaluserconfig, meta = (DisplayName = "Rendering Overrides (Local)"))
class ENGINE_API URendererOverrideSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	/**
		"Enabling will locally override all ShaderPermutationReduction settings from the Renderer section to be enabled.  Saved to local user config only.."
	*/
	UPROPERTY(config, EditAnywhere, Category = ShaderPermutationReduction, meta = (
		ConsoleVariable = "r.SupportAllShaderPermutations", DisplayName = "Force all shader permutation support",
		ConfigRestartRequired = true))
		uint32 bSupportAllShaderPermutations : 1;

	UPROPERTY(config, EditAnywhere, Category = Miscellaneous, meta = (
		ConsoleVariable = "r.SkinCache.ForceRecomputeTangents", DisplayName = "Force all skinned meshes to recompute tangents (also forces Compute SkinCache)",
		ConfigRestartRequired = true))
		uint32 bForceRecomputeTangents : 1;

public:

	//~ Begin UObject Interface

	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
