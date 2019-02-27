// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanSwapChain.h: Vulkan viewport RHI definitions.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanSwapChain.h"
#include "VulkanPlatform.h"
#include "Engine/RendererSettings.h"
#include "IHeadMountedDisplayModule.h"


int32 GShouldCpuWaitForFence = 1;
static FAutoConsoleVariableRef CVarCpuWaitForFence(
	TEXT("r.Vulkan.CpuWaitForFence"),
	GShouldCpuWaitForFence,
	TEXT("Whether to have the Cpu wait for the fence in AcquireImageIndex"),
	ECVF_RenderThreadSafe
);

//disabled by default in swapchain creation if the extension frame pacer is available
int32 GVulkanCPURenderThreadFramePacer = 1;
static FAutoConsoleVariableRef CVarVulkanCPURenderThreadFramePacer(
	TEXT("r.Vulkan.CPURenderthreadFramePacer"),
	GVulkanCPURenderThreadFramePacer,
	TEXT("Whether to enable the simple RHI thread CPU Framepacer for Vulkan"),
	ECVF_RenderThreadSafe
);

int32 GVulkanCPURHIFramePacer = 1;
static FAutoConsoleVariableRef CVarVulkanCPURHIFramePacer(
	TEXT("r.Vulkan.CPURHIThreadFramePacer"),
	GVulkanCPURHIFramePacer,
	TEXT("Whether to enable the simple RHI thread CPU Framepacer for Vulkan"),
	ECVF_RenderThreadSafe
);

int32 GVulkanExtensionFramePacer = 1;
static FAutoConsoleVariableRef CVarVulkanExtensionFramePacer(
	TEXT("r.Vulkan.ExtensionFramePacer"),
	GVulkanExtensionFramePacer,
	TEXT("Whether to enable the google extension Framepacer for Vulkan (when available on device)"),
	ECVF_RenderThreadSafe
);

static int32 GPrintVulkanVsyncDebug = 0;

#if !(UE_BUILD_SHIPPING)
static FAutoConsoleVariableRef CVarVulkanDebugVsync(
	TEXT("r.Vulkan.DebugVsync"),
	GPrintVulkanVsyncDebug,
	TEXT("Whether to print vulkan vsync data"),
	ECVF_RenderThreadSafe
);
#endif

#if !UE_BUILD_SHIPPING

bool GSimulateLostSurfaceInNextTick = false;
bool GSimulateSuboptimalSurfaceInNextTick = false;

// A self registering exec helper to check for the VULKAN_* commands.
class FVulkanCommandsHelper : public FSelfRegisteringExec
{
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
	{
		if (FParse::Command(&Cmd, TEXT("VULKAN_SIMULATE_LOST_SURFACE")))
		{
			GSimulateLostSurfaceInNextTick = true;
			Ar.Log(FString::Printf(TEXT("Vulkan: simulating lost surface next frame")));
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("VULKAN_SIMULATE_SUBOPTIMAL_SURFACE")))
		{
			GSimulateSuboptimalSurfaceInNextTick = true;
			Ar.Log(FString::Printf(TEXT("Vulkan: simulating suboptimal surface next frame")));
			return true;
		}
		else
		{
			return false;
		}
	}
};
static FVulkanCommandsHelper GVulkanCommandsHelper;

VkResult SimulateErrors(VkResult Result)
{
	if (GSimulateLostSurfaceInNextTick)
	{
		GSimulateLostSurfaceInNextTick = false;
		return VK_ERROR_SURFACE_LOST_KHR;
	}

	if (GSimulateSuboptimalSurfaceInNextTick)
	{
		GSimulateSuboptimalSurfaceInNextTick = false;
		return VK_SUBOPTIMAL_KHR;
	}

	return Result;
}

#endif

extern TAutoConsoleVariable<int32> GAllowPresentOnComputeQueue;
static TSet<EPixelFormat> GPixelFormatNotSupportedWarning;

