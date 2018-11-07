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
	FMetalStructuredBuffer* Buffer = new FMetalStructuredBuffer(Stride, Size, CreateInfo.ResourceArray, InUsage);
	if (!CreateInfo.ResourceArray && Buffer->Buffer.GetStorageMode() == mtlpp::StorageMode::Private)
	{
		if (Buffer->GetUsage() & (BUF_Dynamic|BUF_Static))
		{
			LLM_SCOPE(ELLMTag::VertexBuffer);
			SafeReleaseMetalBuffer(Buffer->CPUBuffer);
			Buffer->CPUBuffer = nil;
		}

		if (GMetalBufferZeroFill)
		{
			GetMetalDeviceContext().FillBuffer(Buffer->Buffer, ns::Range(0, Buffer->Buffer.GetLength()), 0);
		}
	}
#if PLATFORM_MAC
	else if (GMetalBufferZeroFill && !CreateInfo.ResourceArray && Buffer->Buffer.GetStorageMode() == mtlpp::StorageMode::Managed)
	{
		MTLPP_VALIDATE(mtlpp::Buffer, Buffer->Buffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, DidModify(ns::Range(0, Buffer->Buffer.GetLength())));
	}
#endif
	return Buffer;
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
		if (Buffer->CPUBuffer)
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
#if PLATFORM_MAC
		else if (GMetalBufferZeroFill)
		{
			GetMetalDeviceContext().FillBuffer(Buffer->Buffer, ns::Range(0, Buffer->Buffer.GetLength()), 0);
		}
#endif
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
				FMemory::Memcpy(VertexBuffer->CPUBuffer.GetContents(), CreateInfo.ResourceArray->GetResourceData(), Size);

#if PLATFORM_MAC
				if(VertexBuffer->CPUBuffer.GetStorageMode() == mtlpp::StorageMode::Managed)
				{
					MTLPP_VALIDATE(mtlpp::Buffer, VertexBuffer->CPUBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, DidModify(ns::Range(0, GMetalBufferZeroFill ? VertexBuffer->CPUBuffer.GetLength() : Size)));
				}
#endif
				
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
		else if (VertexBuffer->Buffer.GetStorageMode() == mtlpp::StorageMode::Private)
		{
			if (VertexBuffer->GetUsage() & (BUF_Dynamic|BUF_Static))
			{
				LLM_SCOPE(ELLMTag::VertexBuffer);
				SafeReleaseMetalBuffer(VertexBuffer->CPUBuffer);
				VertexBuffer->CPUBuffer = nil;
			}

			if (GMetalBufferZeroFill)
			{
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
		}
#if PLATFORM_MAC
		else if (GMetalBufferZeroFill && VertexBuffer->Buffer.GetStorageMode() == mtlpp::StorageMode::Managed)
		{
			MTLPP_VALIDATE(mtlpp::Buffer, VertexBuffer->Buffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, DidModify(ns::Range(0, VertexBuffer->Buffer.GetLength())));
		}
#endif
		
		return VertexBuffer.GetReference();
	}
}
