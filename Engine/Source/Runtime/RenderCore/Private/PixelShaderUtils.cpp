// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PixelShaderUtils.cpp: Implementations of utilities for pixel shaders.
=============================================================================*/

#include "PixelShaderUtils.h"
#include "DummyRenderResources.h"
#include "RenderGraph.h"


// static
void FPixelShaderUtils::DrawFullscreenTriangle(FRHICommandList& RHICmdList, uint32 InstanceCount)
{
	RHICmdList.SetStreamSource(0, GScreenRectangleVertexBuffer.VertexBufferRHI, 0);

	RHICmdList.DrawIndexedPrimitive(
		GScreenRectangleIndexBuffer.IndexBufferRHI,
		/*BaseVertexIndex=*/ 0,
		/*MinIndex=*/ 0,
		/*NumVertices=*/ 3,
		/*StartIndex=*/ 6,
		/*NumPrimitives=*/ 1,
		/*NumInstances=*/ InstanceCount);
}

// static
void FPixelShaderUtils::DrawFullscreenQuad(FRHICommandList& RHICmdList, uint32 InstanceCount)
{
	RHICmdList.SetStreamSource(0, GScreenRectangleVertexBuffer.VertexBufferRHI, 0);

	RHICmdList.DrawIndexedPrimitive(
		GScreenRectangleIndexBuffer.IndexBufferRHI,
		/*BaseVertexIndex=*/ 0,
		/*MinIndex=*/ 0,
		/*NumVertices=*/ 4,
		/*StartIndex=*/ 0,
		/*NumPrimitives=*/ 2,
		/*NumInstances=*/ InstanceCount);
}