FVulkanSwapChain::FVulkanSwapChain(VkInstance InInstance, FVulkanDevice& InDevice, void* WindowHandle, EPixelFormat& InOutPixelFormat, uint32 Width, uint32 Height,
	uint32* InOutDesiredNumBackBuffers, TArray<VkImage>& OutImages, int8 InLockToVsync)
	: SwapChain(VK_NULL_HANDLE)
	, Device(InDevice)
	, Surface(VK_NULL_HANDLE)
	, CurrentImageIndex(-1)
	, SemaphoreIndex(0)
	, NumPresentCalls(0)
	, NumAcquireCalls(0)
	, Instance(InInstance)
	, LockToVsync(InLockToVsync)
{
	check(FVulkanPlatform::SupportsStandardSwapchain());

	NextPresentTargetTime = (FPlatformTime::Seconds() - GStartTime);

	// let the platform create the surface
	FVulkanPlatform::CreateSurface(WindowHandle, Instance, &Surface);

	// Find Pixel format for presentable images
	VkSurfaceFormatKHR CurrFormat;
	FMemory::Memzero(CurrFormat);
	{
		uint32 NumFormats;
		VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkGetPhysicalDeviceSurfaceFormatsKHR(Device.GetPhysicalHandle(), Surface, &NumFormats, nullptr));
		check(NumFormats > 0);

		TArray<VkSurfaceFormatKHR> Formats;
		Formats.AddZeroed(NumFormats);
		VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkGetPhysicalDeviceSurfaceFormatsKHR(Device.GetPhysicalHandle(), Surface, &NumFormats, Formats.GetData()));

		if (InOutPixelFormat == PF_Unknown)
		{
			static const auto* CVarDefaultBackBufferPixelFormat = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultBackBufferPixelFormat"));
			InOutPixelFormat = CVarDefaultBackBufferPixelFormat ? EDefaultBackBufferPixelFormat::Convert2PixelFormat(EDefaultBackBufferPixelFormat::FromInt(CVarDefaultBackBufferPixelFormat->GetValueOnGameThread())) : PF_Unknown;
		}

		if (InOutPixelFormat != PF_Unknown)
		{
			bool bFound = false;
			if (GPixelFormats[InOutPixelFormat].Supported)
			{
				VkFormat Requested = (VkFormat)GPixelFormats[InOutPixelFormat].PlatformFormat;
				for (int32 Index = 0; Index < Formats.Num(); ++Index)
				{
					if (Formats[Index].format == Requested)
					{
						CurrFormat = Formats[Index];
						bFound = true;
						break;
					}
				}

				if (!bFound)
				{
					if (!GPixelFormatNotSupportedWarning.Contains(InOutPixelFormat))
					{
						GPixelFormatNotSupportedWarning.Add(InOutPixelFormat);
						UE_LOG(LogVulkanRHI, Display, TEXT("Requested PixelFormat %d not supported by this swapchain! Falling back to supported swapchain format..."), (uint32)InOutPixelFormat);
					}
					InOutPixelFormat = PF_Unknown;
				}
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Requested PixelFormat %d not supported by this Vulkan implementation!"), (uint32)InOutPixelFormat);
				InOutPixelFormat = PF_Unknown;
			}
		}

		if (InOutPixelFormat == PF_Unknown)
		{
			for (int32 Index = 0; Index < Formats.Num(); ++Index)
			{
				// Reverse lookup
				check(Formats[Index].format != VK_FORMAT_UNDEFINED);
				for (int32 PFIndex = 0; PFIndex < PF_MAX; ++PFIndex)
				{
					if (Formats[Index].format == GPixelFormats[PFIndex].PlatformFormat)
					{
						InOutPixelFormat = (EPixelFormat)PFIndex;
						CurrFormat = Formats[Index];
						UE_LOG(LogVulkanRHI, Verbose, TEXT("No swapchain format requested, picking up VulkanFormat %d"), (uint32)CurrFormat.format);
						break;
					}
				}

				if (InOutPixelFormat != PF_Unknown)
				{
					break;
				}
			}
		}

		if (InOutPixelFormat == PF_Unknown)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Can't find a proper pixel format for the swapchain, trying to pick up the first available"));
			VkFormat PlatformFormat = UEToVkTextureFormat(InOutPixelFormat, false);
			bool bSupported = false;
			for (int32 Index = 0; Index < Formats.Num(); ++Index)
			{
				if (Formats[Index].format == PlatformFormat)
				{
					bSupported = true;
					CurrFormat = Formats[Index];
					break;
				}
			}

			check(bSupported);
		}

		if (InOutPixelFormat == PF_Unknown)
		{
			FString Msg;
			for (int32 Index = 0; Index < Formats.Num(); ++Index)
			{
				if (Index == 0)
				{
					Msg += TEXT("(");
				}
				else
				{
					Msg += TEXT(", ");
				}
				Msg += FString::Printf(TEXT("%d"), (int32)Formats[Index].format);
			}
			if (Formats.Num())
			{
				Msg += TEXT(")");
			}
			UE_LOG(LogVulkanRHI, Fatal, TEXT("Unable to find a pixel format for the swapchain; swapchain returned %d Vulkan formats %s"), Formats.Num(), *Msg);
		}
	}

	VkFormat PlatformFormat = UEToVkTextureFormat(InOutPixelFormat, false);

	Device.SetupPresentQueue(Surface);

	// Fetch present mode
	VkPresentModeKHR PresentMode = VK_PRESENT_MODE_FIFO_KHR;
	if (FVulkanPlatform::SupportsQuerySurfaceProperties())
	{
		// Only dump the present modes the very first time they are queried
		static bool bFirstTimeLog = !!VULKAN_HAS_DEBUGGING_ENABLED;

		uint32 NumFoundPresentModes = 0;
		VERIFYVULKANRESULT(VulkanRHI::vkGetPhysicalDeviceSurfacePresentModesKHR(Device.GetPhysicalHandle(), Surface, &NumFoundPresentModes, nullptr));
		check(NumFoundPresentModes > 0);

		TArray<VkPresentModeKHR> FoundPresentModes;
		FoundPresentModes.AddZeroed(NumFoundPresentModes);
		VERIFYVULKANRESULT(VulkanRHI::vkGetPhysicalDeviceSurfacePresentModesKHR(Device.GetPhysicalHandle(), Surface, &NumFoundPresentModes, FoundPresentModes.GetData()));

		UE_CLOG(bFirstTimeLog, LogVulkanRHI, Display, TEXT("Found %d Surface present modes:"), NumFoundPresentModes);

		bool bFoundPresentModeMailbox = false;
		bool bFoundPresentModeImmediate = false;
		bool bFoundPresentModeFIFO = false;

		for (size_t i = 0; i < NumFoundPresentModes; i++)
		{
			switch (FoundPresentModes[i])
			{
			case VK_PRESENT_MODE_MAILBOX_KHR:
				bFoundPresentModeMailbox = true;
				UE_CLOG(bFirstTimeLog, LogVulkanRHI, Display, TEXT("- VK_PRESENT_MODE_MAILBOX_KHR (%d)"), (int32)VK_PRESENT_MODE_MAILBOX_KHR);
				break;
			case VK_PRESENT_MODE_IMMEDIATE_KHR:
				bFoundPresentModeImmediate = true;
				UE_CLOG(bFirstTimeLog, LogVulkanRHI, Display, TEXT("- VK_PRESENT_MODE_IMMEDIATE_KHR (%d)"), (int32)VK_PRESENT_MODE_IMMEDIATE_KHR);
				break;
			case VK_PRESENT_MODE_FIFO_KHR:
				bFoundPresentModeFIFO = true;
				UE_CLOG(bFirstTimeLog, LogVulkanRHI, Display, TEXT("- VK_PRESENT_MODE_FIFO_KHR (%d)"), (int32)VK_PRESENT_MODE_FIFO_KHR);
				break;
			case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
				UE_CLOG(bFirstTimeLog, LogVulkanRHI, Display, TEXT("- VK_PRESENT_MODE_FIFO_RELAXED_KHR (%d)"), (int32)VK_PRESENT_MODE_FIFO_RELAXED_KHR);
				break;
			default:
				UE_CLOG(bFirstTimeLog, LogVulkanRHI, Display, TEXT("- VkPresentModeKHR %d"), (int32)FoundPresentModes[i]);
				break;
			}
		}

		int32 RequestedPresentMode = -1;
		if (FParse::Value(FCommandLine::Get(), TEXT("vulkanpresentmode="), RequestedPresentMode))
		{
			bool bRequestSuccessful = false;
			switch (RequestedPresentMode)
			{
			case VK_PRESENT_MODE_MAILBOX_KHR:
				if (bFoundPresentModeMailbox)
				{
					PresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
					bRequestSuccessful = true;
				}
				break;
			case VK_PRESENT_MODE_IMMEDIATE_KHR:
				if (bFoundPresentModeImmediate)
				{
					PresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
					bRequestSuccessful = true;
				}
				break;
			case VK_PRESENT_MODE_FIFO_KHR:
				if (bFoundPresentModeFIFO)
				{
					PresentMode = VK_PRESENT_MODE_FIFO_KHR;
					bRequestSuccessful = true;
				}
				break;
			default:
				break;
			}

			if (!bRequestSuccessful)
			{
				UE_CLOG(bFirstTimeLog, LogVulkanRHI, Warning, TEXT("Requested PresentMode (%d) is not handled or available, ignoring..."), RequestedPresentMode);
				RequestedPresentMode = -1;
			}
		}

		if (RequestedPresentMode == -1)
		{
			// Until FVulkanViewport::Present honors SyncInterval, we need to disable vsync for the spectator window if using an HMD.
			const bool bDisableVsyncForHMD = (FVulkanDynamicRHI::HMDVulkanExtensions.IsValid()) ? FVulkanDynamicRHI::HMDVulkanExtensions->ShouldDisableVulkanVSync() : false;

			if (bFoundPresentModeImmediate && (bDisableVsyncForHMD || !LockToVsync))
			{
				PresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			}
			else if (bFoundPresentModeMailbox)
			{
				PresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			}
			else if (bFoundPresentModeFIFO)
			{
				PresentMode = VK_PRESENT_MODE_FIFO_KHR;
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Couldn't find desired PresentMode! Using %d"), static_cast<int32>(FoundPresentModes[0]));
				PresentMode = FoundPresentModes[0];
			}
		}

		UE_CLOG(bFirstTimeLog, LogVulkanRHI, Display, TEXT("Selected VkPresentModeKHR mode %d"), PresentMode);
		bFirstTimeLog = false;
	}

	// Check the surface properties and formats

	VkSurfaceCapabilitiesKHR SurfProperties;
	VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device.GetPhysicalHandle(),
		Surface,
		&SurfProperties));
	VkSurfaceTransformFlagBitsKHR PreTransform;
	if (SurfProperties.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		PreTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		PreTransform = SurfProperties.currentTransform;
	}

	VkCompositeAlphaFlagBitsKHR CompositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
	if (SurfProperties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
	{
		CompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	}

	// 0 means no limit, so use the requested number
	uint32 DesiredNumBuffers = SurfProperties.maxImageCount > 0 ? FMath::Clamp(*InOutDesiredNumBackBuffers, SurfProperties.minImageCount, SurfProperties.maxImageCount) : *InOutDesiredNumBackBuffers;

	uint32 SizeX = FVulkanPlatform::SupportsQuerySurfaceProperties() ? (SurfProperties.currentExtent.width == 0xFFFFFFFF ? Width : SurfProperties.currentExtent.width) : Width;
	uint32 SizeY = FVulkanPlatform::SupportsQuerySurfaceProperties() ? (SurfProperties.currentExtent.height == 0xFFFFFFFF ? Height : SurfProperties.currentExtent.height) : Height;
	//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Create swapchain: %ux%u \n"), SizeX, SizeY);

	VkSwapchainCreateInfoKHR SwapChainInfo;
	ZeroVulkanStruct(SwapChainInfo, VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
	SwapChainInfo.surface = Surface;
	SwapChainInfo.minImageCount = DesiredNumBuffers;
	SwapChainInfo.imageFormat = CurrFormat.format;
	SwapChainInfo.imageColorSpace = CurrFormat.colorSpace;
	SwapChainInfo.imageExtent.width = SizeX;
	SwapChainInfo.imageExtent.height = SizeY;
	SwapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	if (GVulkanDelayAcquireImage == EDelayAcquireImageType::DelayAcquire)
	{
		SwapChainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	SwapChainInfo.preTransform = PreTransform;
	SwapChainInfo.imageArrayLayers = 1;
	SwapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	SwapChainInfo.presentMode = PresentMode;
	SwapChainInfo.oldSwapchain = VK_NULL_HANDLE;
	SwapChainInfo.clipped = VK_TRUE;
	SwapChainInfo.compositeAlpha = CompositeAlpha;

	*InOutDesiredNumBackBuffers = DesiredNumBuffers;

	{
		//#todo-rco: Crappy workaround
		if (SwapChainInfo.imageExtent.width == 0)
		{
			SwapChainInfo.imageExtent.width = Width;
		}
		if (SwapChainInfo.imageExtent.height == 0)
		{
			SwapChainInfo.imageExtent.height = Height;
		}
	}

	VkBool32 bSupportsPresent;
	VERIFYVULKANRESULT(VulkanRHI::vkGetPhysicalDeviceSurfaceSupportKHR(Device.GetPhysicalHandle(), Device.GetPresentQueue()->GetFamilyIndex(), Surface, &bSupportsPresent));
	ensure(bSupportsPresent);

	//ensure(SwapChainInfo.imageExtent.width >= SurfProperties.minImageExtent.width && SwapChainInfo.imageExtent.width <= SurfProperties.maxImageExtent.width);
	//ensure(SwapChainInfo.imageExtent.height >= SurfProperties.minImageExtent.height && SwapChainInfo.imageExtent.height <= SurfProperties.maxImageExtent.height);

	VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkCreateSwapchainKHR(Device.GetInstanceHandle(), &SwapChainInfo, VULKAN_CPU_ALLOCATOR, &SwapChain));

	InternalWidth = FMath::Min(Width, SwapChainInfo.imageExtent.width);
	InternalHeight = FMath::Min(Height, SwapChainInfo.imageExtent.height);

	uint32 NumSwapChainImages;
	VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkGetSwapchainImagesKHR(Device.GetInstanceHandle(), SwapChain, &NumSwapChainImages, nullptr));

	OutImages.AddUninitialized(NumSwapChainImages);
	VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkGetSwapchainImagesKHR(Device.GetInstanceHandle(), SwapChain, &NumSwapChainImages, OutImages.GetData()));

