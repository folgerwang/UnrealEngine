// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DiaphragmDOFPasses.cpp: Implementations of all diaphragm DOF's passes.
=============================================================================*/

#include "PostProcess/DiaphragmDOFPasses.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessBokehDOF.h"
#include "SceneRenderTargetParameters.h"
#include "PostProcess/PostProcessing.h"
#include "DeferredShadingRenderer.h"
#include "PipelineStateCache.h"
#include "ScenePrivate.h"
#include "ClearQuad.h"
#include "SpriteIndexBuffer.h"


// ---------------------------------------------------- Cvars

namespace
{

#if !UE_BUILD_SHIPPING

TAutoConsoleVariable<int32> CVarDebugScatterPerf(
	TEXT("r.DOF.Debug.ScatterPerf"),
	0,
	TEXT(""),
	ECVF_RenderThreadSafe);

#endif

TAutoConsoleVariable<float> CVarScatterNeighborCompareMaxColor(
	TEXT("r.DOF.Scatter.NeighborCompareMaxColor"),
	10,
	TEXT("Controles the linear color clamping upperbound applied before color of pixel and neighbors are compared.")
	TEXT(" To low, and you may not scatter enough; to high you may scatter unnecessarily too much in highlights")
	TEXT(" (Default: 10)."),
	ECVF_RenderThreadSafe);

} // namespace


// ---------------------------------------------------- COMMON

namespace
{

const int32 kDefaultGroupSize = 8;
const int32 kCocTileSize = kDefaultGroupSize;

FIntPoint CocTileGridSize(FIntPoint FullResSize)
{
	uint32 TilesX = FMath::DivideAndRoundUp(FullResSize.X, kCocTileSize);
	uint32 TilesY = FMath::DivideAndRoundUp(FullResSize.Y, kCocTileSize);
	return FIntPoint(TilesX, TilesY);
}


/** Returns the lower res's viewport from a given view size. */
FIntRect GetLowerResViewport(const FIntRect& ViewRect, int32 ResDivisor)
{
	check(ResDivisor >= 1);
	check(FMath::IsPowerOfTwo(ResDivisor));

	FIntRect DestViewport;

	// All diaphragm DOF's lower res viewports are top left cornered to only do a min(SampleUV, MaxUV) when doing convolution.
	DestViewport.Min = FIntPoint::ZeroValue;

	DestViewport.Max.X = FMath::DivideAndRoundUp(ViewRect.Width(), ResDivisor);
	DestViewport.Max.Y = FMath::DivideAndRoundUp(ViewRect.Height(), ResDivisor);
	return DestViewport;
}


const TCHAR* GetEventName(EDiaphragmDOFLayerProcessing e)
{
	static const TCHAR* const kArray[] = {
		TEXT("FgdOnly"),
		TEXT("FgdFill"),
		TEXT("BgdOnly"),
		TEXT("Fgd&Bgd"),
		TEXT("FocusOnly"),
	};
	int32 i = int32(e);
	check(i < ARRAY_COUNT(kArray));
	return kArray[i];
}

const TCHAR* GetEventName(EDiaphragmDOFPostfilterMethod e)
{
	static const TCHAR* const kArray[] = {
		TEXT("Median3x3"),
		TEXT("Max3x3"),
	};
	int32 i = int32(e) - 1;
	check(i < ARRAY_COUNT(kArray));
	return kArray[i];
}

const TCHAR* GetEventName(EDiaphragmDOFBokehSimulation e)
{
	static const TCHAR* const kArray[] = {
		TEXT("None"),
		TEXT("Symmetric"),
		TEXT("Generic"),
	};
	int32 i = int32(e);
	check(i < ARRAY_COUNT(kArray));
	return kArray[i];
}

const TCHAR* GetEventName(FRCPassDiaphragmDOFBuildBokehLUT::EFormat e)
{
	static const TCHAR* const kArray[] = {
		TEXT("Scatter"),
		TEXT("Recombine"),
		TEXT("Gather"),
	};
	int32 i = int32(e);
	check(i < ARRAY_COUNT(kArray));
	return kArray[i];
}

const TCHAR* GetEventName(FRCPassDiaphragmDOFGather::EQualityConfig e)
{
	static const TCHAR* const kArray[] = {
		TEXT("LowQ"),
		TEXT("HighQ"),
		TEXT("ScatterOcclusion"),
		TEXT("Cinematic"),
	};
	int32 i = int32(e);
	check(i < ARRAY_COUNT(kArray));
	return kArray[i];
}

const TCHAR* GetEventName(FRCPassDiaphragmDOFDilateCoc::EMode e)
{
	static const TCHAR* const kArray[] = {
		TEXT("StandAlone"),
		TEXT("MinMax"),
		TEXT("MinAbs"),
	};
	int32 i = int32(e);
	check(i < ARRAY_COUNT(kArray));
	return kArray[i];
}

// Returns X and Y for F(M) = saturate(M * X + Y) so that F(LowM) = 0 and F(HighM) = 1
FVector2D GenerateSaturatedAffineTransformation(float LowM, float HighM)
{
	float X = 1.0f / (HighM - LowM);
	return FVector2D(X, -X * LowM);
}

// Affine transformtations that always return 0 or 1.
const FVector2D kContantlyPassingAffineTransformation(0, 1);
const FVector2D kContantlyBlockingAffineTransformation(0, 0);

}


// ---------------------------------------------------- BOILER PLATE HELPERS

// Experimental shader parameter API.
namespace
{

template<typename ShaderType, typename CompiledShaderInitializerType>
void AutomaticShaderBinding(ShaderType& Instance, const CompiledShaderInitializerType& Initializer, const TCHAR* MemberNameStr)
{
	Instance.Bind(Initializer.ParameterMap);
}

template<typename CompiledShaderInitializerType>
void AutomaticShaderBinding(
	FShaderParameter& Instance, const CompiledShaderInitializerType& Initializer, const TCHAR* MemberNameStr)
{
	Instance.Bind(Initializer.ParameterMap, MemberNameStr);
}

template<typename CompiledShaderInitializerType>
void AutomaticShaderBinding(
	FShaderResourceParameter& Instance, const CompiledShaderInitializerType& Initializer, const TCHAR* MemberNameStr)
{
	Instance.Bind(Initializer.ParameterMap, MemberNameStr);
}

template<typename CompiledShaderInitializerType>
void AutomaticShaderBinding(
	FSceneTextureShaderParameters& Instance, const CompiledShaderInitializerType& Initializer, const TCHAR* MemberNameStr)
{
	Instance.Bind(Initializer);
}

#define POPULATE_SHADER_BINDING_IMPL(MemberType,MemberName) \
	AutomaticShaderBinding(MemberName, Initializer, TEXT(#MemberName));

#define POPULATE_SHADER_MEMBERS_IMPL(MemberType,MemberName) \
	MemberType MemberName;

#define POPULATE_SHADER_SERIALIZATION_IMPL(MemberType,MemberName) \
	Ar << MemberName;


// Populate shader type's parameter members, binding and serialization.
#define SHADER_TYPE_PARAMETERS(ShaderTypeName,ParentShaderTypeName,Parameters) \
	ShaderTypeName() {} \
	ShaderTypeName(const ShaderMetaType::CompiledShaderInitializerType& Initializer) \
		: ParentShaderTypeName(Initializer) \
	{ \
		Parameters(POPULATE_SHADER_BINDING_IMPL) \
	} \
	\
	virtual bool Serialize(FArchive& Ar) override \
	{ \
		bool bShaderHasOutdatedParameters = ParentShaderTypeName::Serialize(Ar); \
		Parameters(POPULATE_SHADER_SERIALIZATION_IMPL) \
		return bShaderHasOutdatedParameters; \
	} \
	\
	Parameters(POPULATE_SHADER_MEMBERS_IMPL)


#define NO_SHADER_PARAMS(PARAMETER)

}

namespace
{


/** Base shader class for diaphragm DOF. */
class FPostProcessDiaphragmDOFShader : public FGlobalShader
{
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DiaphragmDOF::IsSupported(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("COC_TILE_SIZE"), kCocTileSize);
	}

	FPostProcessDiaphragmDOFShader() {}
	FPostProcessDiaphragmDOFShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		Output[0][0].Bind(Initializer.ParameterMap, TEXT("Output0"));
		Output[1][0].Bind(Initializer.ParameterMap, TEXT("Output1"));
		Output[2][0].Bind(Initializer.ParameterMap, TEXT("Output2"));

		if (!Output[0][0].IsBound()) {
			Output[0][0].Bind(Initializer.ParameterMap, TEXT("Output0Mip0"));
			Output[0][1].Bind(Initializer.ParameterMap, TEXT("Output0Mip1"));
			Output[0][2].Bind(Initializer.ParameterMap, TEXT("Output0Mip2"));
			Output[0][3].Bind(Initializer.ParameterMap, TEXT("Output0Mip3"));
			Output[0][4].Bind(Initializer.ParameterMap, TEXT("Output0Mip4"));
		}

		if (!Output[1][0].IsBound())
		{
			Output[1][0].Bind(Initializer.ParameterMap, TEXT("Output1Mip0"));
			Output[1][1].Bind(Initializer.ParameterMap, TEXT("Output1Mip1"));
			Output[1][2].Bind(Initializer.ParameterMap, TEXT("Output1Mip2"));
			Output[1][3].Bind(Initializer.ParameterMap, TEXT("Output1Mip3"));
			Output[1][4].Bind(Initializer.ParameterMap, TEXT("Output1Mip4"));
		}
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;

		for (int32 i = 0; i < ARRAY_COUNT(Output); i++)
		for (int32 j = 0; j < ARRAY_COUNT(Output[0]); j++)
		{
			Ar << Output[i][j];
		}
		
		return bShaderHasOutdatedParameters;
	}

	FPostProcessPassParameters	PostprocessParameter;
	FShaderResourceParameter Output[3][5];
};


/** Experimental helper to reduce boiler plate to dispatch a compute shader */
template<class ComputeShaderType, class TPassType>
class FDispatchDiaphragmDOFPass
{
public:
	static constexpr int32 OutputCount = int32(TPassType::PassOutputCount);

	static constexpr int32 InputViewportCount = 2;

	/** Compute shader to dispatch. */
	TShaderMapRef<ComputeShaderType> Shader;

	/** Compute shader's RHI ressource to dispatch. */
	const FComputeShaderRHIParamRef ShaderRHI;

	/** Viewport. */
	FIntRect DestViewport;

	/** GroupSize */
	FIntPoint GroupSize;

	/** Viewport divisor. */
	int32 DestViewportDivisor;

	/** Whether the outputs should be transitioned to compute or gfx. */
	bool bTransitionOutputToGfx;

