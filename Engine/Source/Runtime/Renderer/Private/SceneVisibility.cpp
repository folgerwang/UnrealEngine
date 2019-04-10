// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneVisibility.cpp: Scene visibility determination.
=============================================================================*/

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "Stats/Stats.h"
#include "Misc/MemStack.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Async/TaskGraphInterfaces.h"
#include "EngineDefines.h"
#include "EngineGlobals.h"
#include "RHIDefinitions.h"
#include "SceneTypes.h"
#include "SceneInterface.h"
#include "RendererInterface.h"
#include "PrimitiveViewRelevance.h"
#include "MaterialShared.h"
#include "SceneManagement.h"
#include "ScenePrivateBase.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneCore.h"
#include "LightSceneInfo.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "DynamicPrimitiveDrawing.h"
#include "ScenePrivate.h"
#include "FXSystem.h"
#include "PostProcess/PostProcessing.h"
#include "SceneView.h"
#include "SceneSoftwareOcclusion.h"
#include "Engine/LODActor.h"
#include "GPUScene.h"
#include "TranslucentRendering.h"
#include "Async/ParallelFor.h"

/*------------------------------------------------------------------------------
	Globals
------------------------------------------------------------------------------*/

static float GWireframeCullThreshold = 5.0f;
static FAutoConsoleVariableRef CVarWireframeCullThreshold(
	TEXT("r.WireframeCullThreshold"),
	GWireframeCullThreshold,
	TEXT("Threshold below which objects in ortho wireframe views will be culled."),
	ECVF_RenderThreadSafe
	);

float GMinScreenRadiusForLights = 0.03f;
static FAutoConsoleVariableRef CVarMinScreenRadiusForLights(
	TEXT("r.MinScreenRadiusForLights"),
	GMinScreenRadiusForLights,
	TEXT("Threshold below which lights will be culled."),
	ECVF_RenderThreadSafe
	);

float GMinScreenRadiusForDepthPrepass = 0.03f;
static FAutoConsoleVariableRef CVarMinScreenRadiusForDepthPrepass(
	TEXT("r.MinScreenRadiusForDepthPrepass"),
	GMinScreenRadiusForDepthPrepass,
	TEXT("Threshold below which meshes will be culled from depth only pass."),
	ECVF_RenderThreadSafe
	);