#if VULKAN_USE_IMAGE_ACQUIRE_FENCES
	ImageAcquiredFences.AddUninitialized(NumSwapChainImages);
	VulkanRHI::FFenceManager& FenceMgr = Device.GetFenceManager();
	for (uint32 BufferIndex = 0; BufferIndex < NumSwapChainImages; ++BufferIndex)
	{
		ImageAcquiredFences[BufferIndex] = Device.GetFenceManager().AllocateFence(true);
	}
#endif
	ImageAcquiredSemaphore.AddUninitialized(DesiredNumBuffers);
	for (uint32 BufferIndex = 0; BufferIndex < DesiredNumBuffers; ++BufferIndex)
	{
		ImageAcquiredSemaphore[BufferIndex] = new VulkanRHI::FSemaphore(Device);
		ImageAcquiredSemaphore[BufferIndex]->AddRef();
	}

#if VULKAN_SUPPORTS_GOOGLE_DISPLAY_TIMING
	if (Device.GetOptionalExtensions().HasGoogleDisplayTiming)
	{
		GDTimingFramePacer = MakeUnique<FGDTimingFramePacer>(Device, SwapChain);
		if (GVulkanExtensionFramePacer)
		{
			GVulkanCPURenderThreadFramePacer = 0;
			GVulkanCPURHIFramePacer = 0;
		}
	}
