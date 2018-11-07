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
#include "VulkanLLM.h"

enum
{
	NumAllocationsPerPool = 8,
};


extern TAutoConsoleVariable<int32> GDynamicGlobalUBs;

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
static TAutoConsoleVariable<int32> GAlwaysWriteDS(
	TEXT("r.Vulkan.AlwaysWriteDS"),
	0,
	TEXT(""),
	ECVF_RenderThreadSafe
);
#endif

static bool ShouldAlwaysWriteDescriptors()
{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	return (GAlwaysWriteDS.GetValueOnAnyThread() != 0);
#else
	return false;
#endif
}

FVulkanComputePipelineDescriptorState::FVulkanComputePipelineDescriptorState(FVulkanDevice* InDevice, FVulkanComputePipeline* InComputePipeline)
	: FVulkanCommonPipelineDescriptorState(InDevice)
	, PackedUniformBuffersMask(0)
	, PackedUniformBuffersDirty(0)
	, HasDescriptorsInSetMask(0)
	, ComputePipeline(InComputePipeline)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanShaders);
	check(InComputePipeline);
	const FVulkanShaderHeader& CodeHeader = InComputePipeline->GetShaderCodeHeader();
	PackedUniformBuffers.Init(CodeHeader, PackedUniformBuffersMask);

	DescriptorSetsLayout = &InComputePipeline->GetLayout().GetDescriptorSetsLayout();

	CreateDescriptorWriteInfos();
	InComputePipeline->AddRef();

	PipelineDescriptorInfo = &InComputePipeline->GetComputeLayout().GetComputePipelineDescriptorInfo();
}

