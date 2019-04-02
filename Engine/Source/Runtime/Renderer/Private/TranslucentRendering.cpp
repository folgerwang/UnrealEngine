// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TranslucentRendering.cpp: Translucent rendering implementation.
=============================================================================*/

#include "TranslucentRendering.h"
#include "DeferredShadingRenderer.h"
#include "BasePassRendering.h"
#include "DynamicPrimitiveDrawing.h"
#include "RendererModule.h"
#include "LightPropagationVolume.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PipelineStateCache.h"
#include "MeshPassProcessor.inl"

DECLARE_CYCLE_STAT(TEXT("TranslucencyTimestampQueryFence Wait"), STAT_TranslucencyTimestampQueryFence_Wait, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("TranslucencyTimestampQuery Wait"), STAT_TranslucencyTimestampQuery_Wait, STATGROUP_SceneRendering);

DECLARE_FLOAT_COUNTER_STAT(TEXT("Translucency GPU Time (MS)"), STAT_TranslucencyGPU, STATGROUP_SceneRendering);

DECLARE_GPU_STAT(Translucency);

static TAutoConsoleVariable<float> CVarSeparateTranslucencyScreenPercentage(
	TEXT("r.SeparateTranslucencyScreenPercentage"),
	100.0f,
	TEXT("Render separate translucency at this percentage of the full resolution.\n")
	TEXT("in percent, >0 and <=100, larger numbers are possible (supersampling).")
	TEXT("<0 is treated like 100."),
	ECVF_Scalability | ECVF_Default);

static TAutoConsoleVariable<int32> CVarSeparateTranslucencyAutoDownsample(
	TEXT("r.SeparateTranslucencyAutoDownsample"),
	0,
	TEXT("Whether to automatically downsample separate translucency based on last frame's GPU time.\n")
	TEXT("Automatic downsampling is only used when r.SeparateTranslucencyScreenPercentage is 100"),
	ECVF_Scalability | ECVF_Default);

static TAutoConsoleVariable<float> CVarSeparateTranslucencyDurationDownsampleThreshold(
	TEXT("r.SeparateTranslucencyDurationDownsampleThreshold"),
	1.5f,
	TEXT("When smoothed full-res translucency GPU duration is larger than this value (ms), the entire pass will be downsampled by a factor of 2 in each dimension."),
	ECVF_Scalability | ECVF_Default);

static TAutoConsoleVariable<float> CVarSeparateTranslucencyDurationUpsampleThreshold(
	TEXT("r.SeparateTranslucencyDurationUpsampleThreshold"),
	.5f,
	TEXT("When smoothed half-res translucency GPU duration is smaller than this value (ms), the entire pass will be restored to full resolution.\n")
	TEXT("This should be around 1/4 of r.SeparateTranslucencyDurationDownsampleThreshold to avoid toggling downsampled state constantly."),
	ECVF_Scalability | ECVF_Default);

static TAutoConsoleVariable<float> CVarSeparateTranslucencyMinDownsampleChangeTime(
	TEXT("r.SeparateTranslucencyMinDownsampleChangeTime"),
	1.0f,
	TEXT("Minimum time in seconds between changes to automatic downsampling state, used to prevent rapid swapping between half and full res."),
	ECVF_Scalability | ECVF_Default);

static TAutoConsoleVariable<int32> CVarSeparateTranslucencyUpsampleMode(
	TEXT("r.SeparateTranslucencyUpsampleMode"),
	1,
	TEXT("Upsample method to use on separate translucency.  These are only used when r.SeparateTranslucencyScreenPercentage is less than 100.\n")
	TEXT("0: bilinear 1: Nearest-Depth Neighbor (only when r.SeparateTranslucencyScreenPercentage is 50)"),
	ECVF_Scalability | ECVF_Default);

int32 GAllowDownsampledStandardTranslucency = 0;

static FAutoConsoleVariableRef CVarAllowDownsampledStandardTranslucency(
	TEXT("r.AllowDownsampledStandardTranslucency"),
	GAllowDownsampledStandardTranslucency,
	TEXT("Allow standard translucency to be rendered in smaller resolution as an optimization\n")
	TEXT("This is incompatible with materials using blend modulate. Use 2 to ignore those. \n")
	TEXT(" <0: off\n")
	TEXT(" 0: on unless a material using blend modulate is used (default)")
	TEXT(" >0: on and ignores any material using blend modulate"),
	ECVF_RenderThreadSafe
);

/** Mostly used to know if debug rendering should be drawn in this pass */
FORCEINLINE bool IsMainTranslucencyPass(ETranslucencyPass::Type TranslucencyPass)
{
	return TranslucencyPass == ETranslucencyPass::TPT_AllTranslucency || TranslucencyPass == ETranslucencyPass::TPT_StandardTranslucency;
}

EMeshPass::Type TranslucencyPassToMeshPass(ETranslucencyPass::Type TranslucencyPass)
{
	EMeshPass::Type TranslucencyMeshPass = EMeshPass::Num;

	switch (TranslucencyPass)
	{
	case ETranslucencyPass::TPT_StandardTranslucency: TranslucencyMeshPass = EMeshPass::TranslucencyStandard; break;
	case ETranslucencyPass::TPT_TranslucencyAfterDOF: TranslucencyMeshPass = EMeshPass::TranslucencyAfterDOF; break;
	case ETranslucencyPass::TPT_AllTranslucency: TranslucencyMeshPass = EMeshPass::TranslucencyAll; break;
	}

	check(TranslucencyMeshPass != EMeshPass::Num);

	return TranslucencyMeshPass;
}

