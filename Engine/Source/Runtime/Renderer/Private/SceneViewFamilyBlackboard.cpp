// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DeferredShadingRenderer.h: Scene rendering definitions.
=============================================================================*/

#include "SceneViewFamilyBlackboard.h"
#include "SceneRendering.h"
#include "SceneRenderTargets.h"
#include "SystemTextures.h"


void SetupSceneViewFamilyBlackboard(
	FRDGBuilder& GraphBuilder,
	FSceneViewFamilyBlackboard* OutBlackboard)
{
	const FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(GraphBuilder.RHICmdList);

	*OutBlackboard = FSceneViewFamilyBlackboard();
	OutBlackboard->SceneDepthBuffer = GraphBuilder.RegisterExternalTexture(SceneContext.SceneDepthZ);
	OutBlackboard->SceneVelocityBuffer = RegisterExternalTextureWithFallback(GraphBuilder, SceneContext.SceneVelocity, GSystemTextures.BlackDummy);
	OutBlackboard->SceneGBufferA = RegisterExternalTextureWithFallback(GraphBuilder, SceneContext.GBufferA, GSystemTextures.DefaultNormal8Bit, TEXT("GBufferA"));
	OutBlackboard->SceneGBufferB = RegisterExternalTextureWithFallback(GraphBuilder, SceneContext.GBufferB, GSystemTextures.BlackDummy, TEXT("GBufferB"));
	OutBlackboard->SceneGBufferC = RegisterExternalTextureWithFallback(GraphBuilder, SceneContext.GBufferC, GSystemTextures.BlackDummy, TEXT("GBufferC"));
	OutBlackboard->SceneGBufferD = RegisterExternalTextureWithFallback(GraphBuilder, SceneContext.GBufferD, GSystemTextures.BlackDummy, TEXT("GBufferD"));
	OutBlackboard->SceneGBufferE = RegisterExternalTextureWithFallback(GraphBuilder, SceneContext.GBufferE, GSystemTextures.BlackDummy, TEXT("GBufferE"));

	if (SceneContext.LightingChannels)
	{
		OutBlackboard->SceneLightingChannels = GraphBuilder.RegisterExternalTexture(SceneContext.LightingChannels, TEXT("LightingChannels"));
		OutBlackboard->bIsSceneLightingChannelsValid = true;
	}
	else
	{
		OutBlackboard->SceneLightingChannels = GraphBuilder.RegisterExternalTexture(GSystemTextures.WhiteDummy, TEXT("LightingChannels"));
		OutBlackboard->bIsSceneLightingChannelsValid = false;
	}
}

FRDGTextureRef GetEyeAdaptationTexture(FRDGBuilder& GraphBuilder, const FViewInfo& View)
{
	if (View.HasValidEyeAdaptation())
	{
		return GraphBuilder.RegisterExternalTexture(View.GetEyeAdaptation(GraphBuilder.RHICmdList), TEXT("ViewEyeAdaptation"));
	}
	else
	{
		return GraphBuilder.RegisterExternalTexture(GSystemTextures.WhiteDummy, TEXT("DefaultViewEyeAdaptation"));
	}
}