void FVulkanComputePipelineDescriptorState::CreateDescriptorWriteInfos()
{
	check(DSWriteContainer.DescriptorWrites.Num() == 0);

	int32 LastSet = -1;
	ensure(DescriptorSetsLayout->RemappingInfo.SetInfos.Num() == 0 || DescriptorSetsLayout->RemappingInfo.SetInfos.Num() == 1);
	for (int32 Index = 0; Index < DescriptorSetsLayout->RemappingInfo.SetInfos.Num(); ++Index)
	{
		const FDescriptorSetRemappingInfo::FSetInfo& SetInfo = DescriptorSetsLayout->RemappingInfo.SetInfos[Index];
		DSWriteContainer.DescriptorWrites.AddZeroed(SetInfo.Types.Num());
		DSWriteContainer.DescriptorImageInfo.AddZeroed(SetInfo.NumImageInfos);
		DSWriteContainer.DescriptorBufferInfo.AddZeroed(SetInfo.NumBufferInfos);

		checkf(SetInfo.Types.Num() < 255, TEXT("Need more bits for BindingToDynamicOffsetMap (currently 8)! Requires %d descriptor bindings in a set!"), SetInfo.Types.Num());
		DSWriteContainer.BindingToDynamicOffsetMap.AddUninitialized(SetInfo.Types.Num());
		FMemory::Memset(DSWriteContainer.BindingToDynamicOffsetMap.GetData(), 255, DSWriteContainer.BindingToDynamicOffsetMap.Num());

		LastSet = FMath::Max(LastSet, Index);
		UsedSetsMask = UsedSetsMask | (1 << Index);
	}

	check(LastSet == -1 || (UsedSetsMask <= (uint32)((1 << (uint32)(LastSet + 1)) - 1)));
	check(DSWriter.Num() == 0);
	int32 NumSets = LastSet + 1;
	DSWriter.AddDefaulted(NumSets);

	VkSampler DefaultSampler = Device->GetDefaultSampler();
	VkImageView DefaultImageView = Device->GetDefaultImageView();
	for (int32 Set = 0; Set < DSWriteContainer.DescriptorImageInfo.Num(); ++Set)
	{
		// Texture.Load() still requires a default sampler...
		DSWriteContainer.DescriptorImageInfo[Set].sampler = DefaultSampler;
		DSWriteContainer.DescriptorImageInfo[Set].imageView = DefaultImageView;
		DSWriteContainer.DescriptorImageInfo[Set].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	VkWriteDescriptorSet* CurrentDescriptorWrite = DSWriteContainer.DescriptorWrites.GetData();
	VkDescriptorImageInfo* CurrentImageInfo = DSWriteContainer.DescriptorImageInfo.GetData();
	VkDescriptorBufferInfo* CurrentBufferInfo = DSWriteContainer.DescriptorBufferInfo.GetData();
	uint8* CurrentBindingToDynamicOffsetMap = DSWriteContainer.BindingToDynamicOffsetMap.GetData();
	TArray<uint32> DynamicOffsetsStart;
	DynamicOffsetsStart.AddZeroed(NumSets);
	uint32 TotalNumDynamicOffsets = 0;

	for (int32 Set = 0; Set < NumSets; ++Set)
	{
		if (UsedSetsMask & (1 << Set))
		{
			const FDescriptorSetRemappingInfo::FSetInfo& SetInfo = DescriptorSetsLayout->RemappingInfo.SetInfos[Set];

			DynamicOffsetsStart[Set] = TotalNumDynamicOffsets;

			uint32 NumDynamicOffsets = DSWriter[Set].SetupDescriptorWrites(SetInfo.Types, CurrentDescriptorWrite, CurrentImageInfo, CurrentBufferInfo, CurrentBindingToDynamicOffsetMap);
			TotalNumDynamicOffsets += NumDynamicOffsets;

			CurrentDescriptorWrite += SetInfo.Types.Num();
			CurrentImageInfo += SetInfo.NumImageInfos;
			CurrentBufferInfo += SetInfo.NumBufferInfos;
			CurrentBindingToDynamicOffsetMap += SetInfo.Types.Num();
		}
	}

	DynamicOffsets.AddZeroed(TotalNumDynamicOffsets);
	for (int32 Set = 0; Set < NumSets; ++Set)
	{
		DSWriter[Set].DynamicOffsets = DynamicOffsetsStart[Set] + DynamicOffsets.GetData();
	}

	DescriptorSetHandles.AddZeroed(NumSets);
	HasDescriptorsInSetMask = UsedSetsMask;
}

template<bool bUseDynamicGlobalUBs>
bool FVulkanComputePipelineDescriptorState::InternalUpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUpdateDescriptorSets);
#endif

	// Early exit
	if (!HasDescriptorsInSetMask)
	{
		return false;
	}

	if (!CmdBuffer->AcquirePoolSetAndDescriptorsIfNeeded(*DescriptorSetsLayout, true, DescriptorSetHandles.GetData()))
	{
		return false;
	}

	int32 DescriptorSetIndex = 0;

	FVulkanUniformBufferUploader* UniformBufferUploader = CmdListContext->GetUniformBufferUploader();
	uint8* CPURingBufferBase = (uint8*)UniformBufferUploader->GetCPUMappedPointer();
	const VkDeviceSize UBOffsetAlignment = Device->GetLimits().minUniformBufferOffsetAlignment;

	const VkDescriptorSet DescriptorSet = DescriptorSetHandles[DescriptorSetIndex];
	++DescriptorSetIndex;

	bool bRequiresPackedUBUpdate = (PackedUniformBuffersDirty != 0);
	if (bRequiresPackedUBUpdate)
	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanApplyPackedUniformBuffers);
#endif
		UpdatePackedUniformBuffers<bUseDynamicGlobalUBs>(UBOffsetAlignment, PipelineDescriptorInfo->RemappingInfo->StageInfos[0].PackedUBBindingIndices.GetData(), PackedUniformBuffers, DSWriter[0], UniformBufferUploader, CPURingBufferBase, PackedUniformBuffersDirty, CmdBuffer);
		PackedUniformBuffersDirty = 0;
	}

	DSWriter[0].SetDescriptorSet(DescriptorSet);

	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		INC_DWORD_STAT_BY(STAT_VulkanNumUpdateDescriptors, DSWriteContainer.DescriptorWrites.Num());
		INC_DWORD_STAT(STAT_VulkanNumDescSets);
		SCOPE_CYCLE_COUNTER(STAT_VulkanVkUpdateDS);
