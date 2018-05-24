// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldScreenGridLighting.cpp
=============================================================================*/

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "ShaderParameters.h"
#include "RendererInterface.h"
#include "RHIStaticStates.h"
#include "GlobalDistanceFieldParameters.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "SceneRenderTargetParameters.h"
#include "DeferredShadingRenderer.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "DistanceFieldLightingShared.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "DistanceFieldLightingPost.h"
#include "GlobalDistanceField.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"

int32 GAOUseJitter = 1;
FAutoConsoleVariableRef CVarAOUseJitter(
	TEXT("r.AOUseJitter"),
	GAOUseJitter,
	TEXT("Whether to use 4x temporal supersampling with Screen Grid DFAO.  When jitter is disabled, a shorter history can be used but there will be more spatial aliasing."),
	ECVF_RenderThreadSafe
	);

int32 GConeTraceDownsampleFactor = 4;

FIntPoint GetBufferSizeForConeTracing()
{
	return FIntPoint::DivideAndRoundDown(GetBufferSizeForAO(), GConeTraceDownsampleFactor);
}

FVector2D JitterOffsets[4] = 
{
	FVector2D(.25f, 0),
	FVector2D(.75f, .25f),
	FVector2D(.5f, .75f),
	FVector2D(0, .5f)
};

extern int32 GAOUseHistory;

FVector2D GetJitterOffset(int32 SampleIndex)
{
	if (GAOUseJitter && GAOUseHistory)
	{
		return JitterOffsets[SampleIndex] * GConeTraceDownsampleFactor;
	}

	return FVector2D(0, 0);
}

void FAOScreenGridResources::InitDynamicRHI()
{
	//@todo - 2d textures
	const uint32 FastVRamFlag = GFastVRamConfig.DistanceFieldAOScreenGridResources | (IsTransientResourceBufferAliasingEnabled() ? BUF_Transient : BUF_None);
	ScreenGridConeVisibility.Initialize(sizeof(uint32), NumConeSampleDirections * ScreenGridDimensions.X * ScreenGridDimensions.Y, PF_R32_UINT, BUF_Static | FastVRamFlag, TEXT("ScreenGridConeVisibility"));

	if (bAllocateResourceForGI)
	{
		ConeDepthVisibilityFunction.Initialize(sizeof(float), NumConeSampleDirections * NumVisibilitySteps * ScreenGridDimensions.X * ScreenGridDimensions.Y, PF_R32_FLOAT, BUF_Static);
		//@todo - fp16
		StepBentNormal.Initialize(sizeof(float) * 4, NumVisibilitySteps * ScreenGridDimensions.X * ScreenGridDimensions.Y, PF_A32B32G32R32F, BUF_Static);
		SurfelIrradiance.Initialize(sizeof(FFloat16Color), ScreenGridDimensions.X * ScreenGridDimensions.Y, PF_FloatRGBA, BUF_Static | FastVRamFlag, TEXT("SurfelIrradiance"));
		HeightfieldIrradiance.Initialize(sizeof(FFloat16Color), ScreenGridDimensions.X * ScreenGridDimensions.Y, PF_FloatRGBA, BUF_Static | FastVRamFlag, TEXT("HeightfieldIrradiance"));
	}
}

