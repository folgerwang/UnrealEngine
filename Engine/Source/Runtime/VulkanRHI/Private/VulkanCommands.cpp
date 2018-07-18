// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanCommands.cpp: Vulkan RHI commands implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "EngineGlobals.h"

static TAutoConsoleVariable<int32> GCVarSubmitOnDispatch(
	TEXT("r.Vulkan.SubmitOnDispatch"),
	0,
	TEXT("0 to not do anything special on dispatch(default)\n")\
	TEXT("1 to submit the cmd buffer after each dispatch"),
	ECVF_RenderThreadSafe
);

int32 GVulkanSubmitAfterEveryEndRenderPass = 0;
static FAutoConsoleVariableRef CVarVulkanSubmitAfterEveryEndRenderPass(
	TEXT("r.Vulkan.SubmitAfterEveryEndRenderPass"),
	GVulkanSubmitAfterEveryEndRenderPass,
	TEXT("Forces a submit after every end render pass.\n")
	TEXT(" 0: Don't(default)\n")
	TEXT(" 1: Enable submitting"),
	ECVF_Default
);

static inline bool UseRealUBs()
{
	static int32 Status = -1;
	if (Status == -1)
	{
		TConsoleVariableData<int32>* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Vulkan.UseRealUBs"));
		if (CVar)
		{
			Status = CVar->GetValueOnAnyThread() != 0;
		}
		else
		{
			Status = 0;
		}
	}
	return Status > 0;
}

// make sure what the hardware expects matches what we give it for indirect arguments
static_assert(sizeof(FRHIDrawIndirectParameters) == sizeof(VkDrawIndirectCommand), "FRHIDrawIndirectParameters size is wrong.");
static_assert(STRUCT_OFFSET(FRHIDrawIndirectParameters, VertexCountPerInstance) == STRUCT_OFFSET(VkDrawIndirectCommand, vertexCount), "Wrong offset of FRHIDrawIndirectParameters::VertexCountPerInstance.");
static_assert(STRUCT_OFFSET(FRHIDrawIndirectParameters, InstanceCount) == STRUCT_OFFSET(VkDrawIndirectCommand, instanceCount), "Wrong offset of FRHIDrawIndirectParameters::InstanceCount.");
static_assert(STRUCT_OFFSET(FRHIDrawIndirectParameters, StartVertexLocation) == STRUCT_OFFSET(VkDrawIndirectCommand, firstVertex), "Wrong offset of FRHIDrawIndirectParameters::StartVertexLocation.");
static_assert(STRUCT_OFFSET(FRHIDrawIndirectParameters, StartInstanceLocation) == STRUCT_OFFSET(VkDrawIndirectCommand, firstInstance), "Wrong offset of FRHIDrawIndirectParameters::StartInstanceLocation.");

static_assert(sizeof(FRHIDrawIndexedIndirectParameters) == sizeof(VkDrawIndexedIndirectCommand), "FRHIDrawIndexedIndirectParameters size is wrong.");
static_assert(STRUCT_OFFSET(FRHIDrawIndexedIndirectParameters, IndexCountPerInstance) == STRUCT_OFFSET(VkDrawIndexedIndirectCommand, indexCount), "Wrong offset of FRHIDrawIndexedIndirectParameters::IndexCountPerInstance.");
static_assert(STRUCT_OFFSET(FRHIDrawIndexedIndirectParameters, InstanceCount) == STRUCT_OFFSET(VkDrawIndexedIndirectCommand, instanceCount), "Wrong offset of FRHIDrawIndexedIndirectParameters::InstanceCount.");
static_assert(STRUCT_OFFSET(FRHIDrawIndexedIndirectParameters, StartIndexLocation) == STRUCT_OFFSET(VkDrawIndexedIndirectCommand, firstIndex), "Wrong offset of FRHIDrawIndexedIndirectParameters::StartIndexLocation.");
static_assert(STRUCT_OFFSET(FRHIDrawIndexedIndirectParameters, BaseVertexLocation) == STRUCT_OFFSET(VkDrawIndexedIndirectCommand, vertexOffset), "Wrong offset of FRHIDrawIndexedIndirectParameters::BaseVertexLocation.");
static_assert(STRUCT_OFFSET(FRHIDrawIndexedIndirectParameters, StartInstanceLocation) == STRUCT_OFFSET(VkDrawIndexedIndirectCommand, firstInstance), "Wrong offset of FRHIDrawIndexedIndirectParameters::StartInstanceLocation.");

static_assert(sizeof(FRHIDispatchIndirectParameters) == sizeof(VkDispatchIndirectCommand), "FRHIDispatchIndirectParameters size is wrong.");
static_assert(STRUCT_OFFSET(FRHIDispatchIndirectParameters, ThreadGroupCountX) == STRUCT_OFFSET(VkDispatchIndirectCommand, x), "FRHIDispatchIndirectParameters X dimension is wrong.");
static_assert(STRUCT_OFFSET(FRHIDispatchIndirectParameters, ThreadGroupCountY) == STRUCT_OFFSET(VkDispatchIndirectCommand, y), "FRHIDispatchIndirectParameters Y dimension is wrong.");
static_assert(STRUCT_OFFSET(FRHIDispatchIndirectParameters, ThreadGroupCountZ) == STRUCT_OFFSET(VkDispatchIndirectCommand, z), "FRHIDispatchIndirectParameters Z dimension is wrong.");


void FVulkanCommandListContext::RHISetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBufferRHI, uint32 Offset)
{
	FVulkanVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	if (VertexBuffer != nullptr)
	{
		PendingGfxState->SetStreamSource(StreamIndex, VertexBuffer->GetHandle(), Offset + VertexBuffer->GetOffset());
	}
}

void FVulkanDynamicRHI::RHISetStreamOutTargets(uint32 NumTargets, const FVertexBufferRHIParamRef* VertexBuffers, const uint32* Offsets)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetRasterizerState(FRasterizerStateRHIParamRef NewStateRHI)
{
	check(0);
}

void FVulkanCommandListContext::RHISetComputeShader(FComputeShaderRHIParamRef ComputeShaderRHI)
{
	FVulkanComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	FVulkanComputePipeline* ComputePipeline = Device->GetPipelineStateCache()->GetOrCreateComputePipeline(ComputeShader);
	RHISetComputePipelineState(ComputePipeline);
}

void FVulkanCommandListContext::RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState)
{
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (CmdBuffer->IsInsideRenderPass())
	{
		TransitionAndLayoutManager.EndEmulatedRenderPass(CmdBuffer);
		if (GVulkanSubmitAfterEveryEndRenderPass)
		{
			CommandBufferManager->SubmitActiveCmdBuffer();
			CommandBufferManager->PrepareForNewActiveCommandBuffer();
			CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
		}
	}

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	if (CmdBuffer->CurrentDescriptorPoolSetContainer == nullptr)
	{
		CmdBuffer->CurrentDescriptorPoolSetContainer = &Device->GetDescriptorPoolsManager().AcquirePoolSetContainer();
	}
#endif
	//#todo-rco: Set PendingGfx to null
	FVulkanComputePipeline* ComputePipeline = ResourceCast(ComputePipelineState);
	PendingComputeState->SetComputePipeline(ComputePipeline);
}

