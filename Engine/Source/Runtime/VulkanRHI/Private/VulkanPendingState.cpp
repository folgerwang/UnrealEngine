// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanPendingState.cpp: Private VulkanPendingState function definitions.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"
#include "VulkanPipeline.h"
#include "VulkanContext.h"

FVulkanDescriptorPool::FVulkanDescriptorPool(FVulkanDevice* InDevice, const FVulkanDescriptorSetsLayout& InLayout, uint32 MaxSetsAllocations)
	: Device(InDevice)
	, MaxDescriptorSets(0)
	, NumAllocatedDescriptorSets(0)
	, PeakAllocatedDescriptorSets(0)
	, Layout(InLayout)
	, DescriptorPool(VK_NULL_HANDLE)
{
	INC_DWORD_STAT(STAT_VulkanNumDescPools);

	// Descriptor sets number required to allocate the max number of descriptor sets layout.
	// When we're hashing pools with types usage ID the descriptor pool can be used for different layouts so the initial layout does not make much sense.
	// In the latter case we'll be probably overallocating the descriptor types but given the relatively small number of max allocations this should not have
	// a serious impact.
	MaxDescriptorSets = MaxSetsAllocations*(VULKAN_HASH_POOLS_WITH_TYPES_USAGE_ID ? 1 : Layout.GetLayouts().Num());
	TArray<VkDescriptorPoolSize, TFixedAllocator<VK_DESCRIPTOR_TYPE_RANGE_SIZE>> Types;
	for (uint32 TypeIndex = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; TypeIndex <= VK_DESCRIPTOR_TYPE_END_RANGE; ++TypeIndex)
	{
		VkDescriptorType DescriptorType =(VkDescriptorType)TypeIndex;
		uint32 NumTypesUsed = Layout.GetTypesUsed(DescriptorType);
		if (NumTypesUsed > 0)
		{
			VkDescriptorPoolSize* Type = new(Types) VkDescriptorPoolSize;
			FMemory::Memzero(*Type);
			Type->type = DescriptorType;
			Type->descriptorCount = NumTypesUsed * MaxSetsAllocations;
		}
	}

	VkDescriptorPoolCreateInfo PoolInfo;
	ZeroVulkanStruct(PoolInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
	// you don't need this flag because pool reset feature. Also this flag increase pool size in memory and vkResetDescriptorPool time.
	//PoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	PoolInfo.poolSizeCount = Types.Num();
	PoolInfo.pPoolSizes = Types.GetData();
	PoolInfo.maxSets = MaxDescriptorSets;

#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanVkCreateDescriptorPool);
#endif
	VERIFYVULKANRESULT(VulkanRHI::vkCreateDescriptorPool(Device->GetInstanceHandle(), &PoolInfo, VULKAN_CPU_ALLOCATOR, &DescriptorPool));

	INC_DWORD_STAT_BY(STAT_VulkanNumDescSetsTotal, MaxDescriptorSets);
}

FVulkanDescriptorPool::~FVulkanDescriptorPool()
{
	DEC_DWORD_STAT_BY(STAT_VulkanNumDescSetsTotal, MaxDescriptorSets);
	DEC_DWORD_STAT(STAT_VulkanNumDescPools);

	if (DescriptorPool != VK_NULL_HANDLE)
	{
		VulkanRHI::vkDestroyDescriptorPool(Device->GetInstanceHandle(), DescriptorPool, VULKAN_CPU_ALLOCATOR);
		DescriptorPool = VK_NULL_HANDLE;
	}
}

void FVulkanDescriptorPool::TrackAddUsage(const FVulkanDescriptorSetsLayout& InLayout)
{
	// Check and increment our current type usage
	for (uint32 TypeIndex = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; TypeIndex <= VK_DESCRIPTOR_TYPE_END_RANGE; ++TypeIndex)
	{
		ensure(Layout.GetTypesUsed((VkDescriptorType)TypeIndex) == InLayout.GetTypesUsed((VkDescriptorType)TypeIndex));
	}

	NumAllocatedDescriptorSets += InLayout.GetLayouts().Num();
	PeakAllocatedDescriptorSets = FMath::Max(NumAllocatedDescriptorSets, PeakAllocatedDescriptorSets);
}

void FVulkanDescriptorPool::TrackRemoveUsage(const FVulkanDescriptorSetsLayout& InLayout)
{
	for (uint32 TypeIndex = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; TypeIndex <= VK_DESCRIPTOR_TYPE_END_RANGE; ++TypeIndex)
	{
		check(Layout.GetTypesUsed((VkDescriptorType)TypeIndex) == InLayout.GetTypesUsed((VkDescriptorType)TypeIndex));
	}

	NumAllocatedDescriptorSets -= InLayout.GetLayouts().Num();
}

void FVulkanDescriptorPool::Reset()
{
	if (DescriptorPool != VK_NULL_HANDLE)
	{
		VERIFYVULKANRESULT(VulkanRHI::vkResetDescriptorPool(Device->GetInstanceHandle(), DescriptorPool, 0));
	}

	NumAllocatedDescriptorSets = 0;
}

bool FVulkanDescriptorPool::AllocateDescriptorSets(const VkDescriptorSetAllocateInfo& InDescriptorSetAllocateInfo, VkDescriptorSet* OutSets)
{
	VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = InDescriptorSetAllocateInfo;
	DescriptorSetAllocateInfo.descriptorPool = DescriptorPool;

	return VK_SUCCESS == VulkanRHI::vkAllocateDescriptorSets(Device->GetInstanceHandle(), &DescriptorSetAllocateInfo, OutSets);
}

FVulkanTypedDescriptorPoolSet::~FVulkanTypedDescriptorPoolSet()
{
	for (FPoolList* Pool = PoolListHead; Pool;)
	{
		FPoolList* Next = Pool->Next;

		delete Pool->Element;
		delete Pool;

		Pool = Next;
	}
	PoolsCount = 0;
}

FVulkanDescriptorPool* FVulkanTypedDescriptorPoolSet::PushNewPool()
{
	// Max number of descriptor sets layout allocations
	const uint32 MaxSetsAllocationsBase = 32;
	// Allow max 128 setS per pool (32 << 2)
	const uint32 MaxSetsAllocations = MaxSetsAllocationsBase << FMath::Min(PoolsCount, 2u);

	auto* NewPool = new FVulkanDescriptorPool(Device, Layout, MaxSetsAllocations);

	if (PoolListCurrent)
	{
		PoolListCurrent->Next = new FPoolList(NewPool);
		PoolListCurrent = PoolListCurrent->Next;
	}
	else
	{
		PoolListCurrent = PoolListHead = new FPoolList(NewPool);
	}
	++PoolsCount;

	return NewPool;
}

FVulkanDescriptorPool* FVulkanTypedDescriptorPoolSet::GetFreePool(bool bForceNewPool)
{
	// Likely this
	if (!bForceNewPool)
	{
		return PoolListCurrent->Element;
	}

	if (PoolListCurrent->Next)
	{
		PoolListCurrent = PoolListCurrent->Next;
		return PoolListCurrent->Element;
	}

	return PushNewPool();
}

bool FVulkanTypedDescriptorPoolSet::AllocateDescriptorSets(const FVulkanDescriptorSetsLayout& InLayout, VkDescriptorSet* OutSets)
{
	const TArray<VkDescriptorSetLayout>& LayoutHandles = InLayout.GetHandles();

	if (LayoutHandles.Num() > 0)
	{
		auto* Pool = PoolListCurrent->Element;
		while (!Pool->AllocateDescriptorSets(InLayout.GetAllocateInfo(), OutSets))
		{
			Pool = GetFreePool(true);
		}

#if VULKAN_ENABLE_AGGRESSIVE_STATS
		//INC_DWORD_STAT_BY(STAT_VulkanNumDescSetsTotal, LayoutHandles.Num());
		Pool->TrackAddUsage(InLayout);
#endif

		return true;
	}

	return true;
}

void FVulkanTypedDescriptorPoolSet::Reset()
{
	for (FPoolList* Pool = PoolListHead; Pool; Pool = Pool->Next)
	{
		Pool->Element->Reset();
	}

	PoolListCurrent = PoolListHead;
}

FVulkanDescriptorPoolSetContainer::~FVulkanDescriptorPoolSetContainer()
{
	for (auto& Pair : TypedDescriptorPools)
	{
		FVulkanTypedDescriptorPoolSet* TypedPool = Pair.Value;
		delete TypedPool;
	}

	TypedDescriptorPools.Reset();
}

FVulkanTypedDescriptorPoolSet* FVulkanDescriptorPoolSetContainer::AcquireTypedPoolSet(const FVulkanDescriptorSetsLayout& Layout)
{
	const uint32 Hash = VULKAN_HASH_POOLS_WITH_TYPES_USAGE_ID ? Layout.GetTypesUsageID() : GetTypeHash(Layout);

	FVulkanTypedDescriptorPoolSet* TypedPool = TypedDescriptorPools.FindRef(Hash);

	if (!TypedPool)
	{
		TypedPool = new FVulkanTypedDescriptorPoolSet(Device, Layout);
		TypedDescriptorPools.Add(Hash, TypedPool);
	}

	return TypedPool;
}

void FVulkanDescriptorPoolSetContainer::Reset()
{
	for (auto& Pair : TypedDescriptorPools)
	{
		FVulkanTypedDescriptorPoolSet* TypedPool = Pair.Value;
		TypedPool->Reset();
	}
}

FVulkanDescriptorPoolsManager::~FVulkanDescriptorPoolsManager()
{
	for (auto* PoolSet : PoolSets)
	{
		delete PoolSet;
	}

	PoolSets.Reset();
}

FVulkanDescriptorPoolSetContainer& FVulkanDescriptorPoolsManager::AcquirePoolSetContainer()
{
	FScopeLock ScopeLock(&CS);

	for (auto* PoolSet : PoolSets)
	{
		if (PoolSet->IsUnused()
#if VULKAN_HAS_DEBUGGING_ENABLED
			//todo-rco: Workaround for RenderDoc not supporting resetting descriptor pools
			&& (!GRenderDocFound || (GFrameNumberRenderThread - PoolSet->GetLastFrameUsed() > NUM_FRAMES_TO_WAIT_BEFORE_RELEASING_TO_OS))
#endif
			)
		{
			PoolSet->SetUsed(true);
			return *PoolSet;
		}
	}

	FVulkanDescriptorPoolSetContainer* PoolSet = new FVulkanDescriptorPoolSetContainer(Device);
	PoolSets.Add(PoolSet);

	return *PoolSet;
}

void FVulkanDescriptorPoolsManager::ReleasePoolSet(FVulkanDescriptorPoolSetContainer& PoolSet)
{
	PoolSet.Reset();
	PoolSet.SetUsed(false);
}

void FVulkanDescriptorPoolsManager::GC()
{
	FScopeLock ScopeLock(&CS);

	// Pool sets are forward allocated - iterate from the back to increase the chance of finding an unused one
	for (int32 Index = PoolSets.Num() - 1; Index >= 0; Index--)
	{
		auto* PoolSet = PoolSets[Index];
		if (PoolSet->IsUnused() && GFrameNumberRenderThread - PoolSet->GetLastFrameUsed() > NUM_FRAMES_TO_WAIT_BEFORE_RELEASING_TO_OS)
		{
			PoolSets.RemoveAtSwap(Index, 1, true);

			if (AsyncDeletionTask)
			{
				if (!AsyncDeletionTask->IsDone())
				{
					AsyncDeletionTask->EnsureCompletion();
				}

				AsyncDeletionTask->GetTask().SetPoolSet(PoolSet);
			}
			else
			{
				AsyncDeletionTask = new FAsyncTask<FVulkanAsyncPoolSetDeletionWorker>(PoolSet);
			}

			AsyncDeletionTask->StartBackgroundTask();

			break;
		}
	}
}


FVulkanPendingComputeState::~FVulkanPendingComputeState()
{
	for (auto It = PipelineStates.CreateIterator(); It; ++It)
	{
		FVulkanCommonPipelineDescriptorState* State = It->Value;
		delete State;
	}
}


void FVulkanPendingComputeState::SetSRVForUBResource(uint32 DescriptorSet, uint32 BindingIndex, FVulkanShaderResourceView* SRV)
{
	if (SRV)
	{
		// make sure any dynamically backed SRV points to current memory
		SRV->UpdateView();
		if (SRV->BufferViews.Num() != 0)
		{
			FVulkanBufferView* BufferView = SRV->GetBufferView();
			checkf(BufferView->View != VK_NULL_HANDLE, TEXT("Empty SRV"));
			CurrentState->SetSRVBufferViewState(DescriptorSet, BindingIndex, BufferView);
		}
		else if (SRV->SourceStructuredBuffer)
		{
			CurrentState->SetStorageBuffer(DescriptorSet, BindingIndex, SRV->SourceStructuredBuffer);
		}
		else
		{
			checkf(SRV->TextureView.View != VK_NULL_HANDLE, TEXT("Empty SRV"));
			VkImageLayout Layout = Context.FindLayout(SRV->TextureView.Image);
			CurrentState->SetSRVTextureView(DescriptorSet, BindingIndex, SRV->TextureView, Layout);
		}
	}
	else
	{
		//CurrentState->SetSRVBufferViewState(BindIndex, nullptr);
	}
}

void FVulkanPendingComputeState::SetUAVForUBResource(uint32 DescriptorSet, uint32 BindingIndex, FVulkanUnorderedAccessView* UAV)
{
	if (UAV)
	{
		// make sure any dynamically backed UAV points to current memory
		UAV->UpdateView();
		if (UAV->SourceStructuredBuffer)
		{
			CurrentState->SetStorageBuffer(DescriptorSet, BindingIndex, UAV->SourceStructuredBuffer);
		}
		else if (UAV->BufferView)
		{
			CurrentState->SetUAVTexelBufferViewState(DescriptorSet, BindingIndex, UAV->BufferView);
		}
		else if (UAV->SourceTexture)
		{
			VkImageLayout Layout = Context.FindOrAddLayout(UAV->TextureView.Image, VK_IMAGE_LAYOUT_UNDEFINED);
			if (Layout != VK_IMAGE_LAYOUT_GENERAL)
			{
				FVulkanTextureBase* VulkanTexture = GetVulkanTextureFromRHITexture(UAV->SourceTexture);
				FVulkanCmdBuffer* CmdBuffer = Context.GetCommandBufferManager()->GetActiveCmdBuffer();
				ensure(CmdBuffer->IsOutsideRenderPass());
				Context.GetTransitionAndLayoutManager().TransitionResource(CmdBuffer, VulkanTexture->Surface, VulkanRHI::EImageLayoutBarrier::ComputeGeneralRW);
			}
			CurrentState->SetUAVTextureView(DescriptorSet, BindingIndex, UAV->TextureView, VK_IMAGE_LAYOUT_GENERAL);
		}
		else
		{
			ensure(0);
		}
	}
}


void FVulkanPendingComputeState::PrepareForDispatch(FVulkanCmdBuffer* InCmdBuffer)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanDispatchCallPrepareTime);
#endif

	check(CurrentState);

	const bool bHasDescriptorSets = CurrentState->UpdateDescriptorSets(&Context, InCmdBuffer);

	VkCommandBuffer CmdBuffer = InCmdBuffer->GetHandle();

	{
		//#todo-rco: Move this to SetComputePipeline()
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanPipelineBind);
#endif
		CurrentPipeline->Bind(CmdBuffer);
		if (bHasDescriptorSets)
		{
			CurrentState->BindDescriptorSets(CmdBuffer);
		}
	}
}