static bool RenderInSeparateTranslucency(const FSceneRenderTargets& SceneContext, ETranslucencyPass::Type TranslucencyPass, bool bPrimitiveDisablesOffscreenBuffer)
{
	// Currently AfterDOF is rendered earlier in the frame and must be rendered in a separate (offscreen) buffer.
	if (TranslucencyPass == ETranslucencyPass::TPT_TranslucencyAfterDOF)
	{
		// If bPrimitiveDisablesOffscreenBuffer, that will trigger an ensure call
		return true;
	}

	// Otherwise it only gets rendered in the separate buffer if it is downsampled
	if (bPrimitiveDisablesOffscreenBuffer ? (GAllowDownsampledStandardTranslucency > 0) : (GAllowDownsampledStandardTranslucency >= 0))
	{
		FIntPoint ScaledSize;
		float DownsamplingScale = 1.f;
		SceneContext.GetSeparateTranslucencyDimensions(ScaledSize, DownsamplingScale);

		if (DownsamplingScale < 1.f)
		{
			return true;
		}
	}

	return false;
}

void FDeferredShadingSceneRenderer::UpdateTranslucencyTimersAndSeparateTranslucencyBufferSize(FRHICommandListImmediate& RHICmdList)
{
	bool bAnyViewWantsDownsampledSeparateTranslucency = false;
	bool bCVarSeparateTranslucencyAutoDownsample = CVarSeparateTranslucencyAutoDownsample.GetValueOnRenderThread() != 0;
#if (!STATS)
	if (bCVarSeparateTranslucencyAutoDownsample)
#endif
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			FSceneViewState* ViewState = View.ViewState;

			if (ViewState)
			{
				//We always tick the separate trans timer but only need the other timer for stats
				bool bSeparateTransTimerSuccess = ViewState->SeparateTranslucencyTimer.Tick(RHICmdList);
				if (STATS)
				{
					ViewState->TranslucencyTimer.Tick(RHICmdList);
					//Stats are fed the most recent available time and so are lagged a little. 
					float MostRecentTotalTime = ViewState->TranslucencyTimer.GetTimeMS() + ViewState->SeparateTranslucencyTimer.GetTimeMS();
					SET_FLOAT_STAT(STAT_TranslucencyGPU, MostRecentTotalTime);
				}

				if (bCVarSeparateTranslucencyAutoDownsample && bSeparateTransTimerSuccess)
				{
					float LastFrameTranslucencyDurationMS = ViewState->SeparateTranslucencyTimer.GetTimeMS();
					const bool bOriginalShouldAutoDownsampleTranslucency = ViewState->bShouldAutoDownsampleTranslucency;

					if (ViewState->bShouldAutoDownsampleTranslucency)
					{
						ViewState->SmoothedFullResTranslucencyGPUDuration = 0;
						const float LerpAlpha = ViewState->SmoothedHalfResTranslucencyGPUDuration == 0 ? 1.0f : .1f;
						ViewState->SmoothedHalfResTranslucencyGPUDuration = FMath::Lerp(ViewState->SmoothedHalfResTranslucencyGPUDuration, LastFrameTranslucencyDurationMS, LerpAlpha);

						// Don't re-asses switching for some time after the last switch
						if (View.Family->CurrentRealTime - ViewState->LastAutoDownsampleChangeTime > CVarSeparateTranslucencyMinDownsampleChangeTime.GetValueOnRenderThread())
						{
							// Downsample if the smoothed time is larger than the threshold
							ViewState->bShouldAutoDownsampleTranslucency = ViewState->SmoothedHalfResTranslucencyGPUDuration > CVarSeparateTranslucencyDurationUpsampleThreshold.GetValueOnRenderThread();

							if (!ViewState->bShouldAutoDownsampleTranslucency)
							{
								// Do 'log LogRenderer verbose' to get these
								UE_LOG(LogRenderer, Verbose, TEXT("Upsample: %.1fms < %.1fms"), ViewState->SmoothedHalfResTranslucencyGPUDuration, CVarSeparateTranslucencyDurationUpsampleThreshold.GetValueOnRenderThread());
							}
						}
					}
					else
					{
						ViewState->SmoothedHalfResTranslucencyGPUDuration = 0;
						const float LerpAlpha = ViewState->SmoothedFullResTranslucencyGPUDuration == 0 ? 1.0f : .1f;
						ViewState->SmoothedFullResTranslucencyGPUDuration = FMath::Lerp(ViewState->SmoothedFullResTranslucencyGPUDuration, LastFrameTranslucencyDurationMS, LerpAlpha);

						if (View.Family->CurrentRealTime - ViewState->LastAutoDownsampleChangeTime > CVarSeparateTranslucencyMinDownsampleChangeTime.GetValueOnRenderThread())
						{
							// Downsample if the smoothed time is larger than the threshold
							ViewState->bShouldAutoDownsampleTranslucency = ViewState->SmoothedFullResTranslucencyGPUDuration > CVarSeparateTranslucencyDurationDownsampleThreshold.GetValueOnRenderThread();

							if (ViewState->bShouldAutoDownsampleTranslucency)
							{
								UE_LOG(LogRenderer, Verbose, TEXT("Downsample: %.1fms > %.1fms"), ViewState->SmoothedFullResTranslucencyGPUDuration, CVarSeparateTranslucencyDurationDownsampleThreshold.GetValueOnRenderThread());
							}
						}
					}

					if (bOriginalShouldAutoDownsampleTranslucency != ViewState->bShouldAutoDownsampleTranslucency)
					{
						ViewState->LastAutoDownsampleChangeTime = View.Family->CurrentRealTime;
					}

					bAnyViewWantsDownsampledSeparateTranslucency = bAnyViewWantsDownsampledSeparateTranslucency || ViewState->bShouldAutoDownsampleTranslucency;
				}
			}
		}
	}

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SceneContext.SetSeparateTranslucencyBufferSize(bAnyViewWantsDownsampledSeparateTranslucency);
}