void FVulkanCommandListContext::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanDispatchCallTime);
#endif

	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	ensure(Cmd->IsOutsideRenderPass());
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingComputeState->PrepareForDispatch(Cmd);
	VulkanRHI::vkCmdDispatch(CmdBuffer, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

	if (GCVarSubmitOnDispatch.GetValueOnRenderThread())
	{
		InternalSubmitActiveCmdBuffer();
	}

	// flush any needed buffers that the compute shader wrote to	
	if (bAutomaticFlushAfterComputeShader)
	{
		FlushAfterComputeShader();
	}

	if (FVulkanPlatform::RegisterGPUWork() && IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(1);
	}
}

void FVulkanCommandListContext::RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset) 
{
	static_assert(sizeof(FRHIDispatchIndirectParameters) == sizeof(VkDispatchIndirectCommand), "Dispatch indirect doesn't match!");
	FVulkanVertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);

	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	ensure(Cmd->IsOutsideRenderPass());
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingComputeState->PrepareForDispatch(Cmd);
	VulkanRHI::vkCmdDispatchIndirect(CmdBuffer, ArgumentBuffer->GetHandle(), ArgumentBuffer->GetOffset() + ArgumentOffset);

	if (GCVarSubmitOnDispatch.GetValueOnRenderThread())
	{
		InternalSubmitActiveCmdBuffer();
	}

	// flush any needed buffers that the compute shader wrote to	
	if (bAutomaticFlushAfterComputeShader)
	{
		FlushAfterComputeShader();
	}

	if (FVulkanPlatform::RegisterGPUWork()/* && IsImmediate()*/)
	{
		GpuProfiler.RegisterGPUWork(1);
	}
}

void FVulkanCommandListContext::RHISetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderStateRHI)
{
	check(0);
}


void FVulkanCommandListContext::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAVRHI)
{
	check(PendingComputeState->GetCurrentShader() == ResourceCast(ComputeShaderRHI));

	FVulkanUnorderedAccessView* UAV = ResourceCast(UAVRHI);
	PendingComputeState->SetUAV(UAVIndex, UAV);
	if (bAutomaticFlushAfterComputeShader && UAV)
	{
		PendingComputeState->AddUAVForAutoFlush(UAV);
	}
}

void FVulkanCommandListContext::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 UAVIndex,FUnorderedAccessViewRHIParamRef UAVRHI, uint32 InitialCount)
{
	check(PendingComputeState->GetCurrentShader() == ResourceCast(ComputeShaderRHI));

	FVulkanUnorderedAccessView* UAV = ResourceCast(UAVRHI);
	ensure(0);
}


void FVulkanCommandListContext::RHISetShaderTexture(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Vertex) == ResourceCast(VertexShaderRHI));
	FVulkanTextureBase* Texture = GetVulkanTextureFromRHITexture(NewTextureRHI);
	VkImageLayout Layout = GetLayoutForDescriptor(Texture->Surface);
	PendingGfxState->SetTexture(DescriptorSet::Vertex, TextureIndex, Texture, Layout);
}

void FVulkanCommandListContext::RHISetShaderTexture(FHullShaderRHIParamRef HullShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	ensureMsgf(0, TEXT("Tessellation not supported yet!"));
/*
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Hull) == ResourceCast(HullShaderRHI));
	FVulkanTextureBase* Texture = GetVulkanTextureFromRHITexture(NewTextureRHI);
	VkImageLayout Layout = GetLayoutForDescriptor(Texture->Surface);
	PendingGfxState->SetTexture(DescriptorSet::Hull, TextureIndex, Texture, Layout);
*/
}

void FVulkanCommandListContext::RHISetShaderTexture(FDomainShaderRHIParamRef DomainShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	ensureMsgf(0, TEXT("Tessellation not supported yet!"));
/*
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Domain) == ResourceCast(DomainShaderRHI));
	FVulkanTextureBase* Texture = GetVulkanTextureFromRHITexture(NewTextureRHI);
	VkImageLayout Layout = GetLayoutForDescriptor(Texture->Surface);
	PendingGfxState->SetTexture(DescriptorSet::Domain, TextureIndex, Texture, Layout);
*/
}

void FVulkanCommandListContext::RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Geometry) == ResourceCast(GeometryShaderRHI));
	FVulkanTextureBase* Texture = GetVulkanTextureFromRHITexture(NewTextureRHI);
	VkImageLayout Layout = GetLayoutForDescriptor(Texture->Surface);
	PendingGfxState->SetTexture(DescriptorSet::Geometry, TextureIndex, Texture, Layout);
}

void FVulkanCommandListContext::RHISetShaderTexture(FPixelShaderRHIParamRef PixelShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Pixel) == ResourceCast(PixelShaderRHI));
	FVulkanTextureBase* Texture = GetVulkanTextureFromRHITexture(NewTextureRHI);
	VkImageLayout Layout = GetLayoutForDescriptor(Texture->Surface);
	PendingGfxState->SetTexture(DescriptorSet::Pixel, TextureIndex, Texture, Layout);
}

void FVulkanCommandListContext::RHISetShaderTexture(FComputeShaderRHIParamRef ComputeShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	check(PendingComputeState->GetCurrentShader() == ResourceCast(ComputeShader));

	FVulkanTextureBase* VulkanTexture = GetVulkanTextureFromRHITexture(NewTextureRHI);
	VkImageLayout Layout = GetLayoutForDescriptor(VulkanTexture->Surface);
	PendingComputeState->SetTexture(TextureIndex, VulkanTexture, Layout);
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Vertex) == ResourceCast(VertexShaderRHI));
	FVulkanShaderResourceView* SRV = ResourceCast(SRVRHI);
	PendingGfxState->SetSRV(DescriptorSet::Vertex, TextureIndex, SRV);
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FHullShaderRHIParamRef HullShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	ensureMsgf(0, TEXT("Tessellation not supported yet!"));
/*
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Hull) == ResourceCast(HullShaderRHI));
	FVulkanShaderResourceView* SRV = ResourceCast(SRVRHI);
	PendingGfxState->SetSRV(DescriptorSet::Hull, TextureIndex, SRV);
*/
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FDomainShaderRHIParamRef DomainShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	ensureMsgf(0, TEXT("Tessellation not supported yet!"));
/*
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Domain) == ResourceCast(DomainShaderRHI));
	FVulkanShaderResourceView* SRV = ResourceCast(SRVRHI);
	PendingGfxState->SetSRV(DescriptorSet::Domain, TextureIndex, SRV);
*/
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FGeometryShaderRHIParamRef GeometryShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Geometry) == ResourceCast(GeometryShaderRHI));
	FVulkanShaderResourceView* SRV = ResourceCast(SRVRHI);
	PendingGfxState->SetSRV(DescriptorSet::Geometry, TextureIndex, SRV);
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Pixel) == ResourceCast(PixelShaderRHI));
	FVulkanShaderResourceView* SRV = ResourceCast(SRVRHI);
	PendingGfxState->SetSRV(DescriptorSet::Pixel, TextureIndex, SRV);
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	check(PendingComputeState->GetCurrentShader() == ResourceCast(ComputeShaderRHI));
	FVulkanShaderResourceView* SRV = ResourceCast(SRVRHI);
	PendingComputeState->SetSRV(TextureIndex, SRV);
}