	/** Constructor that begins the job. */
	FDispatchDiaphragmDOFPass(
		TPassType* InPass,
		FRenderingCompositePassContext& InContext,
		const typename ComputeShaderType::FPermutationDomain& PermutationVector = typename ComputeShaderType::FPermutationDomain())
		: Shader(InContext.GetShaderMap(), PermutationVector)
		, ShaderRHI(Shader->GetComputeShader())
		, DestViewport(0, 0, 0, 0)
		, GroupSize(kDefaultGroupSize, kDefaultGroupSize)
		, DestViewportDivisor(1)
		, bTransitionOutputToGfx(false)
		, Pass(InPass)
		, Context(InContext)
	{
		DestViewport.Min = FIntPoint::ZeroValue;

		// #todo-renderpasses remove once everything is a renderpass
		UnbindRenderTargets(Context.RHICmdList);
		Context.RHICmdList.SetComputeShader(ShaderRHI);

		for (int32 i = 0; i < OutputCount; i++)
		{
			// Only request surface if output is bound.
			if (!Shader->Output[i][0].IsBound())
			{
				continue;
			}

			DestRenderTarget[i] = &Pass->GetOutput(EPassOutputId(i))->RequestSurface(Context);
			for (int32 j = 0; j < ARRAY_COUNT(Shader->Output[0]); j++)
			{
				if (Shader->Output[i][j].IsBound())
				{
					Context.RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget[i]->MipUAVs[j]);

					Context.RHICmdList.SetUAVParameter(ShaderRHI, Shader->Output[i][j].GetBaseIndex(), DestRenderTarget[i]->MipUAVs[j]);
				}
			}
		}

		Shader->FGlobalShader::template SetParameters<FViewUniformShaderParameters>(
			Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
	}

	/** Access shader to set shader parameter more conveniently. */
	FORCEINLINE ComputeShaderType* operator->() const
	{
		return *Shader;
	}
	FORCEINLINE ComputeShaderType* operator*() const
	{
		return *Shader;
	}

	/** Dispatch compute shader. */
	void Dispatch()
	{
		// Set viewport.
		FIntRect PassViewport = FIntRect::DivideAndRoundUp(DestViewport, DestViewportDivisor);
		Context.SetViewportAndCallRHI(PassViewport);

		// Setup post process paramters.
		Shader->PostprocessParameter.SetCS(ShaderRHI, Context, Context.RHICmdList);

		// Dispatch compute shader.
		DispatchComputeShader(Context.RHICmdList, *Shader, 
			FMath::DivideAndRoundUp(PassViewport.Width(), GroupSize.X),
			FMath::DivideAndRoundUp(PassViewport.Height(), GroupSize.Y),
			1);
	}

	/** Destructor that finish the job. */
	~FDispatchDiaphragmDOFPass()
	{
		Context.RHICmdList.FlushComputeShaderCache();

		for (int32 i = 0; i < OutputCount; i++)
		{
			for (int32 j = 0; j < ARRAY_COUNT(Shader->Output[0]); j++)
			{
				if (Shader->Output[i][j].IsBound())
				{
					Context.RHICmdList.TransitionResource(
						EResourceTransitionAccess::EReadable,
						bTransitionOutputToGfx
							? EResourceTransitionPipeline::EComputeToGfx
							: EResourceTransitionPipeline::EComputeToCompute,
						DestRenderTarget[i]->MipUAVs[j]);

					Context.RHICmdList.SetUAVParameter(ShaderRHI, Shader->Output[i][j].GetBaseIndex(), nullptr);
				}
			}
		}
	}


private:
	/** Current pass doing the dispatch. */
	TPassType* const Pass;

	/** Pass's context. */
	FRenderingCompositePassContext& Context;

	/** Pass's dest render targets. */
	const FSceneRenderTargetItem* DestRenderTarget[OutputCount];
};


} // namespace


// ---------------------------------------------------- Global resource

namespace
{

class FDiaphragmDOFGlobalResource : public FRenderResource
{
public:
	// Number of FRHIDrawIndirectParameters instances in DrawIndirectParametersBuffer.
	static constexpr uint32 kDrawIndirectParametersCount = 2;

	// Maximum number of scattering group per instance.
	static constexpr uint32 kMaxScatteringGroupPerInstance = 21;

	FRWBuffer DrawIndirectParametersBuffer;

	FRWBufferStructured ForegroundScatterDrawListBuffer;
	FRWBufferStructured BackgroundScatterDrawListBuffer;

	// Index buffer to have 4 vertex shader invocation per scatter group that is the most efficient in therms of vertex processing
	// using  if RHI does not support rect list topology.
	FSpriteIndexBuffer<16> ScatterIndexBuffer;


	void Allocate(uint32 MaxScatteringGroupCount)
	{
		// Adds additional room for FPostProcessDiaphragmDOFScatterGroupPackCS's tail clearing.
		MaxScatteringGroupCount += kMaxScatteringGroupPerInstance;

		if (AllocatedMaxScatteringGroupCount == MaxScatteringGroupCount)
		{
			return;
		}

		ReleaseDynamicRHI();

		DrawIndirectParametersBuffer.Initialize(
			sizeof(uint32), kDrawIndirectParametersCount * sizeof(FRHIDrawIndexedIndirectParameters) / sizeof(uint32),
			PF_R32_UINT, BUF_DrawIndirect | BUF_Static);

		ForegroundScatterDrawListBuffer.Initialize(
			sizeof(FVector4), MaxScatteringGroupCount * 5,
			BUF_Static, TEXT("FDiaphragmDOFGlobalResource::ScatterDrawListBuffer"));
		BackgroundScatterDrawListBuffer.Initialize(
			sizeof(FVector4), MaxScatteringGroupCount * 5,
			BUF_Static, TEXT("FDiaphragmDOFGlobalResource::ScatterDrawListBuffer"));

		if (!GRHISupportsRectTopology)
		{
			ScatterIndexBuffer.InitRHI();
		}

		AllocatedMaxScatteringGroupCount = MaxScatteringGroupCount;
	}

	virtual void ReleaseDynamicRHI() override
	{
		DrawIndirectParametersBuffer.Release();
		ForegroundScatterDrawListBuffer.Release();
		BackgroundScatterDrawListBuffer.Release();

		if (!GRHISupportsRectTopology)
		{
			ScatterIndexBuffer.ReleaseRHI();
		}
		AllocatedMaxScatteringGroupCount = 0;
	}

	FDiaphragmDOFGlobalResource()
		: AllocatedMaxScatteringGroupCount(0)
	{ }

private:
	uint32 AllocatedMaxScatteringGroupCount;
};

TGlobalResource<FDiaphragmDOFGlobalResource> GDiaphragmDOFGlobalResource;

}


// ---------------------------------------------------- Shader permutation dimensions

namespace
{

class FDDOFDilateRadiusDim     : SHADER_PERMUTATION_RANGE_INT("DIM_DILATE_RADIUS", 1, 3);
class FDDOFDilateModeDim       : SHADER_PERMUTATION_ENUM_CLASS("DIM_DILATE_MODE", FRCPassDiaphragmDOFDilateCoc::EMode);

class FDDOFLayerProcessingDim  : SHADER_PERMUTATION_ENUM_CLASS("DIM_LAYER_PROCESSING", EDiaphragmDOFLayerProcessing);
class FDDOFGatherRingCountDim  : SHADER_PERMUTATION_RANGE_INT("DIM_GATHER_RING_COUNT", FRCPassDiaphragmDOFGather::kMinRingCount, 3);
class FDDOFGatherQualityDim    : SHADER_PERMUTATION_ENUM_CLASS("DIM_GATHER_QUALITY", FRCPassDiaphragmDOFGather::EQualityConfig);
class FDDOFPostfilterMethodDim : SHADER_PERMUTATION_ENUM_CLASS("DIM_POSTFILTER_METHOD", EDiaphragmDOFPostfilterMethod);
class FDDOFClampInputUVDim     : SHADER_PERMUTATION_BOOL("DIM_CLAMP_INPUT_UV");
class FDDOFRGBColorBufferDim   : SHADER_PERMUTATION_BOOL("DIM_RGB_COLOR_BUFFER");

class FDDOFBokehSimulationDim  : SHADER_PERMUTATION_ENUM_CLASS("DIM_BOKEH_SIMULATION", EDiaphragmDOFBokehSimulation);
class FDDOFScatterOcclusionDim : SHADER_PERMUTATION_BOOL("DIM_SCATTER_OCCLUSION");

}


// ---------------------------------------------------- Shared shader parameters

template<class DispatchContextType>
void SetCocModelParameters(FRenderingCompositePassContext& Context, DispatchContextType& DispatchCtx, const DiaphragmDOF::FPhysicalCocModel& CocModel, float CocRadiusBasis = 1.0f)
{
	FVector4 CocModelParameters(0, 0, 0, 0);
	CocModelParameters.X = CocRadiusBasis * CocModel.InfinityBackgroundCocRadius;
	CocModelParameters.Y = CocRadiusBasis * CocModel.MinForegroundCocRadius;
	CocModelParameters.Z = CocRadiusBasis * CocModel.MaxBackgroundCocRadius;
	SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->CocModelParameters, CocModelParameters);

	FVector2D DepthBlurParameters(0, 0);
	DepthBlurParameters.X = CocModel.DepthBlurExponent;
	DepthBlurParameters.Y = CocRadiusBasis * CocModel.MaxDepthBlurRadius;
	SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->DepthBlurParameters, DepthBlurParameters);
}


// ---------------------------------------------------- Flatten

#define FLATTEN_SHADER_PARAMS(PARAMETER) \
	PARAMETER(FShaderParameter, ThreadIdToBufferUV) \
	PARAMETER(FShaderParameter, MaxBufferUV) \
	PARAMETER(FShaderParameter, PreProcessingToProcessingCocRadiusFactor) \

class FPostProcessCocFlattenCS : public FPostProcessDiaphragmDOFShader
{
	DECLARE_GLOBAL_SHADER(FPostProcessCocFlattenCS);
	SHADER_TYPE_PARAMETERS(FPostProcessCocFlattenCS, FPostProcessDiaphragmDOFShader, FLATTEN_SHADER_PARAMS);

	class FDoCocGather4 : SHADER_PERMUTATION_BOOL("DIM_DO_COC_GATHER4");

	using FPermutationDomain = TShaderPermutationDomain<FDoCocGather4>;

	static_assert(FSceneViewScreenPercentageConfig::kMinTAAUpsampleResolutionFraction == 0.5f,
		"Gather4 shader permutation assumes with min TAAU screen percentage = 50%.");
	static_assert(FSceneViewScreenPercentageConfig::kMaxTAAUpsampleResolutionFraction == 2.0f,
		"Gather4 shader permutation assumes with max TAAU screen percentage = 200%.");
};

IMPLEMENT_GLOBAL_SHADER(FPostProcessCocFlattenCS, "/Engine/Private/DiaphragmDOF/DOFCocTileFlatten.usf", "CocFlattenMainCS", SF_Compute);


void FRCPassDiaphragmDOFFlattenCoc::Process(FRenderingCompositePassContext& Context)
{
	FPostProcessCocFlattenCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FPostProcessCocFlattenCS::FDoCocGather4>(Params.InputViewSize != Params.GatherViewSize);

	FDispatchDiaphragmDOFPass<FPostProcessCocFlattenCS, FRCPassDiaphragmDOFFlattenCoc> DispatchCtx(this, Context, PermutationVector);
	DispatchCtx.DestViewport = FIntRect(0, 0, Params.GatherViewSize.X, Params.GatherViewSize.Y);

	SCOPED_DRAW_EVENTF(Context.RHICmdList, DiaphragmDOFFlattenCoc, TEXT("DiaphragmDOF FlattenCoc(Gather4=%s) %dx%d"),
		PermutationVector.Get<FPostProcessCocFlattenCS::FDoCocGather4>() ? TEXT("Yes") : TEXT("No"),
		DispatchCtx.DestViewport.Width(), DispatchCtx.DestViewport.Height());

	{
		FIntPoint SrcSize = GetInputDesc(ePId_Input0)->Extent;

		FVector2D ThreadIdToBufferUV(
			Params.InputViewSize.X / float(Params.GatherViewSize.X * SrcSize.X),
			Params.InputViewSize.Y / float(Params.GatherViewSize.Y * SrcSize.Y));
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->ThreadIdToBufferUV, ThreadIdToBufferUV);

		// - 1.0 instead of - 0.5 because used for Gather4.
		FVector2D MaxBufferUV(
			(Params.InputViewSize.X - 1.0f) / float(SrcSize.X),
			(Params.InputViewSize.Y - 1.0f) / float(SrcSize.Y));
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->MaxBufferUV, MaxBufferUV);
	}
	DispatchCtx.Dispatch();
}

