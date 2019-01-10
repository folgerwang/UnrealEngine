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


static TAutoConsoleVariable<int32> CVarShadowHaveTAA(
	TEXT("r.Shadow.Denoise.TAA"), 1,
	TEXT(""),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarHistoryRejection(
	TEXT("r.Shadow.Denoise.HistoryRejection"), 1,
	TEXT(""),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDoDebugPass(
	TEXT("r.Shadow.Denoise.DebugPass"), 0,
	TEXT("Adds denoiser's the debug pass."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarReflectionHistoryConvolution(
	TEXT("r.Reflection.Denoise.HistoryConvolution"), 1,
	TEXT("Mode to use for history convolution.\n")
	TEXT(" 0: disabled;\n")
	TEXT(" 1: Spatial convolution (default)."),
	ECVF_RenderThreadSafe);


/** The maximum number of mip level supported in the denoiser. */
static const int32 kMaxMipLevel = 4;


// ---------------------------------------------------- Globals

const IScreenSpaceDenoiser* GScreenSpaceDenoiser = nullptr;


// ---------------------------------------------------- Simple functions

static bool IsSupportedLightType(ELightComponentType LightType)
{
	// TODO.
	return LightType == LightType_Directional || LightType == LightType_Rect;
}


// ---------------------------------------------------- Shaders

namespace
{

/** Different signals to denoise. */
enum class ESignalProcessing
{
	Penumbra,
	Reflections,

	MAX,
};


class FSignalProcessingDim : SHADER_PERMUTATION_ENUM_CLASS("DIM_SIGNAL_PROCESSING", ESignalProcessing);


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
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, TileClassificationTexture)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
END_SHADER_PARAMETER_STRUCT()

/** Shader parameter structure to have all information to spatial filtering. */
BEGIN_SHADER_PARAMETER_STRUCT(FSSDConvolutionMetaData, )
	SHADER_PARAMETER_STRUCT(FLightShaderParameters, Light)
	SHADER_PARAMETER(float, HitDistanceToWorldBluringRadius)
END_SHADER_PARAMETER_STRUCT()


class FSSDReduceCS : public FScreenSpaceDenoisingShader
{
	DECLARE_GLOBAL_SHADER(FSSDReduceCS);
	SHADER_USE_PARAMETER_STRUCT(FSSDReduceCS, FScreenSpaceDenoisingShader);

	class FLightTypeDim : SHADER_PERMUTATION_INT("DIM_LIGHT_TYPE", LightType_MAX);

	using FPermutationDomain = TShaderPermutationDomain<FLightTypeDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSSDCommonParameters, CommonParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSSDConvolutionMetaData, ConvolutionMetaData)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SignalInput0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SignalInput1)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SignalOutput0Mip0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SignalOutput1Mip0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SignalOutput0Mip1)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SignalOutput1Mip1)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SignalOutput0Mip2)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SignalOutput1Mip2)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SignalOutput0Mip3)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SignalOutput1Mip3)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, TileClassificationOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, DebugOutput) // TODO: remove
	END_SHADER_PARAMETER_STRUCT()
};

class FSSDTileDilateCS : public FScreenSpaceDenoisingShader
{
	DECLARE_GLOBAL_SHADER(FSSDTileDilateCS);
	SHADER_USE_PARAMETER_STRUCT(FSSDTileDilateCS, FScreenSpaceDenoisingShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSSDCommonParameters, CommonParameters)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, TileClassificationOutputMip0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, TileClassificationOutputMip1)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, TileClassificationOutputMip2)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, TileClassificationOutputMip3)
	END_SHADER_PARAMETER_STRUCT()
};

class FSSDBuildHistoryRejectionCS : public FScreenSpaceDenoisingShader
{
	DECLARE_GLOBAL_SHADER(FSSDBuildHistoryRejectionCS);
	SHADER_USE_PARAMETER_STRUCT(FSSDBuildHistoryRejectionCS, FScreenSpaceDenoisingShader);