#endif

	PresentID = 0;
}

void FVulkanSwapChain::Destroy()
{
	check(FVulkanPlatform::SupportsStandardSwapchain());

	// We could be responding to an OUT_OF_DATE event and the GPU might not be done with swapchain image, so wait for idle.
	// Alternatively could also check on the fence(s) for the image(s) from the swapchain but then timing out/waiting could become an issue.
	Device.WaitUntilIdle();

	VulkanRHI::vkDestroySwapchainKHR(Device.GetInstanceHandle(), SwapChain, VULKAN_CPU_ALLOCATOR);
	SwapChain = VK_NULL_HANDLE;

#if VULKAN_USE_IMAGE_ACQUIRE_FENCES
	VulkanRHI::FFenceManager& FenceMgr = Device.GetFenceManager();
	for (int32 Index = 0; Index < ImageAcquiredFences.Num(); ++Index)
	{
		FenceMgr.ReleaseFence(ImageAcquiredFences[Index]);
	}
#endif

	//#todo-rco: Enqueue for deletion as we first need to destroy the cmd buffers and queues otherwise validation fails
	for (int BufferIndex = 0; BufferIndex < ImageAcquiredSemaphore.Num(); ++BufferIndex)
	{
		ImageAcquiredSemaphore[BufferIndex]->Release();
	}

	VulkanRHI::vkDestroySurfaceKHR(Instance, Surface, VULKAN_CPU_ALLOCATOR);
	Surface = VK_NULL_HANDLE;
}