FPooledRenderTargetDesc FRCPassDiaphragmDOFFlattenCoc::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc UnmodifiedRet = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	UnmodifiedRet.Reset();

	FIntPoint TileCount = CocTileGridSize(UnmodifiedRet.Extent);

	FPooledRenderTargetDesc Ret(FPooledRenderTargetDesc::Create2DDesc(TileCount, PF_FloatRGBA, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
	Ret.DebugName = InPassOutputId == ePId_Output0 ? TEXT("DOFFlattenFgdCoc") : TEXT("DOFFlattenBgdCoc");
	Ret.Format = InPassOutputId == ePId_Output0 ? PF_G16R16F : PF_FloatRGBA;
	return Ret;
}


// ---------------------------------------------------- Dilate

#define COC_DILATE_SHADER_PARAMS(PARAMETER) \
	PARAMETER(FShaderParameter, SampleOffsetMultipler) \
	PARAMETER(FShaderParameter, fSampleOffsetMultipler) \
	PARAMETER(FShaderParameter, CocRadiusToBucketDistanceUpperBound) \
	PARAMETER(FShaderParameter, BucketDistanceToCocRadius) \

class FPostProcessCocDilateCS : public FPostProcessDiaphragmDOFShader
{
	DECLARE_GLOBAL_SHADER(FPostProcessCocDilateCS);
	SHADER_TYPE_PARAMETERS(FPostProcessCocDilateCS, FPostProcessDiaphragmDOFShader, COC_DILATE_SHADER_PARAMS);

	using FPermutationDomain = TShaderPermutationDomain<FDDOFDilateRadiusDim, FDDOFDilateModeDim>;
};

IMPLEMENT_GLOBAL_SHADER(FPostProcessCocDilateCS, "/Engine/Private/DiaphragmDOF/DOFCocTileDilate.usf", "CocDilateMainCS", SF_Compute);


void FRCPassDiaphragmDOFDilateCoc::Process(FRenderingCompositePassContext& Context)
{
	FPostProcessCocDilateCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FDDOFDilateRadiusDim>(Params.SampleRadiusCount);
	PermutationVector.Set<FDDOFDilateModeDim>(Params.Mode);
	// TODO: permutation to do foreground and background separately, to have higher occupancy?

	FDispatchDiaphragmDOFPass<FPostProcessCocDilateCS, FRCPassDiaphragmDOFDilateCoc> DispatchCtx(this, Context, PermutationVector);
	DispatchCtx.DestViewport = FIntRect(FIntPoint::ZeroValue, FIntPoint::DivideAndRoundUp(Params.GatherViewSize, kCocTileSize));

	SCOPED_DRAW_EVENTF(Context.RHICmdList, DiaphragmDOFDilateCoc, TEXT("DiaphragmDOF DilateCoc(1/16 %s radius=%d step=%d) %dx%d"),
		GetEventName(Params.Mode), Params.SampleRadiusCount, Params.SampleDistanceMultiplier,
		DispatchCtx.DestViewport.Width(), DispatchCtx.DestViewport.Height());

	{
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->SampleOffsetMultipler, Params.SampleDistanceMultiplier);
	
		float fSampleOffsetMultipler = Params.SampleDistanceMultiplier;
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->fSampleOffsetMultipler, fSampleOffsetMultipler);

		float CocRadiusToBucketDistanceUpperBound = Params.PreProcessingToProcessingCocRadiusFactor * Params.BluringRadiusErrorMultiplier;
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->CocRadiusToBucketDistanceUpperBound, CocRadiusToBucketDistanceUpperBound);

		float BucketDistanceToCocRadius = 1.0f / CocRadiusToBucketDistanceUpperBound;
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->BucketDistanceToCocRadius, BucketDistanceToCocRadius);
	}
	DispatchCtx.Dispatch();
}

FPooledRenderTargetDesc FRCPassDiaphragmDOFDilateCoc::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	// When dilating only min foreground and max background, only one channel.
	if (Params.Mode == EMode::MinForegroundAndMaxBackground)
	{
		Ret.DebugName = InPassOutputId == ePId_Output0 ? TEXT("DOFDilateMinFgdCoc") : TEXT("DOFDilateMaxBgdCoc");
		Ret.Format = PF_R16F;
	}
	else
	{
		Ret.DebugName = InPassOutputId == ePId_Output0 ? TEXT("DOFDilateFgdCoc") : TEXT("DOFDilateBgdCoc");
		Ret.Format = InPassOutputId == ePId_Output0 ? PF_G16R16F : PF_FloatRGBA;
	}

	return Ret;
}


// ---------------------------------------------------- Setup

#define SETUP_SHADER_PARAMS(PARAMETER) \
	PARAMETER(FShaderParameter, CocModelParameters) \
	PARAMETER(FShaderParameter, DepthBlurParameters) \
	PARAMETER(FShaderParameter, CocRadiusBasis) \


class FPostProcessDiaphragmDOFSetupCS : public FPostProcessDiaphragmDOFShader
{
	DECLARE_GLOBAL_SHADER(FPostProcessDiaphragmDOFSetupCS);
	SHADER_TYPE_PARAMETERS(FPostProcessDiaphragmDOFSetupCS, FPostProcessDiaphragmDOFShader, SETUP_SHADER_PARAMS);

	class FOutputResDivisor : SHADER_PERMUTATION_INT("DIM_OUTPUT_RES_DIVISOR", 3);

	using FPermutationDomain = TShaderPermutationDomain<FOutputResDivisor>;
};

IMPLEMENT_GLOBAL_SHADER(FPostProcessDiaphragmDOFSetupCS, "/Engine/Private/DiaphragmDOF/DOFSetup.usf", "SetupCS", SF_Compute);


void FRCPassDiaphragmDOFSetup::Process(FRenderingCompositePassContext& Context)
{
	int32 DispatchDivisor = 1;
	FPostProcessDiaphragmDOFSetupCS::FPermutationDomain PermutationVector;
	
	FIntPoint GroupSize(kDefaultGroupSize, kDefaultGroupSize);
	float CocRadiusBasis = 1.0f;
	if (Params.bOutputFullResolution && Params.bOutputHalfResolution)
	{
		PermutationVector.Set<FPostProcessDiaphragmDOFSetupCS::FOutputResDivisor>(0);
		GroupSize *= 2;
	}
	else if (Params.bOutputFullResolution)
	{
		PermutationVector.Set<FPostProcessDiaphragmDOFSetupCS::FOutputResDivisor>(1);
	}
	else if (Params.bOutputHalfResolution)
	{
		PermutationVector.Set<FPostProcessDiaphragmDOFSetupCS::FOutputResDivisor>(2);
		DispatchDivisor = 2;
		CocRadiusBasis = Params.HalfResCocRadiusBasis;
	}
	else
	{
		check(0);
	}

	FDispatchDiaphragmDOFPass<FPostProcessDiaphragmDOFSetupCS, FRCPassDiaphragmDOFSetup> DispatchCtx(this, Context, PermutationVector);
	DispatchCtx.DestViewport = GetLowerResViewport(Context.View.ViewRect, DispatchDivisor);
	DispatchCtx.GroupSize = GroupSize;

	// Because DOF's TAA pass is a pixel shader.
	DispatchCtx.bTransitionOutputToGfx = true;

	// Begin of Main diaphragm DOF's draw event.
	{
		BEGIN_DRAW_EVENTF(
			Context.RHICmdList,
			DiaphragmDOF,
			MainDrawEvent,
			TEXT("DiaphragmDOF"));
	}

	// Outputs Coc rage in debug event name with DispatchCtx.DestViewport.Width() because this is one used for deciding the number of
	// rings of the gathering passes, but also the one used for Dilate passes settings.
	SCOPED_DRAW_EVENTF(Context.RHICmdList, DiaphragmDOFDownsample, TEXT("DiaphragmDOF Setup(%s CoC=[%d;%d] alpha=no) %dx%d"),
		!Params.bOutputFullResolution ? TEXT("HalfRes") : (!Params.bOutputHalfResolution ? TEXT("FullRes") : TEXT("Full&HalfRes")),
		FMath::FloorToInt(Params.CocModel.ComputeViewMinForegroundCocRadius(DispatchCtx.DestViewport.Width())),
		FMath::CeilToInt(Params.CocModel.ComputeViewMaxBackgroundCocRadius(DispatchCtx.DestViewport.Width())),
		DispatchCtx.DestViewport.Width(), DispatchCtx.DestViewport.Height());

	{
		SetCocModelParameters(Context, DispatchCtx, Params.CocModel, CocRadiusBasis);
		
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->CocRadiusBasis,
			FVector2D(Params.FullResCocRadiusBasis, Params.HalfResCocRadiusBasis));
	}

	DispatchCtx.Dispatch();
}

FPooledRenderTargetDesc FRCPassDiaphragmDOFSetup::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	// Reset so that the number of samples of decsriptor becomes 1, which is totally legal still with MSAA because
	// the scene color will already be resolved to ShaderResource texture that is always 1. This is to work around
	// hack that MSAA will have targetable texture with MSAA != shader resource, and still have descriptor indicating
	// the number of samples of the targetable resource.
	Ret.Reset();

	Ret.Extent /= InPassOutputId == ePId_Output0 ? 1 : 2;
	Ret.DebugName = InPassOutputId == ePId_Output0 ? TEXT("DOFFullResSetup") : TEXT("DOFHalfResSetup");
	Ret.Format = PF_FloatRGBA;
	Ret.TargetableFlags |= TexCreate_UAV;
	Ret.Flags &= ~(TexCreate_FastVRAM);
	Ret.Flags |= GFastVRamConfig.DOFSetup;
	if (InPassOutputId == ePId_Output2)
	{
		Ret.Format = PF_R16F;
	}
	return Ret;
}


// ---------------------------------------------------- Reduce

#define REDUCE_SHADER_PARAMS(PARAMETER) \
	PARAMETER(FShaderParameter, MaxInputBufferUV) \
	PARAMETER(FShaderParameter, MaxScatteringGroupCount) \
	PARAMETER(FShaderParameter, PreProcessingToProcessingCocRadiusFactor) \
	PARAMETER(FShaderParameter, MinScatteringCocRadius) \
	PARAMETER(FShaderParameter, NeighborCompareMaxColor) \
	PARAMETER(FShaderResourceParameter, OutScatterDrawIndirectParameters) \
	PARAMETER(FShaderResourceParameter, OutForegroundScatterDrawList) \
	PARAMETER(FShaderResourceParameter, OutBackgroundScatterDrawList) \
	PARAMETER(FShaderResourceParameter, EyeAdaptation) \

class FPostProcessDiaphragmDOFReduceCS : public FPostProcessDiaphragmDOFShader
{
	DECLARE_GLOBAL_SHADER(FPostProcessDiaphragmDOFReduceCS);
	SHADER_TYPE_PARAMETERS(FPostProcessDiaphragmDOFReduceCS, FPostProcessDiaphragmDOFShader, REDUCE_SHADER_PARAMS);

