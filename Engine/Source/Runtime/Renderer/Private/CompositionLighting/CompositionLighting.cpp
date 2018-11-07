// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CompositionLighting.cpp: The center for all deferred lighting activities.
=============================================================================*/

#include "CompositionLighting/CompositionLighting.h"
#include "ScenePrivate.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "PostProcess/PostProcessInput.h"
#include "PostProcess/PostProcessing.h"
#include "CompositionLighting/PostProcessAmbient.h"
#include "CompositionLighting/PostProcessLpvIndirect.h"
#include "CompositionLighting/PostProcessAmbientOcclusion.h"
#include "CompositionLighting/PostProcessDeferredDecals.h"
#include "PostProcess/PostProcessSubsurface.h"
#include "LightPropagationVolumeSettings.h"
#include "DecalRenderingShared.h"

/** The global center for all deferred lighting activities. */
FCompositionLighting GCompositionLighting;

DECLARE_GPU_STAT_NAMED(CompositionBeforeBasePass, TEXT("Composition BeforeBasePass") );
DECLARE_GPU_STAT_NAMED(CompositionPreLighting, TEXT("Composition PreLighting") );
DECLARE_GPU_STAT_NAMED(CompositionLpvIndirect, TEXT("Composition LpvIndirect") );
DECLARE_GPU_STAT_NAMED(CompositionPostLighting, TEXT("Composition PostLighting") );

// -------------------------------------------------------

