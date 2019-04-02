// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LongGPUTask.h"
#include "OneColorShader.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "RenderUtils.h"
#include "ClearQuad.h"

IMPLEMENT_SHADER_TYPE(, FLongGPUTaskPS, TEXT("/Engine/Private/OneColorShader.usf"), TEXT("MainLongGPUTask"), SF_Pixel);

int32 NumMeasuredIterationsToAchieve500ms = 0;

void IssueScalableLongGPUTask(FRHICommandListImmediate& RHICmdList, int32 NumIteration /* = -1 by default */)
{
	FRHIResourceCreateInfo Info;
	FTexture2DRHIRef LongTaskRenderTarget = RHICreateTexture2D(1920, 1080, PF_B8G8R8A8, 1, 1, TexCreate_RenderTargetable, Info);

	FRHIRenderPassInfo RPInfo(LongTaskRenderTarget, ERenderTargetActions::DontLoad_Store);
	TransitionRenderPassTargets(RHICmdList, RPInfo);
	RHICmdList.BeginRenderPass(RPInfo, TEXT("LongGPUTask"));

	{
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<TOneColorVS<true>> VertexShader(ShaderMap);
		TShaderMapRef<FLongGPUTaskPS> PixelShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader->GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader->GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
		VertexShader->SetDepthParameter(RHICmdList, 0.0f);

		RHICmdList.SetStreamSource(0, GClearVertexBuffer.VertexBufferRHI, 0);

		if (NumIteration == -1)
		{
			NumIteration = NumMeasuredIterationsToAchieve500ms;
		}

		for (int32 Iteration = 0; Iteration < NumIteration; Iteration++)
		{
			RHICmdList.DrawPrimitive(0, 2, 1);
		}
	}

	RHICmdList.EndRenderPass();
}

void MeasureLongGPUTaskExecutionTime(FRHICommandListImmediate& RHICmdList)
{
	const int32 NumIterationsForMeasurement = 5;

	FRenderQueryRHIRef TimeQueryStart = RHICmdList.CreateRenderQuery(RQT_AbsoluteTime);
	FRenderQueryRHIRef TimeQueryEnd = RHICmdList.CreateRenderQuery(RQT_AbsoluteTime);

	if (TimeQueryStart == nullptr || TimeQueryEnd == nullptr)
	{
		// Not all platforms/drivers support RQT_AbsoluteTime queries
		// Use fixed number of iterations on those platforms
		NumMeasuredIterationsToAchieve500ms = 5;
		return;
	}

	uint64 StartTime = 0;
	uint64 EndTime = 0;

	RHICmdList.EndRenderQuery(TimeQueryStart);

	IssueScalableLongGPUTask(RHICmdList, NumIterationsForMeasurement);

	RHICmdList.EndRenderQuery(TimeQueryEnd);

	// Required by DX12 to resolve the query
	RHICmdList.SubmitCommandsHint();
	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);

	if (RHICmdList.GetRenderQueryResult(TimeQueryStart, StartTime, true) && RHICmdList.GetRenderQueryResult(TimeQueryEnd, EndTime, true))
	{
		NumMeasuredIterationsToAchieve500ms = FMath::Clamp(FMath::FloorToInt(500.0f / ((EndTime - StartTime) / 1000.0f / NumIterationsForMeasurement)), 1, 200);
	}
	else
	{
		// Sometimes it fails even the platform supports RQT_AbsoluteTime
		// Fallback and show a warning
		NumMeasuredIterationsToAchieve500ms = 5;
		UE_LOG(LogTemp, Display, TEXT("Unable to get render query result on a platform supporting RQT_AbsoluteTime queries, defaulting to %d iterations for LongGPUTask"), NumMeasuredIterationsToAchieve500ms);
	}
}
