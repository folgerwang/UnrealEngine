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

#define CMD_BUFFER_TIME_TO_WAIT_BEFORE_DELETING		10

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
extern TAutoConsoleVariable<int32> CVarVulkanDebugBarrier;
#endif

const uint32 GNumberOfFramesBeforeDeletingDescriptorPool = 300;

FVulkanCmdBuffer::FVulkanCmdBuffer(FVulkanDevice* InDevice, FVulkanCommandBufferPool* InCommandBufferPool, bool bInIsUploadOnly)
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
	, bIsUploadOnly(bInIsUploadOnly)
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

	{
		FScopeLock ScopeLock(InCommandBufferPool->GetCS());
		VERIFYVULKANRESULT(VulkanRHI::vkAllocateCommandBuffers(Device->GetInstanceHandle(), &CreateCmdBufInfo, &CommandBufferHandle));
	}
	INC_DWORD_STAT(STAT_VulkanNumCmdBuffers);
	Fence = Device->GetFenceManager().AllocateFence();
}

FVulkanCmdBuffer::~FVulkanCmdBuffer()
{
	VulkanRHI::FFenceManager& FenceManager = Device->GetFenceManager();
	if (State == EState::Submitted)
	{
		// Wait 33ms
		uint64 WaitForCmdBufferInNanoSeconds = 33 * 1000 * 1000LL;
		FenceManager.WaitAndReleaseFence(Fence, WaitForCmdBufferInNanoSeconds);
	}
	else
	{
		// Just free the fence, CmdBuffer was not submitted
		FenceManager.ReleaseFence(Fence);
	}

	{
		FScopeLock ScopeLock(CommandBufferPool->GetCS());
		VulkanRHI::vkFreeCommandBuffers(Device->GetInstanceHandle(), CommandBufferPool->GetHandle(), 1, &CommandBufferHandle);
	}
	CommandBufferHandle = VK_NULL_HANDLE;

	DEC_DWORD_STAT(STAT_VulkanNumCmdBuffers);

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

	// Acquire a descriptor pool set on a first render pass
	if (CurrentDescriptorPoolSetContainer == nullptr)
	{
		AcquirePoolSetContainer();
	}
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
	{
		FScopeLock ScopeLock(CommandBufferPool->GetCS());
		checkf(State == EState::ReadyForBegin, TEXT("Can't Begin as we're NOT ready! CmdBuffer 0x%p State=%d"), CommandBufferHandle, (int32)State);
		State = EState::IsInsideBegin;
	}

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
	check(!CurrentDescriptorPoolSetContainer);

	bNeedsDynamicStateSet = true;
}

void FVulkanCmdBuffer::AcquirePoolSetContainer()
{
	check(!CurrentDescriptorPoolSetContainer);
	CurrentDescriptorPoolSetContainer = &Device->GetDescriptorPoolsManager().AcquirePoolSetContainer();
	ensure(TypedDescriptorPoolSets.Num() == 0);
}

bool FVulkanCmdBuffer::AcquirePoolSetAndDescriptorsIfNeeded(const class FVulkanDescriptorSetsLayout& Layout, bool bNeedDescriptors, VkDescriptorSet* OutDescriptors)
{
	//#todo-rco: This only happens when we call draws outside a render pass...
	if (!CurrentDescriptorPoolSetContainer)
	{
		AcquirePoolSetContainer();
	}

	const uint32 Hash = VULKAN_HASH_POOLS_WITH_TYPES_USAGE_ID ? Layout.GetTypesUsageID() : GetTypeHash(Layout);
	FVulkanTypedDescriptorPoolSet*& FoundTypedSet = TypedDescriptorPoolSets.FindOrAdd(Hash);

	if (!FoundTypedSet)
	{
		FoundTypedSet = CurrentDescriptorPoolSetContainer->AcquireTypedPoolSet(Layout);
		bNeedDescriptors = true;
	}

	if (bNeedDescriptors)
	{
		return FoundTypedSet->AllocateDescriptorSets(Layout, OutDescriptors);
	}

	return false;
}

void FVulkanCmdBuffer::RefreshFenceStatus()
{
	if (State == EState::Submitted)
	{
		VulkanRHI::FFenceManager* FenceMgr = Fence->GetOwner();
		if (FenceMgr->IsFenceSignaled(Fence))
		{
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

			if (CurrentDescriptorPoolSetContainer)
			{
				//#todo-rco: Reset here?
				TypedDescriptorPoolSets.Reset();
				Device->GetDescriptorPoolsManager().ReleasePoolSet(*CurrentDescriptorPoolSetContainer);
				CurrentDescriptorPoolSetContainer = nullptr;
			}
			else
			{
				check(TypedDescriptorPoolSets.Num() == 0);
			}

			// Change state at the end to be safe
			State = EState::ReadyForBegin;
		}
	}
	else
	{
		check(!Fence->IsSignaled());
	}
}