template<bool bSupportIrradiance, bool bUseGlobalDistanceField>
class TConeTraceScreenGridObjectOcclusionCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TConeTraceScreenGridObjectOcclusionCS,Global)
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FTileIntersectionParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance);
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_DISTANCE_FIELD"), bUseGlobalDistanceField);

		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	TConeTraceScreenGridObjectOcclusionCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer);
		ObjectParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		ScreenGridParameters.Bind(Initializer.ParameterMap);
		GlobalDistanceFieldParameters.Bind(Initializer.ParameterMap);
		TileConeDepthRanges.Bind(Initializer.ParameterMap, TEXT("TileConeDepthRanges"));
		TileIntersectionParameters.Bind(Initializer.ParameterMap);
		TanConeHalfAngle.Bind(Initializer.ParameterMap, TEXT("TanConeHalfAngle"));
		BentNormalNormalizeFactor.Bind(Initializer.ParameterMap, TEXT("BentNormalNormalizeFactor"));
		ScreenGridConeVisibility.Bind(Initializer.ParameterMap, TEXT("ScreenGridConeVisibility"));
		ConeDepthVisibilityFunction.Bind(Initializer.ParameterMap, TEXT("ConeDepthVisibilityFunction"));
	}

	TConeTraceScreenGridObjectOcclusionCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		FSceneRenderTargetItem& DistanceFieldNormal, 
		const FDistanceFieldAOParameters& Parameters,
		const FGlobalDistanceFieldInfo& GlobalDistanceFieldInfo)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		SceneTextureParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, ESceneTextureSetupMode::All);
		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		ScreenGridParameters.Set(RHICmdList, ShaderRHI, View, DistanceFieldNormal);

		if (bUseGlobalDistanceField)
		{
			GlobalDistanceFieldParameters.Set(RHICmdList, ShaderRHI, GlobalDistanceFieldInfo.ParameterData);
		}

		FAOSampleData2 AOSampleData;

		TArray<FVector, TInlineAllocator<9> > SampleDirections;
		GetSpacedVectors(View.Family->FrameNumber, SampleDirections);

		for (int32 SampleIndex = 0; SampleIndex < NumConeSampleDirections; SampleIndex++)
		{
			AOSampleData.SampleDirections[SampleIndex] = FVector4(SampleDirections[SampleIndex]);
		}

		SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, GetUniformBufferParameter<FAOSampleData2>(), AOSampleData);

		FTileIntersectionResources* TileIntersectionResources = View.ViewState->AOTileIntersectionResources;
		SetSRVParameter(RHICmdList, ShaderRHI, TileConeDepthRanges, TileIntersectionResources->TileConeDepthRanges.SRV);

		TileIntersectionParameters.Set(RHICmdList, ShaderRHI, *TileIntersectionResources);

		extern float GAOConeHalfAngle;
		SetShaderValue(RHICmdList, ShaderRHI, TanConeHalfAngle, FMath::Tan(GAOConeHalfAngle));

		FVector UnoccludedVector(0);

		for (int32 SampleIndex = 0; SampleIndex < NumConeSampleDirections; SampleIndex++)
		{
			UnoccludedVector += SampleDirections[SampleIndex];
		}

		float BentNormalNormalizeFactorValue = 1.0f / (UnoccludedVector / NumConeSampleDirections).Size();
		SetShaderValue(RHICmdList, ShaderRHI, BentNormalNormalizeFactor, BentNormalNormalizeFactorValue);

		FAOScreenGridResources* ScreenGridResources = View.ViewState->AOScreenGridResources;

		ScreenGridConeVisibility.SetBuffer(RHICmdList, ShaderRHI, ScreenGridResources->ScreenGridConeVisibility);
		if (bSupportIrradiance)
		{
			ConeDepthVisibilityFunction.SetBuffer(RHICmdList, ShaderRHI, ScreenGridResources->ConeDepthVisibilityFunction);
		}
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		ScreenGridConeVisibility.UnsetUAV(RHICmdList, GetComputeShader());
		ConeDepthVisibilityFunction.UnsetUAV(RHICmdList, GetComputeShader());
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		Ar << ObjectParameters;
		Ar << AOParameters;
		Ar << ScreenGridParameters;
		Ar << GlobalDistanceFieldParameters;
		Ar << TileConeDepthRanges;
		Ar << TileIntersectionParameters;
		Ar << TanConeHalfAngle;
		Ar << BentNormalNormalizeFactor;
		Ar << ScreenGridConeVisibility;
		Ar << ConeDepthVisibilityFunction;
		return bShaderHasOutdatedParameters;
	}

private:

	FSceneTextureShaderParameters SceneTextureParameters;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FAOParameters AOParameters;
	FScreenGridParameters ScreenGridParameters;
	FGlobalDistanceFieldParameters GlobalDistanceFieldParameters;
	FShaderResourceParameter TileConeDepthRanges;
	FTileIntersectionParameters TileIntersectionParameters;
	FShaderParameter TanConeHalfAngle;
	FShaderParameter BentNormalNormalizeFactor;
	FRWShaderParameter ScreenGridConeVisibility;
	FRWShaderParameter ConeDepthVisibilityFunction;
};