void FDeferredShadingSceneRenderer::BeginTimingSeparateTranslucencyPass(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	if (View.ViewState && GSupportsTimestampRenderQueries
#if !STATS
		&& (CVarSeparateTranslucencyAutoDownsample.GetValueOnRenderThread() != 0)
#endif
		)
	{
		View.ViewState->SeparateTranslucencyTimer.Begin(RHICmdList);
	}
}

void FDeferredShadingSceneRenderer::EndTimingSeparateTranslucencyPass(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	if (View.ViewState && GSupportsTimestampRenderQueries
#if !STATS
		&& (CVarSeparateTranslucencyAutoDownsample.GetValueOnRenderThread() != 0)
#endif
		)
	{
		View.ViewState->SeparateTranslucencyTimer.End(RHICmdList);
	}
}

/** Pixel shader used to copy scene color into another texture so that materials can read from scene color with a node. */
class FCopySceneColorPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCopySceneColorPS, Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4); }

	FCopySceneColorPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer);
	}
	FCopySceneColorPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		SceneTextureParameters.Set(RHICmdList, GetPixelShader(), View.FeatureLevel, ESceneTextureSetupMode::All);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FSceneTextureShaderParameters SceneTextureParameters;
};

IMPLEMENT_SHADER_TYPE(, FCopySceneColorPS, TEXT("/Engine/Private/TranslucentLightingShaders.usf"), TEXT("CopySceneColorMain"), SF_Pixel);

extern int32 GLightShaftRenderAfterDOF;

bool FSceneRenderer::ShouldRenderTranslucency(ETranslucencyPass::Type TranslucencyPass) const
{
	// Change this condition to control where simple elements should be rendered.
	if (IsMainTranslucencyPass(TranslucencyPass))
	{
		if (ViewFamily.EngineShowFlags.VisualizeLPV)
		{
			return true;
		}

		for (const FViewInfo& View : Views)
		{
			if (View.bHasTranslucentViewMeshElements || View.SimpleElementCollector.BatchedElements.HasPrimsToDraw())
			{
				return true;
			}
		}
	}

	// If lightshafts are rendered in low res, we must reset the offscreen buffer in case is was also used in TPT_StandardTranslucency.
	if (GLightShaftRenderAfterDOF && TranslucencyPass == ETranslucencyPass::TPT_TranslucencyAfterDOF)
	{
		return true;
	}

	for (const FViewInfo& View : Views)
	{
		if (View.TranslucentPrimCount.Num(TranslucencyPass) > 0)
		{
			return true;
		}
	}

	return false;
}

DECLARE_CYCLE_STAT(TEXT("Translucency"), STAT_CLP_Translucency, STATGROUP_ParallelCommandListMarkers);

class FTranslucencyPassParallelCommandListSet : public FParallelCommandListSet
{
	ETranslucencyPass::Type TranslucencyPass;
	bool bRenderInSeparateTranslucency;

public:
	FTranslucencyPassParallelCommandListSet(const FViewInfo& InView, const FSceneRenderer* InSceneRenderer, FRHICommandListImmediate& InParentCmdList, bool bInParallelExecute, bool bInCreateSceneContext, const FMeshPassProcessorRenderState& InDrawRenderState, ETranslucencyPass::Type InTranslucencyPass, bool InRenderInSeparateTranslucency)
		: FParallelCommandListSet(GET_STATID(STAT_CLP_Translucency), InView, InSceneRenderer, InParentCmdList, bInParallelExecute, bInCreateSceneContext, InDrawRenderState)
		, TranslucencyPass(InTranslucencyPass)
		, bRenderInSeparateTranslucency(InRenderInSeparateTranslucency)
	{
	}

	virtual ~FTranslucencyPassParallelCommandListSet()
	{
		Dispatch();
	}

	virtual void SetStateOnCommandList(FRHICommandList& CmdList) override
	{
		// Never needs clear here as it is already done in RenderTranslucency.
		FParallelCommandListSet::SetStateOnCommandList(CmdList);
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(CmdList);
		if (bRenderInSeparateTranslucency)
		{
			SceneContext.BeginRenderingSeparateTranslucency(CmdList, View, *SceneRenderer, false);
		}
		else
		{
			SceneContext.BeginRenderingTranslucency(CmdList, View, *SceneRenderer, false);
		}
	}
};

static TAutoConsoleVariable<int32> CVarRHICmdTranslucencyPassDeferredContexts(
	TEXT("r.RHICmdTranslucencyPassDeferredContexts"),
	1,
	TEXT("True to use deferred contexts to parallelize base pass command list execution."));

static TAutoConsoleVariable<int32> CVarRHICmdFlushRenderThreadTasksTranslucentPass(
	TEXT("r.RHICmdFlushRenderThreadTasksTranslucentPass"),
	0,
	TEXT("Wait for completion of parallel render thread tasks at the end of the translucent pass.  A more granular version of r.RHICmdFlushRenderThreadTasks. If either r.RHICmdFlushRenderThreadTasks or r.RHICmdFlushRenderThreadTasksTranslucentPass is > 0 we will flush."));


static TAutoConsoleVariable<int32> CVarParallelTranslucency(
	TEXT("r.ParallelTranslucency"),
	1,
	TEXT("Toggles parallel translucency rendering. Parallel rendering must be enabled for this to have an effect."),
	ECVF_RenderThreadSafe
);

