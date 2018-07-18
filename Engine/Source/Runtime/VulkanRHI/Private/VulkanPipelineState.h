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

// Common Pipeline state
class FVulkanCommonPipelineDescriptorState : public VulkanRHI::FDeviceChild
{
public:
	FVulkanCommonPipelineDescriptorState(FVulkanDevice* InDevice)
		: VulkanRHI::FDeviceChild(InDevice)
#if !VULKAN_USE_DESCRIPTOR_POOL_MANAGER
		, DSRingBuffer(InDevice)
#endif
	{
	}

protected:
#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	inline void Bind(VkCommandBuffer CmdBuffer, VkPipelineLayout PipelineLayout, VkPipelineBindPoint BindPoint)
	{
		VulkanRHI::vkCmdBindDescriptorSets(CmdBuffer,
			BindPoint,
			PipelineLayout,
			0, DescriptorSetHandles.Num(), DescriptorSetHandles.GetData(),
			(uint32)DynamicOffsets.Num(), DynamicOffsets.GetData());
	}
#endif
	FVulkanDescriptorSetWriteContainer DSWriteContainer;
#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	const FVulkanDescriptorSetsLayout* DescriptorSetsLayout = nullptr;
	FVulkanTypedDescriptorPoolSet* CurrentTypedDescriptorPoolSet = nullptr;
	TArray<VkDescriptorSet> DescriptorSetHandles;

	inline bool AcquirePoolSet(FVulkanCmdBuffer* CmdBuffer)
	{
		// Pipeline state has no current descriptor pools set or set owner is not current - acquire a new pool set
		FVulkanDescriptorPoolSetContainer* CmdBufferPoolSet = CmdBuffer->CurrentDescriptorPoolSetContainer;
		if (CurrentTypedDescriptorPoolSet == nullptr || CurrentTypedDescriptorPoolSet->GetOwner() != CmdBufferPoolSet)
		{
			check(CmdBufferPoolSet);
			CurrentTypedDescriptorPoolSet = CmdBufferPoolSet->AcquireTypedPoolSet(*DescriptorSetsLayout);
			return true;
		}

		return false;
	}

	inline bool AllocateDescriptorSets()
	{
		check(CurrentTypedDescriptorPoolSet);
		return CurrentTypedDescriptorPoolSet->AllocateDescriptorSets(*DescriptorSetsLayout, DescriptorSetHandles.GetData());
	}
#else
	FOLDVulkanDescriptorSetRingBuffer DSRingBuffer;
#endif
	TArray<uint32> DynamicOffsets;
};


class FVulkanComputePipelineDescriptorState : public FVulkanCommonPipelineDescriptorState
{
public:
	FVulkanComputePipelineDescriptorState(FVulkanDevice* InDevice, FVulkanComputePipeline* InComputePipeline);
	~FVulkanComputePipelineDescriptorState()
	{
		ComputePipeline->Release();
	}

	void Reset()
	{
		PackedUniformBuffersDirty = PackedUniformBuffersMask;
#if !VULKAN_USE_DESCRIPTOR_POOL_MANAGER
		DSRingBuffer.Reset();
#endif
	}