FVulkanPendingGfxState::~FVulkanPendingGfxState()
{
	for (auto& Pair : PipelineStates)
	{
		FVulkanGraphicsPipelineDescriptorState* State = Pair.Value;
		delete State;
	}
}

void FVulkanPendingGfxState::PrepareForDraw(FVulkanCmdBuffer* CmdBuffer)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallPrepareTime);
#endif

	check(CmdBuffer->bHasPipeline);

	bool bHasDescriptorSets = CurrentState->UpdateDescriptorSets(&Context, CmdBuffer);

	UpdateDynamicStates(CmdBuffer);

	if (bHasDescriptorSets)
	{
		CurrentState->BindDescriptorSets(CmdBuffer->GetHandle());
	}

	if (bDirtyVertexStreams)
	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanBindVertexStreamsTime);
#endif
		// Its possible to have no vertex buffers
		const FVulkanVertexInputStateInfo& VertexInputStateInfo = CurrentPipeline->Pipeline->GetVertexInputState();
		if (VertexInputStateInfo.AttributesNum == 0)
		{
			// However, we need to verify that there are also no bindings
			check(VertexInputStateInfo.BindingsNum == 0);
			return;
		}

		struct FTemporaryIA
		{
			VkBuffer VertexBuffers[MaxVertexElementCount];
			VkDeviceSize VertexOffsets[MaxVertexElementCount];
			int32 NumUsed = 0;

			void Add(VkBuffer InBuffer, VkDeviceSize InSize)
			{
				check(NumUsed < MaxVertexElementCount);
				VertexBuffers[NumUsed] = InBuffer;
				VertexOffsets[NumUsed] = InSize;
				++NumUsed;
			}
		} TemporaryIA;

		const VkVertexInputAttributeDescription* CurrAttribute = nullptr;
		for (uint32 BindingIndex = 0; BindingIndex < VertexInputStateInfo.BindingsNum; BindingIndex++)
		{
			const VkVertexInputBindingDescription& CurrBinding = VertexInputStateInfo.Bindings[BindingIndex];

			uint32 StreamIndex = VertexInputStateInfo.BindingToStream.FindChecked(BindingIndex);
			FVulkanPendingGfxState::FVertexStream& CurrStream = PendingStreams[StreamIndex];

			// Verify the vertex buffer is set
			if (CurrStream.Stream == VK_NULL_HANDLE)
			{
				// The attribute in stream index is probably compiled out
#if UE_BUILD_DEBUG
				// Lets verify
				for (uint32 AttributeIndex = 0; AttributeIndex < VertexInputStateInfo.AttributesNum; AttributeIndex++)
				{
					if (VertexInputStateInfo.Attributes[AttributeIndex].binding == CurrBinding.binding)
					{
#if VULKAN_ENABLE_SHADER_DEBUG_NAMES
						uint64 VertexShaderKey = GetCurrentShaderKey(ShaderStage::Vertex);
						FVulkanVertexShader* VertexShader = Device->GetShaderFactory().LookupShader<FVulkanVertexShader>(VertexShaderKey);
						UE_LOG(LogVulkanRHI, Warning, TEXT("Missing binding on location %d in '%s' vertex shader"),
							CurrBinding.binding,
							VertexShader ? *VertexShader->GetDebugName() : TEXT("Null"));
#else
						UE_LOG(LogVulkanRHI, Warning, TEXT("Missing binding on location %d in vertex shader"), CurrBinding.binding);
#endif
						ensure(0);
					}
				}
#endif
				continue;
			}

			TemporaryIA.Add(CurrStream.Stream, CurrStream.BufferOffset);
		}

		if (TemporaryIA.NumUsed > 0)
		{
			// Bindings are expected to be in ascending order with no index gaps in between:
			// Correct:		0, 1, 2, 3
			// Incorrect:	1, 0, 2, 3
			// Incorrect:	0, 2, 3, 5
			// Reordering and creation of stream binding index is done in "GenerateVertexInputStateInfo()"
			VulkanRHI::vkCmdBindVertexBuffers(CmdBuffer->GetHandle(), 0, TemporaryIA.NumUsed, TemporaryIA.VertexBuffers, TemporaryIA.VertexOffsets);
		}

		bDirtyVertexStreams = false;
	}
}