float GMinScreenRadiusForCSMDepth = 0.01f;
static FAutoConsoleVariableRef CVarMinScreenRadiusForCSMDepth(
	TEXT("r.MinScreenRadiusForCSMDepth"),
	GMinScreenRadiusForCSMDepth,
	TEXT("Threshold below which meshes will be culled from CSM depth pass."),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarTemporalAASamples(
	TEXT("r.TemporalAASamples"),
	8,
	TEXT("Number of jittered positions for temporal AA (4, 8=default, 16, 32, 64)."),
	ECVF_RenderThreadSafe);

static int32 GHZBOcclusion = 0;
static FAutoConsoleVariableRef CVarHZBOcclusion(
	TEXT("r.HZBOcclusion"),
	GHZBOcclusion,
	TEXT("Defines which occlusion system is used.\n")
	TEXT(" 0: Hardware occlusion queries\n")
	TEXT(" 1: Use HZB occlusion system (default, less GPU and CPU cost, more conservative results)")
	TEXT(" 2: Force HZB occlusion system (overrides rendering platform preferences)"),
	ECVF_RenderThreadSafe
	);

static int32 GVisualizeOccludedPrimitives = 0;
static FAutoConsoleVariableRef CVarVisualizeOccludedPrimitives(
	TEXT("r.VisualizeOccludedPrimitives"),
	GVisualizeOccludedPrimitives,
	TEXT("Draw boxes for all occluded primitives"),
	ECVF_RenderThreadSafe | ECVF_Cheat
	);

static int32 GAllowSubPrimitiveQueries = 1;
static FAutoConsoleVariableRef CVarAllowSubPrimitiveQueries(
	TEXT("r.AllowSubPrimitiveQueries"),
	GAllowSubPrimitiveQueries,
	TEXT("Enables sub primitive queries, currently only used by hierarchical instanced static meshes. 1: Enable, 0 Disabled. When disabled, one query is used for the entire proxy."),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarStaticMeshLODDistanceScale(
	TEXT("r.StaticMeshLODDistanceScale"),
	1.0f,
	TEXT("Scale factor for the distance used in computing discrete LOD for static meshes. (defaults to 1)\n")
	TEXT("(higher values make LODs transition earlier, e.g., 2 is twice as fast / half the distance)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarMinAutomaticViewMipBias(
	TEXT("r.ViewTextureMipBias.Min"),
	-1.0f,
	TEXT("Automatic view mip bias's minimum value (default to -1)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarMinAutomaticViewMipBiasOffset(
	TEXT("r.ViewTextureMipBias.Offset"),
	-0.3,
	TEXT("Automatic view mip bias's constant offset (default to -0.3)."),
	ECVF_RenderThreadSafe);


static int32 GOcclusionCullParallelPrimFetch = 0;
static FAutoConsoleVariableRef CVarOcclusionCullParallelPrimFetch(
	TEXT("r.OcclusionCullParallelPrimFetch"),
	GOcclusionCullParallelPrimFetch,
	TEXT("Enables Parallel Occlusion Cull primitive fetch."),
	ECVF_RenderThreadSafe
	);

static int32 GILCUpdatePrimTaskEnabled = 1;

static FAutoConsoleVariableRef CVarILCUpdatePrimitivesTask(
	TEXT("r.Cache.UpdatePrimsTaskEnabled"),
	GILCUpdatePrimTaskEnabled,
	TEXT("Enable threading for ILC primitive update.  Will overlap with the rest the end of InitViews."),
	ECVF_RenderThreadSafe
	);

static int32 GDoInitViewsLightingAfterPrepass = 0;
static FAutoConsoleVariableRef CVarDoInitViewsLightingAfterPrepass(
	TEXT("r.DoInitViewsLightingAfterPrepass"),
	GDoInitViewsLightingAfterPrepass,
	TEXT("Delays the lighting part of InitViews until after the prepass. This improves the threading throughput and gets the prepass to the GPU ASAP. Experimental options; has an unknown race."),
	ECVF_RenderThreadSafe
	);

static int32 GFramesNotOcclusionTestedToExpandBBoxes = 5;
static FAutoConsoleVariableRef CVarFramesNotOcclusionTestedToExpandBBoxes(
	TEXT("r.GFramesNotOcclusionTestedToExpandBBoxes"),
	GFramesNotOcclusionTestedToExpandBBoxes,
	TEXT("If we don't occlusion test a primitive for this many frames, then we expand the BBox when we do occlusion test it for a few frames. See also r.ExpandNewlyOcclusionTestedBBoxesAmount, r.FramesToExpandNewlyOcclusionTestedBBoxes"),
	ECVF_RenderThreadSafe
);

static int32 GFramesToExpandNewlyOcclusionTestedBBoxes = 2;
static FAutoConsoleVariableRef CVarFramesToExpandNewlyOcclusionTestedBBoxes(
	TEXT("r.FramesToExpandNewlyOcclusionTestedBBoxes"),
	GFramesToExpandNewlyOcclusionTestedBBoxes,
	TEXT("If we don't occlusion test a primitive for r.GFramesNotOcclusionTestedToExpandBBoxes frames, then we expand the BBox when we do occlusion test it for this number of frames. See also r.GFramesNotOcclusionTestedToExpandBBoxes, r.ExpandNewlyOcclusionTestedBBoxesAmount"),
	ECVF_RenderThreadSafe
);

static float GExpandNewlyOcclusionTestedBBoxesAmount = 0.0f;
static FAutoConsoleVariableRef CVarExpandNewlyOcclusionTestedBBoxesAmount(
	TEXT("r.ExpandNewlyOcclusionTestedBBoxesAmount"),
	GExpandNewlyOcclusionTestedBBoxesAmount,
	TEXT("If we don't occlusion test a primitive for r.GFramesNotOcclusionTestedToExpandBBoxes frames, then we expand the BBox when we do occlusion test it for a few frames by this amount. See also r.FramesToExpandNewlyOcclusionTestedBBoxes, r.GFramesNotOcclusionTestedToExpandBBoxes."),
	ECVF_RenderThreadSafe
);

static float GExpandAllTestedBBoxesAmount = 0.0f;
static FAutoConsoleVariableRef CVarExpandAllTestedBBoxesAmount(
	TEXT("r.ExpandAllOcclusionTestedBBoxesAmount"),
	GExpandAllTestedBBoxesAmount,
	TEXT("Amount to expand all occlusion test bounds by."),
	ECVF_RenderThreadSafe
);

static float GNeverOcclusionTestDistance = 0.0f;
static FAutoConsoleVariableRef CVarNeverOcclusionTestDistance(
	TEXT("r.NeverOcclusionTestDistance"),
	GNeverOcclusionTestDistance,
	TEXT("When the distance between the viewpoint and the bounding sphere center is less than this, never occlusion cull."),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static int32 GForceSceneHasDecals = 0;
static FAutoConsoleVariableRef CVarForceSceneHasDecals(
	TEXT("r.ForceSceneHasDecals"),
	GForceSceneHasDecals,
	TEXT("Whether to always assume that scene has decals, so we don't switch depth state conditionally. This can significantly reduce total number of PSOs at a minor GPU cost."),
	ECVF_RenderThreadSafe
);

/** Distance fade cvars */
static int32 GDisableLODFade = false;
static FAutoConsoleVariableRef CVarDisableLODFade( TEXT("r.DisableLODFade"), GDisableLODFade, TEXT("Disable fading for distance culling"), ECVF_RenderThreadSafe );

static float GFadeTime = 0.25f;
static FAutoConsoleVariableRef CVarLODFadeTime( TEXT("r.LODFadeTime"), GFadeTime, TEXT("How long LOD takes to fade (in seconds)."), ECVF_RenderThreadSafe );

static float GDistanceFadeMaxTravel = 1000.0f;
static FAutoConsoleVariableRef CVarDistanceFadeMaxTravel( TEXT("r.DistanceFadeMaxTravel"), GDistanceFadeMaxTravel, TEXT("Max distance that the player can travel during the fade time."), ECVF_RenderThreadSafe );


static TAutoConsoleVariable<int32> CVarParallelInitViews(
	TEXT("r.ParallelInitViews"),
	1,
	TEXT("Toggles parallel init views. 0 = off; 1 = on"),
	ECVF_RenderThreadSafe
	);          

float GLightMaxDrawDistanceScale = 1.0f;
static FAutoConsoleVariableRef CVarLightMaxDrawDistanceScale(
	TEXT("r.LightMaxDrawDistanceScale"),
	GLightMaxDrawDistanceScale,
	TEXT("Scale applied to the MaxDrawDistance of lights.  Useful for fading out local lights more aggressively on some platforms."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

DECLARE_CYCLE_STAT(TEXT("Occlusion Readback"), STAT_CLMM_OcclusionReadback, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("After Occlusion Readback"), STAT_CLMM_AfterOcclusionReadback, STATGROUP_CommandListMarkers);

/*------------------------------------------------------------------------------
	Visibility determination.
------------------------------------------------------------------------------*/

/**
 * Update a primitive's fading state.
 * @param FadingState - State to update.
 * @param View - The view for which to update.
 * @param bVisible - Whether the primitive should be visible in the view.
 */
static void UpdatePrimitiveFadingState(FPrimitiveFadingState& FadingState, FViewInfo& View, bool bVisible)
{
	if (FadingState.bValid)
	{
		if (FadingState.bIsVisible != bVisible)
		{
			float CurrentRealTime = View.Family->CurrentRealTime;

			// Need to kick off a fade, so make sure that we have fading state for that
			if( !IsValidRef(FadingState.UniformBuffer) )
			{
				// Primitive is not currently fading.  Start a new fade!
				FadingState.EndTime = CurrentRealTime + GFadeTime;

				if( bVisible )
				{
					// Fading in
					// (Time - StartTime) / FadeTime
					FadingState.FadeTimeScaleBias.X = 1.0f / GFadeTime;
					FadingState.FadeTimeScaleBias.Y = -CurrentRealTime / GFadeTime;
				}
				else
				{
					// Fading out
					// 1 - (Time - StartTime) / FadeTime
					FadingState.FadeTimeScaleBias.X = -1.0f / GFadeTime;
					FadingState.FadeTimeScaleBias.Y = 1.0f + CurrentRealTime / GFadeTime;
				}

				FDistanceCullFadeUniformShaderParameters Uniforms;
				Uniforms.FadeTimeScaleBias = FadingState.FadeTimeScaleBias;
				FadingState.UniformBuffer = FDistanceCullFadeUniformBufferRef::CreateUniformBufferImmediate( Uniforms, UniformBuffer_MultiFrame );
			}
			else
			{
				// Reverse fading direction but maintain current opacity
				// Solve for d: a*x+b = -a*x+d
				FadingState.FadeTimeScaleBias.Y = 2.0f * CurrentRealTime * FadingState.FadeTimeScaleBias.X + FadingState.FadeTimeScaleBias.Y;
				FadingState.FadeTimeScaleBias.X = -FadingState.FadeTimeScaleBias.X;
				
				if( bVisible )
				{
					// Fading in
					// Solve for x: a*x+b = 1
					FadingState.EndTime = ( 1.0f - FadingState.FadeTimeScaleBias.Y ) / FadingState.FadeTimeScaleBias.X;
				}
				else
				{
					// Fading out
					// Solve for x: a*x+b = 0
					FadingState.EndTime = -FadingState.FadeTimeScaleBias.Y / FadingState.FadeTimeScaleBias.X;
				}

				FDistanceCullFadeUniformShaderParameters Uniforms;
				Uniforms.FadeTimeScaleBias = FadingState.FadeTimeScaleBias;
				FadingState.UniformBuffer = FDistanceCullFadeUniformBufferRef::CreateUniformBufferImmediate( Uniforms, UniformBuffer_MultiFrame );
			}
		}
	}

	FadingState.FrameNumber = View.Family->FrameNumber;
	FadingState.bIsVisible = bVisible;
	FadingState.bValid = true;
}

bool FViewInfo::IsDistanceCulled( float DistanceSquared, float MinDrawDistance, float InMaxDrawDistance, const FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	float MaxDrawDistanceScale = GetCachedScalabilityCVars().ViewDistanceScale;
	float FadeRadius = GDisableLODFade ? 0.0f : GDistanceFadeMaxTravel;
	float MaxDrawDistance = InMaxDrawDistance * MaxDrawDistanceScale;

	// If cull distance is disabled, always show (except foliage)
	if (Family->EngineShowFlags.DistanceCulledPrimitives
		&& !PrimitiveSceneInfo->Proxy->IsDetailMesh())
	{
		return false;
	}

	// The primitive is always culled if it exceeds the max fade distance.
	if (DistanceSquared > FMath::Square(MaxDrawDistance + FadeRadius) ||
		DistanceSquared < FMath::Square(MinDrawDistance))
	{
		return true;
	}

	const bool bDistanceCulled = (DistanceSquared > FMath::Square(MaxDrawDistance));
	const bool bMayBeFading = (DistanceSquared > FMath::Square(MaxDrawDistance - FadeRadius));

	bool bStillFading = false;
	if  (!GDisableLODFade && bMayBeFading && State != NULL && !bDisableDistanceBasedFadeTransitions && PrimitiveSceneInfo->Proxy->IsUsingDistanceCullFade())
	{
		// Update distance-based visibility and fading state if it has not already been updated.
		int32 PrimitiveIndex = PrimitiveSceneInfo->GetIndex();
		FRelativeBitReference PrimitiveBit(PrimitiveIndex);
		if (PotentiallyFadingPrimitiveMap.AccessCorrespondingBit(PrimitiveBit) == false)
		{
			FPrimitiveFadingState& FadingState = ((FSceneViewState*)State)->PrimitiveFadingStates.FindOrAdd(PrimitiveSceneInfo->PrimitiveComponentId);
			UpdatePrimitiveFadingState(FadingState, *this, !bDistanceCulled);
			FUniformBufferRHIParamRef UniformBuffer = FadingState.UniformBuffer;
			bStillFading = (UniformBuffer != NULL);
			PrimitiveFadeUniformBuffers[PrimitiveIndex] = UniformBuffer;
			PrimitiveFadeUniformBufferMap[PrimitiveIndex] = UniformBuffer != nullptr;
			PotentiallyFadingPrimitiveMap.AccessCorrespondingBit(PrimitiveBit) = true;
		}
	}

	// If we're still fading then make sure the object is still drawn, even if it's beyond the max draw distance
	return ( bDistanceCulled && !bStillFading );
}

static int32 FrustumCullNumWordsPerTask = 128;
static FAutoConsoleVariableRef CVarFrustumCullNumWordsPerTask(
	TEXT("r.FrustumCullNumWordsPerTask"),
	FrustumCullNumWordsPerTask,
	TEXT("Performance tweak. Controls the granularity for the ParallelFor for frustum culling."),
	ECVF_Default
	);


template<bool UseCustomCulling, bool bAlsoUseSphereTest>
static int32 FrustumCull(const FScene* Scene, FViewInfo& View)
{
	SCOPE_CYCLE_COUNTER(STAT_FrustumCull);

	FThreadSafeCounter NumCulledPrimitives;
	float MaxDrawDistanceScale = GetCachedScalabilityCVars().ViewDistanceScale;
	MaxDrawDistanceScale *= GetCachedScalabilityCVars().CalculateFieldOfViewDistanceScale(View.DesiredFOV);

	FSceneViewState* ViewState = (FSceneViewState*)View.State;
	const bool bHLODActive = Scene->SceneLODHierarchy.IsActive();
	const FHLODVisibilityState* const HLODState = bHLODActive && ViewState ? &ViewState->HLODVisibilityState : nullptr;

	//Primitives per ParallelFor task
	//Using async FrustumCull. Thanks Yager! See https://udn.unrealengine.com/questions/252385/performance-of-frustumcull.html
	//Performance varies on total primitive count and tasks scheduled. Check the mentioned link above for some measurements.
	//There have been some changes as compared to the code measured in the link

	const int32 BitArrayNum = View.PrimitiveVisibilityMap.Num();
	const int32 BitArrayWords = FMath::DivideAndRoundUp(View.PrimitiveVisibilityMap.Num(), (int32)NumBitsPerDWORD);
	const int32 NumTasks = FMath::DivideAndRoundUp(BitArrayWords, FrustumCullNumWordsPerTask);

	ParallelFor(NumTasks, 
		[&NumCulledPrimitives, Scene, &View, MaxDrawDistanceScale, HLODState](int32 TaskIndex)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FrustumCull_Loop);
			const int32 BitArrayNumInner = View.PrimitiveVisibilityMap.Num();
			FVector ViewOriginForDistanceCulling = View.ViewMatrices.GetViewOrigin();
			float FadeRadius = GDisableLODFade ? 0.0f : GDistanceFadeMaxTravel;
			uint8 CustomVisibilityFlags = EOcclusionFlags::CanBeOccluded | EOcclusionFlags::HasPrecomputedVisibility;

			// Primitives may be explicitly removed from stereo views when using mono
			const int32 TaskWordOffset = TaskIndex * FrustumCullNumWordsPerTask;

			for (int32 WordIndex = TaskWordOffset; WordIndex < TaskWordOffset + FrustumCullNumWordsPerTask && WordIndex * NumBitsPerDWORD < BitArrayNumInner; WordIndex++)
			{
				uint32 Mask = 0x1;
				uint32 VisBits = 0;
				uint32 FadingBits = 0;
				for (int32 BitSubIndex = 0; BitSubIndex < NumBitsPerDWORD && WordIndex * NumBitsPerDWORD + BitSubIndex < BitArrayNumInner; BitSubIndex++, Mask <<= 1)
				{
					int32 Index = WordIndex * NumBitsPerDWORD + BitSubIndex;
					const FPrimitiveBounds& Bounds = Scene->PrimitiveBounds[Index];
					float DistanceSquared = (Bounds.BoxSphereBounds.Origin - ViewOriginForDistanceCulling).SizeSquared();
					int32 VisibilityId = INDEX_NONE;

					if (UseCustomCulling &&
						((Scene->PrimitiveOcclusionFlags[Index] & CustomVisibilityFlags) == CustomVisibilityFlags))
					{
						VisibilityId = Scene->PrimitiveVisibilityIds[Index].ByteIndex;
					}

					// Preserve infinite draw distance
					float MaxDrawDistance = Bounds.MaxCullDistance < FLT_MAX ? Bounds.MaxCullDistance * MaxDrawDistanceScale : FLT_MAX; 
					float MinDrawDistanceSq = Bounds.MinDrawDistanceSq;

					// If cull distance is disabled, always show the primitive (except foliage)
					if (View.Family->EngineShowFlags.DistanceCulledPrimitives
						&& !Scene->Primitives[Index]->Proxy->IsDetailMesh())
					{
						MaxDrawDistance = FLT_MAX;
					}

					// Fading HLODs and their children must be visible, objects hidden by HLODs can be culled
					if (HLODState && HLODState->IsNodeForcedVisible(Index))
					{
						MaxDrawDistance = FLT_MAX;
						MinDrawDistanceSq = 0.f;
					}
					else if (HLODState && HLODState->IsNodeForcedHidden(Index))
					{
						MaxDrawDistance = 0.f;
					}

					if (DistanceSquared > FMath::Square(MaxDrawDistance + FadeRadius) ||
						(DistanceSquared < MinDrawDistanceSq) ||
						(UseCustomCulling && !View.CustomVisibilityQuery->IsVisible(VisibilityId, FBoxSphereBounds(Bounds.BoxSphereBounds.Origin, Bounds.BoxSphereBounds.BoxExtent, Bounds.BoxSphereBounds.SphereRadius))) ||
						(bAlsoUseSphereTest && View.ViewFrustum.IntersectSphere(Bounds.BoxSphereBounds.Origin, Bounds.BoxSphereBounds.SphereRadius) == false) ||
						View.ViewFrustum.IntersectBox(Bounds.BoxSphereBounds.Origin, Bounds.BoxSphereBounds.BoxExtent) == false)
					{
						STAT(NumCulledPrimitives.Increment());
					}
					else
					{
						if (DistanceSquared > FMath::Square(MaxDrawDistance))
						{
							if (Scene->Primitives[Index]->Proxy->IsUsingDistanceCullFade())
							{
								FadingBits |= Mask;
							}
						}
						else
						{
							// The primitive is visible!
							VisBits |= Mask;
							if (DistanceSquared > FMath::Square(MaxDrawDistance - FadeRadius))
							{
								if (Scene->Primitives[Index]->Proxy->IsUsingDistanceCullFade())
								{
									FadingBits |= Mask;
								}
							}
						}
					}
				}
				if (FadingBits)
				{
					check(!View.PotentiallyFadingPrimitiveMap.GetData()[WordIndex]); // this should start at zero
					View.PotentiallyFadingPrimitiveMap.GetData()[WordIndex] = FadingBits;
				}
				if (VisBits)
				{
					check(!View.PrimitiveVisibilityMap.GetData()[WordIndex]); // this should start at zero
					View.PrimitiveVisibilityMap.GetData()[WordIndex] = VisBits;
				}
			}
		},
		!FApp::ShouldUseThreadingForPerformance() || (UseCustomCulling && !View.CustomVisibilityQuery->IsThreadsafe()) || CVarParallelInitViews.GetValueOnRenderThread() == 0 || !IsInActualRenderingThread()
	);

	return NumCulledPrimitives.GetValue();
}

/**
 * Updated primitive fading states for the view.
 */
static void UpdatePrimitiveFading(const FScene* Scene, FViewInfo& View)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdatePrimitiveFading);

	FSceneViewState* ViewState = (FSceneViewState*)View.State;

	if (ViewState)
	{
		uint32 PrevFrameNumber = ViewState->PrevFrameNumber;
		float CurrentRealTime = View.Family->CurrentRealTime;

		// First clear any stale fading states.
		for (FPrimitiveFadingStateMap::TIterator It(ViewState->PrimitiveFadingStates); It; ++It)
		{
			FPrimitiveFadingState& FadingState = It.Value();
			if (FadingState.FrameNumber != PrevFrameNumber ||
				(IsValidRef(FadingState.UniformBuffer) && CurrentRealTime >= FadingState.EndTime))
			{
				It.RemoveCurrent();
			}
		}

		// Should we allow fading transitions at all this frame?  For frames where the camera moved
		// a large distance or where we haven't rendered a view in awhile, it's best to disable
		// fading so users don't see unexpected object transitions.
		if (!GDisableLODFade && !View.bDisableDistanceBasedFadeTransitions)
		{
			// Do a pass over potentially fading primitives and update their states.
			for (FSceneSetBitIterator BitIt(View.PotentiallyFadingPrimitiveMap); BitIt; ++BitIt)
			{
				bool bVisible = View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt);
				FPrimitiveFadingState& FadingState = ViewState->PrimitiveFadingStates.FindOrAdd(Scene->PrimitiveComponentIds[BitIt.GetIndex()]);
				UpdatePrimitiveFadingState(FadingState, View, bVisible);
				FUniformBufferRHIParamRef UniformBuffer = FadingState.UniformBuffer;
				if (UniformBuffer && !bVisible)
				{
					// If the primitive is fading out make sure it remains visible.
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = true;
				}
				View.PrimitiveFadeUniformBuffers[BitIt.GetIndex()] = UniformBuffer;
				View.PrimitiveFadeUniformBufferMap[BitIt.GetIndex()] = UniformBuffer != nullptr;
			}
		}
	}
}

struct FOcclusionBounds
{
	FOcclusionBounds(FPrimitiveOcclusionHistory* InPrimitiveOcclusionHistory, const FVector& InBoundsOrigin, const FVector& InBoundsExtent, bool bInGroupedQuery)
		: PrimitiveOcclusionHistory(InPrimitiveOcclusionHistory)
		, BoundsOrigin(InBoundsOrigin)
		, BoundsExtent(InBoundsExtent)
		, bGroupedQuery(bInGroupedQuery)
	{
	}
	FOcclusionBounds(FPrimitiveOcclusionHistoryKey InPrimitiveOcclusionHistoryKey, const FVector& InBoundsOrigin, const FVector& InBoundsExtent, uint32 InLastQuerySubmitFrame)
		: PrimitiveOcclusionHistoryKey(InPrimitiveOcclusionHistoryKey)
		, BoundsOrigin(InBoundsOrigin)
		, BoundsExtent(InBoundsExtent)
		, LastQuerySubmitFrame(InLastQuerySubmitFrame)
	{
	}
	union 
	{
		FPrimitiveOcclusionHistory* PrimitiveOcclusionHistory;
		FPrimitiveOcclusionHistoryKey PrimitiveOcclusionHistoryKey;
	};

	FVector BoundsOrigin;
	FVector BoundsExtent;	
	union 
	{
		bool bGroupedQuery;
		uint32 LastQuerySubmitFrame;
	};
};

struct FHZBBound
{
	FHZBBound(FPrimitiveOcclusionHistory* InTargetHistory, const FVector& InBoundsOrigin, const FVector& InBoundsExtent)
	: TargetHistory(InTargetHistory)
	, BoundsOrigin(InBoundsOrigin)
	, BoundsExtent(InBoundsExtent)
	{}

	FPrimitiveOcclusionHistory* TargetHistory;
	FVector BoundsOrigin;
	FVector BoundsExtent;
};

#define BALANCE_LOAD 1
#define QUERY_SANITY_CHECK 0

struct FVisForPrimParams
{
	FVisForPrimParams(){}

	FVisForPrimParams(const FScene* InScene, 
						FViewInfo* InView, 
						FViewElementPDI* InOcclusionPDI, 						
						const int32 InStartIndex, 
						const int32 InNumToProcess, 
						const bool bInSubmitQueries, 
						const bool bInHZBOcclusion,						
						TArray<FPrimitiveOcclusionHistory>* OutOcclusionHistory,
						TArray<FPrimitiveOcclusionHistory*>* OutQueriesToRelease,
						TArray<FHZBBound>* OutHZBBounds,
						TArray<FOcclusionBounds>* OutQueriesToRun,
						TArray<bool>* OutSubIsOccluded)
		: Scene(InScene)
		, View(InView)
		, OcclusionPDI(InOcclusionPDI)		
		, StartIndex(InStartIndex)
		, NumToProcess(InNumToProcess)
		, bSubmitQueries(bInSubmitQueries)
		, bHZBOcclusion(bInHZBOcclusion)		
		, bNeedsScanOnRead(false)
		, InsertPrimitiveOcclusionHistory(OutOcclusionHistory)
		, QueriesToRelease(OutQueriesToRelease)
		, HZBBoundsToAdd(OutHZBBounds)
		, QueriesToAdd(OutQueriesToRun)	
		, SubIsOccluded(OutSubIsOccluded)
	{

	}

	void Init(	const FScene* InScene,
				FViewInfo* InView,
				FViewElementPDI* InOcclusionPDI,
				const int32 InStartIndex,
				const int32 InNumToProcess,
				const bool bInSubmitQueries,
				const bool bInHZBOcclusion,				
				TArray<FPrimitiveOcclusionHistory>* OutOcclusionHistory,
				TArray<FPrimitiveOcclusionHistory*>* OutQueriesToRelease,
				TArray<FHZBBound>* OutHZBBounds,
				TArray<FOcclusionBounds>* OutQueriesToRun,
				TArray<bool>* OutSubIsOccluded)
			
	{
		Scene = InScene;
		View = InView;
		OcclusionPDI = InOcclusionPDI;
		StartIndex = InStartIndex;
		NumToProcess = InNumToProcess;
		bSubmitQueries = bInSubmitQueries;
		bHZBOcclusion = bInHZBOcclusion;
		InsertPrimitiveOcclusionHistory = OutOcclusionHistory;
		QueriesToRelease = OutQueriesToRelease;
		HZBBoundsToAdd = OutHZBBounds;
		QueriesToAdd = OutQueriesToRun;
		SubIsOccluded = OutSubIsOccluded;
	}

	const FScene* Scene;
	FViewInfo* View;
	FViewElementPDI* OcclusionPDI;
	int32 StartIndex;
	int32 NumToProcess;
	bool bSubmitQueries;
	bool bHZBOcclusion;	

	// Whether the entries written into the history need to be read using a scan search (see FPrimitiveOcclusionHistory::bNeedsScanOnRead)
	bool bNeedsScanOnRead;

	//occlusion history to insert into.  In parallel these will be all merged back into the view's history on the main thread.
	//use TChunkedArray so pointers to the new FPrimitiveOcclusionHistory's won't change if the array grows.	
	TArray<FPrimitiveOcclusionHistory>*		InsertPrimitiveOcclusionHistory;
	TArray<FPrimitiveOcclusionHistory*>*	QueriesToRelease;
	TArray<FHZBBound>*						HZBBoundsToAdd;
	TArray<FOcclusionBounds>*				QueriesToAdd;
	int32									NumOccludedPrims;
	TArray<bool>*							SubIsOccluded;
};

//This function is shared between the single and multi-threaded versions.  Modifications to any primitives indexed by BitIt should be ok
//since only one of the task threads will ever reference it.  However, any modifications to shared state like the ViewState must be buffered
//to be recombined later.
template<bool bSingleThreaded>
static void FetchVisibilityForPrimitives_Range(FVisForPrimParams& Params, FGlobalDynamicVertexBuffer* DynamicVertexBufferIfSingleThreaded)
{	
	int32 NumOccludedPrimitives = 0;
	
	const FScene* Scene				= Params.Scene;
	FViewInfo& View					= *Params.View;
	FViewElementPDI* OcclusionPDI	= Params.OcclusionPDI;
	const int32 StartIndex			= Params.StartIndex;
	const int32 NumToProcess		= Params.NumToProcess;
	const bool bSubmitQueries		= Params.bSubmitQueries;
	const bool bHZBOcclusion		= Params.bHZBOcclusion;

	const float PrimitiveProbablyVisibleTime = GEngine->PrimitiveProbablyVisibleTime;

	FSceneViewState* ViewState = (FSceneViewState*)View.State;
	const int32 NumBufferedFrames = FOcclusionQueryHelpers::GetNumBufferedFrames(Scene->GetFeatureLevel());
	bool bClearQueries = !View.Family->EngineShowFlags.HitProxies;
	const float CurrentRealTime = View.Family->CurrentRealTime;
	uint32 OcclusionFrameCounter = ViewState->OcclusionFrameCounter;
	FRenderQueryPool& OcclusionQueryPool = ViewState->OcclusionQueryPool;
	FHZBOcclusionTester& HZBOcclusionTests = ViewState->HZBOcclusionTests;

	int32 ReadBackLagTolerance = NumBufferedFrames;

	const bool bIsStereoView = View.StereoPass == eSSP_LEFT_EYE || View.StereoPass == eSSP_RIGHT_EYE;
	const bool bUseRoundRobinOcclusion = bIsStereoView && !View.bIsSceneCapture && View.ViewState->IsRoundRobinEnabled();
	if (bUseRoundRobinOcclusion)
	{
		// We don't allow clearing of a history entry if we do not also submit an occlusion query to replace the deleted one
		// as we want to keep the history as full as possible
		bClearQueries &= bSubmitQueries;

		// However, if this frame happens to be the first frame, then we clear anyway since in the first frame we should not be
		// reading past queries
		bClearQueries |= View.bIgnoreExistingQueries;

		// Round-robin occlusion culling involves reading frames that could be twice as stale as without round-robin
		ReadBackLagTolerance = NumBufferedFrames * 2;
	}
	// Round robin occlusion culling can make holes in the occlusion history which would require scanning the history when reading
	Params.bNeedsScanOnRead = bUseRoundRobinOcclusion;

	TSet<FPrimitiveOcclusionHistory, FPrimitiveOcclusionHistoryKeyFuncs>& ViewPrimitiveOcclusionHistory = ViewState->PrimitiveOcclusionHistorySet;
	TArray<FPrimitiveOcclusionHistory>* InsertPrimitiveOcclusionHistory = Params.InsertPrimitiveOcclusionHistory;
	TArray<FPrimitiveOcclusionHistory*>* QueriesToRelease = Params.QueriesToRelease;
	TArray<FHZBBound>* HZBBoundsToAdd = Params.HZBBoundsToAdd;
	TArray<FOcclusionBounds>* QueriesToAdd = Params.QueriesToAdd;	

	const bool bNewlyConsideredBBoxExpandActive = GExpandNewlyOcclusionTestedBBoxesAmount > 0.0f && GFramesToExpandNewlyOcclusionTestedBBoxes > 0 && GFramesNotOcclusionTestedToExpandBBoxes > 0;
	const float NeverOcclusionTestDistanceSquared = GNeverOcclusionTestDistance * GNeverOcclusionTestDistance;
	const FVector ViewOrigin = View.ViewMatrices.GetViewOrigin();

	const int32 ReserveAmount = NumToProcess;
	if (!bSingleThreaded)
	{		
		check(InsertPrimitiveOcclusionHistory);
		check(QueriesToRelease);
		check(HZBBoundsToAdd);
		check(QueriesToAdd);

		//avoid doing reallocs as much as possible.  Unlikely to make an entry per processed element.		
		InsertPrimitiveOcclusionHistory->Reserve(ReserveAmount);
		QueriesToRelease->Reserve(ReserveAmount);
		HZBBoundsToAdd->Reserve(ReserveAmount);
		QueriesToAdd->Reserve(ReserveAmount);
	}
	
	int32 NumProcessed = 0;
	int32 NumTotalPrims = View.PrimitiveVisibilityMap.Num();
	int32 NumTotalDefUnoccluded = View.PrimitiveDefinitelyUnoccludedMap.Num();

	//if we are load balanced then we iterate only the set bits, and the ranges have been pre-selected to evenly distribute set bits among the tasks with no overlaps.
	//if not, then the entire array is evenly divided by range.
#if BALANCE_LOAD
	for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap, StartIndex); BitIt && (NumProcessed < NumToProcess); ++BitIt, ++NumProcessed)
#else
	for (TBitArray<SceneRenderingBitArrayAllocator>::FIterator BitIt(View.PrimitiveVisibilityMap, StartIndex); BitIt && (NumProcessed < NumToProcess); ++BitIt, ++NumProcessed)
#endif
	{		
		uint8 OcclusionFlags = Scene->PrimitiveOcclusionFlags[BitIt.GetIndex()];
		bool bCanBeOccluded = (OcclusionFlags & EOcclusionFlags::CanBeOccluded) != 0;

#if !BALANCE_LOAD		
		if (!View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt))
		{
			continue;
		}
#endif

		//we can't allow the prim history insertion array to realloc or it will invalidate pointers in the other output arrays.
		const bool bCanAllocPrimHistory = bSingleThreaded || InsertPrimitiveOcclusionHistory->Num() < InsertPrimitiveOcclusionHistory->Max();		

		if (GIsEditor)
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene->Primitives[BitIt.GetIndex()];

			if (PrimitiveSceneInfo->Proxy->IsSelected())
			{
				// to render occluded outline for selected objects
				bCanBeOccluded = false;
			}
		}
		int32 NumSubQueries = 1;
		bool bSubQueries = false;
		const TArray<FBoxSphereBounds>* SubBounds = nullptr;

		check(Params.SubIsOccluded);
		TArray<bool>& SubIsOccluded = *Params.SubIsOccluded;
		int32 SubIsOccludedStart = SubIsOccluded.Num();
		if ((OcclusionFlags & EOcclusionFlags::HasSubprimitiveQueries) && GAllowSubPrimitiveQueries && !View.bDisableQuerySubmissions)
		{
			FPrimitiveSceneProxy* Proxy = Scene->Primitives[BitIt.GetIndex()]->Proxy;
			SubBounds = Proxy->GetOcclusionQueries(&View);
			NumSubQueries = SubBounds->Num();
			bSubQueries = true;
			if (!NumSubQueries)
			{
				View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
				continue;
			}
			SubIsOccluded.Reserve(NumSubQueries);
		}

		bool bAllSubOcclusionStateIsDefinite = true;
		bool bAllSubOccluded = true;
		FPrimitiveComponentId PrimitiveId = Scene->PrimitiveComponentIds[BitIt.GetIndex()];

		for (int32 SubQuery = 0; SubQuery < NumSubQueries; SubQuery++)
		{
			FPrimitiveOcclusionHistory* PrimitiveOcclusionHistory = ViewPrimitiveOcclusionHistory.Find(FPrimitiveOcclusionHistoryKey(PrimitiveId, SubQuery));
			
			bool bIsOccluded = false;
			bool bOcclusionStateIsDefinite = false;			

			if (!PrimitiveOcclusionHistory)
			{
				// If the primitive doesn't have an occlusion history yet, create it.
				if (bSingleThreaded)
				{					
					// In singlethreaded mode we can safely modify the view's history directly.
					PrimitiveOcclusionHistory = &ViewPrimitiveOcclusionHistory[
						ViewPrimitiveOcclusionHistory.Add(FPrimitiveOcclusionHistory(PrimitiveId, SubQuery))
					];
				}
				else if (bCanAllocPrimHistory)
				{
					// In multithreaded mode we have to buffer the new histories and add them to the view during a post-combine
					PrimitiveOcclusionHistory = &(*InsertPrimitiveOcclusionHistory)[
						InsertPrimitiveOcclusionHistory->Add(FPrimitiveOcclusionHistory(PrimitiveId, SubQuery))
					];
				}				
				
				// If the primitive hasn't been visible recently enough to have a history, treat it as unoccluded this frame so it will be rendered as an occluder and its true occlusion state can be determined.
				// already set bIsOccluded = false;

				// Flag the primitive's occlusion state as indefinite, which will force it to be queried this frame.
				// The exception is if the primitive isn't occludable, in which case we know that it's definitely unoccluded.
				bOcclusionStateIsDefinite = bCanBeOccluded ? false : true;
			}
			else
			{
				if (View.bIgnoreExistingQueries)
				{
					// If the view is ignoring occlusion queries, the primitive is definitely unoccluded.
					// already set bIsOccluded = false;
					bOcclusionStateIsDefinite = View.bDisableQuerySubmissions;
				}
				else if (bCanBeOccluded)
				{
					if (bHZBOcclusion)
					{
						if (HZBOcclusionTests.IsValidFrame(PrimitiveOcclusionHistory->LastTestFrameNumber))
						{
							bIsOccluded = !HZBOcclusionTests.IsVisible(PrimitiveOcclusionHistory->HZBTestIndex);
							bOcclusionStateIsDefinite = true;
						}
					}
					else
					{
						// Read the occlusion query results.
						uint64 NumSamples = 0;
						bool bGrouped = false;
						FRenderQueryRHIParamRef PastQuery = PrimitiveOcclusionHistory->GetQueryForReading(OcclusionFrameCounter, NumBufferedFrames, ReadBackLagTolerance, bGrouped);
						if (PastQuery)
						{
							//int32 RefCount = PastQuery.GetReference()->GetRefCount();
							// NOTE: RHIGetOcclusionQueryResult should never fail when using a blocking call, rendering artifacts may show up.
							//if (RHICmdList.GetRenderQueryResult(PastQuery, NumSamples, true))
							if (GDynamicRHI->RHIGetRenderQueryResult(PastQuery, NumSamples, true))
							{
								// we render occlusion without MSAA
								uint32 NumPixels = (uint32)NumSamples;

								// The primitive is occluded if none of its bounding box's pixels were visible in the previous frame's occlusion query.
								bIsOccluded = (NumPixels == 0);

								
								if (!bIsOccluded)
								{
									checkSlow(View.OneOverNumPossiblePixels > 0.0f);
									PrimitiveOcclusionHistory->LastPixelsPercentage = NumPixels * View.OneOverNumPossiblePixels;
								}
								else
								{
									PrimitiveOcclusionHistory->LastPixelsPercentage = 0.0f;
								}								


								// Flag the primitive's occlusion state as definite if it wasn't grouped.
								bOcclusionStateIsDefinite = !bGrouped;
							}
							else
							{
								// If the occlusion query failed, treat the primitive as visible.  
								// already set bIsOccluded = false;
							}
						}
						else
						{
							if (NumBufferedFrames > 1 || GRHIMaximumReccommendedOustandingOcclusionQueries < MAX_int32)
							{
								// If there's no occlusion query for the primitive, assume it is whatever it was last frame
								bIsOccluded = PrimitiveOcclusionHistory->WasOccludedLastFrame;
								bOcclusionStateIsDefinite = PrimitiveOcclusionHistory->OcclusionStateWasDefiniteLastFrame;
							}
							else
							{
								// If there's no occlusion query for the primitive, set it's visibility state to whether it has been unoccluded recently.
								bIsOccluded = (PrimitiveOcclusionHistory->LastProvenVisibleTime + GEngine->PrimitiveProbablyVisibleTime < CurrentRealTime);
								// the state was definite last frame, otherwise we would have ran a query
								bOcclusionStateIsDefinite = true;
							}
							if (bIsOccluded)
							{
								PrimitiveOcclusionHistory->LastPixelsPercentage = 0.0f;
							}
							else
							{
								PrimitiveOcclusionHistory->LastPixelsPercentage = GEngine->MaxOcclusionPixelsFraction;
							}
						}
					}

					if (GVisualizeOccludedPrimitives && OcclusionPDI && bIsOccluded)
					{
						const FBoxSphereBounds& Bounds = bSubQueries ? (*SubBounds)[SubQuery] : Scene->PrimitiveOcclusionBounds[BitIt.GetIndex()];
						DrawWireBox(OcclusionPDI, Bounds.GetBox(), FColor(50, 255, 50), SDPG_Foreground);
					}
				}
				else
				{					
					// Primitives that aren't occludable are considered definitely unoccluded.
					// already set bIsOccluded = false;
					bOcclusionStateIsDefinite = true;
				}

				if (bClearQueries)
				{					
					if (bSingleThreaded)
					{						
						PrimitiveOcclusionHistory->ReleaseQuery(OcclusionQueryPool, OcclusionFrameCounter, NumBufferedFrames);
					}
					else
					{
						if (PrimitiveOcclusionHistory->GetQueryForEviction(OcclusionFrameCounter, NumBufferedFrames))
						{
							QueriesToRelease->Add(PrimitiveOcclusionHistory);							
						}
					}
				}
			}

			if (PrimitiveOcclusionHistory)
			{
				if (bSubmitQueries && bCanBeOccluded)
				{
					bool bSkipNewlyConsidered = false;

					if (bNewlyConsideredBBoxExpandActive)
					{
						if (!PrimitiveOcclusionHistory->BecameEligibleForQueryCooldown && OcclusionFrameCounter - PrimitiveOcclusionHistory->LastConsideredFrameNumber > uint32(GFramesNotOcclusionTestedToExpandBBoxes))
						{
							PrimitiveOcclusionHistory->BecameEligibleForQueryCooldown = GFramesToExpandNewlyOcclusionTestedBBoxes;
						}

						bSkipNewlyConsidered = !!PrimitiveOcclusionHistory->BecameEligibleForQueryCooldown;

						if (bSkipNewlyConsidered)
						{
							PrimitiveOcclusionHistory->BecameEligibleForQueryCooldown--;
						}
					}


					bool bAllowBoundsTest;
					const FBoxSphereBounds OcclusionBounds = (bSubQueries ? (*SubBounds)[SubQuery] : Scene->PrimitiveOcclusionBounds[BitIt.GetIndex()]).ExpandBy(GExpandAllTestedBBoxesAmount + (bSkipNewlyConsidered ? GExpandNewlyOcclusionTestedBBoxesAmount : 0.0));
					if (FVector::DistSquared(ViewOrigin, OcclusionBounds.Origin) < NeverOcclusionTestDistanceSquared)
					{
						bAllowBoundsTest = false;
					}
					else if (View.bHasNearClippingPlane)
					{
						bAllowBoundsTest = View.NearClippingPlane.PlaneDot(OcclusionBounds.Origin) <
							-(FVector::BoxPushOut(View.NearClippingPlane, OcclusionBounds.BoxExtent));

					}
					else if (!View.IsPerspectiveProjection())
					{
						// Transform parallel near plane
						static_assert((int32)ERHIZBuffer::IsInverted != 0, "Check equation for culling!");
						bAllowBoundsTest = View.WorldToScreen(OcclusionBounds.Origin).Z - View.ViewMatrices.GetProjectionMatrix().M[2][2] * OcclusionBounds.SphereRadius < 1;
					}
					else
					{
						bAllowBoundsTest = OcclusionBounds.SphereRadius < HALF_WORLD_MAX;
					}

					if (bAllowBoundsTest)
					{
						PrimitiveOcclusionHistory->LastTestFrameNumber = OcclusionFrameCounter;
						if (bHZBOcclusion)
						{
							// Always run
							if (bSingleThreaded)
							{								
								PrimitiveOcclusionHistory->HZBTestIndex = HZBOcclusionTests.AddBounds(OcclusionBounds.Origin, OcclusionBounds.BoxExtent);
							}
							else
							{
								HZBBoundsToAdd->Emplace(PrimitiveOcclusionHistory, OcclusionBounds.Origin, OcclusionBounds.BoxExtent);
							}
						}
						else
						{
							// decide if a query should be run this frame
							bool bRunQuery, bGroupedQuery;

							if (!bSubQueries && // sub queries are never grouped, we assume the custom code knows what it is doing and will group internally if it wants
								(OcclusionFlags & EOcclusionFlags::AllowApproximateOcclusion))
							{
								if (bIsOccluded)
								{
									// Primitives that were occluded the previous frame use grouped queries.
									bGroupedQuery = true;
									bRunQuery = true;
								}
								else if (bOcclusionStateIsDefinite)
								{
									bGroupedQuery = false;
									float Rnd = GOcclusionRandomStream.GetFraction();
									if (GRHISupportsExactOcclusionQueries)
									{
										float FractionMultiplier = FMath::Max(PrimitiveOcclusionHistory->LastPixelsPercentage / GEngine->MaxOcclusionPixelsFraction, 1.0f);
										bRunQuery = (FractionMultiplier * Rnd) < GEngine->MaxOcclusionPixelsFraction;
									}
									else
									{
										bRunQuery = CurrentRealTime - PrimitiveOcclusionHistory->LastProvenVisibleTime > PrimitiveProbablyVisibleTime * (0.5f * 0.25f * Rnd);
									}									
								}
								else
								{
									bGroupedQuery = false;
									bRunQuery = true;
								}
							}
							else
							{
								// Primitives that need precise occlusion results use individual queries.
								bGroupedQuery = false;
								bRunQuery = true;
							}

							if (bRunQuery)
							{
								const FVector BoundOrigin = OcclusionBounds.Origin + View.ViewMatrices.GetPreViewTranslation();
								const FVector BoundExtent = OcclusionBounds.BoxExtent;

								if (bSingleThreaded)
								{
									checkSlow(DynamicVertexBufferIfSingleThreaded);

									if (GRHIMaximumReccommendedOustandingOcclusionQueries < MAX_int32 && !bGroupedQuery)
									{
										QueriesToAdd->Emplace(FPrimitiveOcclusionHistoryKey(PrimitiveId, SubQuery), BoundOrigin, BoundExtent, PrimitiveOcclusionHistory->LastQuerySubmitFrame());
									}
									else
									{
										PrimitiveOcclusionHistory->SetCurrentQuery(OcclusionFrameCounter,
											bGroupedQuery ?
											View.GroupedOcclusionQueries.BatchPrimitive(BoundOrigin, BoundExtent, *DynamicVertexBufferIfSingleThreaded) :
											View.IndividualOcclusionQueries.BatchPrimitive(BoundOrigin, BoundExtent, *DynamicVertexBufferIfSingleThreaded),
											NumBufferedFrames,
											bGroupedQuery,
											Params.bNeedsScanOnRead
										);
									}
								}
								else
								{
									check(GRHIMaximumReccommendedOustandingOcclusionQueries < MAX_int32); // it would be fairly easy to set up this path to optimize when there are a limited number, but it hasn't been done yet
									QueriesToAdd->Emplace(PrimitiveOcclusionHistory, BoundOrigin, BoundExtent, bGroupedQuery);
								}
							}
						}
					}
					else
					{
						// If the primitive's bounding box intersects the near clipping plane, treat it as definitely unoccluded.
						bIsOccluded = false;
						bOcclusionStateIsDefinite = true;
					}
				}
				// Set the primitive's considered time to keep its occlusion history from being trimmed.
				PrimitiveOcclusionHistory->LastConsideredTime = CurrentRealTime;
				if (!bIsOccluded && bOcclusionStateIsDefinite)
				{
					PrimitiveOcclusionHistory->LastProvenVisibleTime = CurrentRealTime;
				}
				PrimitiveOcclusionHistory->LastConsideredFrameNumber = OcclusionFrameCounter;
				PrimitiveOcclusionHistory->WasOccludedLastFrame = bIsOccluded;
				PrimitiveOcclusionHistory->OcclusionStateWasDefiniteLastFrame = bOcclusionStateIsDefinite;
			}

			if (bSubQueries)
			{
				SubIsOccluded.Add(bIsOccluded);
				if (!bIsOccluded)
				{
					bAllSubOccluded = false;
				}
				if (bIsOccluded || !bOcclusionStateIsDefinite)
				{
					bAllSubOcclusionStateIsDefinite = false;
				}
			}
			else
			{
					
				if (bIsOccluded)
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
					STAT(NumOccludedPrimitives++);
				}
				else if (bOcclusionStateIsDefinite)
				{
					View.PrimitiveDefinitelyUnoccludedMap.AccessCorrespondingBit(BitIt) = true;
				}					
			}			
		}

		if (bSubQueries)
		{
			if (SubIsOccluded.Num() > 0)
			{
				FPrimitiveSceneProxy* Proxy = Scene->Primitives[BitIt.GetIndex()]->Proxy;
				Proxy->AcceptOcclusionResults(&View, &SubIsOccluded, SubIsOccludedStart, SubIsOccluded.Num() - SubIsOccludedStart);
			}

			if (bAllSubOccluded)
			{
				View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
				STAT(NumOccludedPrimitives++);
			}
			else if (bAllSubOcclusionStateIsDefinite)
			{
				View.PrimitiveDefinitelyUnoccludedMap.AccessCorrespondingBit(BitIt) = true;
			}
		}
	}

	check(NumTotalDefUnoccluded == View.PrimitiveDefinitelyUnoccludedMap.Num());
	check(NumTotalPrims == View.PrimitiveVisibilityMap.Num());
	check(!InsertPrimitiveOcclusionHistory || InsertPrimitiveOcclusionHistory->Num() <= ReserveAmount);
	Params.NumOccludedPrims = NumOccludedPrimitives;	
}