int32 FVulkanSwapChain::AcquireImageIndex(VulkanRHI::FSemaphore** OutSemaphore)
{
	check(FVulkanPlatform::SupportsStandardSwapchain());

	// Get the index of the next swapchain image we should render to.
	// We'll wait with an "infinite" timeout, the function will block until an image is ready.
	// The ImageAcquiredSemaphore[ImageAcquiredSemaphoreIndex] will get signaled when the image is ready (upon function return).
	uint32 ImageIndex = 0;
	const int32 PrevSemaphoreIndex = SemaphoreIndex;
	SemaphoreIndex = (SemaphoreIndex + 1) % ImageAcquiredSemaphore.Num();

	// If we have not called present for any of the swapchain images, it will cause a crash/hang
	checkf(!(NumAcquireCalls == ImageAcquiredSemaphore.Num() - 1 && NumPresentCalls == 0), TEXT("vkAcquireNextImageKHR will fail as no images have been presented before acquiring all of them"));
#if VULKAN_USE_IMAGE_ACQUIRE_FENCES
	VulkanRHI::FFenceManager& FenceMgr = Device.GetFenceManager();
	FenceMgr.ResetFence(ImageAcquiredFences[SemaphoreIndex]);
	const VkFence AcquiredFence = ImageAcquiredFences[SemaphoreIndex]->GetHandle();
#else
	const VkFence AcquiredFence = VK_NULL_HANDLE;
#endif
	VkResult Result;
	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanAcquireBackBuffer);
		uint32 IdleStart = FPlatformTime::Cycles();
		Result = VulkanRHI::vkAcquireNextImageKHR(
			Device.GetInstanceHandle(),
			SwapChain,
			UINT64_MAX,
			ImageAcquiredSemaphore[SemaphoreIndex]->GetHandle(),
			AcquiredFence,
			&ImageIndex);

		uint32 ThisCycles = FPlatformTime::Cycles() - IdleStart;
		if (IsInRHIThread())
		{
			GWorkingRHIThreadStallTime += ThisCycles;
		}
		else if (IsInActualRenderingThread())
		{
			GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUPresent] += ThisCycles;
			GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUPresent]++;
		}
	}

	if (Result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		SemaphoreIndex = PrevSemaphoreIndex;
		return (int32)EStatus::OutOfDate;
	}

	if (Result == VK_ERROR_SURFACE_LOST_KHR)
	{
		SemaphoreIndex = PrevSemaphoreIndex;
		return (int32)EStatus::SurfaceLost;
	}

	++NumAcquireCalls;
	*OutSemaphore = ImageAcquiredSemaphore[SemaphoreIndex];

#if VULKAN_HAS_DEBUGGING_ENABLED
	if (Result == VK_ERROR_VALIDATION_FAILED_EXT)
	{
		extern TAutoConsoleVariable<int32> GValidationCvar;
		if (GValidationCvar.GetValueOnRenderThread() == 0)
		{
			UE_LOG(LogVulkanRHI, Fatal, TEXT("vkAcquireNextImageKHR failed with Validation error. Try running with r.Vulkan.EnableValidation=1 to get information from the driver"));
		}
	}
	else
#endif
	{
		checkf(Result == VK_SUCCESS || Result == VK_SUBOPTIMAL_KHR, TEXT("vkAcquireNextImageKHR failed Result = %d"), int32(Result));
	}
	CurrentImageIndex = (int32)ImageIndex;

#if VULKAN_USE_IMAGE_ACQUIRE_FENCES
	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanWaitSwapchain);
		bool bResult = FenceMgr.WaitForFence(ImageAcquiredFences[SemaphoreIndex], UINT64_MAX);
		ensure(bResult);
	}
#endif
	return CurrentImageIndex;
}

#if VULKAN_SUPPORTS_GOOGLE_DISPLAY_TIMING
FGDTimingFramePacer::FGDTimingFramePacer(FVulkanDevice& InDevice, VkSwapchainKHR InSwapChain)
	: Device(InDevice)
	, SwapChain(InSwapChain)
{
	VkRefreshCycleDurationGOOGLE RefreshCycleDuration;
	VkResult Result = VulkanDynamicAPI::vkGetRefreshCycleDurationGOOGLE(Device.GetInstanceHandle(), SwapChain, &RefreshCycleDuration);
	checkf(Result == VK_SUCCESS, TEXT("vkGetRefreshCycleDurationGOOGLE failed: %i"), Result);

	RefreshDuration = RefreshCycleDuration.refreshDuration;
	ensure(RefreshDuration > 0);
	if (RefreshDuration == 0)
	{
		RefreshDuration = 16666667;
	}
	HalfRefreshDuration = (RefreshDuration / 2);

	FMemory::Memzero(CpuPresentTimeHistory);
	FMemory::Memzero(PresentTime); 

	ZeroVulkanStruct(PresentTimesInfo, VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE);
	PresentTimesInfo.swapchainCount = 1;
	PresentTimesInfo.pTimes = &PresentTime;
}

