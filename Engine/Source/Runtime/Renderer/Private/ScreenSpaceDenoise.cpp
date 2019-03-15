// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenSpaceDenoise.cpp: Denoise in screen space.
=============================================================================*/

#include "ScreenSpaceDenoise.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneRenderTargetParameters.h"
#include "ScenePrivate.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"
#include "SceneViewFamilyBlackboard.h"


// ---------------------------------------------------- Cvars


static TAutoConsoleVariable<int32> CVarShadowUse1SPPCodePath(
	TEXT("r.Shadow.Denoiser.Use1SPPCodePath"), 0,
	TEXT("Whether to use the 1spp code path."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarShadowReconstructionSampleCount(
	TEXT("r.Shadow.Denoiser.ReconstructionSamples"), 8,
	TEXT("Maximum number of samples for the reconstruction pass (default = 16)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarShadowPreConvolutionCount(
	TEXT("r.Shadow.Denoiser.PreConvolution"), 1,
	TEXT("Number of pre-convolution passes (default = 1)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarShadowTemporalAccumulation(
	TEXT("r.Shadow.Denoiser.TemporalAccumulation"), 1,
	TEXT(""),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarShadowHistoryConvolutionSampleCount(
	TEXT("r.Shadow.Denoiser.HistoryConvolutionSamples"), 1,
	TEXT("Number of samples to use to convolve the history over time."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarReflectionReconstructionSampleCount(
	TEXT("r.Reflections.Denoiser.ReconstructionSamples"), 16,
	TEXT("Maximum number of samples for the reconstruction pass (default = 16)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarReflectionTemporalAccumulation(
	TEXT("r.Reflections.Denoiser.TemporalAccumulation"), 1,
	TEXT("Accumulates the samples over multiple frames."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarReflectionHistoryConvolutionSampleCount(
	TEXT("r.Reflections.Denoiser.HistoryConvolution.SampleCount"), 1,
	TEXT("Number of samples to use for history post filter (default = 1)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarAOReconstructionSampleCount(
	TEXT("r.AmbientOcclusion.Denoiser.ReconstructionSamples"), 16,
	TEXT("Maximum number of samples for the reconstruction pass (default = 16)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarAOTemporalAccumulation(
	TEXT("r.AmbientOcclusion.Denoiser.TemporalAccumulation"), 1,
	TEXT("Accumulates the samples over multiple frames."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarAOHistoryConvolutionSampleCount(
	TEXT("r.AmbientOcclusion.Denoiser.HistoryConvolution.SampleCount"), 16,
	TEXT("Number of samples to use for history post filter (default = 16)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarAOHistoryConvolutionKernelSpreadFactor(
	TEXT("r.AmbientOcclusion.Denoiser.HistoryConvolution.KernelSpreadFactor"), 3,
	TEXT("Multiplication factor applied on the kernel sample offset (default=3)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarGIReconstructionSampleCount(
	TEXT("r.GlobalIllumination.Denoiser.ReconstructionSamples"), 16,
	TEXT("Maximum number of samples for the reconstruction pass (default = 16)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarGITemporalAccumulation(
	TEXT("r.GlobalIllumination.Denoiser.TemporalAccumulation"), 1,
	TEXT("Accumulates the samples over multiple frames."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarGIHistoryConvolutionSampleCount(
	TEXT("r.GlobalIllumination.Denoiser.HistoryConvolution.SampleCount"), 16,
	TEXT("Number of samples to use for history post filter (default = 1)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarGIHistoryConvolutionKernelSpreadFactor(
	TEXT("r.GlobalIllumination.Denoiser.HistoryConvolution.KernelSpreadFactor"), 3,
	TEXT("Multiplication factor applied on the kernel sample offset (default=3)."),
	ECVF_RenderThreadSafe);


/** The maximum number of mip level supported in the denoiser. */
static const int32 kMaxMipLevel = 4;

/** Maximum number of sample per pixel supported in the stackowiak sample set. */
static const int32 kStackowiakMaxSampleCountPerSet = 56;

/** The maximum number of buffers. */
static const int32 kMaxBufferProcessingCount = IScreenSpaceDenoiser::kMaxBatchSize;

static_assert(IScreenSpaceDenoiser::kMaxBatchSize <= kMaxBufferProcessingCount, "Can't batch more signal than there is internal buffer in the denoiser.");


// ---------------------------------------------------- Globals

const IScreenSpaceDenoiser* GScreenSpaceDenoiser = nullptr;

namespace
{

// ---------------------------------------------------- Enums

/** Different signals to denoise. */
enum class ESignalProcessing
{
	MonochromaticPenumbra,
	Reflections,
	AmbientOcclusion,
	GlobalIllumination,

	MAX,
};


// ---------------------------------------------------- Simple functions

static bool IsSupportedLightType(ELightComponentType LightType)
{
	return LightType == LightType_Point || LightType == LightType_Directional || LightType == LightType_Rect || LightType == LightType_Spot;
}

/** Returns whether a signal processing is supported by the constant pixel density pass layout. */
static bool UsesConstantPixelDensityPassLayout(ESignalProcessing SignalProcessing)
{
	return (
		SignalProcessing == ESignalProcessing::MonochromaticPenumbra ||
		SignalProcessing == ESignalProcessing::Reflections ||
		SignalProcessing == ESignalProcessing::AmbientOcclusion ||
		SignalProcessing == ESignalProcessing::GlobalIllumination);
}

/** Returns whether a signal processing uses an injestion pass. */
static bool SignalUsesInjestion(ESignalProcessing SignalProcessing)
{
	return SignalProcessing == ESignalProcessing::MonochromaticPenumbra;
}

/** Returns whether a signal processing uses an additional pre convolution pass. */
static bool SignalUsesPreConvolution(ESignalProcessing SignalProcessing)
{
	return SignalProcessing == ESignalProcessing::MonochromaticPenumbra;
}

/** Returns whether a signal processing uses a history rejection pre convolution pass. */
static bool SignalUsesRejectionPreConvolution(ESignalProcessing SignalProcessing)
{
	return (
		//SignalProcessing == ESignalProcessing::MonochromaticPenumbra ||
		SignalProcessing == ESignalProcessing::Reflections);
}

/** Returns whether a signal processing uses a history rejection pre convolution pass. */
static bool SignalUsesFinalConvolution(ESignalProcessing SignalProcessing)
{
	return SignalProcessing == ESignalProcessing::MonochromaticPenumbra;
}

/** Returns the number of signal that might be batched at the same time. */
static int32 SignalMaxBatchSize(ESignalProcessing SignalProcessing)
{
	if (SignalProcessing == ESignalProcessing::MonochromaticPenumbra)
	{
		return IScreenSpaceDenoiser::kMaxBatchSize;
	}
	else if (
		SignalProcessing == ESignalProcessing::Reflections ||
		SignalProcessing == ESignalProcessing::AmbientOcclusion ||
		SignalProcessing == ESignalProcessing::GlobalIllumination)
	{
		return 1;
	}
	check(0);
	return 1;
}

/** Returns whether a signal can denoise multi sample per pixel. */
static bool SignalSupportMultiSPP(ESignalProcessing SignalProcessing)
{
	return SignalProcessing == ESignalProcessing::MonochromaticPenumbra;
}


// ---------------------------------------------------- Shaders

// Permutation dimension for the type of signal being denoised.
class FSignalProcessingDim : SHADER_PERMUTATION_ENUM_CLASS("DIM_SIGNAL_PROCESSING", ESignalProcessing);

// Permutation dimension for the number of signal being denoised at the same time.
class FSignalBatchSizeDim : SHADER_PERMUTATION_RANGE_INT("DIM_SIGNAL_BATCH_SIZE", 1, IScreenSpaceDenoiser::kMaxBatchSize);

// Permutation dimension for denoising multiple sample at same time.
class FMultiSPPDim : SHADER_PERMUTATION_BOOL("DIM_MULTI_SPP");


const TCHAR* const kInjestResourceNames[] = {
	// Penumbra
	TEXT("ShadowDenoiserInjest0"),
	TEXT("ShadowDenoiserInjest1"),
	TEXT("ShadowDenoiserInjest2"),
	TEXT("ShadowDenoiserInjest3"),

	// Reflections
	nullptr,
	nullptr,
	nullptr,
	nullptr,

	// AmbientOcclusion
	nullptr,
	nullptr,
	nullptr,
	nullptr,

	// GlobalIllumination
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};

const TCHAR* const kReconstructionResourceNames[] = {
	// Penumbra
	TEXT("ShadowReconstruction0"),
	TEXT("ShadowReconstruction1"),
	TEXT("ShadowReconstruction2"),
	TEXT("ShadowReconstruction3"),

	// Reflections
	TEXT("ReflectionsReconstruction0"),
	TEXT("ReflectionsReconstruction1"),
	TEXT("ReflectionsReconstruction2"),
	TEXT("ReflectionsReconstruction3"),

	// AmbientOcclusion
	TEXT("AOReconstruction0"),
	TEXT("AOReconstruction1"),
	TEXT("AOReconstruction2"),
	TEXT("AOReconstruction3"),

	// GlobalIllumination
	TEXT("GIReconstruction0"),
	TEXT("GIReconstruction1"),
	TEXT("GIReconstruction2"),
	TEXT("GIReconstruction3"),
};

const TCHAR* const kPreConvolutionResourceNames[] = {
	// Penumbra
	TEXT("ShadowPreConvolution0"),
	TEXT("ShadowPreConvolution1"),
	TEXT("ShadowPreConvolution2"),
	TEXT("ShadowPreConvolution3"),

	// Reflections
	nullptr,
	nullptr,
	nullptr,
	nullptr,

	// AmbientOcclusion
	nullptr,
	nullptr,
	nullptr,
	nullptr,

	// GlobalIllumination
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};

const TCHAR* const kRejectionPreConvolutionResourceNames[] = {
	// Penumbra
	TEXT("ShadowRejectionPreConvolution0"),
	TEXT("ShadowRejectionPreConvolution1"),
	TEXT("ShadowRejectionPreConvolution2"),
	TEXT("ShadowRejectionPreConvolution3"),

	// Reflections
	TEXT("ReflectionsRejectionPreConvolution0"),
	TEXT("ReflectionsRejectionPreConvolution1"),
	TEXT("ReflectionsRejectionPreConvolution2"),
	TEXT("ReflectionsRejectionPreConvolution3"),

	// AmbientOcclusion
	nullptr,
	nullptr,
	nullptr,
	nullptr,

	// GlobalIllumination
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};

const TCHAR* const kTemporalAccumulationResourceNames[] = {
	// Penumbra
	TEXT("ShadowTemporalAccumulation0"),
	TEXT("ShadowTemporalAccumulation1"),
	TEXT("ShadowTemporalAccumulation2"),
	TEXT("ShadowTemporalAccumulation3"),

	// Reflections
	TEXT("ReflectionsTemporalAccumulation0"),
	TEXT("ReflectionsTemporalAccumulation1"),
	TEXT("ReflectionsTemporalAccumulation2"),
	TEXT("ReflectionsTemporalAccumulation3"),

	// AmbientOcclusion
	TEXT("AOTemporalAccumulation0"),
	TEXT("AOTemporalAccumulation1"),
	TEXT("AOTemporalAccumulation2"),
	TEXT("AOTemporalAccumulation3"),

	// GlobalIllumination
	TEXT("GITemporalAccumulation0"),
	TEXT("GITemporalAccumulation1"),
	TEXT("GITemporalAccumulation2"),
	TEXT("GITemporalAccumulation3"),
};

const TCHAR* const kHistoryConvolutionResourceNames[] = {
	// Penumbra
	TEXT("ShadowHistoryConvolution0"),
	TEXT("ShadowHistoryConvolution1"),
	TEXT("ShadowHistoryConvolution2"),
	TEXT("ShadowHistoryConvolution3"),

	// Reflections
	TEXT("ReflectionsHistoryConvolution0"),
	TEXT("ReflectionsHistoryConvolution1"),
	TEXT("ReflectionsHistoryConvolution2"),
	TEXT("ReflectionsHistoryConvolution3"),

	// AmbientOcclusion
	TEXT("AOHistoryConvolution0"),
	TEXT("AOHistoryConvolution1"),
	TEXT("AOHistoryConvolution2"),
	TEXT("AOHistoryConvolution3"),

	// GlobalIllumination
	TEXT("GIHistoryConvolution0"),
	TEXT("GIHistoryConvolution1"),
	TEXT("GIHistoryConvolution2"),
	TEXT("GIHistoryConvolution3"),
};

const TCHAR* const kDenoiserOutputResourceNames[] = {
	// Penumbra
	TEXT("ShadowDenoiserOutput0"),
	TEXT("ShadowDenoiserOutput1"),
	TEXT("ShadowDenoiserOutput2"),
	TEXT("ShadowDenoiserOutput3"),

	// Reflections
	nullptr,
	nullptr,
	nullptr,
	nullptr,

	// AmbientOcclusion
	nullptr,
	nullptr,
	nullptr,
	nullptr,

	// GlobalIllumination
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};

static_assert(ARRAY_COUNT(kReconstructionResourceNames) == int32(ESignalProcessing::MAX) * kMaxBufferProcessingCount, "You forgot me!");
static_assert(ARRAY_COUNT(kRejectionPreConvolutionResourceNames) == int32(ESignalProcessing::MAX) * kMaxBufferProcessingCount, "You forgot me!");
static_assert(ARRAY_COUNT(kTemporalAccumulationResourceNames) == int32(ESignalProcessing::MAX) * kMaxBufferProcessingCount, "You forgot me!");
static_assert(ARRAY_COUNT(kHistoryConvolutionResourceNames) == int32(ESignalProcessing::MAX) * kMaxBufferProcessingCount, "You forgot me!");
static_assert(ARRAY_COUNT(kDenoiserOutputResourceNames) == int32(ESignalProcessing::MAX) * kMaxBufferProcessingCount, "You forgot me!");


/** Base class for a screen space denoising shader. */
class FScreenSpaceDenoisingShader : public FGlobalShader
{
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM5;
	}

	FScreenSpaceDenoisingShader() {}
	FScreenSpaceDenoisingShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }
};


/** Shader parameter structure used for all shaders. */
BEGIN_SHADER_PARAMETER_STRUCT(FSSDCommonParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FSceneViewFamilyBlackboard, SceneBlackboard)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, EyeAdaptation)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, TileClassificationTexture)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
END_SHADER_PARAMETER_STRUCT()

/** Shader parameter structure use to bind all signal generically. */
BEGIN_SHADER_PARAMETER_STRUCT(FSSDSignalTextures, )
	SHADER_PARAMETER_RDG_TEXTURE_ARRAY(Texture2D, Textures, [kMaxBufferProcessingCount])
END_SHADER_PARAMETER_STRUCT()

/** Shader parameter structure use to bind all signal generically. */
BEGIN_SHADER_PARAMETER_STRUCT(FSSDSignalUAVs, )
	SHADER_PARAMETER_RDG_TEXTURE_UAV_ARRAY(Texture2D, UAVs, [kMaxBufferProcessingCount])
END_SHADER_PARAMETER_STRUCT()

/** Shader parameter structure to have all information to spatial filtering. */
BEGIN_SHADER_PARAMETER_STRUCT(FSSDConvolutionMetaData, )
	SHADER_PARAMETER_ARRAY(FVector4, LightPositionAndRadius, [IScreenSpaceDenoiser::kMaxBatchSize])
	SHADER_PARAMETER_ARRAY(FVector4, LightDirectionAndLength, [IScreenSpaceDenoiser::kMaxBatchSize])
	SHADER_PARAMETER_ARRAY(float, HitDistanceToWorldBluringRadius, [IScreenSpaceDenoiser::kMaxBatchSize])
	SHADER_PARAMETER_ARRAY(uint32, LightType, [IScreenSpaceDenoiser::kMaxBatchSize])
END_SHADER_PARAMETER_STRUCT()


FSSDSignalTextures CreateMultiplexedTextures(
	FRDGBuilder& GraphBuilder,
	int32 TextureCount,
	const TStaticArray<FRDGTextureDesc, kMaxBufferProcessingCount>& DescArray,
	const TCHAR* const* TextureNames)
{
	check(TextureCount <= kMaxBufferProcessingCount);
	FSSDSignalTextures SignalTextures;
	for (int32 i = 0; i < TextureCount; i++)
	{
		const TCHAR* TextureName = TextureNames[i];
		SignalTextures.Textures[i] = GraphBuilder.CreateTexture(DescArray[i], TextureName);
	}
	return SignalTextures;
}

FSSDSignalUAVs CreateMultiplexedUAVs(FRDGBuilder& GraphBuilder, const FSSDSignalTextures& SignalTextures)
{
	FSSDSignalUAVs UAVs;
	for (int32 i = 0; i < kMaxBufferProcessingCount; i++)
	{
		if (SignalTextures.Textures[i])
			UAVs.UAVs[i] = GraphBuilder.CreateUAV(SignalTextures.Textures[i]);
	}
	return UAVs;
}


class FSSDInjestCS : public FScreenSpaceDenoisingShader
{
	DECLARE_GLOBAL_SHADER(FSSDInjestCS);
	SHADER_USE_PARAMETER_STRUCT(FSSDInjestCS, FScreenSpaceDenoisingShader);

	using FPermutationDomain = TShaderPermutationDomain<FSignalProcessingDim, FSignalBatchSizeDim, FMultiSPPDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		ESignalProcessing SignalProcessing = PermutationVector.Get<FSignalProcessingDim>();

		// Only compile this shader for signal processing that uses it.
		if (!SignalUsesInjestion(SignalProcessing))
		{
			return false;
		}

		// Not all signal processing allow to batch multiple signals at the same time.
		if (PermutationVector.Get<FSignalBatchSizeDim>() > SignalMaxBatchSize(SignalProcessing))
		{
			return false;
		}

		// Only compiler multi SPP permutation for signal that supports it.
		if (PermutationVector.Get<FMultiSPPDim>() && !SignalSupportMultiSPP(SignalProcessing))
		{
			return false;
		}

		return FScreenSpaceDenoisingShader::ShouldCompilePermutation(Parameters);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSSDCommonParameters, CommonParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSSDConvolutionMetaData, ConvolutionMetaData)

		SHADER_PARAMETER_STRUCT(FSSDSignalTextures, SignalInput)
		SHADER_PARAMETER_STRUCT(FSSDSignalUAVs, SignalOutput)
	END_SHADER_PARAMETER_STRUCT()
};

class FSSDSpatialAccumulationCS : public FScreenSpaceDenoisingShader
{
	DECLARE_GLOBAL_SHADER(FSSDSpatialAccumulationCS);
	SHADER_USE_PARAMETER_STRUCT(FSSDSpatialAccumulationCS, FScreenSpaceDenoisingShader);

	static const uint32 kGroupSize = 8;
	
	enum class EStage
	{
		// Spatial kernel used to process raw input for the temporal accumulation.
		ReConstruction,

		// Spatial kernel to pre filter.
		PreConvolution,

		// Spatial kernel used to pre convolve history rejection.
		RejectionPreConvolution,

		// Spatial kernel used to post filter the temporal accumulation.
		PostFiltering,

		// Final spatial kernel, that may output specific buffer encoding to integrate with the rest of the renderer
		FinalOutput,

		MAX
	};

	class FStageDim : SHADER_PERMUTATION_ENUM_CLASS("DIM_STAGE", EStage);
	class FUpscaleDim : SHADER_PERMUTATION_BOOL("DIM_UPSCALE");

	using FPermutationDomain = TShaderPermutationDomain<FSignalProcessingDim, FStageDim, FUpscaleDim, FSignalBatchSizeDim, FMultiSPPDim>;
	
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		ESignalProcessing SignalProcessing = PermutationVector.Get<FSignalProcessingDim>();

		// Only constant pixel density pass layout uses this shader.
		if (!UsesConstantPixelDensityPassLayout(PermutationVector.Get<FSignalProcessingDim>()))
		{
			return false;
		}

		// Not all signal processing allow to batch multiple signals at the same time.
		if (PermutationVector.Get<FSignalBatchSizeDim>() > SignalMaxBatchSize(SignalProcessing))
		{
			return false;
		}

		// Only reconstruction have upscale capability for now.
		if (PermutationVector.Get<FUpscaleDim>() && 
			PermutationVector.Get<FStageDim>() != EStage::ReConstruction)
		{
			return false;
		}

		// Only compile pre convolution for signal that uses it.
		if (!SignalUsesPreConvolution(SignalProcessing) &&
			PermutationVector.Get<FStageDim>() == EStage::PreConvolution)
		{
			return false;
		}

		// Only compile rejection pre convolution for signal that uses it.
		if (!SignalUsesRejectionPreConvolution(SignalProcessing) &&
			PermutationVector.Get<FStageDim>() == EStage::RejectionPreConvolution)
		{
			return false;
		}

		// Only compile final convolution for signal that uses it.
		if (!SignalUsesFinalConvolution(SignalProcessing) &&
			PermutationVector.Get<FStageDim>() == EStage::FinalOutput)
		{
			return false;
		}

		// Only compile multi SPP permutation for signal that supports it.
		if (PermutationVector.Get<FStageDim>() == EStage::ReConstruction &&
			PermutationVector.Get<FMultiSPPDim>() && !SignalSupportMultiSPP(SignalProcessing))
		{
			return false;
		}

		// Only the reconstruction pass can support 1spp.
		if (PermutationVector.Get<FStageDim>() != EStage::ReConstruction &&
			!PermutationVector.Get<FMultiSPPDim>())
		{
			return false;
		}

		return FScreenSpaceDenoisingShader::ShouldCompilePermutation(Parameters);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MaxSampleCount)
		SHADER_PARAMETER(int32, UpscaleFactor)
		SHADER_PARAMETER(float, KernelSpreadFactor)

		SHADER_PARAMETER_STRUCT_INCLUDE(FSSDCommonParameters, CommonParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSSDConvolutionMetaData, ConvolutionMetaData)

		SHADER_PARAMETER_STRUCT(FSSDSignalTextures, SignalInput)
		SHADER_PARAMETER_STRUCT(FSSDSignalUAVs, SignalOutput)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, DebugOutput) // TODO: remove
	END_SHADER_PARAMETER_STRUCT()
};

class FSSDTemporalAccumulationCS : public FScreenSpaceDenoisingShader
{
	DECLARE_GLOBAL_SHADER(FSSDTemporalAccumulationCS);
	SHADER_USE_PARAMETER_STRUCT(FSSDTemporalAccumulationCS, FScreenSpaceDenoisingShader);

	using FPermutationDomain = TShaderPermutationDomain<FSignalProcessingDim, FSignalBatchSizeDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		ESignalProcessing SignalProcessing = PermutationVector.Get<FSignalProcessingDim>();

		// Only constant pixel density pass layout uses this shader.
		if (!UsesConstantPixelDensityPassLayout(SignalProcessing))
		{
			return false;
		}

		// Not all signal processing allow to batch multiple signals at the same time.
		if (PermutationVector.Get<FSignalBatchSizeDim>() > SignalMaxBatchSize(SignalProcessing))
		{
			return false;
		}

		return FScreenSpaceDenoisingShader::ShouldCompilePermutation(Parameters);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_ARRAY(int32, bCameraCut, [IScreenSpaceDenoiser::kMaxBatchSize])
		SHADER_PARAMETER(FMatrix, PrevScreenToTranslatedWorld)

		SHADER_PARAMETER_STRUCT_INCLUDE(FSSDCommonParameters, CommonParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSSDConvolutionMetaData, ConvolutionMetaData)

		SHADER_PARAMETER_STRUCT(FSSDSignalTextures, SignalInput)
		SHADER_PARAMETER_STRUCT(FSSDSignalTextures, HistoryRejectionSignal)
		SHADER_PARAMETER_STRUCT(FSSDSignalUAVs, SignalHistoryOutput)

		SHADER_PARAMETER_STRUCT(FSSDSignalTextures, PrevHistory)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevDepthBuffer)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevGBufferA)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevGBufferB)
		
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, DebugOutput) // TODO: remove
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FSSDInjestCS, "/Engine/Private/ScreenSpaceDenoise/SSDInjest.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FSSDSpatialAccumulationCS, "/Engine/Private/ScreenSpaceDenoise/SSDSpatialAccumulation.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FSSDTemporalAccumulationCS, "/Engine/Private/ScreenSpaceDenoise/SSDTemporalAccumulation.usf", "MainCS", SF_Compute);

} // namespace


/** Generic settings to denoise signal at constant pixel density across the viewport. */
struct FSSDConstantPixelDensitySettings
{
	ESignalProcessing SignalProcessing;
	int32 SignalBatchSize = 1;
	int32 MaxInputSPP = 1;
	float InputResolutionFraction = 1.0f;
	int32 ReconstructionSamples = 1;
	int32 PreConvolutionCount = 0;
	bool bUseTemporalAccumulation = false;
	int32 HistoryConvolutionSampleCount = 1;
	float HistoryConvolutionKernelSpreadFactor = 1.0f;
	TStaticArray<const FLightSceneInfo*, IScreenSpaceDenoiser::kMaxBatchSize> LightSceneInfo;
};


/** Denoises a signal at constant pixel density across the viewport. */
static void DenoiseSignalAtConstantPixelDensity(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FSceneViewFamilyBlackboard& SceneBlackboard,
	const FSSDSignalTextures& InputSignal,
	FSSDConstantPixelDensitySettings Settings,
	TStaticArray<FScreenSpaceFilteringHistory*, IScreenSpaceDenoiser::kMaxBatchSize> PrevFilteringHistory,
	TStaticArray<FScreenSpaceFilteringHistory*, IScreenSpaceDenoiser::kMaxBatchSize> NewFilteringHistory,
	FSSDSignalTextures* OutputSignal)
{
	check(UsesConstantPixelDensityPassLayout(Settings.SignalProcessing));
	ensure(Settings.InputResolutionFraction == 1.0f || Settings.InputResolutionFraction == 0.5f);
	
	auto GetResourceNames = [&](const TCHAR* const ResourceNames[])
	{
		return ResourceNames + (int32(Settings.SignalProcessing) * kMaxBufferProcessingCount);
	};

	const bool bUseMultiInputSPPShaderPath = (
		Settings.MaxInputSPP > 1 || 
		(CVarShadowUse1SPPCodePath.GetValueOnRenderThread() == 0 && Settings.SignalProcessing == ESignalProcessing::MonochromaticPenumbra));

	const FIntPoint DenoiseResolution = View.ViewRect.Size();
	
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(GraphBuilder.RHICmdList);

	// Number of signal to batch.
	int32 MaxSignalBatchSize = SignalMaxBatchSize(Settings.SignalProcessing);
	check(Settings.SignalBatchSize >= 1 && Settings.SignalBatchSize <= MaxSignalBatchSize);

	// Number of texture per batched signal.
	int32 InjestTextureCount = 0;
	int32 ReconstructionTextureCount = 0;
	int32 HistoryTextureCountPerSignal = 0;

	// Descriptor to allocate internal denoising buffer.
	bool bHasReconstructionLayoutDifferentFromHistory = false;
	TStaticArray<FRDGTextureDesc, kMaxBufferProcessingCount> InjestDescs;
	TStaticArray<FRDGTextureDesc, kMaxBufferProcessingCount> ReconstructionDescs;
	TStaticArray<FRDGTextureDesc, kMaxBufferProcessingCount> HistoryDescs;
	FRDGTextureDesc DebugDesc;
	{
		static const EPixelFormat PixelFormatPerChannel[] = {
			PF_Unknown,
			PF_R16F,
			PF_G16R16F,
			PF_FloatRGBA, // there is no 16bits float RGB
			PF_FloatRGBA,
		};

		FRDGTextureDesc RefDesc = FRDGTextureDesc::Create2DDesc(
			SceneBlackboard.SceneDepthBuffer->Desc.Extent,
			PF_Unknown,
			FClearValueBinding::Black,
			/* InFlags = */ TexCreate_None,
			/* InTargetableFlags = */ TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV,
			/* bInForceSeparateTargetAndShaderResource = */ false);

		DebugDesc = RefDesc;
		DebugDesc.Format = PF_FloatRGBA;

		for (int32 i = 0; i < kMaxBufferProcessingCount; i++)
		{
			InjestDescs[i] = RefDesc;
			ReconstructionDescs[i] = RefDesc;
			HistoryDescs[i] = RefDesc;
		}

		if (Settings.SignalProcessing == ESignalProcessing::MonochromaticPenumbra)
		{
			check(Settings.SignalBatchSize >= 1 && Settings.SignalBatchSize <= IScreenSpaceDenoiser::kMaxBatchSize);
			if (!bUseMultiInputSPPShaderPath)
			{
				InjestDescs[0].Format = PixelFormatPerChannel[Settings.SignalBatchSize];
				InjestTextureCount = 1;
			}

			for (int32 BatchedSignalId = 0; BatchedSignalId < Settings.SignalBatchSize; BatchedSignalId++)
			{
				if (bUseMultiInputSPPShaderPath)
				{
					InjestDescs[BatchedSignalId / 2].Format = (BatchedSignalId % 2) ? PF_FloatRGBA :  PF_G16R16F;
					InjestTextureCount = BatchedSignalId / 2 + 1;
				}
				ReconstructionDescs[BatchedSignalId].Format = PF_FloatRGBA;
				HistoryDescs[BatchedSignalId].Format = PF_FloatRGBA;
			}

			HistoryTextureCountPerSignal = 1;
			ReconstructionTextureCount = Settings.SignalBatchSize;
			bHasReconstructionLayoutDifferentFromHistory = true;
		}
		else if (Settings.SignalProcessing == ESignalProcessing::Reflections)
		{
			ReconstructionDescs[0].Format = HistoryDescs[0].Format = PF_FloatRGBA;
			ReconstructionDescs[1].Format = HistoryDescs[1].Format = PF_R16F;
			ReconstructionTextureCount = HistoryTextureCountPerSignal = 2;
			bHasReconstructionLayoutDifferentFromHistory = false;
		}
		else if (Settings.SignalProcessing == ESignalProcessing::AmbientOcclusion)
		{
			ReconstructionDescs[0].Format = HistoryDescs[0].Format = PF_G16R16F;
			ReconstructionTextureCount = HistoryTextureCountPerSignal = 1;
			bHasReconstructionLayoutDifferentFromHistory = false;
		}
		else if (Settings.SignalProcessing == ESignalProcessing::GlobalIllumination)
		{
			ReconstructionDescs[0].Format = PF_FloatRGBA;
			ReconstructionDescs[1].Format = PF_R16F;
			ReconstructionTextureCount = 2;

			HistoryDescs[0].Format = PF_FloatRGBA;
			HistoryDescs[1].Format = PF_R16F; //PF_FloatRGB;
			HistoryTextureCountPerSignal = 2;
			bHasReconstructionLayoutDifferentFromHistory = false;
		}
		else
		{
			check(0);
		}

		check(HistoryTextureCountPerSignal > 0);
		check(ReconstructionTextureCount > 0);
	}

	int32 HistoryTextureCount = HistoryTextureCountPerSignal * Settings.SignalBatchSize;

	check(HistoryTextureCount <= kMaxBufferProcessingCount);

	// Setup common shader parameters.
	FSSDCommonParameters CommonParameters;
	{
		CommonParameters.SceneBlackboard = SceneBlackboard;
		CommonParameters.ViewUniformBuffer = View.ViewUniformBuffer;
		CommonParameters.EyeAdaptation = GetEyeAdaptationTexture(GraphBuilder, View);
	}

	// Setup all the metadata to do spatial convolution.
	FSSDConvolutionMetaData ConvolutionMetaData;
	if (Settings.SignalProcessing == ESignalProcessing::MonochromaticPenumbra)
	{
		for (int32 BatchedSignalId = 0; BatchedSignalId < Settings.SignalBatchSize; BatchedSignalId++)
		{
			FLightSceneProxy* LightSceneProxy = Settings.LightSceneInfo[BatchedSignalId]->Proxy;

			FLightShaderParameters Parameters;
			LightSceneProxy->GetLightShaderParameters(Parameters);

			ConvolutionMetaData.LightPositionAndRadius[BatchedSignalId] = FVector4(
				Parameters.Position, Parameters.SourceRadius);
			ConvolutionMetaData.LightDirectionAndLength[BatchedSignalId] = FVector4(
				Parameters.Direction, Parameters.SourceLength);
			ConvolutionMetaData.HitDistanceToWorldBluringRadius[BatchedSignalId] =
				FMath::Tan(0.5 * FMath::DegreesToRadians(LightSceneProxy->GetLightSourceAngle()));
			ConvolutionMetaData.LightType[BatchedSignalId] = LightSceneProxy->GetLightType();
		}
	}

	FSSDSignalTextures SignalHistory = InputSignal;

	// Injestion pass to precompute some values for the reconstruction pass.
	if (SignalUsesInjestion(Settings.SignalProcessing))
	{
		FSSDSignalTextures NewSignalOutput = CreateMultiplexedTextures(
			GraphBuilder,
			InjestTextureCount, InjestDescs,
			GetResourceNames(kInjestResourceNames));

		FSSDInjestCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSSDInjestCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->ConvolutionMetaData = ConvolutionMetaData;
		PassParameters->SignalInput = SignalHistory;
		PassParameters->SignalOutput = CreateMultiplexedUAVs(GraphBuilder, NewSignalOutput);

		FSSDInjestCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FSignalProcessingDim>(Settings.SignalProcessing);
		PermutationVector.Set<FSignalBatchSizeDim>(Settings.SignalBatchSize);
		PermutationVector.Set<FMultiSPPDim>(bUseMultiInputSPPShaderPath);

		TShaderMapRef<FSSDInjestCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SSD Injest(MultiSPP=%i)",
				int32(PermutationVector.Get<FMultiSPPDim>())),
			*ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(DenoiseResolution, FComputeShaderUtils::kGolden2DGroupSize));

		SignalHistory = NewSignalOutput;
	}

	// Spatial reconstruction with multiple important sampling to be more precise in the history rejection.
	{
		FSSDSignalTextures NewSignalOutput = CreateMultiplexedTextures(
			GraphBuilder,
			ReconstructionTextureCount, ReconstructionDescs,
			GetResourceNames(kReconstructionResourceNames));

		FSSDSpatialAccumulationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSSDSpatialAccumulationCS::FParameters>();
		PassParameters->MaxSampleCount = FMath::Clamp(Settings.ReconstructionSamples, 1, kStackowiakMaxSampleCountPerSet);
		PassParameters->UpscaleFactor = int32(1.0f / Settings.InputResolutionFraction);
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->ConvolutionMetaData = ConvolutionMetaData;
		PassParameters->SignalInput = SignalHistory;
		PassParameters->SignalOutput = CreateMultiplexedUAVs(GraphBuilder, NewSignalOutput);
		
		PassParameters->DebugOutput = GraphBuilder.CreateUAV(GraphBuilder.CreateTexture(DebugDesc, TEXT("SSDDebugReflectionReconstruction")));

		FSSDSpatialAccumulationCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FSignalProcessingDim>(Settings.SignalProcessing);
		PermutationVector.Set<FSignalBatchSizeDim>(Settings.SignalBatchSize);
		PermutationVector.Set<FSSDSpatialAccumulationCS::FStageDim>(FSSDSpatialAccumulationCS::EStage::ReConstruction);
		PermutationVector.Set<FSSDSpatialAccumulationCS::FUpscaleDim>(PassParameters->UpscaleFactor != 1);
		PermutationVector.Set<FMultiSPPDim>(bUseMultiInputSPPShaderPath);

		TShaderMapRef<FSSDSpatialAccumulationCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SSD SpatialAccumulation(Reconstruction MaxSamples=%i Upscale=%i MultiSPP=%i)",
				PassParameters->MaxSampleCount,
				int32(PermutationVector.Get<FSSDSpatialAccumulationCS::FUpscaleDim>()),
				int32(PermutationVector.Get<FMultiSPPDim>())),
			*ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(DenoiseResolution, FSSDSpatialAccumulationCS::kGroupSize));

		SignalHistory = NewSignalOutput;
	}

	// Spatial pre convolutions
	for (int32 PreConvolutionId = 0; PreConvolutionId < Settings.PreConvolutionCount; PreConvolutionId++)
	{
		check(SignalUsesPreConvolution(Settings.SignalProcessing));

		FSSDSignalTextures NewSignalOutput = CreateMultiplexedTextures(
			GraphBuilder,
			ReconstructionTextureCount, ReconstructionDescs,
			GetResourceNames(kPreConvolutionResourceNames));

		FSSDSpatialAccumulationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSSDSpatialAccumulationCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->ConvolutionMetaData = ConvolutionMetaData;
		PassParameters->SignalInput = SignalHistory;
		PassParameters->SignalOutput = CreateMultiplexedUAVs(GraphBuilder, NewSignalOutput);

		PassParameters->DebugOutput = GraphBuilder.CreateUAV(GraphBuilder.CreateTexture(DebugDesc, TEXT("DebugDenoiserPreConvolution")));

		FSSDSpatialAccumulationCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FSignalProcessingDim>(Settings.SignalProcessing);
		PermutationVector.Set<FSignalBatchSizeDim>(Settings.SignalBatchSize);
		PermutationVector.Set<FSSDSpatialAccumulationCS::FStageDim>(FSSDSpatialAccumulationCS::EStage::PreConvolution);
		PermutationVector.Set<FMultiSPPDim>(true);

		TShaderMapRef<FSSDSpatialAccumulationCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SSD PreConvolution(MaxSamples=7)"),
			*ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(DenoiseResolution, FSSDSpatialAccumulationCS::kGroupSize));

		SignalHistory = NewSignalOutput;
	}

	// Temporal pass.
	//
	// Note: always done even if there is no ViewState, because it is already not an idea case for the denoiser quality, therefore not really
	// care about the performance, and the reconstruction may have a different layout than temporal accumulation output.
	if (bHasReconstructionLayoutDifferentFromHistory || Settings.bUseTemporalAccumulation)
	{
		FSSDSignalTextures RejectionPreConvolutionSignal;

		// Temporal rejection might make use of a separable preconvolution.
		if (SignalUsesRejectionPreConvolution(Settings.SignalProcessing))
		{
			{
				int32 RejectionTextureCount = 1;
				TStaticArray<FRDGTextureDesc,kMaxBufferProcessingCount> RejectionSignalProcessingDescs;
				for (int32 i = 0; i < kMaxBufferProcessingCount; i++)
				{
					RejectionSignalProcessingDescs[i] = HistoryDescs[i];
				}

				if (Settings.SignalProcessing == ESignalProcessing::MonochromaticPenumbra)
				{
					for (int32 BatchedSignalId = 0; BatchedSignalId < Settings.SignalBatchSize; BatchedSignalId++)
					{
						RejectionSignalProcessingDescs[BatchedSignalId].Format = PF_FloatRGBA;
					}
					RejectionTextureCount = Settings.SignalBatchSize;
				}
				else if (Settings.SignalProcessing == ESignalProcessing::Reflections)
				{
					RejectionSignalProcessingDescs[0].Format = PF_FloatRGBA;
					RejectionSignalProcessingDescs[1].Format = PF_G16R16F;
					RejectionSignalProcessingDescs[2].Format = PF_FloatRGBA;
					RejectionTextureCount = 3;
				}
				else
				{
					check(0);
				}

				RejectionPreConvolutionSignal = CreateMultiplexedTextures(
					GraphBuilder,
					RejectionTextureCount, RejectionSignalProcessingDescs,
					GetResourceNames(kRejectionPreConvolutionResourceNames));
			}

			FSSDSpatialAccumulationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSSDSpatialAccumulationCS::FParameters>();
			PassParameters->CommonParameters = CommonParameters;
			PassParameters->ConvolutionMetaData = ConvolutionMetaData;
			PassParameters->SignalInput = SignalHistory;
			PassParameters->SignalOutput = CreateMultiplexedUAVs(GraphBuilder, RejectionPreConvolutionSignal);

			FSSDSpatialAccumulationCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FSignalProcessingDim>(Settings.SignalProcessing);
			PermutationVector.Set<FSignalBatchSizeDim>(Settings.SignalBatchSize);
			PermutationVector.Set<FSSDSpatialAccumulationCS::FStageDim>(FSSDSpatialAccumulationCS::EStage::RejectionPreConvolution);
			PermutationVector.Set<FMultiSPPDim>(true);

			PassParameters->DebugOutput = GraphBuilder.CreateUAV(GraphBuilder.CreateTexture(DebugDesc, TEXT("DebugRejectionPreConvolution")));

			TShaderMapRef<FSSDSpatialAccumulationCS> ComputeShader(View.ShaderMap, PermutationVector);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("SSD SpatialAccumulation(RejectionPreConvolution MaxSamples=5)"),
				*ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(DenoiseResolution, FSSDSpatialAccumulationCS::kGroupSize));
		} // if (SignalUsesRejectionPreConvolution(Settings.SignalProcessing))

		FSSDSignalTextures SignalOutput = CreateMultiplexedTextures(
			GraphBuilder,
			HistoryTextureCount, HistoryDescs,
			GetResourceNames(kTemporalAccumulationResourceNames));

		FSSDTemporalAccumulationCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FSignalProcessingDim>(Settings.SignalProcessing);
		PermutationVector.Set<FSignalBatchSizeDim>(Settings.SignalBatchSize);

		TShaderMapRef<FSSDTemporalAccumulationCS> ComputeShader(View.ShaderMap, PermutationVector);

		FSSDTemporalAccumulationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSSDTemporalAccumulationCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->ConvolutionMetaData = ConvolutionMetaData;

		PassParameters->SignalInput = SignalHistory;
		PassParameters->HistoryRejectionSignal = RejectionPreConvolutionSignal;
		PassParameters->SignalHistoryOutput = CreateMultiplexedUAVs(GraphBuilder, SignalOutput);
		
		// Setup common previous frame data.
		PassParameters->PrevScreenToTranslatedWorld = View.PrevViewInfo.ViewMatrices.GetInvTranslatedViewProjectionMatrix();
		PassParameters->PrevDepthBuffer = RegisterExternalTextureWithFallback(GraphBuilder, View.PrevViewInfo.DepthBuffer, GSystemTextures.BlackDummy);
		PassParameters->PrevGBufferA = RegisterExternalTextureWithFallback(GraphBuilder, View.PrevViewInfo.GBufferA, GSystemTextures.BlackDummy);
		PassParameters->PrevGBufferB = RegisterExternalTextureWithFallback(GraphBuilder, View.PrevViewInfo.GBufferB, GSystemTextures.BlackDummy);

		FScreenSpaceFilteringHistory DummyPrevFrameHistory;

		// Setup signals' previous frame historu buffers.
		for (int32 BatchedSignalId = 0; BatchedSignalId < Settings.SignalBatchSize; BatchedSignalId++)
		{
			FScreenSpaceFilteringHistory* PrevFrameHistory = PrevFilteringHistory[BatchedSignalId] ? PrevFilteringHistory[BatchedSignalId] : &DummyPrevFrameHistory;

			PassParameters->bCameraCut[BatchedSignalId] = !PrevFrameHistory->IsValid();

			if (!(View.ViewState && Settings.bUseTemporalAccumulation))
			{
				PassParameters->bCameraCut[BatchedSignalId] = true;
			}

			for (int32 BufferId = 0; BufferId < HistoryTextureCountPerSignal; BufferId++)
			{
				int32 HistoryBufferId = BatchedSignalId * HistoryTextureCountPerSignal + BufferId;
				PassParameters->PrevHistory.Textures[HistoryBufferId] = RegisterExternalTextureWithFallback(
					GraphBuilder, PrevFrameHistory->RT[BufferId], GSystemTextures.BlackDummy);
			}

			// Releases the reference on previous frame so the history's render target can be reused ASAP.
			PrevFrameHistory->SafeRelease();
		} // for (uint32 BatchedSignalId = 0; BatchedSignalId < Settings.SignalBatchSize; BatchedSignalId++)

		PassParameters->DebugOutput = GraphBuilder.CreateUAV(GraphBuilder.CreateTexture(DebugDesc, TEXT("SSDDebugReflectionTemporalAccumulation")));

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SSD TemporalAccumulation"),
			*ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(DenoiseResolution, FComputeShaderUtils::kGolden2DGroupSize));

		SignalHistory = SignalOutput;
	} // if (View.ViewState && Settings.bUseTemporalAccumulation)
	
	// Spatial filter, to converge history faster.
	int32 MaxPostFilterSampleCount = FMath::Clamp(Settings.HistoryConvolutionSampleCount, 1, kStackowiakMaxSampleCountPerSet);
	if (MaxPostFilterSampleCount > 1)
	{
		FSSDSignalTextures SignalOutput = CreateMultiplexedTextures(
			GraphBuilder,
			HistoryTextureCount, HistoryDescs,
			GetResourceNames(kHistoryConvolutionResourceNames));

		FSSDSpatialAccumulationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSSDSpatialAccumulationCS::FParameters>();
		PassParameters->MaxSampleCount = FMath::Clamp(MaxPostFilterSampleCount, 1, kStackowiakMaxSampleCountPerSet);
		PassParameters->KernelSpreadFactor = Settings.HistoryConvolutionKernelSpreadFactor;
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->ConvolutionMetaData = ConvolutionMetaData;
		PassParameters->SignalInput = SignalHistory;
		PassParameters->SignalOutput = CreateMultiplexedUAVs(GraphBuilder, SignalOutput);

		FSSDSpatialAccumulationCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FSignalProcessingDim>(Settings.SignalProcessing);
		PermutationVector.Set<FSignalBatchSizeDim>(Settings.SignalBatchSize);
		PermutationVector.Set<FSSDSpatialAccumulationCS::FStageDim>(FSSDSpatialAccumulationCS::EStage::PostFiltering);
		PermutationVector.Set<FMultiSPPDim>(true);
		
		PassParameters->DebugOutput = GraphBuilder.CreateUAV(GraphBuilder.CreateTexture(DebugDesc, TEXT("SSDDebugReflectionPostfilter")));

		TShaderMapRef<FSSDSpatialAccumulationCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SSD SpatialAccumulation(PostFiltering MaxSamples=%i)", MaxPostFilterSampleCount),
			*ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(DenoiseResolution, FSSDSpatialAccumulationCS::kGroupSize));

		SignalHistory = SignalOutput;
	} // if (MaxPostFilterSampleCount > 1)

	if (!View.bViewStateIsReadOnly)
	{
		check(View.ViewState);

		// Keep depth buffer and GBuffer around for next frame.
		{
			GraphBuilder.QueueTextureExtraction(SceneBlackboard.SceneDepthBuffer, &View.ViewState->PrevFrameViewInfo.DepthBuffer);

			// Requires the normal that are in GBuffer A.
			if (Settings.SignalProcessing == ESignalProcessing::Reflections ||
				Settings.SignalProcessing == ESignalProcessing::AmbientOcclusion ||
				Settings.SignalProcessing == ESignalProcessing::GlobalIllumination)
			{
				GraphBuilder.QueueTextureExtraction(SceneBlackboard.SceneGBufferA, &View.ViewState->PrevFrameViewInfo.GBufferA);
			}

			// Reflections requires the roughness that is in GBuffer B.
			if (Settings.SignalProcessing == ESignalProcessing::Reflections)
			{
				GraphBuilder.QueueTextureExtraction(SceneBlackboard.SceneGBufferB, &View.ViewState->PrevFrameViewInfo.GBufferB);
			}
		}

		// Saves signal histories.
		for (int32 BatchedSignalId = 0; BatchedSignalId < Settings.SignalBatchSize; BatchedSignalId++)
		{
			FScreenSpaceFilteringHistory* NewHistory = NewFilteringHistory[BatchedSignalId];
			check(NewHistory);

			for (int32 BufferId = 0; BufferId < HistoryTextureCountPerSignal; BufferId++)
			{
				int32 HistoryBufferId = BatchedSignalId * HistoryTextureCountPerSignal + BufferId;
				GraphBuilder.QueueTextureExtraction(SignalHistory.Textures[HistoryBufferId], &NewHistory->RT[BufferId]);
			}
		} // for (uint32 BatchedSignalId = 0; BatchedSignalId < Settings.SignalBatchSize; BatchedSignalId++)
	}
	else if (HistoryTextureCountPerSignal >= 2)
	{
		// The SignalHistory1 is always generated for temporal history, but will endup useless if there is no view state,
		// in witch case we do not extract any textures. Don't support a shader permutation that does not produce it, because
		// it is already a not ideal case for the denoiser.
		for (int32 BufferId = 1; BufferId < HistoryTextureCountPerSignal; BufferId++)
		{
			GraphBuilder.RemoveUnusedTextureWarning(SignalHistory.Textures[BufferId]);
		}
	}

	// Final convolution / output to correct
	if (SignalUsesFinalConvolution(Settings.SignalProcessing))
	{
		TStaticArray<FRDGTextureDesc, kMaxBufferProcessingCount> OutputDescs;
		for (int32 i = 0; i < kMaxBufferProcessingCount; i++)
		{
			OutputDescs[i] = HistoryDescs[i];
		}

		if (Settings.SignalProcessing == ESignalProcessing::MonochromaticPenumbra)
		{
			for (int32 BatchedSignalId = 0; BatchedSignalId < Settings.SignalBatchSize; BatchedSignalId++)
			{
				OutputDescs[BatchedSignalId].Format = PF_FloatRGBA;
			}
		}
		else
		{
			check(0);
		}

		*OutputSignal = CreateMultiplexedTextures(
			GraphBuilder,
			Settings.SignalBatchSize, OutputDescs,
			GetResourceNames(kDenoiserOutputResourceNames));

		FSSDSpatialAccumulationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSSDSpatialAccumulationCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->SignalInput = SignalHistory;
		PassParameters->SignalOutput = CreateMultiplexedUAVs(GraphBuilder, *OutputSignal);

		FSSDSpatialAccumulationCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FSignalProcessingDim>(Settings.SignalProcessing);
		PermutationVector.Set<FSignalBatchSizeDim>(Settings.SignalBatchSize);
		PermutationVector.Set<FSSDSpatialAccumulationCS::FStageDim>(FSSDSpatialAccumulationCS::EStage::FinalOutput);
		PermutationVector.Set<FMultiSPPDim>(true);

		TShaderMapRef<FSSDSpatialAccumulationCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SSD SpatialAccumulation(Final)"),
			*ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(DenoiseResolution, FSSDSpatialAccumulationCS::kGroupSize));
	}
	else
	{
		*OutputSignal = SignalHistory;
	}
} // DenoiseSignalAtConstantPixelDensity()