	class FReduceMipCount : SHADER_PERMUTATION_RANGE_INT("DIM_REDUCE_MIP_COUNT", 2, 3);
	class FHybridScatterForeground : SHADER_PERMUTATION_BOOL("DIM_HYBRID_SCATTER_FGD");
	class FHybridScatterBackground : SHADER_PERMUTATION_BOOL("DIM_HYBRID_SCATTER_BGD");

	using FPermutationDomain = TShaderPermutationDomain<
		FReduceMipCount,
		FHybridScatterForeground,
		FHybridScatterBackground,
		FDDOFRGBColorBufferDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// Do not compile storing Coc independently of RGB if not supported.
		if (PermutationVector.Get<FDDOFRGBColorBufferDim>() && !FRCPassDiaphragmDOFGather::SupportRGBColorBuffer(Parameters.Platform))
		{
			return false;
		}

		if (!FRCPassDiaphragmDOFHybridScatter::IsSupported(Parameters.Platform))
		{
			if (PermutationVector.Get<FHybridScatterForeground>() || PermutationVector.Get<FHybridScatterBackground>())
			{
				return false;
			}
		}

		return FPostProcessDiaphragmDOFShader::ShouldCompilePermutation(Parameters);
	}
};

class FPostProcessDiaphragmDOFScatterGroupPackCS : public FPostProcessDiaphragmDOFShader
{
	DECLARE_GLOBAL_SHADER(FPostProcessDiaphragmDOFScatterGroupPackCS);
	SHADER_TYPE_PARAMETERS(FPostProcessDiaphragmDOFScatterGroupPackCS, FPostProcessDiaphragmDOFShader, REDUCE_SHADER_PARAMS);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!FRCPassDiaphragmDOFHybridScatter::IsSupported(Parameters.Platform))
		{
			return false;
		}
		return FPostProcessDiaphragmDOFShader::ShouldCompilePermutation(Parameters);
	}
};

IMPLEMENT_GLOBAL_SHADER(FPostProcessDiaphragmDOFReduceCS, "/Engine/Private/DiaphragmDOF/DOFReduce.usf", "ReduceCS", SF_Compute);

IMPLEMENT_GLOBAL_SHADER(FPostProcessDiaphragmDOFScatterGroupPackCS, "/Engine/Private/DiaphragmDOF/DOFHybridScatterCompilation.usf", "ScatterGroupPackMainCS", SF_Compute);


void FRCPassDiaphragmDOFReduce::Process(FRenderingCompositePassContext& Context)
{
	#if UE_BUILD_SHIPPING
		const bool bDebugScatterPerf = false;
	#else
		const bool bDebugScatterPerf = CVarDebugScatterPerf.GetValueOnRenderThread() == 1;
	#endif

	FIntRect DestViewport(0, 0, Params.InputViewSize.X, Params.InputViewSize.Y);
	bool bDoAnyHybridScatteringExtraction = (Params.bExtractForegroundHybridScattering || Params.bExtractBackgroundHybridScattering) && !bDebugScatterPerf;

	// Saves some scattering group for the clear at end of ScatterDrawListBuffer in ScatterGroupPackMainCS.
	FIntPoint SrcSize = GetInputDesc(ePId_Input0)->Extent;
	uint32 MaxScatteringGroupCount = FMath::Max(Params.MaxScatteringRatio * 0.25f * SrcSize.X * SrcSize.Y - FDiaphragmDOFGlobalResource::kMaxScatteringGroupPerInstance, float(FDiaphragmDOFGlobalResource::kMaxScatteringGroupPerInstance));

	// Emit the draw event soon to contain the ClearUAV().
	SCOPED_DRAW_EVENTF(Context.RHICmdList, DiaphragmDOFReduce, TEXT("DiaphragmDOF Reduce(Mips=%d FgdScatter=%s BgdScatter=%s%s) %dx%d"),
		Params.MipLevelCount,
		Params.bExtractForegroundHybridScattering ? TEXT("Yes") : TEXT("No"),
		Params.bExtractBackgroundHybridScattering ? TEXT("Yes") : TEXT("No"),
		Params.bRGBBufferSeparateCocBuffer ? TEXT(" R11G11B10") : TEXT(""),
		DestViewport.Width(), DestViewport.Height());

	// Clears the draw indirect parameters to have the scattering group count ready to be atomically incremented.
	FRWBuffer* DrawIndirectParametersBuffer = nullptr;
	FRWBufferStructured* ScatterDrawListBuffer[2] = {nullptr, nullptr};
	if (bDoAnyHybridScatteringExtraction)
	{
		GDiaphragmDOFGlobalResource.Allocate(MaxScatteringGroupCount);

		DrawIndirectParametersBuffer = &GDiaphragmDOFGlobalResource.DrawIndirectParametersBuffer;
		ScatterDrawListBuffer[0] = &GDiaphragmDOFGlobalResource.ForegroundScatterDrawListBuffer;
		ScatterDrawListBuffer[1] = &GDiaphragmDOFGlobalResource.BackgroundScatterDrawListBuffer;

		ClearUAV(Context.RHICmdList, *DrawIndirectParametersBuffer, 0);

		Context.RHICmdList.TransitionResource(
			EResourceTransitionAccess::ERWBarrier,
			EResourceTransitionPipeline::EComputeToCompute,
			DrawIndirectParametersBuffer->UAV);

		if (Params.bExtractForegroundHybridScattering)
		{
			Context.RHICmdList.TransitionResource(
				EResourceTransitionAccess::EWritable,
				EResourceTransitionPipeline::EGfxToCompute,
				ScatterDrawListBuffer[0]->UAV);
		}

		if (Params.bExtractBackgroundHybridScattering)
		{
			Context.RHICmdList.TransitionResource(
				EResourceTransitionAccess::EWritable,
				EResourceTransitionPipeline::EGfxToCompute,
				ScatterDrawListBuffer[1]->UAV);
		}
	}

	// Reduce.
	{
		FPostProcessDiaphragmDOFReduceCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FPostProcessDiaphragmDOFReduceCS::FReduceMipCount>(Params.MipLevelCount);
		PermutationVector.Set<FPostProcessDiaphragmDOFReduceCS::FHybridScatterForeground>(Params.bExtractForegroundHybridScattering && !bDebugScatterPerf);
		PermutationVector.Set<FPostProcessDiaphragmDOFReduceCS::FHybridScatterBackground>(Params.bExtractBackgroundHybridScattering && !bDebugScatterPerf);
		PermutationVector.Set<FDDOFRGBColorBufferDim>(Params.bRGBBufferSeparateCocBuffer);

		FDispatchDiaphragmDOFPass<FPostProcessDiaphragmDOFReduceCS, FRCPassDiaphragmDOFReduce> DispatchCtx(this, Context, PermutationVector);
		DispatchCtx.DestViewport = DestViewport;

		{
			FVector2D MaxInputBufferUV;
			MaxInputBufferUV.X = (DestViewport.Width() - 0.5f) / SrcSize.X;
			MaxInputBufferUV.Y = (DestViewport.Height() - 0.5f) / SrcSize.Y;
			SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->MaxInputBufferUV, MaxInputBufferUV);
		}

		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI,
			DispatchCtx->PreProcessingToProcessingCocRadiusFactor, 
			FVector2D(Params.PreProcessingToProcessingCocRadiusFactor, 1.0f / Params.PreProcessingToProcessingCocRadiusFactor));

		if (bDoAnyHybridScatteringExtraction)
		{
			SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI,
				DispatchCtx->MaxScatteringGroupCount, MaxScatteringGroupCount);

			FTextureRHIRef EyeAdaptationTex = GWhiteTexture->TextureRHI;
			if (Context.View.HasValidEyeAdaptation())
			{
				EyeAdaptationTex = Context.View.GetEyeAdaptation(Context.RHICmdList)->GetRenderTargetItem().TargetableTexture;
			}
			SetTextureParameter(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->EyeAdaptation, EyeAdaptationTex);

			SetUAVParameter(Context.RHICmdList, DispatchCtx.ShaderRHI,
				DispatchCtx->OutScatterDrawIndirectParameters, DrawIndirectParametersBuffer->UAV);
			SetUAVParameter(Context.RHICmdList, DispatchCtx.ShaderRHI,
				DispatchCtx->OutForegroundScatterDrawList, ScatterDrawListBuffer[0]->UAV);
			SetUAVParameter(Context.RHICmdList, DispatchCtx.ShaderRHI,
				DispatchCtx->OutBackgroundScatterDrawList, ScatterDrawListBuffer[1]->UAV);

			SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->MinScatteringCocRadius, Params.MinScatteringCocRadius);

			float NeighborCompareMaxColor = CVarScatterNeighborCompareMaxColor.GetValueOnRenderThread();
			SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->NeighborCompareMaxColor, NeighborCompareMaxColor);
		}

		DispatchCtx.Dispatch();

		if (bDoAnyHybridScatteringExtraction)
		{
			SetUAVParameter(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->OutScatterDrawIndirectParameters, nullptr);
			SetUAVParameter(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->OutForegroundScatterDrawList, nullptr);
			SetUAVParameter(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->OutBackgroundScatterDrawList, nullptr);
		}
	}

	if (!bDoAnyHybridScatteringExtraction)
	{
		return;
	}

	Context.RHICmdList.TransitionResource(
		EResourceTransitionAccess::ERWBarrier,
		EResourceTransitionPipeline::EComputeToCompute,
		DrawIndirectParametersBuffer->UAV);

	if (bDoAnyHybridScatteringExtraction)
	{
		Context.RHICmdList.TransitionResource(
			EResourceTransitionAccess::ERWNoBarrier,
			EResourceTransitionPipeline::EComputeToCompute,
			ScatterDrawListBuffer[0]->UAV);
	
		Context.RHICmdList.TransitionResource(
			EResourceTransitionAccess::ERWNoBarrier,
			EResourceTransitionPipeline::EComputeToCompute,
			ScatterDrawListBuffer[1]->UAV);
	}

	// Pack multiple scattering group on same primitive instance to increase wave occupancy in the scattering vertex shader.
	{
		FDispatchDiaphragmDOFPass<FPostProcessDiaphragmDOFScatterGroupPackCS, FRCPassDiaphragmDOFReduce> DispatchCtx(this, Context);
		DispatchCtx.DestViewport = FIntRect(0, 0, 2, 1);
		DispatchCtx.GroupSize = FIntPoint(1, 1);

		{
			SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI,
				DispatchCtx->MaxScatteringGroupCount, MaxScatteringGroupCount);
			SetUAVParameter(Context.RHICmdList, DispatchCtx.ShaderRHI,
				DispatchCtx->OutScatterDrawIndirectParameters, DrawIndirectParametersBuffer->UAV);
			SetUAVParameter(Context.RHICmdList, DispatchCtx.ShaderRHI,
				DispatchCtx->OutForegroundScatterDrawList, ScatterDrawListBuffer[0]->UAV);
			SetUAVParameter(Context.RHICmdList, DispatchCtx.ShaderRHI,
				DispatchCtx->OutBackgroundScatterDrawList, ScatterDrawListBuffer[1]->UAV);
		}
		DispatchCtx.Dispatch();
		{
			SetUAVParameter(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->OutScatterDrawIndirectParameters, nullptr);
			SetUAVParameter(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->OutForegroundScatterDrawList, nullptr);
			SetUAVParameter(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->OutBackgroundScatterDrawList, nullptr);
		}
	}

	Context.RHICmdList.TransitionResource(
		EResourceTransitionAccess::EReadable,
		EResourceTransitionPipeline::EComputeToGfx,
		DrawIndirectParametersBuffer->UAV);

	if (Params.bExtractForegroundHybridScattering)
	{
		Context.RHICmdList.TransitionResource(
			EResourceTransitionAccess::EReadable,
			EResourceTransitionPipeline::EComputeToGfx,
			ScatterDrawListBuffer[0]->UAV);
	}

	if (Params.bExtractBackgroundHybridScattering)
	{
		Context.RHICmdList.TransitionResource(
			EResourceTransitionAccess::EReadable,
			EResourceTransitionPipeline::EComputeToGfx,
			ScatterDrawListBuffer[1]->UAV);
	}
}