void RenderViewTranslucencyInner(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, const FMeshPassProcessorRenderState& DrawRenderState, ETranslucencyPass::Type TranslucencyPass, FTranslucencyPassParallelCommandListSet* ParallelCommandListSet)
{
	SCOPED_GPU_MASK(RHICmdList, View.GPUMask);

	// Draw translucent prims
	if (!View.Family->UseDebugViewPS())
	{
		QUICK_SCOPE_CYCLE_COUNTER(RenderTranslucencyParallel_Start_FDrawSortedTransAnyThreadTask);

		const EMeshPass::Type MeshPass = TranslucencyPassToMeshPass(TranslucencyPass);
		View.ParallelMeshDrawCommandPasses[MeshPass].DispatchDraw(ParallelCommandListSet, RHICmdList);
	}

	if (IsMainTranslucencyPass(TranslucencyPass))
	{
		View.SimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::Translucent, SDPG_World);
		View.SimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, View, EBlendModeFilter::Translucent, SDPG_Foreground);

		// editor and debug rendering
		if (View.bHasTranslucentViewMeshElements)
		{
			{
				QUICK_SCOPE_CYCLE_COUNTER(RenderTranslucencyParallel_SDPG_World);

				DrawDynamicMeshPass(View, RHICmdList,
					[&View, &DrawRenderState](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
				{
					FBasePassMeshProcessor PassMeshProcessor(
						View.Family->Scene->GetRenderScene(),
						View.GetFeatureLevel(),
						&View,
						DrawRenderState,
						DynamicMeshPassContext,
						ETranslucencyPass::TPT_StandardTranslucency);

					const uint64 DefaultBatchElementMask = ~0ull;

					for (int32 MeshIndex = 0; MeshIndex < View.ViewMeshElements.Num(); MeshIndex++)
					{
						const FMeshBatch& MeshBatch = View.ViewMeshElements[MeshIndex];
						PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
					}
				});
			}

			{
				QUICK_SCOPE_CYCLE_COUNTER(RenderTranslucencyParallel_SDPG_Foreground);

				DrawDynamicMeshPass(View, RHICmdList,
					[&View, &DrawRenderState](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
				{
					FBasePassMeshProcessor PassMeshProcessor(
						View.Family->Scene->GetRenderScene(),
						View.GetFeatureLevel(),
						&View,
						DrawRenderState,
						DynamicMeshPassContext,
						ETranslucencyPass::TPT_StandardTranslucency);

					const uint64 DefaultBatchElementMask = ~0ull;

					for (int32 MeshIndex = 0; MeshIndex < View.TopViewMeshElements.Num(); MeshIndex++)
					{
						const FMeshBatch& MeshBatch = View.TopViewMeshElements[MeshIndex];
						PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, nullptr);
					}
				});
			}
		}

		const FSceneViewState* ViewState = (const FSceneViewState*)View.State;
		if (ViewState && View.Family->EngineShowFlags.VisualizeLPV)
		{
			FLightPropagationVolume* LightPropagationVolume = ViewState->GetLightPropagationVolume(View.GetFeatureLevel());

			if (LightPropagationVolume)
			{
				LightPropagationVolume->Visualise(RHICmdList, View);
			}
		}
	}
}

void FDeferredShadingSceneRenderer::RenderViewTranslucency(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, const FMeshPassProcessorRenderState& DrawRenderState, ETranslucencyPass::Type TranslucencyPass)
{
	check(RHICmdList.IsInsideRenderPass());

	RenderViewTranslucencyInner(RHICmdList, View, DrawRenderState, TranslucencyPass, nullptr);
}

void FDeferredShadingSceneRenderer::RenderViewTranslucencyParallel(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, const FMeshPassProcessorRenderState& DrawRenderState, ETranslucencyPass::Type TranslucencyPass)
{
	check(RHICmdList.IsOutsideRenderPass());

	FTranslucencyPassParallelCommandListSet ParallelCommandListSet(
		View,
		this,
		RHICmdList,
		CVarRHICmdTranslucencyPassDeferredContexts.GetValueOnRenderThread() > 0,
		CVarRHICmdFlushRenderThreadTasksTranslucentPass.GetValueOnRenderThread() == 0 && CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() == 0,
		DrawRenderState,
		TranslucencyPass,
		FSceneRenderTargets::Get(RHICmdList).IsSeparateTranslucencyPass()
	);

	RenderViewTranslucencyInner(RHICmdList, View, DrawRenderState, TranslucencyPass, &ParallelCommandListSet);
}

void FDeferredShadingSceneRenderer::SetupDownsampledTranslucencyViewParameters(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	FViewUniformShaderParameters& DownsampledTranslucencyViewParameters)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FIntPoint ScaledSize;
	float DownsamplingScale = 1.f;
	SceneContext.GetSeparateTranslucencyDimensions(ScaledSize, DownsamplingScale);
	ensure(DownsamplingScale < 1.f);

	SceneContext.GetDownsampledTranslucencyDepth(RHICmdList, ScaledSize);
	DownsampleDepthSurface(RHICmdList, SceneContext.GetDownsampledTranslucencyDepthSurface(), View, DownsamplingScale, false);

	DownsampledTranslucencyViewParameters  = *View.CachedViewUniformShaderParameters;

	// Update the parts of DownsampledTranslucencyParameters which are dependent on the buffer size and view rect
	View.SetupViewRectUniformBufferParameters(
		DownsampledTranslucencyViewParameters,
		ScaledSize,
		FIntRect(View.ViewRect.Min.X * DownsamplingScale, View.ViewRect.Min.Y * DownsamplingScale, View.ViewRect.Max.X * DownsamplingScale, View.ViewRect.Max.Y * DownsamplingScale),
		View.ViewMatrices,
		View.PrevViewInfo.ViewMatrices
	);
}

