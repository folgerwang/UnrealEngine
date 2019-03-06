// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12VertexBuffer.cpp: D3D vertex buffer RHI implementation.
	=============================================================================*/

#include "D3D12RHIPrivate.h"

D3D12_RESOURCE_DESC CreateVertexBufferResourceDesc(uint32 Size, uint32 InUsage)
{
	// Describe the vertex buffer.
	D3D12_RESOURCE_DESC Desc = CD3DX12_RESOURCE_DESC::Buffer(Size);

	if (InUsage & BUF_UnorderedAccess)
	{
		Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		static bool bRequiresRawView = (GMaxRHIFeatureLevel < ERHIFeatureLevel::SM5);
		if (bRequiresRawView)
		{
			// Force the buffer to be a raw, byte address buffer
			InUsage |= BUF_ByteAddressBuffer;
		}
	}

	if ((InUsage & BUF_ShaderResource) == 0)
	{
		Desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}

	if (InUsage & BUF_DrawIndirect)
	{
		Desc.Flags |= D3D12RHI_RESOURCE_FLAG_ALLOW_INDIRECT_BUFFER;
	}

	return Desc;
}

FD3D12VertexBuffer::~FD3D12VertexBuffer()
{
	if (ResourceLocation.GetResource() != nullptr)
	{
		UpdateBufferStats<FD3D12VertexBuffer>(&ResourceLocation, false);
	}
}

void FD3D12VertexBuffer::Rename(FD3D12ResourceLocation& NewLocation)
{
	FD3D12ResourceLocation::TransferOwnership(ResourceLocation, NewLocation);

	if (DynamicSRV != nullptr)
	{
		DynamicSRV->Rename(ResourceLocation);
	}
}

void FD3D12VertexBuffer::RenameLDAChain(FD3D12ResourceLocation& NewLocation)
{
	ensure(GetUsage() & BUF_AnyDynamic);
	Rename(NewLocation);

	if (GNumExplicitGPUsForRendering > 1)
	{
		// Mutli-GPU support : renaming the LDA only works if we start we the head link. Otherwise Rename() must be used per GPU.
		ensure(IsHeadLink());
		ensure(GetParentDevice() == NewLocation.GetParentDevice());

		// Update all of the resources in the LDA chain to reference this cross-node resource
		for (FD3D12VertexBuffer* NextBuffer = GetNextObject(); NextBuffer; NextBuffer = NextBuffer->GetNextObject())
		{
			FD3D12ResourceLocation::ReferenceNode(NextBuffer->GetParentDevice(), NextBuffer->ResourceLocation, ResourceLocation);

			if (NextBuffer->DynamicSRV)
			{
				NextBuffer->DynamicSRV->Rename(NextBuffer->ResourceLocation);
			}
		}
	}
}


FVertexBufferRHIRef FD3D12DynamicRHI::RHICreateVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	const D3D12_RESOURCE_DESC Desc = CreateVertexBufferResourceDesc(Size, InUsage);
	const uint32 Alignment = 4;

	FD3D12VertexBuffer* Buffer = GetAdapter().CreateRHIBuffer<FD3D12VertexBuffer>(nullptr, Desc, Alignment, 0, Size, InUsage, CreateInfo);
	if (Buffer->ResourceLocation.IsTransient() )
	{
		// TODO: this should ideally be set in platform-independent code, since this tracking is for the high level
		Buffer->SetCommitted(false);
	}

	return Buffer;
}

void* FD3D12DynamicRHI::RHILockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	return LockBuffer(nullptr, FD3D12DynamicRHI::ResourceCast(VertexBufferRHI), Offset, Size, LockMode);
}

void FD3D12DynamicRHI::RHIUnlockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI)
{
	UnlockBuffer(nullptr, FD3D12DynamicRHI::ResourceCast(VertexBufferRHI));
}

