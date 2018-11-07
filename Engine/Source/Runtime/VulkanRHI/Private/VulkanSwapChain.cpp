// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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

#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
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

extern FAutoConsoleVariable GCVarDelayAcquireBackBuffer;
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

#if VULKAN_SUPPORTS_GOOGLE_DISPLAY_TIMING
	FMemory::Memzero(HistoricalPresentData);
#endif

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
			VkFormat PlatformFormat = UEToVkFormat(InOutPixelFormat, false);
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

	VkFormat PlatformFormat = UEToVkFormat(InOutPixelFormat, false);

	Device.SetupPresentQueue(Surface);

	// Fetch present mode
	VkPresentModeKHR PresentMode = VK_PRESENT_MODE_FIFO_KHR;
	if (FVulkanPlatform::SupportsQuerySurfaceProperties())
	{
		static bool bFirstTimeLog = true;

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
	SwapChainInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	//if (GCVarDelayAcquireBackBuffer->GetInt() != 0) Android does not use DelayAcquireBackBuffer, still has to have VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
	{
		SwapChainInfo.imageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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
	VkRefreshCycleDurationGOOGLE RefreshCycleData;
	if (Device.GetOptionalExtensions().HasGoogleDisplayTiming)
	{
		VkResult Result = VulkanDynamicAPI::vkGetRefreshCycleDurationGOOGLE(Device.GetInstanceHandle(), SwapChain, &RefreshCycleData);
		checkf(Result == VK_SUCCESS, TEXT("%i"), Result);
		RefreshRateNanoSec = RefreshCycleData.refreshDuration;

		//little hacky but disable this one by default if extension is available and enabled.  eventually may want to do this per device
		if (GVulkanExtensionFramePacer)
		{
			GVulkanCPURenderThreadFramePacer = 0;
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

	if (Result == VK_ERROR_VALIDATION_FAILED_EXT)
	{
		extern TAutoConsoleVariable<int32> GValidationCvar;
		if (GValidationCvar.GetValueOnRenderThread() == 0)
		{
			UE_LOG(LogVulkanRHI, Fatal, TEXT("vkAcquireNextImageKHR failed with Validation error. Try running with r.Vulkan.EnableValidation=1 to get information from the driver"));
		}
	}
	else
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

void FVulkanSwapChain::RenderThreadPacing()
{
	check(IsInRenderingThread());
	const int32 SyncInterval = LockToVsync ? RHIGetSyncInterval() : 0;

	//very naive CPU side frame pacer.
	if (GVulkanCPURenderThreadFramePacer && SyncInterval > 0)
	{
		static double PreviousFrameCPUTime = 0;
		static double SampledDeltaTimeMS = 0;
		static uint32 SampleCount = 0;

		double NowCPUTime = FPlatformTime::Seconds();
		double DeltaCPUPresentTimeMS = (NowCPUTime - PreviousFrameCPUTime) * 1000.0;


		double TargetIntervalWithEpsilonMS = (double)SyncInterval * (1.0 / 60.0) * 1000.0;
		const double IntervalThresholdMS = TargetIntervalWithEpsilonMS * 0.1;

		SampledDeltaTimeMS += DeltaCPUPresentTimeMS; SampleCount++;

		double SampledDeltaMS = (SampledDeltaTimeMS / (double)SampleCount) + IntervalThresholdMS;

		if (SampleCount > 1000)
		{
			SampledDeltaTimeMS = SampledDeltaMS;
			SampleCount = 1;
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
		PreviousFrameCPUTime = NowCPUTime;
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
	// These are referenced by VkPresentInfoKHR, so they need to be at the same level in the stack
	VkPresentTimeGOOGLE PresentControl;
	VkPresentTimesInfoGOOGLE PresentControlInfo;
	if (Device.GetOptionalExtensions().HasGoogleDisplayTiming)
	{
		if (GVulkanExtensionFramePacer)
		{
			if (SyncInterval != PreviousSyncInterval)
			{
				PreviousEmittedPresentTime = 0;
				FMemory::Memzero(HistoricalPresentData);
				PreviousSyncInterval = SyncInterval;
			}

			const uint64 TargetFlipRate = FMath::Clamp(SyncInterval, 0, 3);
			int64 SyncIntervalNanos = (30 + 1000000000l * int64(SyncInterval)) / 60;
			int32 UnderDriverInterval = int32(SyncIntervalNanos / RefreshRateNanoSec);
			int32 OverDriverInterval = UnderDriverInterval + 1;
			int64 UnderNanos = int64(UnderDriverInterval) * RefreshRateNanoSec;
			int64 OverNanos = int64(OverDriverInterval) * RefreshRateNanoSec;
			const uint64 TargetInterval = (FMath::Abs(SyncIntervalNanos - UnderNanos) < FMath::Abs(SyncIntervalNanos - OverNanos)) ? UnderNanos : OverNanos;
			//UE_LOG(LogVulkanRHI, Log, TEXT("syncinterval: %i, targetflipL %i, synintervalns: %llu, UnderDriverInterval: %i, OverInterval: %i, UnderNanos: %llu, Overnanos: %llu, TargetInterval: %llu"), SyncInterval, TargetFlipRate, SyncIntervalNanos, UnderDriverInterval, OverDriverInterval, UnderNanos, OverNanos, TargetInterval);
			//const uint64 TargetInterval = TargetFlipRate * RefreshRateNanoSec;

			VkPastPresentationTimingGOOGLE PastFrameTimingData[MaxHistoricalPresentData];

			//MUST call once with nullptr to get the count, or the API won't return any results at all.
			uint32 TotalRead = 0;
			VkResult Result = VulkanDynamicAPI::vkGetPastPresentationTimingGOOGLE(Device.GetInstanceHandle(), SwapChain, &TotalRead, nullptr);
			checkf(Result == VK_SUCCESS, TEXT("vkGetPastPresentationTimingGOOGLE failed: %i"), Result);
			TotalRead = FMath::Min(TotalRead, (uint32)MaxHistoricalPresentData);

			if (TotalRead > 0)
			{
				Result = VK_INCOMPLETE;
				while (Result == VK_INCOMPLETE)
				{
					//this tells vkGetPastPresentationTimingGOOGLE the maximum to fill, and it gets modified with the actual count written
					Result = VulkanDynamicAPI::vkGetPastPresentationTimingGOOGLE(Device.GetInstanceHandle(), SwapChain, &TotalRead, PastFrameTimingData);
					for (int32 i = 0; i < TotalRead; ++i)
					{
						FPresentData& PresentData = HistoricalPresentData[NextHistoricalData];
						PresentData.PresentID = PastFrameTimingData[i].presentID;
						PresentData.ActualPresentTime = PastFrameTimingData[i].actualPresentTime;
						PresentData.DesiredPresentTime = PastFrameTimingData[i].desiredPresentTime;
						NextHistoricalData = (NextHistoricalData + 1) % MaxHistoricalPresentData;
					}
				}
			}
			checkf(Result == VK_SUCCESS, TEXT("vkGetPastPresentationTimingGOOGLE failed: %i"), Result);

			const int32 LastKnownFrameIndex = (NextHistoricalData - 1) < 0 ? (MaxHistoricalPresentData - 1) : (NextHistoricalData - 1);
			if (TotalRead > 0 && GPrintVulkanVsyncDebug)
			{
				const int32 PrevLastKnownFrameIndex = (NextHistoricalData - 2) < 0 ? (MaxHistoricalPresentData - FMath::Abs(NextHistoricalData - 2)) : (NextHistoricalData - 2);
				uint64 PrevTime = HistoricalPresentData[PrevLastKnownFrameIndex].ActualPresentTime;

				const VkPastPresentationTimingGOOGLE& MostRecentKnownPresentedFrame = PastFrameTimingData[TotalRead - 1];
				uint64 LastKnownPresentedTime = MostRecentKnownPresentedFrame.actualPresentTime;
				uint64 LastKnownPresentedID = MostRecentKnownPresentedFrame.presentID;

				uint64 PresentDiff = LastKnownPresentedTime - PrevTime;

				uint64 ActualDesiredDiffNS = MostRecentKnownPresentedFrame.actualPresentTime - MostRecentKnownPresentedFrame.desiredPresentTime;
				float ActualDesiredDiffMS = (double)ActualDesiredDiffNS / 1000000.0;
				float ActualPresentDiffMS = (double)PresentDiff / 1000000.0;

				UE_LOG(LogVulkanRHI, Log, TEXT("lastKnownPresent ID: %llu, previousKnownPresent ID: %llu, actual: %llu, desired: %llu, earliest: %llu, margin: %llu, currentTargetInterval: %llu, actualDesiredDiff: %f, diffsincePrev: %f "), LastKnownPresentedID, HistoricalPresentData[PrevLastKnownFrameIndex].PresentID, LastKnownPresentedTime, MostRecentKnownPresentedFrame.desiredPresentTime, MostRecentKnownPresentedFrame.earliestPresentTime, MostRecentKnownPresentedFrame.presentMargin, TargetInterval, ActualDesiredDiffMS, ActualPresentDiffMS);
			}

			PresentControl.presentID = PresentID;
			PresentControl.desiredPresentTime = 0;

			const FPresentData& LastKnownFrameData = HistoricalPresentData[LastKnownFrameIndex];
			const uint64 LastKnownPresentedID = LastKnownFrameData.PresentID;

			if (LastKnownPresentedID > 0 && LastKnownFrameData.ActualPresentTime > 0)
			{
				const uint32 FrameDifferential = PresentID - LastKnownPresentedID;
				uint64 Epsilon = 4000000; //1ms

				//present time for this frame assuming that all known and unknown historical frames completed on time.  Note that we have a non-zero value for 'FrameDifferential' because
				//we only have present data for frames older than 4-5 vsyncs.  Thus our immediate preceding frames might go overbudget and we don't know that yet.
				const uint64 IdealPresentTime = LastKnownFrameData.ActualPresentTime + ((uint64)FrameDifferential * TargetInterval) - Epsilon;
				uint64 FinalPresentTime = IdealPresentTime;

				//We need to account for the fact that every frame we get new data about old historical frames.  Those frames calculated ideal present times with incomplete data.
				//As we get data about slow frames, we must account for it now so that we don't end up with huge 'catchup' stalls.
				//Consider 3 frames, and a single frame delay in getting present data.  Also consider a simplified model where the refresh rate is 5, and the interval is 2.
				// Frame N:
				//	Knows that Frame N - 2 completed at time 50.  Does not know when N - 1 completes.  Assumes it wll complete on time and thus tells the driver
				//	to present NO EARLIER than time 70.  it is allowed to complete at any time thereafter.
				//
				// Frame N+1:
				//	This frame now knows that N-1 ACTUALLY completed at time 100.  Since frame N completes 'no earlier than time 70' this means it will present on frame 105 if it is an in-budget frame.
				//	Note that this means the delta between N-1 and N is FASTER than our desired syncinterval of 2.
				//	Frame N+1 could now naively calculate a present time of 120 (2 frames later, sync interval 2).  However, this is '3' intervals after Frame N can be expected to complete if it is in-budget.
				//	The problem of extra intervals gets progressively worse with larger refresh rates, and larger amounts of unknown frame latency.
				//	In a realistic setting of 4-5 latency, with 30hhz framerate on a 16hz refreshrate you can easily get 30-100ms of 'stall' frames where the naively calculated present time is long after the previously presented frame.
				//	(remember that Frame N completes early, and N+1 Late).
				//
				// We cannot fix frames already in flight with our updated data, so we must account for them assuming they will be in-budget.  For the example, we want N+1 to complete at time 115.  A correct 2 intervals rather than 3.

				//start at the end of the history ring
				int32 PrevHistoricalIndex = (NextHistoricalData + 0) % MaxHistoricalPresentData;
				int32 HistoricalIndex = (NextHistoricalData + 1) % MaxHistoricalPresentData;
				int32 ExtraHistoricalVsyncs = 0;

				//give it 1ms of fudge.  Often even frames that hit vsync don't report EXACT interval times.
				const uint32 ComparisonInterval = TargetInterval + 1000000;
				for (int32 i = 0; i < MaxHistoricalPresentData - 1; ++i)
				{
					const FPresentData& PrevFrameData = HistoricalPresentData[PrevHistoricalIndex];
					const FPresentData& FrameData = HistoricalPresentData[HistoricalIndex];
					if (FrameData.ActualPresentTime > 0 && PrevFrameData.ActualPresentTime > 0)
					{
						const uint64 ActualFrameDelta = FrameData.ActualPresentTime - PrevFrameData.ActualPresentTime;
						const uint64 ActualFrameLateness = FrameData.ActualPresentTime - FrameData.DesiredPresentTime;

						//if we detect a historical frame that was presented too long after the previous frame,
						//OR we detect a historical frame that was presented too long after it's desired frame
						//we need to make adjustments for 'fat' frames.
						if (ActualFrameDelta > ComparisonInterval/* || ActualFrameLateness > RefreshRateNanoSec*/)
						{
							//basically, a fat frame will cover the desired present time of some number of frames who did not know about its fatness.  These will complete immediately on the next vblank if they are in budget
							//so we need to rollback our naive calculation above that every frame would be on the screen for the desired interval.  Some number will be on screen for much less time depending on how fat is the frame, and how latent is the data.
							const int32 ThisLocalIntervals = (((ActualFrameDelta - TargetInterval) + 1000000) / RefreshRateNanoSec);// +(ActualFrameLateness / RefreshRateNanoSec);
							ExtraHistoricalVsyncs += ThisLocalIntervals;

							if (GPrintVulkanVsyncDebug)
							{
								UE_LOG(LogVulkanRHI, Log, TEXT("Delta: %llu, Interval: %llu, PresID: %i, PrevId: %i, HisInd: %i, PrevHisInd: %i, NextHisData: %i, Vsyncs: %i"), ActualFrameDelta, ComparisonInterval, FrameData.PresentID, PrevFrameData.PresentID, HistoricalIndex, PrevHistoricalIndex, NextHistoricalData, ThisLocalIntervals);
							}
						}
					}
					PrevHistoricalIndex = (PrevHistoricalIndex + 1) % MaxHistoricalPresentData;
					HistoricalIndex = (HistoricalIndex + 1) % MaxHistoricalPresentData;
				}

				//Next present should be at least one interval more than the previous known.
				ExtraHistoricalVsyncs = FMath::Clamp(FMath::Min((int32)FrameDifferential - 1, ExtraHistoricalVsyncs), 0, (int32)FrameDifferential - 1);
				FinalPresentTime -= ExtraHistoricalVsyncs * RefreshRateNanoSec;

				//regardless of any other calculations we should emit at MAXIMUM a time only 1 interval from the one we calculated for the previous frame.
				//this helps account for differences when we lose data about a fat frame in the rolling history.  Also should be no earlier than our last known frame, helps correct issues
				//with hitches and syncinterval changes.
				if (PreviousEmittedPresentTime > LastKnownFrameData.ActualPresentTime)
				{
					FinalPresentTime = FMath::Clamp(FinalPresentTime, LastKnownFrameData.ActualPresentTime, PreviousEmittedPresentTime + TargetInterval);
				}

				//setting desiredPresentTime indicates this frame must be shown NO SOONER than the given time.
				//PresentIDs are monotonically increasing.  Thus if we know:
				// * WHEN a past frame was actually presented (LastKnownPresentedTime)
				// * How many frames in the future we are now (FrameDifferential)
				// * What our target sync interval is (RHIGetSyncInterval)
				// We can figure out when we want to present without actually calling into the clock interface
				PresentControl.desiredPresentTime = FinalPresentTime;

				if (GPrintVulkanVsyncDebug)
				{
					uint64 DeltaNS = IdealPresentTime - FinalPresentTime;
					float DeltaMS = (double)DeltaNS / 1000000.0;
					UE_LOG(LogVulkanRHI, Log, TEXT("CurrentID: %i, Naive: %llu, Fixed: %llu, Delta: %f"), PresentID, IdealPresentTime, FinalPresentTime, DeltaMS);
				}

				PreviousEmittedPresentTime = FinalPresentTime;
			}

			ZeroVulkanStruct(PresentControlInfo, VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE);
			PresentControlInfo.swapchainCount = 1;
			PresentControlInfo.pTimes = &PresentControl;

			Info.pNext = &PresentControlInfo;
		}
		else
		{
			PreviousEmittedPresentTime = 0;
			FMemory::Memzero(HistoricalPresentData);
		}
		//UE_LOG(LogVulkanRHI, Display, TEXT("Preset ID: %i, Time: 0x%x, Chain: %i"), PresentControl.presentID, PresentControl.desiredPresentTime, PresentControlInfo.swapchainCount);
	}
#endif

	//very naive CPU side frame pacer.
	if (GVulkanCPURHIFramePacer && SyncInterval > 0)
	{
		static double PreviousFrameCPUTime = 0;
		double NowCPUTime = FPlatformTime::Seconds();

		double DeltaCPUPresentTimeMS = (NowCPUTime - PreviousFrameCPUTime) * 1000.0;
		double TargetIntervalWithEpsilonMS = (double)SyncInterval * (1.0 / 60.0) * 1000.0;

		if (DeltaCPUPresentTimeMS < (TargetIntervalWithEpsilonMS))
		{
			uint32 IdleStart = FPlatformTime::Cycles();
			QUICK_SCOPE_CYCLE_COUNTER(STAT_StallForEmulatedSyncInterval);
			FPlatformProcess::SleepNoStats((TargetIntervalWithEpsilonMS - DeltaCPUPresentTimeMS) * 0.001f);
			if (GPrintVulkanVsyncDebug)
			{
				UE_LOG(LogVulkanRHI, Log, TEXT("CurrentID: %i, CPU delta: %f, TargetWEps: %f, sleepTime: %f "), PresentID, DeltaCPUPresentTimeMS, TargetIntervalWithEpsilonMS, TargetIntervalWithEpsilonMS - DeltaCPUPresentTimeMS);
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
				UE_LOG(LogVulkanRHI, Log, TEXT("CurrentID: %i, CPU delta: %f"), PresentID, DeltaCPUPresentTimeMS);
			}
		}
		PreviousFrameCPUTime = NowCPUTime;
	}
	PresentID++;

	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanQueuePresent);
		VkResult PresentResult = VulkanRHI::vkQueuePresentKHR(PresentQueue->GetHandle(), &Info);

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