	inline void SetStorageBuffer(uint32 BindPoint, VkBuffer Buffer, uint32 Offset, uint32 Size, VkBufferUsageFlags UsageFlags)
	{
		check((UsageFlags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) == VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		DSWriter.WriteStorageBuffer(BindPoint, Buffer, Offset, Size);
	}

	inline void SetUAVTexelBufferViewState(uint32 BindPoint, FVulkanBufferView* View)
	{
		check(View && (View->Flags & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT) == VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
		DSWriter.WriteStorageTexelBuffer(BindPoint, View);
	}

	inline void SetUAVTextureView(uint32 BindPoint, const FVulkanTextureView& TextureView)
	{
		DSWriter.WriteStorageImage(BindPoint, TextureView.View, VK_IMAGE_LAYOUT_GENERAL);
	}

	inline void SetTexture(uint32 BindPoint, const FVulkanTextureBase* TextureBase, VkImageLayout Layout)
	{
		check(TextureBase);
		DSWriter.WriteImage(BindPoint, TextureBase->PartialView->View, Layout);
	}

	inline void SetSRVBufferViewState(uint32 BindPoint, FVulkanBufferView* View)
	{
		check(View && (View->Flags & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) == VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
		DSWriter.WriteUniformTexelBuffer(BindPoint, View);
	}

	inline void SetSRVTextureView(uint32 BindPoint, const FVulkanTextureView& TextureView, VkImageLayout Layout)
	{
		DSWriter.WriteImage(BindPoint, TextureView.View, Layout);
	}

	inline void SetSamplerState(uint32 BindPoint, FVulkanSamplerState* Sampler)
	{
		check(Sampler);
		DSWriter.WriteSampler(BindPoint, Sampler->Sampler);
	}

	inline void SetShaderParameter(uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValue)
	{
		PackedUniformBuffers.SetPackedGlobalParameter(BufferIndex, ByteOffset, NumBytes, NewValue, PackedUniformBuffersDirty);
	}

	inline void SetUniformBufferConstantData(uint32 BindPoint, const TArray<uint8>& ConstantData)
	{
		PackedUniformBuffers.SetEmulatedUniformBufferIntoPacked(BindPoint, ConstantData, PackedUniformBuffersDirty);
	}

	inline void SetUniformBuffer(uint32 BindPoint, const FVulkanUniformBuffer* UniformBuffer)
	{
		if ((UniformBuffersWithDataMask & (1ULL << (uint64)BindPoint)) != 0)
		{
			extern TAutoConsoleVariable<int32> GDynamicGlobalUBs;
			if (GDynamicGlobalUBs.GetValueOnRenderThread() > 1)
			{
				DSWriter.WriteDynamicUniformBuffer(BindPoint, UniformBuffer->GetHandle(), 0, UniformBuffer->GetSize(), UniformBuffer->GetOffset());
			}
			else
			{
				DSWriter.WriteUniformBuffer(BindPoint, UniformBuffer->GetHandle(), UniformBuffer->GetOffset(), UniformBuffer->GetSize());
			}
		}
	}

	bool UpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer);

	inline void BindDescriptorSets(VkCommandBuffer CmdBuffer)
	{
#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
		Bind(CmdBuffer, ComputePipeline->GetLayout().GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);
#else
		check(DSRingBuffer.CurrDescriptorSets);
		DSRingBuffer.CurrDescriptorSets->Bind(CmdBuffer, ComputePipeline->GetLayout().GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);
#endif
	}

protected:
	FPackedUniformBuffers PackedUniformBuffers;
	uint64 PackedUniformBuffersMask;
	uint64 PackedUniformBuffersDirty;
	FVulkanDescriptorSetWriter DSWriter;
	uint64 UniformBuffersWithDataMask;
	uint64 UnusedResourcesDirtyMask = 0;

	FVulkanComputePipeline* ComputePipeline;

	void CreateDescriptorWriteInfos();

	friend class FVulkanPendingComputeState;
	friend class FVulkanCommandListContext;
};

class FVulkanGraphicsPipelineDescriptorState : public FVulkanCommonPipelineDescriptorState
{
public:
	FVulkanGraphicsPipelineDescriptorState(FVulkanDevice* InDevice, FVulkanRHIGraphicsPipelineState* InGfxPipeline, FVulkanBoundShaderState* InBSS);
	~FVulkanGraphicsPipelineDescriptorState()
	{
		GfxPipeline->Release();
		BSS->Release();
	}

	template<VkDescriptorType Type>
	inline void MarkDirty(DescriptorSet::EStage Stage, bool bDirty)
	{
		ResourcesDirty[(int8)Stage] |= ((uint64)(bDirty ? 1 : 0)) << (uint64)Type;
	}

	inline void SetStorageBuffer(DescriptorSet::EStage Stage, uint32 BindPoint, VkBuffer Buffer, uint32 Offset, uint32 Size, VkBufferUsageFlags UsageFlags)
	{
		check((UsageFlags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) == VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		const bool bDirty = DSWriter[(int8)Stage].WriteStorageBuffer(BindPoint, Buffer, Offset, Size);
		MarkDirty<VK_DESCRIPTOR_TYPE_STORAGE_BUFFER>(Stage, bDirty);
	}

	inline void SetUAVTexelBufferViewState(DescriptorSet::EStage Stage, uint32 BindPoint, FVulkanBufferView* View)
	{
		check(View && (View->Flags & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT) == VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
		const bool bDirty = DSWriter[(int8)Stage].WriteStorageTexelBuffer(BindPoint, View);
		MarkDirty<VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER>(Stage, bDirty);
	}

	inline void SetUAVTextureView(DescriptorSet::EStage Stage, uint32 BindPoint, const FVulkanTextureView& TextureView, VkImageLayout Layout)
	{
		const bool bDirty = DSWriter[(int8)Stage].WriteStorageImage(BindPoint, TextureView.View, Layout);
		MarkDirty<VK_DESCRIPTOR_TYPE_STORAGE_IMAGE>(Stage, bDirty);
	}

	inline void SetTexture(DescriptorSet::EStage Stage, uint32 BindPoint, const FVulkanTextureBase* TextureBase, VkImageLayout Layout)
	{
		check(TextureBase);
		const bool bDirty = DSWriter[(int8)Stage].WriteImage(BindPoint, TextureBase->PartialView->View, Layout);
		MarkDirty<VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE>(Stage, bDirty);
	}

	inline void SetSRVBufferViewState(DescriptorSet::EStage Stage, uint32 BindPoint, FVulkanBufferView* View)
	{
		check(View && (View->Flags & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) == VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
		const bool bDirty = DSWriter[(int8)Stage].WriteUniformTexelBuffer(BindPoint, View);
		MarkDirty<VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER>(Stage, bDirty);
	}

	inline void SetSRVTextureView(DescriptorSet::EStage Stage, uint32 BindPoint, const FVulkanTextureView& TextureView, VkImageLayout Layout)
	{
		const bool bDirty = DSWriter[(int8)Stage].WriteImage(BindPoint, TextureView.View, Layout);
		MarkDirty<VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE>(Stage, bDirty);
	}

	inline void SetSamplerState(DescriptorSet::EStage Stage, uint32 BindPoint, FVulkanSamplerState* Sampler)
	{
		check(Sampler && Sampler->Sampler != VK_NULL_HANDLE);
		const bool bDirty = DSWriter[(int8)Stage].WriteSampler(BindPoint, Sampler->Sampler);
		MarkDirty<VK_DESCRIPTOR_TYPE_SAMPLER>(Stage, bDirty);
	}

	inline void SetShaderParameter(DescriptorSet::EStage Stage, uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValue)
	{
		PackedUniformBuffers[(int8)Stage].SetPackedGlobalParameter(BufferIndex, ByteOffset, NumBytes, NewValue, PackedUniformBuffersDirty[(int8)Stage]);
	}

	inline void SetUniformBufferConstantData(DescriptorSet::EStage Stage, uint32 BindPoint, const TArray<uint8>& ConstantData)
	{
		PackedUniformBuffers[(int8)Stage].SetEmulatedUniformBufferIntoPacked(BindPoint, ConstantData, PackedUniformBuffersDirty[(int8)Stage]);
	}

	inline void SetUniformBuffer(DescriptorSet::EStage Stage, uint32 BindPoint, const FVulkanUniformBuffer* UniformBuffer)
	{
		if ((UniformBuffersWithDataMask[(int8)Stage] & (1ULL << (uint64)BindPoint)) != 0)
		{
			extern TAutoConsoleVariable<int32> GDynamicGlobalUBs;
			if (GDynamicGlobalUBs.GetValueOnRenderThread() > 1)
			{
				const bool bDirty = DSWriter[(int8)Stage].WriteDynamicUniformBuffer(BindPoint, UniformBuffer->GetHandle(), 0, UniformBuffer->GetSize(), UniformBuffer->GetOffset());
				MarkDirty<VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC>(Stage, bDirty);
			}
			else
			{
				const bool bDirty = DSWriter[(int8)Stage].WriteUniformBuffer(BindPoint, UniformBuffer->GetHandle(), UniformBuffer->GetOffset(), UniformBuffer->GetSize());
				MarkDirty<VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER>(Stage, bDirty);
			}
		}
	}

	inline void SetDynamicUniformBuffer(DescriptorSet::EStage Stage, uint32 BindPoint, const FVulkanUniformBuffer* UniformBuffer)
	{
		if ((UniformBuffersWithDataMask[(int8)Stage] & (1ULL << (uint64)BindPoint)) != 0)
		{
			const bool bDirty = DSWriter[(int8)Stage].WriteDynamicUniformBuffer(BindPoint, UniformBuffer->GetHandle(), UniformBuffer->GetOffset(), UniformBuffer->GetSize(), 0);
			MarkDirty<VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC>(Stage, bDirty);
		}
	}

	bool UpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer);

	inline void BindDescriptorSets(VkCommandBuffer CmdBuffer)
	{
#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
		Bind(CmdBuffer, GfxPipeline->Pipeline->GetLayout().GetPipelineLayout(), VK_PIPELINE_BIND_POINT_GRAPHICS);
#else
		check(DSRingBuffer.CurrDescriptorSets);
		DSRingBuffer.CurrDescriptorSets->Bind(CmdBuffer, GfxPipeline->Pipeline->GetLayout().GetPipelineLayout(), VK_PIPELINE_BIND_POINT_GRAPHICS);
#endif
	}

	void Reset()
	{
		FMemory::Memcpy(PackedUniformBuffersDirty, PackedUniformBuffersMask);
		FMemory::Memcpy(ResourcesDirty, ResourcesDirtyMask);
#if !VULKAN_USE_DESCRIPTOR_POOL_MANAGER
		DSRingBuffer.Reset();
#endif
	}

	inline void Verify()
	{
#if 0
		int32 TotalNumWrites = 0;
		int32 TotalNumBuffer = 0;
		int32 TotalNumImages = 0;

		for (int32 Index = 0; Index < DescriptorSet::NumGfxStages; ++Index)
		{
			FVulkanShader* Shader = GfxPipeline->Shaders[Index];
			if (!Shader)
			{
				ensure(DSWriter[Index].NumWrites == 0);
				continue;
			}

			ensure(Shader->GetCodeHeader().NEWDescriptorInfo.DescriptorTypes.Num() == DSWriter[Index].NumWrites);
			TotalNumWrites += Shader->GetCodeHeader().NEWDescriptorInfo.DescriptorTypes.Num();
			TotalNumBuffer += Shader->GetCodeHeader().NEWDescriptorInfo.NumBufferInfos;
			TotalNumImages += Shader->GetCodeHeader().NEWDescriptorInfo.NumImageInfos;
		}
		ensure(TotalNumWrites == DSWriteContainer.DescriptorWrites.Num());
		ensure(TotalNumBuffer == DSWriteContainer.DescriptorBufferInfo.Num());
		ensure(TotalNumImages == DSWriteContainer.DescriptorImageInfo.Num());
#endif
	}

protected:
	// Bitmask of stages that exist in this pipeline
	uint32 UsedStagesMask = 0;
	// Bitmask of stages that have descriptors
	uint32 HasDescriptorsPerStageMask = 0;
	const FVulkanCodeHeader* CodeHeaderPerStage[DescriptorSet::NumGfxStages];

	uint64 ResourcesDirty[DescriptorSet::NumGfxStages];
	uint64 ResourcesDirtyMask[DescriptorSet::NumGfxStages];

	FPackedUniformBuffers PackedUniformBuffers[DescriptorSet::NumGfxStages];
	uint64 PackedUniformBuffersMask[DescriptorSet::NumGfxStages];
	uint64 PackedUniformBuffersDirty[DescriptorSet::NumGfxStages];
	uint64 UniformBuffersWithDataMask[DescriptorSet::NumGfxStages];
	FVulkanDescriptorSetWriter DSWriter[DescriptorSet::EStage::NumGfxStages];

	FVulkanRHIGraphicsPipelineState* GfxPipeline;
	FVulkanBoundShaderState* BSS;
	int32 ID;

	void CreateDescriptorWriteInfos();

	friend class FVulkanPendingGfxState;
	friend class FVulkanCommandListContext;
};

template <bool bIsDynamic>
static inline bool UpdatePackedUniformBuffers(VkDeviceSize UBOffsetAlignment, const FVulkanCodeHeader* CodeHeader, const FPackedUniformBuffers& PackedUniformBuffers,
	FVulkanDescriptorSetWriter& DescriptorWriteSet, FVulkanUniformBufferUploader* UniformBufferUploader, uint8* CPURingBufferBase, uint64 RemainingPackedUniformsMask,
	FVulkanCmdBuffer* InCmdBuffer)
{
	bool bAnyUBDirty = false;
	int32 PackedUBIndex = 0;
	while (RemainingPackedUniformsMask)
	{
		if (RemainingPackedUniformsMask & 1)
		{
			const FPackedUniformBuffers::FPackedBuffer& StagedUniformBuffer = PackedUniformBuffers.GetBuffer(PackedUBIndex);
			int32 BindingIndex = CodeHeader->NEWPackedUBToVulkanBindingIndices[PackedUBIndex].VulkanBindingIndex;

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