	class FLightTypeDim : SHADER_PERMUTATION_INT("DIM_LIGHT_TYPE", LightType_MAX);

	using FPermutationDomain = TShaderPermutationDomain<FLightTypeDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSSDCommonParameters, CommonParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSSDConvolutionMetaData, ConvolutionMetaData)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RawSignalInput0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RawSignalInput1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ReducedSignal0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ReducedSignal1)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SignalOutput0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SignalOutput1)
	END_SHADER_PARAMETER_STRUCT()
};

class FSSDSpatialAccumulationCS : public FScreenSpaceDenoisingShader
{
	DECLARE_GLOBAL_SHADER(FSSDSpatialAccumulationCS);
	SHADER_USE_PARAMETER_STRUCT(FSSDSpatialAccumulationCS, FScreenSpaceDenoisingShader);
	
	using FPermutationDomain = TShaderPermutationDomain<FSignalProcessingDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSSDCommonParameters, CommonParameters)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SignalInput0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SignalInput1)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SignalOutput0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SignalOutput1)
	END_SHADER_PARAMETER_STRUCT()
};

class FSSDTemporalAccumulationCS : public FScreenSpaceDenoisingShader
{
	DECLARE_GLOBAL_SHADER(FSSDTemporalAccumulationCS);
	SHADER_USE_PARAMETER_STRUCT(FSSDTemporalAccumulationCS, FScreenSpaceDenoisingShader);

	class FIsMip0Dim : SHADER_PERMUTATION_BOOL("DIM_IS_MIP_0");

	using FPermutationDomain = TShaderPermutationDomain<
		FIsMip0Dim,
		FSignalProcessingDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// Reflections are only processed at mip 0.
		if (PermutationVector.Get<FSignalProcessingDim>() == ESignalProcessing::Reflections && 
			!PermutationVector.Get<FIsMip0Dim>())
		{
			return false;
		}

		return FScreenSpaceDenoisingShader::ShouldCompilePermutation(Parameters);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, iMipLevel)
		SHADER_PARAMETER(uint32, iMipLevelPow2)
		SHADER_PARAMETER(float, MipLevel)
		SHADER_PARAMETER(float, MipLevelPow2)
		SHADER_PARAMETER(float, InvMipLevelPow2)
		SHADER_PARAMETER(int32, bCameraCut)
		SHADER_PARAMETER(FMatrix, PrevScreenToTranslatedWorld)

		SHADER_PARAMETER_STRUCT_INCLUDE(FSSDCommonParameters, CommonParameters)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SignalInput0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SignalInput1)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevHistory0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevHistory1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevTileClassificationTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevDepthBuffer)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevGBufferA)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevGBufferB)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HistoryRejectionSignal0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HistoryRejectionSignal1)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SignalHistoryOutput0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SignalHistoryOutput1)
	END_SHADER_PARAMETER_STRUCT()
};

class FSSDOutputCS : public FScreenSpaceDenoisingShader
{
	DECLARE_GLOBAL_SHADER(FSSDOutputCS);
	SHADER_USE_PARAMETER_STRUCT(FSSDOutputCS, FScreenSpaceDenoisingShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSSDCommonParameters, CommonParameters)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SignalInput0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SignalInput1)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SignalOutput0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SignalOutput1)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FSSDReduceCS, "/Engine/Private/ScreenSpaceDenoise/SSDReduce.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FSSDTileDilateCS, "/Engine/Private/ScreenSpaceDenoise/SSDTileDilate.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FSSDBuildHistoryRejectionCS, "/Engine/Private/ScreenSpaceDenoise/SSDBuildHistoryRejection.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FSSDSpatialAccumulationCS, "/Engine/Private/ScreenSpaceDenoise/SSDSpatialAccumulation.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FSSDTemporalAccumulationCS, "/Engine/Private/ScreenSpaceDenoise/SSDTemporalAccumulation.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FSSDOutputCS, "/Engine/Private/ScreenSpaceDenoise/SSDOutput.usf", "MainCS", SF_Compute);


#define SSD_DEBUG_PASS 1
#if SSD_DEBUG_PASS

