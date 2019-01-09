// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
MobileSeparateTranslucencyPass.cpp - Mobile specific separate translucency pass
=============================================================================*/

#include "MobileSeparateTranslucencyPass.h"
#include "TranslucentRendering.h"
#include "DynamicPrimitiveDrawing.h"

bool IsMobileSeparateTranslucencyActive(const FViewInfo& View)
{
	if (UseMeshDrawCommandPipeline())
	{
		return View.ParallelMeshDrawCommandPasses[EMeshPass::TranslucencyAfterDOF].HasAnyDraw();
	}
	else
	{
		return View.TranslucentPrimSet.SortedPrimsNum.Num(ETranslucencyPass::TPT_TranslucencyAfterDOF) > 0;
	}
}

void FRCSeparateTranslucensyPassES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, SeparateTranslucensyPass);

	const FViewInfo& View = Context.View;

	FSceneRenderTargets& SceneTargets = FSceneRenderTargets::Get(Context.RHICmdList);
	const FSceneRenderTargetItem& DestRenderTarget = GetOutput(ePId_Output0)->RequestSurface(Context);
		
	FRHIRenderPassInfo RPInfo(DestRenderTarget.TargetableTexture, ERenderTargetActions::Load_Store);
	RPInfo.DepthStencilRenderTarget.Action = EDepthStencilTargetActions::LoadDepthStencil_StoreDepthStencil;
	RPInfo.DepthStencilRenderTarget.DepthStencilTarget = SceneTargets.GetSceneDepthSurface();
	RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthRead_StencilRead;
	Context.RHICmdList.BeginRenderPass(RPInfo, TEXT("SeparateTranslucency"));
	{
		// Set the view family's render target/viewport.
		Context.SetViewportAndCallRHI(View.ViewRect);

		if (UseMeshDrawCommandPipeline())
		{
			View.ParallelMeshDrawCommandPasses[EMeshPass::TranslucencyAfterDOF].DispatchDraw(nullptr, Context.RHICmdList);
		}
		else
		{
			TUniformBufferRef<FMobileBasePassUniformParameters> BasePassUniformBuffer;
			CreateMobileBasePassUniformBuffer(Context.RHICmdList, View, true, BasePassUniformBuffer);

			FDrawingPolicyRenderState DrawRenderState(View, BasePassUniformBuffer);

			// Enable depth test, disable depth writes.
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

			// Draw translucent prims
			FMobileTranslucencyDrawingPolicyFactory::ContextType DrawingContext(ETranslucencyPass::TPT_TranslucencyAfterDOF);
			View.TranslucentPrimSet.DrawPrimitivesForMobile<FMobileTranslucencyDrawingPolicyFactory>(Context.RHICmdList, View, DrawRenderState, DrawingContext);
		}
	}
	Context.RHICmdList.EndRenderPass();
}

FRenderingCompositeOutput* FRCSeparateTranslucensyPassES2::GetOutput(EPassOutputId InPassOutputId)
{
	// draw on top of input (scenecolor)
	if (InPassOutputId == ePId_Output0)
	{
		return GetInput(EPassInputId::ePId_Input0)->GetOutput();
	}

	return nullptr;
}

FPooledRenderTargetDesc FRCSeparateTranslucensyPassES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	Ret.DebugName = TEXT("SeparateTranslucensyPassES2");
	return Ret;
}
