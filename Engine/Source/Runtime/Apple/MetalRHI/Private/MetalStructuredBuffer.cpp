// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#include "MetalRHIPrivate.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"
#include "Containers/ResourceArray.h"

FMetalStructuredBuffer::FMetalStructuredBuffer(uint32 Stride, uint32 InSize, FResourceArrayInterface* ResourceArray, uint32 InUsage)
	: FRHIStructuredBuffer(Stride, InSize, InUsage)
	, FMetalRHIBuffer(InSize, InUsage|EMetalBufferUsage_GPUOnly, RRT_StructuredBuffer)
{
	check((InSize % Stride) == 0);
	
	if (ResourceArray)
	{
		// copy any resources to the CPU address
		void* LockedMemory = RHILockStructuredBuffer(this, 0, InSize, RLM_WriteOnly);
 		FMemory::Memcpy(LockedMemory, ResourceArray->GetResourceData(), InSize);
		ResourceArray->Discard();
		RHIUnlockStructuredBuffer(this);
	}
}

FMetalStructuredBuffer::~FMetalStructuredBuffer()
{
}


FStructuredBufferRHIRef FMetalDynamicRHI::RHICreateStructuredBuffer(uint32 Stride,uint32 Size,uint32 InUsage,FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
	return new FMetalStructuredBuffer(Stride, Size, CreateInfo.ResourceArray, InUsage);
	}
}

void* FMetalDynamicRHI::RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI,uint32 Offset,uint32 Size,EResourceLockMode LockMode)
{
	@autoreleasepool {
	FMetalStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);
	
	// just return the memory plus the offset
	return (uint8*)StructuredBuffer->Lock(LockMode, Offset, Size);
	}
}

void FMetalDynamicRHI::RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	@autoreleasepool {
	FMetalStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);
	StructuredBuffer->Unlock();
	}
}

struct FMetalRHICommandInitialiseStructuredBuffer : public FRHICommand<FMetalRHICommandInitialiseStructuredBuffer>
{
	TRefCountPtr<FMetalStructuredBuffer> Buffer;
	
	FORCEINLINE_DEBUGGABLE FMetalRHICommandInitialiseStructuredBuffer(FMetalStructuredBuffer* InBuffer)
	: Buffer(InBuffer)
	{
	}
	
	virtual ~FMetalRHICommandInitialiseStructuredBuffer()
	{
	}
	
	void Execute(FRHICommandListBase& CmdList)
	{
		GetMetalDeviceContext().AsyncCopyFromBufferToBuffer(Buffer->CPUBuffer, 0, Buffer->Buffer, 0, Buffer->Buffer.GetLength());
		if (Buffer->GetUsage() & (BUF_Dynamic|BUF_Static))
		{
			LLM_SCOPE(ELLMTag::VertexBuffer);
			SafeReleaseMetalBuffer(Buffer->CPUBuffer);
		}
		else
		{
			Buffer->LastUpdate = GFrameNumberRenderThread;
		}
	}
};

FStructuredBufferRHIRef FMetalDynamicRHI::CreateStructuredBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
		// make the RHI object, which will allocate memory
		TRefCountPtr<FMetalStructuredBuffer> VertexBuffer = new FMetalStructuredBuffer(Stride, Size, nullptr, InUsage);
		
		if (CreateInfo.ResourceArray)
		{
			check(Size == CreateInfo.ResourceArray->GetResourceDataSize());
			
			if (VertexBuffer->CPUBuffer)
			{
				FMemory::Memzero(VertexBuffer->CPUBuffer.GetContents(), VertexBuffer->CPUBuffer.GetLength());
				
				FMemory::Memcpy(VertexBuffer->CPUBuffer.GetContents(), CreateInfo.ResourceArray->GetResourceData(), Size);
				
				if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
				{
					FMetalRHICommandInitialiseStructuredBuffer UpdateCommand(VertexBuffer);
					UpdateCommand.Execute(RHICmdList);
				}
				else
				{
					new (RHICmdList.AllocCommand<FMetalRHICommandInitialiseStructuredBuffer>()) FMetalRHICommandInitialiseStructuredBuffer(VertexBuffer);
				}
			}
			else
			{
				// make a buffer usable by CPU
				void* Buffer = RHILockStructuredBuffer(VertexBuffer, 0, Size, RLM_WriteOnly);
				
				// copy the contents of the given data into the buffer
				FMemory::Memcpy(Buffer, CreateInfo.ResourceArray->GetResourceData(), Size);
				
				RHIUnlockStructuredBuffer(VertexBuffer);
			}
			
			// Discard the resource array's contents.
			CreateInfo.ResourceArray->Discard();
		}
		
		return VertexBuffer.GetReference();
	}
}