FPooledRenderTargetDesc FRCPassDiaphragmDOFReduce::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	Ret.DebugName = TEXT("DOFReduce");
	Ret.Format = PF_FloatRGBA;
	Ret.TargetableFlags |= TexCreate_UAV;
	Ret.NumMips = Params.MipLevelCount;

	Ret.Flags &= ~(TexCreate_FastVRAM);
	Ret.Flags |= GFastVRamConfig.DOFReduce;

	// Make sure the mip 0 is a multiple of 2^NumMips so there is no per mip level UV conversion to do in the gathering shader.
	// Also make sure it is a multiple of group size because reduce shader unconditionally output Mip0.
	int32 Multiple = FMath::Max(1 << (Ret.NumMips - 1), kDefaultGroupSize);
	Ret.Extent.X = Multiple * FMath::DivideAndRoundUp(Ret.Extent.X, Multiple);
	Ret.Extent.Y = Multiple * FMath::DivideAndRoundUp(Ret.Extent.Y, Multiple);

	if (InPassOutputId == ePId_Output1)
	{
		Ret.Format = PF_R16F;
	}

	if (Params.bRGBBufferSeparateCocBuffer)
	{
		Ret.Format = InPassOutputId == ePId_Output0 ? PF_FloatR11G11B10 : PF_R16F;
	}

	return Ret;
}


// ---------------------------------------------------- Downsample

#define DOWNSAMPLE_SHADER_PARAMS(PARAMETER) \
	PARAMETER(FShaderParameter, MaxBufferUV) \
	PARAMETER(FShaderParameter, OutputCocRadiusMultiplier) \

class FPostProcessDiaphragmDOFDownsampleCS : public FPostProcessDiaphragmDOFShader
{
	DECLARE_GLOBAL_SHADER(FPostProcessDiaphragmDOFDownsampleCS);
	SHADER_TYPE_PARAMETERS(FPostProcessDiaphragmDOFDownsampleCS, FPostProcessDiaphragmDOFShader, DOWNSAMPLE_SHADER_PARAMS);
};

IMPLEMENT_GLOBAL_SHADER(FPostProcessDiaphragmDOFDownsampleCS, "/Engine/Private/DiaphragmDOF/DOFDownsample.usf", "DownsampleCS", SF_Compute);

void FRCPassDiaphragmDOFDownsample::Process(FRenderingCompositePassContext& Context)
{
	FDispatchDiaphragmDOFPass<FPostProcessDiaphragmDOFDownsampleCS, FRCPassDiaphragmDOFDownsample> DispatchCtx(this, Context);
	DispatchCtx.DestViewport = GetLowerResViewport(FIntRect(FIntPoint::ZeroValue, Params.InputViewSize), 2);

	SCOPED_DRAW_EVENTF(Context.RHICmdList, DiaphragmDOFDownsample, TEXT("DiaphragmDOF Downsample %dx%d"),
		DispatchCtx.DestViewport.Width(), DispatchCtx.DestViewport.Height());

	{
		FIntPoint SrcSize = GetInputDesc(ePId_Input0)->Extent;

		FVector2D MaxBufferUV(
			(Params.InputViewSize.X - 0.5f) / float(SrcSize.X),
			(Params.InputViewSize.Y - 0.5f) / float(SrcSize.Y));
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->MaxBufferUV, MaxBufferUV);
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->OutputCocRadiusMultiplier, Params.OutputCocRadiusMultiplier);
	}

	DispatchCtx.Dispatch();
}

FPooledRenderTargetDesc FRCPassDiaphragmDOFDownsample::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	ensureMsgf((Ret.Extent.X % 2) == 0 && (Ret.Extent.Y % 2) == 0,
		TEXT("DOF's downsample pass wants BufferUV compatible with higher res."));
	Ret.Extent /= 2;
	Ret.DebugName = TEXT("DOFDownsample");
	Ret.TargetableFlags |= TexCreate_UAV;
	if (InPassOutputId == ePId_Output0)
	{
		Ret.Format = Params.bRGBBufferOnly ? PF_FloatR11G11B10 : PF_FloatRGBA;
	}
	else
	{
		Ret.Format = PF_R16F;
	}
	return Ret;
}


// ---------------------------------------------------- Gather

#define GATHER_SHADER_PARAMS(PARAMETER) \
	PARAMETER(FShaderParameter, TemporalJitterPixels) \
	PARAMETER(FShaderParameter, MipBias) \
	PARAMETER(FShaderParameter, DispatchThreadIdToInputBufferUV) \
	PARAMETER(FShaderParameter, MaxRecombineAbsCocRadius) \
	PARAMETER(FShaderParameter, ConsiderCocRadiusAffineTransformation0) \
	PARAMETER(FShaderParameter, ConsiderCocRadiusAffineTransformation1) \
	PARAMETER(FShaderParameter, ConsiderAbsCocRadiusAffineTransformation) \
	PARAMETER(FShaderParameter, InputBufferUVToOutputPixel) \

class FPostProcessDiaphragmDOFGatherCS : public FPostProcessDiaphragmDOFShader
{
	DECLARE_GLOBAL_SHADER(FPostProcessDiaphragmDOFGatherCS);
	SHADER_TYPE_PARAMETERS(FPostProcessDiaphragmDOFGatherCS, FPostProcessDiaphragmDOFShader, GATHER_SHADER_PARAMS);

	using FPermutationDomain = TShaderPermutationDomain<
		FDDOFLayerProcessingDim,
		FDDOFGatherRingCountDim,
		FDDOFBokehSimulationDim,
		FDDOFGatherQualityDim,
		FDDOFClampInputUVDim,
		FDDOFRGBColorBufferDim>;

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		// There is a lot of permutation, so no longer compile permutation.
		if (1)
		{
			// Alway clamp input buffer UV.
			PermutationVector.Set<FDDOFClampInputUVDim>(true);

			// Alway simulate bokeh generically.
			if (PermutationVector.Get<FDDOFBokehSimulationDim>() == EDiaphragmDOFBokehSimulation::SimmetricBokeh)
			{
				PermutationVector.Set<FDDOFBokehSimulationDim>(EDiaphragmDOFBokehSimulation::GenericBokeh);
			}
		}

		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// Do not compile this permutation if we know this is going to be remapped.
		if (RemapPermutation(PermutationVector) != PermutationVector)
		{
			return false;
		}

		// Some platforms might be to slow for even considering large number of gathering samples.
		if (PermutationVector.Get<FDDOFGatherRingCountDim>() > FRCPassDiaphragmDOFGather::MaxRingCount(Parameters.Platform))
		{
			return false;
		}

		// Do not compile storing Coc independently of RGB.
		if (PermutationVector.Get<FDDOFRGBColorBufferDim>() && !FRCPassDiaphragmDOFGather::SupportRGBColorBuffer(Parameters.Platform))
		{
			return false;
		}

		// No point compiling gather pass with hybrid scatter occlusion if the shader platform doesn't support if.
		if (!FRCPassDiaphragmDOFHybridScatter::IsSupported(Parameters.Platform) &&
			PermutationVector.Get<FDDOFGatherQualityDim>() == FRCPassDiaphragmDOFGather::EQualityConfig::HighQualityWithHybridScatterOcclusion) return false;

		// Do not compile bokeh simulation shaders on platform that couldn't handle them anyway.
		if (!FRCPassDiaphragmDOFGather::SupportsBokehSimmulation(Parameters.Platform) &&
			PermutationVector.Get<FDDOFBokehSimulationDim>() != EDiaphragmDOFBokehSimulation::Disabled) return false;

		if (PermutationVector.Get<FDDOFLayerProcessingDim>() == EDiaphragmDOFLayerProcessing::ForegroundOnly)
		{
			// Foreground does not support CocVariance output yet.
			if (PermutationVector.Get<FDDOFGatherQualityDim>() == FRCPassDiaphragmDOFGather::EQualityConfig::HighQualityWithHybridScatterOcclusion) return false;

			// Storing Coc independently of RGB is only supported for low gathering quality.
			if (PermutationVector.Get<FDDOFRGBColorBufferDim>() &&
				PermutationVector.Get<FDDOFGatherQualityDim>() != FRCPassDiaphragmDOFGather::EQualityConfig::LowQualityAccumulator)
				return false;
		}
		else if (PermutationVector.Get<FDDOFLayerProcessingDim>() == EDiaphragmDOFLayerProcessing::ForegroundHoleFilling)
		{
			// Foreground hole filling does not need to output CocVariance, since this is the job of foreground pass.
			if (PermutationVector.Get<FDDOFGatherQualityDim>() == FRCPassDiaphragmDOFGather::EQualityConfig::HighQualityWithHybridScatterOcclusion) return false;

			// Foreground hole filling doesn't have lower quality accumulator.
			if (PermutationVector.Get<FDDOFGatherQualityDim>() == FRCPassDiaphragmDOFGather::EQualityConfig::LowQualityAccumulator) return false;

			// Foreground hole filling doesn't need cinematic quality.
			if (PermutationVector.Get<FDDOFGatherQualityDim>() == FRCPassDiaphragmDOFGather::EQualityConfig::Cinematic) return false;

			// No bokeh simulation on hole filling, always use euclidian closest distance to compute opacity alpha channel.
			if (PermutationVector.Get<FDDOFBokehSimulationDim>() != EDiaphragmDOFBokehSimulation::Disabled) return false;

			// Storing Coc independently of RGB is only supported for RecombineQuality == 0.
			if (PermutationVector.Get<FDDOFRGBColorBufferDim>())
				return false;
		}
		else if (PermutationVector.Get<FDDOFLayerProcessingDim>() == EDiaphragmDOFLayerProcessing::SlightOutOfFocus)
		{
			// Slight out of focus gather pass does not need large radius since only accumulating only
			// abs(CocRadius) < kMaxSlightOutOfFocusRingCount.
			if (PermutationVector.Get<FDDOFGatherRingCountDim>() > FRCPassDiaphragmDOFGather::kMaxSlightOutOfFocusRingCount) return false;

			// Slight out of focus don't need to output CocVariance since there is no hybrid scattering.
			if (PermutationVector.Get<FDDOFGatherQualityDim>() == FRCPassDiaphragmDOFGather::EQualityConfig::HighQualityWithHybridScatterOcclusion) return false;

			// Slight out of focus filling can't have lower quality accumulator since it needs to brute force the focus areas.
			if (PermutationVector.Get<FDDOFGatherQualityDim>() == FRCPassDiaphragmDOFGather::EQualityConfig::LowQualityAccumulator) return false;

			// Slight out of focus doesn't have cinematic quality, yet.
			if (PermutationVector.Get<FDDOFGatherQualityDim>() == FRCPassDiaphragmDOFGather::EQualityConfig::Cinematic) return false;

			// Storing Coc independently of RGB is only supported for RecombineQuality == 0.
			if (PermutationVector.Get<FDDOFRGBColorBufferDim>())
				return false;
		}
		else if (PermutationVector.Get<FDDOFLayerProcessingDim>() == EDiaphragmDOFLayerProcessing::BackgroundOnly)
		{
			// There is no performance point doing high quality gathering without scattering occlusion.
			if (PermutationVector.Get<FDDOFGatherQualityDim>() == FRCPassDiaphragmDOFGather::EQualityConfig::HighQuality) return false;

			// Storing Coc independently of RGB is only supported for low gathering quality.
			if (PermutationVector.Get<FDDOFRGBColorBufferDim>() &&
				PermutationVector.Get<FDDOFGatherQualityDim>() != FRCPassDiaphragmDOFGather::EQualityConfig::LowQualityAccumulator)
				return false;
		}
		else if (PermutationVector.Get<FDDOFLayerProcessingDim>() == EDiaphragmDOFLayerProcessing::ForegroundAndBackground)
		{
			// Gathering foreground and backrgound at same time is not supported yet.
			return false;
		}
		else
		{
			check(0);
		}