FVertexBufferRHIRef FD3D12DynamicRHI::CreateVertexBuffer_RenderThread(FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{	
	const D3D12_RESOURCE_DESC Desc = CreateVertexBufferResourceDesc(Size, InUsage);
	const uint32 Alignment = 4;

	FD3D12VertexBuffer* Buffer = GetAdapter().CreateRHIBuffer<FD3D12VertexBuffer>(&RHICmdList, Desc, Alignment, 0, Size, InUsage, CreateInfo);
	if (Buffer->ResourceLocation.IsTransient())
	{
		// TODO: this should ideally be set in platform-independent code, since this tracking is for the high level
		Buffer->SetCommitted(false);
	}

	return Buffer;
}

void* FD3D12DynamicRHI::LockVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBufferRHI, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
	// Pull down the above RHI implementation so that we can flush only when absolutely necessary
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FDynamicRHI_LockVertexBuffer_RenderThread);
	check(IsInRenderingThread());

	return LockBuffer(&RHICmdList, FD3D12DynamicRHI::ResourceCast(VertexBufferRHI), Offset, SizeRHI, LockMode);
}

void FD3D12DynamicRHI::UnlockVertexBuffer_RenderThread(FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBufferRHI)
{
	// Pull down the above RHI implementation so that we can flush only when absolutely necessary
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FDynamicRHI_UnlockVertexBuffer_RenderThread);
	check(IsInRenderingThread());

	UnlockBuffer(&RHICmdList, FD3D12DynamicRHI::ResourceCast(VertexBufferRHI));
}

void FD3D12DynamicRHI::RHICopyVertexBuffer(FVertexBufferRHIParamRef SourceBufferRHI, FVertexBufferRHIParamRef DestBufferRHI)
{
	FD3D12VertexBuffer*  SourceBuffer = FD3D12DynamicRHI::ResourceCast(SourceBufferRHI);
	FD3D12VertexBuffer*  DestBuffer = FD3D12DynamicRHI::ResourceCast(DestBufferRHI);

	while (SourceBuffer && DestBuffer)
	{
		FD3D12Device* Device = SourceBuffer->GetParentDevice();
		check(Device == DestBuffer->GetParentDevice());

		FD3D12Resource* pSourceResource = SourceBuffer->ResourceLocation.GetResource();
		D3D12_RESOURCE_DESC const& SourceBufferDesc = pSourceResource->GetDesc();

		FD3D12Resource* pDestResource = DestBuffer->ResourceLocation.GetResource();
		D3D12_RESOURCE_DESC const& DestBufferDesc = pDestResource->GetDesc();

		check(SourceBufferDesc.Width == DestBufferDesc.Width);
		check(SourceBuffer->GetSize() == DestBuffer->GetSize());

		FD3D12CommandContext& Context = Device->GetDefaultCommandContext();
		Context.numCopies++;
		Context.CommandListHandle->CopyResource(pDestResource->GetResource(), pSourceResource->GetResource());
		Context.CommandListHandle.UpdateResidency(pDestResource);
		Context.CommandListHandle.UpdateResidency(pSourceResource);

		DEBUG_EXECUTE_COMMAND_CONTEXT(Device->GetDefaultCommandContext());

		Device->RegisterGPUWork(1);

		SourceBuffer = SourceBuffer->GetNextObject();
		DestBuffer = DestBuffer->GetNextObject();
	}
}