#define IMPLEMENT_CONETRACE_CS_TYPE(bSupportIrradiance, bUseGlobalDistanceField) \
	typedef TConeTraceScreenGridObjectOcclusionCS<bSupportIrradiance, bUseGlobalDistanceField> TConeTraceScreenGridObjectOcclusionCS##bSupportIrradiance##bUseGlobalDistanceField; \
	IMPLEMENT_SHADER_TYPE(template<>,TConeTraceScreenGridObjectOcclusionCS##bSupportIrradiance##bUseGlobalDistanceField,TEXT("/Engine/Private/DistanceFieldScreenGridLighting.usf"),TEXT("ConeTraceObjectOcclusionCS"),SF_Compute);

IMPLEMENT_CONETRACE_CS_TYPE(true, true)
IMPLEMENT_CONETRACE_CS_TYPE(false, true)
IMPLEMENT_CONETRACE_CS_TYPE(true, false)
IMPLEMENT_CONETRACE_CS_TYPE(false, false)

const int32 GConeTraceGlobalDFTileSize = 8;

template<bool bConeTraceObjects>
class TConeTraceScreenGridGlobalOcclusionCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TConeTraceScreenGridGlobalOcclusionCS,Global)
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("CONE_TRACE_OBJECTS"), bConeTraceObjects);
		OutEnvironment.SetDefine(TEXT("CONE_TRACE_GLOBAL_DISPATCH_SIZEX"), GConeTraceGlobalDFTileSize);
		OutEnvironment.SetDefine(TEXT("OUTPUT_VISIBILITY_DIRECTLY"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT("TRACE_DOWNSAMPLE_FACTOR"), GConeTraceDownsampleFactor);
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_DISTANCE_FIELD"), TEXT("1"));

		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	TConeTraceScreenGridGlobalOcclusionCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer);
		ObjectParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		ScreenGridParameters.Bind(Initializer.ParameterMap);
		GlobalDistanceFieldParameters.Bind(Initializer.ParameterMap);
		TileConeDepthRanges.Bind(Initializer.ParameterMap, TEXT("TileConeDepthRanges"));
		TileListGroupSize.Bind(Initializer.ParameterMap, TEXT("TileListGroupSize"));
		TanConeHalfAngle.Bind(Initializer.ParameterMap, TEXT("TanConeHalfAngle"));
		BentNormalNormalizeFactor.Bind(Initializer.ParameterMap, TEXT("BentNormalNormalizeFactor"));
		ScreenGridConeVisibility.Bind(Initializer.ParameterMap, TEXT("ScreenGridConeVisibility"));
	}

	TConeTraceScreenGridGlobalOcclusionCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		FIntPoint TileListGroupSizeValue, 
		FSceneRenderTargetItem& DistanceFieldNormal, 
		const FDistanceFieldAOParameters& Parameters,
		const FGlobalDistanceFieldInfo& GlobalDistanceFieldInfo)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		SceneTextureParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, ESceneTextureSetupMode::All);
		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		ScreenGridParameters.Set(RHICmdList, ShaderRHI, View, DistanceFieldNormal);
		GlobalDistanceFieldParameters.Set(RHICmdList, ShaderRHI, GlobalDistanceFieldInfo.ParameterData);

		FAOSampleData2 AOSampleData;

		TArray<FVector, TInlineAllocator<9> > SampleDirections;
		GetSpacedVectors(View.Family->FrameNumber, SampleDirections);

		for (int32 SampleIndex = 0; SampleIndex < NumConeSampleDirections; SampleIndex++)
		{
			AOSampleData.SampleDirections[SampleIndex] = FVector4(SampleDirections[SampleIndex]);
		}

		SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, GetUniformBufferParameter<FAOSampleData2>(), AOSampleData);

		if (UseAOObjectDistanceField())
		{
			FTileIntersectionResources* TileIntersectionResources = View.ViewState->AOTileIntersectionResources;
			SetSRVParameter(RHICmdList, ShaderRHI, TileConeDepthRanges, TileIntersectionResources->TileConeDepthRanges.SRV);
		}

		SetShaderValue(RHICmdList, ShaderRHI, TileListGroupSize, TileListGroupSizeValue);

		extern float GAOConeHalfAngle;
		SetShaderValue(RHICmdList, ShaderRHI, TanConeHalfAngle, FMath::Tan(GAOConeHalfAngle));

		FVector UnoccludedVector(0);

		for (int32 SampleIndex = 0; SampleIndex < NumConeSampleDirections; SampleIndex++)
		{
			UnoccludedVector += SampleDirections[SampleIndex];
		}

		float BentNormalNormalizeFactorValue = 1.0f / (UnoccludedVector / NumConeSampleDirections).Size();
		SetShaderValue(RHICmdList, ShaderRHI, BentNormalNormalizeFactor, BentNormalNormalizeFactorValue);

		FAOScreenGridResources* ScreenGridResources = View.ViewState->AOScreenGridResources;

		int32 NumOutUAVs = 0;
		FUnorderedAccessViewRHIParamRef OutUAVs[1];
		OutUAVs[NumOutUAVs++] = ScreenGridResources->ScreenGridConeVisibility.UAV;

		// Note: no transition, want to overlap object cone tracing and global DF cone tracing since both shaders use atomics to ScreenGridConeVisibility
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWNoBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, NumOutUAVs);

		ScreenGridConeVisibility.SetBuffer(RHICmdList, ShaderRHI, ScreenGridResources->ScreenGridConeVisibility);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		ScreenGridConeVisibility.UnsetUAV(RHICmdList, GetComputeShader());
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		Ar << ObjectParameters;
		Ar << AOParameters;
		Ar << ScreenGridParameters;
		Ar << GlobalDistanceFieldParameters;
		Ar << TileConeDepthRanges;
		Ar << TileListGroupSize;
		Ar << TanConeHalfAngle;
		Ar << BentNormalNormalizeFactor;
		Ar << ScreenGridConeVisibility;
		return bShaderHasOutdatedParameters;
	}