static TAutoConsoleVariable<float> CVarSSSScale(
	TEXT("r.SSS.Scale"),
	1.0f,
	TEXT("Affects the Screen space subsurface scattering pass")
	TEXT("(use shadingmodel SubsurfaceProfile, get near to the object as the default)\n")
	TEXT("is human skin which only scatters about 1.2cm)\n")
	TEXT(" 0: off (if there is no object on the screen using this pass it should automatically disable the post process pass)\n")
	TEXT("<1: scale scatter radius down (for testing)\n")
	TEXT(" 1: use given radius form the Subsurface scattering asset (default)\n")
	TEXT(">1: scale scatter radius up (for testing)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSSSHalfRes(
	TEXT("r.SSS.HalfRes"),
	1,
	TEXT(" 0: full quality (not optimized, as reference)\n")
	TEXT(" 1: parts of the algorithm runs in half resolution which is lower quality but faster (default)"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarSubsurfaceScattering(
	TEXT("r.SubsurfaceScattering"),
	1,
	TEXT(" 0: disabled\n")
	TEXT(" 1: enabled (default)"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

bool IsAmbientCubemapPassRequired(const FSceneView& View)
{
	FScene* Scene = (FScene*)View.Family->Scene;

	return View.FinalPostProcessSettings.ContributingCubemaps.Num() != 0 && IsUsingGBuffers(View.GetShaderPlatform());
}

bool IsLpvIndirectPassRequired(const FViewInfo& View)
{
	FScene* Scene = (FScene*)View.Family->Scene;

	const FSceneViewState* ViewState = (FSceneViewState*)View.State;

	if(ViewState)
	{
		// This check should be inclusive to stereo views
		const bool bIncludeStereoViews = true;

		FLightPropagationVolume* LightPropagationVolume = ViewState->GetLightPropagationVolume(View.GetFeatureLevel(), bIncludeStereoViews);

		if(LightPropagationVolume)
		{
			const FLightPropagationVolumeSettings& LPVSettings = View.FinalPostProcessSettings.BlendableManager.GetSingleFinalDataConst<FLightPropagationVolumeSettings>();

			if(LPVSettings.LPVIntensity > 0.0f)
			{
				return true;
			}
		}
	}

	return false;
}

static bool IsReflectionEnvironmentActive(const FSceneView& View)
{
	FScene* Scene = (FScene*)View.Family->Scene;

	// LPV & Screenspace Reflections : Reflection Environment active if either LPV (assumed true if this was called), Reflection Captures or SSR active

	bool IsReflectingEnvironment = View.Family->EngineShowFlags.ReflectionEnvironment;
	bool HasReflectionCaptures = (Scene->ReflectionSceneData.RegisteredReflectionCaptures.Num() > 0);
	bool HasSSR = View.Family->EngineShowFlags.ScreenSpaceReflections;

	return (Scene->GetFeatureLevel() == ERHIFeatureLevel::SM5 && IsReflectingEnvironment && (HasReflectionCaptures || HasSSR) && !IsAnyForwardShadingEnabled(View.GetShaderPlatform()));
}

static bool IsSkylightActive(const FViewInfo& View)
{
	FScene* Scene = (FScene*)View.Family->Scene;
	return Scene->SkyLight 
		&& Scene->SkyLight->ProcessedTexture
		&& View.Family->EngineShowFlags.SkyLighting;
}

bool ShouldRenderScreenSpaceAmbientOcclusion(const FViewInfo& View)
{
	bool bEnabled = true;

	if (!IsLpvIndirectPassRequired(View))
	{
		bEnabled = View.FinalPostProcessSettings.AmbientOcclusionIntensity > 0
			&& View.Family->EngineShowFlags.Lighting
			&& View.FinalPostProcessSettings.AmbientOcclusionRadius >= 0.1f
			&& !View.Family->UseDebugViewPS()
			&& (FSSAOHelper::IsBasePassAmbientOcclusionRequired(View) || IsAmbientCubemapPassRequired(View) || IsReflectionEnvironmentActive(View) || IsSkylightActive(View) || View.Family->EngineShowFlags.VisualizeBuffer)
			&& !IsSimpleForwardShadingEnabled(View.GetShaderPlatform());
	}

	return bEnabled;
}

static void AddPostProcessingAmbientCubemap(FPostprocessContext& Context, FRenderingCompositeOutputRef AmbientOcclusion)
{
	FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbient());
	Pass->SetInput(ePId_Input0, Context.FinalOutput);
	Pass->SetInput(ePId_Input1, AmbientOcclusion);

	Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
}

// @param Levels 0..3, how many different resolution levels we want to render
static FRenderingCompositeOutputRef AddPostProcessingAmbientOcclusion(FRHICommandListImmediate& RHICmdList, FPostprocessContext& Context, uint32 Levels)
{
	check(Levels >= 0 && Levels <= 3);

	FRenderingCompositePass* AmbientOcclusionInMip1 = nullptr;
	FRenderingCompositePass* AmbientOcclusionInMip2 = nullptr;
	FRenderingCompositePass* AmbientOcclusionPassMip1 = nullptr; 
	FRenderingCompositePass* AmbientOcclusionPassMip2 = nullptr;

	FRenderingCompositePass* HZBInput = Context.Graph.RegisterPass(new FRCPassPostProcessInput(const_cast<FViewInfo&>(Context.View).HZB));
	{
		// generate input in half, quarter, .. resolution
		ESSAOType DownResAOType = FSSAOHelper::IsAmbientOcclusionCompute(Context.View) ? ESSAOType::ECS : ESSAOType::EPS;
		if (Levels >= 2)
		{
			AmbientOcclusionInMip1 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbientOcclusionSetup());
			AmbientOcclusionInMip1->SetInput(ePId_Input0, Context.SceneDepth);
		}

		if (Levels >= 3)
		{
			AmbientOcclusionInMip2 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbientOcclusionSetup());
			AmbientOcclusionInMip2->SetInput(ePId_Input1, FRenderingCompositeOutputRef(AmbientOcclusionInMip1, ePId_Output0));
		}		

		// upsample from lower resolution

		if (Levels >= 3)
		{
			AmbientOcclusionPassMip2 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbientOcclusion(Context.View, DownResAOType));
			AmbientOcclusionPassMip2->SetInput(ePId_Input0, AmbientOcclusionInMip2);
			AmbientOcclusionPassMip2->SetInput(ePId_Input1, AmbientOcclusionInMip2);
			AmbientOcclusionPassMip2->SetInput(ePId_Input3, HZBInput);
		}

		if (Levels >= 2)
		{
			AmbientOcclusionPassMip1 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbientOcclusion(Context.View, DownResAOType));
			AmbientOcclusionPassMip1->SetInput(ePId_Input0, AmbientOcclusionInMip1);
			AmbientOcclusionPassMip1->SetInput(ePId_Input1, AmbientOcclusionInMip1);
			AmbientOcclusionPassMip1->SetInput(ePId_Input2, AmbientOcclusionPassMip2);
			AmbientOcclusionPassMip1->SetInput(ePId_Input3, HZBInput);
		}
	}

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FRenderingCompositePass* GBufferA = nullptr;
	
	// finally full resolution
	ESSAOType FullResAOType = ESSAOType::EPS;
	{
		if(FSSAOHelper::IsAmbientOcclusionCompute(Context.View))
		{
			if(FSSAOHelper::IsAmbientOcclusionAsyncCompute(Context.View, Levels) && GSupportsEfficientAsyncCompute)
			{
				FullResAOType = ESSAOType::EAsyncCS;
			}
			else
			{
				FullResAOType = ESSAOType::ECS;
			}
		}
	}

	if (SceneContext.GBufferA)
	{
		GBufferA = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(SceneContext.GBufferA));
	}

	FRenderingCompositePass* AmbientOcclusionPassMip0 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbientOcclusion(Context.View, FullResAOType, false));
	AmbientOcclusionPassMip0->SetInput(ePId_Input0, GBufferA);
	AmbientOcclusionPassMip0->SetInput(ePId_Input1, AmbientOcclusionInMip1);
	AmbientOcclusionPassMip0->SetInput(ePId_Input2, AmbientOcclusionPassMip1);
	AmbientOcclusionPassMip0->SetInput(ePId_Input3, HZBInput);

	// to make sure this pass is processed as well (before), needed to make process decals before computing AO
	if(AmbientOcclusionInMip1)
	{
		AmbientOcclusionInMip1->AddDependency(Context.FinalOutput);
	}
	else
	{
		AmbientOcclusionPassMip0->AddDependency(Context.FinalOutput);
	}

	Context.FinalOutput = FRenderingCompositeOutputRef(AmbientOcclusionPassMip0);

	SceneContext.bScreenSpaceAOIsValid = true;

	return FRenderingCompositeOutputRef(AmbientOcclusionPassMip0);
}