/** The implementation of the default denoiser of the renderer. */
class FDefaultScreenSpaceDenoiser : public IScreenSpaceDenoiser
{
public:
	const TCHAR* GetDebugName() const override
	{
		return TEXT("ScreenSpaceDenoiser");
	}

	virtual EShadowRequirements GetShadowRequirements(
		const FViewInfo& View,
		const FLightSceneInfo& LightSceneInfo,
		const FShadowRayTracingConfig& RayTracingConfig) const override
	{
		if (RayTracingConfig.RayCountPerPixel != 1 || CVarShadowUse1SPPCodePath.GetValueOnRenderThread() == 0)
		{
			check(SignalSupportMultiSPP(ESignalProcessing::MonochromaticPenumbra));
			return IScreenSpaceDenoiser::EShadowRequirements::PenumbraAndClosestOccluder;
		}
		return IScreenSpaceDenoiser::EShadowRequirements::ClosestOccluder;
	}

	virtual void DenoiseShadows(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneViewFamilyBlackboard& SceneBlackboard,
		const TStaticArray<FShadowParameters, IScreenSpaceDenoiser::kMaxBatchSize>& InputParameters,
		const int32 InputParameterCount,
		TStaticArray<FShadowPenumbraOutputs, IScreenSpaceDenoiser::kMaxBatchSize>& Outputs) const override
	{
		FSSDSignalTextures InputSignal;

		FSSDConstantPixelDensitySettings Settings;
		Settings.SignalProcessing = ESignalProcessing::MonochromaticPenumbra;
		Settings.InputResolutionFraction = 1.0f;
		Settings.ReconstructionSamples = CVarShadowReconstructionSampleCount.GetValueOnRenderThread();
		Settings.PreConvolutionCount = CVarShadowPreConvolutionCount.GetValueOnRenderThread();
		Settings.bUseTemporalAccumulation = CVarShadowTemporalAccumulation.GetValueOnRenderThread() != 0;
		Settings.HistoryConvolutionSampleCount = CVarShadowHistoryConvolutionSampleCount.GetValueOnRenderThread();
		Settings.SignalBatchSize = InputParameterCount;

		for (int32 BatchedSignalId = 0; BatchedSignalId < InputParameterCount; BatchedSignalId++)
		{
			Settings.MaxInputSPP = FMath::Max(Settings.MaxInputSPP, InputParameters[BatchedSignalId].RayTracingConfig.RayCountPerPixel);
		}

		TStaticArray<FScreenSpaceFilteringHistory*, IScreenSpaceDenoiser::kMaxBatchSize> PrevHistories;
		TStaticArray<FScreenSpaceFilteringHistory*, IScreenSpaceDenoiser::kMaxBatchSize> NewHistories;
		for (int32 BatchedSignalId = 0; BatchedSignalId < InputParameterCount; BatchedSignalId++)
		{
			const FShadowParameters& Parameters = InputParameters[BatchedSignalId];

			ensure(IsSupportedLightType(ELightComponentType(Parameters.LightSceneInfo->Proxy->GetLightType())));

			Settings.LightSceneInfo[BatchedSignalId] = Parameters.LightSceneInfo;
			if (Settings.MaxInputSPP == 1 && CVarShadowUse1SPPCodePath.GetValueOnRenderThread() != 0)
			{
				// Only have it distance in ClosestOccluder.
				InputSignal.Textures[BatchedSignalId] = Parameters.InputTextures.ClosestOccluder;
			}
			else
			{
				// Get the packed penumbra and hit distance in Penumbra texture.
				InputSignal.Textures[BatchedSignalId] = Parameters.InputTextures.Penumbra;
			}
			PrevHistories[BatchedSignalId] = PreviousViewInfos->ShadowHistories.Find(Settings.LightSceneInfo[BatchedSignalId]->Proxy->GetLightComponent());
			NewHistories[BatchedSignalId] = nullptr;
				
			if (!View.bViewStateIsReadOnly)
			{
				check(View.ViewState);
				NewHistories[BatchedSignalId] = &View.ViewState->PrevFrameViewInfo.ShadowHistories.FindOrAdd(Settings.LightSceneInfo[BatchedSignalId]->Proxy->GetLightComponent());
			}
		}

		FSSDSignalTextures SignalOutput;
		DenoiseSignalAtConstantPixelDensity(
			GraphBuilder, View, SceneBlackboard,
			InputSignal, Settings,
			PrevHistories,
			NewHistories,
			&SignalOutput);

		for (int32 BatchedSignalId = 0; BatchedSignalId < InputParameterCount; BatchedSignalId++)
		{
			Outputs[BatchedSignalId].DiffusePenumbra = SignalOutput.Textures[BatchedSignalId];
			Outputs[BatchedSignalId].SpecularPenumbra = SignalOutput.Textures[BatchedSignalId];
		}
	}

