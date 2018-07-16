// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalConstantBuffer.cpp: Metal Constant buffer implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h"
#include "MetalBuffer.h"
#include "MetalCommandBuffer.h"
#include "HAL/LowLevelMemTracker.h"

FMetalUniformBuffer::FMetalUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage InUsage)
	: FRHIUniformBuffer(Layout)
	, FMetalRHIBuffer(Layout.ConstantBufferSize, BUF_Volatile, RRT_UniformBuffer)
{
	if (Layout.ConstantBufferSize > 0)
	{
		UE_CLOG(Layout.ConstantBufferSize > 65536, LogMetal, Fatal, TEXT("Trying to allocated a uniform layout of size %d that is greater than the maximum permitted 64k."), Layout.ConstantBufferSize);
		
		void* Data = Lock(RLM_WriteOnly, 0);
		FMemory::Memcpy(Data, Contents, Layout.ConstantBufferSize);
		Unlock();
	}

	// set up an SRT-style uniform buffer
	if (Layout.Resources.Num())
	{
		int32 NumResources = Layout.Resources.Num();
		ResourceTable.Empty(NumResources);
		ResourceTable.AddZeroed(NumResources);
		for (int32 i = 0; i < NumResources; ++i)
		{
			FRHIResource* Resource = *(FRHIResource**)((uint8*)Contents + Layout.ResourceOffsets[i]);

			// Allow null SRV's in uniform buffers for feature levels that don't support SRV's in shaders
			if (!(GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1 && Layout.Resources[i] == UBMT_SRV))
			{
				check(Resource);
			}
			ResourceTable[i] = Resource;
		}
	}
}

FMetalUniformBuffer::~FMetalUniformBuffer()
{
}

void const* FMetalUniformBuffer::GetData()
{
	if (Data)
	{
		return Data->Data;
	}
	else if (Buffer)
	{
		return MTLPP_VALIDATE(mtlpp::Buffer, Buffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, GetContents());
	}
	else
	{
		return nullptr;
	}
}

FUniformBufferRHIRef FMetalDynamicRHI::RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage)
{
	@autoreleasepool {
	check(IsInRenderingThread() || IsInParallelRenderingThread() || IsInRHIThread());
	return new FMetalUniformBuffer(Contents, Layout, Usage);
	}
}