private:

	FSceneTextureShaderParameters SceneTextureParameters;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FAOParameters AOParameters;
	FScreenGridParameters ScreenGridParameters;
	FGlobalDistanceFieldParameters GlobalDistanceFieldParameters;
	FShaderResourceParameter TileConeDepthRanges;
	FShaderParameter TileListGroupSize;
	FShaderParameter TanConeHalfAngle;
	FShaderParameter BentNormalNormalizeFactor;
	FRWShaderParameter ScreenGridConeVisibility;
};

#define IMPLEMENT_CONETRACE_GLOBAL_CS_TYPE(bSupportIrradiance) \
	typedef TConeTraceScreenGridGlobalOcclusionCS<bSupportIrradiance> TConeTraceScreenGridGlobalOcclusionCS##bSupportIrradiance; \
	IMPLEMENT_SHADER_TYPE(template<>,TConeTraceScreenGridGlobalOcclusionCS##bSupportIrradiance,TEXT("/Engine/Private/DistanceFieldScreenGridLighting.usf"),TEXT("ConeTraceGlobalOcclusionCS"),SF_Compute);

IMPLEMENT_CONETRACE_GLOBAL_CS_TYPE(true)
IMPLEMENT_CONETRACE_GLOBAL_CS_TYPE(false)



const int32 GCombineConesSizeX = 8;

class FCombineConeVisibilityCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCombineConeVisibilityCS,Global)
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("COMBINE_CONES_SIZEX"), GCombineConesSizeX);
		OutEnvironment.SetDefine(TEXT("TRACE_DOWNSAMPLE_FACTOR"), GConeTraceDownsampleFactor);

		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FCombineConeVisibilityCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ScreenGridParameters.Bind(Initializer.ParameterMap);
		BentNormalNormalizeFactor.Bind(Initializer.ParameterMap, TEXT("BentNormalNormalizeFactor"));
		ScreenGridConeVisibility.Bind(Initializer.ParameterMap, TEXT("ScreenGridConeVisibility"));
		DistanceFieldBentNormal.Bind(Initializer.ParameterMap, TEXT("DistanceFieldBentNormal"));
		ConeBufferMax.Bind(Initializer.ParameterMap, TEXT("ConeBufferMax"));
		DFNormalBufferUVMax.Bind(Initializer.ParameterMap, TEXT("DFNormalBufferUVMax"));
	}

	FCombineConeVisibilityCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		FSceneRenderTargetItem& DistanceFieldNormal, 
		FSceneRenderTargetItem& DownsampledBentNormal)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		ScreenGridParameters.Set(RHICmdList, ShaderRHI, View, DistanceFieldNormal);

		FAOSampleData2 AOSampleData;

		TArray<FVector, TInlineAllocator<9> > SampleDirections;
		GetSpacedVectors(View.Family->FrameNumber, SampleDirections);

		for (int32 SampleIndex = 0; SampleIndex < NumConeSampleDirections; SampleIndex++)
		{
			AOSampleData.SampleDirections[SampleIndex] = FVector4(SampleDirections[SampleIndex]);
		}

		SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, GetUniformBufferParameter<FAOSampleData2>(), AOSampleData);

		FVector UnoccludedVector(0);

		for (int32 SampleIndex = 0; SampleIndex < NumConeSampleDirections; SampleIndex++)
		{
			UnoccludedVector += SampleDirections[SampleIndex];
		}

		float BentNormalNormalizeFactorValue = 1.0f / (UnoccludedVector / NumConeSampleDirections).Size();
		SetShaderValue(RHICmdList, ShaderRHI, BentNormalNormalizeFactor, BentNormalNormalizeFactorValue);

		FAOScreenGridResources* ScreenGridResources = View.ViewState->AOScreenGridResources;

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, DownsampledBentNormal.UAV);
		DistanceFieldBentNormal.SetTexture(RHICmdList, ShaderRHI, DownsampledBentNormal.ShaderResourceTexture, DownsampledBentNormal.UAV);

		SetSRVParameter(RHICmdList, ShaderRHI, ScreenGridConeVisibility, ScreenGridResources->ScreenGridConeVisibility.SRV);

		FIntPoint const ConeBufferMaxValue(View.ViewRect.Width() / GAODownsampleFactor / GConeTraceDownsampleFactor - 1, View.ViewRect.Height() / GAODownsampleFactor / GConeTraceDownsampleFactor - 1);
		SetShaderValue(RHICmdList, ShaderRHI, ConeBufferMax, ConeBufferMaxValue);

		FIntPoint const DFNormalBufferSize = GetBufferSizeForAO();
		FVector2D const DFNormalBufferUVMaxValue(
			(View.ViewRect.Width()  / GAODownsampleFactor - 0.5f) / DFNormalBufferSize.X,
			(View.ViewRect.Height() / GAODownsampleFactor - 0.5f) / DFNormalBufferSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, DFNormalBufferUVMax, DFNormalBufferUVMaxValue);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FSceneRenderTargetItem& DownsampledBentNormal)
	{
		DistanceFieldBentNormal.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, DownsampledBentNormal.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ScreenGridParameters;
		Ar << BentNormalNormalizeFactor;
		Ar << ScreenGridConeVisibility;
		Ar << DistanceFieldBentNormal;
		Ar << DFNormalBufferUVMax;
		Ar << ConeBufferMax;
		return bShaderHasOutdatedParameters;
	}