class FSSDDebugCS : public FScreenSpaceDenoisingShader
{
	DECLARE_GLOBAL_SHADER(FSSDDebugCS);
	SHADER_USE_PARAMETER_STRUCT(FSSDDebugCS, FScreenSpaceDenoisingShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSSDCommonParameters, CommonParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SignalInput0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SignalInput1)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, DebugOutput0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, DebugOutput1)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FSSDDebugCS, "/Engine/Private/ScreenSpaceDenoise/SSDDebug.usf", "MainCS", SF_Compute);

#endif

} // namespace


static void DenoiseShadowPenumbra(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FLightSceneInfo& LightSceneInfo,
	const FSceneViewFamilyBlackboard& SceneBlackboard,
	FRDGTextureRef PenumbraRT,
	FRDGTextureRef ClosestOccluder,
	FRDGTextureRef* OutPenumbraMask)
{
	const FIntPoint DenoiseResolution = View.ViewRect.Size();

	FLightSceneProxy* LightSceneProxy = LightSceneInfo.Proxy;

	// Descriptor to allocate internal denoising buffer.
	FRDGTextureDesc SignalProcessingDesc = PenumbraRT->Desc;
	SignalProcessingDesc.Format = PF_FloatRGBA;
	SignalProcessingDesc.NumMips = kMaxMipLevel;

	// Setup common shader parameters.
	FSSDCommonParameters CommonParameters;
	{
		CommonParameters.SceneBlackboard = SceneBlackboard;
		CommonParameters.ViewUniformBuffer = View.ViewUniformBuffer;

		// Create the tile classification buffer.
		// TODO: should use a buffer instead of a texture to have scalar.
		{
			FRDGTextureDesc Desc = PenumbraRT->Desc;
			Desc.Format = PF_R16_UINT;
			Desc.Extent = FIntPoint::DivideAndRoundUp(Desc.Extent, FComputeShaderUtils::kGolden2DGroupSize);
			CommonParameters.TileClassificationTexture = GraphBuilder.CreateTexture(Desc, TEXT("SSDFlattenedTiles"));
		}
	}

	// Setup all the metadata to do spatial convolution.
	FSSDConvolutionMetaData ConvolutionMetaData;
	{
		FLightShaderParameters LightParameters;
		LightSceneProxy->GetLightShaderParameters(ConvolutionMetaData.Light);

		if (LightSceneProxy->GetLightType() == LightType_Directional)
		{
			ConvolutionMetaData.HitDistanceToWorldBluringRadius = FMath::Tan(0.5 * FMath::DegreesToRadians(LightSceneProxy->GetLightSourceAngle()));
		}
	}

	// Do initial reduction of the raw signal.
	FRDGTextureRef ReducedSignal0;
	FRDGTextureRef ReducedSignal1;
	{
		ReducedSignal0 = GraphBuilder.CreateTexture(SignalProcessingDesc, TEXT("SSDReducedSignal0"));
		ReducedSignal1 = GraphBuilder.CreateTexture(SignalProcessingDesc, TEXT("SSDReducedSignal1"));

		FSSDReduceCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FSSDReduceCS::FLightTypeDim>(LightSceneProxy->GetLightType());

		FSSDReduceCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSSDReduceCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->ConvolutionMetaData = ConvolutionMetaData;
		PassParameters->SignalInput0 = PenumbraRT;
		PassParameters->SignalInput1 = ClosestOccluder;
		PassParameters->SignalOutput0Mip0 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReducedSignal0, /* MipLevel = */ 0));
		PassParameters->SignalOutput0Mip1 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReducedSignal0, /* MipLevel = */ 1));
		PassParameters->SignalOutput0Mip2 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReducedSignal0, /* MipLevel = */ 2));
		PassParameters->SignalOutput0Mip3 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ReducedSignal0, /* MipLevel = */ 3));

		{
			FRDGTextureRef TileClassificationTexture = CommonParameters.TileClassificationTexture;
			PassParameters->TileClassificationOutput = GraphBuilder.CreateUAV(TileClassificationTexture);
		}

		PassParameters->DebugOutput = GraphBuilder.CreateUAV(GraphBuilder.CreateTexture(PenumbraRT->Desc, TEXT("SSDDebugReduce")));

		TShaderMapRef<FSSDReduceCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SSD Reduce"),
			*ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(DenoiseResolution, FComputeShaderUtils::kGolden2DGroupSize));
	}

	// Dilation of the tile classification.
	{
		FRDGTextureDesc Desc = CommonParameters.TileClassificationTexture->Desc;
		Desc.NumMips = kMaxMipLevel;

		FRDGTextureRef TileClassificationChain = GraphBuilder.CreateTexture(Desc, TEXT("SSDDilatedTiles"));

		FSSDTileDilateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSSDTileDilateCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->TileClassificationOutputMip0 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(TileClassificationChain, /* MipLevel = */ 0));
		PassParameters->TileClassificationOutputMip1 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(TileClassificationChain, /* MipLevel = */ 1));
		PassParameters->TileClassificationOutputMip2 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(TileClassificationChain, /* MipLevel = */ 2));
		PassParameters->TileClassificationOutputMip3 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(TileClassificationChain, /* MipLevel = */ 3));

		TShaderMapRef<FSSDTileDilateCS> ComputeShader(View.ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SSD TileDilate"),
			*ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(DenoiseResolution, FComputeShaderUtils::kGolden2DGroupSize * FComputeShaderUtils::kGolden2DGroupSize));

		CommonParameters.TileClassificationTexture = TileClassificationChain;
	}

	/** Adds a spatial accumulation pass. */
	auto AddSpatialAccumulationPass = [&](FRDGTextureRef SignalInput0, FRDGTextureRef SignalInput1, FRDGTextureRef SignalOutput0, FRDGTextureRef SignalOutput1)
	{
		FSSDSpatialAccumulationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSSDSpatialAccumulationCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->SignalInput0 = SignalInput0;
		PassParameters->SignalInput1 = SignalInput1;
		PassParameters->SignalOutput0 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SignalOutput0, /* MipLevel = */ 0));
		PassParameters->SignalOutput1 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SignalOutput1, /* MipLevel = */ 0));

		FSSDSpatialAccumulationCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FSignalProcessingDim>(ESignalProcessing::Penumbra);

		TShaderMapRef<FSSDSpatialAccumulationCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SSD SpatialAccumulation(Mip=0)"),
			*ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(DenoiseResolution, FComputeShaderUtils::kGolden2DGroupSize));
	};

	// Doing mip level.
	if (View.ViewState && CVarShadowHaveTAA.GetValueOnRenderThread())
	{
		// Generate rejection signal history.
		FRDGTextureRef HistoryRejectionSignal0;
		FRDGTextureRef HistoryRejectionSignal1;
		if (CVarHistoryRejection.GetValueOnRenderThread() == 1)
		{
			FRDGTextureDesc Desc = SignalProcessingDesc;
			Desc.NumMips = 1;
			// Desc.Format = PF_R8G8;

			HistoryRejectionSignal0 = GraphBuilder.CreateTexture(SignalProcessingDesc, TEXT("SSDHistoryRejection0"));
			HistoryRejectionSignal1 = GraphBuilder.CreateTexture(SignalProcessingDesc, TEXT("SSDHistoryRejection1"));

			FSSDBuildHistoryRejectionCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FSSDBuildHistoryRejectionCS::FLightTypeDim>(LightSceneProxy->GetLightType());

			FSSDBuildHistoryRejectionCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSSDBuildHistoryRejectionCS::FParameters>();
			PassParameters->CommonParameters = CommonParameters;
			PassParameters->ConvolutionMetaData = ConvolutionMetaData;
			PassParameters->RawSignalInput0 = PenumbraRT;
			PassParameters->RawSignalInput1 = ClosestOccluder;
			PassParameters->ReducedSignal0 = ReducedSignal0;
			PassParameters->ReducedSignal1 = ReducedSignal1;
			PassParameters->SignalOutput0 = GraphBuilder.CreateUAV(HistoryRejectionSignal0);
			PassParameters->SignalOutput1 = GraphBuilder.CreateUAV(HistoryRejectionSignal1);

			TShaderMapRef<FSSDBuildHistoryRejectionCS> ComputeShader(View.ShaderMap, PermutationVector);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("SSD BuildHistoryRejection"),
				*ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(DenoiseResolution, FComputeShaderUtils::kGolden2DGroupSize));
		}
		else
		{
			FRDGTextureDesc Desc = SignalProcessingDesc;
			Desc.NumMips = 1;
			// Desc.Format = PF_R8G8;

			HistoryRejectionSignal0 = GraphBuilder.CreateTexture(SignalProcessingDesc, TEXT("SSDHistoryRejection0"));
			HistoryRejectionSignal1 = GraphBuilder.CreateTexture(SignalProcessingDesc, TEXT("SSDHistoryRejection1"));

			// TODO: tile classification.
			AddSpatialAccumulationPass(ReducedSignal0, ReducedSignal1, HistoryRejectionSignal0, HistoryRejectionSignal1);
		}

		FRDGTextureRef SignalHistory0 = GraphBuilder.CreateTexture(SignalProcessingDesc, TEXT("SSDSignalHistory0"));
		FRDGTextureRef SignalHistory1 = GraphBuilder.CreateTexture(SignalProcessingDesc, TEXT("SSDSignalHistory1"));

		FScreenSpaceFilteringHistory PrevFrameHistory;
		if (!View.bCameraCut)
		{
			PrevFrameHistory = View.PrevViewInfo.ShadowHistories.FindRef(&LightSceneInfo);
		}

		// Process each mip level independently.
		for (uint32 MipLevel = 0; MipLevel < kMaxMipLevel; MipLevel++)
		{
			FSSDTemporalAccumulationCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FSSDTemporalAccumulationCS::FIsMip0Dim>(MipLevel == 0);
			PermutationVector.Set<FSignalProcessingDim>(ESignalProcessing::Penumbra);

			TShaderMapRef<FSSDTemporalAccumulationCS> ComputeShader(View.ShaderMap, PermutationVector);

			FSSDTemporalAccumulationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSSDTemporalAccumulationCS::FParameters>();
			PassParameters->CommonParameters = CommonParameters;
			PassParameters->iMipLevel = MipLevel;
			PassParameters->iMipLevelPow2 = (1 << MipLevel);
			PassParameters->MipLevel = MipLevel;
			PassParameters->MipLevelPow2 = (1 << MipLevel);
			PassParameters->InvMipLevelPow2 = 1.0f / PassParameters->MipLevelPow2;
			PassParameters->bCameraCut = View.bCameraCut || !PrevFrameHistory.RT[0].IsValid();
			PassParameters->SignalInput0 = ReducedSignal0;
			PassParameters->SignalInput1 = ReducedSignal1;
			PassParameters->HistoryRejectionSignal0 = HistoryRejectionSignal0;
			PassParameters->HistoryRejectionSignal1 = HistoryRejectionSignal1;

			PassParameters->PrevScreenToTranslatedWorld = View.PrevViewInfo.ViewMatrices.GetInvTranslatedViewProjectionMatrix();
			PassParameters->PrevHistory0 = RegisterExternalTextureWithFallback(GraphBuilder, PrevFrameHistory.RT[0], GSystemTextures.BlackDummy);
			PassParameters->PrevHistory1 = RegisterExternalTextureWithFallback(GraphBuilder, PrevFrameHistory.RT[1], GSystemTextures.BlackDummy);
			PassParameters->PrevTileClassificationTexture = RegisterExternalTextureWithFallback(GraphBuilder, PrevFrameHistory.TileClassification, GSystemTextures.BlackDummy);
			PassParameters->PrevDepthBuffer = RegisterExternalTextureWithFallback(GraphBuilder, View.PrevViewInfo.DepthBuffer, GSystemTextures.BlackDummy);
			PassParameters->PrevGBufferA = RegisterExternalTextureWithFallback(GraphBuilder, View.PrevViewInfo.GBufferA, GSystemTextures.BlackDummy);

			PassParameters->SignalHistoryOutput0 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SignalHistory0, MipLevel));
			PassParameters->SignalHistoryOutput1 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SignalHistory1, MipLevel));

			FIntPoint Resolution = FIntPoint::DivideAndRoundUp(DenoiseResolution, 1 << MipLevel);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("SSD TemporalAccumulation(Mip=%d)", MipLevel),
				*ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(Resolution, FComputeShaderUtils::kGolden2DGroupSize));
		}

		// Keep depth buffer and GBufferA around for next frame.
		{
			GraphBuilder.QueueTextureExtraction(SceneBlackboard.SceneDepthBuffer, &View.ViewState->PendingPrevFrameViewInfo.DepthBuffer);
			GraphBuilder.QueueTextureExtraction(SceneBlackboard.SceneGBufferA, &View.ViewState->PendingPrevFrameViewInfo.GBufferA);
		}

		// Saves shadow history.
		{
			FScreenSpaceFilteringHistory& NewShadowHistory = View.ViewState->PendingPrevFrameViewInfo.ShadowHistories.FindOrAdd(&LightSceneInfo);
			GraphBuilder.QueueTextureExtraction(SignalHistory0, &NewShadowHistory.RT[0]);

			GraphBuilder.QueueTextureExtraction(CommonParameters.TileClassificationTexture, &NewShadowHistory.TileClassification);
		}

		ReducedSignal0 = SignalHistory0;
		ReducedSignal1 = SignalHistory1;
	}

	// Output denoised signal.
	FRDGTextureRef OutputSignal = GraphBuilder.CreateTexture(PenumbraRT->Desc, TEXT("SSDOutputSignal"));
	{
		FSSDOutputCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSSDOutputCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->SignalInput0 = ReducedSignal0;
		PassParameters->SignalInput1 = ReducedSignal1;
		PassParameters->SignalOutput0 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutputSignal, /* MipLevel = */ 0));

		TShaderMapRef<FSSDOutputCS> ComputeShader(View.ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SSD Output"),
			*ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(DenoiseResolution, FComputeShaderUtils::kGolden2DGroupSize));
	}

	#if SSD_DEBUG_PASS
	if (CVarDoDebugPass.GetValueOnRenderThread())
	{
		FSSDDebugCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSSDDebugCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->SignalInput0 = ReducedSignal0;
		PassParameters->SignalInput1 = ReducedSignal1;
		PassParameters->DebugOutput0 = GraphBuilder.CreateUAV(GraphBuilder.CreateTexture(PenumbraRT->Desc, TEXT("SSDDebug0")));
		PassParameters->DebugOutput1 = GraphBuilder.CreateUAV(GraphBuilder.CreateTexture(PenumbraRT->Desc, TEXT("SSDDebug1")));

		TShaderMapRef<FSSDDebugCS> ComputeShader(View.ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SSD Debug"),
			*ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(DenoiseResolution, FComputeShaderUtils::kGolden2DGroupSize));
	}
	#endif // SSD_DEBUG_PASS

	*OutPenumbraMask = OutputSignal;
} // DenoiseShadowPenumbra()