FVulkanCommandBufferPool::FVulkanCommandBufferPool(FVulkanDevice* InDevice, FVulkanCommandBufferManager& InMgr)
	: Handle(VK_NULL_HANDLE)
	, Device(InDevice)
	, Mgr(InMgr)
{
}

FVulkanCommandBufferPool::~FVulkanCommandBufferPool()
{
	for (int32 Index = 0; Index < CmdBuffers.Num(); ++Index)
	{
		FVulkanCmdBuffer* CmdBuffer = CmdBuffers[Index];
		delete CmdBuffer;
	}

	VulkanRHI::vkDestroyCommandPool(Device->GetInstanceHandle(), Handle, VULKAN_CPU_ALLOCATOR);
	Handle = VK_NULL_HANDLE;
}

void FVulkanCommandBufferPool::Create(uint32 QueueFamilyIndex)
{
	VkCommandPoolCreateInfo CmdPoolInfo;
	ZeroVulkanStruct(CmdPoolInfo, VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
	CmdPoolInfo.queueFamilyIndex =  QueueFamilyIndex;
	//#todo-rco: Should we use VK_COMMAND_POOL_CREATE_TRANSIENT_BIT?
	CmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VERIFYVULKANRESULT(VulkanRHI::vkCreateCommandPool(Device->GetInstanceHandle(), &CmdPoolInfo, VULKAN_CPU_ALLOCATOR, &Handle));
}

FVulkanCommandBufferManager::FVulkanCommandBufferManager(FVulkanDevice* InDevice, FVulkanCommandListContext* InContext)
	: Device(InDevice)
	, Pool(InDevice, *this)
	, Queue(InContext->GetQueue())
	, ActiveCmdBuffer(nullptr)
	, UploadCmdBuffer(nullptr)
{
	check(Device);

	Pool.Create(Queue->GetFamilyIndex());

	ActiveCmdBuffer = Pool.Create(false);
	ActiveCmdBuffer->InitializeTimings(InContext);
	ActiveCmdBuffer->Begin();
}

FVulkanCommandBufferManager::~FVulkanCommandBufferManager()
{
}

void FVulkanCommandBufferManager::WaitForCmdBuffer(FVulkanCmdBuffer* CmdBuffer, float TimeInSecondsToWait)
{
	FScopeLock ScopeLock(&Pool.CS);
	check(CmdBuffer->IsSubmitted());
	bool bSuccess = Device->GetFenceManager().WaitForFence(CmdBuffer->Fence, (uint64)(TimeInSecondsToWait * 1e9));
	check(bSuccess);
	CmdBuffer->RefreshFenceStatus();
}


void FVulkanCommandBufferManager::SubmitUploadCmdBuffer(uint32 NumSignalSemaphores, VkSemaphore* SignalSemaphores)
{
	FScopeLock ScopeLock(&Pool.CS);
	check(UploadCmdBuffer);
	check(UploadCmdBuffer->CurrentDescriptorPoolSetContainer == nullptr);
	if (!UploadCmdBuffer->IsSubmitted() && UploadCmdBuffer->HasBegun())
	{
		check(UploadCmdBuffer->IsOutsideRenderPass());

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		if (CVarVulkanDebugBarrier.GetValueOnAnyThread() & 4)
		{
			VulkanRHI::InsertHeavyWeightBarrier(UploadCmdBuffer->GetHandle());
		}
#endif

		UploadCmdBuffer->End();
		Queue->Submit(UploadCmdBuffer, NumSignalSemaphores, SignalSemaphores);
		UploadCmdBuffer->SubmittedTime = FPlatformTime::Seconds();
	}

	UploadCmdBuffer = nullptr;
}

void FVulkanCommandBufferManager::SubmitActiveCmdBuffer(VulkanRHI::FSemaphore* SignalSemaphore)
{
	FScopeLock ScopeLock(&Pool.CS);
	check(!UploadCmdBuffer);
	check(ActiveCmdBuffer);
	if (!ActiveCmdBuffer->IsSubmitted() && ActiveCmdBuffer->HasBegun())
	{
		if (!ActiveCmdBuffer->IsOutsideRenderPass())
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Forcing EndRenderPass() for submission"));
			ActiveCmdBuffer->EndRenderPass();
		}

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		if (CVarVulkanDebugBarrier.GetValueOnAnyThread() & 8)
		{
			VulkanRHI::InsertHeavyWeightBarrier(ActiveCmdBuffer->GetHandle());
		}
#endif

		ActiveCmdBuffer->End();
		if (SignalSemaphore)
		{
			Queue->Submit(ActiveCmdBuffer, SignalSemaphore->GetHandle());
		}
		else
		{
			Queue->Submit(ActiveCmdBuffer);
		}
		ActiveCmdBuffer->SubmittedTime = FPlatformTime::Seconds();
	}

	ActiveCmdBuffer = nullptr;
}

