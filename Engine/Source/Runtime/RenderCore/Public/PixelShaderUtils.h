// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PixelShaderUtils.h: Utilities for pixel shaders.
=============================================================================*/

#pragma once

#include "RenderResource.h"
#include "RenderGraphUtils.h"
#include "CommonRenderResources.h"
#include "RHI.h"
#include "RHIStaticStates.h"
#include "PipelineStateCache.h"


/** All utils for pixel shaders. */
struct RENDERCORE_API FPixelShaderUtils
{
	/** Draw a single triangle on the entire viewport. */
	static void DrawFullscreenTriangle(FRHICommandList& RHICmdList, uint32 InstanceCount = 1);

	/** Draw a two triangle on the entire viewport. */
	static void DrawFullscreenQuad(FRHICommandList& RHICmdList, uint32 InstanceCount = 1);


	/** Dispatch a full screen pixel shader to rhi command list with its parameters. */
	template<typename TShaderClass>
	static inline void DrawFullscreenPixelShader(
		FRHICommandList& RHICmdList, 
		const TShaderMap<FGlobalShaderType>* GlobalShaderMap,
		const TShaderClass* PixelShader,
		const typename TShaderClass::FParameters& Parameters,
		const FIntRect& Viewport)
	{
		CA_ASSUME(PixelShader);
		TShaderMapRef<FVisualizeTextureVS> VertexShader(GlobalShaderMap);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		RHICmdList.SetViewport(Viewport.Min.X, Viewport.Min.Y, 0.0f, Viewport.Max.X, Viewport.Max.Y, 1.0f);

		SetShaderParameters(RHICmdList, PixelShader, PixelShader->GetPixelShader(), Parameters);

		DrawFullscreenTriangle(RHICmdList);
	}

	/** Dispatch a pixel shader to render graph builder with its parameters. */
	template<typename TShaderClass>
	static inline void AddFullscreenPass(
		FRDGBuilder& GraphBuilder,
		const TShaderMap<FGlobalShaderType>* GlobalShaderMap,
		FRDGEventName&& PassName,
		const TShaderClass* PixelShader,
		typename TShaderClass::FParameters* Parameters,
		const FIntRect& Viewport)
	{
		CA_ASSUME(PixelShader != nullptr);
		ClearUnusedGraphResources(PixelShader, Parameters);

		GraphBuilder.AddPass(
			Forward<FRDGEventName>(PassName),
			Parameters,
			ERenderGraphPassFlags::None,
			[Parameters, GlobalShaderMap, PixelShader, Viewport](FRHICommandList& RHICmdList)
		{
			FPixelShaderUtils::DrawFullscreenPixelShader(RHICmdList, GlobalShaderMap, PixelShader, *Parameters, Viewport);
		});
	}
};
