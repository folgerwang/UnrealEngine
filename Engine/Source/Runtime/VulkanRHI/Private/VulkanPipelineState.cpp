// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanStatePipeline.cpp: Vulkan pipeline state implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPipelineState.h"
#include "VulkanResources.h"
#include "VulkanPipeline.h"
#include "VulkanContext.h"
#include "VulkanPendingState.h"
#include "VulkanPipeline.h"

enum
{
	NumAllocationsPerPool = 8,
};


const int32 NumGfxStages = FVulkanPlatform::IsVSPSOnly() ? DescriptorSet::NumMobileStages : DescriptorSet::NumGfxStages;

extern TAutoConsoleVariable<int32> GDynamicGlobalUBs;

static TAutoConsoleVariable<int32> GAlwaysWriteDS(
	TEXT("r.Vulkan.AlwaysWriteDS"),
	0,
	TEXT(""),
	ECVF_RenderThreadSafe
);


FVulkanComputePipelineDescriptorState::FVulkanComputePipelineDescriptorState(FVulkanDevice* InDevice, FVulkanComputePipeline* InComputePipeline)
	: FVulkanCommonPipelineDescriptorState(InDevice)
	, PackedUniformBuffersMask(0)
	, PackedUniformBuffersDirty(0)
	, ComputePipeline(InComputePipeline)
{
	check(InComputePipeline);
	PackedUniformBuffers.Init(&InComputePipeline->GetShaderCodeHeader(), PackedUniformBuffersMask, UniformBuffersWithDataMask, UnusedResourcesDirtyMask);

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	DescriptorSetsLayout = &InComputePipeline->GetLayout().GetDescriptorSetsLayout();
#endif

	CreateDescriptorWriteInfos();
	InComputePipeline->AddRef();
}

void FVulkanComputePipelineDescriptorState::CreateDescriptorWriteInfos()
{
	check(DSWriteContainer.DescriptorWrites.Num() == 0);

	const FVulkanCodeHeader& CodeHeader = ComputePipeline->GetShaderCodeHeader();

	DSWriteContainer.DescriptorWrites.AddZeroed(CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num());
	DSWriteContainer.DescriptorImageInfo.AddZeroed(CodeHeader.NEWDescriptorInfo.NumImageInfos);
	DSWriteContainer.DescriptorBufferInfo.AddZeroed(CodeHeader.NEWDescriptorInfo.NumBufferInfos);

	checkf(CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num() < 255, TEXT("Need more bits for BindingToDynamicOffsetMap (currently 8)! Requires %d descriptor bindings in a set!"), CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num());
	DSWriteContainer.BindingToDynamicOffsetMap.AddZeroed(CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num());

	VkSampler DefaultSampler = Device->GetDefaultSampler();
	VkImageView DefaultImageView = Device->GetDefaultImageView();
	for (int32 Index = 0; Index < DSWriteContainer.DescriptorImageInfo.Num(); ++Index)
	{
		// Texture.Load() still requires a default sampler...
		DSWriteContainer.DescriptorImageInfo[Index].sampler = DefaultSampler;
		DSWriteContainer.DescriptorImageInfo[Index].imageView = DefaultImageView;
		DSWriteContainer.DescriptorImageInfo[Index].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	VkWriteDescriptorSet* CurrentDescriptorWrite = DSWriteContainer.DescriptorWrites.GetData();
	VkDescriptorImageInfo* CurrentImageInfo = DSWriteContainer.DescriptorImageInfo.GetData();
	VkDescriptorBufferInfo* CurrentBufferInfo = DSWriteContainer.DescriptorBufferInfo.GetData();
	uint8* CurrentBindingIndexToDynamicOffset = DSWriteContainer.BindingToDynamicOffsetMap.GetData();

	uint32 NumDynamicOffsets = DSWriter.SetupDescriptorWrites(CodeHeader.NEWDescriptorInfo, CurrentDescriptorWrite, CurrentImageInfo, CurrentBufferInfo, CurrentBindingIndexToDynamicOffset);
	DynamicOffsets.AddZeroed(NumDynamicOffsets);
	DSWriter.DynamicOffsets = DynamicOffsets.GetData();

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	DescriptorSetHandles.AddZeroed(DescriptorSetsLayout->GetHandles().Num());
#endif
}

bool FVulkanComputePipelineDescriptorState::UpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUpdateDescriptorSets);
#endif
	int32 WriteIndex = 0;

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	// Early exit
	if (DescriptorSetsLayout->GetHandles().Num() == 0)
	{
		return false;
	}

	// No current descriptor pools set - acquire one and reset
	AcquirePoolSet(CmdBuffer);
	if (!AllocateDescriptorSets())
	{
		return false;
	}
