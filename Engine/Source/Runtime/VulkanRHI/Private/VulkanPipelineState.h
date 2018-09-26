// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanPipelineState.h: Vulkan pipeline state definitions.
=============================================================================*/

#pragma once

#include "VulkanConfiguration.h"
#include "VulkanMemory.h"
#include "VulkanCommandBuffer.h"
#include "VulkanDescriptorSets.h"
#include "VulkanGlobalUniformBuffer.h"
#include "VulkanPipeline.h"
#include "VulkanRHIPrivate.h"
#include "Containers/ArrayView.h"

class FVulkanComputePipeline;
extern TAutoConsoleVariable<int32> GDynamicGlobalUBs;


// Common Pipeline state
class FVulkanCommonPipelineDescriptorState : public VulkanRHI::FDeviceChild
{
public:
	FVulkanCommonPipelineDescriptorState(FVulkanDevice* InDevice)
		: VulkanRHI::FDeviceChild(InDevice)
	{
	}

	virtual ~FVulkanCommonPipelineDescriptorState() {}

	template<VkDescriptorType Type>
	inline void MarkDirty(uint8 DescriptorSet, bool bDirty)
	{
		ResourcesDirtyPerSet[DescriptorSet] |= ((uint64)(bDirty ? 1 : 0)) << (uint64)Type;
	}

