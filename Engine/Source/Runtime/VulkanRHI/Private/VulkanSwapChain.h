// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanSwapChain.h: Vulkan viewport RHI definitions.
=============================================================================*/

#pragma once

namespace VulkanRHI
{
	class FFence;
}

class FVulkanQueue;

class FVulkanSwapChain
{
public:
	FVulkanSwapChain(VkInstance InInstance, FVulkanDevice& InDevice, void* WindowHandle, EPixelFormat& InOutPixelFormat, uint32 Width, uint32 Height,
		uint32* InOutDesiredNumBackBuffers, TArray<VkImage>& OutImages, int8 bLockToVsync);

	void Destroy();

	// Has to be negative as we use this also on other callbacks as the acquired image index
	enum class EStatus
	{
		Healthy = 0,
		OutOfDate = -1,
		SurfaceLost = -2,
	};
	EStatus Present(FVulkanQueue* GfxQueue, FVulkanQueue* PresentQueue, VulkanRHI::FSemaphore* BackBufferRenderingDoneSemaphore);

	void RenderThreadPacing();
	inline int8 DoesLockToVsync() { return LockToVsync; }

protected:
	VkSwapchainKHR SwapChain;
	FVulkanDevice& Device;

	VkSurfaceKHR Surface;

	int32 CurrentImageIndex;
	int32 SemaphoreIndex;
	uint32 NumPresentCalls;
	uint32 NumAcquireCalls;
	uint32 InternalWidth = 0;
	uint32 InternalHeight = 0;

	uint32 RTPacingSampleCount = 0;
	double RTPacingPreviousFrameCPUTime = 0;
	double RTPacingSampledDeltaTimeMS = 0;
	
	double NextPresentTargetTime = 0;

	VkInstance Instance;
	TArray<VulkanRHI::FSemaphore*> ImageAcquiredSemaphore;
#if VULKAN_USE_IMAGE_ACQUIRE_FENCES
	TArray<VulkanRHI::FFence*> ImageAcquiredFences;
#endif
	int8 LockToVsync;

#if VULKAN_SUPPORTS_GOOGLE_DISPLAY_TIMING
	TUniquePtr<class FGDTimingFramePacer> GDTimingFramePacer;
#endif

	uint32 PresentID = 0;

	int32 AcquireImageIndex(VulkanRHI::FSemaphore** OutSemaphore);

	friend class FVulkanViewport;
	friend class FVulkanQueue;
};


#if VULKAN_SUPPORTS_GOOGLE_DISPLAY_TIMING
class FGDTimingFramePacer : FNoncopyable
{
public:
	FGDTimingFramePacer(FVulkanDevice& InDevice, VkSwapchainKHR InSwapChain);

	const VkPresentTimesInfoGOOGLE* GetPresentTimesInfo() const
	{
		return ((SyncDuration > 0) ? &PresentTimesInfo : nullptr);
	}

	void ScheduleNextFrame(uint32 InPresentID, int32 SyncInterval); // Call right before present

private:
	void UpdateSyncDuration(int32 SyncInterval);

	uint64 PredictLastScheduledFramePresentTime(uint32 CurrentPresentID) const;
	uint64 CalculateNearestPresentTime(uint64 CpuPresentTime) const;
	uint64 CalculateNearestVsTime(uint64 ActualPresentTime, uint64 TargetTime) const;

	void PollPastFrameInfo();
	void UpdateCpuToGpuPresentDelta(const VkPastPresentationTimingGOOGLE& PastPresentationTiming);

private:
	struct FKnownFrameInfo
	{
		bool bValid = false;
		uint32 PresentID = 0;
		uint64 ActualPresentTime = 0;
	};

private:
	FVulkanDevice& Device;
	VkSwapchainKHR SwapChain;

	VkPresentTimesInfoGOOGLE PresentTimesInfo;
	VkPresentTimeGOOGLE PresentTime;
	uint64 RefreshDuration = 0;
	uint64 HalfRefreshDuration = 0;

	FKnownFrameInfo LastKnownFrameInfo;
	uint64 LastScheduledPresentTime = 0;
	uint64 SyncDuration = 0;
	int32 SyncInterval = 0;

	uint64 CpuPresentTimeHistory[10];
	uint64 CpuToGpuPresentDelta = 0;
};
#endif //VULKAN_SUPPORTS_GOOGLE_DISPLAY_TIMING