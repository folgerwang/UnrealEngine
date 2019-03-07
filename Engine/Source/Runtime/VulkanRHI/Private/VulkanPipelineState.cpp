// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
	, ComputePipeline(InComputePipeline)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanShaders);
	check(InComputePipeline);
	const FVulkanShaderHeader& CodeHeader = InComputePipeline->GetShaderCodeHeader();
	PackedUniformBuffers.Init(CodeHeader, PackedUniformBuffersMask);

	DescriptorSetsLayout = &InComputePipeline->GetLayout().GetDescriptorSetsLayout();
	PipelineDescriptorInfo = &InComputePipeline->GetComputeLayout().GetComputePipelineDescriptorInfo();

	UsedSetsMask = PipelineDescriptorInfo->HasDescriptorsInSetMask;

	CreateDescriptorWriteInfos();
	InComputePipeline->AddRef();

	ensure(DSWriter.Num() == 0 || DSWriter.Num() == 1);
}

void FVulkanCommonPipelineDescriptorState::CreateDescriptorWriteInfos()
{
	check(DSWriteContainer.DescriptorWrites.Num() == 0);

	const int32 NumSets = DescriptorSetsLayout->RemappingInfo.SetInfos.Num();
	check(UsedSetsMask <= (uint32)(((uint32)1 << NumSets) - 1));

	for (int32 Set = 0; Set < NumSets; ++Set)
	{
		const FDescriptorSetRemappingInfo::FSetInfo& SetInfo = DescriptorSetsLayout->RemappingInfo.SetInfos[Set];
		
		if (UseVulkanDescriptorCache())
		{
			DSWriteContainer.HashableDescriptorInfo.AddZeroed(SetInfo.Types.Num() + 1); // Add 1 for the Layout
		}
		DSWriteContainer.DescriptorWrites.AddZeroed(SetInfo.Types.Num());
		DSWriteContainer.DescriptorImageInfo.AddZeroed(SetInfo.NumImageInfos);
		DSWriteContainer.DescriptorBufferInfo.AddZeroed(SetInfo.NumBufferInfos);

		checkf(SetInfo.Types.Num() < 255, TEXT("Need more bits for BindingToDynamicOffsetMap (currently 8)! Requires %d descriptor bindings in a set!"), SetInfo.Types.Num());
		DSWriteContainer.BindingToDynamicOffsetMap.AddUninitialized(SetInfo.Types.Num());
	}

	FMemory::Memset(DSWriteContainer.BindingToDynamicOffsetMap.GetData(), 255, DSWriteContainer.BindingToDynamicOffsetMap.Num());

	check(DSWriter.Num() == 0);
	DSWriter.AddDefaulted(NumSets);

	const FVulkanSamplerState& DefaultSampler = Device->GetDefaultSampler();
	const FVulkanTextureView& DefaultImageView = Device->GetDefaultImageView();

	FVulkanHashableDescriptorInfo* CurrentHashableDescriptorInfo = nullptr;
	if (UseVulkanDescriptorCache())
	{
		CurrentHashableDescriptorInfo = DSWriteContainer.HashableDescriptorInfo.GetData();
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
		const FDescriptorSetRemappingInfo::FSetInfo& SetInfo = DescriptorSetsLayout->RemappingInfo.SetInfos[Set];

		DynamicOffsetsStart[Set] = TotalNumDynamicOffsets;

		uint32 NumDynamicOffsets = DSWriter[Set].SetupDescriptorWrites(
			SetInfo.Types, CurrentHashableDescriptorInfo,
			CurrentDescriptorWrite, CurrentImageInfo, CurrentBufferInfo, CurrentBindingToDynamicOffsetMap,
			DefaultSampler, DefaultImageView);

		TotalNumDynamicOffsets += NumDynamicOffsets;

		if (CurrentHashableDescriptorInfo) // UseVulkanDescriptorCache()
		{
			CurrentHashableDescriptorInfo += SetInfo.Types.Num();
			CurrentHashableDescriptorInfo->Layout.Max0 = UINT32_MAX;
			CurrentHashableDescriptorInfo->Layout.Max1 = UINT32_MAX;
			CurrentHashableDescriptorInfo->Layout.LayoutId = DescriptorSetsLayout->GetHandleIds()[Set];
			++CurrentHashableDescriptorInfo;
		}

		CurrentDescriptorWrite += SetInfo.Types.Num();
		CurrentImageInfo += SetInfo.NumImageInfos;
		CurrentBufferInfo += SetInfo.NumBufferInfos;
		CurrentBindingToDynamicOffsetMap += SetInfo.Types.Num();
	}

	DynamicOffsets.AddZeroed(TotalNumDynamicOffsets);
	for (int32 Set = 0; Set < NumSets; ++Set)
	{
		DSWriter[Set].DynamicOffsets = DynamicOffsetsStart[Set] + DynamicOffsets.GetData();
	}

	DescriptorSetHandles.AddZeroed(NumSets);
}