	inline void SetStorageBuffer(uint8 DescriptorSet, uint32 BindingIndex, VkBuffer Buffer, uint32 Offset, uint32 Size, VkBufferUsageFlags UsageFlags)
	{
		check((UsageFlags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) == VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		const bool bDirty = DSWriter[DescriptorSet].WriteStorageBuffer(BindingIndex, Buffer, Offset, Size);
		MarkDirty<VK_DESCRIPTOR_TYPE_STORAGE_BUFFER>(DescriptorSet, bDirty);
	}

	inline void SetUAVTexelBufferViewState(uint8 DescriptorSet, uint32 BindingIndex, FVulkanBufferView* View)
	{
		check(View && (View->Flags & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT) == VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
		const bool bDirty = DSWriter[DescriptorSet].WriteStorageTexelBuffer(BindingIndex, View);
		MarkDirty<VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER>(DescriptorSet, bDirty);
	}

	inline void SetUAVTextureView(uint8 DescriptorSet, uint32 BindingIndex, const FVulkanTextureView& TextureView, VkImageLayout Layout)
	{
		const bool bDirty = DSWriter[DescriptorSet].WriteStorageImage(BindingIndex, TextureView.View, Layout);
		MarkDirty<VK_DESCRIPTOR_TYPE_STORAGE_IMAGE>(DescriptorSet, bDirty);
	}

	inline void SetTexture(uint8 DescriptorSet, uint32 BindingIndex, const FVulkanTextureBase* TextureBase, VkImageLayout Layout)
	{
		check(TextureBase);
		const bool bDirty = DSWriter[DescriptorSet].WriteImage(BindingIndex, TextureBase->PartialView->View, Layout);
		MarkDirty<VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE>(DescriptorSet, bDirty);
	}

	inline void SetSRVBufferViewState(uint8 DescriptorSet, uint32 BindingIndex, FVulkanBufferView* View)
	{
		check(View && (View->Flags & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) == VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
		const bool bDirty = DSWriter[DescriptorSet].WriteUniformTexelBuffer(BindingIndex, View);
		MarkDirty<VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER>(DescriptorSet, bDirty);
	}

	inline void SetSRVTextureView(uint8 DescriptorSet, uint32 BindingIndex, const FVulkanTextureView& TextureView, VkImageLayout Layout)
	{
		const bool bDirty = DSWriter[DescriptorSet].WriteImage(BindingIndex, TextureView.View, Layout);
		MarkDirty<VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE>(DescriptorSet, bDirty);
	}

	inline void SetSamplerState(uint8 DescriptorSet, uint32 BindingIndex, FVulkanSamplerState* Sampler)
	{
		check(Sampler && Sampler->Sampler != VK_NULL_HANDLE);
		const bool bDirty = DSWriter[DescriptorSet].WriteSampler(BindingIndex, Sampler->Sampler);
		MarkDirty<VK_DESCRIPTOR_TYPE_SAMPLER>(DescriptorSet, bDirty);
	}

	inline void SetInputAttachment(uint8 DescriptorSet, uint32 BindingIndex, VkImageView View, VkImageLayout Layout)
	{
		const bool bDirty = DSWriter[DescriptorSet].WriteInputAttachment(BindingIndex, View, Layout);
		MarkDirty<VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT>(DescriptorSet, bDirty);
	}

	template<bool bDynamic>
	inline void SetUniformBuffer(uint8 DescriptorSet, uint32 BindingIndex, const FVulkanRealUniformBuffer* UniformBuffer)
	{
/*
		if ((UniformBuffersWithDataMask[DescriptorSet] & (1ULL << (uint64)BindingIndex)) != 0)
*/
		{
			if (bDynamic)
			{
				const bool bDirty = DSWriter[DescriptorSet].WriteDynamicUniformBuffer(BindingIndex, UniformBuffer->GetHandle(), 0, UniformBuffer->GetSize(), UniformBuffer->GetOffset());
				MarkDirty<VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC>(DescriptorSet, bDirty);
			}
			else
			{
				const bool bDirty = DSWriter[DescriptorSet].WriteUniformBuffer(BindingIndex, UniformBuffer->GetHandle(), UniformBuffer->GetOffset(), UniformBuffer->GetSize());
				MarkDirty<VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER>(DescriptorSet, bDirty);
			}
		}
	}


protected:
	inline void Bind(VkCommandBuffer CmdBuffer, VkPipelineLayout PipelineLayout, VkPipelineBindPoint BindPoint)
	{
		VulkanRHI::vkCmdBindDescriptorSets(CmdBuffer,
			BindPoint,
			PipelineLayout,
			0, DescriptorSetHandles.Num(), DescriptorSetHandles.GetData(),
			(uint32)DynamicOffsets.Num(), DynamicOffsets.GetData());
	}

	//#todo-rco: Won't work multithreaded!
	FVulkanDescriptorSetWriteContainer DSWriteContainer;
	const FVulkanDescriptorSetsLayout* DescriptorSetsLayout = nullptr;

	//#todo-rco: Won't work multithreaded!
	TArray<VkDescriptorSet> DescriptorSetHandles;

	// Bitmask of sets that exist in this pipeline
	//#todo-rco: Won't work multithreaded!
	uint32			UsedSetsMask = 0;

	//#todo-rco: Won't work multithreaded!
	TArray<uint32> DynamicOffsets;

	TStaticArray<uint64, ShaderStage::MaxNumSets>			ResourcesDirtyPerSet;
	TStaticArray<uint64, ShaderStage::MaxNumSets>			ResourcesDirtyPerSetMask;

	TArray<FVulkanDescriptorSetWriter> DSWriter;
};


class FVulkanComputePipelineDescriptorState : public FVulkanCommonPipelineDescriptorState
{
public:
	FVulkanComputePipelineDescriptorState(FVulkanDevice* InDevice, FVulkanComputePipeline* InComputePipeline);
	virtual ~FVulkanComputePipelineDescriptorState()
	{
		ComputePipeline->Release();
	}

	void Reset()
	{
		PackedUniformBuffersDirty = PackedUniformBuffersMask;
	}

	inline void SetPackedGlobalShaderParameter(uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValue)
	{
		PackedUniformBuffers.SetPackedGlobalParameter(BufferIndex, ByteOffset, NumBytes, NewValue, PackedUniformBuffersDirty);
	}

	inline void SetUniformBufferConstantData(uint32 BindingIndex, const TArray<uint8>& ConstantData)
	{
		PackedUniformBuffers.SetEmulatedUniformBufferIntoPacked(BindingIndex, ConstantData, PackedUniformBuffersDirty);
	}

	bool UpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer)
	{
		const bool bUseDynamicGlobalUBs = (GDynamicGlobalUBs->GetInt() > 0);
		if (bUseDynamicGlobalUBs)
		{
			return InternalUpdateDescriptorSets<true>(CmdListContext, CmdBuffer);
		}
		else
		{
			return InternalUpdateDescriptorSets<false>(CmdListContext, CmdBuffer);
		}
	}

	inline void BindDescriptorSets(VkCommandBuffer CmdBuffer)
	{
		Bind(CmdBuffer, ComputePipeline->GetLayout().GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);
	}

	inline const FVulkanComputePipelineDescriptorInfo& GetComputePipelineDescriptorInfo() const
	{
		return *PipelineDescriptorInfo;
		//return GfxPipeline->Pipeline->GetGfxLayout().GetGfxPipelineDescriptorInfo();
	}

protected:
	const FVulkanComputePipelineDescriptorInfo* PipelineDescriptorInfo;

	FPackedUniformBuffers PackedUniformBuffers;
	uint64 PackedUniformBuffersMask;
	uint64 PackedUniformBuffersDirty;
/*
	uint64 UniformBuffersWithDataMask;
*/
	uint32 HasDescriptorsInSetMask;
	FVulkanComputePipeline* ComputePipeline;

	template<bool bUseDynamicGlobalUBs>
	bool InternalUpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer);

	void CreateDescriptorWriteInfos();

	friend class FVulkanPendingComputeState;
	friend class FVulkanCommandListContext;
};

class FVulkanGraphicsPipelineDescriptorState : public FVulkanCommonPipelineDescriptorState
{
public:
	FVulkanGraphicsPipelineDescriptorState(FVulkanDevice* InDevice, FVulkanRHIGraphicsPipelineState* InGfxPipeline);
	virtual ~FVulkanGraphicsPipelineDescriptorState()
	{
		GfxPipeline->Release();
	}