void FVulkanCommandListContext::RHISetShaderSampler(FVertexShaderRHIParamRef VertexShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Vertex) == ResourceCast(VertexShaderRHI));
	FVulkanSamplerState* Sampler = ResourceCast(NewStateRHI);
	PendingGfxState->SetSamplerState(DescriptorSet::Vertex, SamplerIndex, Sampler);
}

void FVulkanCommandListContext::RHISetShaderSampler(FHullShaderRHIParamRef HullShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	ensureMsgf(0, TEXT("Tessellation not supported yet!"));
/*
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Hull) == ResourceCast(HullShaderRHI));
	FVulkanSamplerState* Sampler = ResourceCast(NewStateRHI);
	PendingGfxState->SetSamplerState(DescriptorSet::Hull, SamplerIndex, Sampler);
*/
}

void FVulkanCommandListContext::RHISetShaderSampler(FDomainShaderRHIParamRef DomainShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	ensureMsgf(0, TEXT("Tessellation not supported yet!"));
/*
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Domain) == ResourceCast(DomainShaderRHI));
	FVulkanSamplerState* Sampler = ResourceCast(NewStateRHI);
	PendingGfxState->SetSamplerState(DescriptorSet::Domain, SamplerIndex, Sampler);
*/
}

void FVulkanCommandListContext::RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Geometry) == ResourceCast(GeometryShaderRHI));
	FVulkanSamplerState* Sampler = ResourceCast(NewStateRHI);
	PendingGfxState->SetSamplerState(DescriptorSet::Geometry, SamplerIndex, Sampler);
}

void FVulkanCommandListContext::RHISetShaderSampler(FPixelShaderRHIParamRef PixelShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Pixel) == ResourceCast(PixelShaderRHI));
	FVulkanSamplerState* Sampler = ResourceCast(NewStateRHI);
	PendingGfxState->SetSamplerState(DescriptorSet::Pixel, SamplerIndex, Sampler);
}

void FVulkanCommandListContext::RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	check(PendingComputeState->GetCurrentShader() == ResourceCast(ComputeShaderRHI));
	FVulkanSamplerState* Sampler = ResourceCast(NewStateRHI);
	PendingComputeState->SetSamplerState(SamplerIndex, Sampler);
}

void FVulkanCommandListContext::RHISetShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Vertex) == ResourceCast(VertexShaderRHI));

	PendingGfxState->SetShaderParameter(DescriptorSet::Vertex, BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FVulkanCommandListContext::RHISetShaderParameter(FHullShaderRHIParamRef HullShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	ensureMsgf(0, TEXT("Tessellation not supported yet!"));
/*
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Hull) == ResourceCast(HullShaderRHI));

	PendingGfxState->SetShaderParameter(DescriptorSet::Hull, BufferIndex, BaseIndex, NumBytes, NewValue);
*/
}

void FVulkanCommandListContext::RHISetShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	ensureMsgf(0, TEXT("Tessellation not supported yet!"));
/*
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Domain) == ResourceCast(DomainShaderRHI));

	PendingGfxState->SetShaderParameter(DescriptorSet::Domain, BufferIndex, BaseIndex, NumBytes, NewValue);
*/
}

void FVulkanCommandListContext::RHISetShaderParameter(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Geometry) == ResourceCast(GeometryShaderRHI));

	PendingGfxState->SetShaderParameter(DescriptorSet::Geometry, BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FVulkanCommandListContext::RHISetShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	check(PendingGfxState->CurrentBSS && PendingGfxState->CurrentBSS->GetShader(DescriptorSet::Pixel) == ResourceCast(PixelShaderRHI));

	PendingGfxState->SetShaderParameter(DescriptorSet::Pixel, BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FVulkanCommandListContext::RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	check(PendingComputeState->GetCurrentShader() == ResourceCast(ComputeShaderRHI));

	PendingComputeState->SetShaderParameter(BufferIndex, BaseIndex, NumBytes, NewValue);
}

struct FSrtResourceBinding
{
	typedef TRefCountPtr<FRHIResource> ResourceRef;

	FSrtResourceBinding(): BindingIndex(-1), Resource(nullptr) {}
	FSrtResourceBinding(int32 InBindingIndex, FRHIResource* InResource): BindingIndex(InBindingIndex), Resource(InResource) {}

	int32 BindingIndex;
	ResourceRef Resource;
};


typedef TArray<FSrtResourceBinding, TInlineAllocator<16>> FResourceBindingArray;

static void GatherUniformBufferResources(
	const TArray<uint32>& InBindingArray,
	const uint32& InBindingMask,
	const FVulkanUniformBuffer* UniformBuffer,
	uint32 BufferIndex,
	FResourceBindingArray& OutResourcesBindings)
{
	check(UniformBuffer);

	if (!((1 << BufferIndex) & InBindingMask))
	{
		return;
	}

	const TArray<TRefCountPtr<FRHIResource>>& ResourceArray = UniformBuffer->GetResourceTable();

	// Expected to get an empty array
	check(OutResourcesBindings.Num() == 0);

	// Verify mask and array correlational validity
	check(InBindingMask == 0 ? (InBindingArray.Num() == 0) : (InBindingArray.Num() > 0));

	// InBindingArray contains index to the buffer offset and also buffer offsets
	uint32 BufferOffset = InBindingArray[BufferIndex];
	const uint32* ResourceInfos = &InBindingArray[BufferOffset];
	uint32 ResourceInfo = *ResourceInfos++;

	// The mask check at the top of this function does not appear to replace this check completely.  The mask only tells you if data exists for a given descriptor set; doesn't tell you what kind of data exists.
	// Because different data types are stored in different arrays, it is possible to end up trying to parse the token stream for one array solely because another array happened to have valid data in it 
	// for the current descriptors.  Bad things can (and do) result - like trying to push a sampler resource as if it were a texture resource.
	if (BufferOffset > 0)
	{
		// Extract all resources related to the current BufferIndex
		do
		{
			// Verify that we have correct buffer index
			check(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);

			// Extract binding index from ResourceInfo
			const uint32 BindingIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			// Extract index of the resource stored in the resource table from ResourceInfo
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);

			if (ResourceIndex < ResourceArray.Num())
			{
				check(ResourceArray[ResourceIndex]);
				OutResourcesBindings.Add(FSrtResourceBinding(BindingIndex, ResourceArray[ResourceIndex]));
			}

			// Iterate to next info
			ResourceInfo = *ResourceInfos++;
		}
		while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}
}