static uint64 TimeNanoseconds()
{
#if PLATFORM_ANDROID
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec*1000000000ull + ts.tv_nsec;
#else
	return (uint64)(FPlatformTime::Seconds()*1000000000.0);
#endif
}

void FGDTimingFramePacer::ScheduleNextFrame(uint32 InPresentID, int32 InSyncInterval)
{
	UpdateSyncDuration(InSyncInterval);
	if (SyncDuration == 0)
	{
		return;
	}

	PollPastFrameInfo();
	if (!LastKnownFrameInfo.bValid)
	{
		LastScheduledPresentTime = 0;
		return;
	}

	const uint64 CpuPresentTime = TimeNanoseconds();
	const int32 HistorySize = ARRAY_COUNT(CpuPresentTimeHistory);
	const int32 HistoryIndex = InPresentID % HistorySize;
	CpuPresentTimeHistory[HistoryIndex] = CpuPresentTime;
	
	uint64 CpuTargetPresentTime = CalculateNearestPresentTime(CpuPresentTime);
	uint64 GpuTargetPresentTime = CalculateNearestVsTime(LastKnownFrameInfo.ActualPresentTime, PredictLastScheduledFramePresentTime(InPresentID) + SyncDuration);
	
	uint64 TargetPresentTime = FMath::Max(CpuTargetPresentTime, GpuTargetPresentTime);
	LastScheduledPresentTime = TargetPresentTime;

	PresentTime.presentID = InPresentID;
	PresentTime.desiredPresentTime = (TargetPresentTime - HalfRefreshDuration);

	if (GPrintVulkanVsyncDebug != 0)
	{
		double cpuP = CpuTargetPresentTime/1000000000.0;
		double gpuP = GpuTargetPresentTime/1000000000.0;
		double desP = PresentTime.desiredPresentTime/1000000000.0;
		double lastP = LastKnownFrameInfo.ActualPresentTime/1000000000.0;
		double cpuDelta = CpuToGpuPresentDelta/1000000000.0;
		double cpuNow = CpuPresentTime/1000000000.0;
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT(" -- ID: %u, desired %.3f, pred-gpu %.3f, pred-cpu %.3f, last: %.3f, cpu-gpu-delta: %.3f, now-cpu %.3f"), PresentTime.presentID, desP, gpuP, cpuP, lastP, cpuDelta, cpuNow);
	}
}

void FGDTimingFramePacer::UpdateSyncDuration(int32 InSyncInterval)
{
	if (SyncInterval == InSyncInterval)
	{
		return;
	}
	SyncInterval = InSyncInterval;

	// reset cached history on sync interval changes
	FMemory::Memzero(CpuPresentTimeHistory);
	LastKnownFrameInfo.bValid = false;
	LastScheduledPresentTime = 0;
	
	SyncDuration = ((1000000000llu * FMath::Clamp(SyncInterval, 0, 3) + 30) / 60);
	if (SyncDuration > 0)
	{
		SyncDuration = (FMath::Max((SyncDuration + HalfRefreshDuration) / RefreshDuration, 1llu) * RefreshDuration);
	}
}

uint64 FGDTimingFramePacer::PredictLastScheduledFramePresentTime(uint32 CurrentPresentID) const
{
	const uint32 PredictFrameCount = (CurrentPresentID - LastKnownFrameInfo.PresentID - 1);
	return FMath::Max(LastScheduledPresentTime, LastKnownFrameInfo.ActualPresentTime + (SyncDuration * PredictFrameCount));
}

uint64 FGDTimingFramePacer::CalculateNearestPresentTime(uint64 CpuPresentTime) const
{
	const uint64 NearestGpuPresentTime = CpuPresentTime + CpuToGpuPresentDelta;
	return CalculateNearestVsTime(LastKnownFrameInfo.ActualPresentTime, NearestGpuPresentTime - HalfRefreshDuration);
}

uint64 FGDTimingFramePacer::CalculateNearestVsTime(uint64 ActualPresentTime, uint64 TargetTime) const
{
	if (TargetTime > ActualPresentTime)
	{
		return (ActualPresentTime + ((TargetTime - ActualPresentTime) + HalfRefreshDuration) / RefreshDuration * RefreshDuration);
	}
	return ActualPresentTime;
}

void FGDTimingFramePacer::PollPastFrameInfo()
{
	for (;;)
	{
		// MUST call once with nullptr to get the count, or the API won't return any results at all.
		uint32 Count = 0;
		VkResult Result = VulkanDynamicAPI::vkGetPastPresentationTimingGOOGLE(Device.GetInstanceHandle(), SwapChain, &Count, nullptr);
		checkf(Result == VK_SUCCESS, TEXT("vkGetPastPresentationTimingGOOGLE failed: %i"), Result);

		if (Count == 0)
		{
			break;
		}
		
		Count = 1;
		VkPastPresentationTimingGOOGLE PastPresentationTiming;
		Result = VulkanDynamicAPI::vkGetPastPresentationTimingGOOGLE(Device.GetInstanceHandle(), SwapChain, &Count, &PastPresentationTiming);
		checkf(Result == VK_SUCCESS || Result == VK_INCOMPLETE, TEXT("vkGetPastPresentationTimingGOOGLE failed: %i"), Result);

		LastKnownFrameInfo.PresentID = PastPresentationTiming.presentID;
		LastKnownFrameInfo.ActualPresentTime = PastPresentationTiming.actualPresentTime;
		LastKnownFrameInfo.bValid = true;

		UpdateCpuToGpuPresentDelta(PastPresentationTiming);
	}
}