FAutoConsoleTaskPriority CPrio_FetchVisibilityForPrimitivesTask(
	TEXT("TaskGraph.TaskPriorities.FetchVisibilityForPrimitivesTask"),
	TEXT("Task and thread priority for FetchVisibilityForPrimitivesTask."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);

class FetchVisibilityForPrimitivesTask
{
	FVisForPrimParams& Params;

public:

	FetchVisibilityForPrimitivesTask(FVisForPrimParams& InParams)
		: Params(InParams)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FetchVisibilityForPrimitivesTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_FetchVisibilityForPrimitivesTask.Get();
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FetchVisibilityForPrimitives_Range<false>(Params, nullptr);
	}
};

static int32 FetchVisibilityForPrimitives(const FScene* Scene, FViewInfo& View, const bool bSubmitQueries, const bool bHZBOcclusion, FGlobalDynamicVertexBuffer& DynamicVertexBuffer)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FetchVisibilityForPrimitives);
	FSceneViewState* ViewState = (FSceneViewState*)View.State;
	
	static int32 SubIsOccludedArrayIndex = 0;
	SubIsOccludedArrayIndex = 1 - SubIsOccludedArrayIndex;

	const int32 NumBufferedFrames = FOcclusionQueryHelpers::GetNumBufferedFrames(Scene->GetFeatureLevel());
	uint32 OcclusionFrameCounter = ViewState->OcclusionFrameCounter;
	TSet<FPrimitiveOcclusionHistory, FPrimitiveOcclusionHistoryKeyFuncs>& ViewPrimitiveOcclusionHistory = ViewState->PrimitiveOcclusionHistorySet;

	if (GOcclusionCullParallelPrimFetch && GSupportsParallelOcclusionQueries)
	{		
		static const int32 MaxNumCullTasks = 4;
		static const int32 ActualNumCullTasks = 4;
		static const int32 NumOutputArrays = MaxNumCullTasks;
		
		FGraphEventRef TaskRefArray[NumOutputArrays];

		//params for each task
		FVisForPrimParams Params[NumOutputArrays];

		//output arrays for each task
		TArray<FPrimitiveOcclusionHistory> OutputOcclusionHistory[NumOutputArrays];
		TArray<FPrimitiveOcclusionHistory*> OutQueriesToRelease[NumOutputArrays];
		TArray<FHZBBound> OutHZBBounds[NumOutputArrays];
		TArray<FOcclusionBounds> OutQueriesToRun[NumOutputArrays];	

		static TArray<bool> FrameSubIsOccluded[NumOutputArrays][FSceneView::NumBufferedSubIsOccludedArrays];

		//optionally balance the tasks by how the visible primitives are distributed in the array rather than just breaking up the array by range.
		//should make the tasks more equal length.
#if BALANCE_LOAD
		int32 StartIndices[NumOutputArrays] = { 0 };
		int32 ProcessRange[NumOutputArrays] = { 0 };
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FetchVisibilityForPrimitivesPreProcess);
			int32 NumBitsSet = 0;
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt, ++NumBitsSet)
			{
			}
			
			int32 BitsPerTask = NumBitsSet / ActualNumCullTasks;
			int32 NumBitsForRange = 0;
			int32 CurrentStartIndex = 0;
			int32 RangeToSet = 0;

			//accumulate set bits for each task until we reach the target, then set the start/end and move on.
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt && RangeToSet < (ActualNumCullTasks - 1); ++BitIt)
			{
				++NumBitsForRange;
				if (NumBitsForRange == BitsPerTask)
				{
					StartIndices[RangeToSet] = CurrentStartIndex;
					ProcessRange[RangeToSet] = NumBitsForRange;

					++RangeToSet;
					NumBitsForRange = 0;
					CurrentStartIndex = BitIt.GetIndex() + 1;
				}
			}

			//final range is the rest of the set bits, no matter how many there are.
			StartIndices[ActualNumCullTasks - 1] = CurrentStartIndex;
			ProcessRange[ActualNumCullTasks - 1] = NumBitsSet - (BitsPerTask * 3);
		}
#endif

		const int32 NumPrims = View.PrimitiveVisibilityMap.Num();
		const int32 NumPerTask = NumPrims / ActualNumCullTasks;
		int32 StartIndex = 0;
		int32 NumToProcess = NumPerTask;

		FGraphEventArray TaskWaitArray;
		int32 NumTasks = 0;
		for (int32 i = 0; i < ActualNumCullTasks && (StartIndex < NumPrims); ++i, ++NumTasks)
		{
			NumToProcess = (i == (ActualNumCullTasks - 1)) ? (NumPrims - StartIndex) : NumPerTask;
			TArray<bool>& SubIsOccluded = FrameSubIsOccluded[i][SubIsOccludedArrayIndex];
			SubIsOccluded.Reset();

			Params[i].Init(
				Scene,
				&View,
				nullptr,
#if BALANCE_LOAD
				StartIndices[i],
				ProcessRange[i],
#else
				StartIndex,
				NumToProcess,
#endif
				bSubmitQueries,
				bHZBOcclusion,				
				&OutputOcclusionHistory[i],
				&OutQueriesToRelease[i],
				&OutHZBBounds[i],
				&OutQueriesToRun[i],
				&SubIsOccluded
				);

			TaskRefArray[i] = TGraphTask<FetchVisibilityForPrimitivesTask>::CreateTask().ConstructAndDispatchWhenReady(Params[i]);			
			TaskWaitArray.Add(TaskRefArray[i]);

			StartIndex += NumToProcess;
		}

		FRenderQueryPool& OcclusionQueryPool = ViewState->OcclusionQueryPool;
		FHZBOcclusionTester& HZBOcclusionTests = ViewState->HZBOcclusionTests;		

		int32 NumOccludedPrims = 0;
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FetchVisibilityForPrimitivesCombine);

			//wait for them all so we don't start modifying the prim histories while the gather is running
			FTaskGraphInterface::Get().WaitUntilTasksComplete(TaskWaitArray, ENamedThreads::GetRenderThread_Local());

#if QUERY_SANITY_CHECK
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FetchVisibilityForPrimitivesSanity);
				TSet<int32> ReleaseQuerySet;
				TSet<int32> RunQuerySet;
				TSet<int32> MasterPrimsProcessed;
				for (int32 i = 0; i < NumTasks; ++i)
				{
					bool bAlreadyIn = false;
					for (auto ReleaseQueryIter = OutQueriesToRelease[i].CreateIterator(); ReleaseQueryIter; ++ReleaseQueryIter)
					{
						FPrimitiveOcclusionHistory* History = *ReleaseQueryIter;
						ReleaseQuerySet.Add(History->PrimitiveId.PrimIDValue, &bAlreadyIn);
						checkf(!bAlreadyIn, TEXT("Prim: %i double released query."), History->PrimitiveId.PrimIDValue);
					}

					for (auto RunQueriesIter = OutQueriesToRun[i].CreateIterator(); RunQueriesIter; ++RunQueriesIter)
					{
						FPrimitiveOcclusionHistory* History = RunQueriesIter->PrimitiveOcclusionHistory;
						RunQuerySet.Add(History->PrimitiveId.PrimIDValue, &bAlreadyIn);
						checkf(!bAlreadyIn, TEXT("Prim: %i double run query."), History->PrimitiveId.PrimIDValue);
					}					
				}
			}
#endif

			//Add/Release query ops use stored PrimitiveHistory pointers. We must do ALL of these from all tasks before adding any new PrimitiveHistories to the view.
			//Adding new histories to the view could cause the array to resize which would invalidate all the stored output pointers for the other operations.
			for (int32 i = 0; i < NumTasks; ++i)
			{
				//HZB output
				for (auto HZBBoundIter = OutHZBBounds[i].CreateIterator(); HZBBoundIter; ++HZBBoundIter)
				{
					HZBBoundIter->TargetHistory->HZBTestIndex = HZBOcclusionTests.AddBounds(HZBBoundIter->BoundsOrigin, HZBBoundIter->BoundsExtent);
				}

				//Manual query release handling
				for (auto ReleaseQueryIter = OutQueriesToRelease[i].CreateIterator(); ReleaseQueryIter; ++ReleaseQueryIter)
				{
					FPrimitiveOcclusionHistory* History = *ReleaseQueryIter;
					History->ReleaseQuery(OcclusionQueryPool, OcclusionFrameCounter, NumBufferedFrames);
				}
				
				//New query batching
				for (auto RunQueriesIter = OutQueriesToRun[i].CreateIterator(); RunQueriesIter; ++RunQueriesIter)
				{
					RunQueriesIter->PrimitiveOcclusionHistory->SetCurrentQuery(OcclusionFrameCounter,
						RunQueriesIter->bGroupedQuery ?
						View.GroupedOcclusionQueries.BatchPrimitive(RunQueriesIter->BoundsOrigin, RunQueriesIter->BoundsExtent, DynamicVertexBuffer) :
						View.IndividualOcclusionQueries.BatchPrimitive(RunQueriesIter->BoundsOrigin, RunQueriesIter->BoundsExtent, DynamicVertexBuffer),
						NumBufferedFrames,
						RunQueriesIter->bGroupedQuery,
						Params[i].bNeedsScanOnRead
						);
				}
			}

			//now add new primitive histories to the view. may resize the view's array.
			for (int32 i = 0; i < NumTasks; ++i)
			{								
				const TArray<FPrimitiveOcclusionHistory>& NewHistoryArray = OutputOcclusionHistory[i];				
				for (int32 HistoryIndex = 0; HistoryIndex < NewHistoryArray.Num(); ++HistoryIndex)
				{
					const FPrimitiveOcclusionHistory& CopySourceHistory = NewHistoryArray[HistoryIndex];
					ViewPrimitiveOcclusionHistory.Add(CopySourceHistory);
				}

				//accumulate occluded prims across tasks
				NumOccludedPrims += Params[i].NumOccludedPrims;
			}
		}
		
		return NumOccludedPrims;
	}
	else
	{
		//SubIsOccluded stuff needs a frame's lifetime
		TArray<bool>& SubIsOccluded = View.FrameSubIsOccluded[SubIsOccludedArrayIndex];
		SubIsOccluded.Reset();

		static TArray<FOcclusionBounds> PendingIndividualQueriesWhenOptimizing;
		PendingIndividualQueriesWhenOptimizing.Reset();

		static TArray<FOcclusionBounds*> PendingIndividualQueriesWhenOptimizingSorter;
		PendingIndividualQueriesWhenOptimizingSorter.Reset();

		FViewElementPDI OcclusionPDI(&View, nullptr, nullptr);
		int32 StartIndex = 0;
		int32 NumToProcess = View.PrimitiveVisibilityMap.Num();				
		FVisForPrimParams Params(
			Scene,
			&View,
			&OcclusionPDI,
			StartIndex,
			NumToProcess,
			bSubmitQueries,
			bHZBOcclusion,			
			nullptr,
			nullptr,
			nullptr,
			&PendingIndividualQueriesWhenOptimizing,
			&SubIsOccluded
			);

		FetchVisibilityForPrimitives_Range<true>(Params, &DynamicVertexBuffer);

		int32 IndQueries = PendingIndividualQueriesWhenOptimizing.Num();
		if (IndQueries)
		{
			int32 SoftMaxQueries = GRHIMaximumReccommendedOustandingOcclusionQueries / FMath::Min(NumBufferedFrames, 2); // extra RHIT frame does not count
			int32 UsedQueries = View.GroupedOcclusionQueries.GetNumBatchOcclusionQueries();

			int32 FirstQueryToDo = 0;
			int32 QueriesToDo = IndQueries;


			if (SoftMaxQueries < UsedQueries + IndQueries)
			{
				QueriesToDo = (IndQueries + 9) / 10;  // we need to make progress, even if it means stalling and waiting for the GPU. At a minimum, we will do 10%

				if (SoftMaxQueries > UsedQueries + QueriesToDo)
				{
					// we can do more than the minimum
					QueriesToDo = SoftMaxQueries - UsedQueries;
				}
			}
			if (QueriesToDo == IndQueries)
			{
				for (int32 Index = 0; Index < IndQueries; Index++)
				{
					FOcclusionBounds* RunQueriesIter = &PendingIndividualQueriesWhenOptimizing[Index];
					FPrimitiveOcclusionHistory* PrimitiveOcclusionHistory = ViewPrimitiveOcclusionHistory.Find(RunQueriesIter->PrimitiveOcclusionHistoryKey);

					PrimitiveOcclusionHistory->SetCurrentQuery(OcclusionFrameCounter,
						View.IndividualOcclusionQueries.BatchPrimitive(RunQueriesIter->BoundsOrigin, RunQueriesIter->BoundsExtent, DynamicVertexBuffer),
						NumBufferedFrames,
						false,
						Params.bNeedsScanOnRead
					);
				}
			}
			else
			{
				check(QueriesToDo < IndQueries);
				PendingIndividualQueriesWhenOptimizingSorter.Reserve(PendingIndividualQueriesWhenOptimizing.Num());
				for (int32 Index = 0; Index < IndQueries; Index++)
				{
					FOcclusionBounds* RunQueriesIter = &PendingIndividualQueriesWhenOptimizing[Index];
					PendingIndividualQueriesWhenOptimizingSorter.Add(RunQueriesIter);
				}

				PendingIndividualQueriesWhenOptimizingSorter.Sort(
					[](const FOcclusionBounds& A, const FOcclusionBounds& B) 
					{
						return A.LastQuerySubmitFrame < B.LastQuerySubmitFrame;
					}
				);
				for (int32 Index = 0; Index < QueriesToDo; Index++)
				{
					FOcclusionBounds* RunQueriesIter = PendingIndividualQueriesWhenOptimizingSorter[Index];
					FPrimitiveOcclusionHistory* PrimitiveOcclusionHistory = ViewPrimitiveOcclusionHistory.Find(RunQueriesIter->PrimitiveOcclusionHistoryKey);
					PrimitiveOcclusionHistory->SetCurrentQuery(OcclusionFrameCounter,
						View.IndividualOcclusionQueries.BatchPrimitive(RunQueriesIter->BoundsOrigin, RunQueriesIter->BoundsExtent, DynamicVertexBuffer),
						NumBufferedFrames,
						false,
						Params.bNeedsScanOnRead
					);
				}
			}


			// lets prevent this from staying too large for too long
			if (PendingIndividualQueriesWhenOptimizing.GetSlack() > IndQueries * 4)
			{
				PendingIndividualQueriesWhenOptimizing.Empty();
				PendingIndividualQueriesWhenOptimizingSorter.Empty();
			}
			else
			{
				PendingIndividualQueriesWhenOptimizing.Reset();
				PendingIndividualQueriesWhenOptimizingSorter.Reset();
			}
		}
		return Params.NumOccludedPrims;
	}
}

/**
 * Cull occluded primitives in the view.
 */
static int32 OcclusionCull(FRHICommandListImmediate& RHICmdList, const FScene* Scene, FViewInfo& View, FGlobalDynamicVertexBuffer& DynamicVertexBuffer)
{
	SCOPE_CYCLE_COUNTER(STAT_OcclusionCull);	
	RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_OcclusionReadback));

	// INITVIEWS_TODO: This could be more efficient if broken up in to separate concerns:
	// - What is occluded?
	// - For which primitives should we render occlusion queries?
	// - Generate occlusion query geometry.

	int32 NumOccludedPrimitives = 0;
	FSceneViewState* ViewState = (FSceneViewState*)View.State;
	
	// Disable HZB on OpenGL platforms to avoid rendering artifacts
	// It can be forced on by setting HZBOcclusion to 2
	bool bHZBOcclusion = (!IsOpenGLPlatform(GShaderPlatformForFeatureLevel[Scene->GetFeatureLevel()]) && !IsSwitchPlatform(GShaderPlatformForFeatureLevel[Scene->GetFeatureLevel()]) && GHZBOcclusion) || (GHZBOcclusion == 2);

	// Use precomputed visibility data if it is available.
	if (View.PrecomputedVisibilityData)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_LookupPrecomputedVisibility);

		FViewElementPDI OcclusionPDI(&View, nullptr, nullptr);
		uint8 PrecomputedVisibilityFlags = EOcclusionFlags::CanBeOccluded | EOcclusionFlags::HasPrecomputedVisibility;
		for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
		{
			if ((Scene->PrimitiveOcclusionFlags[BitIt.GetIndex()] & PrecomputedVisibilityFlags) == PrecomputedVisibilityFlags)
			{
				FPrimitiveVisibilityId VisibilityId = Scene->PrimitiveVisibilityIds[BitIt.GetIndex()];
				if ((View.PrecomputedVisibilityData[VisibilityId.ByteIndex] & VisibilityId.BitMask) == 0)
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
					INC_DWORD_STAT_BY(STAT_StaticallyOccludedPrimitives,1);
					STAT(NumOccludedPrimitives++);

					if (GVisualizeOccludedPrimitives)
					{
						const FBoxSphereBounds& Bounds = Scene->PrimitiveOcclusionBounds[BitIt.GetIndex()];
						DrawWireBox(&OcclusionPDI, Bounds.GetBox(), FColor(100, 50, 50), SDPG_Foreground);
					}
				}
			}
		}
	}

	float CurrentRealTime = View.Family->CurrentRealTime;
	if (ViewState)
	{
		if (ViewState->SceneSoftwareOcclusion)
		{
			SCOPE_CYCLE_COUNTER(STAT_SoftwareOcclusionCull)
			NumOccludedPrimitives += ViewState->SceneSoftwareOcclusion->Process(RHICmdList, Scene, View);
		}
		else if (Scene->GetFeatureLevel() >= ERHIFeatureLevel::ES3_1)
		{
			bool bSubmitQueries = !View.bDisableQuerySubmissions;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			bSubmitQueries = bSubmitQueries && !ViewState->HasViewParent() && !ViewState->bIsFrozen;
#endif

			if( bHZBOcclusion )
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_MapHZBResults);
				check(!ViewState->HZBOcclusionTests.IsValidFrame(ViewState->OcclusionFrameCounter));
				ViewState->HZBOcclusionTests.MapResults(RHICmdList);
			}
 
			// Perform round-robin occlusion queries
			if (View.ViewState->IsRoundRobinEnabled() &&
				!View.bIsSceneCapture && // We only round-robin on the main renderer (not scene captures)
				!View.bIgnoreExistingQueries && // We do not alternate occlusion queries when we want to refresh the occlusion history
				(View.StereoPass == eSSP_LEFT_EYE || View.StereoPass == eSSP_RIGHT_EYE)) // Only relevant to stereo views
			{
				// For even frames, prevent left eye from occlusion querying
				// For odd frames, prevent right eye from occlusion querying
				const bool FrameParity = ((View.ViewState->PrevFrameNumber & 0x01) == 1);
				bSubmitQueries &= (FrameParity && View.StereoPass == eSSP_LEFT_EYE) ||
								  (!FrameParity && View.StereoPass == eSSP_RIGHT_EYE);
			}

			NumOccludedPrimitives += FetchVisibilityForPrimitives(Scene, View, bSubmitQueries, bHZBOcclusion, DynamicVertexBuffer);

			if( bHZBOcclusion )
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_HZBUnmapResults);

				ViewState->HZBOcclusionTests.UnmapResults(RHICmdList);

				if( bSubmitQueries )
				{
					ViewState->HZBOcclusionTests.SetValidFrameNumber(ViewState->OcclusionFrameCounter);
				}
			}
		}
		else
		{
			// No occlusion queries, so mark primitives as not occluded
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
			{
				View.PrimitiveDefinitelyUnoccludedMap.AccessCorrespondingBit(BitIt) = true;
			}
		}
	}
	RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_AfterOcclusionReadback));
	return NumOccludedPrimitives;
}

const int32 InputsPrimNumPerRelevancePacket = 128;
const int32 AverageMeshBatchNumPerRelevancePacket = InputsPrimNumPerRelevancePacket * 2;

template<class T, int TAmplifyFactor = 1>
struct FRelevancePrimSet
{
	enum
	{
		MaxInputPrims = InputsPrimNumPerRelevancePacket - 1, // leave space for NumPrims.
		MaxOutputPrims = MaxInputPrims * TAmplifyFactor
	};
	int32 NumPrims;

	T Prims[MaxOutputPrims];

	FORCEINLINE FRelevancePrimSet()
		: NumPrims(0)
	{
		//FMemory::Memzero(Prims, sizeof(T) * GetMaxOutputPrim());
	}
	FORCEINLINE void AddPrim(T Prim)
	{
		checkSlow(NumPrims < MaxOutputPrims);
		Prims[NumPrims++] = Prim;
	}
	FORCEINLINE bool IsFull() const
	{
		return NumPrims >= MaxOutputPrims;
	}
	template<class TARRAY>
	FORCEINLINE void AppendTo(TARRAY& DestArray)
	{
		DestArray.Append(Prims, NumPrims);
	}
};

struct FMarkRelevantStaticMeshesForViewData
{
	FVector ViewOrigin;
	int32 ForcedLODLevel;
	float LODScale;
	float InvLODScale;
	float MinScreenRadiusForCSMDepthSquared;
	float MinScreenRadiusForDepthPrepassSquared;
	bool bFullEarlyZPass;

	FMarkRelevantStaticMeshesForViewData(FViewInfo& View)
	{
		ViewOrigin = View.ViewMatrices.GetViewOrigin();

		// outside of the loop to be more efficient
		ForcedLODLevel = (View.Family->EngineShowFlags.LOD) ? GetCVarForceLOD() : 0;

		LODScale = CVarStaticMeshLODDistanceScale.GetValueOnRenderThread() * View.LODDistanceFactor;
		InvLODScale = 1.0f / LODScale;

		MinScreenRadiusForCSMDepthSquared = GMinScreenRadiusForCSMDepth * GMinScreenRadiusForCSMDepth;
		MinScreenRadiusForDepthPrepassSquared = GMinScreenRadiusForDepthPrepass * GMinScreenRadiusForDepthPrepass;

		extern bool ShouldForceFullDepthPass(EShaderPlatform ShaderPlatform);
		bFullEarlyZPass = ShouldForceFullDepthPass(View.GetShaderPlatform());
	}
};

namespace EMarkMaskBits
{
	enum Type
	{
		StaticMeshVisibilityMapMask = 0x2,
		StaticMeshFadeOutDitheredLODMapMask = 0x10,
		StaticMeshFadeInDitheredLODMapMask = 0x20,
	};
}

typedef TArray<FVisibleMeshDrawCommand, TInlineAllocator<AverageMeshBatchNumPerRelevancePacket>> FPassDrawCommandArray;
typedef TArray<const FStaticMeshBatch*, TInlineAllocator<AverageMeshBatchNumPerRelevancePacket>> FPassDrawCommandBuildRequestArray;