#else
	DSRingBuffer.CurrDescriptorSets = DSRingBuffer.RequestDescriptorSets(CmdListContext, CmdBuffer, ComputePipeline->GetLayout());
	if (!DSRingBuffer.CurrDescriptorSets)
	{
		return false;
	}

	const FOLDVulkanDescriptorSets::FDescriptorSetArray& DescriptorSetHandles = DSRingBuffer.CurrDescriptorSets->GetHandles();
#endif
	int32 DescriptorSetIndex = 0;

	FVulkanUniformBufferUploader* UniformBufferUploader = CmdListContext->GetUniformBufferUploader();
	uint8* CPURingBufferBase = (uint8*)UniformBufferUploader->GetCPUMappedPointer();

	const VkDescriptorSet DescriptorSet = DescriptorSetHandles[DescriptorSetIndex];
	++DescriptorSetIndex;

	bool bRequiresPackedUBUpdate = (PackedUniformBuffersDirty != 0);
	if (bRequiresPackedUBUpdate)
	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanApplyDSUniformBuffers);
#endif

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
		if (GDynamicGlobalUBs->GetInt() > 0)
		{
			UpdatePackedUniformBuffers<true>(Device->GetLimits().minUniformBufferOffsetAlignment, &ComputePipeline->GetShaderCodeHeader(), PackedUniformBuffers, DSWriter, UniformBufferUploader, CPURingBufferBase, PackedUniformBuffersDirty, CmdBuffer);
		}
		else
#endif
		{
			UpdatePackedUniformBuffers<false>(Device->GetLimits().minUniformBufferOffsetAlignment, &ComputePipeline->GetShaderCodeHeader(), PackedUniformBuffers, DSWriter, UniformBufferUploader, CPURingBufferBase, PackedUniformBuffersDirty, CmdBuffer);
		}
		PackedUniformBuffersDirty = 0;
	}

	DSWriter.SetDescriptorSet(DescriptorSet);

#if VULKAN_ENABLE_AGGRESSIVE_STATS
	INC_DWORD_STAT_BY(STAT_VulkanNumUpdateDescriptors, DSWriteContainer.DescriptorWrites.Num());
	INC_DWORD_STAT_BY(STAT_VulkanNumDescSets, DescriptorSetIndex);
#endif

	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanVkUpdateDS);
#endif
		VulkanRHI::vkUpdateDescriptorSets(Device->GetInstanceHandle(), DSWriteContainer.DescriptorWrites.Num(), DSWriteContainer.DescriptorWrites.GetData(), 0, nullptr);
	}

	return true;
}