void FVulkanPendingGfxState::InternalUpdateDynamicStates(FVulkanCmdBuffer* Cmd)
{
	bool bInCmdNeedsDynamicState = Cmd->bNeedsDynamicStateSet;

	bool bNeedsUpdateViewport = !Cmd->bHasViewport || (FMemory::Memcmp((const void*)&Cmd->CurrentViewport, (const void*)&Viewport, sizeof(VkViewport)) != 0);
	// Validate and update Viewport
	if (bNeedsUpdateViewport)
	{
		ensure(Viewport.width > 0 || Viewport.height > 0);
		VulkanRHI::vkCmdSetViewport(Cmd->GetHandle(), 0, 1, &Viewport);
		FMemory::Memcpy(Cmd->CurrentViewport, Viewport);
		Cmd->bHasViewport = true;
	}

	bool bNeedsUpdateScissor = !Cmd->bHasScissor || (FMemory::Memcmp((const void*)&Cmd->CurrentScissor, (const void*)&Scissor, sizeof(VkRect2D)) != 0);
	if (bNeedsUpdateScissor)
	{
		VulkanRHI::vkCmdSetScissor(Cmd->GetHandle(), 0, 1, &Scissor);
		FMemory::Memcpy(Cmd->CurrentScissor, Scissor);
		Cmd->bHasScissor = true;
	}

	bool bNeedsUpdateStencil = !Cmd->bHasStencilRef || (Cmd->CurrentStencilRef != StencilRef);
	if (bNeedsUpdateStencil)
	{
		VulkanRHI::vkCmdSetStencilReference(Cmd->GetHandle(), VK_STENCIL_FRONT_AND_BACK, StencilRef);
		Cmd->CurrentStencilRef = StencilRef;
		Cmd->bHasStencilRef = true;
	}

	Cmd->bNeedsDynamicStateSet = false;
}