		return FPostProcessDiaphragmDOFShader::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// The gathering pass shader code gives a really hard time to the HLSL compiler. To improve
		// iteration time on the shader, only pass down a /O1 instead of /O3.
		if (Parameters.Platform == SP_PCD3D_SM5)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
		}
	}
};

IMPLEMENT_GLOBAL_SHADER(FPostProcessDiaphragmDOFGatherCS, "/Engine/Private/DiaphragmDOF/DOFGatherPass.usf", "GatherMainCS", SF_Compute);

void FRCPassDiaphragmDOFGather::Process(FRenderingCompositePassContext& Context)
{
	check(Params.RingCount <= MaxRingCount(Context.GetShaderPlatform()));

	// Reduce pass output unconditionally in Mip0, so the input view is actually slightly larger that gives room to not
	// clamp UV in gather pass.
	FIntPoint ReduceOutputRectMip0(
		kDefaultGroupSize * FMath::DivideAndRoundUp(Params.InputViewSize.X, kDefaultGroupSize),
		kDefaultGroupSize * FMath::DivideAndRoundUp(Params.InputViewSize.Y, kDefaultGroupSize));

	FIntPoint SrcSize = GetInputDesc(ePId_Input0)->Extent;

	FPostProcessDiaphragmDOFGatherCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FDDOFLayerProcessingDim>(Params.LayerProcessing);
	PermutationVector.Set<FDDOFGatherRingCountDim>(Params.RingCount);
	PermutationVector.Set<FDDOFGatherQualityDim>(Params.QualityConfig);
	PermutationVector.Set<FDDOFBokehSimulationDim>(Params.BokehSimulation);
	PermutationVector.Set<FDDOFClampInputUVDim>(ReduceOutputRectMip0 != SrcSize);
	PermutationVector.Set<FDDOFRGBColorBufferDim>(Params.bRGBBufferSeparateCocBuffer);
	PermutationVector = FPostProcessDiaphragmDOFGatherCS::RemapPermutation(PermutationVector);

	FDispatchDiaphragmDOFPass<FPostProcessDiaphragmDOFGatherCS, FRCPassDiaphragmDOFGather> DispatchCtx(
		this, Context, PermutationVector);
	DispatchCtx.DestViewport = FIntRect(FIntPoint::ZeroValue, Params.OutputViewSize);

	// Affine transformtation to control whether a CocRadius is considered or not.
	FVector2D ConsiderCocRadiusAffineTransformation0 = kContantlyPassingAffineTransformation;
	FVector2D ConsiderCocRadiusAffineTransformation1 = kContantlyPassingAffineTransformation;
	FVector2D ConsiderAbsCocRadiusAffineTransformation = kContantlyPassingAffineTransformation;
	{
		// Gathering scalability.
		const float GatheringScalingDownFactor = float(Params.InputViewSize.X) / float(Params.OutputViewSize.X);

		// Coc radius considered.
		const float RecombineCocRadiusBorder = GatheringScalingDownFactor * (kMaxSlightOutOfFocusRingCount - 1.0f);

		if (Params.LayerProcessing == EDiaphragmDOFLayerProcessing::ForegroundOnly)
		{
			ConsiderCocRadiusAffineTransformation0 = GenerateSaturatedAffineTransformation(
				-(RecombineCocRadiusBorder - 1.0f), -RecombineCocRadiusBorder);

			ConsiderAbsCocRadiusAffineTransformation = GenerateSaturatedAffineTransformation(
				RecombineCocRadiusBorder - 1.0f, RecombineCocRadiusBorder);
		}
		else if (Params.LayerProcessing == EDiaphragmDOFLayerProcessing::ForegroundHoleFilling)
		{
			ConsiderCocRadiusAffineTransformation0 = GenerateSaturatedAffineTransformation(
				RecombineCocRadiusBorder, RecombineCocRadiusBorder + 1.0f);
		}
		else if (Params.LayerProcessing == EDiaphragmDOFLayerProcessing::BackgroundOnly)
		{
			ConsiderCocRadiusAffineTransformation0 = GenerateSaturatedAffineTransformation(
				RecombineCocRadiusBorder - 1.0f, RecombineCocRadiusBorder);

			ConsiderAbsCocRadiusAffineTransformation = GenerateSaturatedAffineTransformation(
				RecombineCocRadiusBorder - 1.0f, RecombineCocRadiusBorder);
		}
		else if (Params.LayerProcessing == EDiaphragmDOFLayerProcessing::SlightOutOfFocus)
		{
			ConsiderAbsCocRadiusAffineTransformation = GenerateSaturatedAffineTransformation(
				RecombineCocRadiusBorder + GatheringScalingDownFactor * 1.0f, RecombineCocRadiusBorder);
		}
		else
		{
			checkf(0, TEXT("What layer processing is that?"));
		}
	}

	SCOPED_DRAW_EVENTF(Context.RHICmdList, DiaphragmDOFGather, TEXT("DiaphragmDOF Gather(%s %s Bokeh=%s Rings=%d%s%s) %dx%d"),
		GetEventName(Params.LayerProcessing),
		GetEventName(Params.QualityConfig),
		GetEventName(Params.BokehSimulation),
		Params.RingCount,
		PermutationVector.Get<FDDOFClampInputUVDim>() ? TEXT(" ClampUV") : TEXT(""),
		PermutationVector.Get<FDDOFRGBColorBufferDim>() ? TEXT(" R11G11B10") : TEXT(""),
		DispatchCtx.DestViewport.Width(), DispatchCtx.DestViewport.Height());

	{
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->TemporalJitterPixels, Context.View.TemporalJitterPixels);

		float MipBias = FMath::Log2(float(Params.InputViewSize.X) / float(Params.OutputViewSize.X));
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->MipBias, MipBias);

		FVector2D DispatchThreadIdToInputBufferUV(
			float(Params.InputViewSize.X) / float(Params.OutputViewSize.X * SrcSize.X),
			float(Params.InputViewSize.Y) / float(Params.OutputViewSize.Y * SrcSize.Y));
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->DispatchThreadIdToInputBufferUV, DispatchThreadIdToInputBufferUV);

		float MaxRecombineAbsCocRadius = 3.0 * float(Params.InputViewSize.X) / float(Params.OutputViewSize.X);
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->MaxRecombineAbsCocRadius, MaxRecombineAbsCocRadius);

		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->ConsiderCocRadiusAffineTransformation0, ConsiderCocRadiusAffineTransformation0);
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->ConsiderCocRadiusAffineTransformation1, ConsiderCocRadiusAffineTransformation1);
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->ConsiderAbsCocRadiusAffineTransformation, ConsiderAbsCocRadiusAffineTransformation);

		FVector2D InputBufferUVToOutputPixel(
			float(SrcSize.X * Params.OutputViewSize.X) / float(Params.InputViewSize.X),
			float(SrcSize.Y * Params.OutputViewSize.Y) / float(Params.InputViewSize.Y));
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->InputBufferUVToOutputPixel, InputBufferUVToOutputPixel);
	}
	DispatchCtx.Dispatch();
}

FPooledRenderTargetDesc FRCPassDiaphragmDOFGather::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	Ret.Extent = Params.OutputBufferSize;
	Ret.Format = PF_FloatRGBA;
	Ret.TargetableFlags |= TexCreate_RenderTargetable | TexCreate_UAV;
	Ret.NumMips = 1;

	if (Params.LayerProcessing == EDiaphragmDOFLayerProcessing::ForegroundOnly)
	{
		Ret.DebugName = TEXT("DOFGatherForeground");
	}
	else if (Params.LayerProcessing == EDiaphragmDOFLayerProcessing::ForegroundHoleFilling)
	{
		Ret.DebugName = TEXT("DOFGatherForegroundFill");
	}
	else if (Params.LayerProcessing == EDiaphragmDOFLayerProcessing::BackgroundOnly)
	{
		if (InPassOutputId == 0)
		{
			Ret.DebugName = TEXT("DOFGatherBackground");
		}
		else if (InPassOutputId == 2 && Params.QualityConfig == FRCPassDiaphragmDOFGather::EQualityConfig::HighQualityWithHybridScatterOcclusion)
		{
			Ret.DebugName = TEXT("DOFScatterOcclusionBackground");
			Ret.Format = PF_G16R16F;
		}
	}
	else if (Params.LayerProcessing == EDiaphragmDOFLayerProcessing::SlightOutOfFocus)
	{
		Ret.DebugName = TEXT("DOFGatherFocus");
	}
	else
	{
		Ret.DebugName = InPassOutputId == ePId_Output1 ? TEXT("DOFGatherBackground") : TEXT("DOFGatherForeground");
	}

	return Ret;
}


// ---------------------------------------------------- Postfilter

#define POST_FILTER_SHADER_PARAMS(PARAMETER) \
	PARAMETER(FShaderParameter, MaxInputBufferUV) \
	PARAMETER(FShaderParameter, MinGatherRadius) \

class FPostProcessDiaphragmDOFPostfilterCS : public FPostProcessDiaphragmDOFShader
{
	DECLARE_GLOBAL_SHADER(FPostProcessDiaphragmDOFPostfilterCS);
	SHADER_TYPE_PARAMETERS(FPostProcessDiaphragmDOFPostfilterCS, FPostProcessDiaphragmDOFShader, POST_FILTER_SHADER_PARAMS);

	class FTileOptimization : SHADER_PERMUTATION_BOOL("DIM_TILE_PERMUTATION");

	using FPermutationDomain = TShaderPermutationDomain<FDDOFLayerProcessingDim, FDDOFPostfilterMethodDim, FTileOptimization>;

	static FPermutationDomain RemapPermutationVector(FPermutationDomain PermutationVector)
	{
		// Tile permutation optimisation is only for Max3x3 post filtering.
		if (PermutationVector.Get<FDDOFPostfilterMethodDim>() != EDiaphragmDOFPostfilterMethod::RGBMax3x3)
			PermutationVector.Set<FTileOptimization>(false);
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (RemapPermutationVector(PermutationVector) != PermutationVector) return false;
		if (PermutationVector.Get<FDDOFPostfilterMethodDim>() == EDiaphragmDOFPostfilterMethod::None) return false;

		return FPostProcessDiaphragmDOFShader::ShouldCompilePermutation(Parameters);
	}
};

