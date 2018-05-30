// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldLightingPost.cpp
=============================================================================*/

#include "DistanceFieldLightingPost.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "DistanceFieldLightingShared.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "CompositionLighting/PostProcessAmbientOcclusion.h"
#include "PipelineStateCache.h"

int32 GAOUseHistory = 1;
FAutoConsoleVariableRef CVarAOUseHistory(
	TEXT("r.AOUseHistory"),
	GAOUseHistory,
	TEXT("Whether to apply a temporal filter to the distance field AO, which reduces flickering but also adds trails when occluders are moving."),
	ECVF_RenderThreadSafe
	);

int32 GAOClearHistory = 0;
FAutoConsoleVariableRef CVarAOClearHistory(
	TEXT("r.AOClearHistory"),
	GAOClearHistory,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GAOHistoryStabilityPass = 1;
FAutoConsoleVariableRef CVarAOHistoryStabilityPass(
	TEXT("r.AOHistoryStabilityPass"),
	GAOHistoryStabilityPass,
	TEXT("Whether to gather stable results to fill in holes in the temporal reprojection.  Adds some GPU cost but improves temporal stability with foliage."),
	ECVF_RenderThreadSafe
	);

float GAOHistoryWeight = .85f;
FAutoConsoleVariableRef CVarAOHistoryWeight(
	TEXT("r.AOHistoryWeight"),
	GAOHistoryWeight,
	TEXT("Amount of last frame's AO to lerp into the final result.  Higher values increase stability, lower values have less streaking under occluder movement."),
	ECVF_RenderThreadSafe
	);

float GAOHistoryDistanceThreshold = 30;
FAutoConsoleVariableRef CVarAOHistoryDistanceThreshold(
	TEXT("r.AOHistoryDistanceThreshold"),
	GAOHistoryDistanceThreshold,
	TEXT("World space distance threshold needed to discard last frame's DFAO results.  Lower values reduce ghosting from characters when near a wall but increase flickering artifacts."),
	ECVF_RenderThreadSafe
	);

float GAOViewFadeDistanceScale = .7f;
FAutoConsoleVariableRef CVarAOViewFadeDistanceScale(
	TEXT("r.AOViewFadeDistanceScale"),
	GAOViewFadeDistanceScale,
	TEXT("Distance over which AO will fade out as it approaches r.AOMaxViewDistance, as a fraction of r.AOMaxViewDistance."),
	ECVF_RenderThreadSafe
	);

bool UseAOHistoryStabilityPass()
{
	extern int32 GDistanceFieldAOQuality;
	return GAOHistoryStabilityPass && GDistanceFieldAOQuality >= 2;
}

class FGeometryAwareUpsampleParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		DistanceFieldNormalTexture.Bind(ParameterMap, TEXT("DistanceFieldNormalTexture"));
		DistanceFieldNormalSampler.Bind(ParameterMap, TEXT("DistanceFieldNormalSampler"));
		BentNormalAOTexture.Bind(ParameterMap, TEXT("BentNormalAOTexture"));
		BentNormalAOSampler.Bind(ParameterMap, TEXT("BentNormalAOSampler"));
		DistanceFieldGBufferTexelSize.Bind(ParameterMap, TEXT("DistanceFieldGBufferTexelSize"));
		DistanceFieldGBufferJitterOffset.Bind(ParameterMap, TEXT("DistanceFieldGBufferJitterOffset"));
		BentNormalBufferAndTexelSize.Bind(ParameterMap, TEXT("BentNormalBufferAndTexelSize"));
		MinDownsampleFactorToBaseLevel.Bind(ParameterMap, TEXT("MinDownsampleFactorToBaseLevel"));
		DistanceFadeScale.Bind(ParameterMap, TEXT("DistanceFadeScale"));
		JitterOffset.Bind(ParameterMap, TEXT("JitterOffset"));
	}

	void Set(FRHICommandList& RHICmdList, const FPixelShaderRHIParamRef& ShaderRHI, const FViewInfo& View, FSceneRenderTargetItem& DistanceFieldNormal, FSceneRenderTargetItem& DistanceFieldAOBentNormal)
	{
		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			DistanceFieldNormalTexture,
			DistanceFieldNormalSampler,
			TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(),
			DistanceFieldNormal.ShaderResourceTexture
		);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			BentNormalAOTexture,
			BentNormalAOSampler,
			TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(),
			DistanceFieldAOBentNormal.ShaderResourceTexture
		);

		extern FVector2D GetJitterOffset(int32 SampleIndex);
		FVector2D const JitterOffsetValue = GetJitterOffset(View.ViewState->GetDistanceFieldTemporalSampleIndex());

		const FIntPoint DownsampledBufferSize = GetBufferSizeForAO();
		const FVector2D BaseLevelTexelSizeValue(1.0f / DownsampledBufferSize.X, 1.0f / DownsampledBufferSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, DistanceFieldGBufferTexelSize, BaseLevelTexelSizeValue);

		SetShaderValue(RHICmdList, ShaderRHI, DistanceFieldGBufferJitterOffset, BaseLevelTexelSizeValue * JitterOffsetValue);

		extern FIntPoint GetBufferSizeForConeTracing();
		const FIntPoint ConeTracingBufferSize = GetBufferSizeForConeTracing();
		const FVector4 BentNormalBufferAndTexelSizeValue(ConeTracingBufferSize.X, ConeTracingBufferSize.Y, 1.0f / ConeTracingBufferSize.X, 1.0f / ConeTracingBufferSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, BentNormalBufferAndTexelSize, BentNormalBufferAndTexelSizeValue);

		extern int32 GConeTraceDownsampleFactor;
		const float MinDownsampleFactor = GConeTraceDownsampleFactor;
		SetShaderValue(RHICmdList, ShaderRHI, MinDownsampleFactorToBaseLevel, MinDownsampleFactor);

		extern float GAOViewFadeDistanceScale;
		const float DistanceFadeScaleValue = 1.0f / ((1.0f - GAOViewFadeDistanceScale) * GetMaxAOViewDistance());
		SetShaderValue(RHICmdList, ShaderRHI, DistanceFadeScale, DistanceFadeScaleValue);


		SetShaderValue(RHICmdList, ShaderRHI, JitterOffset, JitterOffsetValue);
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar, FGeometryAwareUpsampleParameters& P)
	{
		Ar << P.DistanceFieldNormalTexture;
		Ar << P.DistanceFieldNormalSampler;
		Ar << P.BentNormalAOTexture;
		Ar << P.BentNormalAOSampler;
		Ar << P.DistanceFieldGBufferTexelSize;
		Ar << P.DistanceFieldGBufferJitterOffset;
		Ar << P.BentNormalBufferAndTexelSize;
		Ar << P.MinDownsampleFactorToBaseLevel;
		Ar << P.DistanceFadeScale;
		Ar << P.JitterOffset;
		return Ar;
	}