void FVulkanPendingGfxState::UpdateInputAttachments(FVulkanFramebuffer* Framebuffer)
{
	const FVulkanGfxPipelineDescriptorInfo& GfxDescriptorInfo = CurrentState->GetGfxPipelineDescriptorInfo();
	const TArray<FInputAttachmentData>& InputAttachmentData = GfxDescriptorInfo.GetInputAttachmentData();

	for (int32 Index = 0; Index < InputAttachmentData.Num(); ++Index)
	{
		const FInputAttachmentData& AttachmentData = InputAttachmentData[Index];
		switch (AttachmentData.Type)
		{
		case FVulkanShaderHeader::EAttachmentType::Color:
			//#todo-rco: Only supports first render target in frame buffer...
			CurrentState->SetInputAttachment(AttachmentData.DescriptorSet, AttachmentData.BindingIndex, Framebuffer->AttachmentTextureViews[0], VK_IMAGE_LAYOUT_GENERAL);
			break;
		case FVulkanShaderHeader::EAttachmentType::Depth:
			CurrentState->SetInputAttachment(AttachmentData.DescriptorSet, AttachmentData.BindingIndex, Framebuffer->GetPartialDepthTextureView(), VK_IMAGE_LAYOUT_GENERAL);
			break;
		default:
			check(0);
		}
	}
}