IMPLEMENT_GLOBAL_SHADER(FPostProcessDiaphragmDOFPostfilterCS, "/Engine/Private/DiaphragmDOF/DOFPostfiltering.usf", "PostfilterMainCS", SF_Compute);

void FRCPassDiaphragmDOFPostfilter::Process(FRenderingCompositePassContext& Context)
{
	FPostProcessDiaphragmDOFPostfilterCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FDDOFLayerProcessingDim>(Params.LayerProcessing);
	PermutationVector.Set<FDDOFPostfilterMethodDim>(Params.PostfilterMethod);
	PermutationVector.Set<FPostProcessDiaphragmDOFPostfilterCS::FTileOptimization>(
		GetInput(ePId_Input2)->GetOutput() != nullptr);
	PermutationVector = FPostProcessDiaphragmDOFPostfilterCS::RemapPermutationVector(PermutationVector);

	FDispatchDiaphragmDOFPass<FPostProcessDiaphragmDOFPostfilterCS, FRCPassDiaphragmDOFPostfilter> DispatchCtx(this, Context, PermutationVector);
	DispatchCtx.DestViewport = FIntRect(FIntPoint::ZeroValue, Params.OutputViewSize);

	SCOPED_DRAW_EVENTF(Context.RHICmdList, DiaphragmDOFPostfilter, TEXT("DiaphragmDOF Postfilter(%s %s%s) %dx%d"),
		GetEventName(Params.LayerProcessing), GetEventName(Params.PostfilterMethod),
		PermutationVector.Get<FPostProcessDiaphragmDOFPostfilterCS::FTileOptimization>() ? TEXT(" TileOptimisation") : TEXT(""),
		DispatchCtx.DestViewport.Width(), DispatchCtx.DestViewport.Height());

	{
		FIntPoint SrcSize = GetInputDesc(ePId_Input0)->Extent;
		FVector2D MaxInputBufferUV(
			(Params.OutputViewSize.X - 0.5f) / float(SrcSize.X),
			(Params.OutputViewSize.Y - 0.5f) / float(SrcSize.Y));
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->MaxInputBufferUV, MaxInputBufferUV);

		float MaxRecombineAbsCocRadius = 3.0 * float(Params.InputViewSize.X) / float(Params.OutputViewSize.X);
		float MinGatherRadius = MaxRecombineAbsCocRadius - 1;
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->MinGatherRadius, MinGatherRadius);
	}
	DispatchCtx.Dispatch();
}

FPooledRenderTargetDesc FRCPassDiaphragmDOFPostfilter::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	Ret.Format = PF_FloatRGBA;
	Ret.TargetableFlags |= TexCreate_UAV;
	Ret.Flags &= ~(TexCreate_FastVRAM);
	Ret.Flags |= GFastVRamConfig.DOFPostfilter;
	return Ret;
}


// ---------------------------------------------------- Build bokeh.

#define BUILD_BOKEH_LUT_SHADER_PARAMS(PARAMETER) \
	PARAMETER(FShaderParameter, BladeCount) \
	PARAMETER(FShaderParameter, DiaphragmRotation) \
	PARAMETER(FShaderParameter, CocRadiusToCircumscribedRadius) \
	PARAMETER(FShaderParameter, CocRadiusToIncircleRadius) \
	PARAMETER(FShaderParameter, DiaphragmBladeRadius) \
	PARAMETER(FShaderParameter, DiaphragmBladeCenterOffset) \

class FPostProcessDiaphragmDOFBuildBokehLUTCS : public FPostProcessDiaphragmDOFShader
{
	DECLARE_GLOBAL_SHADER(FPostProcessDiaphragmDOFBuildBokehLUTCS);
	SHADER_TYPE_PARAMETERS(FPostProcessDiaphragmDOFBuildBokehLUTCS, FPostProcessDiaphragmDOFShader, BUILD_BOKEH_LUT_SHADER_PARAMS);

	class FBokehSimulationDim : SHADER_PERMUTATION_BOOL("DIM_ROUND_BLADES");
	class FLUTFormatDim : SHADER_PERMUTATION_ENUM_CLASS("DIM_LUT_FORMAT", FRCPassDiaphragmDOFBuildBokehLUT::EFormat);

	using FPermutationDomain = TShaderPermutationDomain<FBokehSimulationDim, FLUTFormatDim>;
};

IMPLEMENT_GLOBAL_SHADER(FPostProcessDiaphragmDOFBuildBokehLUTCS, "/Engine/Private/DiaphragmDOF/DOFBokehLUT.usf", "BuildBokehLUTMainCS", SF_Compute);

void FRCPassDiaphragmDOFBuildBokehLUT::Process(FRenderingCompositePassContext& Context)
{
	FPostProcessDiaphragmDOFBuildBokehLUTCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FPostProcessDiaphragmDOFBuildBokehLUTCS::FBokehSimulationDim>(BokehModel.BokehShape == DiaphragmDOF::EBokehShape::RoundedBlades);
	PermutationVector.Set<FPostProcessDiaphragmDOFBuildBokehLUTCS::FLUTFormatDim>(Format);

	FDispatchDiaphragmDOFPass<FPostProcessDiaphragmDOFBuildBokehLUTCS, FRCPassDiaphragmDOFBuildBokehLUT> DispatchCtx(this, Context, PermutationVector);
	DispatchCtx.DestViewport = FIntRect(0, 0, kLUTSize, kLUTSize);

	SCOPED_DRAW_EVENTF(Context.RHICmdList, BuildBokehLUT, TEXT("DiaphragmDOF BuildBokehLUT(Blades=%d, Shape=%s, LUT=%s) %dx%d"),
		BokehModel.DiaphragmBladeCount,
		BokehModel.BokehShape == DiaphragmDOF::EBokehShape::RoundedBlades ? TEXT("Rounded") : TEXT("Straight"),
		GetEventName(Format),
		DispatchCtx.DestViewport.Width(), DispatchCtx.DestViewport.Height());

		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->BladeCount, BokehModel.DiaphragmBladeCount);
	SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->CocRadiusToCircumscribedRadius, BokehModel.CocRadiusToCircumscribedRadius);
	SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->CocRadiusToIncircleRadius, BokehModel.CocRadiusToIncircleRadius);
	SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->DiaphragmRotation, BokehModel.DiaphragmRotation);
	SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->DiaphragmBladeRadius,
		BokehModel.RoundedBlades.DiaphragmBladeRadius);
	SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->DiaphragmBladeCenterOffset,
		BokehModel.RoundedBlades.DiaphragmBladeCenterOffset);
	DispatchCtx.Dispatch();
}

FPooledRenderTargetDesc FRCPassDiaphragmDOFBuildBokehLUT::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	static const TCHAR* const kDebugNames[] = {
		TEXT("DOFScatterBokehLUT"),
		TEXT("DOFRecombineBokehLUT"),
		TEXT("DOFGatherBokehLUT"),
	};

	FPooledRenderTargetDesc Ret;
	Ret.NumMips = 1;
	Ret.Format = Format == EFormat::GatherSamplePos ? PF_G16R16F : PF_R16F;
	Ret.Extent = FIntPoint(kLUTSize, kLUTSize);
	Ret.DebugName = kDebugNames[int32(Format)];
	Ret.TargetableFlags |= TexCreate_UAV;
	return Ret;
}


// ---------------------------------------------------- Scatter

#define HYBRID_SCATTER_PARAMS(PARAMETER) \
	PARAMETER(FShaderParameter, CocRadiusToCircumscribedRadius) \
	PARAMETER(FShaderParameter, ScatteringScaling) \
	PARAMETER(FShaderResourceParameter, ScatterDrawList) \

class FPostProcessDiaphragmDOFHybridScatterVS : public FPostProcessDiaphragmDOFShader
{
	DECLARE_GLOBAL_SHADER(FPostProcessDiaphragmDOFHybridScatterVS);
	SHADER_TYPE_PARAMETERS(FPostProcessDiaphragmDOFHybridScatterVS, FPostProcessDiaphragmDOFShader, HYBRID_SCATTER_PARAMS);

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!FRCPassDiaphragmDOFHybridScatter::IsSupported(Parameters.Platform))
		{
			return false;
		}
		return FPostProcessDiaphragmDOFShader::ShouldCompilePermutation(Parameters);
	}
};

class FPostProcessDiaphragmDOFHybridScatterPS : public FPostProcessDiaphragmDOFShader
{
	DECLARE_GLOBAL_SHADER(FPostProcessDiaphragmDOFHybridScatterPS);
	SHADER_TYPE_PARAMETERS(FPostProcessDiaphragmDOFHybridScatterPS, FPostProcessDiaphragmDOFShader, HYBRID_SCATTER_PARAMS);

	class FBokehSimulationDim : SHADER_PERMUTATION_BOOL("DIM_BOKEH_SIMULATION");

	using FPermutationDomain = TShaderPermutationDomain<FDDOFLayerProcessingDim, FBokehSimulationDim, FDDOFScatterOcclusionDim>;

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		// Pixel shader are exactly the same between foreground and background when there is no bokeh LUT.
		if (PermutationVector.Get<FDDOFLayerProcessingDim>() == EDiaphragmDOFLayerProcessing::BackgroundOnly && 
			!PermutationVector.Get<FBokehSimulationDim>())
		{
			PermutationVector.Set<FDDOFLayerProcessingDim>(EDiaphragmDOFLayerProcessing::ForegroundOnly);
		}

		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!FRCPassDiaphragmDOFHybridScatter::IsSupported(Parameters.Platform))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// Do not compile this permutation if it gets remapped at runtime.
		if (RemapPermutation(PermutationVector) != PermutationVector)
		{
			return false;
		}

		if (PermutationVector.Get<FDDOFLayerProcessingDim>() != EDiaphragmDOFLayerProcessing::ForegroundOnly &&
			PermutationVector.Get<FDDOFLayerProcessingDim>() != EDiaphragmDOFLayerProcessing::BackgroundOnly) return false;

		return FPostProcessDiaphragmDOFShader::ShouldCompilePermutation(Parameters);
	}
};

IMPLEMENT_GLOBAL_SHADER(
	FPostProcessDiaphragmDOFHybridScatterVS, "/Engine/Private/DiaphragmDOF/DOFHybridScatterVertexShader.usf",
	"ScatterMainVS", SF_Vertex);

IMPLEMENT_GLOBAL_SHADER(
	FPostProcessDiaphragmDOFHybridScatterPS, "/Engine/Private/DiaphragmDOF/DOFHybridScatterPixelShader.usf",
	"ScatterMainPS", SF_Pixel);