	FReflectionsOutputs DenoiseReflections(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneViewFamilyBlackboard& SceneBlackboard,
		const FReflectionsInputs& ReflectionInputs,
		const FReflectionsRayTracingConfig RayTracingConfig) const override
	{
		FSSDSignalTextures InputSignal;
		InputSignal.Textures[0] = ReflectionInputs.Color;
		InputSignal.Textures[1] = ReflectionInputs.RayHitDistance;

		FSSDConstantPixelDensitySettings Settings;
		Settings.SignalProcessing = ESignalProcessing::Reflections;
		Settings.InputResolutionFraction = RayTracingConfig.ResolutionFraction;
		Settings.ReconstructionSamples = CVarReflectionReconstructionSampleCount.GetValueOnRenderThread();
		Settings.bUseTemporalAccumulation = CVarReflectionTemporalAccumulation.GetValueOnRenderThread() != 0;
		Settings.HistoryConvolutionSampleCount = CVarReflectionHistoryConvolutionSampleCount.GetValueOnRenderThread();

		TStaticArray<FScreenSpaceFilteringHistory*, IScreenSpaceDenoiser::kMaxBatchSize> PrevHistories;
		TStaticArray<FScreenSpaceFilteringHistory*, IScreenSpaceDenoiser::kMaxBatchSize> NewHistories;
		PrevHistories[0] = &PreviousViewInfos->ReflectionsHistory;
		NewHistories[0] = View.ViewState ? &View.ViewState->PrevFrameViewInfo.ReflectionsHistory : nullptr;

		FSSDSignalTextures SignalOutput;
		DenoiseSignalAtConstantPixelDensity(
			GraphBuilder, View, SceneBlackboard,
			InputSignal, Settings,
			PrevHistories,
			NewHistories,
			&SignalOutput);

		FReflectionsOutputs ReflectionsOutput;
		ReflectionsOutput.Color = SignalOutput.Textures[0];
		return ReflectionsOutput;
	}
	