private:
	FShaderResourceParameter DistanceFieldNormalTexture;
	FShaderResourceParameter DistanceFieldNormalSampler;
	FShaderResourceParameter BentNormalAOTexture;
	FShaderResourceParameter BentNormalAOSampler;
	FShaderParameter DistanceFieldGBufferTexelSize;
	FShaderParameter DistanceFieldGBufferJitterOffset;
	FShaderParameter BentNormalBufferAndTexelSize;
	FShaderParameter MinDownsampleFactorToBaseLevel;
	FShaderParameter DistanceFadeScale;
	FShaderParameter JitterOffset;
};

class FUpdateHistoryDepthRejectionPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FUpdateHistoryDepthRejectionPS, Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
	}

	/** Default constructor. */
	FUpdateHistoryDepthRejectionPS() {}

	/** Initialization constructor. */
	FUpdateHistoryDepthRejectionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer);
		AOParameters.Bind(Initializer.ParameterMap);
		GeometryAwareUpsampleParameters.Bind(Initializer.ParameterMap);
		BentNormalHistoryTexture.Bind(Initializer.ParameterMap, TEXT("BentNormalHistoryTexture"));
		BentNormalHistorySampler.Bind(Initializer.ParameterMap, TEXT("BentNormalHistorySampler"));
		HistoryWeight.Bind(Initializer.ParameterMap, TEXT("HistoryWeight"));
		HistoryDistanceThreshold.Bind(Initializer.ParameterMap, TEXT("HistoryDistanceThreshold"));
		UseHistoryFilter.Bind(Initializer.ParameterMap, TEXT("UseHistoryFilter"));
		VelocityTexture.Bind(Initializer.ParameterMap, TEXT("VelocityTexture"));
		VelocityTextureSampler.Bind(Initializer.ParameterMap, TEXT("VelocityTextureSampler"));
		HistoryScreenPositionScaleBias.Bind(Initializer.ParameterMap, TEXT("HistoryScreenPositionScaleBias"));
		HistoryUVMinMax.Bind(Initializer.ParameterMap, TEXT("HistoryUVMinMax"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		const FIntRect& HistoryViewRect,
		FSceneRenderTargetItem& DistanceFieldNormal, 
		FSceneRenderTargetItem& DistanceFieldAOBentNormal,
		FSceneRenderTargetItem& BentNormalHistoryTextureValue, 
		IPooledRenderTarget* VelocityTextureValue,
		const FDistanceFieldAOParameters& Parameters)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		SceneTextureParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, ESceneTextureSetupMode::All);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		GeometryAwareUpsampleParameters.Set(RHICmdList, ShaderRHI, View, DistanceFieldNormal, DistanceFieldAOBentNormal);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			BentNormalHistoryTexture,
			BentNormalHistorySampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			BentNormalHistoryTextureValue.ShaderResourceTexture
			);

		SetShaderValue(RHICmdList, ShaderRHI, HistoryWeight, GAOHistoryWeight);
		SetShaderValue(RHICmdList, ShaderRHI, HistoryDistanceThreshold, GAOHistoryDistanceThreshold);
		SetShaderValue(RHICmdList, ShaderRHI, UseHistoryFilter, UseAOHistoryStabilityPass() ? 1.0f : 0.0f);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			VelocityTexture,
			VelocityTextureSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			VelocityTextureValue ? VelocityTextureValue->GetRenderTargetItem().ShaderResourceTexture : GBlackTexture->TextureRHI
			);

		{
			FIntPoint HistoryBufferSize = FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor);

			const float InvBufferSizeX = 1.0f / HistoryBufferSize.X;
			const float InvBufferSizeY = 1.0f / HistoryBufferSize.Y;

			const FVector4 HistoryScreenPositionScaleBiasValue(
				HistoryViewRect.Width() * InvBufferSizeX / +2.0f,
				HistoryViewRect.Height() * InvBufferSizeY / (-2.0f * GProjectionSignY),
				(HistoryViewRect.Height() / 2.0f + HistoryViewRect.Min.Y) * InvBufferSizeY,
				(HistoryViewRect.Width() / 2.0f + HistoryViewRect.Min.X) * InvBufferSizeX);

			// Pull in the max UV to exclude the region which will read outside the viewport due to bilinear filtering
			const FVector4 HistoryUVMinMaxValue(
				(HistoryViewRect.Min.X + 0.5f) * InvBufferSizeX,
				(HistoryViewRect.Min.Y + 0.5f) * InvBufferSizeY,
				(HistoryViewRect.Max.X - 0.5f) * InvBufferSizeX,
				(HistoryViewRect.Max.Y - 0.5f) * InvBufferSizeY);

			SetShaderValue(RHICmdList, ShaderRHI, HistoryScreenPositionScaleBias, HistoryScreenPositionScaleBiasValue);
			SetShaderValue(RHICmdList, ShaderRHI, HistoryUVMinMax, HistoryUVMinMaxValue);
		}
	}
	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		Ar << AOParameters;
		Ar << GeometryAwareUpsampleParameters;
		Ar << BentNormalHistoryTexture;
		Ar << BentNormalHistorySampler;
		Ar << HistoryWeight;
		Ar << HistoryDistanceThreshold;
		Ar << UseHistoryFilter;
		Ar << VelocityTexture;
		Ar << VelocityTextureSampler;
		Ar << HistoryScreenPositionScaleBias;
		Ar << HistoryUVMinMax;
		return bShaderHasOutdatedParameters;
	}