FVulkanGraphicsPipelineDescriptorState::FVulkanGraphicsPipelineDescriptorState(FVulkanDevice* InDevice, FVulkanRHIGraphicsPipelineState* InGfxPipeline, FVulkanBoundShaderState* InBSS)
	: FVulkanCommonPipelineDescriptorState(InDevice)
	, GfxPipeline(InGfxPipeline)
	, BSS(InBSS)
{
	FMemory::Memzero(PackedUniformBuffersMask);
	FMemory::Memzero(PackedUniformBuffersDirty);
	FMemory::Memzero(CodeHeaderPerStage);
	FMemory::Memzero(ResourcesDirty);
	FMemory::Memzero(ResourcesDirtyMask);

	for (int32 Index = 0; Index < (int32)NumGfxStages; ++Index)
	{
		const FVulkanShader* StageShader = BSS->GetShader((DescriptorSet::EStage)Index);
		if (StageShader)
		{
			UsedStagesMask |= (1 << Index);

			const FVulkanCodeHeader& CodeHeader = StageShader->GetCodeHeader();
			if (CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num() > 0)
			{
				HasDescriptorsPerStageMask |= (1 << Index);
			}
			CodeHeaderPerStage[Index] = &CodeHeader;
		}
	}

	PackedUniformBuffers[DescriptorSet::Vertex].Init(CodeHeaderPerStage[DescriptorSet::Vertex], PackedUniformBuffersMask[DescriptorSet::Vertex], UniformBuffersWithDataMask[DescriptorSet::Vertex], ResourcesDirtyMask[DescriptorSet::Vertex]);

	if (BSS->GetPixelShader())
	{
		PackedUniformBuffers[DescriptorSet::Pixel].Init(CodeHeaderPerStage[DescriptorSet::Pixel], PackedUniformBuffersMask[DescriptorSet::Pixel], UniformBuffersWithDataMask[DescriptorSet::Pixel], ResourcesDirtyMask[DescriptorSet::Pixel]);
	}
	if (BSS->GetGeometryShader())
	{
		PackedUniformBuffers[DescriptorSet::Geometry].Init(CodeHeaderPerStage[DescriptorSet::Geometry], PackedUniformBuffersMask[DescriptorSet::Geometry], UniformBuffersWithDataMask[DescriptorSet::Geometry], ResourcesDirtyMask[DescriptorSet::Geometry]);
	}
	if (BSS->GetHullShader())
	{
		ensureMsgf(0, TEXT("Tessellation not supported yet!"));
/*
		PackedUniformBuffers[DescriptorSet::Domain].Init(CodeHeaderPerStage[DescriptorSet::Domain], PackedUniformBuffersMask[DescriptorSet::Domain], UniformBuffersWithDataMask[DescriptorSet::Domain], ResourcesDirtyMask[DescriptorSet::Domain]);
		PackedUniformBuffers[DescriptorSet::Hull].Init(CodeHeaderPerStage[DescriptorSet::Hull], PackedUniformBuffersMask[DescriptorSet::Hull], UniformBuffersWithDataMask[DescriptorSet::Hull], ResourcesDirtyMask[DescriptorSet::Domain]);
*/
	}

	check(InGfxPipeline);
	check(InGfxPipeline->Pipeline);
#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	DescriptorSetsLayout = &InGfxPipeline->Pipeline->GetLayout().GetDescriptorSetsLayout();
#endif

	CreateDescriptorWriteInfos();

	static int32 IDCounter = 0;
	ID = IDCounter++;

	//UE_LOG(LogVulkanRHI, Warning, TEXT("GfxPSOState %p For PSO %p Writes:%d"), this, InGfxPipeline, DSWriteContainer.DescriptorWrites.Num());

	InGfxPipeline->AddRef();
	BSS->AddRef();
}