void FDeferredShadingSceneRenderer::ConditionalResolveSceneColorForTranslucentMaterials(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& SceneColorCopy)
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		bool bNeedsResolve = false;
		for (int32 TranslucencyPass = 0; TranslucencyPass < ETranslucencyPass::TPT_MAX && !bNeedsResolve; ++TranslucencyPass)
		{
			bNeedsResolve |= View.TranslucentPrimCount.UseSceneColorCopy((ETranslucencyPass::Type)TranslucencyPass);
		}

		if (bNeedsResolve)
		{
			FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

			SCOPED_DRAW_EVENTF(RHICmdList, EventCopy, TEXT("CopySceneColor from SceneColor for translucency"));

			RHICmdList.CopyToResolveTarget(SceneContext.GetSceneColorSurface(), SceneContext.GetSceneColorTexture(), FResolveRect(View.ViewRect.Min.X, View.ViewRect.Min.Y, View.ViewRect.Max.X, View.ViewRect.Max.Y));

			if (!SceneColorCopy)
			{
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(SceneContext.GetBufferSizeXY(), PF_B8G8R8A8, FClearValueBinding::White, TexCreate_None, TexCreate_RenderTargetable, false));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SceneColorCopy, TEXT("SceneColorCopy"));
			}

			FRHIRenderPassInfo RPInfo(SceneColorCopy->GetRenderTargetItem().TargetableTexture, ERenderTargetActions::DontLoad_Store);
			TransitionRenderPassTargets(RHICmdList, RPInfo);
			RHICmdList.BeginRenderPass(RPInfo, TEXT("ResolveColorForTranslucentMaterials"));
			{

				RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();


				TShaderMapRef<FScreenVS> ScreenVertexShader(View.ShaderMap);
				TShaderMapRef<FCopySceneColorPS> PixelShader(View.ShaderMap);

				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*ScreenVertexShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

				PixelShader->SetParameters(RHICmdList, View);

				DrawRectangle(
					RHICmdList,
					0, 0,
					View.ViewRect.Width(), View.ViewRect.Height(),
					View.ViewRect.Min.X, View.ViewRect.Min.Y,
					View.ViewRect.Width(), View.ViewRect.Height(),
					FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
					SceneContext.GetBufferSizeXY(),
					*ScreenVertexShader,
					EDRF_UseTriangleOptimization);
			}
			RHICmdList.EndRenderPass();
			RHICmdList.CopyToResolveTarget(SceneColorCopy->GetRenderTargetItem().TargetableTexture, SceneColorCopy->GetRenderTargetItem().ShaderResourceTexture, FResolveParams());
		}
	}
}