void FCompositionLighting::ProcessBeforeBasePass(FRHICommandListImmediate& RHICmdList, FViewInfo& View, bool bDBuffer, uint32 SSAOLevels)
{
	check(IsInRenderingThread());

	// so that the passes can register themselves to the graph
	if (bDBuffer || SSAOLevels)
	{
		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);

		FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);

		// Add the passes we want to add to the graph (commenting a line means the pass is not inserted into the graph) ----------

		// decals are before AmbientOcclusion so the decal can output a normal that AO is affected by
		if (bDBuffer) 
		{
			FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDeferredDecals(DRS_BeforeBasePass));
			Pass->SetInput(ePId_Input0, Context.FinalOutput);

			Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
		}

		if (SSAOLevels)
		{
			AddPostProcessingAmbientOcclusion(RHICmdList, Context, SSAOLevels);
		}

		// The graph setup should be finished before this line ----------------------------------------

		SCOPED_DRAW_EVENT(RHICmdList, CompositionBeforeBasePass);
		SCOPED_GPU_STAT(RHICmdList, CompositionBeforeBasePass);

		CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("Composition_BeforeBasePass"));
	}
}

void FCompositionLighting::ProcessAfterBasePass(FRHICommandListImmediate& RHICmdList, FViewInfo& View)
{
	check(IsInRenderingThread());
	
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	// might get renamed to refracted or ...WithAO
	SceneContext.GetSceneColor()->SetDebugName(TEXT("SceneColor"));
	// to be able to observe results with VisualizeTexture

	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GetSceneColor());
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GBufferA);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GBufferB);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GBufferC);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GBufferD);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GBufferE);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GBufferVelocity);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.ScreenSpaceAO);
	
	// so that the passes can register themselves to the graph
	{
		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);

		FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);

		// Add the passes we want to add to the graph ----------
		
		if( Context.View.Family->EngineShowFlags.Decals &&
			!Context.View.Family->EngineShowFlags.ShaderComplexity)
		{
			// DRS_AfterBasePass is for Volumetric decals which don't support ShaderComplexity yet
			FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDeferredDecals(DRS_AfterBasePass));
			Pass->SetInput(ePId_Input0, Context.FinalOutput);

			Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
		}

		// decal are distracting when looking at LightCulling.
		bool bDoDecal = Context.View.Family->EngineShowFlags.Decals && !Context.View.Family->EngineShowFlags.VisualizeLightCulling;

		if (bDoDecal && IsUsingGBuffers(View.GetShaderPlatform()))
		{
			// decals are before AmbientOcclusion so the decal can output a normal that AO is affected by
			FRenderingCompositePass* BeforeLightingPass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDeferredDecals(DRS_BeforeLighting));
			BeforeLightingPass->SetInput(ePId_Input0, Context.FinalOutput);
			Context.FinalOutput = FRenderingCompositeOutputRef(BeforeLightingPass);
		}

		if (bDoDecal && !IsSimpleForwardShadingEnabled(View.GetShaderPlatform()))
		{
			// DBuffer decals with emissive component
			FRenderingCompositePass* EmissivePass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDeferredDecals(DRS_Emissive));
			EmissivePass->SetInput(ePId_Input0, Context.FinalOutput);
			Context.FinalOutput = FRenderingCompositeOutputRef(EmissivePass);
		}

		// Forwared shading SSAO is applied before the basepass using only the depth buffer.
		if (!IsForwardShadingEnabled(View.GetShaderPlatform()))
		{
			FRenderingCompositeOutputRef AmbientOcclusion;

			uint32 SSAOLevels = FSSAOHelper::ComputeAmbientOcclusionPassCount(Context.View);
			if (SSAOLevels)
			{
				if(!FSSAOHelper::IsAmbientOcclusionAsyncCompute(Context.View, SSAOLevels))
				{
					AmbientOcclusion = AddPostProcessingAmbientOcclusion(RHICmdList, Context, SSAOLevels);

					if (bDoDecal)
					{
						FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDeferredDecals(DRS_AmbientOcclusion));
						Pass->AddDependency(Context.FinalOutput);

						Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
					}
				}
				else
				{
					ensureMsgf(
						FDecalRendering::BuildVisibleDecalList(*(FScene*)Context.View.Family->Scene, Context.View, DRS_AmbientOcclusion, nullptr) == false,
						TEXT("Ambient occlusion decals are not supported with Async compute SSAO."));
				}

				if (FSSAOHelper::IsBasePassAmbientOcclusionRequired(Context.View))
				{
					FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBasePassAO());
					Pass->AddDependency(Context.FinalOutput);

					Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
				}
			}

			if (IsAmbientCubemapPassRequired(Context.View))
			{
				AddPostProcessingAmbientCubemap(Context, AmbientOcclusion);
			}
		}

		// The graph setup should be finished before this line ----------------------------------------

		SCOPED_DRAW_EVENT(RHICmdList, LightCompositionTasks_PreLighting);
		SCOPED_GPU_STAT(RHICmdList, CompositionPreLighting);

		TRefCountPtr<IPooledRenderTarget>& SceneColor = SceneContext.GetSceneColor();

		Context.FinalOutput.GetOutput()->RenderTargetDesc = SceneColor->GetDesc();
		Context.FinalOutput.GetOutput()->PooledRenderTarget = SceneColor;

		CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("CompositionLighting_AfterBasePass"));
	}
}