void FVulkanGraphicsPipelineDescriptorState::CreateDescriptorWriteInfos()
{
	check(DSWriteContainer.DescriptorWrites.Num() == 0);

	for (int32 Stage = 0; Stage < NumGfxStages; Stage++)
	{
		const FVulkanShader* StageShader = BSS->GetShader((DescriptorSet::EStage)Stage);
		if (!StageShader)
		{
			continue;
		}

		const FVulkanCodeHeader& CodeHeader = StageShader->GetCodeHeader();
		DSWriteContainer.DescriptorWrites.AddZeroed(CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num());
		DSWriteContainer.DescriptorImageInfo.AddZeroed(CodeHeader.NEWDescriptorInfo.NumImageInfos);
		DSWriteContainer.DescriptorBufferInfo.AddZeroed(CodeHeader.NEWDescriptorInfo.NumBufferInfos);

		checkf(CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num() < 255, TEXT("Need more bits for BindingToDynamicOffsetMap (currently 8)! Requires %d descriptor bindings in a set!"), CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num());
		DSWriteContainer.BindingToDynamicOffsetMap.AddUninitialized(CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num());
		FMemory::Memset(DSWriteContainer.BindingToDynamicOffsetMap.GetData(), 255, DSWriteContainer.BindingToDynamicOffsetMap.Num());
	}

	VkSampler DefaultSampler = Device->GetDefaultSampler();
	VkImageView DefaultImageView = Device->GetDefaultImageView();
	for (int32 Index = 0; Index < DSWriteContainer.DescriptorImageInfo.Num(); ++Index)
	{
		// Texture.Load() still requires a default sampler...
		DSWriteContainer.DescriptorImageInfo[Index].sampler = DefaultSampler;
		DSWriteContainer.DescriptorImageInfo[Index].imageView = DefaultImageView;
		DSWriteContainer.DescriptorImageInfo[Index].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	VkWriteDescriptorSet* CurrentDescriptorWrite = DSWriteContainer.DescriptorWrites.GetData();
	VkDescriptorImageInfo* CurrentImageInfo = DSWriteContainer.DescriptorImageInfo.GetData();
	VkDescriptorBufferInfo* CurrentBufferInfo = DSWriteContainer.DescriptorBufferInfo.GetData();
	uint8* CurrentBindingToDynamicOffsetMap = DSWriteContainer.BindingToDynamicOffsetMap.GetData();
	uint32 DynamicOffsetsStart[DescriptorSet::NumGfxStages];
	uint32 TotalNumDynamicOffsets = 0;
	for (int32 Stage = 0; Stage < NumGfxStages; Stage++)
	{
		DynamicOffsetsStart[Stage] = TotalNumDynamicOffsets;

		const FVulkanShader* StageShader = BSS->GetShader((DescriptorSet::EStage)Stage);
		if (!StageShader)
		{
			continue;
		}

		const FVulkanCodeHeader& CodeHeader = StageShader->GetCodeHeader();
		uint32 NumDynamicOffsets = DSWriter[Stage].SetupDescriptorWrites(CodeHeader.NEWDescriptorInfo, CurrentDescriptorWrite, CurrentImageInfo, CurrentBufferInfo, CurrentBindingToDynamicOffsetMap);
		TotalNumDynamicOffsets += NumDynamicOffsets;

		CurrentDescriptorWrite += CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num();
		CurrentImageInfo += CodeHeader.NEWDescriptorInfo.NumImageInfos;
		CurrentBufferInfo += CodeHeader.NEWDescriptorInfo.NumBufferInfos;
		CurrentBindingToDynamicOffsetMap += CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num();
	}

	DynamicOffsets.AddZeroed(TotalNumDynamicOffsets);
	for (int32 Stage = 0; Stage < NumGfxStages; Stage++)
	{
		DSWriter[Stage].DynamicOffsets = DynamicOffsetsStart[Stage] + DynamicOffsets.GetData();
	}

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	DescriptorSetHandles.AddZeroed(DescriptorSetsLayout->GetHandles().Num());
#endif
}

bool FVulkanGraphicsPipelineDescriptorState::UpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUpdateDescriptorSets);
#endif

	int32 WriteIndex = 0;

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	// Early exit
	if (!HasDescriptorsPerStageMask)
	{
		return false;
	}

	// No current descriptor pools set - acquire one and reset
	bool bNewDescriptorPool = AcquirePoolSet(CmdBuffer);
	bool bNeedsWrite = bNewDescriptorPool;
	bNeedsWrite = bNeedsWrite || (GAlwaysWriteDS.GetValueOnAnyThread() != 0);
#else
	DSRingBuffer.CurrDescriptorSets = DSRingBuffer.RequestDescriptorSets(CmdListContext, CmdBuffer, GfxPipeline->Pipeline->GetLayout());
	if (!DSRingBuffer.CurrDescriptorSets)
	{
		return false;
	}

	const FOLDVulkanDescriptorSets::FDescriptorSetArray& DescriptorSetHandles = DSRingBuffer.CurrDescriptorSets->GetHandles();