template<bool bUseDynamicGlobalUBs>
bool FVulkanComputePipelineDescriptorState::InternalUpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUpdateDescriptorSets);
#endif

	// Early exit
	if (!UsedSetsMask)
	{
		return false;
	}

	FVulkanUniformBufferUploader* UniformBufferUploader = CmdListContext->GetUniformBufferUploader();
	uint8* CPURingBufferBase = (uint8*)UniformBufferUploader->GetCPUMappedPointer();
	const VkDeviceSize UBOffsetAlignment = Device->GetLimits().minUniformBufferOffsetAlignment;

	if (PackedUniformBuffersDirty != 0)
	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanApplyPackedUniformBuffers);
#endif
		UpdatePackedUniformBuffers<bUseDynamicGlobalUBs>(UBOffsetAlignment, PipelineDescriptorInfo->RemappingInfo->StageInfos[0].PackedUBBindingIndices.GetData(), PackedUniformBuffers, DSWriter[0], UniformBufferUploader, CPURingBufferBase, PackedUniformBuffersDirty, CmdBuffer);
		PackedUniformBuffersDirty = 0;
	}

	if (UseVulkanDescriptorCache())
	{
		Device->GetDescriptorSetCache().GetDescriptorSets(GetDSetsKey(), *DescriptorSetsLayout, DSWriter, DescriptorSetHandles.GetData());
	}
	else
	{
		if (!CmdBuffer->AcquirePoolSetAndDescriptorsIfNeeded(*DescriptorSetsLayout, true, DescriptorSetHandles.GetData()))
		{
			return false;
		}

		const VkDescriptorSet DescriptorSet = DescriptorSetHandles[0];
		DSWriter[0].SetDescriptorSet(DescriptorSet);

		{
	#if VULKAN_ENABLE_AGGRESSIVE_STATS
			INC_DWORD_STAT_BY(STAT_VulkanNumUpdateDescriptors, DSWriteContainer.DescriptorWrites.Num());
			INC_DWORD_STAT(STAT_VulkanNumDescSets);
			SCOPE_CYCLE_COUNTER(STAT_VulkanVkUpdateDS);
	#endif
			VulkanRHI::vkUpdateDescriptorSets(Device->GetInstanceHandle(), DSWriteContainer.DescriptorWrites.Num(), DSWriteContainer.DescriptorWrites.GetData(), 0, nullptr);
		}
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

	check(InGfxPipeline);
	check(InGfxPipeline->Pipeline);
	DescriptorSetsLayout = &InGfxPipeline->Pipeline->GetLayout().GetDescriptorSetsLayout();
	PipelineDescriptorInfo = &InGfxPipeline->Pipeline->GetGfxLayout().GetGfxPipelineDescriptorInfo();
	
	UsedSetsMask = PipelineDescriptorInfo->HasDescriptorsInSetMask;
	const FVulkanShaderFactory& ShaderFactory = Device->GetShaderFactory();

	const FVulkanVertexShader* VertexShader = ShaderFactory.LookupShader<FVulkanVertexShader>(InGfxPipeline->GetShaderKey(SF_Vertex));
	check(VertexShader);
	PackedUniformBuffers[ShaderStage::Vertex].Init(VertexShader->GetCodeHeader(), PackedUniformBuffersMask[ShaderStage::Vertex]);

	uint64 PixelShaderKey = InGfxPipeline->GetShaderKey(SF_Pixel);
	if (PixelShaderKey)
	{
		const FVulkanPixelShader* PixelShader = ShaderFactory.LookupShader<FVulkanPixelShader>(PixelShaderKey);
		check(PixelShader);

		PackedUniformBuffers[ShaderStage::Pixel].Init(PixelShader->GetCodeHeader(), PackedUniformBuffersMask[ShaderStage::Pixel]);
	}

#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
	uint64 GeometryShaderKey = InGfxPipeline->GetShaderKey(SF_Geometry);
	if (GeometryShaderKey)
	{
		const FVulkanGeometryShader* GeometryShader = ShaderFactory.LookupShader<FVulkanGeometryShader>(GeometryShaderKey);
		check(GeometryShader);

		PackedUniformBuffers[ShaderStage::Geometry].Init(GeometryShader->GetCodeHeader(), PackedUniformBuffersMask[ShaderStage::Geometry]);
	}

	uint64 HullShaderKey = InGfxPipeline->GetShaderKey(SF_Hull);
	if (HullShaderKey)
	{
		const FVulkanHullShader* HullShader = ShaderFactory.LookupShader<FVulkanHullShader>(HullShaderKey);
		PackedUniformBuffers[ShaderStage::Hull].Init(HullShader->GetCodeHeader(), PackedUniformBuffersMask[ShaderStage::Hull]);
	}

	uint64 DomainShaderKey = InGfxPipeline->GetShaderKey(SF_Domain);
	if (DomainShaderKey)
	{
		const FVulkanDomainShader* DomainShader = ShaderFactory.LookupShader<FVulkanDomainShader>(DomainShaderKey);
		PackedUniformBuffers[ShaderStage::Domain].Init(DomainShader->GetCodeHeader(), PackedUniformBuffersMask[ShaderStage::Domain]);
	}
#endif
	CreateDescriptorWriteInfos();

	//UE_LOG(LogVulkanRHI, Warning, TEXT("GfxPSOState %p For PSO %p Writes:%d"), this, InGfxPipeline, DSWriteContainer.DescriptorWrites.Num());

	InGfxPipeline->AddRef();
}