#if D3D12_RHI_RAYTRACING
void FD3D12CommandContext::RHICopyBufferRegion(FVertexBufferRHIParamRef DestBufferRHI, uint64 DstOffset, FVertexBufferRHIParamRef SourceBufferRHI, uint64 SrcOffset, uint64 NumBytes)
{
	FD3D12VertexBuffer*  SourceBuffer = FD3D12DynamicRHI::ResourceCast(SourceBufferRHI);
	FD3D12VertexBuffer*  DestBuffer = FD3D12DynamicRHI::ResourceCast(DestBufferRHI);

	while (SourceBuffer && DestBuffer)
	{
		FD3D12Device* Device = SourceBuffer->GetParentDevice();
		check(Device == DestBuffer->GetParentDevice());

		FD3D12Resource* pSourceResource = SourceBuffer->ResourceLocation.GetResource();
		D3D12_RESOURCE_DESC const& SourceBufferDesc = pSourceResource->GetDesc();

		FD3D12Resource* pDestResource = DestBuffer->ResourceLocation.GetResource();
		D3D12_RESOURCE_DESC const& DestBufferDesc = pDestResource->GetDesc();

		checkf(pSourceResource != pDestResource, TEXT("CopyBufferRegion cannot be used on the same resource. This can happen when both the source and the dest are suballocated from the same resource."));

		check(DstOffset + NumBytes <= DestBufferDesc.Width);
		check(SrcOffset + NumBytes <= SourceBufferDesc.Width);

		numCopies++;

		FConditionalScopeResourceBarrier ScopeResourceBarrierDest(CommandListHandle, pDestResource, D3D12_RESOURCE_STATE_COPY_DEST, 0);
		CommandListHandle.FlushResourceBarriers();
		CommandListHandle->CopyBufferRegion(pDestResource->GetResource(), DestBuffer->ResourceLocation.GetOffsetFromBaseOfResource() + DstOffset, pSourceResource->GetResource(), SourceBuffer->ResourceLocation.GetOffsetFromBaseOfResource() + SrcOffset, NumBytes);
		CommandListHandle.UpdateResidency(pDestResource);
		CommandListHandle.UpdateResidency(pSourceResource);

		Device->RegisterGPUWork(1);

		SourceBuffer = SourceBuffer->GetNextObject();
		DestBuffer = DestBuffer->GetNextObject();
	}
}