#endif
		VulkanRHI::vkUpdateDescriptorSets(Device->GetInstanceHandle(), DSWriteContainer.DescriptorWrites.Num(), DSWriteContainer.DescriptorWrites.GetData(), 0, nullptr);
	}

	return true;
}


FVulkanGraphicsPipelineDescriptorState::FVulkanGraphicsPipelineDescriptorState(FVulkanDevice* InDevice, FVulkanRHIGraphicsPipelineState* InGfxPipeline)
	: FVulkanCommonPipelineDescriptorState(InDevice)
	, GfxPipeline(InGfxPipeline)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanShaders);
	FMemory::Memzero(PackedUniformBuffersMask);
	FMemory::Memzero(PackedUniformBuffersDirty);
	FMemory::Memzero(ResourcesDirtyPerSet);
	FMemory::Memzero(ResourcesDirtyPerSetMask);

	check(InGfxPipeline);
	check(InGfxPipeline->Pipeline);
	DescriptorSetsLayout = &InGfxPipeline->Pipeline->GetLayout().GetDescriptorSetsLayout();
	PipelineDescriptorInfo = &InGfxPipeline->Pipeline->GetGfxLayout().GetGfxPipelineDescriptorInfo();

	UsedSetsMask = PipelineDescriptorInfo->HasDescriptorsInSetMask;

	PackedUniformBuffers[ShaderStage::Vertex].Init(InGfxPipeline->GetShader(SF_Vertex)->GetCodeHeader(), PackedUniformBuffersMask[ShaderStage::Vertex]);
	UsedStagesMask |= 1 << ShaderStage::Vertex;
	HasDescriptorsPerStageMask |= DescriptorSetsLayout->RemappingInfo.StageInfos[ShaderStage::Vertex].IsEmpty() ? 0 : (1 << ShaderStage::Vertex);
	UsedPackedUBStagesMask |= DescriptorSetsLayout->RemappingInfo.StageInfos[ShaderStage::Vertex].PackedUBBindingIndices.Num() > 0 ? (1 << ShaderStage::Vertex) : 0;

	if (InGfxPipeline->GetShader(SF_Pixel))
	{
		PackedUniformBuffers[ShaderStage::Pixel].Init(InGfxPipeline->GetShader(SF_Pixel)->GetCodeHeader(), PackedUniformBuffersMask[ShaderStage::Pixel]);
		UsedStagesMask |= 1 << ShaderStage::Pixel;
		HasDescriptorsPerStageMask |= DescriptorSetsLayout->RemappingInfo.StageInfos[ShaderStage::Pixel].IsEmpty() ? 0 : (1 << ShaderStage::Pixel);
		UsedPackedUBStagesMask |= DescriptorSetsLayout->RemappingInfo.StageInfos[ShaderStage::Pixel].PackedUBBindingIndices.Num() > 0 ? (1 << ShaderStage::Pixel) : 0;
	}
	if (InGfxPipeline->GetShader(SF_Geometry))
	{
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
		PackedUniformBuffers[ShaderStage::Geometry].Init(InGfxPipeline->GetShader(SF_Geometry)->GetCodeHeader(), PackedUniformBuffersMask[ShaderStage::Geometry]);
		UsedStagesMask |= 1 << ShaderStage::Geometry;
		HasDescriptorsPerStageMask |= DescriptorSetsLayout->RemappingInfo.StageInfos[ShaderStage::Geometry].IsEmpty() ? 0 : (1 << ShaderStage::Geometry);
		UsedPackedUBStagesMask |= DescriptorSetsLayout->RemappingInfo.StageInfos[ShaderStage::Geometry].PackedUBBindingIndices.Num() > 0 ? (1 << ShaderStage::Geometry) : 0;
#else
		ensureMsgf(0, TEXT("Geometry not supported!"));
#endif
	}
	if (InGfxPipeline->GetShader(SF_Hull))
	{
		ensureMsgf(0, TEXT("Tessellation not supported yet!"));
/*
		PackedUniformBuffers[ShaderStage::Domain].Init(CodeHeaderPerStage[ShaderStage::Domain], PackedUniformBuffersMask[ShaderStage::Domain], UniformBuffersWithDataMask[ShaderStage::Domain], ResourcesDirtyMask[ShaderStage::Domain]);
		PackedUniformBuffers[ShaderStage::Hull].Init(CodeHeaderPerStage[ShaderStage::Hull], PackedUniformBuffersMask[ShaderStage::Hull], UniformBuffersWithDataMask[ShaderStage::Hull], ResourcesDirtyMask[ShaderStage::Domain]);
		UsedStagesMask |= (1 << ShaderStage::Hull) | (1 << ShaderStage::Domain);
*/
	}

	//checkf(PipelineDescriptorInfo->DescriptorSets.Num() <= DescriptorSet::NumGfxStages, TEXT("Update ResourcesDirty & ResourcesDirtyMask!"));
	for (int32 Index = 0; Index < PipelineDescriptorInfo->RemappingInfo->SetInfos.Num(); ++Index)
	{
		const FDescriptorSetRemappingInfo::FSetInfo& SetInfo = PipelineDescriptorInfo->RemappingInfo->SetInfos[Index];
		uint64 Value = ((uint64)1 << (uint64)SetInfo.Types.Num()) - 1;
		ResourcesDirtyPerSetMask[Index] = Value;
	}

	CreateDescriptorWriteInfos();

	static int32 IDCounter = 0;
	ID = IDCounter++;

	//UE_LOG(LogVulkanRHI, Warning, TEXT("GfxPSOState %p For PSO %p Writes:%d"), this, InGfxPipeline, DSWriteContainer.DescriptorWrites.Num());

	InGfxPipeline->AddRef();
}