	inline void SetPackedGlobalShaderParameter(uint8 Stage, uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValue)
	{
		PackedUniformBuffers[Stage].SetPackedGlobalParameter(BufferIndex, ByteOffset, NumBytes, NewValue, PackedUniformBuffersDirty[Stage]);
	}

	inline void SetUniformBufferConstantData(uint8 Stage, uint32 BindingIndex, const TArray<uint8>& ConstantData)
	{
		PackedUniformBuffers[Stage].SetEmulatedUniformBufferIntoPacked(BindingIndex, ConstantData, PackedUniformBuffersDirty[Stage]);
	}

	inline void SetDynamicUniformBuffer(uint8 DescriptorSet, uint32 BindingIndex, const FVulkanRealUniformBuffer* UniformBuffer)
	{
		ensure(0);
/*
		if ((UniformBuffersWithDataMask[DescriptorSet] & (1ULL << (uint64)BindingIndex)) != 0)
		{
			const bool bDirty = DSWriter[DescriptorSet].WriteDynamicUniformBuffer(BindingIndex, UniformBuffer->GetHandle(), UniformBuffer->GetOffset(), UniformBuffer->GetSize(), 0);
			MarkDirty<VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC>(DescriptorSet, bDirty);
		}
*/
	}

	bool UpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer)
	{
		const bool bUseDynamicGlobalUBs = (GDynamicGlobalUBs->GetInt() > 0);
		if (bUseDynamicGlobalUBs)
		{
			return InternalUpdateDescriptorSets<true>(CmdListContext, CmdBuffer);
		}
		else
		{
			return InternalUpdateDescriptorSets<false>(CmdListContext, CmdBuffer);
		}
	}