inline void FVulkanCommandListContext::SetShaderUniformBuffer(DescriptorSet::EStage Stage, const FVulkanUniformBuffer* UniformBuffer, int32 BindingIndex, const FVulkanShader* Shader)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanSetUniformBufferTime);
#endif
	check(Shader == PendingGfxState->CurrentBSS->GetShader(Stage));
	if (UniformBuffer->GetLayout().ConstantBufferSize > 0)
	{
		if (UseRealUBs())
		{
			PendingGfxState->SetUniformBuffer(Stage, BindingIndex, UniformBuffer);
		}
		else
		{
			PendingGfxState->SetUniformBufferConstantData(Stage, BindingIndex, UniformBuffer->ConstantData);
		}
	}

	const FShaderCompilerResourceTable& ResourceBindingTable = Shader->GetCodeHeader().SerializedBindings.ShaderResourceTable;
	if (ResourceBindingTable.ResourceTableLayoutHashes.Num() == 0)
	{
		return;
	}

	// Uniform Buffers
	float CurrentTime = 0.0f;

	if (ResourceBindingTable.TextureMap.Num() != 0)
	{
		// Gather texture bindings from the SRT table
		FResourceBindingArray TextureBindings;
		GatherUniformBufferResources(ResourceBindingTable.TextureMap, ResourceBindingTable.ResourceTableBits, UniformBuffer, BindingIndex, TextureBindings);
		CurrentTime = (float)FApp::GetCurrentTime();
		for (int32 Index = 0; Index < TextureBindings.Num(); Index++)
		{
			const FSrtResourceBinding& CurrTextureBinding = TextureBindings[Index];
			FTextureRHIParamRef TexRef = (FTextureRHIParamRef)CurrTextureBinding.Resource.GetReference();
			const FVulkanTextureBase* BaseTexture = FVulkanTextureBase::Cast(TexRef);
			if (BaseTexture)
			{
				VkImageLayout Layout = GetLayoutForDescriptor(BaseTexture->Surface);
				PendingGfxState->SetTexture(Stage, CurrTextureBinding.BindingIndex, BaseTexture, Layout);
				TexRef->SetLastRenderTime(CurrentTime);
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Invalid texture in SRT table for shader '%s'"), *Shader->DebugName);
			}
		}
	}

	FResourceBindingArray SRVBindings;
	if (ResourceBindingTable.ShaderResourceViewMap.Num() != 0)
	{
		GatherUniformBufferResources(ResourceBindingTable.ShaderResourceViewMap, ResourceBindingTable.ResourceTableBits, UniformBuffer, BindingIndex, SRVBindings);
		if (CurrentTime == 0.0f)
		{
			CurrentTime = (float)FApp::GetCurrentTime();
		}
		for (int32 Index = 0; Index < SRVBindings.Num(); Index++)
		{
			const FSrtResourceBinding* CurrSRVBinding = &SRVBindings[Index];
			FRHIShaderResourceView* CurrentSRV = static_cast<FRHIShaderResourceView*>(CurrSRVBinding->Resource.GetReference());
			if (CurrentSRV)
			{
				FVulkanShaderResourceView* SRV = ResourceCast(CurrentSRV);
				PendingGfxState->SetSRV(Stage, CurrSRVBinding->BindingIndex, SRV);
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Invalid SRV in SRT table for shader '%s'"), *Shader->DebugName);
			}
		}
	}

	// Gather Sampler bindings from the SRT table
	FResourceBindingArray SamplerBindings;
	if (ResourceBindingTable.SamplerMap.Num() != 0)
	{
		GatherUniformBufferResources(ResourceBindingTable.SamplerMap, ResourceBindingTable.ResourceTableBits, UniformBuffer, BindingIndex, SamplerBindings);
		if (CurrentTime == 0.0f)
		{
			CurrentTime = (float)FApp::GetCurrentTime();
		}
		for (int32 Index = 0; Index < SamplerBindings.Num(); Index++)
		{
			const FSrtResourceBinding* CurrSamplerBinding = &SamplerBindings[Index];
			FVulkanSamplerState* CurrSampler = static_cast<FVulkanSamplerState*>(CurrSamplerBinding->Resource.GetReference());
			if (CurrSampler)
			{
				if (CurrSampler->Sampler)
				{
					PendingGfxState->SetSamplerState(Stage, CurrSamplerBinding->BindingIndex, CurrSampler);
				}
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Invalid sampler in SRT table for shader '%s'"), *Shader->DebugName);
			}
		}
	}
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FVertexShaderRHIParamRef VertexShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);
	SetShaderUniformBuffer(DescriptorSet::Vertex, UniformBuffer, BufferIndex, ResourceCast(VertexShaderRHI));
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FHullShaderRHIParamRef HullShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	ensureMsgf(0, TEXT("Tessellation not supported yet!"));
/*
	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);
	SetShaderUniformBuffer(DescriptorSet::Hull, UniformBuffer, BufferIndex, ResourceCast(HullShaderRHI));
*/
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FDomainShaderRHIParamRef DomainShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	ensureMsgf(0, TEXT("Tessellation not supported yet!"));
/*
	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);
	SetShaderUniformBuffer(DescriptorSet::Domain, UniformBuffer, BufferIndex, ResourceCast(DomainShaderRHI));
*/
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);
	SetShaderUniformBuffer(DescriptorSet::Geometry, UniformBuffer, BufferIndex, ResourceCast(GeometryShaderRHI));
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);
	SetShaderUniformBuffer(DescriptorSet::Pixel, UniformBuffer, BufferIndex, ResourceCast(PixelShaderRHI));
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	check(PendingComputeState->GetCurrentShader() == ResourceCast(ComputeShaderRHI));