IScreenSpaceDenoiser::FReflectionOutputs DenoiseReflections(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FSceneViewFamilyBlackboard& SceneBlackboard,
	const IScreenSpaceDenoiser::FReflectionInputs& ReflectionInputs)
{
	const FIntPoint DenoiseResolution = View.ViewRect.Size();

	// Descriptor to allocate internal denoising buffer.
	FRDGTextureDesc SignalProcessingDesc = ReflectionInputs.Color->Desc;
	SignalProcessingDesc.Format = PF_FloatRGBA;
	SignalProcessingDesc.NumMips = 1;

	// Setup common shader parameters.
	FSSDCommonParameters CommonParameters;
	{
		CommonParameters.SceneBlackboard = SceneBlackboard;
		CommonParameters.ViewUniformBuffer = View.ViewUniformBuffer;
	}

	FScreenSpaceFilteringHistory PrevFrameHistory;
	if (!View.bCameraCut)
	{
		PrevFrameHistory = View.PrevViewInfo.ReflectionsHistory;
	}

	FRDGTextureRef SignalHistory0 = GraphBuilder.CreateTexture(SignalProcessingDesc, TEXT("SSDReflectionsHistory0"));
	FRDGTextureRef SignalHistory1 = GraphBuilder.CreateTexture(SignalProcessingDesc, TEXT("SSDReflectionsHistory1"));

	// Temporal pass.
	{
		const int32 MipLevel = 0;

		FSSDTemporalAccumulationCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FSSDTemporalAccumulationCS::FIsMip0Dim>(MipLevel == 0);
		PermutationVector.Set<FSignalProcessingDim>(ESignalProcessing::Reflections);

		TShaderMapRef<FSSDTemporalAccumulationCS> ComputeShader(View.ShaderMap, PermutationVector);

		FSSDTemporalAccumulationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSSDTemporalAccumulationCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->bCameraCut = View.bCameraCut || !PrevFrameHistory.RT[0].IsValid();
		PassParameters->SignalInput0 = ReflectionInputs.Color;
		PassParameters->SignalInput1 = ReflectionInputs.RayHitDistance;

		PassParameters->PrevScreenToTranslatedWorld = View.PrevViewInfo.ViewMatrices.GetInvTranslatedViewProjectionMatrix();
		PassParameters->PrevHistory0 = RegisterExternalTextureWithFallback(GraphBuilder, PrevFrameHistory.RT[0], GSystemTextures.BlackDummy);
		PassParameters->PrevHistory1 = RegisterExternalTextureWithFallback(GraphBuilder, PrevFrameHistory.RT[1], GSystemTextures.BlackDummy);
		PassParameters->PrevTileClassificationTexture = RegisterExternalTextureWithFallback(GraphBuilder, PrevFrameHistory.TileClassification, GSystemTextures.BlackDummy);
		PassParameters->PrevDepthBuffer = RegisterExternalTextureWithFallback(GraphBuilder, View.PrevViewInfo.DepthBuffer, GSystemTextures.BlackDummy);
		PassParameters->PrevGBufferA = RegisterExternalTextureWithFallback(GraphBuilder, View.PrevViewInfo.GBufferA, GSystemTextures.BlackDummy);
		PassParameters->PrevGBufferB = RegisterExternalTextureWithFallback(GraphBuilder, View.PrevViewInfo.GBufferB, GSystemTextures.BlackDummy);

		PassParameters->SignalHistoryOutput0 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SignalHistory0, MipLevel));
		PassParameters->SignalHistoryOutput1 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SignalHistory1, MipLevel));

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SSD TemporalAccumulation"),
			*ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(DenoiseResolution, FComputeShaderUtils::kGolden2DGroupSize));
	}
	
	// Spatial filter, to converge history faster.
	if (CVarReflectionHistoryConvolution.GetValueOnRenderThread())
	{
		FRDGTextureRef SignalOutput0 = GraphBuilder.CreateTexture(SignalProcessingDesc, TEXT("SSDReflectionsHistory0"));
		FRDGTextureRef SignalOutput1 = GraphBuilder.CreateTexture(SignalProcessingDesc, TEXT("SSDReflectionsHistory1"));

		FSSDSpatialAccumulationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSSDSpatialAccumulationCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->SignalInput0 = SignalHistory0;
		PassParameters->SignalInput1 = SignalHistory1;
		PassParameters->SignalOutput0 = GraphBuilder.CreateUAV(SignalOutput0);
		PassParameters->SignalOutput1 = GraphBuilder.CreateUAV(SignalOutput1);

		FSSDSpatialAccumulationCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FSignalProcessingDim>(ESignalProcessing::Reflections);

		TShaderMapRef<FSSDSpatialAccumulationCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SSD SpatialAccumulation"),
			*ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(DenoiseResolution, FComputeShaderUtils::kGolden2DGroupSize));

		SignalHistory0 = SignalOutput0;
		SignalHistory1 = SignalOutput1;
	}

	if (View.ViewState)
	{
		// Keep depth buffer and GBuffer around for next frame.
		{
			GraphBuilder.QueueTextureExtraction(SceneBlackboard.SceneDepthBuffer, &View.ViewState->PendingPrevFrameViewInfo.DepthBuffer);
			GraphBuilder.QueueTextureExtraction(SceneBlackboard.SceneGBufferA, &View.ViewState->PendingPrevFrameViewInfo.GBufferA);
			GraphBuilder.QueueTextureExtraction(SceneBlackboard.SceneGBufferB, &View.ViewState->PendingPrevFrameViewInfo.GBufferB);
		}

		// Saves history.
		{
			FScreenSpaceFilteringHistory& NewHistory = View.ViewState->PendingPrevFrameViewInfo.ReflectionsHistory;
			GraphBuilder.QueueTextureExtraction(SignalHistory0, &NewHistory.RT[0]);
			GraphBuilder.QueueTextureExtraction(SignalHistory1, &NewHistory.RT[1]);
		}
	}

	IScreenSpaceDenoiser::FReflectionOutputs Outputs;
	Outputs.Color = SignalHistory0; // TODO.
	return Outputs;
} // DenoiseReflections()