#endif

	FVulkanUniformBufferUploader* UniformBufferUploader = CmdListContext->GetUniformBufferUploader();
	uint8* CPURingBufferBase = (uint8*)UniformBufferUploader->GetCPUMappedPointer();

	const bool bUseDynamicGlobalUBs = (GDynamicGlobalUBs->GetInt() > 0);
	const VkDeviceSize UBOffsetAlignment = Device->GetLimits().minUniformBufferOffsetAlignment;
	int32 NumNonDirtyStages = 0;

	// Process updates
	{
		uint32 RemainingStagesMask = UsedStagesMask;
		uint32 RemainingHasDescriptorsPerStageMask = HasDescriptorsPerStageMask;
		for (int32 Stage = 0; Stage < NumGfxStages && (RemainingStagesMask > 0); Stage++)
		{
			// Only process Shader Stages that exist in this pipeline
			if (RemainingStagesMask & 1)
			{
				if (RemainingHasDescriptorsPerStageMask & 1)
				{
					bool bPackedUBChanged = false;
					if (PackedUniformBuffersDirty[Stage] != 0)
					{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
						SCOPE_CYCLE_COUNTER(STAT_VulkanApplyDSUniformBuffers);
#endif
#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
						if (bUseDynamicGlobalUBs)
						{
							bPackedUBChanged = UpdatePackedUniformBuffers<true>(UBOffsetAlignment, CodeHeaderPerStage[Stage], PackedUniformBuffers[Stage], DSWriter[Stage], UniformBufferUploader, CPURingBufferBase, PackedUniformBuffersDirty[Stage], CmdBuffer);
						}
						else
#endif
						{
							bPackedUBChanged = UpdatePackedUniformBuffers<false>(UBOffsetAlignment, CodeHeaderPerStage[Stage], PackedUniformBuffers[Stage], DSWriter[Stage], UniformBufferUploader, CPURingBufferBase, PackedUniformBuffersDirty[Stage], CmdBuffer);
						}
					}

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
					if (!bPackedUBChanged && !ResourcesDirty[Stage] && !bNewDescriptorPool)
					{
						++NumNonDirtyStages;
					}
					bNeedsWrite = bNeedsWrite || bPackedUBChanged || ResourcesDirty[Stage];
#endif
				}
			}

			RemainingStagesMask >>= 1;
			RemainingHasDescriptorsPerStageMask >>= 1;
		}
	}

	// Allocate sets based on what changed
#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	if (bNeedsWrite)
#endif
	{
#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
		if (!AllocateDescriptorSets())
		{
			return false;
		}
#endif
		uint32 RemainingStagesMask = HasDescriptorsPerStageMask;
		uint32 Stage = 0;
		while (RemainingStagesMask)
		{
			if (RemainingStagesMask & 1)
			{
				const VkDescriptorSet DescriptorSet = DescriptorSetHandles[Stage];
				DSWriter[Stage].SetDescriptorSet(DescriptorSet);
			}

			++Stage;
			RemainingStagesMask >>= 1;
		}

#if VULKAN_ENABLE_AGGRESSIVE_STATS
		INC_DWORD_STAT_BY(STAT_VulkanNumUpdateDescriptors, DSWriteContainer.DescriptorWrites.Num());
		INC_DWORD_STAT_BY(STAT_VulkanNumDescSets, Stage);
		INC_DWORD_STAT_BY(STAT_VulkanNumRedundantDescSets, NumNonDirtyStages);
		SCOPE_CYCLE_COUNTER(STAT_VulkanVkUpdateDS);
#endif
		VulkanRHI::vkUpdateDescriptorSets(Device->GetInstanceHandle(), DSWriteContainer.DescriptorWrites.Num(), DSWriteContainer.DescriptorWrites.GetData(), 0, nullptr);

		FMemory::Memzero(ResourcesDirty);
		FMemory::Memzero(PackedUniformBuffersDirty);
	}

	return true;
}


void FVulkanCommandListContext::RHISetGraphicsPipelineState(FGraphicsPipelineStateRHIParamRef GraphicsState)
{
	FVulkanRHIGraphicsPipelineState* Pipeline = ResourceCast(GraphicsState);
	
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (PendingGfxState->SetGfxPipeline(Pipeline) || !CmdBuffer->bHasPipeline)
	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanPipelineBind);
#endif
		PendingGfxState->CurrentPipeline->Pipeline->Bind(CmdBuffer->GetHandle());
		CmdBuffer->bHasPipeline = true;
		PendingGfxState->MarkNeedsDynamicStates();
		PendingGfxState->StencilRef = 0;
	}

	// Yuck - Bind pending pixel shader UAVs from SetRenderTargets
	{
		for (int32 Index = 0; Index < PendingPixelUAVs.Num(); ++Index)
		{
			PendingGfxState->SetUAV(DescriptorSet::Pixel, PendingPixelUAVs[Index].BindIndex, PendingPixelUAVs[Index].UAV);
		}
	}
}