#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanSetUniformBufferTime);
#endif
	FVulkanComputePipelineDescriptorState& State = *PendingComputeState->CurrentState;

	// Walk through all resources to set all appropriate states
	FVulkanComputeShader* Shader = ResourceCast(ComputeShaderRHI);

	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);
	
	// Uniform Buffers
	if (UniformBuffer->GetLayout().ConstantBufferSize > 0)
	{
		if (UseRealUBs())
		{
			State.SetUniformBuffer(BufferIndex, UniformBuffer);
		}
		else
		{
			State.SetUniformBufferConstantData(BufferIndex, UniformBuffer->ConstantData);
		}
	}

	const FShaderCompilerResourceTable& ResourceBindingTable = Shader->CodeHeader.SerializedBindings.ShaderResourceTable;
	if (ResourceBindingTable.ResourceTableLayoutHashes.Num() == 0)
	{
		return;
	}

	float CurrentTime = 0.0f;
	// Gather texture bindings from the SRT table
	if (ResourceBindingTable.TextureMap.Num() != 0)
	{
		FResourceBindingArray TextureBindings;
		GatherUniformBufferResources(ResourceBindingTable.TextureMap, ResourceBindingTable.ResourceTableBits, UniformBuffer, BufferIndex, TextureBindings);
		CurrentTime = (float)FApp::GetCurrentTime();
		for (int32 Index = 0; Index < TextureBindings.Num(); Index++)
		{
			const FSrtResourceBinding& CurrTextureBinding = TextureBindings[Index];
			FTextureRHIParamRef TexRef = (FTextureRHIParamRef)CurrTextureBinding.Resource.GetReference();
			const FVulkanTextureBase* BaseTexture = FVulkanTextureBase::Cast(TexRef);
			if (BaseTexture)
			{
				VkImageLayout Layout = GetLayoutForDescriptor(BaseTexture->Surface);
				State.SetTexture(CurrTextureBinding.BindingIndex, BaseTexture, Layout);
				TexRef->SetLastRenderTime(CurrentTime);
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Invalid texture in SRT table for shader '%s'"), *Shader->DebugName);
			}
		}
	}

	if (ResourceBindingTable.ShaderResourceViewMap.Num() != 0)
	{
		FResourceBindingArray SRVBindings;
		GatherUniformBufferResources(ResourceBindingTable.ShaderResourceViewMap, ResourceBindingTable.ResourceTableBits, UniformBuffer, BufferIndex, SRVBindings);
		if (CurrentTime == 0.0f)
		{
			CurrentTime = (float)FApp::GetCurrentTime();
		}
		for (int32 Index = 0; Index < SRVBindings.Num(); Index++)
		{
			const FSrtResourceBinding* CurrSRVBinding = &SRVBindings[Index];
			FRHIShaderResourceView* CurrentSRV = static_cast<FRHIShaderResourceView*>(CurrSRVBinding->Resource.GetReference());
			if (CurrentSRV)
			{
				FVulkanShaderResourceView* SRV = ResourceCast(CurrentSRV);
				PendingComputeState->SetSRV(CurrSRVBinding->BindingIndex, SRV);
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Invalid SRV in SRT table for shader '%s'"), *Shader->DebugName);
			}
		}
	}

	// Gather Sampler bindings from the SRT table
	FResourceBindingArray SamplerBindings;
	if (ResourceBindingTable.SamplerMap.Num() != 0)
	{
		GatherUniformBufferResources(ResourceBindingTable.SamplerMap, ResourceBindingTable.ResourceTableBits, UniformBuffer, BufferIndex, SamplerBindings);
		if (CurrentTime == 0.0f)
		{
			CurrentTime = (float)FApp::GetCurrentTime();
		}
		for (int32 Index = 0; Index < SamplerBindings.Num(); Index++)
		{
			const FSrtResourceBinding* CurrSamplerBinding = &SamplerBindings[Index];
			FVulkanSamplerState* CurrSampler = static_cast<FVulkanSamplerState*>(CurrSamplerBinding->Resource.GetReference());
			if (CurrSampler)
			{
				State.SetSamplerState(CurrSamplerBinding->BindingIndex, CurrSampler);
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Invalid sampler in SRT table for shader '%s'"), *Shader->DebugName);
			}
		}
	}
}

void FVulkanCommandListContext::RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewStateRHI, uint32 StencilRef)
{
	check(0);
}

void FVulkanCommandListContext::RHISetBlendState(FBlendStateRHIParamRef NewStateRHI, const FLinearColor& BlendFactor)
{
	check(0);
}

void FVulkanCommandListContext::RHISetStencilRef(uint32 StencilRef)
{
	PendingGfxState->SetStencilRef(StencilRef);
}

void FVulkanCommandListContext::RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
#endif
	RHI_DRAW_CALL_STATS(PrimitiveType, NumInstances*NumPrimitives);

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	PendingGfxState->PrepareForDraw(CmdBuffer);
	NumInstances = FMath::Max(1U, NumInstances);
	uint32 NumVertices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);
	VulkanRHI::vkCmdDraw(CmdBuffer->GetHandle(), NumVertices, NumInstances, BaseVertexIndex, 0);

	if (FVulkanPlatform::RegisterGPUWork() && IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(NumPrimitives * NumInstances, NumVertices * NumInstances);
	}
}

void FVulkanCommandListContext::RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
	static_assert(sizeof(FRHIDrawIndirectParameters) == sizeof(VkDrawIndirectCommand), "Draw indirect doesn't match!");

#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
#endif
	RHI_DRAW_CALL_INC();

	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingGfxState->PrepareForDraw(Cmd);

	FVulkanVertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);

	VulkanRHI::vkCmdDrawIndirect(CmdBuffer, ArgumentBuffer->GetHandle(), ArgumentBuffer->GetOffset() + ArgumentOffset, 1, sizeof(VkDrawIndirectCommand));

	if (FVulkanPlatform::RegisterGPUWork() && IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(1);
	}
}

void FVulkanCommandListContext::RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance,
	uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
#endif
	RHI_DRAW_CALL_STATS(PrimitiveType, NumInstances*NumPrimitives);
	checkf(GRHISupportsFirstInstance || FirstInstance == 0, TEXT("FirstInstance must be 0, see GRHISupportsFirstInstance"));

	FVulkanIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingGfxState->PrepareForDraw(Cmd);
	VulkanRHI::vkCmdBindIndexBuffer(CmdBuffer, IndexBuffer->GetHandle(), IndexBuffer->GetOffset(), IndexBuffer->GetIndexType());

	uint32 NumIndices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);
	NumInstances = FMath::Max(1U, NumInstances);
	VulkanRHI::vkCmdDrawIndexed(CmdBuffer, NumIndices, NumInstances, StartIndex, BaseVertexIndex, FirstInstance);

	if (FVulkanPlatform::RegisterGPUWork() && IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(NumPrimitives * NumInstances, NumVertices * NumInstances);
	}
}

void FVulkanCommandListContext::RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
#endif
	RHI_DRAW_CALL_INC();

	FVulkanIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingGfxState->PrepareForDraw(Cmd);
	VulkanRHI::vkCmdBindIndexBuffer(CmdBuffer, IndexBuffer->GetHandle(), IndexBuffer->GetOffset(), IndexBuffer->GetIndexType());

	FVulkanStructuredBuffer* ArgumentBuffer = ResourceCast(ArgumentsBufferRHI);
	VulkanRHI::vkCmdDrawIndexedIndirect(CmdBuffer, ArgumentBuffer->GetHandle(), ArgumentBuffer->GetOffset() + DrawArgumentsIndex * sizeof(VkDrawIndexedIndirectCommand), NumInstances, sizeof(VkDrawIndexedIndirectCommand));

	if (FVulkanPlatform::RegisterGPUWork() && IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(1);
	}
}