private:

	FSceneTextureShaderParameters SceneTextureParameters;
	FAOParameters AOParameters;
	FGeometryAwareUpsampleParameters GeometryAwareUpsampleParameters;
	FShaderResourceParameter BentNormalHistoryTexture;
	FShaderResourceParameter BentNormalHistorySampler;
	FShaderParameter HistoryWeight;
	FShaderParameter HistoryDistanceThreshold;
	FShaderParameter UseHistoryFilter;
	FShaderResourceParameter VelocityTexture;
	FShaderResourceParameter VelocityTextureSampler;
	FShaderParameter HistoryScreenPositionScaleBias;
	FShaderParameter HistoryUVMinMax;
};

IMPLEMENT_SHADER_TYPE(, FUpdateHistoryDepthRejectionPS,TEXT("/Engine/Private/DistanceFieldLightingPost.usf"),TEXT("UpdateHistoryDepthRejectionPS"),SF_Pixel);


template<bool bManuallyClampUV>
class TFilterHistoryPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TFilterHistoryPS, Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("MANUALLY_CLAMP_UV"), bManuallyClampUV);
	}

	/** Default constructor. */
	TFilterHistoryPS() {}

	/** Initialization constructor. */
	TFilterHistoryPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		BentNormalAOTexture.Bind(Initializer.ParameterMap, TEXT("BentNormalAOTexture"));
		BentNormalAOSampler.Bind(Initializer.ParameterMap, TEXT("BentNormalAOSampler"));
		HistoryWeight.Bind(Initializer.ParameterMap, TEXT("HistoryWeight"));
		BentNormalAOTexelSize.Bind(Initializer.ParameterMap, TEXT("BentNormalAOTexelSize"));
		MaxSampleBufferUV.Bind(Initializer.ParameterMap, TEXT("MaxSampleBufferUV"));
		DistanceFieldNormalTexture.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalTexture"));
		DistanceFieldNormalSampler.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalSampler"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		FSceneRenderTargetItem& DistanceFieldNormal, 
		FSceneRenderTargetItem& BentNormalHistoryTextureValue)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			BentNormalAOTexture,
			BentNormalAOSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			BentNormalHistoryTextureValue.ShaderResourceTexture
			);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			DistanceFieldNormalTexture,
			DistanceFieldNormalSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistanceFieldNormal.ShaderResourceTexture
			);

		SetShaderValue(RHICmdList, ShaderRHI, HistoryWeight, GAOHistoryWeight);
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		
		const FIntPoint DownsampledBufferSize(SceneContext.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor));
		const FVector2D BaseLevelTexelSizeValue(1.0f / DownsampledBufferSize.X, 1.0f / DownsampledBufferSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, BentNormalAOTexelSize, BaseLevelTexelSizeValue);

		if (bManuallyClampUV)
		{
			FVector2D MaxSampleBufferUVValue(
				(View.ViewRect.Width() / GAODownsampleFactor - 0.5f - GAODownsampleFactor) / DownsampledBufferSize.X,
				(View.ViewRect.Height() / GAODownsampleFactor - 0.5f - GAODownsampleFactor) / DownsampledBufferSize.Y);
			SetShaderValue(RHICmdList, ShaderRHI, MaxSampleBufferUV, MaxSampleBufferUVValue);
		}
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << BentNormalAOTexture;
		Ar << BentNormalAOSampler;
		Ar << HistoryWeight;
		Ar << BentNormalAOTexelSize;
		Ar << MaxSampleBufferUV;
		Ar << DistanceFieldNormalTexture;
		Ar << DistanceFieldNormalSampler;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderResourceParameter BentNormalAOTexture;
	FShaderResourceParameter BentNormalAOSampler;
	FShaderParameter HistoryWeight;
	FShaderParameter BentNormalAOTexelSize;
	FShaderParameter MaxSampleBufferUV;
	FShaderResourceParameter DistanceFieldNormalTexture;
	FShaderResourceParameter DistanceFieldNormalSampler;
};