void FGDTimingFramePacer::UpdateCpuToGpuPresentDelta(const VkPastPresentationTimingGOOGLE& PastPresentationTiming)
{
	const int32 HistorySize = ARRAY_COUNT(CpuPresentTimeHistory);
	if ((PresentTime.presentID - PastPresentationTiming.presentID) >= HistorySize)
	{
		return;
	}
		
	const int32 HistoryIndex = PastPresentationTiming.presentID % HistorySize;
	const uint64 PastCpuPresentTime = CpuPresentTimeHistory[HistoryIndex];
	if (PastCpuPresentTime == 0)
	{
		CpuToGpuPresentDelta = SyncDuration;
		return;
	}

	// "presentMargin" may be negative despite being unsigned
	const uint64 Delta = PastPresentationTiming.earliestPresentTime - (PastCpuPresentTime + (int64_t&)PastPresentationTiming.presentMargin);
	const uint64 FilterParam = (CpuToGpuPresentDelta == 0) ? 0 : 10; // greater -> smoother
	CpuToGpuPresentDelta = (CpuToGpuPresentDelta * FilterParam + Delta) / (FilterParam + 1);
	// filter out bad frames, in general delta should be 2-4 sync durations
	CpuToGpuPresentDelta = FMath::Min(CpuToGpuPresentDelta, SyncDuration*4);
}
#endif //VULKAN_SUPPORTS_GOOGLE_DISPLAY_TIMING

void FVulkanSwapChain::RenderThreadPacing()
{
	check(IsInRenderingThread());
	const int32 SyncInterval = LockToVsync ? RHIGetSyncInterval() : 0;

	//very naive CPU side frame pacer.
	if (GVulkanCPURenderThreadFramePacer && SyncInterval > 0)
	{
		double NowCPUTime = FPlatformTime::Seconds();
		double DeltaCPUPresentTimeMS = (NowCPUTime - RTPacingPreviousFrameCPUTime) * 1000.0;


		double TargetIntervalWithEpsilonMS = (double)SyncInterval * (1.0 / 60.0) * 1000.0;
		const double IntervalThresholdMS = TargetIntervalWithEpsilonMS * 0.1;

		RTPacingSampledDeltaTimeMS += DeltaCPUPresentTimeMS; RTPacingSampleCount++;

		double SampledDeltaMS = (RTPacingSampledDeltaTimeMS / (double)RTPacingSampleCount) + IntervalThresholdMS;

		if (RTPacingSampleCount > 1000)
		{
			RTPacingSampledDeltaTimeMS = SampledDeltaMS;
			RTPacingSampleCount = 1;
		}

		if (SampledDeltaMS < (TargetIntervalWithEpsilonMS))
		{
			uint32 IdleStart = FPlatformTime::Cycles();
			QUICK_SCOPE_CYCLE_COUNTER(STAT_StallForEmulatedSyncInterval);
			FPlatformProcess::SleepNoStats((TargetIntervalWithEpsilonMS - SampledDeltaMS) * 0.001f);
			if (GPrintVulkanVsyncDebug)
			{
				UE_LOG(LogVulkanRHI, Log, TEXT("CPU RT delta: %f, TargetWEps: %f, sleepTime: %f "), SampledDeltaMS, TargetIntervalWithEpsilonMS, TargetIntervalWithEpsilonMS - DeltaCPUPresentTimeMS);
			}

			uint32 ThisCycles = FPlatformTime::Cycles() - IdleStart;
			GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUPresent] += ThisCycles;
			GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUPresent]++;
		}
		else
		{
			if (GPrintVulkanVsyncDebug)
			{
				UE_LOG(LogVulkanRHI, Log, TEXT("CPU RT delta: %f"), DeltaCPUPresentTimeMS);
			}
		}
		RTPacingPreviousFrameCPUTime = NowCPUTime;
	}
}