void FCompositionLighting::ProcessLpvIndirect(FRHICommandListImmediate& RHICmdList, FViewInfo& View)
{
	check(IsInRenderingThread());
	
	FMemMark Mark(FMemStack::Get());
	FRenderingCompositePassContext CompositeContext(RHICmdList, View);
	FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);

	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		FRenderingCompositePass* SSAO = Context.Graph.RegisterPass(new FRCPassPostProcessInput(SceneContext.ScreenSpaceAO));

		FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new FRCPassPostProcessLpvIndirect());
		Pass->SetInput(ePId_Input0, Context.FinalOutput);
		Pass->SetInput(ePId_Input1, SSAO );

		Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
	}

	// The graph setup should be finished before this line ----------------------------------------

	SCOPED_DRAW_EVENT(RHICmdList, CompositionLpvIndirect);
	SCOPED_GPU_STAT(RHICmdList, CompositionLpvIndirect);

	// we don't replace the final element with the scenecolor because this is what those passes should do by themself

	CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("CompositionLighting"));
}

void FCompositionLighting::ProcessAfterLighting(FRHICommandListImmediate& RHICmdList, FViewInfo& View)
{
	check(IsInRenderingThread());
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	{
		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);
		FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);
		FRenderingCompositeOutputRef AmbientOcclusion;

		// Screen Space Subsurface Scattering
		{
			float Radius = CVarSSSScale.GetValueOnRenderThread();
			bool bSimpleDynamicLighting = IsAnyForwardShadingEnabled(View.GetShaderPlatform());
			bool bScreenSpaceSubsurfacePassNeeded = ((View.ShadingModelMaskInView & GetUseSubsurfaceProfileShadingModelMask()) != 0) && IsSubsurfacePostprocessRequired();
			bool bSubsurfaceAllowed = CVarSubsurfaceScattering.GetValueOnRenderThread() == 1;

			if (bScreenSpaceSubsurfacePassNeeded 
				&& !bSimpleDynamicLighting 
				&& bSubsurfaceAllowed)
			{
				bool bHalfRes = CVarSSSHalfRes.GetValueOnRenderThread() != 0;
				bool bSingleViewportMode = View.Family->Views.Num() == 1;

				if(Radius > 0 && View.Family->EngineShowFlags.SubsurfaceScattering)
				{
					FRenderingCompositePass* PassSetup = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSubsurfaceSetup(View, bHalfRes));
					PassSetup->SetInput(ePId_Input0, Context.FinalOutput);

					FRenderingCompositePass* PassX = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSubsurface(0, bHalfRes));
					PassX->SetInput(ePId_Input0, PassSetup);

					FRenderingCompositePass* PassY = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSubsurface(1, bHalfRes));
					PassY->SetInput(ePId_Input0, PassX);
					PassY->SetInput(ePId_Input1, PassSetup);

					// full res composite pass, no blurring (Radius=0), replaces SceneColor
					FRenderingCompositePass* RecombinePass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSubsurfaceRecombine(bHalfRes, bSingleViewportMode));
					RecombinePass->SetInput(ePId_Input0, Context.FinalOutput);
					RecombinePass->SetInput(ePId_Input1, PassY);
					RecombinePass->SetInput(ePId_Input2, PassSetup);
					Context.FinalOutput = FRenderingCompositeOutputRef(RecombinePass);
				}
				else
				{
					// needed for Scalability
					FRenderingCompositePass* RecombinePass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSubsurfaceRecombine(bHalfRes, bSingleViewportMode));
					RecombinePass->SetInput(ePId_Input0, Context.FinalOutput);
					Context.FinalOutput = FRenderingCompositeOutputRef(RecombinePass);
				}
			}
		}

		// The graph setup should be finished before this line ----------------------------------------

		SCOPED_DRAW_EVENT(RHICmdList, CompositionAfterLighting);
		SCOPED_GPU_STAT(RHICmdList, CompositionPostLighting);

		// we don't replace the final element with the scenecolor because this is what those passes should do by themself

		CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("CompositionLighting"));
	}

	// We only release the after the last view was processed (SplitScreen)
	if(View.Family->Views[View.Family->Views.Num() - 1] == &View)
	{
		// The RT should be released as early as possible to allow sharing of that memory for other purposes.
		// This becomes even more important with some limited VRam (XBoxOne).
		SceneContext.SetLightAttenuation(0);
	}
}