struct FDrawCommandRelevancePacket
{
	FDrawCommandRelevancePacket()
	{
		bUseCachedMeshDrawCommands = UseCachedMeshDrawCommands();

		for (int32 PassIndex = 0; PassIndex < EMeshPass::Num; ++PassIndex)
		{
			NumDynamicBuildRequestElements[PassIndex] = 0;
		}
	}

	FPassDrawCommandArray VisibleCachedDrawCommands[EMeshPass::Num];
	FPassDrawCommandBuildRequestArray DynamicBuildRequests[EMeshPass::Num];
	int32 NumDynamicBuildRequestElements[EMeshPass::Num];
	bool bUseCachedMeshDrawCommands;

	void AddCommandsForMesh(
		int32 PrimitiveIndex, 
		const FPrimitiveSceneInfo* InPrimitiveSceneInfo,
		const FStaticMeshBatchRelevance& RESTRICT StaticMeshRelevance, 
		const FStaticMeshBatch& RESTRICT StaticMesh, 
		const FScene* RESTRICT Scene, 
		bool bCanCache, 
		EMeshPass::Type PassType)
	{
		const EShadingPath ShadingPath = Scene->GetShadingPath();
		const bool bUseCachedMeshCommand = bUseCachedMeshDrawCommands
			&& !!(FPassProcessorManager::GetPassFlags(ShadingPath, PassType) & EMeshPassFlags::CachedMeshCommands)
			&& StaticMeshRelevance.bSupportsCachingMeshDrawCommands
			&& bCanCache;

		if (bUseCachedMeshCommand)
		{
			const int32 StaticMeshCommandInfoIndex = StaticMeshRelevance.GetStaticMeshCommandInfoIndex(PassType);
			if (StaticMeshCommandInfoIndex >= 0)
			{
				const FCachedMeshDrawCommandInfo& CachedMeshDrawCommand = InPrimitiveSceneInfo->StaticMeshCommandInfos[StaticMeshCommandInfoIndex];
				const FCachedPassMeshDrawList& SceneDrawList = Scene->CachedDrawLists[PassType];

				FVisibleMeshDrawCommand NewVisibleMeshDrawCommand;

				const FMeshDrawCommand* MeshDrawCommand = CachedMeshDrawCommand.StateBucketId >= 0
					? &Scene->CachedMeshDrawCommandStateBuckets[FSetElementId::FromInteger(CachedMeshDrawCommand.StateBucketId)].MeshDrawCommand
					: &SceneDrawList.MeshDrawCommands[CachedMeshDrawCommand.CommandIndex];

				NewVisibleMeshDrawCommand.Setup(
					MeshDrawCommand,
					PrimitiveIndex,
					CachedMeshDrawCommand.StateBucketId,
					CachedMeshDrawCommand.MeshFillMode,
					CachedMeshDrawCommand.MeshCullMode,
					CachedMeshDrawCommand.SortKey);

				VisibleCachedDrawCommands[(uint32)PassType].Add(NewVisibleMeshDrawCommand);
			}
		}
		else
		{
			NumDynamicBuildRequestElements[PassType] += StaticMeshRelevance.NumElements;
			DynamicBuildRequests[PassType].Add(&StaticMesh);
		}
	}
};

struct FRelevancePacket
{
	const float CurrentWorldTime;
	const float DeltaWorldTime;

	FRHICommandListImmediate& RHICmdList;
	const FScene* Scene;
	const FViewInfo& View;
	const FViewCommands& ViewCommands;
	const uint8 ViewBit;
	const FMarkRelevantStaticMeshesForViewData& ViewData;
	FPrimitiveViewMasks& OutHasDynamicMeshElementsMasks;
	FPrimitiveViewMasks& OutHasDynamicEditorMeshElementsMasks;
	uint8* RESTRICT MarkMasks;

	FRelevancePrimSet<int32> Input;
	FRelevancePrimSet<int32> RelevantStaticPrimitives;
	FRelevancePrimSet<int32> NotDrawRelevant;
	FRelevancePrimSet<int32> TranslucentSelfShadowPrimitives;
	FRelevancePrimSet<FPrimitiveSceneInfo*> VisibleDynamicPrimitivesWithSimpleLights;
	int32 NumVisibleDynamicPrimitives;
	int32 NumVisibleDynamicEditorPrimitives;
	FMeshPassMask VisibleDynamicMeshesPassMask;
	FTranslucenyPrimCount TranslucentPrimCount;
	bool bHasDistortionPrimitives;
	bool bHasCustomDepthPrimitives;
	FRelevancePrimSet<FPrimitiveSceneInfo*> LazyUpdatePrimitives;
	FRelevancePrimSet<FPrimitiveSceneInfo*> DirtyIndirectLightingCacheBufferPrimitives;
	FRelevancePrimSet<FPrimitiveSceneInfo*> RecachedReflectionCapturePrimitives;

	TArray<FMeshDecalBatch> MeshDecalBatches;
	TArray<FVolumetricMeshBatch> VolumetricMeshBatches;
	FDrawCommandRelevancePacket DrawCommandPacket;

	struct FPrimitiveLODMask
	{
		FPrimitiveLODMask()
			: PrimitiveIndex(INDEX_NONE)
		{}

		FPrimitiveLODMask(const int32 InPrimitiveIndex, const FLODMask& InLODMask)
			: PrimitiveIndex(InPrimitiveIndex)
			, LODMask(InLODMask)
		{}

		int32 PrimitiveIndex;
		FLODMask LODMask;
	};

	FRelevancePrimSet<FPrimitiveLODMask> PrimitivesLODMask; // group both lod mask with primitive index to be able to properly merge them in the view

	/** Custom Data for each primitive per view */
	struct FViewCustomData
	{
		FViewCustomData()
			: Primitive(nullptr)
			, CustomData(nullptr)
		{}

		FViewCustomData(const FPrimitiveSceneInfo* InPrimitive, void* InCustomData)
			: Primitive(InPrimitive)
			, CustomData(InCustomData)
		{}

		const FPrimitiveSceneInfo* Primitive;
		void* CustomData;
	};

	FRelevancePrimSet<FViewCustomData> PrimitivesCustomData; // group both custom data with primitive to be able to properly merge them in the view
	FMemStackBase& PrimitiveCustomDataMemStack;
	FPrimitiveViewMasks& OutHasViewCustomDataMasks;

	uint16 CombinedShadingModelMask;
	bool bUsesGlobalDistanceField;
	bool bUsesLightingChannels;
	bool bTranslucentSurfaceLighting;
	bool bUsesSceneDepth;

	FRelevancePacket(
		FRHICommandListImmediate& InRHICmdList,
		const FScene* InScene, 
		const FViewInfo& InView, 
		const FViewCommands& InViewCommands,
		uint8 InViewBit,
		const FMarkRelevantStaticMeshesForViewData& InViewData,
		FPrimitiveViewMasks& InOutHasDynamicMeshElementsMasks,
		FPrimitiveViewMasks& InOutHasDynamicEditorMeshElementsMasks,
		uint8* InMarkMasks,
		FMemStackBase& InPrimitiveCustomDataMemStack,
		FPrimitiveViewMasks& InOutHasViewCustomDataMasks)

		: CurrentWorldTime(InView.Family->CurrentWorldTime)
		, DeltaWorldTime(InView.Family->DeltaWorldTime)
		, RHICmdList(InRHICmdList)
		, Scene(InScene)
		, View(InView)
		, ViewCommands(InViewCommands)
		, ViewBit(InViewBit)
		, ViewData(InViewData)
		, OutHasDynamicMeshElementsMasks(InOutHasDynamicMeshElementsMasks)
		, OutHasDynamicEditorMeshElementsMasks(InOutHasDynamicEditorMeshElementsMasks)
		, MarkMasks(InMarkMasks)
		, NumVisibleDynamicPrimitives(0)
		, NumVisibleDynamicEditorPrimitives(0)
		, bHasDistortionPrimitives(false)
		, bHasCustomDepthPrimitives(false)
		, PrimitiveCustomDataMemStack(InPrimitiveCustomDataMemStack)
		, OutHasViewCustomDataMasks(InOutHasViewCustomDataMasks)
		, CombinedShadingModelMask(0)
		, bUsesGlobalDistanceField(false)
		, bUsesLightingChannels(false)
		, bTranslucentSurfaceLighting(false)
		, bUsesSceneDepth(false)
	{
	}

	void AnyThreadTask()
	{
		ComputeRelevance();
		MarkRelevant();
	}

	void ComputeRelevance()
	{
		CombinedShadingModelMask = 0;
		bUsesGlobalDistanceField = false;
		bUsesLightingChannels = false;
		bTranslucentSurfaceLighting = false;
		const EShadingPath ShadingPath = Scene->GetShadingPath();
		const bool bAddLightmapDensityCommands = View.Family->EngineShowFlags.LightMapDensity && AllowDebugViewmodes();

		SCOPE_CYCLE_COUNTER(STAT_ComputeViewRelevance);
		for (int32 Index = 0; Index < Input.NumPrims; Index++)
		{
			int32 BitIndex = Input.Prims[Index];
			FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene->Primitives[BitIndex];
			FPrimitiveViewRelevance& ViewRelevance = const_cast<FPrimitiveViewRelevance&>(View.PrimitiveViewRelevanceMap[BitIndex]);
			ViewRelevance = PrimitiveSceneInfo->Proxy->GetViewRelevance(&View);
			ViewRelevance.bInitializedThisFrame = true;

			const bool bStaticRelevance = ViewRelevance.bStaticRelevance;
			const bool bDrawRelevance = ViewRelevance.bDrawRelevance;
			const bool bDynamicRelevance = ViewRelevance.bDynamicRelevance;
			const bool bShadowRelevance = ViewRelevance.bShadowRelevance;
			const bool bEditorRelevance = ViewRelevance.bEditorPrimitiveRelevance;
			const bool bEditorSelectionRelevance = ViewRelevance.bEditorStaticSelectionRelevance;
			const bool bTranslucentRelevance = ViewRelevance.HasTranslucency();

			if (View.bIsReflectionCapture && !PrimitiveSceneInfo->Proxy->IsVisibleInReflectionCaptures())
			{
				NotDrawRelevant.AddPrim(BitIndex);
				continue;
			}

			if (bStaticRelevance && (bDrawRelevance || bShadowRelevance))
			{
				RelevantStaticPrimitives.AddPrim(BitIndex);
			}

			if (!bDrawRelevance)
			{
				NotDrawRelevant.AddPrim(BitIndex);
				continue;
			}

			if (bEditorRelevance)
			{
				++NumVisibleDynamicEditorPrimitives;

				if (GIsEditor)
				{
					OutHasDynamicEditorMeshElementsMasks[BitIndex] |= ViewBit;
				}
			}
			else if(bDynamicRelevance)
			{
				// Keep track of visible dynamic primitives.
				++NumVisibleDynamicPrimitives;
				OutHasDynamicMeshElementsMasks[BitIndex] |= ViewBit;

				if (ViewRelevance.bHasSimpleLights)
				{
					VisibleDynamicPrimitivesWithSimpleLights.AddPrim(PrimitiveSceneInfo);
				}
			}

			if (ViewRelevance.bUseCustomViewData)
			{				
				OutHasViewCustomDataMasks[BitIndex] |= ViewBit;
			}

			if (bTranslucentRelevance && !bEditorRelevance && ViewRelevance.bRenderInMainPass)
			{
				if (View.Family->AllowTranslucencyAfterDOF())
				{
					if (ViewRelevance.bNormalTranslucencyRelevance)
					{
						TranslucentPrimCount.Add(ETranslucencyPass::TPT_StandardTranslucency, ViewRelevance.bUsesSceneColorCopy, ViewRelevance.bDisableOffscreenRendering);
					}

					if (ViewRelevance.bSeparateTranslucencyRelevance)
					{
						TranslucentPrimCount.Add(ETranslucencyPass::TPT_TranslucencyAfterDOF, ViewRelevance.bUsesSceneColorCopy, ViewRelevance.bDisableOffscreenRendering);
					}
				}
				else // Otherwise, everything is rendered in a single bucket. This is not related to whether DOF is currently enabled or not.
				{
					// When using all translucency, Standard and AfterDOF are sorted together instead of being rendered like 2 buckets.
					TranslucentPrimCount.Add(ETranslucencyPass::TPT_AllTranslucency, ViewRelevance.bUsesSceneColorCopy, ViewRelevance.bDisableOffscreenRendering);
				}

				if (ViewRelevance.bDistortionRelevance)
				{
					bHasDistortionPrimitives = true;
				}
			}

			CombinedShadingModelMask |= ViewRelevance.ShadingModelMaskRelevance;
			bUsesGlobalDistanceField |= ViewRelevance.bUsesGlobalDistanceField;
			bUsesLightingChannels |= ViewRelevance.bUsesLightingChannels;
			bTranslucentSurfaceLighting |= ViewRelevance.bTranslucentSurfaceLighting;
			bUsesSceneDepth |= ViewRelevance.bUsesSceneDepth;

			if (ViewRelevance.bRenderCustomDepth)
			{
				bHasCustomDepthPrimitives = true;
			}

			extern bool GUseTranslucencyShadowDepths;
			if (GUseTranslucencyShadowDepths && ViewRelevance.bTranslucentSelfShadow)
			{
				TranslucentSelfShadowPrimitives.AddPrim(BitIndex);
			}

			// INITVIEWS_TODO: Do this in a separate pass? There are no dependencies
			// here except maybe ParentPrimitives. This could be done in a 
			// low-priority background task and forgotten about.

			PrimitiveSceneInfo->LastRenderTime = CurrentWorldTime;

			// If the primitive is definitely unoccluded or if in Wireframe mode and the primitive is estimated
			// to be unoccluded, then update the primitive components's LastRenderTime 
			// on the game thread. This signals that the primitive is visible.
			if (View.PrimitiveDefinitelyUnoccludedMap[BitIndex] || (View.Family->EngineShowFlags.Wireframe && View.PrimitiveVisibilityMap[BitIndex]))
			{
				// Update the PrimitiveComponent's LastRenderTime.
				*(PrimitiveSceneInfo->ComponentLastRenderTime) = CurrentWorldTime;
				*(PrimitiveSceneInfo->ComponentLastRenderTimeOnScreen) = CurrentWorldTime;
			}

			// Cache the nearest reflection proxy if needed
			if (PrimitiveSceneInfo->NeedsReflectionCaptureUpdate())
			{
				// mobile should not have any outstanding reflection capture update requests at this point
				ensure(Scene->GetShadingPath() != EShadingPath::Mobile);
		
				PrimitiveSceneInfo->CacheReflectionCaptures();

				// With forward shading we need to track reflection capture cache updates
				// in order to update primitive's uniform buffer's closest reflection capture id.
				if (IsForwardShadingEnabled(Scene->GetShaderPlatform()))
				{
					RecachedReflectionCapturePrimitives.AddPrim(PrimitiveSceneInfo);
				}
			}

			if (PrimitiveSceneInfo->NeedsUniformBufferUpdate())
			{
				LazyUpdatePrimitives.AddPrim(PrimitiveSceneInfo);
			}
			if (PrimitiveSceneInfo->NeedsIndirectLightingCacheBufferUpdate())
			{
				DirtyIndirectLightingCacheBufferPrimitives.AddPrim(PrimitiveSceneInfo);
			}
		}
	}

	void MarkRelevant()
	{
		SCOPE_CYCLE_COUNTER(STAT_StaticRelevance);

		// using a local counter to reduce memory traffic
		int32 NumVisibleStaticMeshElements = 0;
		FViewInfo& WriteView = const_cast<FViewInfo&>(View);
		const FSceneViewState* ViewState = (FSceneViewState*)View.State;
		const EShadingPath ShadingPath = Scene->GetShadingPath();

		const bool bHLODActive = Scene->SceneLODHierarchy.IsActive();
		const FHLODVisibilityState* const HLODState = bHLODActive && ViewState ? &ViewState->HLODVisibilityState : nullptr;

		for (int32 StaticPrimIndex = 0, Num = RelevantStaticPrimitives.NumPrims; StaticPrimIndex < Num; ++StaticPrimIndex)
		{
			int32 PrimitiveIndex = RelevantStaticPrimitives.Prims[StaticPrimIndex];
			const FPrimitiveSceneInfo* RESTRICT PrimitiveSceneInfo = Scene->Primitives[PrimitiveIndex];
			const FPrimitiveBounds& Bounds = Scene->PrimitiveBounds[PrimitiveIndex];
			const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveIndex];
			const bool bIsPrimitiveDistanceCullFading = View.PrimitiveFadeUniformBufferMap[PrimitiveIndex];

			float MeshScreenSizeSquared = 0;
			FLODMask LODToRender;

			if (PrimitiveSceneInfo->bIsUsingCustomLODRules)
			{
				LODToRender = PrimitiveSceneInfo->Proxy->GetCustomLOD(View, View.LODDistanceFactor, ViewData.ForcedLODLevel, MeshScreenSizeSquared);
			}
			else
			{
				LODToRender = ComputeLODForMeshes(PrimitiveSceneInfo->StaticMeshRelevances, View, Bounds.BoxSphereBounds.Origin, Bounds.BoxSphereBounds.SphereRadius, ViewData.ForcedLODLevel, MeshScreenSizeSquared, ViewData.LODScale);
			}

			PrimitivesLODMask.AddPrim(FRelevancePacket::FPrimitiveLODMask(PrimitiveIndex, LODToRender));

			void* UserViewCustomData = nullptr;

			if (OutHasViewCustomDataMasks[PrimitiveIndex] != 0) // Has a relevance for this view
			{
				UserViewCustomData = PrimitiveSceneInfo->Proxy->InitViewCustomData(View, View.LODDistanceFactor, PrimitiveCustomDataMemStack, true, false, &LODToRender, MeshScreenSizeSquared);

				if (UserViewCustomData != nullptr)
				{
					PrimitivesCustomData.AddPrim(FRelevancePacket::FViewCustomData(PrimitiveSceneInfo, UserViewCustomData));
				}
			}

			const bool bIsHLODFading = HLODState ? HLODState->IsNodeFading(PrimitiveIndex) : false;
			const bool bIsHLODFadingOut = HLODState ? HLODState->IsNodeFadingOut(PrimitiveIndex) : false;
			const bool bIsLODDithered = LODToRender.IsDithered();

			float DistanceSquared = (Bounds.BoxSphereBounds.Origin - ViewData.ViewOrigin).SizeSquared();
			const float LODFactorDistanceSquared = DistanceSquared * FMath::Square(View.LODDistanceFactor * ViewData.InvLODScale);
			const bool bDrawShadowDepth = FMath::Square(Bounds.BoxSphereBounds.SphereRadius) > ViewData.MinScreenRadiusForCSMDepthSquared * LODFactorDistanceSquared;
			const bool bDrawDepthOnly = ViewData.bFullEarlyZPass || FMath::Square(Bounds.BoxSphereBounds.SphereRadius) > GMinScreenRadiusForDepthPrepass * GMinScreenRadiusForDepthPrepass * LODFactorDistanceSquared;

			const bool bAddLightmapDensityCommands = View.Family->EngineShowFlags.LightMapDensity && AllowDebugViewmodes();

			const int32 NumStaticMeshes = PrimitiveSceneInfo->StaticMeshRelevances.Num();
			for(int32 MeshIndex = 0;MeshIndex < NumStaticMeshes;MeshIndex++)
			{
				const FStaticMeshBatchRelevance& StaticMeshRelevance = PrimitiveSceneInfo->StaticMeshRelevances[MeshIndex];
				const FStaticMeshBatch& StaticMesh = PrimitiveSceneInfo->StaticMeshes[MeshIndex];

				if (LODToRender.ContainsLOD(StaticMeshRelevance.LODIndex))
				{
					uint8 MarkMask = 0;
					bool bNeedsBatchVisibility = false;
					bool bHiddenByHLODFade = false; // Hide mesh LOD levels that HLOD is substituting

					if (bIsHLODFading)
					{
						if (bIsHLODFadingOut)
						{
							if (bIsLODDithered && LODToRender.DitheredLODIndices[1] == StaticMeshRelevance.LODIndex)
							{
								bHiddenByHLODFade = true;
							}
							else
							{
								MarkMask |= EMarkMaskBits::StaticMeshFadeOutDitheredLODMapMask;	
							}
						}
						else
						{
							if (bIsLODDithered && LODToRender.DitheredLODIndices[0] == StaticMeshRelevance.LODIndex)
							{
								bHiddenByHLODFade = true;
							}
							else
							{
								MarkMask |= EMarkMaskBits::StaticMeshFadeInDitheredLODMapMask;
							}
						}
					}
					else if (bIsLODDithered)
					{
						if (LODToRender.DitheredLODIndices[0] == StaticMeshRelevance.LODIndex)
						{
							MarkMask |= EMarkMaskBits::StaticMeshFadeOutDitheredLODMapMask;
						}
						else
						{
							MarkMask |= EMarkMaskBits::StaticMeshFadeInDitheredLODMapMask;
						}
					}

					// Don't cache if it requires per view per mesh state for LOD dithering or distance cull fade.
					const bool bIsMeshDitheringLOD = StaticMeshRelevance.bDitheredLODTransition && (MarkMask & (EMarkMaskBits::StaticMeshFadeOutDitheredLODMapMask | EMarkMaskBits::StaticMeshFadeInDitheredLODMapMask));
					const bool bCanCache = !bIsPrimitiveDistanceCullFading && !bIsMeshDitheringLOD;

					if (ViewRelevance.bShadowRelevance && bDrawShadowDepth && StaticMeshRelevance.CastShadow)
					{
						bNeedsBatchVisibility = true;
					}

					if (ViewRelevance.bDrawRelevance)
					{
						if ((StaticMeshRelevance.bUseForMaterial || StaticMeshRelevance.bUseAsOccluder) 
							&& (ViewRelevance.bRenderInMainPass || ViewRelevance.bRenderCustomDepth) 
							&& !bHiddenByHLODFade)
						{
							if (StaticMeshRelevance.bUseForDepthPass && bDrawDepthOnly)
							{
								DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::DepthPass);
							}

							// Mark static mesh as visible for rendering
							if (StaticMeshRelevance.bUseForMaterial)
							{
								DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::BasePass);
								MarkMask |= EMarkMaskBits::StaticMeshVisibilityMapMask;

								if (ShadingPath == EShadingPath::Mobile)
								{
									DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::MobileBasePassCSM);
								}

								if (ViewRelevance.bRenderCustomDepth)
								{
									DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::CustomDepth);
								}

								if (bAddLightmapDensityCommands)
								{
									DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::LightmapDensity);
								}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
								else if (View.Family->UseDebugViewPS())
								{
									DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::DebugViewMode);
								}
#endif

#if WITH_EDITOR
								if (StaticMeshRelevance.bSelectable)
								{
									if (View.bAllowTranslucentPrimitivesInHitProxy)
									{
										DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::HitProxy);
									}
									else
									{
										DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::HitProxyOpaqueOnly);
									}
								}
#endif

								if (ViewRelevance.bVelocityRelevance
									&& FVelocityRendering::PrimitiveHasVelocity(View.GetFeatureLevel(), PrimitiveSceneInfo)
									&& FVelocityRendering::PrimitiveHasVelocityForView(View, Bounds.BoxSphereBounds, PrimitiveSceneInfo))
								{
									DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::Velocity);
								}

								++NumVisibleStaticMeshElements;
							}

							bNeedsBatchVisibility = true;
						}

						if (StaticMeshRelevance.bUseForMaterial
							&& ViewRelevance.HasTranslucency()
							&& !ViewRelevance.bEditorPrimitiveRelevance
							&& ViewRelevance.bRenderInMainPass)
						{
							if (View.Family->AllowTranslucencyAfterDOF())
							{
								if (ViewRelevance.bNormalTranslucencyRelevance)
								{
									DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::TranslucencyStandard);
								}

								if (ViewRelevance.bSeparateTranslucencyRelevance)
								{
									DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::TranslucencyAfterDOF);
								}
							}
							else
							{
								// Otherwise, everything is rendered in a single bucket. This is not related to whether DOF is currently enabled or not.
								// When using all translucency, Standard and AfterDOF are sorted together instead of being rendered like 2 buckets.
								DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::TranslucencyAll);
							}

							if (ViewRelevance.bDistortionRelevance)
							{
								DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::Distortion);
							}

							if (ShadingPath == EShadingPath::Mobile && View.bIsSceneCapture)
							{
								DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::MobileInverseOpacity);
							}
						}

#if WITH_EDITOR
						if (ViewRelevance.bEditorStaticSelectionRelevance)
						{
							DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::EditorSelection);
						}