#define VARIATION1(A) \
	typedef TFilterHistoryPS<A> TFilterHistoryPS##A; \
	IMPLEMENT_SHADER_TYPE(template<>,TFilterHistoryPS##A,TEXT("/Engine/Private/DistanceFieldLightingPost.usf"),TEXT("FilterHistoryPS"),SF_Pixel);

VARIATION1(false)
VARIATION1(true)

#undef VARIATION1

class FGeometryAwareUpsamplePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGeometryAwareUpsamplePS, Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
	}

	/** Default constructor. */
	FGeometryAwareUpsamplePS() {}

	/** Initialization constructor. */
	FGeometryAwareUpsamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		AOParameters.Bind(Initializer.ParameterMap);
		GeometryAwareUpsampleParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		FSceneRenderTargetItem& DistanceFieldNormal,
		FSceneRenderTargetItem& DistanceFieldAOBentNormal,
		const FDistanceFieldAOParameters& Parameters)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		GeometryAwareUpsampleParameters.Set(RHICmdList, ShaderRHI, View, DistanceFieldNormal, DistanceFieldAOBentNormal);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << AOParameters;
		Ar << GeometryAwareUpsampleParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FAOParameters AOParameters;
	FGeometryAwareUpsampleParameters GeometryAwareUpsampleParameters;
};