void CreateTranslucentBasePassUniformBuffer(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	IPooledRenderTarget* SceneColorCopy,
	ESceneTextureSetupMode SceneTextureSetupMode,
	TUniformBufferRef<FTranslucentBasePassUniformParameters>& BasePassUniformBuffer,
	const int32 ViewIndex)
{
	FSceneRenderTargets& SceneRenderTargets = FSceneRenderTargets::Get(RHICmdList);

	FTranslucentBasePassUniformParameters BasePassParameters;
	SetupSharedBasePassParameters(RHICmdList, View, SceneRenderTargets, BasePassParameters.Shared);

	{
		SetupSceneTextureUniformParameters(SceneRenderTargets, View.FeatureLevel, SceneTextureSetupMode, BasePassParameters.SceneTextures);
		BasePassParameters.SceneTextures.EyeAdaptation = GetEyeAdaptation(View);
	}

	// Material SSR
	{
		float PrevSceneColorPreExposureInvValue = 1.0f / View.PreExposure;

		if (View.HZB)
		{
			BasePassParameters.HZBTexture = View.HZB->GetRenderTargetItem().ShaderResourceTexture;
			BasePassParameters.HZBSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

			const TRefCountPtr<IPooledRenderTarget>* PrevSceneColorRT = &GSystemTextures.BlackDummy;

			if (View.PrevViewInfo.CustomSSRInput.IsValid())
			{
				PrevSceneColorRT = &View.PrevViewInfo.CustomSSRInput;
				PrevSceneColorPreExposureInvValue = 1.0f / View.PrevViewInfo.TemporalAAHistory.SceneColorPreExposure;
			}
			else if (View.PrevViewInfo.TemporalAAHistory.IsValid())
			{
				PrevSceneColorRT = &View.PrevViewInfo.TemporalAAHistory.RT[0];
				PrevSceneColorPreExposureInvValue = 1.0f / View.PrevViewInfo.TemporalAAHistory.SceneColorPreExposure;
			}

			BasePassParameters.PrevSceneColor = (*PrevSceneColorRT)->GetRenderTargetItem().ShaderResourceTexture;
			BasePassParameters.PrevSceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

			const FVector2D HZBUvFactor(
				float(View.ViewRect.Width()) / float(2 * View.HZBMipmap0Size.X),
				float(View.ViewRect.Height()) / float(2 * View.HZBMipmap0Size.Y)
			);
			const FVector4 HZBUvFactorAndInvFactorValue(
				HZBUvFactor.X,
				HZBUvFactor.Y,
				1.0f / HZBUvFactor.X,
				1.0f / HZBUvFactor.Y
			);

			BasePassParameters.HZBUvFactorAndInvFactor = HZBUvFactorAndInvFactorValue;
		}
		else
		{
			BasePassParameters.HZBTexture = GBlackTexture->TextureRHI;
			BasePassParameters.HZBSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			BasePassParameters.PrevSceneColor = GBlackTexture->TextureRHI;
			BasePassParameters.PrevSceneColorSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		}

		FIntPoint ViewportOffset = View.ViewRect.Min;
		FIntPoint ViewportExtent = View.ViewRect.Size();
		FIntPoint BufferSize = SceneRenderTargets.GetBufferSizeXY();

		if (View.PrevViewInfo.TemporalAAHistory.IsValid())
		{
			ViewportOffset = View.PrevViewInfo.TemporalAAHistory.ViewportRect.Min;
			ViewportExtent = View.PrevViewInfo.TemporalAAHistory.ViewportRect.Size();
			BufferSize = View.PrevViewInfo.TemporalAAHistory.RT[0]->GetDesc().Extent;
		}

		FVector2D InvBufferSize(1.0f / float(BufferSize.X), 1.0f / float(BufferSize.Y));

		FVector4 ScreenPosToPixelValue(
			ViewportExtent.X * 0.5f * InvBufferSize.X,
			-ViewportExtent.Y * 0.5f * InvBufferSize.Y,
			(ViewportExtent.X * 0.5f + ViewportOffset.X) * InvBufferSize.X,
			(ViewportExtent.Y * 0.5f + ViewportOffset.Y) * InvBufferSize.Y);

		BasePassParameters.PrevScreenPositionScaleBias = ScreenPosToPixelValue;
		BasePassParameters.PrevSceneColorPreExposureInv = PrevSceneColorPreExposureInvValue;
	}

	// Translucency Lighting Volume
	{
		if (SceneRenderTargets.GetTranslucencyVolumeAmbient(TVC_Inner) != nullptr)
		{
			BasePassParameters.TranslucencyLightingVolumeAmbientInner = SceneRenderTargets.GetTranslucencyVolumeAmbient(TVC_Inner, ViewIndex)->GetRenderTargetItem().ShaderResourceTexture;
			BasePassParameters.TranslucencyLightingVolumeAmbientOuter = SceneRenderTargets.GetTranslucencyVolumeAmbient(TVC_Outer, ViewIndex)->GetRenderTargetItem().ShaderResourceTexture;
			BasePassParameters.TranslucencyLightingVolumeDirectionalInner = SceneRenderTargets.GetTranslucencyVolumeDirectional(TVC_Inner, ViewIndex)->GetRenderTargetItem().ShaderResourceTexture;
			BasePassParameters.TranslucencyLightingVolumeDirectionalOuter = SceneRenderTargets.GetTranslucencyVolumeDirectional(TVC_Outer, ViewIndex)->GetRenderTargetItem().ShaderResourceTexture;
		}
		else
		{
			const FTextureRHIRef DummyTLV = GSystemTextures.BlackDummy->GetRenderTargetItem().ShaderResourceTexture;
			BasePassParameters.TranslucencyLightingVolumeAmbientInner = DummyTLV;
			BasePassParameters.TranslucencyLightingVolumeAmbientOuter = DummyTLV;
			BasePassParameters.TranslucencyLightingVolumeDirectionalInner = DummyTLV;
			BasePassParameters.TranslucencyLightingVolumeDirectionalOuter = DummyTLV;
		}

		BasePassParameters.TranslucencyLightingVolumeAmbientInnerSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		BasePassParameters.TranslucencyLightingVolumeAmbientOuterSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		BasePassParameters.TranslucencyLightingVolumeDirectionalInnerSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		BasePassParameters.TranslucencyLightingVolumeDirectionalOuterSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	}

	BasePassParameters.SceneTextures.SceneColorCopyTexture = SceneColorCopy ? SceneColorCopy->GetRenderTargetItem().ShaderResourceTexture : GBlackTexture->TextureRHI;

	FScene* Scene = View.Family->Scene ? View.Family->Scene->GetRenderScene() : nullptr;

	if (Scene)
	{
		Scene->UniformBuffers.TranslucentBasePassUniformBuffer.UpdateUniformBufferImmediate(BasePassParameters);
		BasePassUniformBuffer = Scene->UniformBuffers.TranslucentBasePassUniformBuffer;
	}
	else
	{
		BasePassUniformBuffer = TUniformBufferRef<FTranslucentBasePassUniformParameters>::CreateUniformBufferImmediate(BasePassParameters, UniformBuffer_SingleFrame);
	}
}

class FTranslucencyUpsamplingPS : public FGlobalShader
{
protected:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FTranslucencyUpsamplingPS(bool InbUseNearestDepthNeighborUpsample)
		: bUseNearestDepthNeighborUpsample(InbUseNearestDepthNeighborUpsample)
	{
	}

	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderParameter LowResColorTexelSize;
	FShaderResourceParameter LowResDepthTexture;
	FShaderResourceParameter LowResColorTexture;
	FShaderResourceParameter BilinearClampedSampler;
	FShaderResourceParameter PointClampedSampler;

public:

	/** Initialization constructor. */
	FTranslucencyUpsamplingPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer, bool InbUseNearestDepthNeighborUpsample)
		: FGlobalShader(Initializer)
		, bUseNearestDepthNeighborUpsample(InbUseNearestDepthNeighborUpsample)
	{
		SceneTextureParameters.Bind(Initializer);
		LowResColorTexelSize.Bind(Initializer.ParameterMap, TEXT("LowResColorTexelSize"));
		LowResDepthTexture.Bind(Initializer.ParameterMap, TEXT("LowResDepthTexture"));
		LowResColorTexture.Bind(Initializer.ParameterMap, TEXT("LowResColorTexture"));
		BilinearClampedSampler.Bind(Initializer.ParameterMap, TEXT("BilinearClampedSampler"));
		PointClampedSampler.Bind(Initializer.ParameterMap, TEXT("PointClampedSampler"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneTextureParameters << LowResColorTexelSize << LowResDepthTexture << LowResColorTexture << BilinearClampedSampler << PointClampedSampler;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		TRefCountPtr<IPooledRenderTarget>& DownsampledTranslucency = SceneContext.SeparateTranslucencyRT;

		float Width = DownsampledTranslucency->GetDesc().Extent.X;
		float Height = DownsampledTranslucency->GetDesc().Extent.Y;
		SetShaderValue(RHICmdList, ShaderRHI, LowResColorTexelSize, FVector4(Width, Height, 1.0f / Width, 1.0f / Height));

		SetTextureParameter(RHICmdList, ShaderRHI, LowResColorTexture, DownsampledTranslucency->GetRenderTargetItem().ShaderResourceTexture);
		SetTextureParameter(RHICmdList, ShaderRHI, LowResDepthTexture, SceneContext.GetDownsampledTranslucencyDepthSurface());

		SetSamplerParameter(RHICmdList, ShaderRHI, BilinearClampedSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		SetSamplerParameter(RHICmdList, ShaderRHI, PointClampedSampler, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		SceneTextureParameters.Set(RHICmdList, GetPixelShader(), View.FeatureLevel, ESceneTextureSetupMode::All);
	}

	const bool bUseNearestDepthNeighborUpsample;
};

class FTranslucencySimpleUpsamplingPS : public FTranslucencyUpsamplingPS
{
protected:
	DECLARE_SHADER_TYPE(FTranslucencySimpleUpsamplingPS, Global);
	FTranslucencySimpleUpsamplingPS() : FTranslucencyUpsamplingPS(false) {}
public:
	FTranslucencySimpleUpsamplingPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FTranslucencyUpsamplingPS(Initializer, false) {}
};

IMPLEMENT_SHADER_TYPE(, FTranslucencySimpleUpsamplingPS, TEXT("/Engine/Private/TranslucencyUpsampling.usf"), TEXT("SimpleUpsamplingPS"), SF_Pixel);

class FTranslucencyNearestDepthNeighborUpsamplingPS : public FTranslucencyUpsamplingPS
{
protected:
	DECLARE_SHADER_TYPE(FTranslucencyNearestDepthNeighborUpsamplingPS, Global);
	FTranslucencyNearestDepthNeighborUpsamplingPS() : FTranslucencyUpsamplingPS(true) {}
public:
	FTranslucencyNearestDepthNeighborUpsamplingPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FTranslucencyUpsamplingPS(Initializer, true) {}
};

IMPLEMENT_SHADER_TYPE(, FTranslucencyNearestDepthNeighborUpsamplingPS, TEXT("/Engine/Private/TranslucencyUpsampling.usf"), TEXT("NearestDepthNeighborUpsamplingPS"), SF_Pixel);

bool UseNearestDepthNeighborUpsampleForSeparateTranslucency(const FSceneRenderTargets& SceneContext)
{
	FIntPoint OutScaledSize;
	float OutScale;
	SceneContext.GetSeparateTranslucencyDimensions(OutScaledSize, OutScale);

	return CVarSeparateTranslucencyUpsampleMode.GetValueOnRenderThread() != 0 && FMath::Abs(OutScale - .5f) < .001f;
}

void UpsampleTranslucency(FRHICommandList& RHICmdList, const FViewInfo& View, bool bOverwrite)
{
	SCOPED_DRAW_EVENTF(RHICmdList, EventUpsampleCopy, TEXT("Upsample translucency"));

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);
	RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	if (bOverwrite) // When overwriting, we also need to set the alpha as other translucent primitive could accumulate into the buffer.
	{
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	}
	else
	{
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_SourceAlpha>::GetRHI();
	}

	TShaderMapRef<FScreenVS> ScreenVertexShader(View.ShaderMap);
	FTranslucencyUpsamplingPS* UpsamplingPixelShader = nullptr;
	if (UseNearestDepthNeighborUpsampleForSeparateTranslucency(SceneContext))
	{
		TShaderMapRef<FTranslucencyNearestDepthNeighborUpsamplingPS> PixelShader(View.ShaderMap);
		UpsamplingPixelShader = *PixelShader;
	}
	else
	{
		TShaderMapRef<FTranslucencySimpleUpsamplingPS> PixelShader(View.ShaderMap);
		UpsamplingPixelShader = *PixelShader;
	}

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*ScreenVertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(UpsamplingPixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
	UpsamplingPixelShader->SetParameters(RHICmdList, View);

	FIntPoint OutScaledSize;
	float OutScale;
	SceneContext.GetSeparateTranslucencyDimensions(OutScaledSize, OutScale);

	TRefCountPtr<IPooledRenderTarget>& DownsampledTranslucency = SceneContext.SeparateTranslucencyRT;
	int32 TextureWidth = DownsampledTranslucency->GetDesc().Extent.X;
	int32 TextureHeight = DownsampledTranslucency->GetDesc().Extent.Y;

	DrawRectangle(
		RHICmdList,
		View.ViewRect.Min.X, View.ViewRect.Min.Y,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Min.X * OutScale, View.ViewRect.Min.Y * OutScale,
		View.ViewRect.Width() * OutScale, View.ViewRect.Height() * OutScale,
		View.ViewRect.Size(),
		FIntPoint(TextureWidth, TextureHeight),
		*ScreenVertexShader,
		EDRF_UseTriangleOptimization);

	SceneContext.FinishRenderingSceneColor(RHICmdList);
}

void FDeferredShadingSceneRenderer::RenderTranslucency(FRHICommandListImmediate& RHICmdList, ETranslucencyPass::Type TranslucencyPass, IPooledRenderTarget* SceneColorCopy)
{
	check(RHICmdList.IsOutsideRenderPass());

	if (!ShouldRenderTranslucency(TranslucencyPass))
	{
		return; // Early exit if nothing needs to be done.
	}

	SCOPED_DRAW_EVENT(RHICmdList, Translucency);
	SCOPED_GPU_STAT(RHICmdList, Translucency);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	// Support for parallel rendering.
	const bool bUseParallel = GRHICommandList.UseParallelAlgorithms() && CVarParallelTranslucency.GetValueOnRenderThread();
	if (bUseParallel)
	{
		SceneContext.AllocLightAttenuation(RHICmdList); // materials will attempt to get this texture before the deferred command to set it up executes
	}
	FScopedCommandListWaitForTasks Flusher(bUseParallel && (CVarRHICmdFlushRenderThreadTasksTranslucentPass.GetValueOnRenderThread() > 0 || CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() > 0), RHICmdList);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		checkSlow(RHICmdList.IsOutsideRenderPass());

		SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
		FViewInfo& View = Views[ViewIndex];
		if (!View.ShouldRenderView())
		{
			continue;
		}

#if STATS
		if (View.ViewState && IsMainTranslucencyPass(TranslucencyPass))
		{
			View.ViewState->TranslucencyTimer.Begin(RHICmdList);
		}
#endif

		Scene->UniformBuffers.UpdateViewUniformBuffer(View);

		TUniformBufferRef<FTranslucentBasePassUniformParameters> BasePassUniformBuffer;
		CreateTranslucentBasePassUniformBuffer(RHICmdList, View, SceneColorCopy, ESceneTextureSetupMode::All, BasePassUniformBuffer, ViewIndex);
		FMeshPassProcessorRenderState DrawRenderState(View, BasePassUniformBuffer);

		// If downsampling we need to render in the separate buffer. Otherwise we also need to render offscreen to apply TPT_TranslucencyAfterDOF
		if (RenderInSeparateTranslucency(SceneContext, TranslucencyPass, View.TranslucentPrimCount.DisableOffscreenRendering(TranslucencyPass)))
		{
			checkSlow(RHICmdList.IsOutsideRenderPass());

			FIntPoint ScaledSize;
			float DownsamplingScale = 1.f;
			SceneContext.GetSeparateTranslucencyDimensions(ScaledSize, DownsamplingScale);

			if (DownsamplingScale < 1.f)
			{
				FViewUniformShaderParameters DownsampledTranslucencyViewParameters;
				SetupDownsampledTranslucencyViewParameters(RHICmdList, View, DownsampledTranslucencyViewParameters);
				Scene->UniformBuffers.ViewUniformBuffer.UpdateUniformBufferImmediate(DownsampledTranslucencyViewParameters);
				DrawRenderState.SetViewUniformBuffer(Scene->UniformBuffers.ViewUniformBuffer);

				if ((View.IsInstancedStereoPass() || View.bIsMobileMultiViewEnabled) && View.Family->Views.Num() > 0)
				{
					// When drawing the left eye in a stereo scene, copy the right eye view values into the instanced view uniform buffer.
					const EStereoscopicPass StereoPassIndex = (View.StereoPass != eSSP_FULL) ? eSSP_RIGHT_EYE : eSSP_FULL;

					const FViewInfo& InstancedView = static_cast<const FViewInfo&>(View.Family->GetStereoEyeView(StereoPassIndex));
					SetupDownsampledTranslucencyViewParameters(RHICmdList, InstancedView, DownsampledTranslucencyViewParameters);
					Scene->UniformBuffers.InstancedViewUniformBuffer.UpdateUniformBufferImmediate(reinterpret_cast<FInstancedViewUniformShaderParameters&>(DownsampledTranslucencyViewParameters));
					DrawRenderState.SetInstancedViewUniformBuffer(Scene->UniformBuffers.InstancedViewUniformBuffer);
				}
			}
			if (TranslucencyPass == ETranslucencyPass::TPT_TranslucencyAfterDOF)
			{
				BeginTimingSeparateTranslucencyPass(RHICmdList, View);
			}

			SceneContext.BeginRenderingSeparateTranslucency(RHICmdList, View, *this, ViewIndex == 0 || View.Family->bMultiGPUForkAndJoin);

			// Draw only translucent prims that are in the SeparateTranslucency pass
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

			if (bUseParallel)
			{
				RHICmdList.EndRenderPass();
				RenderViewTranslucencyParallel(RHICmdList, View, DrawRenderState, TranslucencyPass);
			}
			else
			{
				RenderViewTranslucency(RHICmdList, View, DrawRenderState, TranslucencyPass);
				RHICmdList.EndRenderPass();
			}

			SceneContext.ResolveSeparateTranslucency(RHICmdList, View);

			if (TranslucencyPass == ETranslucencyPass::TPT_TranslucencyAfterDOF)
			{
				EndTimingSeparateTranslucencyPass(RHICmdList, View);
			}
			if (TranslucencyPass != ETranslucencyPass::TPT_TranslucencyAfterDOF)
			{
				UpsampleTranslucency(RHICmdList, View, false);
			}

			checkSlow(RHICmdList.IsOutsideRenderPass());
		}
		else
		{
			SceneContext.BeginRenderingTranslucency(RHICmdList, View, *this, ViewIndex == 0 || View.Family->bMultiGPUForkAndJoin);
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

			if (bUseParallel && !ViewFamily.UseDebugViewPS())
			{
				RHICmdList.EndRenderPass();
				RenderViewTranslucencyParallel(RHICmdList, View, DrawRenderState, TranslucencyPass);
			}
			else
			{
				RenderViewTranslucency(RHICmdList, View, DrawRenderState, TranslucencyPass);
				RHICmdList.EndRenderPass();
			}

			SceneContext.FinishRenderingTranslucency(RHICmdList);
		}

#if STATS
		if (View.ViewState && IsMainTranslucencyPass(TranslucencyPass))
		{
			STAT(View.ViewState->TranslucencyTimer.End(RHICmdList));
		}
#endif
	}

	checkSlow(RHICmdList.IsOutsideRenderPass());
}