#endif

						if (ViewRelevance.bHasVolumeMaterialDomain)
						{
							VolumetricMeshBatches.AddUninitialized(1);
							FVolumetricMeshBatch& BatchAndProxy = VolumetricMeshBatches.Last();
							BatchAndProxy.Mesh = &StaticMesh;
							BatchAndProxy.Proxy = PrimitiveSceneInfo->Proxy;
						}

						if (ViewRelevance.bRenderInMainPass && ViewRelevance.bDecal)
						{
							MeshDecalBatches.AddUninitialized(1);
							FMeshDecalBatch& BatchAndProxy = MeshDecalBatches.Last();
							BatchAndProxy.Mesh = &StaticMesh;
							BatchAndProxy.Proxy = PrimitiveSceneInfo->Proxy;
							BatchAndProxy.SortKey = PrimitiveSceneInfo->Proxy->GetTranslucencySortPriority();
						}
					}

					if (MarkMask)
					{
						MarkMasks[StaticMeshRelevance.Id] = MarkMask;
					}

					// Static meshes which don't need per-element visibility always draw all elements
					if (bNeedsBatchVisibility && StaticMeshRelevance.bRequiresPerElementVisibility)
					{
						WriteView.StaticMeshBatchVisibility[StaticMesh.BatchVisibilityId] = StaticMesh.VertexFactory->GetStaticBatchElementVisibility(View, &StaticMesh, UserViewCustomData);
					}
				}
			}
		}
		static_assert(sizeof(WriteView.NumVisibleStaticMeshElements) == sizeof(int32), "Atomic is the wrong size");
		FPlatformAtomics::InterlockedAdd((volatile int32*)&WriteView.NumVisibleStaticMeshElements, NumVisibleStaticMeshElements);
	}

	void RenderThreadFinalize()
	{
		FViewInfo& WriteView = const_cast<FViewInfo&>(View);
		FViewCommands& WriteViewCommands = const_cast<FViewCommands&>(ViewCommands);
		
		for (int32 Index = 0; Index < NotDrawRelevant.NumPrims; Index++)
		{
			WriteView.PrimitiveVisibilityMap[NotDrawRelevant.Prims[Index]] = false;
		}

		WriteView.ShadingModelMaskInView |= CombinedShadingModelMask;
		WriteView.bUsesGlobalDistanceField |= bUsesGlobalDistanceField;
		WriteView.bUsesLightingChannels |= bUsesLightingChannels;
		WriteView.bTranslucentSurfaceLighting |= bTranslucentSurfaceLighting;
		WriteView.bUsesSceneDepth |= bUsesSceneDepth;
		VisibleDynamicPrimitivesWithSimpleLights.AppendTo(WriteView.VisibleDynamicPrimitivesWithSimpleLights);
		WriteView.NumVisibleDynamicPrimitives += NumVisibleDynamicPrimitives;
		WriteView.NumVisibleDynamicEditorPrimitives += NumVisibleDynamicEditorPrimitives;
		WriteView.TranslucentPrimCount.Append(TranslucentPrimCount);
		WriteView.bHasDistortionPrimitives |= bHasDistortionPrimitives;
		WriteView.bHasCustomDepthPrimitives |= bHasCustomDepthPrimitives;
		DirtyIndirectLightingCacheBufferPrimitives.AppendTo(WriteView.DirtyIndirectLightingCacheBufferPrimitives);

		WriteView.MeshDecalBatches.Append(MeshDecalBatches);
		WriteView.VolumetricMeshBatches.Append(VolumetricMeshBatches);

		for (int32 Index = 0; Index < RecachedReflectionCapturePrimitives.NumPrims; ++Index)
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = RecachedReflectionCapturePrimitives.Prims[Index];

			PrimitiveSceneInfo->SetNeedsUniformBufferUpdate(true);
			PrimitiveSceneInfo->ConditionalUpdateUniformBuffer(RHICmdList);

			FScene& WriteScene = *const_cast<FScene*>(Scene);
			AddPrimitiveToUpdateGPU(WriteScene, PrimitiveSceneInfo->GetIndex());
		}

		for (int32 Index = 0; Index < LazyUpdatePrimitives.NumPrims; Index++)
		{
			LazyUpdatePrimitives.Prims[Index]->ConditionalUpdateUniformBuffer(RHICmdList);
		}

		for (int32 i = 0; i < PrimitivesCustomData.NumPrims; ++i)
		{
			WriteView.SetCustomData(PrimitivesCustomData.Prims[i].Primitive, PrimitivesCustomData.Prims[i].CustomData);
		}		

		for (int32 i = 0; i < PrimitivesLODMask.NumPrims; ++i)
		{
			WriteView.PrimitivesLODMask[PrimitivesLODMask.Prims[i].PrimitiveIndex] = PrimitivesLODMask.Prims[i].LODMask;
		}

		for (int32 PassIndex = 0; PassIndex < EMeshPass::Num; PassIndex++)
		{
			FPassDrawCommandArray& SrcCommands = DrawCommandPacket.VisibleCachedDrawCommands[PassIndex];
			FMeshCommandOneFrameArray& DstCommands = WriteViewCommands.MeshCommands[PassIndex];
			if (SrcCommands.Num() > 0)
			{
				static_assert(sizeof(SrcCommands[0]) == sizeof(DstCommands[0]), "Memcpy sizes must match.");
				const int32 PrevNum = DstCommands.AddUninitialized(SrcCommands.Num());
				FMemory::Memcpy(&DstCommands[PrevNum], &SrcCommands[0], SrcCommands.Num() * sizeof(SrcCommands[0]));
			}

			FPassDrawCommandBuildRequestArray& SrcRequests = DrawCommandPacket.DynamicBuildRequests[PassIndex];
			TArray<const FStaticMeshBatch*, SceneRenderingAllocator>& DstRequests = WriteViewCommands.DynamicMeshCommandBuildRequests[PassIndex];
			if (SrcRequests.Num() > 0)
			{
				static_assert(sizeof(SrcRequests[0]) == sizeof(DstRequests[0]), "Memcpy sizes must match.");
				const int32 PrevNum = DstRequests.AddUninitialized(SrcRequests.Num());
				FMemory::Memcpy(&DstRequests[PrevNum], &SrcRequests[0], SrcRequests.Num() * sizeof(SrcRequests[0]));
			}

			WriteViewCommands.NumDynamicMeshCommandBuildRequestElements[PassIndex] += DrawCommandPacket.NumDynamicBuildRequestElements[PassIndex];
		}

		// Prepare translucent self shadow uniform buffers.
		for (int32 Index = 0; Index < TranslucentSelfShadowPrimitives.NumPrims; ++Index)
		{
			const int32 PrimitiveIndex = TranslucentSelfShadowPrimitives.Prims[Index];

			FUniformBufferRHIRef& UniformBuffer = WriteView.TranslucentSelfShadowUniformBufferMap.FindOrAdd(PrimitiveIndex);

			if (!UniformBuffer)
			{
				FTranslucentSelfShadowUniformParameters Parameters;
				SetupTranslucentSelfShadowUniformParameters(nullptr, Parameters);
				UniformBuffer = FTranslucentSelfShadowUniformParameters::CreateUniformBuffer(Parameters, EUniformBufferUsage::UniformBuffer_SingleFrame);
			}
		}
	}
};

static void ComputeAndMarkRelevanceForViewParallel(
	FRHICommandListImmediate& RHICmdList,
	const FScene* Scene,
	FViewInfo& View,
	FViewCommands& ViewCommands,
	uint8 ViewBit,
	FPrimitiveViewMasks& OutHasDynamicMeshElementsMasks,
	FPrimitiveViewMasks& OutHasDynamicEditorMeshElementsMasks,
	FPrimitiveViewMasks& HasViewCustomDataMasks
	)
{
	check(OutHasDynamicMeshElementsMasks.Num() == Scene->Primitives.Num());

	FFrozenSceneViewMatricesGuard FrozenMatricesGuard(View);
	const FMarkRelevantStaticMeshesForViewData ViewData(View);

	int32 NumMesh = View.StaticMeshVisibilityMap.Num();
	uint8* RESTRICT MarkMasks = (uint8*)FMemStack::Get().Alloc(NumMesh + 31 , 8); // some padding to simplify the high speed transpose
	FMemory::Memzero(MarkMasks, NumMesh + 31);

	int32 EstimateOfNumPackets = NumMesh / (FRelevancePrimSet<int32>::MaxInputPrims * 4);

	TArray<FRelevancePacket*,SceneRenderingAllocator> Packets;
	Packets.Reserve(EstimateOfNumPackets);

	bool WillExecuteInParallel = FApp::ShouldUseThreadingForPerformance() && CVarParallelInitViews.GetValueOnRenderThread() > 0 && IsInActualRenderingThread();

	{
		FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap);
		if (BitIt)
		{

			FRelevancePacket* Packet = new(FMemStack::Get()) FRelevancePacket(
				RHICmdList,
				Scene, 
				View, 
				ViewCommands,
				ViewBit,
				ViewData,
				OutHasDynamicMeshElementsMasks,
				OutHasDynamicEditorMeshElementsMasks,
				MarkMasks, 
				WillExecuteInParallel ? View.AllocateCustomDataMemStack() : View.GetCustomDataGlobalMemStack(),
				HasViewCustomDataMasks);
			Packets.Add(Packet);

			while (1)
			{
				Packet->Input.AddPrim(BitIt.GetIndex());
				++BitIt;
				if (Packet->Input.IsFull() || !BitIt)
				{
					if (!BitIt)
					{
						break;
					}
					else
					{
						Packet = new(FMemStack::Get()) FRelevancePacket(
							RHICmdList,
							Scene, 
							View, 
							ViewCommands,
							ViewBit,
							ViewData,
							OutHasDynamicMeshElementsMasks,
							OutHasDynamicEditorMeshElementsMasks,
							MarkMasks,
							WillExecuteInParallel ? View.AllocateCustomDataMemStack() : View.GetCustomDataGlobalMemStack(),
							HasViewCustomDataMasks);
						Packets.Add(Packet);
					}
				}
			}
		}
	}
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ComputeAndMarkRelevanceForViewParallel_ParallelFor);
		ParallelFor(Packets.Num(), 
			[&Packets](int32 Index)
			{
				Packets[Index]->AnyThreadTask();
			},
			!WillExecuteInParallel
		);
	}
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ComputeAndMarkRelevanceForViewParallel_RenderThreadFinalize);

		for (int32 PassIndex = 0; PassIndex < EMeshPass::Num; PassIndex++)
		{
			int32 NumVisibleCachedMeshDrawCommands = 0;
			int32 NumDynamicBuildRequests = 0;

			for (auto Packet : Packets)
			{
				NumVisibleCachedMeshDrawCommands += Packet->DrawCommandPacket.VisibleCachedDrawCommands[PassIndex].Num();
				NumDynamicBuildRequests += Packet->DrawCommandPacket.DynamicBuildRequests[PassIndex].Num();
			}

			ViewCommands.MeshCommands[PassIndex].Reserve(NumVisibleCachedMeshDrawCommands);
			ViewCommands.DynamicMeshCommandBuildRequests[PassIndex].Reserve(NumDynamicBuildRequests);
		}

		for (auto Packet : Packets)
		{
			Packet->RenderThreadFinalize();
			Packet->~FRelevancePacket();
		}

		Packets.Empty();
	}

	QUICK_SCOPE_CYCLE_COUNTER(STAT_ComputeAndMarkRelevanceForViewParallel_TransposeMeshBits);
	check(View.StaticMeshVisibilityMap.Num() == NumMesh && 
		View.StaticMeshFadeOutDitheredLODMap.Num() == NumMesh && 
		View.StaticMeshFadeInDitheredLODMap.Num() == NumMesh
		);
	uint32* RESTRICT StaticMeshVisibilityMap_Words = View.StaticMeshVisibilityMap.GetData();
	uint32* RESTRICT StaticMeshFadeOutDitheredLODMap_Words = View.StaticMeshFadeOutDitheredLODMap.GetData();
	uint32* RESTRICT StaticMeshFadeInDitheredLODMap_Words = View.StaticMeshFadeInDitheredLODMap.GetData();
	const uint64* RESTRICT MarkMasks64 = (const uint64* RESTRICT)MarkMasks;
	const uint8* RESTRICT MarkMasks8 = MarkMasks;
	for (int32 BaseIndex = 0; BaseIndex < NumMesh; BaseIndex += 32)
	{
		uint32 StaticMeshVisibilityMap_Word = 0;
		uint32 StaticMeshFadeOutDitheredLODMap_Word = 0;
		uint32 StaticMeshFadeInDitheredLODMap_Word = 0;
		uint32 Mask = 1;
		bool bAny = false;
		for (int32 QWordIndex = 0; QWordIndex < 4; QWordIndex++)
		{
			if (*MarkMasks64++)
			{
				for (int32 ByteIndex = 0; ByteIndex < 8; ByteIndex++, Mask <<= 1, MarkMasks8++)
				{
					uint8 MaskMask = *MarkMasks8;
					StaticMeshVisibilityMap_Word |= (MaskMask & EMarkMaskBits::StaticMeshVisibilityMapMask) ? Mask : 0;
					StaticMeshFadeOutDitheredLODMap_Word |= (MaskMask & EMarkMaskBits::StaticMeshFadeOutDitheredLODMapMask) ? Mask : 0;
					StaticMeshFadeInDitheredLODMap_Word |= (MaskMask & EMarkMaskBits::StaticMeshFadeInDitheredLODMapMask) ? Mask : 0;
				}
				bAny = true;
			}
			else
			{
				MarkMasks8 += 8;
				Mask <<= 8;
			}
		}
		if (bAny)
		{
			checkSlow(!*StaticMeshVisibilityMap_Words && !*StaticMeshFadeOutDitheredLODMap_Words && !*StaticMeshFadeInDitheredLODMap_Words);
			*StaticMeshVisibilityMap_Words = StaticMeshVisibilityMap_Word;
			*StaticMeshFadeOutDitheredLODMap_Words = StaticMeshFadeOutDitheredLODMap_Word;
			*StaticMeshFadeInDitheredLODMap_Words = StaticMeshFadeInDitheredLODMap_Word;
		}
		StaticMeshVisibilityMap_Words++;
		StaticMeshFadeOutDitheredLODMap_Words++;
		StaticMeshFadeInDitheredLODMap_Words++;
	}
}

static void SetDynamicMeshElementViewCustomData(TArray<FViewInfo>& InViews, const FPrimitiveViewMasks& InHasViewCustomDataMasks, const FPrimitiveSceneInfo* InPrimitiveSceneInfo)
{
	int32 PrimitiveIndex = InPrimitiveSceneInfo->GetIndex();

	if (InHasViewCustomDataMasks[PrimitiveIndex] != 0)
	{
		for (int32 ViewIndex = 0; ViewIndex < InViews.Num(); ViewIndex++)
		{
			FViewInfo& ViewInfo = InViews[ViewIndex];

			if (InHasViewCustomDataMasks[PrimitiveIndex] & (1 << ViewIndex) && ViewInfo.GetCustomData(InPrimitiveSceneInfo->GetIndex()) == nullptr)
			{
				ViewInfo.SetCustomData(InPrimitiveSceneInfo, InPrimitiveSceneInfo->Proxy->InitViewCustomData(ViewInfo, ViewInfo.LODDistanceFactor, ViewInfo.GetCustomDataGlobalMemStack(), false, false));
			}
		}
	}
}

void ComputeDynamicMeshRelevance(EShadingPath ShadingPath, bool bAddLightmapDensityCommands, const FPrimitiveViewRelevance& ViewRelevance, const FMeshBatchAndRelevance& MeshBatch, FViewInfo& View, FMeshPassMask& PassMask)
{
	const int32 NumElements = MeshBatch.Mesh->Elements.Num();

	if (ViewRelevance.bDrawRelevance && (ViewRelevance.bRenderInMainPass || ViewRelevance.bRenderCustomDepth))
	{
		PassMask.Set(EMeshPass::DepthPass);
		View.NumVisibleDynamicMeshElements[EMeshPass::DepthPass] += NumElements;

		PassMask.Set(EMeshPass::BasePass);
		View.NumVisibleDynamicMeshElements[EMeshPass::BasePass] += NumElements;

		if (ShadingPath == EShadingPath::Mobile)
		{
			PassMask.Set(EMeshPass::MobileBasePassCSM);
			View.NumVisibleDynamicMeshElements[EMeshPass::MobileBasePassCSM] += NumElements;
		}

		if (ViewRelevance.bRenderCustomDepth)
		{
			PassMask.Set(EMeshPass::CustomDepth);
			View.NumVisibleDynamicMeshElements[EMeshPass::CustomDepth] += NumElements;
		}

		if (bAddLightmapDensityCommands)
		{
			PassMask.Set(EMeshPass::LightmapDensity);
			View.NumVisibleDynamicMeshElements[EMeshPass::LightmapDensity] += NumElements;
		}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		else if (View.Family->UseDebugViewPS())
		{
			PassMask.Set(EMeshPass::DebugViewMode);
			View.NumVisibleDynamicMeshElements[EMeshPass::DebugViewMode] += NumElements;
		}
#endif

#if WITH_EDITOR
		if (View.bAllowTranslucentPrimitivesInHitProxy)
		{
			PassMask.Set(EMeshPass::HitProxy);
			View.NumVisibleDynamicMeshElements[EMeshPass::HitProxy] += NumElements;
		}
		else
		{
			PassMask.Set(EMeshPass::HitProxyOpaqueOnly);
			View.NumVisibleDynamicMeshElements[EMeshPass::HitProxyOpaqueOnly] += NumElements;
		}
#endif

		if (ViewRelevance.bVelocityRelevance)
		{
			PassMask.Set(EMeshPass::Velocity);
			View.NumVisibleDynamicMeshElements[EMeshPass::Velocity] += NumElements;
		}
	}

	if (ViewRelevance.HasTranslucency()
		&& !ViewRelevance.bEditorPrimitiveRelevance
		&& ViewRelevance.bRenderInMainPass)
	{
		if (View.Family->AllowTranslucencyAfterDOF())
		{
			if (ViewRelevance.bNormalTranslucencyRelevance)
			{
				PassMask.Set(EMeshPass::TranslucencyStandard);
				View.NumVisibleDynamicMeshElements[EMeshPass::TranslucencyStandard] += NumElements;
			}

			if (ViewRelevance.bSeparateTranslucencyRelevance)
			{
				PassMask.Set(EMeshPass::TranslucencyAfterDOF);
				View.NumVisibleDynamicMeshElements[EMeshPass::TranslucencyAfterDOF] += NumElements;
			}
		}
		else
		{
			PassMask.Set(EMeshPass::TranslucencyAll);
			View.NumVisibleDynamicMeshElements[EMeshPass::TranslucencyAll] += NumElements;
		}

		if (ViewRelevance.bDistortionRelevance)
		{
			PassMask.Set(EMeshPass::Distortion);
			View.NumVisibleDynamicMeshElements[EMeshPass::Distortion] += NumElements;
		}

		if (ShadingPath == EShadingPath::Mobile && View.bIsSceneCapture)
		{
			PassMask.Set(EMeshPass::MobileInverseOpacity);
			View.NumVisibleDynamicMeshElements[EMeshPass::MobileInverseOpacity] += NumElements;
		}
	}

#if WITH_EDITOR
	if (ViewRelevance.bDrawRelevance)
	{
		PassMask.Set(EMeshPass::EditorSelection);
		View.NumVisibleDynamicMeshElements[EMeshPass::EditorSelection] += NumElements;
	}
#endif

	if (ViewRelevance.bHasVolumeMaterialDomain)
	{
		View.VolumetricMeshBatches.AddUninitialized(1);
		FVolumetricMeshBatch& BatchAndProxy = View.VolumetricMeshBatches.Last();
		BatchAndProxy.Mesh = MeshBatch.Mesh;
		BatchAndProxy.Proxy = MeshBatch.PrimitiveSceneProxy;
	}

	if (ViewRelevance.bRenderInMainPass && ViewRelevance.bDecal)
	{
		View.MeshDecalBatches.AddUninitialized(1);
		FMeshDecalBatch& BatchAndProxy = View.MeshDecalBatches.Last();
		BatchAndProxy.Mesh = MeshBatch.Mesh;
		BatchAndProxy.Proxy = MeshBatch.PrimitiveSceneProxy;
		BatchAndProxy.SortKey = MeshBatch.PrimitiveSceneProxy->GetTranslucencySortPriority();
	}
}

void FSceneRenderer::GatherDynamicMeshElements(
	TArray<FViewInfo>& InViews, 
	const FScene* InScene, 
	const FSceneViewFamily& InViewFamily, 
	FGlobalDynamicIndexBuffer& DynamicIndexBuffer,
	FGlobalDynamicVertexBuffer& DynamicVertexBuffer,
	FGlobalDynamicReadBuffer& DynamicReadBuffer,
	const FPrimitiveViewMasks& HasDynamicMeshElementsMasks, 
	const FPrimitiveViewMasks& HasDynamicEditorMeshElementsMasks, 
	const FPrimitiveViewMasks& HasViewCustomDataMasks,
	FMeshElementCollector& Collector)
{
	SCOPE_CYCLE_COUNTER(STAT_GetDynamicMeshElements);

	int32 NumPrimitives = InScene->Primitives.Num();
	check(HasDynamicMeshElementsMasks.Num() == NumPrimitives);

	int32 ViewCount = InViews.Num();
	{
		Collector.ClearViewMeshArrays();

		for (int32 ViewIndex = 0; ViewIndex < ViewCount; ViewIndex++)
		{
			Collector.AddViewMeshArrays(
				&InViews[ViewIndex], 
				&InViews[ViewIndex].DynamicMeshElements,
				&InViews[ViewIndex].SimpleElementCollector,
				&InViews[ViewIndex].DynamicPrimitiveShaderData, 
				InViewFamily.GetFeatureLevel(),
				&DynamicIndexBuffer,
				&DynamicVertexBuffer,
				&DynamicReadBuffer);
		}

		const bool bIsInstancedStereo = (ViewCount > 0) ? (InViews[0].IsInstancedStereoPass() || InViews[0].bIsMobileMultiViewEnabled) : false;
		const EShadingPath ShadingPath = Scene->GetShadingPath();

		for (int32 PrimitiveIndex = 0; PrimitiveIndex < NumPrimitives; ++PrimitiveIndex)
		{
			const uint8 ViewMask = HasDynamicMeshElementsMasks[PrimitiveIndex];

			if (ViewMask != 0)
			{
				// Don't cull a single eye when drawing a stereo pair
				const uint8 ViewMaskFinal = (bIsInstancedStereo) ? ViewMask | 0x3 : ViewMask;

				FPrimitiveSceneInfo* PrimitiveSceneInfo = InScene->Primitives[PrimitiveIndex];
				Collector.SetPrimitive(PrimitiveSceneInfo->Proxy, PrimitiveSceneInfo->DefaultDynamicHitProxyId);

				SetDynamicMeshElementViewCustomData(InViews, HasViewCustomDataMasks, PrimitiveSceneInfo);

				// Mark DynamicMeshEndIndices start.
				if (PrimitiveIndex > 0)
				{
					for (int32 ViewIndex = 0; ViewIndex < ViewCount; ViewIndex++)
					{
						InViews[ViewIndex].DynamicMeshEndIndices[PrimitiveIndex - 1] = Collector.GetMeshBatchCount(ViewIndex);
					}
				}

				PrimitiveSceneInfo->Proxy->GetDynamicMeshElements(InViewFamily.Views, InViewFamily, ViewMaskFinal, Collector);

				// Mark DynamicMeshEndIndices end.
				for (int32 ViewIndex = 0; ViewIndex < ViewCount; ViewIndex++)
				{
					InViews[ViewIndex].DynamicMeshEndIndices[PrimitiveIndex] = Collector.GetMeshBatchCount(ViewIndex);
				}

				// Compute DynamicMeshElementsMeshPassRelevance for this primitive.
				for (int32 ViewIndex = 0; ViewIndex < ViewCount; ViewIndex++)
				{
					if (ViewMaskFinal & (1 << ViewIndex))
					{
						FViewInfo& View = InViews[ViewIndex];
						const bool bAddLightmapDensityCommands = View.Family->EngineShowFlags.LightMapDensity && AllowDebugViewmodes();
						const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveIndex];

						const int32 LastNumDynamicMeshElements = View.DynamicMeshElementsPassRelevance.Num();
						View.DynamicMeshElementsPassRelevance.SetNum(View.DynamicMeshElements.Num());

						for (int32 ElementIndex = LastNumDynamicMeshElements; ElementIndex < View.DynamicMeshElements.Num(); ++ElementIndex)
						{
							const FMeshBatchAndRelevance& MeshBatch = View.DynamicMeshElements[ElementIndex];
							FMeshPassMask& PassRelevance = View.DynamicMeshElementsPassRelevance[ElementIndex];

							ComputeDynamicMeshRelevance(ShadingPath, bAddLightmapDensityCommands, ViewRelevance, MeshBatch, View, PassRelevance);
						}
					}
				}
			}
		}
	}

	if (GIsEditor)
	{
		Collector.ClearViewMeshArrays();

		for (int32 ViewIndex = 0; ViewIndex < ViewCount; ViewIndex++)
		{
			Collector.AddViewMeshArrays(
				&InViews[ViewIndex], 
				&InViews[ViewIndex].DynamicEditorMeshElements, 
				&InViews[ViewIndex].EditorSimpleElementCollector, 
				&InViews[ViewIndex].DynamicPrimitiveShaderData, 
				InViewFamily.GetFeatureLevel(),
				&DynamicIndexBuffer,
				&DynamicVertexBuffer,
				&DynamicReadBuffer);
		}

		for (int32 PrimitiveIndex = 0; PrimitiveIndex < NumPrimitives; ++PrimitiveIndex)
		{
			const uint8 ViewMask = HasDynamicEditorMeshElementsMasks[PrimitiveIndex];

			if (ViewMask != 0)
			{
				FPrimitiveSceneInfo* PrimitiveSceneInfo = InScene->Primitives[PrimitiveIndex];
				Collector.SetPrimitive(PrimitiveSceneInfo->Proxy, PrimitiveSceneInfo->DefaultDynamicHitProxyId);

				SetDynamicMeshElementViewCustomData(InViews, HasViewCustomDataMasks, PrimitiveSceneInfo);

				PrimitiveSceneInfo->Proxy->GetDynamicMeshElements(InViewFamily.Views, InViewFamily, ViewMask, Collector);
			}
		}
	}
	MeshCollector.ProcessTasks();
}

