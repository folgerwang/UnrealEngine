// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanViewport.h: Vulkan viewport RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanResources.h"
#include "HAL/CriticalSection.h"

class FVulkanDynamicRHI;
class FVulkanSwapChain;
class FVulkanQueue;

namespace VulkanRHI
{
	class FSemaphore;
}

class FVulkanViewport : public FRHIViewport, public VulkanRHI::FDeviceChild
{
public:
	enum { NUM_BUFFERS = 3 };

	FVulkanViewport(FVulkanDynamicRHI* InRHI, FVulkanDevice* InDevice, void* InWindowHandle, uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen, EPixelFormat InPreferredPixelFormat);
	~FVulkanViewport();

	FVulkanTexture2D* GetBackBuffer(FRHICommandList& RHICmdList);

	void WaitForFrameEventCompletion();

	void IssueFrameEvent();

	inline FIntPoint GetSizeXY() const
	{
		return FIntPoint(SizeX, SizeY);
	}

	virtual void SetCustomPresent(FRHICustomPresent* InCustomPresent) override final
	{
		CustomPresent = InCustomPresent;
	}

	virtual FRHICustomPresent* GetCustomPresent() const override final
	{
		return CustomPresent;
	}

	virtual void Tick(float DeltaTime) override final;

	void AdvanceBackBufferFrame();

	bool Present(FVulkanCommandListContext* Context, FVulkanCmdBuffer* CmdBuffer, FVulkanQueue* Queue, FVulkanQueue* PresentQueue, bool bLockToVsync);

	inline uint32 GetPresentCount() const
	{
		return PresentCount;
	}

protected:
	VkImage BackBufferImages[NUM_BUFFERS];
	VulkanRHI::FSemaphore* RenderingDoneSemaphores[NUM_BUFFERS];
	FVulkanTextureView TextureViews[NUM_BUFFERS];

	// 'Dummy' back buffer
	TRefCountPtr<FVulkanBackBuffer> RenderingBackBuffer;
	TRefCountPtr<FVulkanBackBuffer> RHIBackBuffer;

	/** narrow-scoped section that locks access to back buffer during its recreation*/
	FCriticalSection RecreatingSwapchain;

	FVulkanDynamicRHI* RHI;
	uint32 SizeX;
	uint32 SizeY;
	bool bIsFullscreen;
	EPixelFormat PixelFormat;
	int32 AcquiredImageIndex;
	FVulkanSwapChain* SwapChain;
	void* WindowHandle;
	uint32 PresentCount;

	int8 LockToVsync;

	// Just a pointer, not owned by this class
	VulkanRHI::FSemaphore* AcquiredSemaphore;

	FCustomPresentRHIRef CustomPresent;

	FVulkanCmdBuffer* LastFrameCommandBuffer = nullptr;
	uint64 LastFrameFenceCounter = 0;

	void CreateSwapchain();
	void AcquireBackBuffer(FRHICommandListBase& CmdList, FVulkanBackBuffer* NewBackBuffer);

	void RecreateSwapchain(void* NewNativeWindow, bool bForce = false);
	void RecreateSwapchainFromRT(EPixelFormat PreferredPixelFormat);
	void Resize(uint32 InSizeX, uint32 InSizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat);

	static int32 DoAcquireImageIndex(FVulkanViewport* Viewport);
	bool DoCheckedSwapChainJob(TFunction<int32(FVulkanViewport*)> SwapChainJob);

	friend class FVulkanDynamicRHI;
	friend class FVulkanCommandListContext;
	friend struct FRHICommandAcquireBackBuffer;
};

template<>
struct TVulkanResourceTraits<FRHIViewport>
{
	typedef FVulkanViewport TConcreteType;
};