void FVulkanGraphicsPipelineDescriptorState::CreateDescriptorWriteInfos()
{
	check(DSWriteContainer.DescriptorWrites.Num() == 0);

	int32 LastSet = -1;
	for (int32 Index = 0; Index < DescriptorSetsLayout->RemappingInfo.SetInfos.Num(); ++Index)
	{
		const FDescriptorSetRemappingInfo::FSetInfo& SetInfo = DescriptorSetsLayout->RemappingInfo.SetInfos[Index];
		DSWriteContainer.DescriptorWrites.AddZeroed(SetInfo.Types.Num());
		DSWriteContainer.DescriptorImageInfo.AddZeroed(SetInfo.NumImageInfos);
		DSWriteContainer.DescriptorBufferInfo.AddZeroed(SetInfo.NumBufferInfos);

		checkf(SetInfo.Types.Num() < 255, TEXT("Need more bits for BindingToDynamicOffsetMap (currently 8)! Requires %d descriptor bindings in a set!"), SetInfo.Types.Num());
		DSWriteContainer.BindingToDynamicOffsetMap.AddUninitialized(SetInfo.Types.Num());
		FMemory::Memset(DSWriteContainer.BindingToDynamicOffsetMap.GetData(), 255, DSWriteContainer.BindingToDynamicOffsetMap.Num());

		LastSet = FMath::Max(LastSet, Index);
		UsedSetsMask = UsedSetsMask | (1 << Index);
	}

	check(LastSet == -1 || (UsedSetsMask <= (uint32)((1 << (uint32)(LastSet + 1)) - 1)));
	check(DSWriter.Num() == 0);
	int32 NumSets = LastSet + 1;
	DSWriter.AddDefaulted(NumSets);

	VkSampler DefaultSampler = Device->GetDefaultSampler();
	VkImageView DefaultImageView = Device->GetDefaultImageView();
	for (int32 Set = 0; Set < DSWriteContainer.DescriptorImageInfo.Num(); ++Set)
	{
		// Texture.Load() still requires a default sampler...
		DSWriteContainer.DescriptorImageInfo[Set].sampler = DefaultSampler;
		DSWriteContainer.DescriptorImageInfo[Set].imageView = DefaultImageView;
		DSWriteContainer.DescriptorImageInfo[Set].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	VkWriteDescriptorSet* CurrentDescriptorWrite = DSWriteContainer.DescriptorWrites.GetData();
	VkDescriptorImageInfo* CurrentImageInfo = DSWriteContainer.DescriptorImageInfo.GetData();
	VkDescriptorBufferInfo* CurrentBufferInfo = DSWriteContainer.DescriptorBufferInfo.GetData();
	uint8* CurrentBindingToDynamicOffsetMap = DSWriteContainer.BindingToDynamicOffsetMap.GetData();
	TArray<uint32> DynamicOffsetsStart;
	DynamicOffsetsStart.AddZeroed(NumSets);
	uint32 TotalNumDynamicOffsets = 0;

	for (int32 Set = 0; Set < NumSets; ++Set)
	{
		if (UsedSetsMask & (1 << Set))
		{
			const FDescriptorSetRemappingInfo::FSetInfo& SetInfo = DescriptorSetsLayout->RemappingInfo.SetInfos[Set];

			DynamicOffsetsStart[Set] = TotalNumDynamicOffsets;

			uint32 NumDynamicOffsets = DSWriter[Set].SetupDescriptorWrites(SetInfo.Types, CurrentDescriptorWrite, CurrentImageInfo, CurrentBufferInfo, CurrentBindingToDynamicOffsetMap);
			TotalNumDynamicOffsets += NumDynamicOffsets;

			CurrentDescriptorWrite += SetInfo.Types.Num();
			CurrentImageInfo += SetInfo.NumImageInfos;
			CurrentBufferInfo += SetInfo.NumBufferInfos;
			CurrentBindingToDynamicOffsetMap += SetInfo.Types.Num();
		}
	}

	DynamicOffsets.AddZeroed(TotalNumDynamicOffsets);
	for (int32 Set = 0; Set < NumSets; ++Set)
	{
		DSWriter[Set].DynamicOffsets = DynamicOffsetsStart[Set] + DynamicOffsets.GetData();
	}

	DescriptorSetHandles.AddZeroed(NumSets);
}

template<bool bUseDynamicGlobalUBs>
bool FVulkanGraphicsPipelineDescriptorState::InternalUpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUpdateDescriptorSets);
#endif

	const uint32 HasDescriptorsInSetMask = PipelineDescriptorInfo->HasDescriptorsInSetMask;
	// Early exit
	if (!HasDescriptorsInSetMask)
	{
		return false;
	}

	bool bNeedsWrite = ShouldAlwaysWriteDescriptors();
	if (!bNeedsWrite)
	{
		for (int32 Index = 0; Index < ShaderStage::NumStages; ++Index)
		{
			bNeedsWrite = bNeedsWrite || ResourcesDirtyPerSet[Index];
		}
	}

	FVulkanUniformBufferUploader* UniformBufferUploader = CmdListContext->GetUniformBufferUploader();
	uint8* CPURingBufferBase = (uint8*)UniformBufferUploader->GetCPUMappedPointer();
	const VkDeviceSize UBOffsetAlignment = Device->GetLimits().minUniformBufferOffsetAlignment;

	const FDescriptorSetRemappingInfo* RESTRICT RemappingInfo = PipelineDescriptorInfo->RemappingInfo;

	// Process updates
	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanApplyPackedUniformBuffers);
#endif
		uint32 RemainingPackedUBStagesMask = UsedPackedUBStagesMask;
		int32 Stage = 0;
		while (RemainingPackedUBStagesMask > 0)
		{
			if (RemainingPackedUBStagesMask & 1)
			{
				bool bPackedUBChanged = false;
				uint32 DescriptorSet = RemappingInfo->StageInfos[Stage].PackedUBDescriptorSet;
				if (PackedUniformBuffersDirty[Stage] != 0)
				{
					bPackedUBChanged = UpdatePackedUniformBuffers<bUseDynamicGlobalUBs>(UBOffsetAlignment, RemappingInfo->StageInfos[Stage].PackedUBBindingIndices.GetData(), PackedUniformBuffers[Stage], DSWriter[DescriptorSet], UniformBufferUploader, CPURingBufferBase, PackedUniformBuffersDirty[Stage], CmdBuffer);
				}
				bNeedsWrite = bNeedsWrite || bPackedUBChanged;
			}

			RemainingPackedUBStagesMask >>= 1;
			++Stage;
		}
	}

	// Allocate sets based on what changed
	if (CmdBuffer->AcquirePoolSetAndDescriptorsIfNeeded(*DescriptorSetsLayout, bNeedsWrite, DescriptorSetHandles.GetData()))
	{
		uint32 RemainingSetsMask = HasDescriptorsInSetMask;
		uint32 Set = 0;
		uint32 NumSets = 0;
		while (RemainingSetsMask)
		{
			if (RemainingSetsMask & 1)
			{
				const VkDescriptorSet DescriptorSet = DescriptorSetHandles[Set];
				DSWriter[Set].SetDescriptorSet(DescriptorSet);
				++NumSets;
			}

			++Set;
			RemainingSetsMask >>= 1;
		}

#if VULKAN_ENABLE_AGGRESSIVE_STATS
		INC_DWORD_STAT_BY(STAT_VulkanNumUpdateDescriptors, DSWriteContainer.DescriptorWrites.Num());
		INC_DWORD_STAT_BY(STAT_VulkanNumDescSets, NumSets);
		SCOPE_CYCLE_COUNTER(STAT_VulkanVkUpdateDS);
#endif
		VulkanRHI::vkUpdateDescriptorSets(Device->GetInstanceHandle(), DSWriteContainer.DescriptorWrites.Num(), DSWriteContainer.DescriptorWrites.GetData(), 0, nullptr);

		FMemory::Memzero(ResourcesDirtyPerSet);
		FMemory::Memzero(PackedUniformBuffersDirty);
	}

	return true;
}