/**
 * Helper for InitViews to detect large camera movement, in both angle and position.
 */
static bool IsLargeCameraMovement(FSceneView& View, const FMatrix& PrevViewMatrix, const FVector& PrevViewOrigin, float CameraRotationThreshold, float CameraTranslationThreshold)
{
	float RotationThreshold = FMath::Cos(FMath::DegreesToRadians(CameraRotationThreshold));
	float ViewRightAngle = View.ViewMatrices.GetViewMatrix().GetColumn(0) | PrevViewMatrix.GetColumn(0);
	float ViewUpAngle = View.ViewMatrices.GetViewMatrix().GetColumn(1) | PrevViewMatrix.GetColumn(1);
	float ViewDirectionAngle = View.ViewMatrices.GetViewMatrix().GetColumn(2) | PrevViewMatrix.GetColumn(2);

	FVector Distance = FVector(View.ViewMatrices.GetViewOrigin()) - PrevViewOrigin;
	return 
		ViewRightAngle < RotationThreshold ||
		ViewUpAngle < RotationThreshold ||
		ViewDirectionAngle < RotationThreshold ||
		Distance.SizeSquared() > CameraTranslationThreshold * CameraTranslationThreshold;
}

float Halton( int32 Index, int32 Base )
{
	float Result = 0.0f;
	float InvBase = 1.0f / Base;
	float Fraction = InvBase;
	while( Index > 0 )
	{
		Result += ( Index % Base ) * Fraction;
		Index /= Base;
		Fraction *= InvBase;
	}
	return Result;
}

void FSceneRenderer::PreVisibilityFrameSetup(FRHICommandListImmediate& RHICmdList)
{
	// Notify the RHI we are beginning to render a scene.
	RHICmdList.BeginScene();

	{
		static auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DoLazyStaticMeshUpdate"));
		const bool DoLazyStaticMeshUpdate = (CVar->GetInt() && !GIsEditor);

		if (DoLazyStaticMeshUpdate)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_PreVisibilityFrameSetup_EvictionForLazyStaticMeshUpdate);
			static int32 RollingRemoveIndex = 0;
			static int32 RollingPassShrinkIndex = 0;
			if (RollingRemoveIndex >= Scene->Primitives.Num())
			{
				RollingRemoveIndex = 0;
				RollingPassShrinkIndex++;
				if (RollingPassShrinkIndex >= ARRAY_COUNT(Scene->CachedDrawLists))
				{
					RollingPassShrinkIndex = 0;
				}
				// Periodically shrink the SparseArray containing cached mesh draw commands which we are causing to be regenerated with UpdateStaticMeshes
				Scene->CachedDrawLists[RollingPassShrinkIndex].MeshDrawCommands.Shrink();
			}
			const int32 NumRemovedPerFrame = 10;
			for (int32 NumRemoved = 0; NumRemoved < NumRemovedPerFrame && RollingRemoveIndex < Scene->Primitives.Num(); NumRemoved++, RollingRemoveIndex++)
			{
				Scene->Primitives[RollingRemoveIndex]->UpdateStaticMeshes(RHICmdList, false);
			}
		}
	}

	// Notify the FX system that the scene is about to perform visibility checks.
	if (Scene->FXSystem && !Views[0].bIsPlanarReflection)
	{
		Scene->FXSystem->PreInitViews();
	}

	// Draw lines to lights affecting this mesh if its selected.
	if (ViewFamily.EngineShowFlags.LightInfluences)
	{
		for (TArray<FPrimitiveSceneInfo*>::TConstIterator It(Scene->Primitives); It; ++It)
		{
			const FPrimitiveSceneInfo* PrimitiveSceneInfo = *It;
			if (PrimitiveSceneInfo->Proxy->IsSelected())
			{
				FLightPrimitiveInteraction *LightList = PrimitiveSceneInfo->LightList;
				while (LightList)
				{
					const FLightSceneInfo* LightSceneInfo = LightList->GetLight();

					bool bDynamic = true;
					bool bRelevant = false;
					bool bLightMapped = true;
					bool bShadowMapped = false;
					PrimitiveSceneInfo->Proxy->GetLightRelevance(LightSceneInfo->Proxy, bDynamic, bRelevant, bLightMapped, bShadowMapped);

					if (bRelevant)
					{
						// Draw blue for light-mapped lights and orange for dynamic lights
						const FColor LineColor = bLightMapped ? FColor(0,140,255) : FColor(255,140,0);
						for (int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
						{
							FViewInfo& View = Views[ViewIndex];
							FViewElementPDI LightInfluencesPDI(&View,nullptr,&View.DynamicPrimitiveShaderData);
							LightInfluencesPDI.DrawLine(PrimitiveSceneInfo->Proxy->GetBounds().Origin, LightSceneInfo->Proxy->GetLightToWorld().GetOrigin(), LineColor, SDPG_World);
						}
					}
					LightList = LightList->GetNextLight();
				}
			}
		}
	}

	// Setup motion blur parameters (also check for camera movement thresholds)
	for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		FSceneViewState* ViewState = View.ViewState;

		check(View.VerifyMembersChecks());

		// Once per render increment the occlusion frame counter.
		if (ViewState)
		{
			ViewState->OcclusionFrameCounter++;
		}

		// HighResScreenshot should get best results so we don't do the occlusion optimization based on the former frame
		extern bool GIsHighResScreenshot;
		const bool bIsHitTesting = ViewFamily.EngineShowFlags.HitProxies;
		if (GIsHighResScreenshot || !DoOcclusionQueries(FeatureLevel) || bIsHitTesting)
		{
			View.bDisableQuerySubmissions = true;
			View.bIgnoreExistingQueries = true;
		}
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		// set up the screen area for occlusion
		float NumPossiblePixels = SceneContext.UseDownsizedOcclusionQueries() && IsValidRef(SceneContext.GetSmallDepthSurface()) ? 
			(float)View.ViewRect.Width() / SceneContext.GetSmallColorDepthDownsampleFactor() * (float)View.ViewRect.Height() / SceneContext.GetSmallColorDepthDownsampleFactor() :
			View.ViewRect.Width() * View.ViewRect.Height();
		View.OneOverNumPossiblePixels = NumPossiblePixels > 0.0 ? 1.0f / NumPossiblePixels : 0.0f;

		// Still need no jitter to be set for temporal feedback on SSR (it is enabled even when temporal AA is off).
		check(View.TemporalJitterPixels.X == 0.0f);
		check(View.TemporalJitterPixels.Y == 0.0f);
		
		// Cache the projection matrix b		
		// Cache the projection matrix before AA is applied
		View.ViewMatrices.SaveProjectionNoAAMatrix();

		if (ViewState)
		{
			check(View.bViewStateIsReadOnly);
			View.bViewStateIsReadOnly = ViewFamily.bWorldIsPaused || ViewFamily.EngineShowFlags.HitProxies;

			ViewState->SetupDistanceFieldTemporalOffset(ViewFamily);
		}

		if( View.AntiAliasingMethod == AAM_TemporalAA && ViewState )
		{
			// Subpixel jitter for temporal AA
			int32 TemporalAASamples = CVarTemporalAASamples.GetValueOnRenderThread();
		
			if( TemporalAASamples > 1 && View.bAllowTemporalJitter )
			{
				float SampleX, SampleY;

				if (Scene->GetFeatureLevel() < ERHIFeatureLevel::SM4)
				{
					// Only support 2 samples for mobile temporal AA.
					TemporalAASamples = 2;
				}

				if( TemporalAASamples == 2 )
				{
					#if 0
						// 2xMSAA
						// Pattern docs: http://msdn.microsoft.com/en-us/library/windows/desktop/ff476218(v=vs.85).aspx
						//   N.
						//   .S
						float SamplesX[] = { -4.0f/16.0f, 4.0/16.0f };
						float SamplesY[] = { -4.0f/16.0f, 4.0/16.0f };
					#else
						// This pattern is only used for mobile.
						// Shift to reduce blur.
						float SamplesX[] = { -8.0f/16.0f, 0.0/16.0f };
						float SamplesY[] = { /* - */ 0.0f/16.0f, 8.0/16.0f };
					#endif
					ViewState->OnFrameRenderingSetup(ARRAY_COUNT(SamplesX), ViewFamily);
					uint32 Index = ViewState->GetCurrentTemporalAASampleIndex();
					SampleX = SamplesX[ Index ];
					SampleY = SamplesY[ Index ];
				}
				else if( TemporalAASamples == 3 )
				{
					// 3xMSAA
					//   A..
					//   ..B
					//   .C.
					// Rolling circle pattern (A,B,C).
					float SamplesX[] = { -2.0f/3.0f,  2.0/3.0f,  0.0/3.0f };
					float SamplesY[] = { -2.0f/3.0f,  0.0/3.0f,  2.0/3.0f };
					ViewState->OnFrameRenderingSetup(ARRAY_COUNT(SamplesX), ViewFamily);
					uint32 Index = ViewState->GetCurrentTemporalAASampleIndex();
					SampleX = SamplesX[ Index ];
					SampleY = SamplesY[ Index ];
				}
				else if( TemporalAASamples == 4 )
				{
					// 4xMSAA
					// Pattern docs: http://msdn.microsoft.com/en-us/library/windows/desktop/ff476218(v=vs.85).aspx
					//   .N..
					//   ...E
					//   W...
					//   ..S.
					// Rolling circle pattern (N,E,S,W).
					float SamplesX[] = { -2.0f/16.0f,  6.0/16.0f, 2.0/16.0f, -6.0/16.0f };
					float SamplesY[] = { -6.0f/16.0f, -2.0/16.0f, 6.0/16.0f,  2.0/16.0f };
					ViewState->OnFrameRenderingSetup(ARRAY_COUNT(SamplesX), ViewFamily);
					uint32 Index = ViewState->GetCurrentTemporalAASampleIndex();
					SampleX = SamplesX[ Index ];
					SampleY = SamplesY[ Index ];
				}
				else if( TemporalAASamples == 5 )
				{
					// Compressed 4 sample pattern on same vertical and horizontal line (less temporal flicker).
					// Compressed 1/2 works better than correct 2/3 (reduced temporal flicker).
					//   . N .
					//   W . E
					//   . S .
					// Rolling circle pattern (N,E,S,W).
					float SamplesX[] = {  0.0f/2.0f,  1.0/2.0f,  0.0/2.0f, -1.0/2.0f };
					float SamplesY[] = { -1.0f/2.0f,  0.0/2.0f,  1.0/2.0f,  0.0/2.0f };
					ViewState->OnFrameRenderingSetup(ARRAY_COUNT(SamplesX), ViewFamily);
					uint32 Index = ViewState->GetCurrentTemporalAASampleIndex();
					SampleX = SamplesX[ Index ];
					SampleY = SamplesY[ Index ];
				}
				else if (View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale)
				{
					// When doing TAA upsample with screen percentage < 100%, we need extra temporal samples to have a
					// constant temporal sample density for final output pixels to avoid output pixel aligned converging issues.
					float EffectivePrimaryResolutionFraction = float(View.ViewRect.Width()) / float(View.GetSecondaryViewRectSize().X);
					int32 EffectiveTemporalAASamples = float(TemporalAASamples) * FMath::Max(1.f, 1.f / (EffectivePrimaryResolutionFraction * EffectivePrimaryResolutionFraction));

					ViewState->OnFrameRenderingSetup(EffectiveTemporalAASamples, ViewFamily);
					uint32 TemporalSampleIndex = ViewState->GetCurrentTemporalAASampleIndex();

					// Uniformly distribute temporal jittering in [-.5; .5], because there is no longer any alignement of input and output pixels.
					SampleX = Halton(TemporalSampleIndex + 1, 2) - 0.5f;
					SampleY = Halton(TemporalSampleIndex + 1, 3) - 0.5f;

					View.MaterialTextureMipBias = -(FMath::Max(-FMath::Log2(EffectivePrimaryResolutionFraction), 0.0f) ) + CVarMinAutomaticViewMipBiasOffset.GetValueOnRenderThread();
					View.MaterialTextureMipBias = FMath::Max(View.MaterialTextureMipBias, CVarMinAutomaticViewMipBias.GetValueOnRenderThread());
				}
				else
				{
					ViewState->OnFrameRenderingSetup(TemporalAASamples, ViewFamily);
					uint32 Index = ViewState->GetCurrentTemporalAASampleIndex();

					float u1 = Halton( Index + 1, 2 );
					float u2 = Halton( Index + 1, 3 );

					// Generates samples in normal distribution
					// exp( x^2 / Sigma^2 )
					
					static auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TemporalAAFilterSize"));
					float FilterSize = CVar->GetFloat();

					// Scale distribution to set non-unit variance
					// Variance = Sigma^2
					float Sigma = 0.47f * FilterSize;

					// Window to [-0.5, 0.5] output
					// Without windowing we could generate samples far away on the infinite tails.
					float OutWindow = 0.5f;
					float InWindow = FMath::Exp( -0.5 * FMath::Square( OutWindow / Sigma ) );
					
					// Box-Muller transform
					float Theta = 2.0f * PI * u2;
					float r = Sigma * FMath::Sqrt( -2.0f * FMath::Loge( (1.0f - u1) * InWindow + u1 ) );
					
					SampleX = r * FMath::Cos( Theta );
					SampleY = r * FMath::Sin( Theta );
				}

				View.TemporalJitterPixels.X = SampleX;
				View.TemporalJitterPixels.Y = SampleY;

				View.ViewMatrices.HackAddTemporalAAProjectionJitter(FVector2D(SampleX * 2.0f / View.ViewRect.Width(), SampleY * -2.0f / View.ViewRect.Height()));
			}
		}
		else if(ViewState && !View.bViewStateIsReadOnly)
		{
			// no TemporalAA
			ViewState->OnFrameRenderingSetup(1, ViewFamily);

			ViewState->PrevFrameViewInfo.TemporalAAHistory.SafeRelease();
		}

		// Setup a new FPreviousViewInfo from current frame infos.
		FPreviousViewInfo NewPrevViewInfo;
		{
			NewPrevViewInfo.ViewMatrices = View.ViewMatrices;
		}

		if ( ViewState )
		{
			// update previous frame matrices in case world origin was rebased on this frame
			if (!View.OriginOffsetThisFrame.IsZero())
			{
				ViewState->PrevFrameViewInfo.ViewMatrices.ApplyWorldOffset(View.OriginOffsetThisFrame);
			}

			// determine if we are initializing or we should reset the persistent state
			const float DeltaTime = View.Family->CurrentRealTime - ViewState->LastRenderTime;
			const bool bFirstFrameOrTimeWasReset = DeltaTime < -0.0001f || ViewState->LastRenderTime < 0.0001f;
			const bool bIsLargeCameraMovement = IsLargeCameraMovement(
				View,
				ViewState->PrevFrameViewInfo.ViewMatrices.GetViewMatrix(),
				ViewState->PrevFrameViewInfo.ViewMatrices.GetViewOrigin(),
				45.0f, 10000.0f);
			const bool bResetCamera = (bFirstFrameOrTimeWasReset || View.bCameraCut || bIsLargeCameraMovement);
			
#if RHI_RAYTRACING
			// Note: 0.18 deg is the minimum angle for avoiding numerical precision issue (which would cause constant invalidation)
			const bool bIsThereALargeMomvement= IsLargeCameraMovement(
				View, ViewState->PrevFrameViewInfo.ViewMatrices.GetViewMatrix(),
				ViewState->PrevFrameViewInfo.ViewMatrices.GetViewOrigin(),
				0.18f /*degree*/, 0.1f /*cm*/);
			const bool bIsProjMatrixDifferent = View.ViewMatrices.GetProjectionNoAAMatrix() != View.ViewState->PrevFrameViewInfo.ViewMatrices.GetProjectionNoAAMatrix();
			const bool bInvalidatePathTracer = View.RayTracingRenderMode == ERayTracingRenderMode::PathTracing &&
			(
				bResetCamera ||
				Scene->bPathTracingNeedsInvalidation ||
				View.ViewRect != ViewState->PathTracingRect ||
				bIsProjMatrixDifferent ||
				bIsThereALargeMomvement
			);

			if (bInvalidatePathTracer)
			{
				ViewState->PathTracingIrradianceRT.SafeRelease();
				ViewState->PathTracingSampleCountRT.SafeRelease();
				ViewState->VarianceMipTreeDimensions = FIntVector(0);
				ViewState->PathTracingRect = View.ViewRect;
				ViewState->TotalRayCount = 0;
				Scene->bPathTracingNeedsInvalidation = false;
			}
#endif // RHI_RAYTRACING

			if (bResetCamera)
			{
				View.PrevViewInfo = NewPrevViewInfo;

				// PT: If the motion blur shader is the last shader in the post-processing chain then it is the one that is
				//     adjusting for the viewport offset.  So it is always required and we can't just disable the work the
				//     shader does.  The correct fix would be to disable the effect when we don't need it and to properly mark
				//     the uber-postprocessing effect as the last effect in the chain.

				View.bPrevTransformsReset = true;
			}
			else
			{
				View.PrevViewInfo = ViewState->PrevFrameViewInfo;
			}

			// Replace previous view info of the view state with this frame, clearing out references over render target.
			if (!View.bViewStateIsReadOnly)
			{
				ViewState->PrevFrameViewInfo = NewPrevViewInfo;
			}

			// detect conditions where we should reset occlusion queries
			if (bFirstFrameOrTimeWasReset || 
				ViewState->LastRenderTime + GEngine->PrimitiveProbablyVisibleTime < View.Family->CurrentRealTime ||
				View.bCameraCut ||
				IsLargeCameraMovement(
					View, 
				    ViewState->PrevViewMatrixForOcclusionQuery, 
				    ViewState->PrevViewOriginForOcclusionQuery, 
				    GEngine->CameraRotationThreshold, GEngine->CameraTranslationThreshold))
			{
				View.bIgnoreExistingQueries = true;
				View.bDisableDistanceBasedFadeTransitions = true;
			}

			// Turn on/off round-robin occlusion querying in the ViewState
			static const auto CVarRROCC = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.RoundRobinOcclusion"));
			const bool bEnableRoundRobin = CVarRROCC ? (CVarRROCC->GetValueOnAnyThread() != false) : false;
			if (bEnableRoundRobin != ViewState->IsRoundRobinEnabled())
			{
				ViewState->UpdateRoundRobin(bEnableRoundRobin);
				View.bIgnoreExistingQueries = true;
			}

			ViewState->PrevViewMatrixForOcclusionQuery = View.ViewMatrices.GetViewMatrix();
			ViewState->PrevViewOriginForOcclusionQuery = View.ViewMatrices.GetViewOrigin();
				
			// store old view matrix and detect conditions where we should reset motion blur 
#if RHI_RAYTRACING
			{
				if (bResetCamera || IsLargeCameraMovement(View, ViewState->PrevFrameViewInfo.ViewMatrices.GetViewMatrix(), ViewState->PrevFrameViewInfo.ViewMatrices.GetViewOrigin(), 0.1f, 0.1f))
				{
					ViewState->RayTracingNumIterations = 1;
				}
				else
				{
					ViewState->RayTracingNumIterations++;
				}
			}
#endif // RHI_RAYTRACING

			// we don't use DeltaTime as it can be 0 (in editor) and is computed by subtracting floats (loses precision over time)
			// Clamp DeltaWorldTime to reasonable values for the purposes of motion blur, things like TimeDilation can make it very small
			if (View.bViewStateIsReadOnly)
			{
				const bool bEnableTimeScale = !ViewState->bSequencerIsPaused;
				const float FixedBlurTimeScale = 2.0f;// 1 / (30 * 1 / 60)

				ViewState->MotionBlurTimeScale = bEnableTimeScale ? (1.0f / (FMath::Max(View.Family->DeltaWorldTime, .00833f) * 30.0f)) : FixedBlurTimeScale;
			}

			ViewState->PrevFrameNumber = ViewState->PendingPrevFrameNumber;
			ViewState->PendingPrevFrameNumber = View.Family->FrameNumber;

			// This finishes the update of view state
			ViewState->UpdateLastRenderTime(*View.Family);

			ViewState->UpdateTemporalLODTransition(View);
		}
		else
		{
			// Without a viewstate, we just assume that camera has not moved.
			View.PrevViewInfo = NewPrevViewInfo;
		}
	}

	// Setup global dither fade in and fade out uniform buffers.
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		FDitherUniformShaderParameters DitherUniformShaderParameters;
		DitherUniformShaderParameters.LODFactor = View.GetTemporalLODTransition();
		View.DitherFadeOutUniformBuffer = FDitherUniformBufferRef::CreateUniformBufferImmediate(DitherUniformShaderParameters, UniformBuffer_SingleFrame);

		DitherUniformShaderParameters.LODFactor = View.GetTemporalLODTransition() - 1.0f;
		View.DitherFadeInUniformBuffer = FDitherUniformBufferRef::CreateUniformBufferImmediate(DitherUniformShaderParameters, UniformBuffer_SingleFrame);
	}
}

static TAutoConsoleVariable<int32> CVarAlsoUseSphereForFrustumCull(
	TEXT("r.AlsoUseSphereForFrustumCull"),
	0,  
	TEXT("Performance tweak. If > 0, then use a sphere cull before and in addition to a box for frustum culling."),
	ECVF_RenderThreadSafe
	);


void UpdateReflectionSceneData(FScene* Scene)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UpdateReflectionSceneData)

	FReflectionEnvironmentSceneData& ReflectionSceneData = Scene->ReflectionSceneData;

	ReflectionSceneData.SortedCaptures.Reset(ReflectionSceneData.RegisteredReflectionCaptures.Num());
	ReflectionSceneData.NumBoxCaptures = 0;
	ReflectionSceneData.NumSphereCaptures = 0;

	const int32 MaxCubemaps = ReflectionSceneData.CubemapArray.GetMaxCubemaps();

	// Pack visible reflection captures into the uniform buffer, each with an index to its cubemap array entry.
	// GPUScene primitive data stores closest reflection capture as index into this buffer, so this index which must be invalidate every time OutSortData contents change.
	for (int32 ReflectionProxyIndex = 0; ReflectionProxyIndex < ReflectionSceneData.RegisteredReflectionCaptures.Num() && ReflectionSceneData.SortedCaptures.Num() < GMaxNumReflectionCaptures; ReflectionProxyIndex++)
	{
		FReflectionCaptureProxy* CurrentCapture = ReflectionSceneData.RegisteredReflectionCaptures[ReflectionProxyIndex];

		FReflectionCaptureSortData NewSortEntry;

		NewSortEntry.CubemapIndex = -1;
		NewSortEntry.CaptureOffsetAndAverageBrightness = FVector4(CurrentCapture->CaptureOffset, 1.0f);
		NewSortEntry.CaptureProxy = CurrentCapture;
		if (Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM5)
		{
			FCaptureComponentSceneState* ComponentStatePtr = ReflectionSceneData.AllocatedReflectionCaptureState.Find(CurrentCapture->Component);
			if (!ComponentStatePtr)
			{
				// Skip reflection captures without built data to upload
				continue;
			}

			NewSortEntry.CubemapIndex = ComponentStatePtr->CubemapIndex;
			check(NewSortEntry.CubemapIndex < MaxCubemaps || NewSortEntry.CubemapIndex == 0);
			NewSortEntry.CaptureOffsetAndAverageBrightness.W = ComponentStatePtr->AverageBrightness;
		}

		NewSortEntry.Guid = CurrentCapture->Guid;
		NewSortEntry.PositionAndRadius = FVector4(CurrentCapture->Position, CurrentCapture->InfluenceRadius);
		float ShapeTypeValue = (float)CurrentCapture->Shape;
		NewSortEntry.CaptureProperties = FVector4(CurrentCapture->Brightness, NewSortEntry.CubemapIndex, ShapeTypeValue, 0);

		if (CurrentCapture->Shape == EReflectionCaptureShape::Plane)
		{
			//planes count as boxes in the compute shader.
			++ReflectionSceneData.NumBoxCaptures;
			NewSortEntry.BoxTransform = FMatrix(
				FPlane(CurrentCapture->ReflectionPlane),
				FPlane(CurrentCapture->ReflectionXAxisAndYScale),
				FPlane(0, 0, 0, 0),
				FPlane(0, 0, 0, 0));

			NewSortEntry.BoxScales = FVector4(0);
		}
		else if (CurrentCapture->Shape == EReflectionCaptureShape::Sphere)
		{
			++ReflectionSceneData.NumSphereCaptures;
		}
		else
		{
			++ReflectionSceneData.NumBoxCaptures;
			NewSortEntry.BoxTransform = CurrentCapture->BoxTransform;
			NewSortEntry.BoxScales = FVector4(CurrentCapture->BoxScales, CurrentCapture->BoxTransitionDistance);
		}

		ReflectionSceneData.SortedCaptures.Add(NewSortEntry);
	}

	ReflectionSceneData.SortedCaptures.Sort();

	for (int32 CaptureIndex = 0; CaptureIndex < ReflectionSceneData.SortedCaptures.Num(); CaptureIndex++)
	{
		ReflectionSceneData.SortedCaptures[CaptureIndex].CaptureProxy->SortedCaptureIndex = CaptureIndex;
	}


	// If SortedCaptures change, then in case of forward renderer all scene primitives need to be updated, as they 
	// store index into sorted reflection capture uniform buffer for the forward renderer.
	if (IsForwardShadingEnabled(Scene->GetShaderPlatform()) && ReflectionSceneData.AllocatedReflectionCaptureStateHasChanged)
	{
		const int32 NumPrimitives = Scene->Primitives.Num();
		for (int32 PrimitiveIndex = 0; PrimitiveIndex < NumPrimitives; ++PrimitiveIndex)
		{
			Scene->Primitives[PrimitiveIndex]->SetNeedsUniformBufferUpdate(true);
		}

		Scene->GPUScene.bUpdateAllPrimitives = true;

		ReflectionSceneData.AllocatedReflectionCaptureStateHasChanged = false;
	}


	// Mark all primitives for reflection proxy update
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_MarkAllPrimitivesForReflectionProxyUpdate);

		if (Scene->ReflectionSceneData.bRegisteredReflectionCapturesHasChanged)
		{
			// Mobile needs to re-cache all mesh commands when scene capture data has changed
			const bool bNeedsStaticMeshUpdate = Scene->GetShadingPath() == EShadingPath::Mobile;
			
			// Mark all primitives as needing an update
			// Note: Only visible primitives will actually update their reflection proxy
			for (int32 PrimitiveIndex = 0; PrimitiveIndex < Scene->Primitives.Num(); PrimitiveIndex++)
			{
				FPrimitiveSceneInfo* Primitive = Scene->Primitives[PrimitiveIndex];
				Primitive->CachedReflectionCaptureProxy = nullptr;
				Primitive->CachedPlanarReflectionProxy = nullptr;
				FMemory::Memzero(Primitive->CachedReflectionCaptureProxies);
				Primitive->bNeedsCachedReflectionCaptureUpdate = true;
				
				if (bNeedsStaticMeshUpdate)
				{
					Primitive->CacheReflectionCaptures();
					Primitive->BeginDeferredUpdateStaticMeshes();
				}
			}

			Scene->ReflectionSceneData.bRegisteredReflectionCapturesHasChanged = false;
		}
	}
}

