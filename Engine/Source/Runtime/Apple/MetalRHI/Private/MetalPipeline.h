// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetalRHIPrivate.h"
#if METAL_DEBUG_OPTIONS
#include "MetalDebugCommandEncoder.h"
#endif

enum EMetalPipelineHashBits
{
	NumBits_RenderTargetFormat = 5, //(x8=40),
	NumBits_DepthFormat = 3, //(x1=3),
	NumBits_StencilFormat = 3, //(x1=3),
	NumBits_SampleCount = 3, //(x1=3),

	NumBits_BlendState = 5, //(x8=40),
	NumBits_PrimitiveTopology = 2, //(x1=2)
	NumBits_IndexType = 2, //(x1=2)
};

enum EMetalPipelineHashOffsets
{
	Offset_BlendState0 = 0,
	Offset_BlendState1 = Offset_BlendState0 + NumBits_BlendState,
	Offset_BlendState2 = Offset_BlendState1 + NumBits_BlendState,
	Offset_BlendState3 = Offset_BlendState2 + NumBits_BlendState,
	Offset_BlendState4 = Offset_BlendState3 + NumBits_BlendState,
	Offset_BlendState5 = Offset_BlendState4 + NumBits_BlendState,
	Offset_BlendState6 = Offset_BlendState5 + NumBits_BlendState,
	Offset_BlendState7 = Offset_BlendState6 + NumBits_BlendState,
	Offset_PrimitiveTopology = Offset_BlendState7 + NumBits_BlendState,
	Offset_IndexType = Offset_PrimitiveTopology + NumBits_PrimitiveTopology,
	Offset_RasterEnd = Offset_IndexType + NumBits_IndexType,

	Offset_RenderTargetFormat0 = 64,
	Offset_RenderTargetFormat1 = Offset_RenderTargetFormat0 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat2 = Offset_RenderTargetFormat1 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat3 = Offset_RenderTargetFormat2 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat4 = Offset_RenderTargetFormat3 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat5 = Offset_RenderTargetFormat4 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat6 = Offset_RenderTargetFormat5 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat7 = Offset_RenderTargetFormat6 + NumBits_RenderTargetFormat,
	Offset_DepthFormat = Offset_RenderTargetFormat7 + NumBits_RenderTargetFormat,
	Offset_StencilFormat = Offset_DepthFormat + NumBits_DepthFormat,
	Offset_SampleCount = Offset_StencilFormat + NumBits_StencilFormat,
	Offset_End = Offset_SampleCount + NumBits_SampleCount
};

struct FMetalTessellationPipelineDesc
{
	FMetalTessellationPipelineDesc()
	: DomainVertexDescriptor(nil)
	{
	}
	mtlpp::VertexDescriptor DomainVertexDescriptor;
	NSUInteger TessellationInputControlPointBufferIndex;
	NSUInteger TessellationOutputControlPointBufferIndex;
	NSUInteger TessellationPatchControlPointOutSize;
	NSUInteger TessellationPatchConstBufferIndex;
	NSUInteger TessellationInputPatchConstBufferIndex;
	NSUInteger TessellationPatchConstOutSize;
	NSUInteger TessellationTessFactorOutSize;
	NSUInteger TessellationFactorBufferIndex;
	NSUInteger TessellationPatchCountBufferIndex;
	NSUInteger TessellationControlPointIndexBufferIndex;
	NSUInteger TessellationIndexBufferIndex;
	NSUInteger DSNumUniformBuffers; // DEBUG ONLY
};

@interface FMetalShaderPipeline : FApplePlatformObject
{
@public
	mtlpp::RenderPipelineState RenderPipelineState;
	mtlpp::ComputePipelineState ComputePipelineState;
	FMetalTessellationPipelineDesc TessellationPipelineDesc;
#if METAL_DEBUG_OPTIONS
	FMetalDebugShaderResourceMask ResourceMask[EMetalShaderStagesNum];
	mtlpp::RenderPipelineReflection RenderPipelineReflection;
	mtlpp::ComputePipelineReflection ComputePipelineReflection;
	ns::String VertexSource;
	ns::String FragmentSource;
	ns::String ComputeSource;
	mtlpp::RenderPipelineDescriptor RenderDesc;
	mtlpp::ComputePipelineDescriptor ComputeDesc;
#endif
}
#if METAL_DEBUG_OPTIONS
- (instancetype)init;
- (void)initResourceMask;
- (void)initResourceMask:(EMetalShaderFrequency)Frequency;
#endif
@end
