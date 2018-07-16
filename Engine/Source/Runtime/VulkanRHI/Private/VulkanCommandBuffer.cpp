// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanCommandBuffer.cpp: Vulkan device RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanContext.h"
#include "VulkanDescriptorSets.h"

static int32 GUseSingleQueue = 0;
static FAutoConsoleVariableRef CVarVulkanUseSingleQueue(
	TEXT("r.Vulkan.UseSingleQueue"),
	GUseSingleQueue,
	TEXT("Forces using the same queue for uploads and graphics.\n")
	TEXT(" 0: Uses multiple queues(default)\n")
	TEXT(" 1: Always uses the gfx queue for submissions"),
	ECVF_Default
);

static int32 GVulkanProfileCmdBuffers = 0;
static FAutoConsoleVariableRef CVarVulkanProfileCmdBuffers(
	TEXT("r.Vulkan.ProfileCmdBuffers"),
	GVulkanProfileCmdBuffers,
	TEXT("Insert GPU timing queries in every cmd buffer\n"),
	ECVF_Default
);

const uint32 GNumberOfFramesBeforeDeletingDescriptorPool = 300;

FVulkanCmdBuffer::FVulkanCmdBuffer(FVulkanDevice* InDevice, FVulkanCommandBufferPool* InCommandBufferPool)
	: bNeedsDynamicStateSet(true)
	, bHasPipeline(false)
	, bHasViewport(false)
	, bHasScissor(false)
	, bHasStencilRef(false)
	, CurrentStencilRef(0)
	, Device(InDevice)
	, CommandBufferHandle(VK_NULL_HANDLE)
	, State(EState::ReadyForBegin)
	, Fence(nullptr)
	, FenceSignaledCounter(0)
	, SubmittedFenceCounter(0)
	, CommandBufferPool(InCommandBufferPool)
	, Timing(nullptr)
	, LastValidTiming(0)
{
	FMemory::Memzero(CurrentViewport);
	FMemory::Memzero(CurrentScissor);
	
	VkCommandBufferAllocateInfo CreateCmdBufInfo;
	ZeroVulkanStruct(CreateCmdBufInfo, VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
	CreateCmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	CreateCmdBufInfo.commandBufferCount = 1;
	CreateCmdBufInfo.commandPool = CommandBufferPool->GetHandle();

	VERIFYVULKANRESULT(VulkanRHI::vkAllocateCommandBuffers(Device->GetInstanceHandle(), &CreateCmdBufInfo, &CommandBufferHandle));
	Fence = Device->GetFenceManager().AllocateFence();
}

FVulkanCmdBuffer::~FVulkanCmdBuffer()
{
	VulkanRHI::FFenceManager& FenceManager = Device->GetFenceManager();
	if (State == EState::Submitted)
	{
		// Wait 60ms
		uint64 WaitForCmdBufferInNanoSeconds = 60 * 1000 * 1000LL;
		FenceManager.WaitAndReleaseFence(Fence, WaitForCmdBufferInNanoSeconds);
	}
	else
	{
		// Just free the fence, CmdBuffer was not submitted
		FenceManager.ReleaseFence(Fence);
	}

	VulkanRHI::vkFreeCommandBuffers(Device->GetInstanceHandle(), CommandBufferPool->GetHandle(), 1, &CommandBufferHandle);
	CommandBufferHandle = VK_NULL_HANDLE;

	if (Timing)
	{
		Timing->Release();
	}
}

void FVulkanCmdBuffer::BeginRenderPass(const FVulkanRenderTargetLayout& Layout, FVulkanRenderPass* RenderPass, FVulkanFramebuffer* Framebuffer, const VkClearValue* AttachmentClearValues)
{
	checkf(IsOutsideRenderPass(), TEXT("Can't BeginRP as already inside one! CmdBuffer 0x%p State=%d"), CommandBufferHandle, (int32)State);

	VkRenderPassBeginInfo Info;
	ZeroVulkanStruct(Info, VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
	Info.renderPass = RenderPass->GetHandle();
	Info.framebuffer = Framebuffer->GetHandle();
	Info.renderArea.offset.x = 0;
	Info.renderArea.offset.y = 0;
	Info.renderArea.extent.width = Framebuffer->GetWidth();
	Info.renderArea.extent.height = Framebuffer->GetHeight();
	Info.clearValueCount = Layout.GetNumUsedClearValues();
	Info.pClearValues = AttachmentClearValues;

	VulkanRHI::vkCmdBeginRenderPass(CommandBufferHandle, &Info, VK_SUBPASS_CONTENTS_INLINE);

	State = EState::IsInsideRenderPass;

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	// Acquire a descriptor pool set on a first render pass
	if (CurrentDescriptorPoolSetContainer == nullptr)
	{
		AcquirePoolSet();
	}
#endif
}

void FVulkanCmdBuffer::End()
{
	checkf(IsOutsideRenderPass(), TEXT("Can't End as we're inside a render pass! CmdBuffer 0x%p State=%d"), CommandBufferHandle, (int32)State);

	if (GVulkanProfileCmdBuffers)
	{
		if (Timing)
		{
			Timing->EndTiming(this);
			LastValidTiming = FenceSignaledCounter;
		}
	}

	VERIFYVULKANRESULT(VulkanRHI::vkEndCommandBuffer(GetHandle()));
	State = EState::HasEnded;
}

inline void FVulkanCmdBuffer::InitializeTimings(FVulkanCommandListContext* InContext)
{
	if (GVulkanProfileCmdBuffers && !Timing)
	{
		if (InContext)
		{
			Timing = new FVulkanGPUTiming(InContext, Device);
			Timing->Initialize();
		}
	}
}

void FVulkanCmdBuffer::AddWaitSemaphore(VkPipelineStageFlags InWaitFlags, VulkanRHI::FSemaphore* InWaitSemaphore)
{
	WaitFlags.Add(InWaitFlags);
	InWaitSemaphore->AddRef();
	check(!WaitSemaphores.Contains(InWaitSemaphore));
	WaitSemaphores.Add(InWaitSemaphore);
}

void FVulkanCmdBuffer::Begin()
{
	checkf(State == EState::ReadyForBegin, TEXT("Can't Begin as we're NOT ready! CmdBuffer 0x%p State=%d"), CommandBufferHandle, (int32)State);

	VkCommandBufferBeginInfo CmdBufBeginInfo;
	ZeroVulkanStruct(CmdBufBeginInfo, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
	CmdBufBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VERIFYVULKANRESULT(VulkanRHI::vkBeginCommandBuffer(CommandBufferHandle, &CmdBufBeginInfo));

	if (GVulkanProfileCmdBuffers)
	{
		InitializeTimings(&Device->GetImmediateContext());
		if (Timing)
		{
			Timing->StartTiming(this);
		}
	}
#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	check(!CurrentDescriptorPoolSetContainer);
#endif

	bNeedsDynamicStateSet = true;
	State = EState::IsInsideBegin;
}

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
void FVulkanCmdBuffer::AcquirePoolSet()
{
	check(!CurrentDescriptorPoolSetContainer);
	CurrentDescriptorPoolSetContainer = &Device->GetDescriptorPoolsManager().AcquirePoolSetContainer();
}
#endif

void FVulkanCmdBuffer::RefreshFenceStatus()
{
	if (State == EState::Submitted)
	{
		VulkanRHI::FFenceManager* FenceMgr = Fence->GetOwner();
		if (FenceMgr->IsFenceSignaled(Fence))
		{
			State = EState::ReadyForBegin;
			bHasPipeline = false;
			bHasViewport = false;
			bHasScissor = false;
			bHasStencilRef = false;

			for (VulkanRHI::FSemaphore* Semaphore : SubmittedWaitSemaphores)
			{
				Semaphore->Release();
			}
			SubmittedWaitSemaphores.Reset();

			FMemory::Memzero(CurrentViewport);
			FMemory::Memzero(CurrentScissor);
			CurrentStencilRef = 0;

			VulkanRHI::vkResetCommandBuffer(CommandBufferHandle, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
#if VULKAN_REUSE_FENCES
			Fence->GetOwner()->ResetFence(Fence);
#else
			VulkanRHI::FFence* PrevFence = Fence;
			Fence = FenceMgr->AllocateFence();
			FenceMgr->ReleaseFence(PrevFence);
#endif
			++FenceSignaledCounter;

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
			if (CurrentDescriptorPoolSetContainer)
			{
				Device->GetDescriptorPoolsManager().ReleasePoolSet(*CurrentDescriptorPoolSetContainer);
				CurrentDescriptorPoolSetContainer = nullptr;
			}
#endif
		}
	}
	else
	{
		check(!Fence->IsSignaled());
	}
}

FVulkanCommandBufferPool::FVulkanCommandBufferPool(FVulkanDevice* InDevice)
	: Device(InDevice)
	, Handle(VK_NULL_HANDLE)
{
}

FVulkanCommandBufferPool::~FVulkanCommandBufferPool()
{
	for (int32 Index = 0; Index < CmdBuffers.Num(); ++Index)
	{
		FVulkanCmdBuffer* CmdBuffer = CmdBuffers[Index];
		delete CmdBuffer;
	}

	VulkanRHI::vkDestroyCommandPool(Device->GetInstanceHandle(), Handle, nullptr);
	Handle = VK_NULL_HANDLE;
}

void FVulkanCommandBufferPool::Create(uint32 QueueFamilyIndex)
{
	VkCommandPoolCreateInfo CmdPoolInfo;
	ZeroVulkanStruct(CmdPoolInfo, VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
	CmdPoolInfo.queueFamilyIndex =  QueueFamilyIndex;
	//#todo-rco: Should we use VK_COMMAND_POOL_CREATE_TRANSIENT_BIT?
	CmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VERIFYVULKANRESULT(VulkanRHI::vkCreateCommandPool(Device->GetInstanceHandle(), &CmdPoolInfo, nullptr, &Handle));
}

FVulkanCommandBufferManager::FVulkanCommandBufferManager(FVulkanDevice* InDevice, FVulkanCommandListContext* InContext)
	: Device(InDevice)
	, Pool(InDevice)
	, Queue(InContext->GetQueue())
	, ActiveCmdBuffer(nullptr)
	, UploadCmdBuffer(nullptr)
{
	check(Device);

	Pool.Create(Queue->GetFamilyIndex());

	ActiveCmdBuffer = Pool.Create();
	ActiveCmdBuffer->InitializeTimings(InContext);
	ActiveCmdBuffer->Begin();
}

FVulkanCommandBufferManager::~FVulkanCommandBufferManager()
{
}

void FVulkanCommandBufferManager::WaitForCmdBuffer(FVulkanCmdBuffer* CmdBuffer, float TimeInSecondsToWait)
{
	check(CmdBuffer->IsSubmitted());
	bool bSuccess = Device->GetFenceManager().WaitForFence(CmdBuffer->Fence, (uint64)(TimeInSecondsToWait * 1e9));
	check(bSuccess);
	CmdBuffer->RefreshFenceStatus();
}


void FVulkanCommandBufferManager::SubmitUploadCmdBuffer(uint32 NumSignalSemaphores, VkSemaphore* SignalSemaphores)
{
	check(UploadCmdBuffer);
#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	check(UploadCmdBuffer->CurrentDescriptorPoolSetContainer == nullptr);
#endif
	if (!UploadCmdBuffer->IsSubmitted() && UploadCmdBuffer->HasBegun())
	{
		check(UploadCmdBuffer->IsOutsideRenderPass());
		UploadCmdBuffer->End();
		Queue->Submit(UploadCmdBuffer, NumSignalSemaphores, SignalSemaphores);
	}

	UploadCmdBuffer = nullptr;
}

void FVulkanCommandBufferManager::SubmitActiveCmdBuffer(VulkanRHI::FSemaphore* SignalSemaphore)
{
	check(!UploadCmdBuffer);
	check(ActiveCmdBuffer);
	if (!ActiveCmdBuffer->IsSubmitted() && ActiveCmdBuffer->HasBegun())
	{
		if (!ActiveCmdBuffer->IsOutsideRenderPass())
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Forcing EndRenderPass() for submission"));
			ActiveCmdBuffer->EndRenderPass();
		}
		ActiveCmdBuffer->End();
		if (SignalSemaphore)
		{
			Queue->Submit(ActiveCmdBuffer, SignalSemaphore->GetHandle());
		}
		else
		{
			Queue->Submit(ActiveCmdBuffer);
		}
	}

	ActiveCmdBuffer = nullptr;
}

FVulkanCmdBuffer* FVulkanCommandBufferPool::Create()
{
	check(Device);
	FVulkanCmdBuffer* CmdBuffer = new FVulkanCmdBuffer(Device, this);
	CmdBuffers.Add(CmdBuffer);
	check(CmdBuffer);
	return CmdBuffer;
}

void FVulkanCommandBufferPool::RefreshFenceStatus(FVulkanCmdBuffer* SkipCmdBuffer)
{
	for (int32 Index = 0; Index < CmdBuffers.Num(); ++Index)
	{
		FVulkanCmdBuffer* CmdBuffer = CmdBuffers[Index];
		if (CmdBuffer != SkipCmdBuffer)
		{
			CmdBuffer->RefreshFenceStatus();
		}
	}
}

void FVulkanCommandBufferManager::PrepareForNewActiveCommandBuffer()
{
	check(!UploadCmdBuffer);

	for (int32 Index = 0; Index < Pool.CmdBuffers.Num(); ++Index)
	{
		FVulkanCmdBuffer* CmdBuffer = Pool.CmdBuffers[Index];
		CmdBuffer->RefreshFenceStatus();
		if (CmdBuffer->State == FVulkanCmdBuffer::EState::ReadyForBegin)
		{
			ActiveCmdBuffer = CmdBuffer;
			ActiveCmdBuffer->Begin();
			return;
		}
		else
		{
			check(CmdBuffer->State == FVulkanCmdBuffer::EState::Submitted);
		}
	}

	// All cmd buffers are being executed still
	ActiveCmdBuffer = Pool.Create();
	ActiveCmdBuffer->Begin();
}

uint32 FVulkanCommandBufferManager::CalculateGPUTime()
{
	uint32 Time = 0;
	for (int32 Index = 0; Index < Pool.CmdBuffers.Num(); ++Index)
	{
		FVulkanCmdBuffer* CmdBuffer = Pool.CmdBuffers[Index];
		if (CmdBuffer->HasValidTiming())
		{
			Time += CmdBuffer->Timing->GetTiming(false);
		}
	}
	return Time;
}

FVulkanCmdBuffer* FVulkanCommandBufferManager::GetUploadCmdBuffer()
{
	if (!UploadCmdBuffer)
	{
		for (int32 Index = 0; Index < Pool.CmdBuffers.Num(); ++Index)
		{
			FVulkanCmdBuffer* CmdBuffer = Pool.CmdBuffers[Index];
			CmdBuffer->RefreshFenceStatus();
			if (CmdBuffer->State == FVulkanCmdBuffer::EState::ReadyForBegin)
			{
				UploadCmdBuffer = CmdBuffer;
				UploadCmdBuffer->Begin();
				return UploadCmdBuffer;
			}
		}

		// All cmd buffers are being executed still
		UploadCmdBuffer = Pool.Create();
		UploadCmdBuffer->Begin();
	}

	return UploadCmdBuffer;
}
