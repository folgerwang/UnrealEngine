// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LongGPUTask.h"
#include "OneColorShader.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "RenderUtils.h"
#include "ClearQuad.h"

IMPLEMENT_SHADER_TYPE(, FLongGPUTaskPS, TEXT("/Engine/Private/OneColorShader.usf"), TEXT("MainLongGPUTask"), SF_Pixel);

int32 NumMeasuredIterationsToAchieve100ms = 0;

const int32 NumIterationsForMeasurement = 5;

FRenderQueryRHIRef TimeQueryStart;
FRenderQueryRHIRef TimeQueryEnd;

void IssueScalableLongGPUTask(FRHICommandListImmediate& RHICmdList, int32 NumIteration /* = -1 by default */)
{
	FRHIResourceCreateInfo Info;
	FTexture2DRHIRef LongTaskRenderTarget = RHICreateTexture2D(1920, 1080, PF_B8G8R8A8, 1, 1, TexCreate_RenderTargetable, Info);

	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;

	SetRenderTarget(RHICmdList, LongTaskRenderTarget, FTextureRHIRef(), true);
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

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
		// Use the measured number of iterations to achieve 100ms
		// If the query results are still not available, stall
		if (NumMeasuredIterationsToAchieve100ms == 0)
		{
			if (TimeQueryStart != nullptr && TimeQueryEnd != nullptr) // Not all platforms/drivers support RQT_AbsoluteTime queries
			{
				uint64 StartTime = 0;
				uint64 EndTime = 0;

				// Results are in microseconds
				RHICmdList.GetRenderQueryResult(TimeQueryStart, StartTime, true);
				RHICmdList.GetRenderQueryResult(TimeQueryEnd, EndTime, true);

				NumMeasuredIterationsToAchieve100ms = FMath::Clamp(FMath::FloorToInt(100.0f / ((EndTime - StartTime) / 1000.0f / NumIterationsForMeasurement)), 1, 200);
			}
			else
			{
				NumMeasuredIterationsToAchieve100ms = 5; // Use a constant time on these platforms
			}
		}

		NumIteration = NumMeasuredIterationsToAchieve100ms;
	}

	for (int32 Iteration = 0; Iteration < NumIteration; Iteration++)
	{
		RHICmdList.DrawPrimitive(PT_TriangleStrip, 0, 2, 1);
	}
}

void MeasureLongGPUTaskExecutionTime(FRHICommandListImmediate& RHICmdList)
{
	if (TimeQueryStart != nullptr && TimeQueryEnd != nullptr)
	{
		return;
	}
	check(TimeQueryStart == nullptr && TimeQueryEnd == nullptr);

	TimeQueryStart = RHICmdList.CreateRenderQuery(RQT_AbsoluteTime);
	TimeQueryEnd = RHICmdList.CreateRenderQuery(RQT_AbsoluteTime);

	if (TimeQueryStart != nullptr && TimeQueryEnd != nullptr) // Not all platforms/drivers support RQT_AbsoluteTime queries
	{
		RHICmdList.EndRenderQuery(TimeQueryStart);

		IssueScalableLongGPUTask(RHICmdList, NumIterationsForMeasurement);

		RHICmdList.EndRenderQuery(TimeQueryEnd);
	}
}

void ClearLongGPUTaskQueries()
{
	TimeQueryStart = nullptr;
	TimeQueryEnd = nullptr;
}