void FVulkanCommandListContext::RHISetGraphicsPipelineState(FGraphicsPipelineStateRHIParamRef GraphicsState)
{
	FVulkanRHIGraphicsPipelineState* Pipeline = ResourceCast(GraphicsState);
	
#if VULKAN_ENABLE_LRU_CACHE
	if (!Pipeline)
	{
		return; // this happens when we immediately evict an cached PSO...the thing is never actually created
	}
	FVulkanPipelineStateCacheManager* PipelineStateCache = Device->GetPipelineStateCache();
	PipelineStateCache->PipelineLRU.Touch(Pipeline);
#endif
	check(Pipeline);

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (PendingGfxState->SetGfxPipeline(Pipeline) || !CmdBuffer->bHasPipeline)
	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanPipelineBind);
#endif
		PendingGfxState->Bind(CmdBuffer->GetHandle(), TransitionAndLayoutManager.CurrentFramebuffer);
		CmdBuffer->bHasPipeline = true;
		PendingGfxState->MarkNeedsDynamicStates();
		PendingGfxState->StencilRef = 0;
	}

	// Yuck - Bind pending pixel shader UAVs from SetRenderTargets
	{
		for (int32 Index = 0; Index < PendingPixelUAVs.Num(); ++Index)
		{
			PendingGfxState->SetUAVForStage(ShaderStage::Pixel, PendingPixelUAVs[Index].BindIndex, PendingPixelUAVs[Index].UAV);
		}
	}
}


template bool FVulkanGraphicsPipelineDescriptorState::InternalUpdateDescriptorSets<true>(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer);
template bool FVulkanGraphicsPipelineDescriptorState::InternalUpdateDescriptorSets<false>(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer);
template bool FVulkanComputePipelineDescriptorState::InternalUpdateDescriptorSets<true>(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer);
template bool FVulkanComputePipelineDescriptorState::InternalUpdateDescriptorSets<false>(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer);
