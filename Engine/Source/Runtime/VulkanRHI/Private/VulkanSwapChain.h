// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
	VkInstance Instance;
	TArray<VulkanRHI::FSemaphore*> ImageAcquiredSemaphore;
#if VULKAN_USE_IMAGE_ACQUIRE_FENCES
	TArray<VulkanRHI::FFence*> ImageAcquiredFences;
#endif
	int8 LockToVsync;

#if VULKAN_SUPPORTS_GOOGLE_DISPLAY_TIMING
	struct FPresentData
	{
		uint32 PresentID;
		uint64 ActualPresentTime;
		uint64 DesiredPresentTime;
	};
	enum
	{
		MaxHistoricalPresentData = 10,
	};
	uint64 RefreshRateNanoSec = 0;
	int32 NextHistoricalData = 0;
	int32 PreviousSyncInterval = 0;
	FPresentData HistoricalPresentData[MaxHistoricalPresentData];
	uint64 PreviousEmittedPresentTime = 0;
#endif

	uint32 PresentID = 0;

	int32 AcquireImageIndex(VulkanRHI::FSemaphore** OutSemaphore);

	friend class FVulkanViewport;
	friend class FVulkanQueue;
};