private:

	FScreenGridParameters ScreenGridParameters;
	FShaderParameter BentNormalNormalizeFactor;
	FShaderParameter DFNormalBufferUVMax;
	FShaderParameter ConeBufferMax;
	FShaderResourceParameter ScreenGridConeVisibility;
	FRWShaderParameter DistanceFieldBentNormal;
};

IMPLEMENT_SHADER_TYPE(,FCombineConeVisibilityCS,TEXT("/Engine/Private/DistanceFieldScreenGridLighting.usf"),TEXT("CombineConeVisibilityCS"),SF_Compute);

void PostProcessBentNormalAOScreenGrid(
	FRHICommandListImmediate& RHICmdList, 
	const FDistanceFieldAOParameters& Parameters, 
	const FViewInfo& View, 
	IPooledRenderTarget* VelocityTexture,
	FSceneRenderTargetItem& BentNormalInterpolation,
	FSceneRenderTargetItem& DistanceFieldNormal,
	TRefCountPtr<IPooledRenderTarget>& BentNormalOutput)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FSceneViewState* ViewState = (FSceneViewState*)View.State;
	FIntRect* DistanceFieldAOHistoryViewRect = ViewState ? &ViewState->DistanceFieldAOHistoryViewRect : nullptr;
	TRefCountPtr<IPooledRenderTarget>* BentNormalHistoryState = ViewState ? &ViewState->DistanceFieldAOHistoryRT : NULL;

	UpdateHistory(
		RHICmdList,
		View, 
		TEXT("DistanceFieldAOHistory"),
		VelocityTexture,
		DistanceFieldNormal,
		BentNormalInterpolation,
		DistanceFieldAOHistoryViewRect,
		BentNormalHistoryState,
		BentNormalOutput,
		Parameters);
}