void FSceneRenderer::ComputeViewVisibility(FRHICommandListImmediate& RHICmdList, FExclusiveDepthStencil::Type BasePassDepthStencilAccess, FViewVisibleCommandsPerView& ViewCommandsPerView, 
	FGlobalDynamicIndexBuffer& DynamicIndexBuffer, FGlobalDynamicVertexBuffer& DynamicVertexBuffer, FGlobalDynamicReadBuffer& DynamicReadBuffer)
{
	SCOPE_CYCLE_COUNTER(STAT_ViewVisibilityTime);
	SCOPED_NAMED_EVENT(FSceneRenderer_ComputeViewVisibility, FColor::Magenta);

	STAT(int32 NumProcessedPrimitives = 0);
	STAT(int32 NumCulledPrimitives = 0);
	STAT(int32 NumOccludedPrimitives = 0);

	// Allocate the visible light info.
	if (Scene->Lights.GetMaxIndex() > 0)
	{
		VisibleLightInfos.AddZeroed(Scene->Lights.GetMaxIndex());
	}

	int32 NumPrimitives = Scene->Primitives.Num();
	float CurrentRealTime = ViewFamily.CurrentRealTime;

	FPrimitiveViewMasks HasDynamicMeshElementsMasks;
	HasDynamicMeshElementsMasks.AddZeroed(NumPrimitives);

	FPrimitiveViewMasks HasViewCustomDataMasks;
	HasViewCustomDataMasks.AddZeroed(NumPrimitives);

	FPrimitiveViewMasks HasDynamicEditorMeshElementsMasks;

	if (GIsEditor)
	{
		HasDynamicEditorMeshElementsMasks.AddZeroed(NumPrimitives);
	}

	const bool bIsInstancedStereo = (Views.Num() > 0) ? (Views[0].IsInstancedStereoPass() || Views[0].bIsMobileMultiViewEnabled) : false;
	UpdateReflectionSceneData(Scene);

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ViewVisibilityTime_ConditionalUpdateStaticMeshesWithoutVisibilityCheck);

		Scene->ConditionalMarkStaticMeshElementsForUpdate();

		for (TSet<FPrimitiveSceneInfo*>::TIterator It(Scene->PrimitivesNeedingStaticMeshUpdateWithoutVisibilityCheck); It; ++It)
		{
			FPrimitiveSceneInfo* Primitive = *It;
			Primitive->ConditionalUpdateStaticMeshes(RHICmdList);
		}
		Scene->PrimitivesNeedingStaticMeshUpdateWithoutVisibilityCheck.Reset();
	}

	uint8 ViewBit = 0x1;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex, ViewBit <<= 1)
	{
		STAT(NumProcessedPrimitives += NumPrimitives);

		FViewInfo& View = Views[ViewIndex];
		FViewCommands& ViewCommands = ViewCommandsPerView[ViewIndex];
		FSceneViewState* ViewState = (FSceneViewState*)View.State;

		// Allocate the view's visibility maps.
		View.PrimitiveVisibilityMap.Init(false,Scene->Primitives.Num());
		// we don't initialized as we overwrite the whole array (in GatherDynamicMeshElements)
		View.DynamicMeshEndIndices.SetNumUninitialized(Scene->Primitives.Num());
		View.PrimitiveDefinitelyUnoccludedMap.Init(false,Scene->Primitives.Num());
		View.PotentiallyFadingPrimitiveMap.Init(false,Scene->Primitives.Num());
		View.PrimitiveFadeUniformBuffers.AddZeroed(Scene->Primitives.Num());
		View.PrimitiveFadeUniformBufferMap.Init(false, Scene->Primitives.Num());
		View.StaticMeshVisibilityMap.Init(false,Scene->StaticMeshes.GetMaxIndex());
		View.StaticMeshFadeOutDitheredLODMap.Init(false,Scene->StaticMeshes.GetMaxIndex());
		View.StaticMeshFadeInDitheredLODMap.Init(false,Scene->StaticMeshes.GetMaxIndex());
		View.StaticMeshBatchVisibility.AddZeroed(Scene->StaticMeshBatchVisibility.GetMaxIndex());
		View.PrimitivesLODMask.Init(FLODMask(), Scene->Primitives.Num());

		View.PrimitivesCustomData.Init(nullptr, Scene->Primitives.Num());
		View.PrimitivesWithCustomData.Reserve(Scene->Primitives.Num());

		// We must reserve to prevent realloc otherwise it will cause memory leak if we Execute In Parallel
		const bool WillExecuteInParallel = FApp::ShouldUseThreadingForPerformance() && CVarParallelInitViews.GetValueOnRenderThread() > 0;
		View.PrimitiveCustomDataMemStack.Reserve(WillExecuteInParallel ? FMath::CeilToInt(((float)View.PrimitiveVisibilityMap.Num() / (float)FRelevancePrimSet<int32>::MaxInputPrims)) + 1 : 1);

		View.AllocateCustomDataMemStack();

		View.VisibleLightInfos.Empty(Scene->Lights.GetMaxIndex());

		// The dirty list allocation must take into account the max possible size because when GILCUpdatePrimTaskEnabled is true,
		// the indirect lighting cache will be update on by threaded job, which can not do reallocs on the buffer (since it uses the SceneRenderingAllocator).
		View.DirtyIndirectLightingCacheBufferPrimitives.Reserve(Scene->Primitives.Num());

		for(int32 LightIndex = 0;LightIndex < Scene->Lights.GetMaxIndex();LightIndex++)
		{
			if( LightIndex+2 < Scene->Lights.GetMaxIndex() )
			{
				if (LightIndex > 2)
				{
					FLUSH_CACHE_LINE(&View.VisibleLightInfos(LightIndex-2));
				}
				// @todo optimization These prefetches cause asserts since LightIndex > View.VisibleLightInfos.Num() - 1
				//FPlatformMisc::Prefetch(&View.VisibleLightInfos[LightIndex+2]);
				//FPlatformMisc::Prefetch(&View.VisibleLightInfos[LightIndex+1]);
			}
			new(View.VisibleLightInfos) FVisibleLightViewInfo();
		}

		View.PrimitiveViewRelevanceMap.Empty(Scene->Primitives.Num());
		View.PrimitiveViewRelevanceMap.AddZeroed(Scene->Primitives.Num());

		// If this is the visibility-parent of other views, reset its ParentPrimitives list.
		const bool bIsParent = ViewState && ViewState->IsViewParent();
		if ( bIsParent )
		{
			// PVS-Studio does not understand the validation of ViewState above, so we're disabling
			// its warning that ViewState may be null:
			ViewState->ParentPrimitives.Empty(); //-V595
		}

		if (ViewState)
		{	
			SCOPE_CYCLE_COUNTER(STAT_DecompressPrecomputedOcclusion);
			View.PrecomputedVisibilityData = ViewState->GetPrecomputedVisibilityData(View, Scene);
		}
		else
		{
			View.PrecomputedVisibilityData = NULL;
		}

		if (View.PrecomputedVisibilityData)
		{
			bUsedPrecomputedVisibility = true;
		}

		bool bNeedsFrustumCulling = true;

		// Development builds sometimes override frustum culling, e.g. dependent views in the editor.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if( ViewState )
		{
#if WITH_EDITOR
			// For visibility child views, check if the primitive was visible in the parent view.
			const FSceneViewState* const ViewParent = (FSceneViewState*)ViewState->GetViewParent();
			if(ViewParent)
			{
				bNeedsFrustumCulling = false;
				for (FSceneBitArray::FIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
				{
					if (ViewParent->ParentPrimitives.Contains(Scene->PrimitiveComponentIds[BitIt.GetIndex()]))
					{
						BitIt.GetValue() = true;
					}
				}
			}
#endif
			// For views with frozen visibility, check if the primitive is in the frozen visibility set.
			if(ViewState->bIsFrozen)
			{
				bNeedsFrustumCulling = false;
				for (FSceneBitArray::FIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
				{
					if (ViewState->FrozenPrimitives.Contains(Scene->PrimitiveComponentIds[BitIt.GetIndex()]))
					{
						BitIt.GetValue() = true;
					}
				}
			}
		}
#endif

		// Most views use standard frustum culling.
		if (bNeedsFrustumCulling)
		{
			// Update HLOD transition/visibility states to allow use during distance culling
			FLODSceneTree& HLODTree = Scene->SceneLODHierarchy;
			if (HLODTree.IsActive())
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_ViewVisibilityTime_HLODUpdate);
				HLODTree.UpdateVisibilityStates(View);
			}
			else
			{
				HLODTree.ClearVisibilityState(View);
			}

			int32 NumCulledPrimitivesForView;
			if (View.CustomVisibilityQuery && View.CustomVisibilityQuery->Prepare())
			{
				if (CVarAlsoUseSphereForFrustumCull.GetValueOnRenderThread())
				{
					NumCulledPrimitivesForView = FrustumCull<true, true>(Scene, View);
				}
				else
				{
					NumCulledPrimitivesForView = FrustumCull<true, false>(Scene, View);
				}
			}
			else
			{
				if (CVarAlsoUseSphereForFrustumCull.GetValueOnRenderThread())
				{
					NumCulledPrimitivesForView = FrustumCull<false, true>(Scene, View);
				}
				else
				{
					NumCulledPrimitivesForView = FrustumCull<false, false>(Scene, View);
				}
			}
			STAT(NumCulledPrimitives += NumCulledPrimitivesForView);
			UpdatePrimitiveFading(Scene, View);			
		}

		// If any primitives are explicitly hidden, remove them now.
		if (View.HiddenPrimitives.Num())
		{
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
			{
				if (View.HiddenPrimitives.Contains(Scene->PrimitiveComponentIds[BitIt.GetIndex()]))
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
				}
			}
		}

		// If the view has any show only primitives, hide everything else
		if (View.ShowOnlyPrimitives.IsSet())
		{
			View.bHasNoVisiblePrimitive = View.ShowOnlyPrimitives->Num() == 0;
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
			{
				if (!View.ShowOnlyPrimitives->Contains(Scene->PrimitiveComponentIds[BitIt.GetIndex()]))
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
				}
			}
		}

		if (View.bStaticSceneOnly)
		{
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
			{
				// Reflection captures should only capture objects that won't move, since reflection captures won't update at runtime
				if (!Scene->Primitives[BitIt.GetIndex()]->Proxy->HasStaticLighting())
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
				}
			}
		}

		// Cull small objects in wireframe in ortho views
		// This is important for performance in the editor because wireframe disables any kind of occlusion culling
		if (View.Family->EngineShowFlags.Wireframe)
		{
			float ScreenSizeScale = FMath::Max(View.ViewMatrices.GetProjectionMatrix().M[0][0] * View.ViewRect.Width(), View.ViewMatrices.GetProjectionMatrix().M[1][1] * View.ViewRect.Height());
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
			{
				if (ScreenSizeScale * Scene->PrimitiveBounds[BitIt.GetIndex()].BoxSphereBounds.SphereRadius <= GWireframeCullThreshold)
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
				}
			}
		}

		// Occlusion cull for all primitives in the view frustum, but not in wireframe.
		if (!View.Family->EngineShowFlags.Wireframe)
		{
			int32 NumOccludedPrimitivesInView = OcclusionCull(RHICmdList, Scene, View, DynamicVertexBuffer);
			STAT(NumOccludedPrimitives += NumOccludedPrimitivesInView);
		}

		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_ViewVisibilityTime_ConditionalUpdateStaticMeshes);

			for (TSet<FPrimitiveSceneInfo*>::TIterator It(Scene->PrimitivesNeedingStaticMeshUpdate); It; ++It)
			{
				FPrimitiveSceneInfo* Primitive = *It;

				if (View.PrimitiveVisibilityMap[Primitive->GetIndex()])
				{
					Primitive->ConditionalUpdateStaticMeshes(RHICmdList);
				}
			}
		}

		// ISR views can't compute relevance until all views are frustum culled
		if (!bIsInstancedStereo)
		{
			SCOPE_CYCLE_COUNTER(STAT_ViewRelevance);
			ComputeAndMarkRelevanceForViewParallel(RHICmdList, Scene, View, ViewCommands, ViewBit, HasDynamicMeshElementsMasks, HasDynamicEditorMeshElementsMasks, HasViewCustomDataMasks);
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Store the primitive for parent occlusion rendering.
		if (FPlatformProperties::SupportsWindowedMode() && ViewState && ViewState->IsViewParent())
		{
			for (FSceneDualSetBitIterator BitIt(View.PrimitiveVisibilityMap, View.PrimitiveDefinitelyUnoccludedMap); BitIt; ++BitIt)
			{
				ViewState->ParentPrimitives.Add(Scene->PrimitiveComponentIds[BitIt.GetIndex()]);
			}
		}
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// if we are freezing the scene, then remember the primitives that are rendered.
		if (ViewState && ViewState->bIsFreezing)
		{
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
			{
				ViewState->FrozenPrimitives.Add(Scene->PrimitiveComponentIds[BitIt.GetIndex()]);
			}
		}
#endif

		// TODO: right now decals visibility computed right before rendering them, ideally it should be done in InitViews and this flag should be replaced with list of visible decals  
	    // Currently used to disable stencil operations in forward base pass when scene has no any decals
		View.bSceneHasDecals = (Scene->Decals.Num() > 0) || (GForceSceneHasDecals != 0);
	}

	if ((Views.Num() > 1) && bIsInstancedStereo)
	{
		// Ensure primitives from the right-eye view are visible in the left-eye (instanced) view
		FSceneBitArray& LeftView = Views[0].PrimitiveVisibilityMap;
		const FSceneBitArray& RightView = Views[1].PrimitiveVisibilityMap;

		check(LeftView.Num() == RightView.Num())

		const uint32 NumWords = FMath::DivideAndRoundUp(LeftView.Num(), NumBitsPerDWORD);
		uint32* const LeftData = LeftView.GetData();
		const uint32* const RightData = RightView.GetData();

		for (uint32 Index = 0; Index < NumWords; ++Index)
		{
			LeftData[Index] |= RightData[Index];
		}
	}
	
	ViewBit = 0x1;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		FViewInfo& View = Views[ViewIndex];
		FViewCommands& ViewCommands = ViewCommandsPerView[ViewIndex];
		
		if (bIsInstancedStereo)
		{
			SCOPE_CYCLE_COUNTER(STAT_ViewRelevance);
			ComputeAndMarkRelevanceForViewParallel(RHICmdList, Scene, View, ViewCommands, ViewBit, HasDynamicMeshElementsMasks, HasDynamicEditorMeshElementsMasks, HasViewCustomDataMasks);
		}
		ViewBit <<= 1;
	}

	// Gather FMeshBatches from scene proxies
	GatherDynamicMeshElements(Views, Scene, ViewFamily, DynamicIndexBuffer, DynamicVertexBuffer, DynamicReadBuffer,
		HasDynamicMeshElementsMasks, HasDynamicEditorMeshElementsMasks, HasViewCustomDataMasks, MeshCollector);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		if (!View.ShouldRenderView())
		{
			continue;
		}

		FViewCommands& ViewCommands = ViewCommandsPerView[ViewIndex];
		SetupMeshPass(View, BasePassDepthStencilAccess, ViewCommands);
	}

	INC_DWORD_STAT_BY(STAT_ProcessedPrimitives,NumProcessedPrimitives);
	INC_DWORD_STAT_BY(STAT_CulledPrimitives,NumCulledPrimitives);
	INC_DWORD_STAT_BY(STAT_OccludedPrimitives,NumOccludedPrimitives);
}

void FSceneRenderer::PostVisibilityFrameSetup(FILCUpdatePrimTaskData& OutILCTaskData)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PostVisibilityFrameSetup);

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_PostVisibilityFrameSetup_Sort);
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{		
			FViewInfo& View = Views[ViewIndex];

			View.MeshDecalBatches.Sort();

			if (View.State)
			{
				((FSceneViewState*)View.State)->TrimHistoryRenderTargets(Scene);
			}
		}
	}

	bool bCheckLightShafts = false;
	if (Scene->GetFeatureLevel() <= ERHIFeatureLevel::ES3_1)
	{
		// Clear the mobile light shaft data.
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{		
			FViewInfo& View = Views[ViewIndex];
			View.bLightShaftUse = false;
			View.LightShaftCenter.X = 0.0f;
			View.LightShaftCenter.Y = 0.0f;
			View.LightShaftColorMask = FLinearColor(0.0f,0.0f,0.0f);
			View.LightShaftColorApply = FLinearColor(0.0f,0.0f,0.0f);
		}
		
		extern int32 GLightShafts;
		bCheckLightShafts = ViewFamily.EngineShowFlags.LightShafts && GLightShafts;
	}

	if (ViewFamily.EngineShowFlags.HitProxies == 0 && Scene->PrecomputedLightVolumes.Num() > 0
		&& GILCUpdatePrimTaskEnabled && FPlatformProcess::SupportsMultithreading())
	{
		Scene->IndirectLightingCache.StartUpdateCachePrimitivesTask(Scene, *this, true, OutILCTaskData);
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_PostVisibilityFrameSetup_Light_Visibility);
	// determine visibility of each light
	for(TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights);LightIt;++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		// view frustum cull lights in each view
		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{		
			const FLightSceneProxy* Proxy = LightSceneInfo->Proxy;
			FViewInfo& View = Views[ViewIndex];
			FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[LightIt.GetIndex()];
			// dir lights are always visible, and point/spot only if in the frustum
			if( Proxy->GetLightType() == LightType_Point ||
				Proxy->GetLightType() == LightType_Spot ||
				Proxy->GetLightType() == LightType_Rect )
			{
				FSphere const& BoundingSphere = Proxy->GetBoundingSphere();
				if (View.ViewFrustum.IntersectSphere(BoundingSphere.Center, BoundingSphere.W))
				{
					if (View.IsPerspectiveProjection())
					{
						FSphere Bounds = Proxy->GetBoundingSphere();
						float DistanceSquared = (Bounds.Center - View.ViewMatrices.GetViewOrigin()).SizeSquared();
						float MaxDistSquared = Proxy->GetMaxDrawDistance() * Proxy->GetMaxDrawDistance() * GLightMaxDrawDistanceScale * GLightMaxDrawDistanceScale;
						const bool bDrawLight = (FMath::Square(FMath::Min(0.0002f, GMinScreenRadiusForLights / Bounds.W) * View.LODDistanceFactor) * DistanceSquared < 1.0f)
													&& (MaxDistSquared == 0 || DistanceSquared < MaxDistSquared);
							
						VisibleLightViewInfo.bInViewFrustum = bDrawLight;
					}
					else
					{
						VisibleLightViewInfo.bInViewFrustum = true;
					}
				}
			}
			else
			{
				VisibleLightViewInfo.bInViewFrustum = true;

				static const auto MobileMSAAVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileMSAA"));
				bool bNotMobileMSAA = !(MobileMSAAVar ? MobileMSAAVar->GetValueOnRenderThread() > 1 : false);

				// Setup single sun-shaft from direction lights for mobile.
				if(bCheckLightShafts && LightSceneInfo->bEnableLightShaftBloom)
				{
					// Find directional light for sun shafts.
					// Tweaked values from UE3 implementation.
					extern const float PointLightFadeDistanceIncrease;
					extern const float PointLightRadiusFadeFactor;

					const FVector WorldSpaceBlurOrigin = LightSceneInfo->Proxy->GetPosition();
					// Transform into post projection space
					FVector4 ProjectedBlurOrigin = View.WorldToScreen(WorldSpaceBlurOrigin);

					const float DistanceToBlurOrigin = (View.ViewMatrices.GetViewOrigin() - WorldSpaceBlurOrigin).Size() + PointLightFadeDistanceIncrease;

					// Don't render if the light's origin is behind the view
					if(ProjectedBlurOrigin.W >= 0.0f
						// Don't render point lights that have completely faded out
						&& (LightSceneInfo->Proxy->GetLightType() == LightType_Directional 
							|| DistanceToBlurOrigin < LightSceneInfo->Proxy->GetRadius() * PointLightRadiusFadeFactor))
					{
						View.bLightShaftUse = bNotMobileMSAA;
						View.LightShaftCenter.X = ProjectedBlurOrigin.X / ProjectedBlurOrigin.W;
						View.LightShaftCenter.Y = ProjectedBlurOrigin.Y / ProjectedBlurOrigin.W;
						// TODO: Might want to hookup different colors for these.
						View.LightShaftColorMask = LightSceneInfo->BloomTint;
						View.LightShaftColorApply = LightSceneInfo->BloomTint;

						// Apply bloom scale
						View.LightShaftColorMask  *= FLinearColor(LightSceneInfo->BloomScale, LightSceneInfo->BloomScale, LightSceneInfo->BloomScale, 1.0f);
						View.LightShaftColorApply *= FLinearColor(LightSceneInfo->BloomScale, LightSceneInfo->BloomScale, LightSceneInfo->BloomScale, 1.0f);
					}
				}
			}

			// Draw shapes for reflection captures
			if( View.bIsReflectionCapture 
				&& VisibleLightViewInfo.bInViewFrustum
				&& Proxy->HasStaticLighting() 
				&& Proxy->GetLightType() != LightType_Directional )
			{
				FVector Origin = Proxy->GetOrigin();
				FVector ToLight = Origin - View.ViewMatrices.GetViewOrigin();
				float DistanceSqr = ToLight | ToLight;
				float Radius = Proxy->GetRadius();

				if( DistanceSqr < Radius * Radius )
				{
					FLightShaderParameters LightParameters;

					Proxy->GetLightShaderParameters(LightParameters);

					// Force to be at least 0.75 pixels
					float CubemapSize = (float)IConsoleManager::Get().FindTConsoleVariableDataInt( TEXT("r.ReflectionCaptureResolution") )->GetValueOnAnyThread();
					float Distance = FMath::Sqrt( DistanceSqr );
					float MinRadius = Distance * 0.75f / CubemapSize;
					LightParameters.SourceRadius = FMath::Max( MinRadius, LightParameters.SourceRadius );

					// Snap to cubemap pixel center to reduce aliasing
					FVector Scale = ToLight.GetAbs();
					int32 MaxComponent = Scale.X > Scale.Y ? ( Scale.X > Scale.Z ? 0 : 2 ) : ( Scale.Y > Scale.Z ? 1 : 2 );
					for( int32 k = 1; k < 3; k++ )
					{
						float Projected = ToLight[ (MaxComponent + k) % 3 ] / Scale[ MaxComponent ];
						float Quantized = ( FMath::RoundToFloat( Projected * (0.5f * CubemapSize) - 0.5f ) + 0.5f ) / (0.5f * CubemapSize);
						ToLight[ (MaxComponent + k) % 3 ] = Quantized * Scale[ MaxComponent ];
					}
					Origin = ToLight + View.ViewMatrices.GetViewOrigin();
				
					FLinearColor Color( LightParameters.Color.X, LightParameters.Color.Y, LightParameters.Color.Z, LightParameters.FalloffExponent );
					if( !Proxy->IsRectLight() )
					{
						const float SphereArea = (4.0f * PI) * FMath::Square( LightParameters.SourceRadius );
						const float CylinderArea = (2.0f * PI) * LightParameters.SourceRadius * LightParameters.SourceLength;
						const float SurfaceArea = SphereArea + CylinderArea;
						Color *= 4.0f / SurfaceArea;
					}

					if( Proxy->IsInverseSquared() )
					{
						float LightRadiusMask = FMath::Square( 1.0f - FMath::Square( DistanceSqr * FMath::Square( LightParameters.InvRadius ) ) );
						Color.A = LightRadiusMask;
					}
					else
					{
						// Remove inverse square falloff
						Color *= DistanceSqr + 1.0f;

						// Apply falloff
						Color.A = FMath::Pow( 1.0f - DistanceSqr * FMath::Square(LightParameters.InvRadius ), LightParameters.FalloffExponent );
					}
					
					// Spot falloff
					FVector L = ToLight.GetSafeNormal();
					Color.A *= FMath::Square( FMath::Clamp( ( (L | LightParameters.Direction) - LightParameters.SpotAngles.X ) * LightParameters.SpotAngles.Y, 0.0f, 1.0f ) );

					Color.A *= LightParameters.SpecularScale;

					// Rect is one sided
					if( Proxy->IsRectLight() && (L | LightParameters.Direction) < 0.0f )
						continue;
				
					FMaterialRenderProxy* const ColoredMeshInstance = new(FMemStack::Get()) FColoredMaterialRenderProxy( GEngine->DebugMeshMaterial->GetRenderProxy(), Color );

					FMatrix LightToWorld = Proxy->GetLightToWorld();
					LightToWorld.RemoveScaling();

					FViewElementPDI LightPDI( &View, NULL, &View.DynamicPrimitiveShaderData );

					if( Proxy->IsRectLight() )
					{
						DrawBox( &LightPDI, LightToWorld, FVector( 0.0f, LightParameters.SourceRadius, LightParameters.SourceLength ), ColoredMeshInstance, SDPG_World );
					}
					else if( LightParameters.SourceLength > 0.0f )
					{
						DrawSphere( &LightPDI, Origin + 0.5f * LightParameters.SourceLength * LightToWorld.GetUnitAxis( EAxis::Z ), FRotator::ZeroRotator, LightParameters.SourceRadius * FVector::OneVector, 36, 24, ColoredMeshInstance, SDPG_World );
						DrawSphere( &LightPDI, Origin - 0.5f * LightParameters.SourceLength * LightToWorld.GetUnitAxis( EAxis::Z ), FRotator::ZeroRotator, LightParameters.SourceRadius * FVector::OneVector, 36, 24, ColoredMeshInstance, SDPG_World );
						DrawCylinder( &LightPDI, Origin, LightToWorld.GetUnitAxis( EAxis::X ), LightToWorld.GetUnitAxis( EAxis::Y ), LightToWorld.GetUnitAxis( EAxis::Z ), LightParameters.SourceRadius, 0.5f * LightParameters.SourceLength, 36, ColoredMeshInstance, SDPG_World );
					}
					else
					{
						DrawSphere( &LightPDI, Origin, FRotator::ZeroRotator, LightParameters.SourceRadius * FVector::OneVector, 36, 24, ColoredMeshInstance, SDPG_World );
					}
				}
			}
		}
	}
	}
	{

		QUICK_SCOPE_CYCLE_COUNTER(STAT_PostVisibilityFrameSetup_InitFogConstants);
		InitFogConstants();
	}
}