void FD3D12CommandContext::RHICopyBufferRegions(const TArrayView<const FCopyBufferRegionParams> Params)
{
	auto TransitionBuffer = [](FD3D12Resource* pResource, FD3D12CommandListHandle& InCommandListHandle, const D3D12_RESOURCE_STATES Desired)
	{
		bool bUseTracking = pResource->RequiresResourceStateTracking();
		D3D12_RESOURCE_STATES Current;
		const uint32 Subresource = 0;

		if (!bUseTracking)
		{
			Current = pResource->GetDefaultResourceState();
			if (Current != Desired)
			{
				InCommandListHandle.AddTransitionBarrier(pResource, Current, Desired, Subresource);
			}
		}
		else
		{
			FD3D12DynamicRHI::TransitionResource(InCommandListHandle, pResource, Desired, Subresource);
		}
	};

	// Transition buffers to copy states
	for (auto& Param : Params)
	{
		FD3D12VertexBuffer*  SourceBuffer = FD3D12DynamicRHI::ResourceCast(Param.SourceBuffer);
		FD3D12VertexBuffer*  DestBuffer = FD3D12DynamicRHI::ResourceCast(Param.DestBuffer);

		while (SourceBuffer && DestBuffer)
		{
			FD3D12Device* Device = SourceBuffer->GetParentDevice();
			check(Device == DestBuffer->GetParentDevice());

			FD3D12Resource* pSourceResource = SourceBuffer->ResourceLocation.GetResource();
			FD3D12Resource* pDestResource = DestBuffer->ResourceLocation.GetResource();

			checkf(pSourceResource != pDestResource, TEXT("CopyBufferRegion cannot be used on the same resource. This can happen when both the source and the dest are suballocated from the same resource."));

			TransitionBuffer(pSourceResource, CommandListHandle, D3D12_RESOURCE_STATE_COPY_SOURCE);
			TransitionBuffer(pDestResource, CommandListHandle, D3D12_RESOURCE_STATE_COPY_DEST);

			SourceBuffer = SourceBuffer->GetNextObject();
			DestBuffer = DestBuffer->GetNextObject();
		}
	}

	CommandListHandle.FlushResourceBarriers();

	for (auto& Param : Params)
	{
		FD3D12VertexBuffer*  SourceBuffer = FD3D12DynamicRHI::ResourceCast(Param.SourceBuffer);
		FD3D12VertexBuffer*  DestBuffer = FD3D12DynamicRHI::ResourceCast(Param.DestBuffer);
		uint64 SrcOffset = Param.SrcOffset;
		uint64 DstOffset = Param.DstOffset;
		uint64 NumBytes = Param.NumBytes;

		while (SourceBuffer && DestBuffer)
		{
			FD3D12Device* Device = SourceBuffer->GetParentDevice();
			check(Device == DestBuffer->GetParentDevice());

			FD3D12Resource* pSourceResource = SourceBuffer->ResourceLocation.GetResource();
			D3D12_RESOURCE_DESC const& SourceBufferDesc = pSourceResource->GetDesc();

			FD3D12Resource* pDestResource = DestBuffer->ResourceLocation.GetResource();
			D3D12_RESOURCE_DESC const& DestBufferDesc = pDestResource->GetDesc();

			check(DstOffset + NumBytes <= DestBufferDesc.Width);
			check(SrcOffset + NumBytes <= SourceBufferDesc.Width);

			numCopies++;

			CommandListHandle->CopyBufferRegion(pDestResource->GetResource(), DestBuffer->ResourceLocation.GetOffsetFromBaseOfResource() + DstOffset, pSourceResource->GetResource(), SourceBuffer->ResourceLocation.GetOffsetFromBaseOfResource() + SrcOffset, NumBytes);
			CommandListHandle.UpdateResidency(pDestResource);
			CommandListHandle.UpdateResidency(pSourceResource);

			Device->RegisterGPUWork(1);

			SourceBuffer = SourceBuffer->GetNextObject();
			DestBuffer = DestBuffer->GetNextObject();
		}
	}

	// Transition buffers to generic read
	for (auto& Param : Params)
	{
		FD3D12VertexBuffer*  SourceBuffer = FD3D12DynamicRHI::ResourceCast(Param.SourceBuffer);
		FD3D12VertexBuffer*  DestBuffer = FD3D12DynamicRHI::ResourceCast(Param.DestBuffer);

		while (SourceBuffer && DestBuffer)
		{
			FD3D12Device* Device = SourceBuffer->GetParentDevice();
			check(Device == DestBuffer->GetParentDevice());

			FD3D12Resource* pSourceResource = SourceBuffer->ResourceLocation.GetResource();
			FD3D12Resource* pDestResource = DestBuffer->ResourceLocation.GetResource();

			TransitionBuffer(pSourceResource, CommandListHandle, D3D12_RESOURCE_STATE_GENERIC_READ);
			TransitionBuffer(pDestResource, CommandListHandle, D3D12_RESOURCE_STATE_GENERIC_READ);

			SourceBuffer = SourceBuffer->GetNextObject();
			DestBuffer = DestBuffer->GetNextObject();
		}
	}
}
#endif

FVertexBufferRHIRef FD3D12DynamicRHI::CreateAndLockVertexBuffer_RenderThread(FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
{
	const D3D12_RESOURCE_DESC Desc = CreateVertexBufferResourceDesc(Size, InUsage);
	const uint32 Alignment = 4;

	FD3D12VertexBuffer* Buffer = GetAdapter().CreateRHIBuffer<FD3D12VertexBuffer>(nullptr, Desc, Alignment, 0, Size, InUsage, CreateInfo);
	if (Buffer->ResourceLocation.IsTransient())
	{
		// TODO: this should ideally be set in platform-independent code, since this tracking is for the high level
		Buffer->SetCommitted(false);
	}
	OutDataBuffer = LockVertexBuffer_RenderThread(RHICmdList, Buffer, 0, Size, RLM_WriteOnly);

	return Buffer;
}