	FAmbientOcclusionOutputs DenoiseAmbientOcclusion(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneViewFamilyBlackboard& SceneBlackboard,
		const FAmbientOcclusionInputs& ReflectionInputs,
		const FAmbientOcclusionRayTracingConfig RayTracingConfig) const override
	{
		FSSDSignalTextures InputSignal;
		InputSignal.Textures[0] = ReflectionInputs.Mask;
		InputSignal.Textures[1] = ReflectionInputs.RayHitDistance;
		
		FSSDConstantPixelDensitySettings Settings;
		Settings.SignalProcessing = ESignalProcessing::AmbientOcclusion;
		Settings.InputResolutionFraction = RayTracingConfig.ResolutionFraction;
		Settings.ReconstructionSamples = CVarAOReconstructionSampleCount.GetValueOnRenderThread();
		Settings.bUseTemporalAccumulation = CVarAOTemporalAccumulation.GetValueOnRenderThread() != 0;
		Settings.HistoryConvolutionSampleCount = CVarAOHistoryConvolutionSampleCount.GetValueOnRenderThread();
		Settings.HistoryConvolutionKernelSpreadFactor = CVarAOHistoryConvolutionKernelSpreadFactor.GetValueOnRenderThread();

		TStaticArray<FScreenSpaceFilteringHistory*, IScreenSpaceDenoiser::kMaxBatchSize> PrevHistories;
		TStaticArray<FScreenSpaceFilteringHistory*, IScreenSpaceDenoiser::kMaxBatchSize> NewHistories;
		PrevHistories[0] = &PreviousViewInfos->AmbientOcclusionHistory;
		NewHistories[0] = View.ViewState ? &View.ViewState->PrevFrameViewInfo.AmbientOcclusionHistory : nullptr;

		FSSDSignalTextures SignalOutput;
		DenoiseSignalAtConstantPixelDensity(
			GraphBuilder, View, SceneBlackboard,
			InputSignal, Settings,
			PrevHistories,
			NewHistories,
			&SignalOutput);

		FAmbientOcclusionOutputs AmbientOcclusionOutput;
		AmbientOcclusionOutput.AmbientOcclusionMask = SignalOutput.Textures[0];
		return AmbientOcclusionOutput;
	}