IMPLEMENT_SHADER_TYPE(,FGeometryAwareUpsamplePS, TEXT("/Engine/Private/DistanceFieldLightingPost.usf"), TEXT("GeometryAwareUpsamplePS"), SF_Pixel);

void AllocateOrReuseAORenderTarget(FRHICommandList& RHICmdList, TRefCountPtr<IPooledRenderTarget>& Target, const TCHAR* Name, EPixelFormat Format, uint32 Flags) 
{
	if (!Target)
	{
		FIntPoint BufferSize = GetBufferSizeForAO();

		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, Format, FClearValueBinding::None, Flags, TexCreate_RenderTargetable | TexCreate_UAV, false));
		Desc.AutoWritable = false;
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, Target, Name, true, ERenderTargetTransience::NonTransient);
	}
}

void GeometryAwareUpsample(FRHICommandList& RHICmdList, const FViewInfo& View, TRefCountPtr<IPooledRenderTarget>& DistanceFieldAOBentNormal, FSceneRenderTargetItem& DistanceFieldNormal, FSceneRenderTargetItem& BentNormalInterpolation, const FDistanceFieldAOParameters& Parameters)
{
	SCOPED_DRAW_EVENT(RHICmdList, GeometryAwareUpsample);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	SetRenderTarget(RHICmdList, DistanceFieldAOBentNormal->GetRenderTargetItem().TargetableTexture, FTextureRHIParamRef());

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	{
		RHICmdList.SetViewport(0, 0, 0.0f, View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor, 1.0f);
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

		TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		TShaderMapRef<FGeometryAwareUpsamplePS> PixelShader(View.ShaderMap);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
		PixelShader->SetParameters(RHICmdList, View, DistanceFieldNormal, BentNormalInterpolation, Parameters);

		VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);

		DrawRectangle(
			RHICmdList,
			0, 0,
			View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
			0, 0,
			View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
			FIntPoint(View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor),
			SceneContext.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor),
			*VertexShader);
	}

	RHICmdList.CopyToResolveTarget(DistanceFieldAOBentNormal->GetRenderTargetItem().TargetableTexture, DistanceFieldAOBentNormal->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
}