void FVulkanPendingGfxState::SetSRVForUBResource(uint8 DescriptorSet, uint32 BindingIndex, FVulkanShaderResourceView* SRV)
{
	if (SRV)
	{
		// make sure any dynamically backed SRV points to current memory
		SRV->UpdateView();
		if (SRV->BufferViews.Num() != 0)
		{
			FVulkanBufferView* BufferView = SRV->GetBufferView();
			checkf(BufferView->View != VK_NULL_HANDLE, TEXT("Empty SRV"));

			CurrentState->SetSRVBufferViewState(DescriptorSet, BindingIndex, BufferView);
		}
		else if (SRV->SourceStructuredBuffer)
		{
			CurrentState->SetStorageBuffer(DescriptorSet, BindingIndex, SRV->SourceStructuredBuffer);
		}
		else
		{
			checkf(SRV->TextureView.View != VK_NULL_HANDLE, TEXT("Empty SRV"));
			VkImageLayout Layout = Context.FindLayout(SRV->TextureView.Image);
			CurrentState->SetSRVTextureView(DescriptorSet, BindingIndex, SRV->TextureView, Layout);
		}
	}
	else
	{
		//CurrentState->SetSRVBufferViewState(Stage, BindIndex, nullptr);
	}
}

void FVulkanPendingGfxState::SetUAVForUBResource(uint8 DescriptorSet, uint32 BindingIndex, FVulkanUnorderedAccessView* UAV)
{
	if (UAV)
	{
		// make sure any dynamically backed UAV points to current memory
		UAV->UpdateView();
		if (UAV->SourceStructuredBuffer)
		{
			CurrentState->SetStorageBuffer(DescriptorSet, BindingIndex, UAV->SourceStructuredBuffer);
		}
		else if (UAV->BufferView)
		{
			CurrentState->SetUAVTexelBufferViewState(DescriptorSet, BindingIndex, UAV->BufferView);
		}
		else if (UAV->SourceTexture)
		{
			VkImageLayout Layout = Context.FindLayout(UAV->TextureView.Image);
			CurrentState->SetUAVTextureView(DescriptorSet, BindingIndex, UAV->TextureView, Layout);
		}
		else
		{
			ensure(0);
		}
	}
}

