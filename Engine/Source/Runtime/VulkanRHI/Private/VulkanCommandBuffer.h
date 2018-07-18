// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanCommandBuffer.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "VulkanConfiguration.h"

class FVulkanDevice;
class FVulkanCommandBufferPool;
class FVulkanCommandBufferManager;
class FVulkanRenderTargetLayout;
class FVulkanQueue;
#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
class FVulkanDescriptorPoolSetContainer;
#endif

namespace VulkanRHI
{
	class FFence;
	class FSemaphore;
}

class FVulkanCmdBuffer
{
protected:
	friend class FVulkanCommandBufferManager;
	friend class FVulkanCommandBufferPool;
	friend class FVulkanQueue;

	FVulkanCmdBuffer(FVulkanDevice* InDevice, FVulkanCommandBufferPool* InCommandBufferPool);
	~FVulkanCmdBuffer();

public:
	FVulkanCommandBufferPool* GetOwner()
	{
		return CommandBufferPool;
	}

	inline bool IsInsideRenderPass() const
	{
		return State == EState::IsInsideRenderPass;
	}

	inline bool IsOutsideRenderPass() const
	{
		return State == EState::IsInsideBegin;
	}

	inline bool HasBegun() const
	{
		return State == EState::IsInsideBegin || State == EState::IsInsideRenderPass;
	}

	inline bool HasEnded() const
	{
		return State == EState::HasEnded;
	}

	inline bool IsSubmitted() const
	{
		return State == EState::Submitted;
	}

	inline VkCommandBuffer GetHandle()
	{
		return CommandBufferHandle;
	}

	inline volatile uint64 GetFenceSignaledCounter() const
	{
		return FenceSignaledCounter;
	}

	inline volatile uint64 GetSubmittedFenceCounter() const
	{
		return SubmittedFenceCounter;
	}

	inline bool HasValidTiming() const
	{
		return (Timing != nullptr) && (FMath::Abs((int64)FenceSignaledCounter - (int64)LastValidTiming) < 3);
	}
	
	void AddWaitSemaphore(VkPipelineStageFlags InWaitFlags, VulkanRHI::FSemaphore* InWaitSemaphore);

	void Begin();
	void End();

	enum class EState
	{
		ReadyForBegin,
		IsInsideBegin,
		IsInsideRenderPass,
		HasEnded,
		Submitted,
	};

	bool bNeedsDynamicStateSet;
	bool bHasPipeline;
	bool bHasViewport;
	bool bHasScissor;
	bool bHasStencilRef;

	VkViewport CurrentViewport;
	VkRect2D CurrentScissor;
	uint32 CurrentStencilRef;

	// You never want to call Begin/EndRenderPass directly as it will mess up with the FTransitionAndLayoutManager
	void BeginRenderPass(const FVulkanRenderTargetLayout& Layout, class FVulkanRenderPass* RenderPass, class FVulkanFramebuffer* Framebuffer, const VkClearValue* AttachmentClearValues);
	void EndRenderPass()
	{
		checkf(IsInsideRenderPass(), TEXT("Can't EndRP as we're NOT inside one! CmdBuffer 0x%p State=%d"), CommandBufferHandle, (int32)State);
		VulkanRHI::vkCmdEndRenderPass(CommandBufferHandle);
		State = EState::IsInsideBegin;
	}

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	FVulkanDescriptorPoolSetContainer* CurrentDescriptorPoolSetContainer = nullptr;
#endif

private:
	FVulkanDevice* Device;
	VkCommandBuffer CommandBufferHandle;
	EState State;

	TArray<VkPipelineStageFlags> WaitFlags;
	TArray<VulkanRHI::FSemaphore*> WaitSemaphores;
	TArray<VulkanRHI::FSemaphore*> SubmittedWaitSemaphores;

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	void AcquirePoolSet();
#endif

	void MarkSemaphoresAsSubmitted()
	{
		WaitFlags.Reset();
		// Move to pending delete list
		SubmittedWaitSemaphores = WaitSemaphores;
		WaitSemaphores.Reset();
	}

	// Do not cache this pointer as it might change depending on VULKAN_REUSE_FENCES
	VulkanRHI::FFence* Fence;

	// Last value passed after the fence got signaled
	volatile uint64 FenceSignaledCounter;
	// Last value when we submitted the cmd buffer; useful to track down if something waiting for the fence has actually been submitted
	volatile uint64 SubmittedFenceCounter;

	void RefreshFenceStatus();
	void InitializeTimings(FVulkanCommandListContext* InContext);

	FVulkanCommandBufferPool* CommandBufferPool;

	FVulkanGPUTiming* Timing;
	uint64 LastValidTiming;

	friend class FVulkanDynamicRHI;
	friend class FTransitionAndLayoutManager;
};

class FVulkanCommandBufferPool
{
public:
	FVulkanCommandBufferPool(FVulkanDevice* InDevice);

	~FVulkanCommandBufferPool();

	void RefreshFenceStatus(FVulkanCmdBuffer* SkipCmdBuffer = nullptr);

	inline VkCommandPool GetHandle() const
	{
		check(Handle != VK_NULL_HANDLE);
		return Handle;
	}

private:
	FVulkanDevice* Device;
	VkCommandPool Handle;

	FVulkanCmdBuffer* Create();

	TArray<FVulkanCmdBuffer*> CmdBuffers;

	void Create(uint32 QueueFamilyIndex);
	friend class FVulkanCommandBufferManager;
};

class FVulkanCommandBufferManager
{
public:
	FVulkanCommandBufferManager(FVulkanDevice* InDevice, FVulkanCommandListContext* InContext);
	~FVulkanCommandBufferManager();

	inline FVulkanCmdBuffer* GetActiveCmdBuffer()
	{
		if (UploadCmdBuffer)
		{
			SubmitUploadCmdBuffer();
		}

		return ActiveCmdBuffer;
	}

	inline bool HasPendingUploadCmdBuffer() const
	{
		return UploadCmdBuffer != nullptr;
	}

	inline bool HasPendingActiveCmdBuffer() const
	{
		return ActiveCmdBuffer != nullptr;
	}

	VULKANRHI_API FVulkanCmdBuffer* GetUploadCmdBuffer();

	VULKANRHI_API void SubmitUploadCmdBuffer(uint32 NumSignalSemaphores = 0, VkSemaphore* SignalSemaphores = nullptr);

	void SubmitActiveCmdBuffer(VulkanRHI::FSemaphore* SignalSemaphore = nullptr);

	void WaitForCmdBuffer(FVulkanCmdBuffer* CmdBuffer, float TimeInSecondsToWait = 1.0f);

	// Update the fences of all cmd buffers except SkipCmdBuffer
	void RefreshFenceStatus(FVulkanCmdBuffer* SkipCmdBuffer = nullptr)
	{
		Pool.RefreshFenceStatus(SkipCmdBuffer);
	}

	void PrepareForNewActiveCommandBuffer();

	inline VkCommandPool GetHandle() const
	{
		return Pool.GetHandle();
	}

	uint32 CalculateGPUTime();

private:
	FVulkanDevice* Device;
	FVulkanCommandBufferPool Pool;
	FVulkanQueue* Queue;
	FVulkanCmdBuffer* ActiveCmdBuffer;
	FVulkanCmdBuffer* UploadCmdBuffer;
};