void UpdateHistory(
	FRHICommandList& RHICmdList,
	const FViewInfo& View, 
	const TCHAR* BentNormalHistoryRTName,
	IPooledRenderTarget* VelocityTexture,
	FSceneRenderTargetItem& DistanceFieldNormal,
	FSceneRenderTargetItem& BentNormalInterpolation,
	/** Contains last frame's history, if non-NULL.  This will be updated with the new frame's history. */
	FIntRect* DistanceFieldAOHistoryViewRect,
	TRefCountPtr<IPooledRenderTarget>* BentNormalHistoryState,
	/** Output of Temporal Reprojection for the next step in the pipeline. */
	TRefCountPtr<IPooledRenderTarget>& BentNormalHistoryOutput,
	const FDistanceFieldAOParameters& Parameters)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	if (BentNormalHistoryState && GAOUseHistory)
	{
		FIntPoint BufferSize = GetBufferSizeForAO();

		if (*BentNormalHistoryState 
			&& !View.bCameraCut 
			&& !View.bPrevTransformsReset
			&& !GAOClearHistory
			// If the scene render targets reallocate, toss the history so we don't read uninitialized data
			&& (*BentNormalHistoryState)->GetDesc().Extent == BufferSize)
		{
			uint32 HistoryPassOutputFlags = UseAOHistoryStabilityPass() ? GFastVRamConfig.DistanceFieldAOHistory : 0;
			// Reuse a render target from the pool with a consistent name, for vis purposes
			TRefCountPtr<IPooledRenderTarget> NewBentNormalHistory;
			AllocateOrReuseAORenderTarget(RHICmdList, NewBentNormalHistory, BentNormalHistoryRTName, PF_FloatRGBA, HistoryPassOutputFlags);

			SCOPED_DRAW_EVENT(RHICmdList, UpdateHistory);

			{
				SetRenderTarget(RHICmdList, NewBentNormalHistory->GetRenderTargetItem().TargetableTexture, FTextureRHIParamRef());
				RHICmdList.SetViewport(0, 0, 0.0f, View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor, 1.0f);

				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

				TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;

				TShaderMapRef<FUpdateHistoryDepthRejectionPS> PixelShader(View.ShaderMap);

				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
				PixelShader->SetParameters(RHICmdList, View, *DistanceFieldAOHistoryViewRect, DistanceFieldNormal, BentNormalInterpolation,(*BentNormalHistoryState)->GetRenderTargetItem(), VelocityTexture, Parameters);

				VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);

				DrawRectangle( 
					RHICmdList,
					0, 0, 
					View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
					View.ViewRect.Min.X / GAODownsampleFactor, View.ViewRect.Min.Y / GAODownsampleFactor,
					View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
					FIntPoint(View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor),
					SceneContext.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor),
					*VertexShader);

				RHICmdList.CopyToResolveTarget(NewBentNormalHistory->GetRenderTargetItem().TargetableTexture, NewBentNormalHistory->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
			}

			if (UseAOHistoryStabilityPass())
			{
				const FPooledRenderTargetDesc& HistoryDesc = (*BentNormalHistoryState)->GetDesc();

				// Reallocate history if buffer sizes have changed
				if (HistoryDesc.Extent != SceneContext.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor))
				{
					GRenderTargetPool.FreeUnusedResource(*BentNormalHistoryState);
					*BentNormalHistoryState = NULL;
					// Update the view state's render target reference with the new history
					AllocateOrReuseAORenderTarget(RHICmdList, *BentNormalHistoryState, BentNormalHistoryRTName, PF_FloatRGBA);
				}

				{
					SetRenderTarget(RHICmdList, (*BentNormalHistoryState)->GetRenderTargetItem().TargetableTexture, FTextureRHIParamRef());
					RHICmdList.SetViewport(0, 0, 0.0f, View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor, 1.0f);

					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

					TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;

					if (View.ViewRect.Min == FIntPoint::ZeroValue && View.ViewRect.Max == SceneContext.GetBufferSizeXY())
					{
						TShaderMapRef<TFilterHistoryPS<false> > PixelShader(View.ShaderMap);

						GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
						SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
						PixelShader->SetParameters(RHICmdList, View, DistanceFieldNormal, NewBentNormalHistory->GetRenderTargetItem());
					}
					else
					{
						TShaderMapRef<TFilterHistoryPS<true> > PixelShader(View.ShaderMap);

						GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
						SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
						PixelShader->SetParameters(RHICmdList, View, DistanceFieldNormal, NewBentNormalHistory->GetRenderTargetItem());
					}

					VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);

					DrawRectangle( 
						RHICmdList,
						0, 0, 
						View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
						0, 0,
						View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
						FIntPoint(View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor),
						SceneContext.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor),
						*VertexShader);

					RHICmdList.CopyToResolveTarget((*BentNormalHistoryState)->GetRenderTargetItem().TargetableTexture, (*BentNormalHistoryState)->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
				}

				BentNormalHistoryOutput = *BentNormalHistoryState;
			}
			else
			{
				// Update the view state's render target reference with the new history
				*BentNormalHistoryState = NewBentNormalHistory;
				BentNormalHistoryOutput = NewBentNormalHistory;
			}
		}
		else
		{
			// Use the current frame's upscaled mask for next frame's history
			TRefCountPtr<IPooledRenderTarget> DistanceFieldAOBentNormal;
			AllocateOrReuseAORenderTarget(RHICmdList, DistanceFieldAOBentNormal, TEXT("DistanceFieldBentNormalAO"), PF_FloatRGBA, GFastVRamConfig.DistanceFieldAOBentNormal);

			GeometryAwareUpsample(RHICmdList, View, DistanceFieldAOBentNormal, DistanceFieldNormal, BentNormalInterpolation, Parameters);

			RHICmdList.CopyToResolveTarget(DistanceFieldAOBentNormal->GetRenderTargetItem().TargetableTexture, DistanceFieldAOBentNormal->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());

			*BentNormalHistoryState = DistanceFieldAOBentNormal;
			BentNormalHistoryOutput = DistanceFieldAOBentNormal;
		}

		DistanceFieldAOHistoryViewRect->Min = FIntPoint::ZeroValue;
		DistanceFieldAOHistoryViewRect->Max.X = View.ViewRect.Size().X / GAODownsampleFactor;
		DistanceFieldAOHistoryViewRect->Max.Y = View.ViewRect.Size().Y / GAODownsampleFactor;
	}
	else
	{
		// Temporal reprojection is disabled or there is no view state - just upscale
		TRefCountPtr<IPooledRenderTarget> DistanceFieldAOBentNormal;
		AllocateOrReuseAORenderTarget(RHICmdList, DistanceFieldAOBentNormal, TEXT("DistanceFieldBentNormalAO"), PF_FloatRGBA, GFastVRamConfig.DistanceFieldAOBentNormal);

		GeometryAwareUpsample(RHICmdList, View, DistanceFieldAOBentNormal, DistanceFieldNormal, BentNormalInterpolation, Parameters);

		RHICmdList.CopyToResolveTarget(DistanceFieldAOBentNormal->GetRenderTargetItem().TargetableTexture, DistanceFieldAOBentNormal->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());

		BentNormalHistoryOutput = DistanceFieldAOBentNormal;
	}
}