int32 GDSetCacheTargetSetsPerPool = 4096;
FAutoConsoleVariableRef CVarDSetCacheTargetSetsPerPool(
	TEXT("r.Vulkan.DSetCacheTargetSetsPerPool"),
	GDSetCacheTargetSetsPerPool,
	TEXT("Target number of descriptor set allocations per single pool.\n"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

int32 GDSetCacheMaxPoolLookups = 2;
FAutoConsoleVariableRef CVarDSetCacheMaxPoolLookups(
	TEXT("r.Vulkan.DSetCacheMaxPoolLookups"),
	GDSetCacheMaxPoolLookups,
	TEXT("Maximum count of pool's caches to lookup before allocating new descriptor.\n"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

FVulkanGenericDescriptorPool::FVulkanGenericDescriptorPool(FVulkanDevice* InDevice, uint32 InMaxDescriptorSets)
	: Device(InDevice)
	, MaxDescriptorSets(InMaxDescriptorSets)
	, DescriptorPool(VK_NULL_HANDLE)
{
	// Based on statisticts of runing BR_50v50.replay
	// TODO Need a better solution
	const uint32 LimitMaxUniformBuffers = MaxDescriptorSets * 2;
	const uint32 LimitMaxSamplers = MaxDescriptorSets / 2;
	const uint32 LimitMaxCombinedImageSamplers = MaxDescriptorSets * 3;
	const uint32 LimitMaxUniformTexelBuffers = MaxDescriptorSets / 2;
	const uint32 LimitMaxStorageTexelBuffers = MaxDescriptorSets / 4;
	const uint32 LimitMaxStorageBuffers = MaxDescriptorSets / 4;
	const uint32 LimitMaxStorageImage = MaxDescriptorSets / 4;
	const uint32 LimitMaxSampledImages = MaxDescriptorSets * 2;
	const uint32 LimitMaxInputAttachments = MaxDescriptorSets / 16;

	TArray<VkDescriptorPoolSize> Types;
	VkDescriptorPoolSize* Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	Type->descriptorCount = LimitMaxUniformBuffers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	Type->descriptorCount = LimitMaxUniformBuffers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_SAMPLER;
	Type->descriptorCount = LimitMaxSamplers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	Type->descriptorCount = LimitMaxCombinedImageSamplers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	Type->descriptorCount = LimitMaxSampledImages;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
	Type->descriptorCount = LimitMaxUniformTexelBuffers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
	Type->descriptorCount = LimitMaxStorageTexelBuffers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	Type->descriptorCount = LimitMaxStorageBuffers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	Type->descriptorCount = LimitMaxStorageImage;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	Type->descriptorCount = LimitMaxInputAttachments;

	VkDescriptorPoolCreateInfo PoolInfo;
	ZeroVulkanStruct(PoolInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
	PoolInfo.poolSizeCount = Types.Num();
	PoolInfo.pPoolSizes = Types.GetData();
	PoolInfo.maxSets = MaxDescriptorSets;

#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanVkCreateDescriptorPool);
#endif
	VERIFYVULKANRESULT(VulkanRHI::vkCreateDescriptorPool(Device->GetInstanceHandle(), &PoolInfo, VULKAN_CPU_ALLOCATOR, &DescriptorPool));

	INC_DWORD_STAT_BY(STAT_VulkanNumDescSetsTotal, MaxDescriptorSets);
	INC_DWORD_STAT(STAT_VulkanNumDescPools);
}

FVulkanGenericDescriptorPool::~FVulkanGenericDescriptorPool()
{
	if (DescriptorPool != VK_NULL_HANDLE)
	{
		DEC_DWORD_STAT_BY(STAT_VulkanNumDescSetsTotal, MaxDescriptorSets);
		DEC_DWORD_STAT(STAT_VulkanNumDescPools);

		VulkanRHI::vkDestroyDescriptorPool(Device->GetInstanceHandle(), DescriptorPool, VULKAN_CPU_ALLOCATOR);
	}
}

void FVulkanGenericDescriptorPool::Reset()
{
	check(DescriptorPool != VK_NULL_HANDLE);
	VERIFYVULKANRESULT(VulkanRHI::vkResetDescriptorPool(Device->GetInstanceHandle(), DescriptorPool, 0));
}

bool FVulkanGenericDescriptorPool::AllocateDescriptorSet(VkDescriptorSetLayout Layout, VkDescriptorSet& OutSet)
{
	check(DescriptorPool != VK_NULL_HANDLE);

	VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo;
	ZeroVulkanStruct(DescriptorSetAllocateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
	DescriptorSetAllocateInfo.descriptorPool = DescriptorPool;
	DescriptorSetAllocateInfo.descriptorSetCount = 1;
	DescriptorSetAllocateInfo.pSetLayouts = &Layout;

	return (VK_SUCCESS == VulkanRHI::vkAllocateDescriptorSets(Device->GetInstanceHandle(), &DescriptorSetAllocateInfo, &OutSet));
}

FVulkanDescriptorSetCache::FVulkanDescriptorSetCache(FVulkanDevice* InDevice)
	: Device(InDevice)
	, PoolAllocRatio(0.0f)
{
	constexpr uint32 ProbePoolMaxNumSets = 128; // Used for initial estimation of the Allocation Ratio
	CachedPools.Add(MakeUnique<FCachedPool>(Device, ProbePoolMaxNumSets));
}

FVulkanDescriptorSetCache::~FVulkanDescriptorSetCache()
{
}

void FVulkanDescriptorSetCache::UpdateAllocRatio()
{
	const float FilterParam = ((PoolAllocRatio > 0.0f) ? 2.0f : 0.0f);
	PoolAllocRatio = (PoolAllocRatio * FilterParam + CachedPools[0]->CalcAllocRatio()) / (FilterParam + 1.0f);
}

void FVulkanDescriptorSetCache::AddCachedPool()
{
	check(PoolAllocRatio > 0.0f);
	const uint32 MaxDescriptorSets = FMath::RoundFromZero(GDSetCacheTargetSetsPerPool / PoolAllocRatio);
	if (FreePool)
	{
		constexpr float MinErrorTolerance = -0.10f;
		constexpr float MaxErrorTolerance = 0.50f;
		const float Error = ((static_cast<float>(FreePool->GetMaxDescriptorSets()) -
			static_cast<float>(MaxDescriptorSets)) / static_cast<float>(MaxDescriptorSets));
		if ((Error >= MinErrorTolerance) && (Error <= MaxErrorTolerance))
		{
			FreePool->Reset();
			CachedPools.EmplaceAt(0, MoveTemp(FreePool));
			return;
		}
		UE_LOG(LogVulkanRHI, Display, TEXT("FVulkanDescriptorSetCache::AddCachedPool() MaxDescriptorSets Error: %f. Tolerance: [%f..%f]."),
			static_cast<double>(Error), static_cast<double>(MinErrorTolerance), static_cast<double>(MaxErrorTolerance));
		FreePool.Reset();
	}
	CachedPools.EmplaceAt(0, MakeUnique<FCachedPool>(Device, MaxDescriptorSets));
}

void FVulkanDescriptorSetCache::GetDescriptorSets(const FVulkanDSetsKey& DSetsKey, const FVulkanDescriptorSetsLayout& SetsLayout,
	TArray<FVulkanDescriptorSetWriter>& DSWriters, VkDescriptorSet* OutSets)
{
	check(CachedPools.Num() > 0);

	for (int32 Index = 0; (Index < GDSetCacheMaxPoolLookups) && (Index < CachedPools.Num()); ++Index)
	{
		if (CachedPools[Index]->FindDescriptorSets(DSetsKey, OutSets))
		{
			return;
		}
	}

	bool bFirstTime = true;
	while (!CachedPools[0]->CreateDescriptorSets(DSetsKey, SetsLayout, DSWriters, OutSets))
	{
		checkf(bFirstTime, TEXT("FATAL! Failed to create descriptor sets from new pool!"));
		bFirstTime = false;
		UpdateAllocRatio();
		AddCachedPool();
	}
}

void FVulkanDescriptorSetCache::GC()
{
	// Loop is for OOM safety. Normally there would be at most 1 loop.
	while ((CachedPools.Num() > GDSetCacheMaxPoolLookups) && CachedPools.Last()->CanGC())
	{
		const uint32 RemoveIndex = (CachedPools.Num() - 1);
		if (FreePool)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("FVulkanDescriptorSetCache::GC() Free Pool is not empty! Too small r.Vulkan.DSetCacheTargetSetsPerPool?"));
		}
		FreePool = MoveTemp(CachedPools[RemoveIndex]);
		CachedPools.RemoveAt(RemoveIndex, 1, false);
	}
}

const float FVulkanDescriptorSetCache::FCachedPool::MinAllocRatio = 0.5f;
const float FVulkanDescriptorSetCache::FCachedPool::MaxAllocRatio = 16.0f;

bool FVulkanDescriptorSetCache::FCachedPool::FindDescriptorSets(const FVulkanDSetsKey& DSetsKey, VkDescriptorSet* OutSets)
{
	FSetsEntry* SetsEntry = SetsCache.Find(DSetsKey);
	if (!SetsEntry)
	{
		return false;
	}

	FMemory::Memcpy(OutSets, &SetsEntry->Sets, sizeof(VkDescriptorSet) * SetsEntry->NumSets);
	RecentFrame = GFrameNumberRenderThread;

	return true;
}

bool FVulkanDescriptorSetCache::FCachedPool::CreateDescriptorSets(const FVulkanDSetsKey& DSetsKey, const FVulkanDescriptorSetsLayout& SetsLayout,
	TArray<FVulkanDescriptorSetWriter>& DSWriters, VkDescriptorSet* OutSets)
{
	FSetsEntry NewSetEntry{};

	NewSetEntry.NumSets = DSWriters.Num();
	check(NewSetEntry.NumSets <= NewSetEntry.Sets.Num());
	check(NewSetEntry.NumSets == SetsLayout.GetHandles().Num());

	for (int32 Index = 0; Index < NewSetEntry.NumSets; ++Index)
	{
		FVulkanDescriptorSetWriter& DSWriter = DSWriters[Index];
		if (DSWriter.GetNumWrites() == 0) // Should not normally happen
		{
			NewSetEntry.Sets[Index] = VK_NULL_HANDLE;
			continue;
		}
		if (VkDescriptorSet* FoundSet = SetCache.Find(DSWriter.GetKey()))
		{
			NewSetEntry.Sets[Index] = *FoundSet;
			continue;
		}

		if ((SetCache.Num() == SetCapacity) ||
			!Pool.AllocateDescriptorSet(SetsLayout.GetHandles()[Index], NewSetEntry.Sets[Index]))
		{
			return false;
		}
		SetCache.Emplace(DSWriter.GetKey().CopyDeep(), NewSetEntry.Sets[Index]);

		DSWriter.SetDescriptorSet(NewSetEntry.Sets[Index]);

#if VULKAN_ENABLE_AGGRESSIVE_STATS
		INC_DWORD_STAT_BY(STAT_VulkanNumUpdateDescriptors, DSWriter.GetNumWrites());
		INC_DWORD_STAT(STAT_VulkanNumDescSets);
		SCOPE_CYCLE_COUNTER(STAT_VulkanVkUpdateDS);
#endif
		VulkanRHI::vkUpdateDescriptorSets(Pool.GetDevice()->GetInstanceHandle(),
			DSWriter.GetNumWrites(), DSWriter.GetWriteDescriptors(), 0, nullptr);
	}

	SetsCache.Emplace(DSetsKey.CopyDeep(), MoveTemp(NewSetEntry));

	FMemory::Memcpy(OutSets, &NewSetEntry.Sets, sizeof(VkDescriptorSet) * NewSetEntry.NumSets);
	RecentFrame = GFrameNumberRenderThread;

	return true;
}

bool FVulkanDescriptorSetCache::FCachedPool::CanGC() const
{
	constexpr uint32 FramesBeforeGC = NUM_FRAMES_TO_WAIT_BEFORE_RELEASING_TO_OS;
	return ((GFrameNumberRenderThread - RecentFrame) > FramesBeforeGC);
}

float FVulkanDescriptorSetCache::FCachedPool::CalcAllocRatio() const
{
	float AllocRatio = (static_cast<float>(SetCache.Num()) / static_cast<float>(Pool.GetMaxDescriptorSets()));
	if (AllocRatio < MinAllocRatio)
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("FVulkanDescriptorSetCache::FCachedPool::CalcAllocRatio() Pool Allocation Ratio is too low: %f. Using: %f."),
			static_cast<double>(AllocRatio), static_cast<double>(MinAllocRatio));
		AllocRatio = MinAllocRatio;
	}
	return AllocRatio;
}