	FGlobalIlluminationOutputs DenoiseGlobalIllumination(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneViewFamilyBlackboard& SceneBlackboard,
		const FGlobalIlluminationInputs& Inputs,
		const FAmbientOcclusionRayTracingConfig Config) const override
	{
		FSSDSignalTextures InputSignal;
		InputSignal.Textures[0] = Inputs.Color;
		InputSignal.Textures[1] = Inputs.RayHitDistance;

		FSSDConstantPixelDensitySettings Settings;
		Settings.SignalProcessing = ESignalProcessing::GlobalIllumination;
		Settings.InputResolutionFraction = Config.ResolutionFraction;
		Settings.ReconstructionSamples = CVarGIReconstructionSampleCount.GetValueOnRenderThread();
		Settings.bUseTemporalAccumulation = CVarGITemporalAccumulation.GetValueOnRenderThread() != 0;
		Settings.HistoryConvolutionSampleCount = CVarGIHistoryConvolutionSampleCount.GetValueOnRenderThread();
		Settings.HistoryConvolutionKernelSpreadFactor = CVarGIHistoryConvolutionKernelSpreadFactor.GetValueOnRenderThread();

		TStaticArray<FScreenSpaceFilteringHistory*, IScreenSpaceDenoiser::kMaxBatchSize> PrevHistories;
		TStaticArray<FScreenSpaceFilteringHistory*, IScreenSpaceDenoiser::kMaxBatchSize> NewHistories;
		PrevHistories[0] = &PreviousViewInfos->GlobalIlluminationHistory;
		NewHistories[0] = View.ViewState ? &View.ViewState->PrevFrameViewInfo.GlobalIlluminationHistory : nullptr;

		FSSDSignalTextures SignalOutput;
		DenoiseSignalAtConstantPixelDensity(
			GraphBuilder, View, SceneBlackboard,
			InputSignal, Settings,
			PrevHistories,
			NewHistories,
			&SignalOutput);

		FGlobalIlluminationOutputs GlobalIlluminationOutputs;
		GlobalIlluminationOutputs.Color = SignalOutput.Textures[0];
		return GlobalIlluminationOutputs;
	}