template<bool bModulateToSceneColor>
class TDistanceFieldAOUpsamplePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDistanceFieldAOUpsamplePS, Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("MODULATE_SCENE_COLOR"), bModulateToSceneColor);
	}

	/** Default constructor. */
	TDistanceFieldAOUpsamplePS() {}

	/** Initialization constructor. */
	TDistanceFieldAOUpsamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer);
		DFAOUpsampleParameters.Bind(Initializer.ParameterMap);
		MinIndirectDiffuseOcclusion.Bind(Initializer.ParameterMap,TEXT("MinIndirectDiffuseOcclusion"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, TRefCountPtr<IPooledRenderTarget>& DistanceFieldAOBentNormal)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		SceneTextureParameters.Set(RHICmdList, ShaderRHI, View.FeatureLevel, ESceneTextureSetupMode::All);
		DFAOUpsampleParameters.Set(RHICmdList, ShaderRHI, View, DistanceFieldAOBentNormal);

		FScene* Scene = (FScene*)View.Family->Scene;
		const float MinOcclusion = Scene->SkyLight ? Scene->SkyLight->MinOcclusion : 0;
		SetShaderValue(RHICmdList, ShaderRHI, MinIndirectDiffuseOcclusion, MinOcclusion);
	}
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		Ar << DFAOUpsampleParameters;
		Ar << MinIndirectDiffuseOcclusion;
		return bShaderHasOutdatedParameters;
	}

private:

	FSceneTextureShaderParameters SceneTextureParameters;
	FDFAOUpsampleParameters DFAOUpsampleParameters;
	FShaderParameter MinIndirectDiffuseOcclusion;
};

#define IMPLEMENT_UPSAMPLE_PS_TYPE(bModulateToSceneColor) \
	typedef TDistanceFieldAOUpsamplePS<bModulateToSceneColor> TDistanceFieldAOUpsamplePS##bModulateToSceneColor; \
	IMPLEMENT_SHADER_TYPE(template<>,TDistanceFieldAOUpsamplePS##bModulateToSceneColor,TEXT("/Engine/Private/DistanceFieldLightingPost.usf"),TEXT("AOUpsamplePS"),SF_Pixel);

IMPLEMENT_UPSAMPLE_PS_TYPE(false)
IMPLEMENT_UPSAMPLE_PS_TYPE(true)

void UpsampleBentNormalAO(
	FRHICommandList& RHICmdList, 
	const TArray<FViewInfo>& Views, 
	TRefCountPtr<IPooledRenderTarget>& DistanceFieldAOBentNormal,
	bool bModulateSceneColor)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		SCOPED_DRAW_EVENT(RHICmdList, UpsampleAO);

		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		if (bModulateSceneColor)
		{
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero, BO_Add, BF_Zero, BF_One>::GetRHI();
		}
		else
		{
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		}

		TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

		if (bModulateSceneColor)
		{
			TShaderMapRef<TDistanceFieldAOUpsamplePS<true> > PixelShader(View.ShaderMap);
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
			PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal);
		}
		else
		{
			TShaderMapRef<TDistanceFieldAOUpsamplePS<false> > PixelShader(View.ShaderMap);
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
			PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal);
		}

		DrawRectangle( 
			RHICmdList,
			0, 0, 
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Min.X / GAODownsampleFactor, View.ViewRect.Min.Y / GAODownsampleFactor, 
			View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
			FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
			GetBufferSizeForAO(),
			*VertexShader);
	}
}