void FDeferredShadingSceneRenderer::RenderDistanceFieldAOScreenGrid(
	FRHICommandListImmediate& RHICmdList, 
	const FViewInfo& View,
	const FDistanceFieldAOParameters& Parameters, 
	const TRefCountPtr<IPooledRenderTarget>& VelocityTexture,
	const TRefCountPtr<IPooledRenderTarget>& DistanceFieldNormal, 
	TRefCountPtr<IPooledRenderTarget>& OutDynamicBentNormalAO)
{
	const bool bUseDistanceFieldGI = IsDistanceFieldGIAllowed(View);
	const bool bUseGlobalDistanceField = UseGlobalDistanceField(Parameters) && Scene->DistanceFieldSceneData.NumObjectsInBuffer > 0;
	const bool bUseObjectDistanceField = UseAOObjectDistanceField();

	const FIntPoint ConeTraceBufferSize = GetBufferSizeForConeTracing();
	const FIntPoint TileListGroupSize = GetTileListGroupSizeForView(View);

	FAOScreenGridResources*& ScreenGridResources = View.ViewState->AOScreenGridResources;

	if ( !ScreenGridResources 
		|| ScreenGridResources->ScreenGridDimensions != ConeTraceBufferSize 
		|| ScreenGridResources->bAllocateResourceForGI != bUseDistanceFieldGI
		|| !ScreenGridResources->IsInitialized()
		|| GFastVRamConfig.bDirty )
	{
		if (ScreenGridResources)
		{
			ScreenGridResources->ReleaseResource();
		}
		else
		{
			ScreenGridResources = new FAOScreenGridResources();
		}

		ScreenGridResources->bAllocateResourceForGI = bUseDistanceFieldGI;
		ScreenGridResources->ScreenGridDimensions = ConeTraceBufferSize;

		ScreenGridResources->InitResource();
	}
	ScreenGridResources->AcquireTransientResource();

	SetRenderTarget(RHICmdList, NULL, NULL);

	if (bUseGlobalDistanceField)
	{
		SCOPED_DRAW_EVENT(RHICmdList, ConeTraceGlobal);

		float ConeVisibilityClearValue = 1.0f;
		ClearUAV(RHICmdList, ScreenGridResources->ScreenGridConeVisibility, *(uint32*)&ConeVisibilityClearValue);

		const uint32 GroupSizeX = FMath::DivideAndRoundUp(View.ViewRect.Size().X / GAODownsampleFactor / GConeTraceDownsampleFactor, GConeTraceGlobalDFTileSize);
		const uint32 GroupSizeY = FMath::DivideAndRoundUp(View.ViewRect.Size().Y / GAODownsampleFactor / GConeTraceDownsampleFactor, GConeTraceGlobalDFTileSize);

		check(View.GlobalDistanceFieldInfo.Clipmaps.Num() > 0);

		if (bUseObjectDistanceField)
		{
			TShaderMapRef<TConeTraceScreenGridGlobalOcclusionCS<true> > ComputeShader(View.ShaderMap);

			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, TileListGroupSize, DistanceFieldNormal->GetRenderTargetItem(), Parameters, View.GlobalDistanceFieldInfo);
			DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
			ComputeShader->UnsetParameters(RHICmdList, View);
		}
		else
		{
			TShaderMapRef<TConeTraceScreenGridGlobalOcclusionCS<false> > ComputeShader(View.ShaderMap);

			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, TileListGroupSize, DistanceFieldNormal->GetRenderTargetItem(), Parameters, View.GlobalDistanceFieldInfo);
			DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
			ComputeShader->UnsetParameters(RHICmdList, View);
		}
	}

	if (bUseObjectDistanceField)
	{
		SCOPED_DRAW_EVENT(RHICmdList, ConeTraceObjects);
		FTileIntersectionResources* TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;

		if (bUseGlobalDistanceField)
		{
			check(View.GlobalDistanceFieldInfo.Clipmaps.Num() > 0);

			if (bUseDistanceFieldGI)
			{
				TShaderMapRef<TConeTraceScreenGridObjectOcclusionCS<true, true> > ComputeShader(View.ShaderMap);

				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View, DistanceFieldNormal->GetRenderTargetItem(), Parameters, View.GlobalDistanceFieldInfo);
				DispatchIndirectComputeShader(RHICmdList, *ComputeShader, TileIntersectionResources->ObjectTilesIndirectArguments.Buffer, 0);
				ComputeShader->UnsetParameters(RHICmdList, View);
			}
			else
			{
				TShaderMapRef<TConeTraceScreenGridObjectOcclusionCS<false, true> > ComputeShader(View.ShaderMap);

				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View, DistanceFieldNormal->GetRenderTargetItem(), Parameters, View.GlobalDistanceFieldInfo);
				DispatchIndirectComputeShader(RHICmdList, *ComputeShader, TileIntersectionResources->ObjectTilesIndirectArguments.Buffer, 0);
				ComputeShader->UnsetParameters(RHICmdList, View);
			}
		}
		else
		{
			if (bUseDistanceFieldGI)
			{
				TShaderMapRef<TConeTraceScreenGridObjectOcclusionCS<true, false> > ComputeShader(View.ShaderMap);

				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View, DistanceFieldNormal->GetRenderTargetItem(), Parameters, View.GlobalDistanceFieldInfo);
				DispatchIndirectComputeShader(RHICmdList, *ComputeShader, TileIntersectionResources->ObjectTilesIndirectArguments.Buffer, 0);
				ComputeShader->UnsetParameters(RHICmdList, View);
			}
			else
			{
				TShaderMapRef<TConeTraceScreenGridObjectOcclusionCS<false, false> > ComputeShader(View.ShaderMap);

				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View, DistanceFieldNormal->GetRenderTargetItem(), Parameters, View.GlobalDistanceFieldInfo);
				DispatchIndirectComputeShader(RHICmdList, *ComputeShader, TileIntersectionResources->ObjectTilesIndirectArguments.Buffer, 0);
				ComputeShader->UnsetParameters(RHICmdList, View);
			}
		}

		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, ScreenGridResources->ScreenGridConeVisibility.UAV);
	}

	TRefCountPtr<IPooledRenderTarget> DownsampledIrradiance;

	if (bUseDistanceFieldGI)
	{
		{
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(ConeTraceBufferSize, PF_FloatRGBA, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, DownsampledIrradiance, TEXT("DownsampledIrradiance"));
		}

		extern void ComputeIrradianceForScreenGrid(
			FRHICommandListImmediate& RHICmdList,
			const FViewInfo& View,
			const FScene* Scene,
			const FDistanceFieldAOParameters& Parameters,
			FSceneRenderTargetItem& DistanceFieldNormal, 
			const FAOScreenGridResources& ScreenGridResources,
			FSceneRenderTargetItem& IrradianceTexture);

		ComputeIrradianceForScreenGrid(RHICmdList, View, Scene, Parameters, DistanceFieldNormal->GetRenderTargetItem(), *ScreenGridResources, DownsampledIrradiance->GetRenderTargetItem());
	}

	// Compute heightfield occlusion after heightfield GI, otherwise it self-shadows incorrectly
	View.HeightfieldLightingViewInfo.ComputeOcclusionForScreenGrid(View, RHICmdList, DistanceFieldNormal->GetRenderTargetItem(), *ScreenGridResources, Parameters);

	TRefCountPtr<IPooledRenderTarget> DownsampledBentNormal;

	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(ConeTraceBufferSize, PF_FloatRGBA, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
		Desc.Flags |= GFastVRamConfig.DistanceFieldAODownsampledBentNormal;
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, DownsampledBentNormal, TEXT("DownsampledBentNormal"));
	}

	{
		SCOPED_DRAW_EVENT(RHICmdList, CombineCones);
		const uint32 GroupSizeX = FMath::DivideAndRoundUp(ConeTraceBufferSize.X, GCombineConesSizeX);
		const uint32 GroupSizeY = FMath::DivideAndRoundUp(ConeTraceBufferSize.Y, GCombineConesSizeX);

		TShaderMapRef<FCombineConeVisibilityCS> ComputeShader(View.ShaderMap);

		RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
		ComputeShader->SetParameters(RHICmdList, View, DistanceFieldNormal->GetRenderTargetItem(), DownsampledBentNormal->GetRenderTargetItem());
		DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
		ComputeShader->UnsetParameters(RHICmdList, DownsampledBentNormal->GetRenderTargetItem());
	}

	if ( IsTransientResourceBufferAliasingEnabled() )
	{
		ScreenGridResources->DiscardTransientResource();
	}

	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, DownsampledBentNormal);

	PostProcessBentNormalAOScreenGrid(
		RHICmdList, 
		Parameters, 
		View, 
		VelocityTexture, 
		DownsampledBentNormal->GetRenderTargetItem(),
		DistanceFieldNormal->GetRenderTargetItem(), 
		OutDynamicBentNormalAO);
}
