// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalIndexBuffer.cpp: Metal Index buffer RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"
#include "MetalCommandQueue.h"
#include "Containers/ResourceArray.h"
#include "RenderUtils.h"
#include "HAL/LowLevelMemTracker.h"

static uint32 MetalIndexBufferUsage(uint32 InUsage)
{
	uint32 Usage = InUsage;
	if (RHISupportsTessellation(GMaxRHIShaderPlatform))
	{
		Usage |= BUF_ShaderResource;
	}
	Usage |= EMetalBufferUsage_GPUOnly|EMetalBufferUsage_LinearTex;
	return Usage;
}

/** Constructor */
FMetalIndexBuffer::FMetalIndexBuffer(uint32 InStride, uint32 InSize, uint32 InUsage)
	: FRHIIndexBuffer(InStride, InSize, InUsage)
	, FMetalRHIBuffer(InSize, MetalIndexBufferUsage(InUsage), RRT_IndexBuffer)
	, IndexType((InStride == 2) ? mtlpp::IndexType::UInt16 : mtlpp::IndexType::UInt32)
{
	if (RHISupportsTessellation(GMaxRHIShaderPlatform))
	{
		EPixelFormat Format = IndexType == mtlpp::IndexType::UInt16 ? PF_R16_UINT : PF_R32_UINT;
		CreateLinearTexture(Format, this);
	}
}

FMetalIndexBuffer::~FMetalIndexBuffer()
{
}

FIndexBufferRHIRef FMetalDynamicRHI::RHICreateIndexBuffer(uint32 Stride,uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
	// make the RHI object, which will allocate memory
	FMetalIndexBuffer* IndexBuffer = new FMetalIndexBuffer(Stride, Size, InUsage);
	
	if (CreateInfo.ResourceArray)
	{
		check(Size == CreateInfo.ResourceArray->GetResourceDataSize());

		// make a buffer usable by CPU
		void* Buffer = RHILockIndexBuffer(IndexBuffer, 0, Size, RLM_WriteOnly);

		// copy the contents of the given data into the buffer
		FMemory::Memcpy(Buffer, CreateInfo.ResourceArray->GetResourceData(), Size);

		RHIUnlockIndexBuffer(IndexBuffer);

		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}
	else if (IndexBuffer->Buffer.GetStorageMode() == mtlpp::StorageMode::Private)
	{
		if (IndexBuffer->UsePrivateMemory())
		{
			LLM_SCOPE(ELLMTag::IndexBuffer);
			SafeReleaseMetalBuffer(IndexBuffer->CPUBuffer);
			IndexBuffer->CPUBuffer = nil;
		}

		if (GMetalBufferZeroFill && !FMetalCommandQueue::SupportsFeature(EMetalFeaturesFences))
		{
			GetMetalDeviceContext().FillBuffer(IndexBuffer->Buffer, ns::Range(0, IndexBuffer->Buffer.GetLength()), 0);
		}
	}
#if PLATFORM_MAC
	else if (GMetalBufferZeroFill && IndexBuffer->Buffer.GetStorageMode() == mtlpp::StorageMode::Managed)
	{
		MTLPP_VALIDATE(mtlpp::Buffer, IndexBuffer->Buffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, DidModify(ns::Range(0, IndexBuffer->Buffer.GetLength())));
	}
#endif

	return IndexBuffer;
	}
}

void* FMetalDynamicRHI::RHILockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	@autoreleasepool {
	FMetalIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

	return (uint8*)IndexBuffer->Lock(LockMode, Offset, Size);
	}
}

void FMetalDynamicRHI::RHIUnlockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI)
{
	@autoreleasepool {
	FMetalIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

	IndexBuffer->Unlock();
	}
}

struct FMetalRHICommandInitialiseIndexBuffer : public FRHICommand<FMetalRHICommandInitialiseIndexBuffer>
{
	TRefCountPtr<FMetalIndexBuffer> Buffer;
	
	FORCEINLINE_DEBUGGABLE FMetalRHICommandInitialiseIndexBuffer(FMetalIndexBuffer* InBuffer)
	: Buffer(InBuffer)
	{
	}
	
