// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DeferredShadingRenderer.h: Scene rendering definitions.
=============================================================================*/

#include "SceneViewFamilyBlackboard.h"
#include "SceneRenderTargets.h"
#include "SystemTextures.h"


void SetupSceneViewFamilyBlackboard(
	FRDGBuilder& GraphBuilder,
	FSceneViewFamilyBlackboard* OutBlackboard)
{
	const FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(GraphBuilder.RHICmdList);

	FRDGTextureRef BlackDummy = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
	FRDGTextureRef DefaultNormal8Bit = GraphBuilder.RegisterExternalTexture(GSystemTextures.DefaultNormal8Bit);

	*OutBlackboard = FSceneViewFamilyBlackboard();
	OutBlackboard->SceneDepthBuffer = GraphBuilder.RegisterExternalTexture(SceneContext.SceneDepthZ);
	OutBlackboard->SceneVelocityBuffer = SceneContext.SceneVelocity ? GraphBuilder.RegisterExternalTexture(SceneContext.SceneVelocity) : BlackDummy;
	OutBlackboard->SceneGBufferA = SceneContext.GBufferA ? GraphBuilder.RegisterExternalTexture(SceneContext.GBufferA) : DefaultNormal8Bit;
}