	FGlobalIlluminationOutputs DenoiseSkyLight(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneViewFamilyBlackboard& SceneBlackboard,
		const FGlobalIlluminationInputs& Inputs,
		const FAmbientOcclusionRayTracingConfig Config) const override
	{
		FSSDSignalTextures InputSignal;
		InputSignal.Textures[0] = Inputs.Color;
		InputSignal.Textures[1] = Inputs.RayHitDistance;

		FSSDConstantPixelDensitySettings Settings;
		Settings.SignalProcessing = ESignalProcessing::GlobalIllumination;
		Settings.InputResolutionFraction = Config.ResolutionFraction;
		Settings.ReconstructionSamples = CVarGIReconstructionSampleCount.GetValueOnRenderThread();
		Settings.bUseTemporalAccumulation = CVarGITemporalAccumulation.GetValueOnRenderThread() != 0;
		Settings.HistoryConvolutionSampleCount = CVarGIHistoryConvolutionSampleCount.GetValueOnRenderThread();
		Settings.HistoryConvolutionKernelSpreadFactor = CVarGIHistoryConvolutionKernelSpreadFactor.GetValueOnRenderThread();

		TStaticArray<FScreenSpaceFilteringHistory*, IScreenSpaceDenoiser::kMaxBatchSize> PrevHistories;
		TStaticArray<FScreenSpaceFilteringHistory*, IScreenSpaceDenoiser::kMaxBatchSize> NewHistories;
		PrevHistories[0] = &PreviousViewInfos->SkyLightHistory;
		NewHistories[0] = View.ViewState ? &View.ViewState->PrevFrameViewInfo.SkyLightHistory : nullptr;

		FSSDSignalTextures SignalOutput;
		DenoiseSignalAtConstantPixelDensity(
			GraphBuilder, View, SceneBlackboard,
			InputSignal, Settings,
			PrevHistories,
			NewHistories,
			&SignalOutput);

		FGlobalIlluminationOutputs GlobalIlluminationOutputs;
		GlobalIlluminationOutputs.Color = SignalOutput.Textures[0];
		return GlobalIlluminationOutputs;
	}

}; // class FDefaultScreenSpaceDenoiser


// static
const IScreenSpaceDenoiser* IScreenSpaceDenoiser::GetDefaultDenoiser()
{
	static IScreenSpaceDenoiser* GDefaultDenoiser = new FDefaultScreenSpaceDenoiser;
	return GDefaultDenoiser;
}