void FRCPassDiaphragmDOFHybridScatter::Process(FRenderingCompositePassContext& Context)
{
	bool bIsForeground = Params.LayerProcessing == EDiaphragmDOFLayerProcessing::ForegroundOnly;

	FPostProcessDiaphragmDOFHybridScatterPS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FDDOFLayerProcessingDim>(bIsForeground ? EDiaphragmDOFLayerProcessing::ForegroundOnly : EDiaphragmDOFLayerProcessing::BackgroundOnly);
	PermutationVector.Set<FPostProcessDiaphragmDOFHybridScatterPS::FBokehSimulationDim>(GetInput(ePId_Input2)->GetOutput() ? true : false);
	PermutationVector.Set<FDDOFScatterOcclusionDim>(GetInput(ePId_Input3)->GetOutput() ? true : false);
	PermutationVector = FPostProcessDiaphragmDOFHybridScatterPS::RemapPermutation(PermutationVector);

	TShaderMapRef<FPostProcessDiaphragmDOFHybridScatterVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessDiaphragmDOFHybridScatterPS> PixelShader(Context.GetShaderMap(), PermutationVector);

	int32 DrawIndirectParametersOffset = 0;
	FRWBuffer* DrawIndirectParametersBuffer = &GDiaphragmDOFGlobalResource.DrawIndirectParametersBuffer;
	FRWBufferStructured* ScatterDrawListBuffer = nullptr;
	if (bIsForeground)
	{
		ScatterDrawListBuffer = &GDiaphragmDOFGlobalResource.ForegroundScatterDrawListBuffer;
	}
	else
	{
		DrawIndirectParametersOffset = 1;
		ScatterDrawListBuffer = &GDiaphragmDOFGlobalResource.BackgroundScatterDrawListBuffer;
	}

	PassOutputs[ePId_Output0].PooledRenderTarget = GetInput(ePId_Input0)->GetOutput()->PooledRenderTarget;

	FSceneRenderTargetItem& DestRenderTarget = PassOutputs[ePId_Output0].PooledRenderTarget->GetRenderTargetItem();
	FIntRect DestViewport = FIntRect(FIntPoint::ZeroValue, Params.OutputViewSize);;

	EPrimitiveType PrimitiveType = GRHISupportsRectTopology ? PT_RectList : PT_TriangleList;

	Context.RHICmdList.TransitionResource(
		EResourceTransitionAccess::ERWBarrier,
		EResourceTransitionPipeline::EComputeToGfx,
		DestRenderTarget.UAV);

	FRHIRenderPassInfo RPInfo(DestRenderTarget.TargetableTexture, ERenderTargetActions::Load_Store);
	Context.RHICmdList.BeginRenderPass(RPInfo, TEXT("DOFHybridScatter"));
	{
		Context.SetViewportAndCallRHI(DestViewport, 0.0f, 1.0f);

		SCOPED_DRAW_EVENTF(Context.RHICmdList, DiaphragmDOFIndirectScatter, TEXT("DiaphragmDOF IndirectScatter(%s Bokeh=%s Occlusion=%s 1/2) %dx%d"),
			GetEventName(bIsForeground ? EDiaphragmDOFLayerProcessing::ForegroundOnly : EDiaphragmDOFLayerProcessing::BackgroundOnly),
			PermutationVector.Get<FPostProcessDiaphragmDOFHybridScatterPS::FBokehSimulationDim>() ? TEXT("Generic") : TEXT("None"),
			PermutationVector.Get<FDDOFScatterOcclusionDim>() ? TEXT("Yes") : TEXT("No"),
			DestViewport.Width(), DestViewport.Height());

		{
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PrimitiveType;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);
		}

		float ScatteringScaling = float(Params.OutputViewSize.X) / float(Params.InputViewSize.X);

		{
			FVertexShaderRHIParamRef ShaderRHI = VertexShader->GetVertexShader();

			SetShaderValue(Context.RHICmdList, ShaderRHI, VertexShader->CocRadiusToCircumscribedRadius,
				BokehModel.CocRadiusToCircumscribedRadius);

			SetSRVParameter(Context.RHICmdList, ShaderRHI, VertexShader->ScatterDrawList,
				ScatterDrawListBuffer->SRV);

			VertexShader->PostprocessParameter.SetVS(ShaderRHI, Context);

			SetShaderValue(Context.RHICmdList, ShaderRHI, VertexShader->ScatteringScaling, ScatteringScaling);
		}

		{
			FPixelShaderRHIParamRef ShaderRHI = PixelShader->GetPixelShader();
			PixelShader->PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context);
			PixelShader->SetParameters<FViewUniformShaderParameters>(
				Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

			SetShaderValue(Context.RHICmdList, ShaderRHI, PixelShader->ScatteringScaling, ScatteringScaling);
		}

		Context.RHICmdList.SetStreamSource(0, NULL, 0);

		if (GRHISupportsRectTopology)
		{
			Context.RHICmdList.DrawPrimitiveIndirect(
				DrawIndirectParametersBuffer->Buffer,
				sizeof(FRHIDrawIndirectParameters) * DrawIndirectParametersOffset);
		}
		else
		{
			Context.RHICmdList.DrawIndexedPrimitiveIndirect(
				GDiaphragmDOFGlobalResource.ScatterIndexBuffer.IndexBufferRHI,
				DrawIndirectParametersBuffer->Buffer,
				sizeof(FRHIDrawIndexedIndirectParameters) * DrawIndirectParametersOffset);
		}
	}
	Context.RHICmdList.EndRenderPass();
	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, FResolveParams());

	{
		FVertexShaderRHIParamRef ShaderRHI = VertexShader->GetVertexShader();

		SetSRVParameter(Context.RHICmdList, ShaderRHI, VertexShader->ScatterDrawList, nullptr);
	}

	Context.RHICmdList.TransitionResource(
		EResourceTransitionAccess::EReadable,
		EResourceTransitionPipeline::EGfxToCompute,
		DestRenderTarget.UAV);
}

FPooledRenderTargetDesc FRCPassDiaphragmDOFHybridScatter::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	Ret.DebugName = true ? TEXT("DOFHybridScatterBgd") : TEXT("DOFHybridScatterFgd");
	Ret.TargetableFlags |= TexCreate_UAV;
	return Ret;
}


// ---------------------------------------------------- Recombine

#define SHADER_PARAMS(PARAMETER) \
	PARAMETER(FSceneTextureShaderParameters, SceneTextureParameters) \
	PARAMETER(FShaderParameter, TemporalJitterPixels) \
	PARAMETER(FShaderParameter, CocModelParameters) \
	PARAMETER(FShaderParameter, DepthBlurParameters) \
	PARAMETER(FShaderParameter, DOFBufferUVMax) \

class FPostProcessDiaphragmDOFRecombineCS : public FPostProcessDiaphragmDOFShader
{
	DECLARE_GLOBAL_SHADER(FPostProcessDiaphragmDOFRecombineCS);
	SHADER_TYPE_PARAMETERS(FPostProcessDiaphragmDOFRecombineCS, FPostProcessDiaphragmDOFShader, SHADER_PARAMS);
	#undef SHADER_PARAMS

	class FQualityDim : SHADER_PERMUTATION_INT("DIM_QUALITY", 3);

	using FPermutationDomain = TShaderPermutationDomain<FDDOFLayerProcessingDim, FDDOFBokehSimulationDim, FQualityDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FDDOFLayerProcessingDim>() != EDiaphragmDOFLayerProcessing::ForegroundOnly &&
		    PermutationVector.Get<FDDOFLayerProcessingDim>() != EDiaphragmDOFLayerProcessing::BackgroundOnly &&
			PermutationVector.Get<FDDOFLayerProcessingDim>() != EDiaphragmDOFLayerProcessing::ForegroundAndBackground) return false;

		// Do not compile bokeh simulation shaders on platform that couldn't handle them anyway.
		if (!FRCPassDiaphragmDOFGather::SupportsBokehSimmulation(Parameters.Platform) &&
			PermutationVector.Get<FDDOFBokehSimulationDim>() != EDiaphragmDOFBokehSimulation::Disabled) return false;

		return FPostProcessDiaphragmDOFShader::ShouldCompilePermutation(Parameters);
	}
};

IMPLEMENT_GLOBAL_SHADER(FPostProcessDiaphragmDOFRecombineCS, "/Engine/Private/DiaphragmDOF/DOFRecombine.usf", "RecombineMainCS", SF_Compute);

void FRCPassDiaphragmDOFRecombine::Process(FRenderingCompositePassContext& Context)
{
	FPostProcessDiaphragmDOFRecombineCS::FPermutationDomain PermutationVector;
	if (!GetInput(ePId_Input3)->GetOutput())
	{
		PermutationVector.Set<FDDOFLayerProcessingDim>(EDiaphragmDOFLayerProcessing::BackgroundOnly);
	}
	else if (!GetInput(ePId_Input7)->GetOutput())
	{
		PermutationVector.Set<FDDOFLayerProcessingDim>(EDiaphragmDOFLayerProcessing::ForegroundOnly);
	}
	else
	{
		check(GetInput(ePId_Input3)->GetOutput() && GetInput(ePId_Input7)->GetOutput());
		PermutationVector.Set<FDDOFLayerProcessingDim>(EDiaphragmDOFLayerProcessing::ForegroundAndBackground);
	}
	PermutationVector.Set<FDDOFBokehSimulationDim>(Params.BokehSimulation);
	PermutationVector.Set<FPostProcessDiaphragmDOFRecombineCS::FQualityDim>(Params.Quality);

	FDispatchDiaphragmDOFPass<FPostProcessDiaphragmDOFRecombineCS, FRCPassDiaphragmDOFRecombine> DispatchCtx(this, Context, PermutationVector);
	DispatchCtx.DestViewport = Context.View.ViewRect;

	SCOPED_DRAW_EVENTF(Context.RHICmdList, DiaphragmDOFRecombine, TEXT("DiaphragmDOF Recombine(%s Quality=%d Bokeh=%s alpha=no) %dx%d"),
		GetEventName(PermutationVector.Get<FDDOFLayerProcessingDim>()),
		Params.Quality,
		GetEventName(Params.BokehSimulation),
		DispatchCtx.DestViewport.Width(), DispatchCtx.DestViewport.Height());

	{
		DispatchCtx->SceneTextureParameters.Set(Context.RHICmdList, DispatchCtx.ShaderRHI, Context.View.FeatureLevel, ESceneTextureSetupMode::All);
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->TemporalJitterPixels, Context.View.TemporalJitterPixels);

		// TODO: Stop full <-> half res conversion in Recombine pass's gathering kernel.
		SetCocModelParameters(Context, DispatchCtx, Params.CocModel, /* CocRadiusBasis = */ DispatchCtx.DestViewport.Width() * 0.5f);

		FIntPoint DOFGatherBufferSize = GetInput(ePId_Input3)->GetOutput() ? GetInputDesc(ePId_Input3)->Extent : GetInputDesc(ePId_Input7)->Extent;
		FVector2D DOFBufferUVMax;
		DOFBufferUVMax.X = (Params.GatheringViewSize.X - 0.5f) / float(DOFGatherBufferSize.X);
		DOFBufferUVMax.Y = (Params.GatheringViewSize.Y - 0.5f) / float(DOFGatherBufferSize.Y);
		SetShaderValue(Context.RHICmdList, DispatchCtx.ShaderRHI, DispatchCtx->DOFBufferUVMax, DOFBufferUVMax);
	}

	DispatchCtx.Dispatch();

	STOP_DRAW_EVENT(*Params.MainDrawEvent);
}

FPooledRenderTargetDesc FRCPassDiaphragmDOFRecombine::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	// Reset so that the number of samples of descriptor becomes 1, which is totally legal still with MSAA because
	// the scene color will already be resolved to ShaderResource texture that is always 1. This is to work around
	// hack that MSAA will have targetable texture with MSAA != shader resource, and still have descriptor indicating
	// the number of samples of the targetable resource.
	Ret.Reset();

	Ret.DebugName = TEXT("DOFRecombine");
	Ret.TargetableFlags |= TexCreate_UAV;
	return Ret;
}