FVulkanSwapChain::EStatus FVulkanSwapChain::Present(FVulkanQueue* GfxQueue, FVulkanQueue* PresentQueue, VulkanRHI::FSemaphore* BackBufferRenderingDoneSemaphore)
{
	check(FVulkanPlatform::SupportsStandardSwapchain());

	if (CurrentImageIndex == -1)
	{
		// Skip present silently if image has not been acquired
		return EStatus::Healthy;
	}

	//ensure(GfxQueue == PresentQueue);

	VkPresentInfoKHR Info;
	ZeroVulkanStruct(Info, VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
	VkSemaphore Semaphore = VK_NULL_HANDLE;
	if (BackBufferRenderingDoneSemaphore)
	{
		Info.waitSemaphoreCount = 1;
		Semaphore = BackBufferRenderingDoneSemaphore->GetHandle();
		Info.pWaitSemaphores = &Semaphore;
	}
	Info.swapchainCount = 1;
	Info.pSwapchains = &SwapChain;
	Info.pImageIndices = (uint32*)&CurrentImageIndex;

	const int32 SyncInterval = LockToVsync ? RHIGetSyncInterval() : 0;
	ensureMsgf(SyncInterval <= 3 && SyncInterval >= 0, TEXT("Unsupported sync interval: %i"), SyncInterval);
	FVulkanPlatform::EnablePresentInfoExtensions(Info);

#if VULKAN_SUPPORTS_GOOGLE_DISPLAY_TIMING
	if (GVulkanExtensionFramePacer && Device.GetOptionalExtensions().HasGoogleDisplayTiming)
	{
		check(GDTimingFramePacer);
		GDTimingFramePacer->ScheduleNextFrame(PresentID, SyncInterval);
		Info.pNext = GDTimingFramePacer->GetPresentTimesInfo();
	}
#endif

	//very naive CPU side frame pacer.
	if (GVulkanCPURHIFramePacer && SyncInterval > 0)
	{
		const double NowCPUTime = (FPlatformTime::Seconds() - GStartTime);

		const double TimeToSleep = (NextPresentTargetTime - NowCPUTime);
		const double TargetIntervalWithEpsilon = (double)SyncInterval * (1.0 / 60.0);

		if (TimeToSleep > 0.0)
		{
			uint32 IdleStart = FPlatformTime::Cycles();
			QUICK_SCOPE_CYCLE_COUNTER(STAT_StallForEmulatedSyncInterval);
			FPlatformProcess::SleepNoStats(static_cast<float>(TimeToSleep));
			if (GPrintVulkanVsyncDebug)
			{
				UE_LOG(LogVulkanRHI, Log, TEXT("CurrentID: %i, CPU TimeToSleep: %f, TargetWEps: %f"), TimeToSleep * 1000.0, TargetIntervalWithEpsilon * 1000.0);
			}

			uint32 ThisCycles = FPlatformTime::Cycles() - IdleStart;
			if (IsInRHIThread())
			{
				GWorkingRHIThreadStallTime += ThisCycles;
			}
			else if (IsInActualRenderingThread())
			{
				GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUPresent] += ThisCycles;
				GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUPresent]++;
			}
		}
		else
		{
			if (GPrintVulkanVsyncDebug)
			{
				UE_LOG(LogVulkanRHI, Log, TEXT("CurrentID: %i, CPU TimeToSleep: %f"), PresentID, TimeToSleep * 1000.0);
			}
		}
		NextPresentTargetTime = FMath::Max(NextPresentTargetTime + TargetIntervalWithEpsilon, NowCPUTime);
	}
	PresentID++;

	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanQueuePresent);
		uint32 IdleStart = FPlatformTime::Cycles();
		VkResult PresentResult = VulkanRHI::vkQueuePresentKHR(PresentQueue->GetHandle(), &Info);
		uint32 ThisCycles = FPlatformTime::Cycles() - IdleStart;
		if (IsInRHIThread())
		{
			GWorkingRHIThreadStallTime += ThisCycles;
		}
		else if (IsInActualRenderingThread())
		{
			GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUPresent] += ThisCycles;
			GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUPresent]++;
		}

#if !UE_BUILD_SHIPPING
		PresentResult = SimulateErrors(PresentResult);
#endif

		if (PresentResult == VK_ERROR_OUT_OF_DATE_KHR)
		{
			return EStatus::OutOfDate;
		}

		if (PresentResult == VK_ERROR_SURFACE_LOST_KHR)
		{
			return EStatus::SurfaceLost;
		}

		if (PresentResult != VK_SUCCESS && PresentResult != VK_SUBOPTIMAL_KHR)
		{
			VERIFYVULKANRESULT(PresentResult);
		}
	}

	++NumPresentCalls;

	return EStatus::Healthy;
}


void FVulkanDevice::SetupPresentQueue(VkSurfaceKHR Surface)
{
	if (!PresentQueue)
	{
		const auto SupportsPresent = [Surface](VkPhysicalDevice PhysicalDevice, FVulkanQueue* Queue)
		{
			VkBool32 bSupportsPresent = VK_FALSE;
			const uint32 FamilyIndex = Queue->GetFamilyIndex();
			VERIFYVULKANRESULT(VulkanRHI::vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, FamilyIndex, Surface, &bSupportsPresent));
			if (bSupportsPresent)
			{
				UE_LOG(LogVulkanRHI, Display, TEXT("Queue Family %d: Supports Present"), FamilyIndex);
			}
			return (bSupportsPresent == VK_TRUE);
		};

		bool bGfx = SupportsPresent(Gpu, GfxQueue);
		checkf(bGfx, TEXT("Graphics Queue doesn't support present!"));
		bool bCompute = SupportsPresent(Gpu, ComputeQueue);
		if (TransferQueue->GetFamilyIndex() != GfxQueue->GetFamilyIndex() && TransferQueue->GetFamilyIndex() != ComputeQueue->GetFamilyIndex())
		{
			SupportsPresent(Gpu, TransferQueue);
		}
		if (GAllowPresentOnComputeQueue.GetValueOnAnyThread() != 0 && ComputeQueue->GetFamilyIndex() != GfxQueue->GetFamilyIndex() && bCompute)
		{
			//#todo-rco: Do other IHVs have a fast path here?
			bPresentOnComputeQueue = IsRHIDeviceAMD();
			PresentQueue = ComputeQueue;
		}
		else
		{
			PresentQueue = GfxQueue;
		}
	}
}