	virtual ~FMetalRHICommandInitialiseIndexBuffer()
	{
	}
	
	void Execute(FRHICommandListBase& CmdList)
	{
		if (Buffer->CPUBuffer)
		{
			GetMetalDeviceContext().AsyncCopyFromBufferToBuffer(Buffer->CPUBuffer, 0, Buffer->Buffer, 0, Buffer->Buffer.GetLength());
			if (Buffer->UsePrivateMemory())
			{
				LLM_SCOPE(ELLMTag::IndexBuffer);
				SafeReleaseMetalBuffer(Buffer->CPUBuffer);
			}
			else
			{
				Buffer->LastUpdate = GFrameNumberRenderThread;
			}
		}
		else if (GMetalBufferZeroFill && !FMetalCommandQueue::SupportsFeature(EMetalFeaturesFences))
		{
			GetMetalDeviceContext().FillBuffer(Buffer->Buffer, ns::Range(0, Buffer->Buffer.GetLength()), 0);
		}
	}
};

FIndexBufferRHIRef FMetalDynamicRHI::CreateIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
		// make the RHI object, which will allocate memory
		TRefCountPtr<FMetalIndexBuffer> IndexBuffer = new FMetalIndexBuffer(Stride, Size, InUsage);
		
		if (CreateInfo.ResourceArray)
		{
			check(Size == CreateInfo.ResourceArray->GetResourceDataSize());
			
			if (IndexBuffer->CPUBuffer)
			{
				FMemory::Memcpy(IndexBuffer->CPUBuffer.GetContents(), CreateInfo.ResourceArray->GetResourceData(), Size);

#if PLATFORM_MAC
				if(IndexBuffer->CPUBuffer.GetStorageMode() == mtlpp::StorageMode::Managed)
				{
					MTLPP_VALIDATE(mtlpp::Buffer, IndexBuffer->CPUBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, DidModify(ns::Range(0, GMetalBufferZeroFill ? IndexBuffer->CPUBuffer.GetLength() : Size)));
				}
#endif
				
				if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
				{
					FMetalRHICommandInitialiseIndexBuffer UpdateCommand(IndexBuffer);
					UpdateCommand.Execute(RHICmdList);
				}
				else
				{
					new (RHICmdList.AllocCommand<FMetalRHICommandInitialiseIndexBuffer>()) FMetalRHICommandInitialiseIndexBuffer(IndexBuffer);
				}
			}
			else
			{
				// make a buffer usable by CPU
				void* Buffer = RHILockIndexBuffer(IndexBuffer, 0, Size, RLM_WriteOnly);
				
				// copy the contents of the given data into the buffer
				FMemory::Memcpy(Buffer, CreateInfo.ResourceArray->GetResourceData(), Size);
				
				RHIUnlockIndexBuffer(IndexBuffer);
			}
			
			// Discard the resource array's contents.
			CreateInfo.ResourceArray->Discard();
		}
		else if (IndexBuffer->Buffer.GetStorageMode() == mtlpp::StorageMode::Private)
		{
			if (IndexBuffer->UsePrivateMemory())
			{
				LLM_SCOPE(ELLMTag::IndexBuffer);
				SafeReleaseMetalBuffer(IndexBuffer->CPUBuffer);
				IndexBuffer->CPUBuffer = nil;
			}
			
			if (GMetalBufferZeroFill)
			{
				if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
				{
					FMetalRHICommandInitialiseIndexBuffer UpdateCommand(IndexBuffer);
					UpdateCommand.Execute(RHICmdList);
				}
				else
				{
					new (RHICmdList.AllocCommand<FMetalRHICommandInitialiseIndexBuffer>()) FMetalRHICommandInitialiseIndexBuffer(IndexBuffer);
				}
			}
		}
#if PLATFORM_MAC
		else if (GMetalBufferZeroFill && IndexBuffer->Buffer.GetStorageMode() == mtlpp::StorageMode::Managed)
		{
			MTLPP_VALIDATE(mtlpp::Buffer, IndexBuffer->Buffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, DidModify(ns::Range(0, IndexBuffer->Buffer.GetLength())));
		}
#endif
		
		return IndexBuffer.GetReference();
	}
}