uint32 GetShadowQuality();

/** 
* Performs once per frame setup prior to visibility determination.
*/
void FDeferredShadingSceneRenderer::PreVisibilityFrameSetup(FRHICommandListImmediate& RHICmdList)
{
	// Possible stencil dither optimization approach
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		View.bAllowStencilDither = bDitheredLODTransitionsUseStencil;
	}

	FSceneRenderer::PreVisibilityFrameSetup(RHICmdList);
}

/**
 * Initialize scene's views.
 * Check visibility, build visible mesh commands, etc.
 */
bool FDeferredShadingSceneRenderer::InitViews(FRHICommandListImmediate& RHICmdList, FExclusiveDepthStencil::Type BasePassDepthStencilAccess, struct FILCUpdatePrimTaskData& ILCTaskData, FGraphEventArray& UpdateViewCustomDataEvents)
{
	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_InitViews, FColor::Emerald);
	SCOPE_CYCLE_COUNTER(STAT_InitViewsTime);
	check(RHICmdList.IsOutsideRenderPass());

	PreVisibilityFrameSetup(RHICmdList);

	RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);

	FViewVisibleCommandsPerView ViewCommandsPerView;
	ViewCommandsPerView.SetNum(Views.Num());

	ComputeViewVisibility(RHICmdList, BasePassDepthStencilAccess, ViewCommandsPerView, DynamicIndexBufferForInitViews, DynamicVertexBufferForInitViews, DynamicReadBufferForInitViews);

	RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);

	// This has to happen before Scene->IndirectLightingCache.UpdateCache, since primitives in View.IndirectShadowPrimitives need ILC updates
	CreateIndirectCapsuleShadows();
	RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);

	PostVisibilityFrameSetup(ILCTaskData);
	RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);

	FVector AverageViewPosition(0);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{		
		FViewInfo& View = Views[ViewIndex];
		AverageViewPosition += View.ViewMatrices.GetViewOrigin() / Views.Num();
	}

	bool bDoInitViewAftersPrepass = !!GDoInitViewsLightingAfterPrepass;

	if (!bDoInitViewAftersPrepass)
	{
		InitViewsPossiblyAfterPrepass(RHICmdList, ILCTaskData, UpdateViewCustomDataEvents);
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_InitViews_InitRHIResources);
		// initialize per-view uniform buffer.
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];

			if (View.ViewState)
			{
				if (!View.ViewState->ForwardLightingResources)
				{
					View.ViewState->ForwardLightingResources.Reset(new FForwardLightingViewResources());
				}

				View.ForwardLightingResources = View.ViewState->ForwardLightingResources.Get();
			}
			else
			{
				View.ForwardLightingResourcesStorage.Reset(new FForwardLightingViewResources());
				View.ForwardLightingResources = View.ForwardLightingResourcesStorage.Get();
			}

#if RHI_RAYTRACING
			View.IESLightProfileResource = View.ViewState ? &View.ViewState->IESLightProfileResources : nullptr;
#endif
			// Set the pre-exposure before initializing the constant buffers.
			if (View.ViewState)
			{
				View.ViewState->UpdatePreExposure(View);
			}

			// Initialize the view's RHI resources.
			View.InitRHIResources();
		}
	}

	SetupVolumetricFog();

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_InitViews_OnStartRender);
		OnStartRender(RHICmdList);
	}

	return bDoInitViewAftersPrepass;
}

void FDeferredShadingSceneRenderer::SetupSceneReflectionCaptureBuffer(FRHICommandListImmediate& RHICmdList)
{
	FReflectionCaptureShaderData SamplePositionsBuffer;

	const TArray<FReflectionCaptureSortData>& SortedCaptures = Scene->ReflectionSceneData.SortedCaptures;

	for (int32 CaptureIndex = 0; CaptureIndex < SortedCaptures.Num(); CaptureIndex++)
	{
		SamplePositionsBuffer.PositionAndRadius[CaptureIndex] = SortedCaptures[CaptureIndex].PositionAndRadius;
		SamplePositionsBuffer.CaptureProperties[CaptureIndex] = SortedCaptures[CaptureIndex].CaptureProperties;
		SamplePositionsBuffer.CaptureOffsetAndAverageBrightness[CaptureIndex] = SortedCaptures[CaptureIndex].CaptureOffsetAndAverageBrightness;
		SamplePositionsBuffer.BoxTransform[CaptureIndex] = SortedCaptures[CaptureIndex].BoxTransform;
		SamplePositionsBuffer.BoxScales[CaptureIndex] = SortedCaptures[CaptureIndex].BoxScales;
	}

	Scene->UniformBuffers.ReflectionCaptureUniformBuffer.UpdateUniformBufferImmediate(SamplePositionsBuffer);


	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		FViewInfo& View = Views[ViewIndex];

		View.ReflectionCaptureUniformBuffer = Scene->UniformBuffers.ReflectionCaptureUniformBuffer;
		View.NumBoxReflectionCaptures = 0;
		View.NumSphereReflectionCaptures = 0;
		View.FurthestReflectionCaptureDistance = 0.0f;

		if (View.Family->EngineShowFlags.ReflectionEnvironment)
		{
			View.NumBoxReflectionCaptures = Scene->ReflectionSceneData.NumBoxCaptures;
			View.NumSphereReflectionCaptures = Scene->ReflectionSceneData.NumSphereCaptures;

			for (int32 CaptureIndex = 0; CaptureIndex < SortedCaptures.Num(); CaptureIndex++)
			{
				const FSphere BoundingSphere(SortedCaptures[CaptureIndex].PositionAndRadius, SortedCaptures[CaptureIndex].PositionAndRadius.W);

				const float Distance = View.ViewMatrices.GetViewMatrix().TransformPosition(BoundingSphere.Center).Z + BoundingSphere.W;

				View.FurthestReflectionCaptureDistance = FMath::Max(View.FurthestReflectionCaptureDistance, Distance);
			}
		}
	}
}

void FDeferredShadingSceneRenderer::InitViewsPossiblyAfterPrepass(FRHICommandListImmediate& RHICmdList, struct FILCUpdatePrimTaskData& ILCTaskData, FGraphEventArray& UpdateViewCustomDataEvents)
{
	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_InitViewsPossiblyAfterPrepass, FColor::Emerald);
	SCOPE_CYCLE_COUNTER(STAT_InitViewsPossiblyAfterPrepass);

	if (ViewFamily.EngineShowFlags.DynamicShadows 
		&& !IsSimpleForwardShadingEnabled(ShaderPlatform)
		&& !ViewFamily.EngineShowFlags.HitProxies)
	{
		// Setup dynamic shadows.
		InitDynamicShadows(RHICmdList, DynamicIndexBufferForInitShadows, DynamicVertexBufferForInitShadows, DynamicReadBufferForInitShadows);

		RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
	}

	// If parallel ILC update is disabled, then process it in place.
	if (ViewFamily.EngineShowFlags.HitProxies == 0
		&& Scene->PrecomputedLightVolumes.Num() > 0
		&& !(GILCUpdatePrimTaskEnabled && FPlatformProcess::SupportsMultithreading()))
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_PostVisibilityFrameSetup_IndirectLightingCache_Update);
		check(!ILCTaskData.TaskRef.IsValid());
		Scene->IndirectLightingCache.UpdateCache(Scene, *this, true);
	}

	// If we kicked off ILC update via task, wait and finalize.
	if (ILCTaskData.TaskRef.IsValid())
	{
		Scene->IndirectLightingCache.FinalizeCacheUpdates(Scene, *this, ILCTaskData);
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_InitViews_UpdatePrimitiveIndirectLightingCacheBuffers);
		// Now that the indirect lighting cache is updated, we can update the primitive precomputed lighting buffers.
		UpdatePrimitiveIndirectLightingCacheBuffers();
	}

	UpdateTranslucencyTimersAndSeparateTranslucencyBufferSize(RHICmdList);

	SetupSceneReflectionCaptureBuffer(RHICmdList);
}

/*------------------------------------------------------------------------------
	FLODSceneTree Implementation
------------------------------------------------------------------------------*/
void FLODSceneTree::AddChildNode(const FPrimitiveComponentId ParentId, FPrimitiveSceneInfo* ChildSceneInfo)
{
	if (ParentId.IsValid() && ChildSceneInfo)
	{
		FLODSceneNode* Parent = SceneNodes.Find(ParentId);

		if (!Parent)
		{
			Parent = &SceneNodes.Add(ParentId, FLODSceneNode());

			// Scene info can be added later depending on order of adding to the scene
			// but at least add componentId, that way when parent is added, it will add its info properly
			int32 ParentIndex = Scene->PrimitiveComponentIds.Find(ParentId);
			if (Scene->Primitives.IsValidIndex(ParentIndex))
			{
				Parent->SceneInfo = Scene->Primitives[ParentIndex];				
			}
		}

		Parent->AddChild(ChildSceneInfo);
	}
}

void FLODSceneTree::RemoveChildNode(const FPrimitiveComponentId ParentId, FPrimitiveSceneInfo* ChildSceneInfo)
{
	if (ParentId.IsValid() && ChildSceneInfo)
	{
		if (FLODSceneNode* Parent = SceneNodes.Find(ParentId))
		{
			Parent->RemoveChild(ChildSceneInfo);

			// Delete from scene if no children remain
			if (Parent->ChildrenSceneInfos.Num() == 0)
			{
				SceneNodes.Remove(ParentId);
			}
		}
	}
}

void FLODSceneTree::UpdateNodeSceneInfo(FPrimitiveComponentId NodeId, FPrimitiveSceneInfo* SceneInfo)
{
	if (FLODSceneNode* Node = SceneNodes.Find(NodeId))
	{
		Node->SceneInfo = SceneInfo;
	}
}

void FLODSceneTree::ClearVisibilityState(FViewInfo& View)
{
	if (FSceneViewState* ViewState = (FSceneViewState*)View.State)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Skip update logic when frozen
		if (ViewState->bIsFrozen)
		{
			return;
		}
#endif
		FHLODVisibilityState& HLODState = ViewState->HLODVisibilityState;

		if(HLODState.IsValidPrimitiveIndex(0))
		{
			HLODState.PrimitiveFadingLODMap.Empty(0);
			HLODState.PrimitiveFadingOutLODMap.Empty(0);
			HLODState.ForcedVisiblePrimitiveMap.Empty(0);
			HLODState.ForcedHiddenPrimitiveMap.Empty(0);
		}

		TMap<FPrimitiveComponentId, FHLODSceneNodeVisibilityState>& VisibilityStates = ViewState->HLODSceneNodeVisibilityStates;

		if(VisibilityStates.Num() > 0)
		{
			VisibilityStates.Empty(0);
		}
	}
}

void FLODSceneTree::UpdateVisibilityStates(FViewInfo& View)
{
	if (FSceneViewState* ViewState = (FSceneViewState*)View.State)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Skip update logic when frozen
		if (ViewState->bIsFrozen)
		{
			return;
		}
#endif

		// Per-frame initialization
		FHLODVisibilityState& HLODState = ViewState->HLODVisibilityState;
		TMap<FPrimitiveComponentId, FHLODSceneNodeVisibilityState>& VisibilityStates = ViewState->HLODSceneNodeVisibilityStates;

		HLODState.PrimitiveFadingLODMap.Init(false, Scene->Primitives.Num());
		HLODState.PrimitiveFadingOutLODMap.Init(false, Scene->Primitives.Num());
		HLODState.ForcedVisiblePrimitiveMap.Init(false, Scene->Primitives.Num());
		HLODState.ForcedHiddenPrimitiveMap.Init(false, Scene->Primitives.Num());
		TArray<FPrimitiveViewRelevance, SceneRenderingAllocator>& RelevanceMap = View.PrimitiveViewRelevanceMap;

		if (HLODState.PrimitiveFadingLODMap.Num() != Scene->Primitives.Num())
		{
			checkf(0, TEXT("HLOD update incorrectly allocated primitive maps"));
			return;
		}		

		int32 UpdateCount = ++HLODState.UpdateCount;

		// Update persistent state on temporal dither sync frames
		const FTemporalLODState& LODState = ViewState->GetTemporalLODState();
		bool bSyncFrame = false;
		
		if (HLODState.TemporalLODSyncTime != LODState.TemporalLODTime[0])
		{
			HLODState.TemporalLODSyncTime = LODState.TemporalLODTime[0];
			bSyncFrame = true;

			// Only update our scaling on sync frames else we might end up changing transition direction mid-fade	
			const FCachedSystemScalabilityCVars& ScalabilityCVars = GetCachedScalabilityCVars();
			if (ScalabilityCVars.FieldOfViewAffectsHLOD)
			{
				HLODState.FOVDistanceScaleSq = ScalabilityCVars.CalculateFieldOfViewDistanceScale(View.DesiredFOV);
				HLODState.FOVDistanceScaleSq *= HLODState.FOVDistanceScaleSq;
			}
			else
			{
				HLODState.FOVDistanceScaleSq = 1.f;
			}
		}

		for (auto Iter = SceneNodes.CreateIterator(); Iter; ++Iter)
		{
			FLODSceneNode& Node = Iter.Value();
			FPrimitiveSceneInfo* SceneInfo = Node.SceneInfo;

			if (!SceneInfo || !SceneInfo->PrimitiveComponentId.IsValid() || !SceneInfo->IsIndexValid())
			{
				continue;
			}

			FHLODSceneNodeVisibilityState& NodeVisibility = VisibilityStates.FindOrAdd(SceneInfo->PrimitiveComponentId);
			const TArray<FStaticMeshBatchRelevance>& NodeMeshRelevances = SceneInfo->StaticMeshRelevances;

			// Ignore already updated nodes, or those that we can't work with
			if (NodeVisibility.UpdateCount == UpdateCount || !NodeMeshRelevances.IsValidIndex(0))
			{
				continue;
			}

			const int32 NodeIndex = SceneInfo->GetIndex();	

			if (!Scene->PrimitiveBounds.IsValidIndex(NodeIndex))
			{
				checkf(0, TEXT("A HLOD Node's PrimitiveSceneInfo PackedIndex was out of Scene.Primitive bounds!"));
				continue;
			}

			FPrimitiveBounds& Bounds = Scene->PrimitiveBounds[NodeIndex];
			const bool bForcedIntoView = FMath::IsNearlyZero(Bounds.MinDrawDistanceSq);

			// Update visibility states of this node and owned children
			const float DistanceSquared = Bounds.BoxSphereBounds.ComputeSquaredDistanceFromBoxToPoint(View.ViewMatrices.GetViewOrigin());
			const bool bIsInDrawRange = DistanceSquared >= Bounds.MinDrawDistanceSq * HLODState.FOVDistanceScaleSq;

			const bool bWasFadingPreUpdate = !!NodeVisibility.bIsFading;
			const bool bIsDitheredTransition = NodeMeshRelevances[0].bDitheredLODTransition;

			if (bIsDitheredTransition && !bForcedIntoView)
			{		
				// Update fading state with syncs
				if (bSyncFrame)
				{
					// Fade when HLODs change threshold
					const bool bChangedRange = bIsInDrawRange != !!NodeVisibility.bWasVisible;

					if (NodeVisibility.bIsFading)
					{
						NodeVisibility.bIsFading = false;
					}
					else if (bChangedRange)
					{
						NodeVisibility.bIsFading = true;
					}

					NodeVisibility.bWasVisible = NodeVisibility.bIsVisible;
					NodeVisibility.bIsVisible = bIsInDrawRange;
				}
			}
			else
			{
				// Instant transitions without dithering
				NodeVisibility.bWasVisible = NodeVisibility.bIsVisible;
				NodeVisibility.bIsVisible = bIsInDrawRange || bForcedIntoView;
				NodeVisibility.bIsFading = false;
			}

			// Flush cached lighting data when changing visible contents
			if (NodeVisibility.bIsVisible != NodeVisibility.bWasVisible || bWasFadingPreUpdate || NodeVisibility.bIsFading)
			{
				FLightPrimitiveInteraction* NodeLightList = SceneInfo->LightList;
				while (NodeLightList)
				{
					NodeLightList->FlushCachedShadowMapData();
					NodeLightList = NodeLightList->GetNextLight();
				}
			}

			// Force fully disabled view relevance so shadows don't attempt to recompute
			if (!NodeVisibility.bIsVisible)
			{
				if (RelevanceMap.IsValidIndex(NodeIndex))
				{
					FPrimitiveViewRelevance& ViewRelevance = RelevanceMap[NodeIndex];
					FMemory::Memzero(&ViewRelevance, sizeof(FPrimitiveViewRelevance));
					ViewRelevance.bInitializedThisFrame = true;
				}
				else
				{
					checkf(0, TEXT("A HLOD Node's PrimitiveSceneInfo PackedIndex was out of View.Relevancy bounds!"));
				}
			}

			// NOTE: We update our children last as HideNodeChildren can add new visibility
			// states, potentially invalidating our cached reference above, NodeVisibility
			if (NodeVisibility.bIsFading)
			{
				// Fade until state back in sync
				HLODState.PrimitiveFadingLODMap[NodeIndex] = true;
				HLODState.PrimitiveFadingOutLODMap[NodeIndex] = !NodeVisibility.bIsVisible;
				HLODState.ForcedVisiblePrimitiveMap[NodeIndex] = true;
				ApplyNodeFadingToChildren(ViewState, Node, NodeVisibility, true, !!NodeVisibility.bIsVisible);
			}
			else if (NodeVisibility.bIsVisible)
			{
				// If stable and visible, override hierarchy visibility
				HLODState.ForcedVisiblePrimitiveMap[NodeIndex] = true;
				HideNodeChildren(ViewState, Node);
			}
			else
			{
				// Not visible and waiting for a transition to fade, keep HLOD hidden
				HLODState.ForcedHiddenPrimitiveMap[NodeIndex] = true;
			}
		}
	}	
}

void FLODSceneTree::ApplyNodeFadingToChildren(FSceneViewState* ViewState, FLODSceneNode& Node, FHLODSceneNodeVisibilityState& NodeVisibility, const bool bIsFading, const bool bIsFadingOut)
{
	checkSlow(ViewState);
	if (Node.SceneInfo)
	{
		FHLODVisibilityState& HLODState = ViewState->HLODVisibilityState;
		NodeVisibility.UpdateCount = HLODState.UpdateCount;

		// Force visibility during fades
		for (const auto Child : Node.ChildrenSceneInfos)
		{
			if (!Child || !Child->PrimitiveComponentId.IsValid() || !Child->IsIndexValid())
			{
				continue;
			}

			const int32 ChildIndex = Child->GetIndex();

			if (!HLODState.PrimitiveFadingLODMap.IsValidIndex(ChildIndex))
			{
				checkf(0, TEXT("A HLOD Child's PrimitiveSceneInfo PackedIndex was out of FadingMap's bounds!"));
				continue;
			}
		
			HLODState.PrimitiveFadingLODMap[ChildIndex] = bIsFading;
			HLODState.PrimitiveFadingOutLODMap[ChildIndex] = bIsFadingOut;
			HLODState.ForcedHiddenPrimitiveMap[ChildIndex] = false;

			if (bIsFading)
			{
				HLODState.ForcedVisiblePrimitiveMap[ChildIndex] = true;
			}

			// Fading only occurs at the adjacent hierarchy level, below should be hidden
			if (FLODSceneNode* ChildNode = SceneNodes.Find(Child->PrimitiveComponentId))
			{
				HideNodeChildren(ViewState, *ChildNode);
			}
		}
	}
}

void FLODSceneTree::HideNodeChildren(FSceneViewState* ViewState, FLODSceneNode& Node)
{
	checkSlow(ViewState);
	if (Node.SceneInfo)
	{
		FHLODVisibilityState& HLODState = ViewState->HLODVisibilityState;
		TMap<FPrimitiveComponentId, FHLODSceneNodeVisibilityState>& VisibilityStates = ViewState->HLODSceneNodeVisibilityStates;
		FHLODSceneNodeVisibilityState& NodeVisibility = VisibilityStates.FindOrAdd(Node.SceneInfo->PrimitiveComponentId);

		if (NodeVisibility.UpdateCount != HLODState.UpdateCount)
		{
			NodeVisibility.UpdateCount = HLODState.UpdateCount;

			for (const auto Child : Node.ChildrenSceneInfos)
			{
				if (!Child || !Child->PrimitiveComponentId.IsValid() || !Child->IsIndexValid())
				{
					continue;
				}

				const int32 ChildIndex = Child->GetIndex();

				if (!HLODState.ForcedHiddenPrimitiveMap.IsValidIndex(ChildIndex))
				{
					checkf(0, TEXT("A HLOD Child's PrimitiveSceneInfo PackedIndex was out of ForcedHidden's bounds!"));
					continue;
				}

				HLODState.ForcedHiddenPrimitiveMap[ChildIndex] = true;

				if (FLODSceneNode* ChildNode = SceneNodes.Find(Child->PrimitiveComponentId))
				{
					HideNodeChildren(ViewState, *ChildNode);
				}
			}
		}
	}
}