template<bool bUseDynamicGlobalUBs>
bool FVulkanGraphicsPipelineDescriptorState::InternalUpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUpdateDescriptorSets);
#endif

	// Early exit
	if (!UsedSetsMask)
	{
		return false;
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
		for (int32 Stage = 0; Stage < ShaderStage::NumStages; ++Stage)
		{
			if (PackedUniformBuffersDirty[Stage] != 0)
			{
				const uint32 DescriptorSet = RemappingInfo->StageInfos[Stage].PackedUBDescriptorSet;
				MarkDirty(UpdatePackedUniformBuffers<bUseDynamicGlobalUBs>(UBOffsetAlignment, RemappingInfo->StageInfos[Stage].PackedUBBindingIndices.GetData(), PackedUniformBuffers[Stage], DSWriter[DescriptorSet], UniformBufferUploader, CPURingBufferBase, PackedUniformBuffersDirty[Stage], CmdBuffer));
				PackedUniformBuffersDirty[Stage] = 0;
			}
		}
	}

	if (UseVulkanDescriptorCache())
	{
		if (bIsResourcesDirty)
		{
			Device->GetDescriptorSetCache().GetDescriptorSets(GetDSetsKey(), *DescriptorSetsLayout, DSWriter, DescriptorSetHandles.GetData());
			bIsResourcesDirty = false;
		}
	}
	else
	{
		const bool bNeedsWrite = (bIsResourcesDirty || ShouldAlwaysWriteDescriptors());

		// Allocate sets based on what changed
		if (CmdBuffer->AcquirePoolSetAndDescriptorsIfNeeded(*DescriptorSetsLayout, bNeedsWrite, DescriptorSetHandles.GetData()))
		{
			uint32 RemainingSetsMask = UsedSetsMask;
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

			bIsResourcesDirty = false;
		}
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

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	bool bForceResetPipeline = !CmdBuffer->bHasPipeline;

	if (PendingGfxState->SetGfxPipeline(Pipeline, bForceResetPipeline))
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