bool FCompositionLighting::CanProcessAsyncSSAO(TArray<FViewInfo>& Views)
{
	bool bAnyAsyncSSAO = true;
	for (int32 i = 0; i < Views.Num(); ++i)
	{
		uint32 Levels = FSSAOHelper::ComputeAmbientOcclusionPassCount(Views[i]);
		if (!FSSAOHelper::IsAmbientOcclusionAsyncCompute(Views[i], Levels))
		{
			bAnyAsyncSSAO = false;
			break;
		}
	}
	return bAnyAsyncSSAO;
}

void FCompositionLighting::PrepareAsyncSSAO(FRHICommandListImmediate& RHICmdList, TArray<FViewInfo>& Views)
{
	//clear out last frame's fence.
	ensureMsgf(AsyncSSAOFence == nullptr, TEXT("Old AsyncCompute SSAO fence has not been cleared."));

	static FName AsyncSSAOFenceName(TEXT("AsyncSSAOFence"));
	AsyncSSAOFence = RHICmdList.CreateComputeFence(AsyncSSAOFenceName);

	//Grab the async compute commandlist.
	FRHIAsyncComputeCommandListImmediate& RHICmdListComputeImmediate = FRHICommandListExecutor::GetImmediateAsyncComputeCommandList();
	RHICmdListComputeImmediate.SetAsyncComputeBudget(FSSAOHelper::GetAmbientOcclusionAsyncComputeBudget());
}