FVulkanCmdBuffer* FVulkanCommandBufferPool::Create(bool bIsUploadOnly)
{
	check(Device);
	FVulkanCmdBuffer* CmdBuffer = new FVulkanCmdBuffer(Device, this, bIsUploadOnly);
	CmdBuffers.Add(CmdBuffer);
	check(CmdBuffer);
	return CmdBuffer;
}

void FVulkanCommandBufferPool::RefreshFenceStatus(FVulkanCmdBuffer* SkipCmdBuffer)
{
	FScopeLock ScopeLock(&CS);
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
	FScopeLock ScopeLock(&Pool.CS);
	check(!UploadCmdBuffer);

	for (int32 Index = 0; Index < Pool.CmdBuffers.Num(); ++Index)
	{
		FVulkanCmdBuffer* CmdBuffer = Pool.CmdBuffers[Index];
		CmdBuffer->RefreshFenceStatus();
#if VULKAN_USE_DIFFERENT_POOL_CMDBUFFERS
		if (!CmdBuffer->bIsUploadOnly)
#endif
		{
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
	}

	// All cmd buffers are being executed still
	ActiveCmdBuffer = Pool.Create(false);
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
	FScopeLock ScopeLock(&Pool.CS);
	if (!UploadCmdBuffer)
	{
		for (int32 Index = 0; Index < Pool.CmdBuffers.Num(); ++Index)
		{
			FVulkanCmdBuffer* CmdBuffer = Pool.CmdBuffers[Index];
			CmdBuffer->RefreshFenceStatus();
#if VULKAN_USE_DIFFERENT_POOL_CMDBUFFERS
			if (CmdBuffer->bIsUploadOnly)
#endif
			{
				if (CmdBuffer->State == FVulkanCmdBuffer::EState::ReadyForBegin)
				{
					UploadCmdBuffer = CmdBuffer;
					UploadCmdBuffer->Begin();
					return UploadCmdBuffer;
				}
			}
		}

		// All cmd buffers are being executed still
		UploadCmdBuffer = Pool.Create(true);
		UploadCmdBuffer->Begin();
	}

	return UploadCmdBuffer;
}

#if VULKAN_DELETE_STALE_CMDBUFFERS
struct FRHICommandFreeUnusedCmdBuffers final : public FRHICommand<FRHICommandFreeUnusedCmdBuffers>
{
	FVulkanCommandBufferPool* Pool;

	FRHICommandFreeUnusedCmdBuffers(FVulkanCommandBufferPool* InPool)
		: Pool(InPool)
	{
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		Pool->FreeUnusedCmdBuffers();
	}
};
#endif


void FVulkanCommandBufferPool::FreeUnusedCmdBuffers()
{
#if VULKAN_DELETE_STALE_CMDBUFFERS
	FScopeLock ScopeLock(&CS);
	const double CurrentTime = FPlatformTime::Seconds();
	for (int32 Index = CmdBuffers.Num() - 1; Index >= 0; --Index)
	{
		FVulkanCmdBuffer* CmdBuffer = CmdBuffers[Index];
		if (CmdBuffer->State == FVulkanCmdBuffer::EState::ReadyForBegin && CurrentTime - CmdBuffer->SubmittedTime > CMD_BUFFER_TIME_TO_WAIT_BEFORE_DELETING)
		{
			delete CmdBuffer;
			CmdBuffers.RemoveAtSwap(Index, 1, false);
		}
	}
#endif
}

void FVulkanCommandBufferManager::FreeUnusedCmdBuffers()
{
#if VULKAN_DELETE_STALE_CMDBUFFERS
	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (!IsInRenderingThread() || (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread()))
	{
		Pool.FreeUnusedCmdBuffers();
	}
	else
	{
		check(IsInRenderingThread());
		new (RHICmdList.AllocCommand<FRHICommandFreeUnusedCmdBuffers>()) FRHICommandFreeUnusedCmdBuffers(&Pool);
	}
#endif
}