	inline void BindDescriptorSets(VkCommandBuffer CmdBuffer)
	{
		Bind(CmdBuffer, GfxPipeline->Pipeline->GetLayout().GetPipelineLayout(), VK_PIPELINE_BIND_POINT_GRAPHICS);
	}

	void Reset()
	{
		FMemory::Memcpy(PackedUniformBuffersDirty, PackedUniformBuffersMask);
		FMemory::Memcpy(ResourcesDirtyPerSet, ResourcesDirtyPerSetMask);
	}

	inline const FVulkanGfxPipelineDescriptorInfo& GetGfxPipelineDescriptorInfo() const
	{
		return *PipelineDescriptorInfo;
	}

protected:
	// Bitmask of stages that exist in this pipeline
	uint32			UsedStagesMask = 0;
	// Bitmask of stages that have descriptors
	uint32			HasDescriptorsPerStageMask = 0;
	uint32			UsedPackedUBStagesMask = 0;

	const FVulkanGfxPipelineDescriptorInfo* PipelineDescriptorInfo;

	TStaticArray<FPackedUniformBuffers, ShaderStage::NumStages> PackedUniformBuffers;
	TStaticArray<uint64, ShaderStage::NumStages> PackedUniformBuffersMask;
	TStaticArray<uint64, ShaderStage::NumStages> PackedUniformBuffersDirty;

	FVulkanRHIGraphicsPipelineState* GfxPipeline;
	int32 ID;

	template<bool bUseDynamicGlobalUBs>
	bool InternalUpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer);

	void CreateDescriptorWriteInfos();

	friend class FVulkanPendingGfxState;
	friend class FVulkanCommandListContext;
};

template <bool bIsDynamic>
static inline bool UpdatePackedUniformBuffers(VkDeviceSize UBOffsetAlignment, const uint16* RESTRICT PackedUBBindingIndices, const FPackedUniformBuffers& PackedUniformBuffers,
	FVulkanDescriptorSetWriter& DescriptorWriteSet, FVulkanUniformBufferUploader* UniformBufferUploader, uint8* RESTRICT CPURingBufferBase, uint64 RemainingPackedUniformsMask,
	FVulkanCmdBuffer* InCmdBuffer)
{
	bool bAnyUBDirty = false;
	int32 PackedUBIndex = 0;
	while (RemainingPackedUniformsMask)
	{
		if (RemainingPackedUniformsMask & 1)
		{
			const FPackedUniformBuffers::FPackedBuffer& StagedUniformBuffer = PackedUniformBuffers.GetBuffer(PackedUBIndex);
			int32 BindingIndex = PackedUBBindingIndices[PackedUBIndex];

			const int32 UBSize = StagedUniformBuffer.Num();

			// get offset into the RingBufferBase pointer
			uint64 RingBufferOffset = UniformBufferUploader->AllocateMemory(UBSize, UBOffsetAlignment, InCmdBuffer);

			// get location in the ring buffer to use
			FMemory::Memcpy(CPURingBufferBase + RingBufferOffset, StagedUniformBuffer.GetData(), UBSize);

			if (bIsDynamic)
			{
				const bool bDirty = DescriptorWriteSet.WriteDynamicUniformBuffer(BindingIndex, UniformBufferUploader->GetCPUBufferHandle(), UniformBufferUploader->GetCPUBufferOffset(), UBSize, RingBufferOffset);
				bAnyUBDirty = bAnyUBDirty || bDirty;

			}
			else
			{
				const bool bDirty = DescriptorWriteSet.WriteUniformBuffer(BindingIndex, UniformBufferUploader->GetCPUBufferHandle(), RingBufferOffset + UniformBufferUploader->GetCPUBufferOffset(), UBSize);
				bAnyUBDirty = bAnyUBDirty || bDirty;

			}
		}
		RemainingPackedUniformsMask = RemainingPackedUniformsMask >> 1;
		++PackedUBIndex;
	}

	return bAnyUBDirty;
}