void FVulkanCommandListContext::RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FIndexBufferRHIParamRef IndexBufferRHI,FVertexBufferRHIParamRef ArgumentBufferRHI,uint32 ArgumentOffset)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
#endif
	RHI_DRAW_CALL_INC();

	FVulkanIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingGfxState->PrepareForDraw(Cmd);
	VulkanRHI::vkCmdBindIndexBuffer(CmdBuffer, IndexBuffer->GetHandle(), IndexBuffer->GetOffset(), IndexBuffer->GetIndexType());

	FVulkanVertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);

	VulkanRHI::vkCmdDrawIndexedIndirect(CmdBuffer, ArgumentBuffer->GetHandle(), ArgumentBuffer->GetOffset() + ArgumentOffset, 1, sizeof(VkDrawIndexedIndirectCommand));

	if (FVulkanPlatform::RegisterGPUWork() && IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(1); 
	}
}

void FVulkanCommandListContext::RHIBeginDrawPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUPPrepTime);
#endif

//	checkSlow(GPendingDrawPrimitiveUPVertexData == nullptr);

	TempFrameAllocationBuffer.Alloc(VertexDataStride * NumVertices, VertexDataStride, UserPrimitive.VertexAllocInfo);
	OutVertexData = UserPrimitive.VertexAllocInfo.Data;

	UserPrimitive.PrimitiveType = PrimitiveType;
	UserPrimitive.NumPrimitives = NumPrimitives;
	UserPrimitive.NumVertices = NumVertices;
	UserPrimitive.VertexDataStride = VertexDataStride;
}


void FVulkanCommandListContext::RHIEndDrawPrimitiveUP()
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
#endif
	RHI_DRAW_CALL_STATS(UserPrimitive.PrimitiveType, UserPrimitive.NumPrimitives);
	PendingGfxState->SetStreamSource(0, UserPrimitive.VertexAllocInfo.GetHandle(), UserPrimitive.VertexAllocInfo.GetBindOffset());
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	PendingGfxState->PrepareForDraw(CmdBuffer);
	VkCommandBuffer Cmd = CmdBuffer->GetHandle();
	VulkanRHI::vkCmdDraw(CmdBuffer->GetHandle(), UserPrimitive.NumVertices, 1, UserPrimitive.MinVertexIndex, 0);

	if (FVulkanPlatform::RegisterGPUWork() && IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(UserPrimitive.NumPrimitives, UserPrimitive.NumVertices);
	}
}

void FVulkanCommandListContext::RHIBeginDrawIndexedPrimitiveUP( uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride,
	void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUPPrepTime);
#endif

	TempFrameAllocationBuffer.Alloc(VertexDataStride * NumVertices, IndexDataStride, UserPrimitive.VertexAllocInfo);
	OutVertexData = UserPrimitive.VertexAllocInfo.Data;

	check(IndexDataStride == 2 || IndexDataStride == 4);
	UserPrimitive.IndexType = (IndexDataStride == 2) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
	TempFrameAllocationBuffer.Alloc(IndexDataStride * NumIndices, IndexDataStride, UserPrimitive.IndexAllocInfo);
	OutIndexData = UserPrimitive.IndexAllocInfo.Data;

	UserPrimitive.PrimitiveType = PrimitiveType;
	UserPrimitive.NumPrimitives = NumPrimitives;
	UserPrimitive.MinVertexIndex = MinVertexIndex;
	UserPrimitive.IndexDataStride = IndexDataStride;

	UserPrimitive.NumVertices = NumVertices;
	UserPrimitive.VertexDataStride = VertexDataStride;
}

void FVulkanCommandListContext::RHIEndDrawIndexedPrimitiveUP()
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
#endif
	RHI_DRAW_CALL_STATS(UserPrimitive.PrimitiveType, UserPrimitive.NumPrimitives);
	PendingGfxState->SetStreamSource(0, UserPrimitive.VertexAllocInfo.GetHandle(), UserPrimitive.VertexAllocInfo.GetBindOffset());
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	PendingGfxState->PrepareForDraw(CmdBuffer);
	VkCommandBuffer Cmd = CmdBuffer->GetHandle();
	uint32 NumIndices = GetVertexCountForPrimitiveCount(UserPrimitive.NumPrimitives, UserPrimitive.PrimitiveType);
	VulkanRHI::vkCmdBindIndexBuffer(Cmd, UserPrimitive.IndexAllocInfo.GetHandle(), UserPrimitive.IndexAllocInfo.GetBindOffset(), UserPrimitive.IndexType);
	VulkanRHI::vkCmdDrawIndexed(Cmd, NumIndices, 1, UserPrimitive.MinVertexIndex, 0, 0);

	if (FVulkanPlatform::RegisterGPUWork() && IsImmediate())
	{
		GpuProfiler.RegisterGPUWork(UserPrimitive.NumPrimitives, UserPrimitive.NumVertices);
	}
}

void FVulkanCommandListContext::RHIClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	if (!(bClearColor || bClearDepth || bClearStencil))
	{
		return;
	}

	check(bClearColor ? NumClearColors > 0 : true);

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	//FRCLog::Printf(TEXT("RHIClearMRT"));

	const uint32 NumColorAttachments = TransitionAndLayoutManager.CurrentFramebuffer->GetNumColorAttachments();
	check(!bClearColor || (uint32)NumClearColors <= NumColorAttachments);
	InternalClearMRT(CmdBuffer, bClearColor, bClearColor ? NumClearColors : 0, ClearColorArray, bClearDepth, Depth, bClearStencil, Stencil);
}

void FVulkanCommandListContext::InternalClearMRT(FVulkanCmdBuffer* CmdBuffer, bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	if (TransitionAndLayoutManager.CurrentRenderPass)
	{
		const VkExtent2D& Extents = TransitionAndLayoutManager.CurrentRenderPass->GetLayout().GetExtent2D();
		VkClearRect Rect;
		FMemory::Memzero(Rect);
		Rect.rect.offset.x = 0;
		Rect.rect.offset.y = 0;
		Rect.rect.extent = Extents;

		VkClearAttachment Attachments[MaxSimultaneousRenderTargets + 1];
		FMemory::Memzero(Attachments);

		uint32 NumAttachments = NumClearColors;
		if (bClearColor)
		{
			for (int32 i = 0; i < NumClearColors; ++i)
			{
				Attachments[i].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				Attachments[i].colorAttachment = i;
				Attachments[i].clearValue.color.float32[0] = ClearColorArray[i].R;
				Attachments[i].clearValue.color.float32[1] = ClearColorArray[i].G;
				Attachments[i].clearValue.color.float32[2] = ClearColorArray[i].B;
				Attachments[i].clearValue.color.float32[3] = ClearColorArray[i].A;
			}
		}

		if (bClearDepth || bClearStencil)
		{
			Attachments[NumClearColors].aspectMask = bClearDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
			Attachments[NumClearColors].aspectMask |= bClearStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
			Attachments[NumClearColors].colorAttachment = 0;
			Attachments[NumClearColors].clearValue.depthStencil.depth = Depth;
			Attachments[NumClearColors].clearValue.depthStencil.stencil = Stencil;
			++NumAttachments;
		}

		VulkanRHI::vkCmdClearAttachments(CmdBuffer->GetHandle(), NumAttachments, Attachments, 1, &Rect);
	}
	else
	{
		ensure(0);
		//VulkanRHI::vkCmdClearColorImage(CmdBuffer->GetHandle(), )
	}
}

