// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DecalRenderingShared.h
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "DecalRenderingCommon.h"

class FDeferredDecalProxy;
class FScene;
class FViewInfo;
struct FShaderCompilerEnvironment;

/**
 * Compact decal data for rendering
 */
struct FTransientDecalRenderData
{
	const FMaterialRenderProxy* MaterialProxy;
	const FMaterial* MaterialResource;
	const FDeferredDecalProxy* DecalProxy;
	float FadeAlpha;
	float ConservativeRadius;
	EDecalBlendMode FinalDecalBlendMode;
	bool bHasNormal;

	FTransientDecalRenderData(const FScene& InScene, const FDeferredDecalProxy* InDecalProxy, float InConservativeRadius);
};
	
typedef TArray<FTransientDecalRenderData, SceneRenderingAllocator> FTransientDecalRenderDataList;

/**
 * Shared decal functionality for deferred and forward shading
 */
struct FDecalRendering
{
	static FMatrix ComputeComponentToClipMatrix(const FViewInfo& View, const FMatrix& DecalComponentToWorld);
	static void SetVertexShaderOnly(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FViewInfo& View, const FMatrix& FrustumComponentToClip);
	static bool BuildVisibleDecalList(const FScene& Scene, const FViewInfo& View, EDecalRenderStage DecalRenderStage, FTransientDecalRenderDataList* OutVisibleDecals);
	static void SetShader(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FViewInfo& View, const FTransientDecalRenderData& DecalData, EDecalRenderStage DecalRenderStage, const FMatrix& FrustumComponentToClip);

	// Set common compilation environment parameters for decal shaders (FDeferredDecalPS, FMeshDecalsPS, etc.)
	static void SetDecalCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);
	static void SetEmissiveDBufferDecalCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);
};