void FCompositionLighting::ProcessAsyncSSAO(FRHICommandListImmediate& RHICmdList, TArray<FViewInfo>& Views)
{
	check(IsInRenderingThread());
	if (GSupportsEfficientAsyncCompute)
	{
		PrepareAsyncSSAO(RHICmdList, Views);

		// so that the passes can register themselves to the graph
		for (int32 i = 0; i < Views.Num(); ++i)
		{
			FViewInfo& View = Views[i];
			FMemMark Mark(FMemStack::Get());
			FRenderingCompositePassContext CompositeContext(RHICmdList, View);

			// Add the passes we want to add to the graph (commenting a line means the pass is not inserted into the graph) ----------		
			uint32 Levels = FSSAOHelper::ComputeAmbientOcclusionPassCount(View);		
			if (FSSAOHelper::IsAmbientOcclusionAsyncCompute(View, Levels))
			{
				FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);

				FRenderingCompositeOutputRef AmbientOcclusion = AddPostProcessingAmbientOcclusion(RHICmdList, Context, Levels);
				Context.FinalOutput = FRenderingCompositeOutputRef(AmbientOcclusion);			

				// The graph setup should be finished before this line ----------------------------------------
				CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("Composition_ProcessAsyncSSAO"));
			}		
		}
		FinishAsyncSSAO(RHICmdList);
	}
	else
	{
		// so that the passes can register themselves to the graph
		for (int32 i = 0; i < Views.Num(); ++i)
		{
			FViewInfo& View = Views[i];
			FMemMark Mark(FMemStack::Get());
			FRenderingCompositePassContext CompositeContext(RHICmdList, View);

			// Add the passes we want to add to the graph (commenting a line means the pass is not inserted into the graph) ----------		
			if (FSSAOHelper::IsAmbientOcclusionCompute(View))
			{
				FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);

				FRenderingCompositeOutputRef AmbientOcclusion = AddPostProcessingAmbientOcclusion(RHICmdList, Context, 1);
				Context.FinalOutput = FRenderingCompositeOutputRef(AmbientOcclusion);			

				// The graph setup should be finished before this line ----------------------------------------
				CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("Composition_ProcessSSAO"));
			}		
		}
	}
}

void FCompositionLighting::FinishAsyncSSAO(FRHICommandListImmediate& RHICmdList)
{
	if (AsyncSSAOFence)
	{
		//Grab the async compute commandlist.
		FRHIAsyncComputeCommandListImmediate& RHICmdListComputeImmediate = FRHICommandListExecutor::GetImmediateAsyncComputeCommandList();

		RHICmdListComputeImmediate.SetAsyncComputeBudget(EAsyncComputeBudget::EAll_4);
		RHICmdListComputeImmediate.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, nullptr, 0, AsyncSSAOFence);
		FRHIAsyncComputeCommandListImmediate::ImmediateDispatch(RHICmdListComputeImmediate);		
	}
}

void FCompositionLighting::GfxWaitForAsyncSSAO(FRHICommandListImmediate& RHICmdList)
{
	if (AsyncSSAOFence)
	{
		RHICmdList.WaitComputeFence(AsyncSSAOFence);
		AsyncSSAOFence = nullptr;
	}
}

bool FCompositionLighting::IsSubsurfacePostprocessRequired() const
{
	const bool bSSSEnabled = CVarSubsurfaceScattering->GetInt() != 0;
	const bool bSSSScaleEnabled = CVarSSSScale.GetValueOnAnyThread() > 0.0f;
	return (bSSSEnabled && bSSSScaleEnabled);	
}