void FVulkanDynamicRHI::RHISuspendRendering()
{
}

void FVulkanDynamicRHI::RHIResumeRendering()
{
}

bool FVulkanDynamicRHI::RHIIsRenderingSuspended()
{
	return false;
}

void FVulkanDynamicRHI::RHIBlockUntilGPUIdle()
{
	Device->WaitUntilIdle();
}

uint32 FVulkanDynamicRHI::RHIGetGPUFrameCycles()
{
	return GGPUFrameTime;
}

void FVulkanCommandListContext::RHIAutomaticCacheFlushAfterComputeShader(bool bEnable)
{
	bAutomaticFlushAfterComputeShader = bEnable;
}

void FVulkanCommandListContext::RHIFlushComputeShaderCache()
{
	FlushAfterComputeShader();
}

void FVulkanDynamicRHI::RHIExecuteCommandList(FRHICommandList* CmdList)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHIEnableDepthBoundsTest(bool bEnable)
{
	check(0);
}

void FVulkanCommandListContext::RHISetDepthBounds(float MinDepth, float MaxDepth)
{
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	VulkanRHI::vkCmdSetDepthBounds(CmdBuffer->GetHandle(), MinDepth, MaxDepth);
}

void FVulkanCommandListContext::RequestSubmitCurrentCommands()
{
	if (Device->GetComputeQueue() == Queue)
	{
		if (CommandBufferManager->HasPendingUploadCmdBuffer())
		{
			CommandBufferManager->SubmitUploadCmdBuffer();
		}
		bSubmitAtNextSafePoint = true;
		SafePointSubmit();
	}
	else
	{
		ensure(IsImmediate());
		bSubmitAtNextSafePoint = true;
	}
}

void FVulkanCommandListContext::InternalSubmitActiveCmdBuffer()
{
	CommandBufferManager->SubmitActiveCmdBuffer();
	CommandBufferManager->PrepareForNewActiveCommandBuffer();
}

void FVulkanCommandListContext::PrepareForCPURead()
{
	ensure(IsImmediate());
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (CmdBuffer && CmdBuffer->HasBegun())
	{
		if (CmdBuffer->IsInsideRenderPass())
		{
			//#todo-rco: If we get real render passes then this is not needed
			TransitionAndLayoutManager.EndEmulatedRenderPass(CmdBuffer);
		}

		CommandBufferManager->SubmitActiveCmdBuffer();
		if (!GWaitForIdleOnSubmit)
		{
			// The wait has already happened if GWaitForIdleOnSubmit is set
			CommandBufferManager->WaitForCmdBuffer(CmdBuffer);
		}
	}
}

void FVulkanCommandListContext::RHISubmitCommandsHint()
{
	if (Device->IsRealAsyncComputeContext(this))
	{
		// Split the Immediate command buffer, so we can insert the semaphore
		FVulkanCommandListContext* ImmediateContext = &Device->GetImmediateContext();
		ensure(this != ImmediateContext);
		ImmediateContext->RHISubmitCommandsHint();

		// Now submit this compute context with a semaphore to the active cmd context
		VulkanRHI::FSemaphore* Semaphore = new VulkanRHI::FSemaphore(*Device);
		if (CommandBufferManager->HasPendingUploadCmdBuffer())
		{
			CommandBufferManager->SubmitUploadCmdBuffer();
		}
		CommandBufferManager->SubmitActiveCmdBuffer(Semaphore);

		ImmediateContext->GetCommandBufferManager()->GetActiveCmdBuffer()->AddWaitSemaphore(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, Semaphore);
	}
	else
	{
		RequestSubmitCurrentCommands();
		FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
		if (CmdBuffer && CmdBuffer->HasBegun() && CmdBuffer->IsOutsideRenderPass())
		{
			SafePointSubmit();
		}
		CommandBufferManager->RefreshFenceStatus();
	}
}

void FVulkanCommandListContext::FlushAfterComputeShader()
{
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	int32 NumResourcesToFlush = PendingComputeState->UAVListForAutoFlush.Num();
	if (NumResourcesToFlush > 0)
	{
		TArray<VkImageMemoryBarrier> ImageBarriers;
		TArray<VkBufferMemoryBarrier> BufferBarriers;
		for (FVulkanUnorderedAccessView* UAV : PendingComputeState->UAVListForAutoFlush)
		{
			if (UAV->SourceVertexBuffer)
			{
				VkBufferMemoryBarrier Barrier;
				VulkanRHI::SetupAndZeroBufferBarrier(Barrier, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, UAV->SourceVertexBuffer->GetHandle(), UAV->SourceVertexBuffer->GetOffset(), UAV->SourceVertexBuffer->GetSize());
				BufferBarriers.Add(Barrier);
			}
			else if (UAV->SourceStructuredBuffer)
			{
				VkBufferMemoryBarrier Barrier;
				VulkanRHI::SetupAndZeroBufferBarrier(Barrier, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, UAV->SourceStructuredBuffer->GetHandle(), UAV->SourceStructuredBuffer->GetOffset(), UAV->SourceStructuredBuffer->GetSize());
				BufferBarriers.Add(Barrier);
			}
			else if (UAV->SourceTexture)
			{
				FVulkanTextureBase* Texture = (FVulkanTextureBase*)UAV->SourceTexture->GetTextureBaseRHI();
				VkImageMemoryBarrier Barrier;
				VkImageLayout Layout = TransitionAndLayoutManager.FindOrAddLayout(Texture->Surface.Image, VK_IMAGE_LAYOUT_GENERAL);
				VulkanRHI::SetupAndZeroImageBarrierOLD(Barrier, Texture->Surface, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, Layout, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, Layout);
				ImageBarriers.Add(Barrier);
			}
			else if (UAV->SourceIndexBuffer)
			{
				VkBufferMemoryBarrier Barrier;
				VulkanRHI::SetupAndZeroBufferBarrier(Barrier, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, UAV->SourceIndexBuffer->GetHandle(), UAV->SourceIndexBuffer->GetOffset(), UAV->SourceIndexBuffer->GetSize());
				BufferBarriers.Add(Barrier);
			}
			else
			{
				ensure(0);
			}
		}
		VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, BufferBarriers.Num(), BufferBarriers.GetData(), ImageBarriers.Num(), ImageBarriers.GetData());
		PendingComputeState->UAVListForAutoFlush.SetNum(0, false);
	}
}