/** The implementation of the default denoiser of the renderer. */
class FDefaultScreenSpaceDenoiser : public IScreenSpaceDenoiser
{
public:
	const TCHAR* GetDebugName() const override
	{
		return TEXT("SSD");
	}

	FShadowRayTracingConfig GetShadowRayTracingConfig(
		const FViewInfo& View,
		const FLightSceneInfo& LightSceneInfo) const override
	{
		FShadowRayTracingConfig Config;

		if (IsSupportedLightType(ELightComponentType(LightSceneInfo.Proxy->GetLightType())))
		{
			// TODO: actually need only the ray hit distance.
			Config.Requirements = IScreenSpaceDenoiser::EShadowRayTracingOutputs::PenumbraAndClosestOccluder;
		}
		else
		{
			Config.Requirements = IScreenSpaceDenoiser::EShadowRayTracingOutputs::ClosestOccluder;
		}

		return Config;
	}

	FShadowPenumbraOutputs DenoiseShadowPenumbra(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		const FLightSceneInfo& LightSceneInfo,
		const FSceneViewFamilyBlackboard& SceneBlackboard,
		const FShadowPenumbraInputs& ShadowInputs) const override
	{
		FShadowPenumbraOutputs Outputs;

		//temp workaround for compilation errors with android compiler
#if RHI_RAYTRACING
		if (IsSupportedLightType(ELightComponentType(LightSceneInfo.Proxy->GetLightType())))
		{
			::DenoiseShadowPenumbra(
				GraphBuilder,
				View, LightSceneInfo,
				SceneBlackboard,
				ShadowInputs.Penumbra,
				ShadowInputs.ClosestOccluder,
				&Outputs.DiffusePenumbra);
		}
		else
#endif
		{
			Outputs.DiffusePenumbra = ShadowInputs.Penumbra;
		}

		// TODO: this is not right, need to take the directionality into account.
		Outputs.SpecularPenumbra = Outputs.DiffusePenumbra;

		return Outputs;
	}

	FReflectionOutputs DenoiseReflections(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		const FSceneViewFamilyBlackboard& SceneBlackboard,
		const FReflectionInputs& ReflectionInputs) const override
	{
		return ::DenoiseReflections(GraphBuilder, View, SceneBlackboard, ReflectionInputs);
	}

}; // class FDefaultScreenSpaceDenoiser


// static
const IScreenSpaceDenoiser* IScreenSpaceDenoiser::GetDefaultDenoiser()
{
	static IScreenSpaceDenoiser* GDefaultDenoiser = new FDefaultScreenSpaceDenoiser;
	return GDefaultDenoiser;
}