void FVulkanCommandListContext::PrepareParallelFromBase(const FVulkanCommandListContext& BaseContext)
{
	//#todo-rco: Temp
	TransitionAndLayoutManager.TempCopy(BaseContext.TransitionAndLayoutManager);
}


FVulkanCommandContextContainer::FVulkanCommandContextContainer(FVulkanDevice* InDevice)
	: VulkanRHI::FDeviceChild(InDevice)
	, CmdContext(nullptr)
{
	check(IsInRenderingThread());

	CmdContext = Device->AcquireDeferredContext();
}

IRHICommandContext* FVulkanCommandContextContainer::GetContext()
{
	//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("*** Thread %d GetContext() Container=%p\n"), FPlatformTLS::GetCurrentThreadId(), this);
	//FPlatformTLS::SetTlsValue(GGnmManager.GetParallelTranslateTLS(), (void*)1);

	CmdContext->PrepareParallelFromBase(Device->GetImmediateContext());

	FVulkanCommandBufferManager* CmdMgr = CmdContext->GetCommandBufferManager();
	FVulkanCmdBuffer* CmdBuffer = CmdMgr->GetActiveCmdBuffer();
	if (!CmdBuffer)
	{
		CmdMgr->PrepareForNewActiveCommandBuffer();
		CmdBuffer = CmdMgr->GetActiveCmdBuffer();
	}
	else if (CmdBuffer->IsInsideRenderPass())
	{
		CmdContext->TransitionAndLayoutManager.EndEmulatedRenderPass(CmdBuffer);
	}
	else if (CmdBuffer->IsSubmitted())
	{
		CmdMgr->PrepareForNewActiveCommandBuffer();
		CmdBuffer = CmdMgr->GetActiveCmdBuffer();
	}
	if (!CmdBuffer->HasBegun())
	{
		CmdBuffer->Begin();
	}

	CmdContext->RHIPushEvent(TEXT("Parallel Context"), FColor::Blue);

	//CmdContext->InitContextBuffers();
	//CmdContext->ClearState();
	return CmdContext;
}


void FVulkanCommandContextContainer::FinishContext()
{
	//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("*** Thread %d FinishContext() Container=%p\n"), FPlatformTLS::GetCurrentThreadId(), this);

	//GGnmManager.TimeSubmitOnCmdListEnd(CmdContext);

	//store off all memory ranges for DCBs to be submitted to the GPU.
	//FinalCommandList = CmdContext->GetContext().Finalize(CmdContext->GetBeginCmdListTimestamp(), CmdContext->GetEndCmdListTimestamp());

	FVulkanCommandBufferManager* CmdMgr = CmdContext->GetCommandBufferManager();
	FVulkanCmdBuffer* CmdBuffer = CmdMgr->GetActiveCmdBuffer();
	if (CmdBuffer->IsInsideRenderPass())
	{
		CmdContext->TransitionAndLayoutManager.EndEmulatedRenderPass(CmdBuffer);
	}
	check(CmdBuffer->HasBegun());

	CmdContext->RHIPopEvent();

	//CmdContext = nullptr;
	//CmdContext->CommandBufferManager->GetActiveCmdBuffer()->End();
	//check(!CmdContext/* && FinalCommandList.SubmissionAddrs.Num() > 0*/);

	//FPlatformTLS::SetTlsValue(GGnmManager.GetParallelTranslateTLS(), (void*)0);
}

void FVulkanCommandContextContainer::SubmitAndFreeContextContainer(int32 Index, int32 Num)
{
	//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("*** Thread %d Submit() Container=%p %d/%d\n"), FPlatformTLS::GetCurrentThreadId(), this, Index, Num);
	if (!Index)
	{
		FVulkanCommandListContext& Imm = Device->GetImmediateContext();
		FVulkanCommandBufferManager* ImmCmdMgr = Imm.GetCommandBufferManager();
		FVulkanCmdBuffer* ImmCmdBuf = ImmCmdMgr->GetActiveCmdBuffer();
		if (ImmCmdBuf && !ImmCmdBuf->IsSubmitted())
		{
			if (ImmCmdBuf->IsInsideRenderPass())
			{
				Imm.TransitionAndLayoutManager.EndEmulatedRenderPass(ImmCmdBuf);
			}
			ImmCmdMgr->SubmitActiveCmdBuffer();
		}
	}
	//GGnmManager.AddSubmission(FinalCommandList);
	check(CmdContext);
	FVulkanCommandBufferManager* CmdBufMgr = CmdContext->GetCommandBufferManager();
	check(!CmdBufMgr->HasPendingUploadCmdBuffer());
	//{
	//	CmdBufMgr->SubmitUploadCmdBuffer(false);
	//}
	FVulkanCmdBuffer* CmdBuffer = CmdBufMgr->GetActiveCmdBuffer();
	check(!CmdBuffer->IsInsideRenderPass());
	//{
	//	CmdContext->TransitionState.EndRenderPass(CmdBuffer);
	//}
	CmdBufMgr->SubmitActiveCmdBuffer();

	Device->ReleaseDeferredContext(CmdContext);

	//check(!CmdContext/* && FinalCommandList.SubmissionAddrs.Num() != 0*/);
	if (Index == Num - 1)
	{
		FVulkanCommandListContext& Imm = Device->GetImmediateContext();
		FVulkanCommandBufferManager* ImmCmdMgr = Imm.GetCommandBufferManager();
		FVulkanCmdBuffer* ImmCmdBuf = ImmCmdMgr->GetActiveCmdBuffer();
		if (ImmCmdBuf)
		{
			if (ImmCmdBuf->IsSubmitted())
			{
				ImmCmdMgr->PrepareForNewActiveCommandBuffer();
				ImmCmdBuf = ImmCmdMgr->GetActiveCmdBuffer();
			}
		}
		else
		{
			ImmCmdMgr->PrepareForNewActiveCommandBuffer();
			ImmCmdBuf = ImmCmdMgr->GetActiveCmdBuffer();
		}
		check(ImmCmdBuf->HasBegun());

		//printf("EndParallelContexts: %i, %i\n", Index, Num);
		//GGnmManager.EndParallelContexts();
	}
	//FinalCommandList.Reset();
	delete this;
}

void* FVulkanCommandContextContainer::operator new(size_t Size)
{
	return FMemory::Malloc(Size);
}

void FVulkanCommandContextContainer::operator delete(void* RawMemory)
{
	FMemory::Free(RawMemory);
}
