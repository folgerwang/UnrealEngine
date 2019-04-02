// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanDevice.cpp: Vulkan device RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanDevice.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "VulkanPlatform.h"
#include "VulkanLLM.h"

TAutoConsoleVariable<int32> GRHIAllowAsyncComputeCvar(
	TEXT("r.Vulkan.AllowAsyncCompute"),
	0,
	TEXT("0 to disable async compute queue(if available)")
	TEXT("1 to allow async compute queue")
);

TAutoConsoleVariable<int32> GAllowPresentOnComputeQueue(
	TEXT("r.Vulkan.AllowPresentOnComputeQueue"),
	0,
	TEXT("0 to present on the graphics queue")
	TEXT("1 to allow presenting on the compute queue if available")
);

static TAutoConsoleVariable<int32> GCVarRobustBufferAccess(
	TEXT("r.Vulkan.RobustBufferAccess"),
	1,
	TEXT("0 to disable robust buffer access")
	TEXT("1 to enable (default)"),
	ECVF_ReadOnly
);

// Mirror GPixelFormats with format information for buffers
VkFormat GVulkanBufferFormat[PF_MAX];

EDelayAcquireImageType GVulkanDelayAcquireImage = EDelayAcquireImageType::DelayAcquire;

TAutoConsoleVariable<int32> CVarDelayAcquireBackBuffer(
	TEXT("r.Vulkan.DelayAcquireBackBuffer"),
	1,
	TEXT("Whether to delay acquiring the back buffer \n")
	TEXT(" 0: acquire next image on frame start \n")
	TEXT(" 1: acquire next image just before presenting, rendering is done to intermediate image which is then copied to real backbuffer (default) \n")
	TEXT(" 2: acquire next image immediately after presenting current"),
	ECVF_ReadOnly
);

static EDelayAcquireImageType DelayAcquireBackBuffer()
{
	int32 DelayType = CVarDelayAcquireBackBuffer.GetValueOnAnyThread();
	switch (DelayType)
	{
	case 1:
		return EDelayAcquireImageType::DelayAcquire;
	case 2:
		return EDelayAcquireImageType::PreAcquire;
	}
	return EDelayAcquireImageType::None;
}

static void EnableDrawMarkers()
{
	static IConsoleVariable* ShowMaterialDrawEventVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ShowMaterialDrawEvents"));

	const bool bDrawEvents = GetEmitDrawEvents() != 0;
	const bool bMaterialDrawEvents = ShowMaterialDrawEventVar ? ShowMaterialDrawEventVar->GetInt() != 0 : false;

	UE_LOG(LogRHI, Display, TEXT("Setting GPU Capture Options: 1"));
	if (!bDrawEvents)
	{
		UE_LOG(LogRHI, Display, TEXT("Toggling draw events: 1"));
		SetEmitDrawEvents(true);
	}
	if (!bMaterialDrawEvents && ShowMaterialDrawEventVar)
	{
		UE_LOG(LogRHI, Display, TEXT("Toggling showmaterialdrawevents: 1"));
		ShowMaterialDrawEventVar->Set(-1);
	}
}

#if VULKAN_SUPPORTS_VALIDATION_CACHE
static void LoadValidationCache(VkDevice Device, VkValidationCacheEXT& OutValidationCache)
{
	VkValidationCacheCreateInfoEXT ValidationCreateInfo;
	ZeroVulkanStruct(ValidationCreateInfo, VK_STRUCTURE_TYPE_VALIDATION_CACHE_CREATE_INFO_EXT);
	TArray<uint8> InData;

	const FString& CacheFilename = VulkanRHI::GetValidationCacheFilename();
	UE_LOG(LogVulkanRHI, Display, TEXT("Trying validation cache file %s"), *CacheFilename);
	if (FFileHelper::LoadFileToArray(InData, *CacheFilename, FILEREAD_Silent) && InData.Num() > 0)
	{
		// The code below supports SDK 1.0.65 Vulkan spec, which contains the following table:
		//
		// Offset	 Size            Meaning
		// ------    ------------    ------------------------------------------------------------------
		//      0               4    length in bytes of the entire validation cache header written as a
		//                           stream of bytes, with the least significant byte first
		//      4               4    a VkValidationCacheHeaderVersionEXT value written as a stream of
		//                           bytes, with the least significant byte first
		//      8    VK_UUID_SIZE    a layer commit ID expressed as a UUID, which uniquely identifies
		//                           the version of the validation layers used to generate these
		//                           validation results
		int32* DataPtr = (int32*)InData.GetData();
		if (*DataPtr > 0)
		{
			++DataPtr;
			int32 Version = *DataPtr++;
			if (Version == VK_PIPELINE_CACHE_HEADER_VERSION_ONE)
			{
				DataPtr += VK_UUID_SIZE / sizeof(int32);
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Bad validation cache file %s, version=%d, expected %d"), *CacheFilename, Version, VK_PIPELINE_CACHE_HEADER_VERSION_ONE);
				InData.Reset(0);
			}
		}
		else
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Bad validation cache file %s, header size=%d"), *CacheFilename, *DataPtr);
			InData.Reset(0);
		}
	}

	ValidationCreateInfo.initialDataSize = InData.Num();
	ValidationCreateInfo.pInitialData = InData.Num() > 0 ? InData.GetData() : nullptr;
	//ValidationCreateInfo.flags = 0;
	PFN_vkCreateValidationCacheEXT vkCreateValidationCache = (PFN_vkCreateValidationCacheEXT)(void*)VulkanRHI::vkGetDeviceProcAddr(Device, "vkCreateValidationCacheEXT");
	if (vkCreateValidationCache)
	{
		VkResult Result = vkCreateValidationCache(Device, &ValidationCreateInfo, VULKAN_CPU_ALLOCATOR, &OutValidationCache);
		if (Result != VK_SUCCESS)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Failed to create Vulkan validation cache, VkResult=%d"), Result);
		}
	}
}
#endif


FVulkanDevice::FVulkanDevice(VkPhysicalDevice InGpu)
	: Device(VK_NULL_HANDLE)
	, ResourceHeapManager(this)
	, DeferredDeletionQueue(this)
	, DefaultSampler(nullptr)
	, DefaultImage(nullptr)
	, Gpu(InGpu)
	, GfxQueue(nullptr)
	, ComputeQueue(nullptr)
	, TransferQueue(nullptr)
	, PresentQueue(nullptr)
	, ImmediateContext(nullptr)
	, ComputeContext(nullptr)
	, PipelineStateCache(nullptr)
{
	FMemory::Memzero(GpuProps);
#if VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
	FMemory::Memzero(GpuIdProps);
#endif
	FMemory::Memzero(PhysicalFeatures);
	FMemory::Memzero(FormatProperties);
	FMemory::Memzero(PixelFormatComponentMapping);
}

FVulkanDevice::~FVulkanDevice()
{
	if (Device != VK_NULL_HANDLE)
	{
		Destroy();
		Device = VK_NULL_HANDLE;
	}
}

void FVulkanDevice::CreateDevice()
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanMisc);
	check(Device == VK_NULL_HANDLE);

	// Setup extension and layer info
	VkDeviceCreateInfo DeviceInfo;
	ZeroVulkanStruct(DeviceInfo, VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);

	bool bDebugMarkersFound = false;
	TArray<const ANSICHAR*> DeviceExtensions;
	TArray<const ANSICHAR*> ValidationLayers;
	GetDeviceExtensionsAndLayers(DeviceExtensions, ValidationLayers, bDebugMarkersFound);

	ParseOptionalDeviceExtensions(DeviceExtensions);

	DeviceInfo.enabledExtensionCount = DeviceExtensions.Num();
	DeviceInfo.ppEnabledExtensionNames = DeviceExtensions.GetData();

	DeviceInfo.enabledLayerCount = ValidationLayers.Num();
	DeviceInfo.ppEnabledLayerNames = (DeviceInfo.enabledLayerCount > 0) ? ValidationLayers.GetData() : nullptr;

	// Setup Queue info
	TArray<VkDeviceQueueCreateInfo> QueueFamilyInfos;
	int32 GfxQueueFamilyIndex = -1;
	int32 ComputeQueueFamilyIndex = -1;
	int32 TransferQueueFamilyIndex = -1;
	UE_LOG(LogVulkanRHI, Display, TEXT("Found %d Queue Families"), QueueFamilyProps.Num());
	uint32 NumPriorities = 0;
	for (int32 FamilyIndex = 0; FamilyIndex < QueueFamilyProps.Num(); ++FamilyIndex)
	{
		const VkQueueFamilyProperties& CurrProps = QueueFamilyProps[FamilyIndex];

		bool bIsValidQueue = false;
		if ((CurrProps.queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT)
		{
			if (GfxQueueFamilyIndex == -1)
			{
				GfxQueueFamilyIndex = FamilyIndex;
				bIsValidQueue = true;
			}
			else
			{
				//#todo-rco: Support for multi-queue/choose the best queue!
			}
		}

		if ((CurrProps.queueFlags & VK_QUEUE_COMPUTE_BIT) == VK_QUEUE_COMPUTE_BIT)
		{
			if (ComputeQueueFamilyIndex == -1 &&
				(GRHIAllowAsyncComputeCvar.GetValueOnAnyThread() != 0 || GAllowPresentOnComputeQueue.GetValueOnAnyThread() != 0) && GfxQueueFamilyIndex != FamilyIndex)
			{
				ComputeQueueFamilyIndex = FamilyIndex;
				bIsValidQueue = true;
			}
		}

		if ((CurrProps.queueFlags & VK_QUEUE_TRANSFER_BIT) == VK_QUEUE_TRANSFER_BIT)
		{
			// Prefer a non-gfx transfer queue
			if (TransferQueueFamilyIndex == -1 && (CurrProps.queueFlags & VK_QUEUE_GRAPHICS_BIT) != VK_QUEUE_GRAPHICS_BIT && (CurrProps.queueFlags & VK_QUEUE_COMPUTE_BIT) != VK_QUEUE_COMPUTE_BIT)
			{
				TransferQueueFamilyIndex = FamilyIndex;
				bIsValidQueue = true;
			}
		}

		auto GetQueueInfoString = [](const VkQueueFamilyProperties& Props)
		{
			FString Info;
			if ((Props.queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT)
			{
				Info += TEXT(" Gfx");
			}
			if ((Props.queueFlags & VK_QUEUE_COMPUTE_BIT) == VK_QUEUE_COMPUTE_BIT)
			{
				Info += TEXT(" Compute");
			}
			if ((Props.queueFlags & VK_QUEUE_TRANSFER_BIT) == VK_QUEUE_TRANSFER_BIT)
			{
				Info += TEXT(" Xfer");
			}
			if ((Props.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) == VK_QUEUE_SPARSE_BINDING_BIT)
			{
				Info += TEXT(" Sparse");
			}

			return Info;
		};

		if (!bIsValidQueue)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("Skipping unnecessary Queue Family %d: %d queues%s"), FamilyIndex, CurrProps.queueCount, *GetQueueInfoString(CurrProps));
			continue;
		}

		int32 QueueIndex = QueueFamilyInfos.Num();
		QueueFamilyInfos.AddZeroed(1);
		VkDeviceQueueCreateInfo& CurrQueue = QueueFamilyInfos[QueueIndex];
		CurrQueue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		CurrQueue.queueFamilyIndex = FamilyIndex;
		CurrQueue.queueCount = CurrProps.queueCount;
		NumPriorities += CurrProps.queueCount;
		UE_LOG(LogVulkanRHI, Display, TEXT("Initializing Queue Family %d: %d queues%s"), FamilyIndex, CurrProps.queueCount, *GetQueueInfoString(CurrProps));
	}

	TArray<float> QueuePriorities;
	QueuePriorities.AddUninitialized(NumPriorities);
	float* CurrentPriority = QueuePriorities.GetData();
	for (int32 Index = 0; Index < QueueFamilyInfos.Num(); ++Index)
	{
		VkDeviceQueueCreateInfo& CurrQueue = QueueFamilyInfos[Index];
		CurrQueue.pQueuePriorities = CurrentPriority;

		const VkQueueFamilyProperties& CurrProps = QueueFamilyProps[CurrQueue.queueFamilyIndex];
		for (int32 QueueIndex = 0; QueueIndex < (int32)CurrProps.queueCount; ++QueueIndex)
		{
			*CurrentPriority++ = 1.0f;
		}
	}

	DeviceInfo.queueCreateInfoCount = QueueFamilyInfos.Num();
	DeviceInfo.pQueueCreateInfos = QueueFamilyInfos.GetData();

	PhysicalFeatures.robustBufferAccess = GCVarRobustBufferAccess.GetValueOnAnyThread() > 0 ? VK_TRUE : VK_FALSE;
	FVulkanPlatform::RestrictEnabledPhysicalDeviceFeatures(PhysicalFeatures);
	DeviceInfo.pEnabledFeatures = &PhysicalFeatures;

	FVulkanPlatform::EnablePhysicalDeviceFeatureExtensions(DeviceInfo);

	// Create the device
	VkResult Result = VulkanRHI::vkCreateDevice(Gpu, &DeviceInfo, VULKAN_CPU_ALLOCATOR, &Device);
	if (Result == VK_ERROR_INITIALIZATION_FAILED)
	{
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, TEXT("Cannot create a Vulkan device. Try updating your video driver to a more recent version.\n"), TEXT("Vulkan device creation failed"));
		FPlatformMisc::RequestExitWithStatus(true, 1);
	}
	VERIFYVULKANRESULT_EXPANDED(Result);

	// Create Graphics Queue, here we submit command buffers for execution
	GfxQueue = new FVulkanQueue(this, GfxQueueFamilyIndex);
	if (ComputeQueueFamilyIndex == -1)
	{
		// If we didn't find a dedicated Queue, use the default one
		ComputeQueueFamilyIndex = GfxQueueFamilyIndex;
	}
	else
	{
		// Dedicated queue
		if (GRHIAllowAsyncComputeCvar.GetValueOnAnyThread() != 0)
		{
			bAsyncComputeQueue = true;
		}
	}
	ComputeQueue = new FVulkanQueue(this, ComputeQueueFamilyIndex);
	if (TransferQueueFamilyIndex == -1)
	{
		// If we didn't find a dedicated Queue, use the default one
		TransferQueueFamilyIndex = ComputeQueueFamilyIndex;
	}
	TransferQueue = new FVulkanQueue(this, TransferQueueFamilyIndex);

	uint64 NumBits = QueueFamilyProps[GfxQueueFamilyIndex].timestampValidBits;
	if (NumBits > 0)
	{
		ensure(NumBits == QueueFamilyProps[ComputeQueueFamilyIndex].timestampValidBits);
		if (NumBits == 64)
		{
			// Undefined behavior trying to 1 << 64 on uint64
			TimestampValidBitsMask = UINT64_MAX;
		}
		else
		{
			TimestampValidBitsMask = ((uint64)1 << (uint64)NumBits) - (uint64)1;
		}
	}

#if VULKAN_ENABLE_DRAW_MARKERS
#if 0//VULKAN_SUPPORTS_DEBUG_UTILS
	FVulkanDynamicRHI* RHI = (FVulkanDynamicRHI*)GDynamicRHI;
	if (RHI->SupportsDebugUtilsExt() && GRenderDocFound)
	{
		DebugMarkers.CmdBeginDebugLabel = (PFN_vkCmdBeginDebugUtilsLabelEXT)(void*)VulkanRHI::vkGetInstanceProcAddr(RHI->GetInstance(), "vkCmdBeginDebugUtilsLabelEXT");
		DebugMarkers.CmdEndDebugLabel = (PFN_vkCmdEndDebugUtilsLabelEXT)(void*)VulkanRHI::vkGetInstanceProcAddr(RHI->GetInstance(), "vkCmdEndDebugUtilsLabelEXT");
		DebugMarkers.SetDebugName = (PFN_vkSetDebugUtilsObjectNameEXT)(void*)VulkanRHI::vkGetInstanceProcAddr(RHI->GetInstance(), "vkSetDebugUtilsObjectNameEXT");
		if (DebugMarkers.CmdBeginDebugLabel && DebugMarkers.CmdEndDebugLabel && DebugMarkers.SetDebugName)
		{
			bDebugMarkersFound = true;
		}
	}
	else
#endif	// VULKAN_SUPPORTS_DEBUG_UTILS
	if (bDebugMarkersFound || FVulkanPlatform::ForceEnableDebugMarkers())
	{
		DebugMarkers.CmdBegin = (PFN_vkCmdDebugMarkerBeginEXT)(void*)VulkanRHI::vkGetDeviceProcAddr(Device, "vkCmdDebugMarkerBeginEXT");
		DebugMarkers.CmdEnd = (PFN_vkCmdDebugMarkerEndEXT)(void*)VulkanRHI::vkGetDeviceProcAddr(Device, "vkCmdDebugMarkerEndEXT");
		DebugMarkers.CmdSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)(void*)VulkanRHI::vkGetDeviceProcAddr(Device, "vkDebugMarkerSetObjectNameEXT");

		if (DebugMarkers.CmdBegin && DebugMarkers.CmdEnd && DebugMarkers.CmdSetObjectName)
		{
			bDebugMarkersFound = true;
		}

		if (!DebugMarkers.CmdBegin || !DebugMarkers.CmdEnd || !DebugMarkers.CmdSetObjectName)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Extension found, but entry points for vkCmdDebugMarker(Begin|End)EXT NOT found!"));
			bDebugMarkersFound = false;
			DebugMarkers.CmdBegin = nullptr;
			DebugMarkers.CmdEnd = nullptr;
			DebugMarkers.CmdSetObjectName = nullptr;
		}
	}
	else
	{
		if (DebugMarkers.CmdBegin && DebugMarkers.CmdEnd && DebugMarkers.CmdSetObjectName)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Extension not found, but entry points for vkCmdDebugMarker(Begin|End)EXT found!"));
			bDebugMarkersFound = true;
		}
	}

	if (bDebugMarkersFound)
	{
		// We're running under RenderDoc or other trace tool, so enable capturing mode
		EnableDrawMarkers();
	}
#endif

#if VULKAN_ENABLE_DUMP_LAYER
	EnableDrawMarkers();
#endif
	
	GVulkanDelayAcquireImage = DelayAcquireBackBuffer();
}

void FVulkanDevice::SetupFormats()
{
	for (uint32 Index = 0; Index < VK_FORMAT_RANGE_SIZE; ++Index)
	{
		const VkFormat Format = (VkFormat)Index;
		FMemory::Memzero(FormatProperties[Index]);
		VulkanRHI::vkGetPhysicalDeviceFormatProperties(Gpu, Format, &FormatProperties[Index]);
	}

	static_assert(sizeof(VkFormat) <= sizeof(GPixelFormats[0].PlatformFormat), "PlatformFormat must be increased!");

	// Initialize the platform pixel format map.
	for (int32 Index = 0; Index < PF_MAX; ++Index)
	{
		GPixelFormats[Index].PlatformFormat = VK_FORMAT_UNDEFINED;
		GPixelFormats[Index].Supported = false;
		GVulkanBufferFormat[Index] = VK_FORMAT_UNDEFINED;
		
		// Set default component mapping
		VkComponentMapping& ComponentMapping = PixelFormatComponentMapping[Index];
		ComponentMapping.r = VK_COMPONENT_SWIZZLE_R;
		ComponentMapping.g = VK_COMPONENT_SWIZZLE_G;
		ComponentMapping.b = VK_COMPONENT_SWIZZLE_B;
		ComponentMapping.a = VK_COMPONENT_SWIZZLE_A;
	}

	// Default formats
	MapFormatSupport(PF_B8G8R8A8, VK_FORMAT_B8G8R8A8_UNORM);
	SetComponentMapping(PF_B8G8R8A8, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

	MapFormatSupport(PF_G8, VK_FORMAT_R8_UNORM);
	SetComponentMapping(PF_G8, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupportWithFallback(PF_G16, VK_FORMAT_R16_UNORM, {VK_FORMAT_R16_SFLOAT});
	SetComponentMapping(PF_G16, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupport(PF_FloatRGB, VK_FORMAT_B10G11R11_UFLOAT_PACK32);
	SetComponentMapping(PF_FloatRGB, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupport(PF_FloatRGBA, VK_FORMAT_R16G16B16A16_SFLOAT, 8);
	SetComponentMapping(PF_FloatRGBA, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

	MapFormatSupportWithFallback(PF_DepthStencil, VK_FORMAT_D32_SFLOAT_S8_UINT, {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT});
	SetComponentMapping(PF_DepthStencil, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY);

	MapFormatSupport(PF_ShadowDepth, VK_FORMAT_D16_UNORM);
	SetComponentMapping(PF_ShadowDepth, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY);

	// Requirement for GPU particles
	MapFormatSupport(PF_G32R32F, VK_FORMAT_R32G32_SFLOAT, 8);
	SetComponentMapping(PF_G32R32F, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupport(PF_A32B32G32R32F, VK_FORMAT_R32G32B32A32_SFLOAT, 16);
	SetComponentMapping(PF_A32B32G32R32F, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

	MapFormatSupportWithFallback(PF_G16R16, VK_FORMAT_R16G16_UNORM, {VK_FORMAT_R16G16_SFLOAT});
	SetComponentMapping(PF_G16R16, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupport(PF_G16R16F, VK_FORMAT_R16G16_SFLOAT);
	SetComponentMapping(PF_G16R16F, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupport(PF_G16R16F_FILTER, VK_FORMAT_R16G16_SFLOAT);
	SetComponentMapping(PF_G16R16F_FILTER, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupport(PF_R16_UINT, VK_FORMAT_R16_UINT);
	SetComponentMapping(PF_R16_UINT, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupport(PF_R16_SINT, VK_FORMAT_R16_SINT);
	SetComponentMapping(PF_R16_SINT, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupport(PF_R32_UINT, VK_FORMAT_R32_UINT);
	SetComponentMapping(PF_R32_UINT, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupport(PF_R32_SINT, VK_FORMAT_R32_SINT);
	SetComponentMapping(PF_R32_SINT, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupport(PF_R8_UINT, VK_FORMAT_R8_UINT);
	SetComponentMapping(PF_R8_UINT, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupportWithFallback(PF_D24, VK_FORMAT_X8_D24_UNORM_PACK32, {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT});
	SetComponentMapping(PF_D24, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupport(PF_R16F, VK_FORMAT_R16_SFLOAT);
	SetComponentMapping(PF_R16F, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupport(PF_R16F_FILTER, VK_FORMAT_R16_SFLOAT);
	SetComponentMapping(PF_R16F_FILTER, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupport(PF_FloatR11G11B10, VK_FORMAT_B10G11R11_UFLOAT_PACK32, 4);
	SetComponentMapping(PF_FloatR11G11B10, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupport(PF_A2B10G10R10, VK_FORMAT_A2B10G10R10_UNORM_PACK32, 4);
	SetComponentMapping(PF_A2B10G10R10, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

	MapFormatSupportWithFallback(PF_A16B16G16R16, VK_FORMAT_R16G16B16A16_UNORM, {VK_FORMAT_R16G16B16A16_SFLOAT});
	GPixelFormats[PF_A16B16G16R16].BlockBytes = 8;
	SetComponentMapping(PF_A16B16G16R16, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

	MapFormatSupport(PF_A8, VK_FORMAT_R8_UNORM);
	SetComponentMapping(PF_A8, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_R);

	MapFormatSupport(PF_R5G6B5_UNORM, VK_FORMAT_R5G6B5_UNORM_PACK16);
	SetComponentMapping(PF_R5G6B5_UNORM, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

	MapFormatSupport(PF_R8G8B8A8, VK_FORMAT_R8G8B8A8_UNORM);
	SetComponentMapping(PF_R8G8B8A8, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

	MapFormatSupport(PF_R8G8B8A8_UINT, VK_FORMAT_R8G8B8A8_UINT);
	SetComponentMapping(PF_R8G8B8A8_UINT, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

	MapFormatSupport(PF_R8G8B8A8_SNORM, VK_FORMAT_R8G8B8A8_SNORM);
	SetComponentMapping(PF_R8G8B8A8_SNORM, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

	MapFormatSupport(PF_R16G16_UINT, VK_FORMAT_R16G16_UINT);
	SetComponentMapping(PF_R16G16_UINT, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupport(PF_R16G16B16A16_UINT, VK_FORMAT_R16G16B16A16_UINT);
	SetComponentMapping(PF_R16G16B16A16_UINT, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

	MapFormatSupport(PF_R16G16B16A16_SINT, VK_FORMAT_R16G16B16A16_SINT);
	SetComponentMapping(PF_R16G16B16A16_SINT, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

	MapFormatSupport(PF_R32G32B32A32_UINT, VK_FORMAT_R32G32B32A32_UINT);
	SetComponentMapping(PF_R32G32B32A32_UINT, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

	MapFormatSupportWithFallback(PF_R16G16B16A16_SNORM, VK_FORMAT_R16G16B16A16_SNORM, {VK_FORMAT_R16G16B16A16_SFLOAT});
	SetComponentMapping(PF_R16G16B16A16_SNORM, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

	MapFormatSupportWithFallback(PF_R16G16B16A16_UNORM, VK_FORMAT_R16G16B16A16_UNORM, {VK_FORMAT_R16G16B16A16_SFLOAT});
	SetComponentMapping(PF_R16G16B16A16_UNORM, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

	MapFormatSupport(PF_R8G8, VK_FORMAT_R8G8_UNORM);
	SetComponentMapping(PF_R8G8, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupport(PF_V8U8, VK_FORMAT_R8G8_UNORM);
	SetComponentMapping(PF_V8U8, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

	MapFormatSupport(PF_R32_FLOAT, VK_FORMAT_R32_SFLOAT);
	SetComponentMapping(PF_R32_FLOAT, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

	if (FVulkanPlatform::SupportsBCTextureFormats())
	{
		MapFormatSupport(PF_DXT1, VK_FORMAT_BC1_RGB_UNORM_BLOCK);	// Also what OpenGL expects (RGBA instead RGB, but not SRGB)
		SetComponentMapping(PF_DXT1, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE);

		MapFormatSupport(PF_DXT3, VK_FORMAT_BC2_UNORM_BLOCK);
		SetComponentMapping(PF_DXT3, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

		MapFormatSupport(PF_DXT5, VK_FORMAT_BC3_UNORM_BLOCK);
		SetComponentMapping(PF_DXT5, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

		MapFormatSupport(PF_BC4, VK_FORMAT_BC4_UNORM_BLOCK);
		SetComponentMapping(PF_BC4, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

		MapFormatSupport(PF_BC5, VK_FORMAT_BC5_UNORM_BLOCK);
		SetComponentMapping(PF_BC5, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

		MapFormatSupport(PF_BC6H, VK_FORMAT_BC6H_UFLOAT_BLOCK);
		SetComponentMapping(PF_BC6H, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

		MapFormatSupport(PF_BC7, VK_FORMAT_BC7_UNORM_BLOCK);
		SetComponentMapping(PF_BC7, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);
	}

	if (FVulkanPlatform::SupportsASTCTextureFormats())
	{
		MapFormatSupport(PF_ASTC_4x4, VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
		if (GPixelFormats[PF_ASTC_4x4].Supported)
		{
			SetComponentMapping(PF_ASTC_4x4, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);
		}

		MapFormatSupport(PF_ASTC_6x6, VK_FORMAT_ASTC_6x6_UNORM_BLOCK);
		if (GPixelFormats[PF_ASTC_6x6].Supported)
		{
			SetComponentMapping(PF_ASTC_6x6, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);
		}

		MapFormatSupport(PF_ASTC_8x8, VK_FORMAT_ASTC_8x8_UNORM_BLOCK);
		if (GPixelFormats[PF_ASTC_8x8].Supported)
		{
			SetComponentMapping(PF_ASTC_8x8, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);
		}

		MapFormatSupport(PF_ASTC_10x10, VK_FORMAT_ASTC_10x10_UNORM_BLOCK);
		if (GPixelFormats[PF_ASTC_10x10].Supported)
		{
			SetComponentMapping(PF_ASTC_10x10, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);
		}

		MapFormatSupport(PF_ASTC_12x12, VK_FORMAT_ASTC_12x12_UNORM_BLOCK);
		if (GPixelFormats[PF_ASTC_12x12].Supported)
		{
			SetComponentMapping(PF_ASTC_12x12, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);
		}

		// ETC1 is a subset of ETC2 R8G8B8.
		MapFormatSupport(PF_ETC1, VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK);
		if (GPixelFormats[PF_ETC1].Supported)
		{
			SetComponentMapping(PF_ETC1, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE);
		}

		MapFormatSupport(PF_ETC2_RGB, VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK);
		if (GPixelFormats[PF_ETC2_RGB].Supported)
		{
			SetComponentMapping(PF_ETC2_RGB, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE);
		}

		MapFormatSupport(PF_ETC2_RGBA, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
		if (GPixelFormats[PF_ETC2_RGB].Supported)
		{
			SetComponentMapping(PF_ETC2_RGBA, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);
		}
	}

	// Verify available Vertex Formats
	static_assert(VET_None == 0, "Change loop below to skip VET_None");
	for (int32 Index = (int32)VET_None + 1; Index < VET_MAX; ++Index)
	{
		EVertexElementType UEType = (EVertexElementType)Index;
		VkFormat VulkanFormat = UEToVkBufferFormat(UEType);
		if (!IsBufferFormatSupported(VulkanFormat))
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("EVertexFormat(%d) is not supported with Vk format %d"), (int32)UEType, (int32)VulkanFormat);
		}
	}
}

#if VULKAN_SUPPORTS_COLOR_CONVERSIONS
VkSamplerYcbcrConversion FVulkanDevice::CreateSamplerColorConversion(const VkSamplerYcbcrConversionCreateInfo& CreateInfo)
{
	const uint32 CreateInfoHash = FCrc::MemCrc32(&CreateInfo, sizeof(CreateInfo));
	VkSamplerYcbcrConversion* const FindResult = SamplerColorConversionMap.Find(CreateInfoHash);
	if (FindResult != nullptr)
	{
		return *FindResult;
	}
	else
	{
		VkSamplerYcbcrConversion NewConversion;
		VERIFYVULKANRESULT(VulkanRHI::vkCreateSamplerYcbcrConversionKHR(GetInstanceHandle(), &CreateInfo, VULKAN_CPU_ALLOCATOR, &NewConversion));
		SamplerColorConversionMap.Add(CreateInfoHash, NewConversion);
		return NewConversion;
	}
}
#endif

void FVulkanDevice::MapFormatSupport(EPixelFormat UEFormat, VkFormat VulkanFormat)
{
	MapFormatSupportWithFallback(UEFormat, VulkanFormat, TArrayView<VkFormat>());
}

void FVulkanDevice::MapFormatSupportWithFallback(EPixelFormat UEFormat, VkFormat VulkanFormat, TArrayView<const VkFormat> FallbackTextureFormats)
{
	VkFormat SupportedTextureFormat = IsTextureFormatSupported(VulkanFormat) ? VulkanFormat : VK_FORMAT_UNDEFINED;
	VkFormat SupportedBufferFormat = IsBufferFormatSupported(VulkanFormat) ? VulkanFormat : VK_FORMAT_UNDEFINED;
	
	FPixelFormatInfo& FormatInfo = GPixelFormats[UEFormat];
	// at this point we don't know if high level code will use this pixel format for buffers or textures
	FormatInfo.Supported = (SupportedTextureFormat!= VK_FORMAT_UNDEFINED || SupportedBufferFormat!= VK_FORMAT_UNDEFINED);
	FormatInfo.PlatformFormat = SupportedTextureFormat;
	
	GVulkanBufferFormat[UEFormat] = SupportedBufferFormat;
		
	if (SupportedTextureFormat == VK_FORMAT_UNDEFINED)
	{
		for (int32 Idx = 0; Idx < FallbackTextureFormats.Num(); ++Idx)
		{
			VkFormat FallbackTextureFormat = FallbackTextureFormats[Idx];
			if (IsTextureFormatSupported(FallbackTextureFormat))
			{
				SupportedTextureFormat = FallbackTextureFormat;
				FormatInfo.PlatformFormat = FallbackTextureFormat;
				FormatInfo.Supported = true;
								
				UE_LOG(LogVulkanRHI, Display, TEXT("EPixelFormat(%d) (images) is not supported with Vk format %d, falling back to Vk format %d"), (int32)UEFormat, (int32)VulkanFormat, (int32)FallbackTextureFormat);
			}
		}
	}
			
	if (!FormatInfo.Supported)
	{
		UE_LOG(LogVulkanRHI, Error, TEXT("EPixelFormat(%d) is not supported with Vk format %d"), (int32)UEFormat, (int32)VulkanFormat);
	}
}

void FVulkanDevice::SetComponentMapping(EPixelFormat UEFormat, VkComponentSwizzle r, VkComponentSwizzle g, VkComponentSwizzle b, VkComponentSwizzle a)
{
	// Please ensure that we support the mapping, otherwise there is no point setting it.
	check(GPixelFormats[UEFormat].Supported);
	VkComponentMapping& ComponentMapping = PixelFormatComponentMapping[UEFormat];
	ComponentMapping.r = r;
	ComponentMapping.g = g;
	ComponentMapping.b = b;
	ComponentMapping.a = a;
}

void FVulkanDevice::MapFormatSupport(EPixelFormat UEFormat, VkFormat VulkanFormat, int32 BlockBytes)
{
	MapFormatSupport(UEFormat, VulkanFormat);
	FPixelFormatInfo& FormatInfo = GPixelFormats[UEFormat];
	FormatInfo.BlockBytes = BlockBytes;
}

bool FVulkanDevice::QueryGPU(int32 DeviceIndex)
{
	bool bDiscrete = false;

	VulkanRHI::vkGetPhysicalDeviceProperties(Gpu, &GpuProps);
#if VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
	if (GetOptionalExtensions().HasKHRGetPhysicalDeviceProperties2)
	{
		VkPhysicalDeviceProperties2KHR GpuProps2;
		ZeroVulkanStruct(GpuProps2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR);
		GpuProps2.pNext = &GpuIdProps;
		ZeroVulkanStruct(GpuIdProps, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES_KHR);
		VulkanRHI::vkGetPhysicalDeviceProperties2KHR(Gpu, &GpuProps2);
	}
#endif
	auto GetDeviceTypeString = [&]()
	{
		FString Info;
		switch (GpuProps.deviceType)
		{
		case  VK_PHYSICAL_DEVICE_TYPE_OTHER:
			Info = TEXT("Other");
			break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			Info = TEXT("Integrated GPU");
			break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			Info = TEXT("Discrete GPU");
			bDiscrete = true;
			break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
			Info = TEXT("Virtual GPU");
			break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU:
			Info = TEXT("CPU");
			break;
		default:
			Info = TEXT("Unknown");
			break;
		}
		return Info;
	};

	UE_LOG(LogVulkanRHI, Display, TEXT("Device %d: %s"), DeviceIndex, ANSI_TO_TCHAR(GpuProps.deviceName));
	UE_LOG(LogVulkanRHI, Display, TEXT("- API %d.%d.%d(0x%x) Driver 0x%x VendorId 0x%x"), VK_VERSION_MAJOR(GpuProps.apiVersion), VK_VERSION_MINOR(GpuProps.apiVersion), VK_VERSION_PATCH(GpuProps.apiVersion), GpuProps.apiVersion, GpuProps.driverVersion, GpuProps.vendorID);
	UE_LOG(LogVulkanRHI, Display, TEXT("- DeviceID 0x%x Type %s"), GpuProps.deviceID, *GetDeviceTypeString());
	UE_LOG(LogVulkanRHI, Display, TEXT("- Max Descriptor Sets Bound %d Timestamps %d"), GpuProps.limits.maxBoundDescriptorSets, GpuProps.limits.timestampComputeAndGraphics);

	uint32 QueueCount = 0;
	VulkanRHI::vkGetPhysicalDeviceQueueFamilyProperties(Gpu, &QueueCount, nullptr);
	check(QueueCount >= 1);

	QueueFamilyProps.AddUninitialized(QueueCount);
	VulkanRHI::vkGetPhysicalDeviceQueueFamilyProperties(Gpu, &QueueCount, QueueFamilyProps.GetData());

	return bDiscrete;
}

void FVulkanDevice::InitGPU(int32 DeviceIndex)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanMisc);

	// Query features
	VulkanRHI::vkGetPhysicalDeviceFeatures(Gpu, &PhysicalFeatures);

	UE_LOG(LogVulkanRHI, Display, TEXT("Using Device %d: Geometry %d Tessellation %d"), DeviceIndex, PhysicalFeatures.geometryShader, PhysicalFeatures.tessellationShader);

	CreateDevice();

	SetupFormats();

	MemoryManager.Init(this);

	ResourceHeapManager.Init();

	FenceManager.Init(this);

	StagingManager.Init(this);

#if VULKAN_SUPPORTS_GPU_CRASH_DUMPS
	if (GGPUCrashDebuggingEnabled)
	{
		if (OptionalDeviceExtensions.HasAMDBufferMarker)
		{
			VkBufferCreateInfo CreateInfo;
			ZeroVulkanStruct(CreateInfo, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
			CreateInfo.size = GMaxCrashBufferEntries * sizeof(uint32_t);
			CreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			VERIFYVULKANRESULT(VulkanRHI::vkCreateBuffer(Device, &CreateInfo, VULKAN_CPU_ALLOCATOR, &CrashMarker.Buffer));

			VkMemoryRequirements MemReq;
			FMemory::Memzero(MemReq);
			VulkanRHI::vkGetBufferMemoryRequirements(Device, CrashMarker.Buffer, &MemReq);

			CrashMarker.Allocation = MemoryManager.Alloc(false, CreateInfo.size, MemReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
				VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, nullptr, __FILE__, __LINE__);

			uint32* Entry = (uint32*)CrashMarker.Allocation->Map(VK_WHOLE_SIZE, 0);
			check(Entry);
			// Start with 0 entries
			*Entry = 0;
			VERIFYVULKANRESULT(VulkanRHI::vkBindBufferMemory(Device, CrashMarker.Buffer, CrashMarker.Allocation->GetHandle(), 0));
		}
		else if (OptionalDeviceExtensions.HasNVDiagnosticCheckpoints)
		{
			CrashMarker.Allocation = MemoryManager.Alloc(false, GMaxCrashBufferEntries * sizeof(uint32_t), UINT32_MAX, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
				VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, nullptr, __FILE__, __LINE__);
			uint32* Entry = (uint32*)CrashMarker.Allocation->Map(VK_WHOLE_SIZE, 0);
			check(Entry);
			// Start with 0 entries
			*Entry = 0;
		}
	}
#endif

	if (UseVulkanDescriptorCache())
	{
		DescriptorSetCache = new FVulkanDescriptorSetCache(this);
	}
	else
	{
		DescriptorPoolsManager = new FVulkanDescriptorPoolsManager();
		DescriptorPoolsManager->Init(this);
	}

	PipelineStateCache = new FVulkanPipelineStateCacheManager(this);

	TArray<FString> CacheFilenames;
	FString StagedCacheDirectory = FPaths::ProjectDir() / TEXT("Build") / TEXT("ShaderCaches") / FPlatformProperties::IniPlatformName();

	// look for any staged caches
	TArray<FString> StagedCaches;
	IFileManager::Get().FindFiles(StagedCaches, *StagedCacheDirectory, TEXT("cache"));
	// FindFiles returns the filenames without directory, so prepend the stage directory
	for (const FString& Filename : StagedCaches)
	{
		CacheFilenames.Add(StagedCacheDirectory / Filename);
	}

	// always look in the saved directory (for the cache from previous run that wasn't moved over to stage directory)
	CacheFilenames.Add(VulkanRHI::GetPipelineCacheFilename());

	ImmediateContext = new FVulkanCommandListContextImmediate((FVulkanDynamicRHI*)GDynamicRHI, this, GfxQueue);

	if (GfxQueue->GetFamilyIndex() != ComputeQueue->GetFamilyIndex() && GRHIAllowAsyncComputeCvar.GetValueOnAnyThread() != 0)
	{
		ComputeContext = new FVulkanCommandListContextImmediate((FVulkanDynamicRHI*)GDynamicRHI, this, ComputeQueue);
		GEnableAsyncCompute = true;
	}
	else
	{
		ComputeContext = ImmediateContext;
	}

	extern TAutoConsoleVariable<int32> GRHIThreadCvar;
	if (GRHIThreadCvar->GetInt() > 1)
	{
		int32 Num = FTaskGraphInterface::Get().GetNumWorkerThreads();
		for (int32 Index = 0; Index < Num; Index++)
		{
			FVulkanCommandListContext* CmdContext = new FVulkanCommandListContext((FVulkanDynamicRHI*)GDynamicRHI, this, GfxQueue, ImmediateContext);
			CommandContexts.Add(CmdContext);
		}
	}

#if VULKAN_SUPPORTS_VALIDATION_CACHE
	if (OptionalDeviceExtensions.HasEXTValidationCache)
	{
		LoadValidationCache(Device, ValidationCache);
	}
#endif

	PipelineStateCache->InitAndLoad(CacheFilenames);

	// Setup default resource
	{
		FSamplerStateInitializerRHI Default(SF_Point);
		DefaultSampler = ResourceCast(RHICreateSamplerState(Default).GetReference());

		FRHIResourceCreateInfo CreateInfo;
		DefaultImage = new FVulkanSurface(*this, VK_IMAGE_VIEW_TYPE_2D, PF_B8G8R8A8, 1, 1, 1, false, 0, 1, 1, TexCreate_RenderTargetable | TexCreate_ShaderResource, CreateInfo);
		DefaultTextureView.Create(*this, DefaultImage->Image, VK_IMAGE_VIEW_TYPE_2D, DefaultImage->GetFullAspectMask(), PF_B8G8R8A8, VK_FORMAT_B8G8R8A8_UNORM, 0, 1, 0, 1);
	}
}

void FVulkanDevice::PrepareForDestroy()
{
	WaitUntilIdle();
}

void FVulkanDevice::Destroy()
{
#if VULKAN_SUPPORTS_VALIDATION_CACHE
	if (ValidationCache != VK_NULL_HANDLE)
	{
		PFN_vkDestroyValidationCacheEXT vkDestroyValidationCache = (PFN_vkDestroyValidationCacheEXT)(void*)VulkanRHI::vkGetDeviceProcAddr(Device, "vkDestroyValidationCacheEXT");
		if (vkDestroyValidationCache)
		{
			vkDestroyValidationCache(Device, ValidationCache, VULKAN_CPU_ALLOCATOR);
		}
	}
#endif

	VulkanRHI::vkDestroyImageView(GetInstanceHandle(), DefaultTextureView.View, VULKAN_CPU_ALLOCATOR);
	DefaultTextureView = {};

	delete DescriptorSetCache;
	DescriptorSetCache = nullptr;
	
	delete DescriptorPoolsManager;
	DescriptorPoolsManager = nullptr;

	// No need to delete as it's stored in SamplerMap
	DefaultSampler = nullptr;

	delete DefaultImage;
	DefaultImage = nullptr;

#if VULKAN_SUPPORTS_COLOR_CONVERSIONS
	for (const auto& Pair : SamplerColorConversionMap)
	{
		VulkanRHI::vkDestroySamplerYcbcrConversionKHR(GetInstanceHandle(), Pair.Value, VULKAN_CPU_ALLOCATOR);
	}
	SamplerColorConversionMap.Reset();
#endif

	for (int32 Index = CommandContexts.Num() - 1; Index >= 0; --Index)
	{
		delete CommandContexts[Index];
	}
	CommandContexts.Reset();

	if (ComputeContext != ImmediateContext)
	{
		delete ComputeContext;
	}
	ComputeContext = nullptr;

	delete ImmediateContext;
	ImmediateContext = nullptr;

	for (FVulkanOcclusionQueryPool* Pool : UsedOcclusionQueryPools)
	{
		delete Pool;
	}
	UsedOcclusionQueryPools.SetNum(0, false);
	for (FVulkanOcclusionQueryPool* Pool : FreeOcclusionQueryPools)
	{
		delete Pool;
	}
	FreeOcclusionQueryPools.SetNum(0, false);

	delete PipelineStateCache;
	PipelineStateCache = nullptr;
	StagingManager.Deinit();

	if (GGPUCrashDebuggingEnabled)
	{
#if VULKAN_SUPPORTS_AMD_BUFFER_MARKER
		if (OptionalDeviceExtensions.HasAMDBufferMarker)
		{
			CrashMarker.Allocation->Unmap();
			VulkanRHI::vkDestroyBuffer(Device, CrashMarker.Buffer, VULKAN_CPU_ALLOCATOR);
			CrashMarker.Buffer = VK_NULL_HANDLE;

			MemoryManager.Free(CrashMarker.Allocation);
		}
#endif
#if VULKAN_SUPPORTS_NV_DIAGNOSTIC_CHECKPOINT
		if (OptionalDeviceExtensions.HasNVDiagnosticCheckpoints)
		{
			CrashMarker.Allocation->Unmap();
			MemoryManager.Free(CrashMarker.Allocation);
		}
#endif
	}

	FRHIResource::FlushPendingDeletes();
	DeferredDeletionQueue.Clear();

	ResourceHeapManager.Deinit();

	delete TransferQueue;
	delete ComputeQueue;
	delete GfxQueue;

	FenceManager.Deinit();
	MemoryManager.Deinit();

	VulkanRHI::vkDestroyDevice(Device, VULKAN_CPU_ALLOCATOR);
	Device = VK_NULL_HANDLE;
}

void FVulkanDevice::WaitUntilIdle()
{
	VERIFYVULKANRESULT(VulkanRHI::vkDeviceWaitIdle(Device));

	//#todo-rco: Loop through all contexts!
	GetImmediateContext().GetCommandBufferManager()->RefreshFenceStatus();
}

bool FVulkanDevice::IsTextureFormatSupported(VkFormat Format) const
{
	auto ArePropertiesSupported = [](const VkFormatProperties& Prop) -> bool
	{
		return (Prop.linearTilingFeatures != 0) || (Prop.optimalTilingFeatures != 0);
	};

	if (Format >= 0 && Format < VK_FORMAT_RANGE_SIZE)
	{
		const VkFormatProperties& Prop = FormatProperties[Format];
		return ArePropertiesSupported(Prop);
	}

	// Check for extension formats
	const VkFormatProperties* FoundProperties = ExtensionFormatProperties.Find(Format);
	if (FoundProperties)
	{
		return ArePropertiesSupported(*FoundProperties);
	}

	// Add it for faster caching next time
	VkFormatProperties& NewProperties = ExtensionFormatProperties.Add(Format);
	FMemory::Memzero(NewProperties);
	VulkanRHI::vkGetPhysicalDeviceFormatProperties(Gpu, Format, &NewProperties);

	return ArePropertiesSupported(NewProperties);
}

bool FVulkanDevice::IsBufferFormatSupported(VkFormat Format) const
{
	auto ArePropertiesSupported = [](const VkFormatProperties& Prop) -> bool
	{
		return (Prop.bufferFeatures != 0);
	};

	if (Format >= 0 && Format < VK_FORMAT_RANGE_SIZE)
	{
		const VkFormatProperties& Prop = FormatProperties[Format];
		return Prop.bufferFeatures != 0;
	}

	// Check for extension formats
	const VkFormatProperties* FoundProperties = ExtensionFormatProperties.Find(Format);
	if (FoundProperties)
	{
		return FoundProperties->bufferFeatures != 0;
	}

	// Add it for faster caching next time
	VkFormatProperties& NewProperties = ExtensionFormatProperties.Add(Format);
	FMemory::Memzero(NewProperties);
	VulkanRHI::vkGetPhysicalDeviceFormatProperties(Gpu, Format, &NewProperties);

	return NewProperties.bufferFeatures != 0;
}

const VkComponentMapping& FVulkanDevice::GetFormatComponentMapping(EPixelFormat UEFormat) const
{
	if (UEFormat == PF_X24_G8)
	{
		return GetFormatComponentMapping(PF_DepthStencil);
	}
	check(GPixelFormats[UEFormat].Supported);
	return PixelFormatComponentMapping[UEFormat];
}

void FVulkanDevice::NotifyDeletedRenderTarget(VkImage Image)
{
	//#todo-rco: Loop through all contexts!
	GetImmediateContext().NotifyDeletedRenderTarget(Image);
}

void FVulkanDevice::NotifyDeletedImage(VkImage Image)
{
	//#todo-rco: Loop through all contexts!
	GetImmediateContext().NotifyDeletedImage(Image);
}

void FVulkanDevice::PrepareForCPURead()
{
	//#todo-rco: Process other contexts first!
	ImmediateContext->PrepareForCPURead();
}

void FVulkanDevice::SubmitCommands(FVulkanCommandListContext* Context)
{
	FVulkanCommandBufferManager* CmdMgr = Context->GetCommandBufferManager();
	if (CmdMgr->HasPendingUploadCmdBuffer())
	{
		CmdMgr->SubmitUploadCmdBuffer();
	}
	if (CmdMgr->HasPendingActiveCmdBuffer())
	{
		//#todo-rco: If we get real render passes then this is not needed
		if (Context->TransitionAndLayoutManager.CurrentRenderPass)
		{
			Context->TransitionAndLayoutManager.EndEmulatedRenderPass(CmdMgr->GetActiveCmdBuffer());
		}

		CmdMgr->SubmitActiveCmdBuffer();
	}
	CmdMgr->PrepareForNewActiveCommandBuffer();
}

void FVulkanDevice::SubmitCommandsAndFlushGPU()
{
	if (ComputeContext != ImmediateContext)
	{
		SubmitCommands(ComputeContext);
	}

	SubmitCommands(ImmediateContext);

	//#todo-rco: Process other contexts first!
}

void FVulkanDevice::NotifyDeletedGfxPipeline(class FVulkanRHIGraphicsPipelineState* Pipeline)
{
	if (ComputeContext != ImmediateContext)
	{
		//ensure(0);
	}

	//#todo-rco: Loop through all contexts!
	if (ImmediateContext)
	{
		ImmediateContext->PendingGfxState->NotifyDeletedPipeline(Pipeline);
	}
}

void FVulkanDevice::NotifyDeletedComputePipeline(class FVulkanComputePipeline* Pipeline)
{
	if (ComputeContext && ComputeContext != ImmediateContext)
	{
		ComputeContext->PendingComputeState->NotifyDeletedPipeline(Pipeline);
	}

	//#todo-rco: Loop through all contexts!
	if (ImmediateContext)
	{
		ImmediateContext->PendingComputeState->NotifyDeletedPipeline(Pipeline);
	}
}

static FCriticalSection GContextCS;
FVulkanCommandListContext* FVulkanDevice::AcquireDeferredContext()
{
	FScopeLock Lock(&GContextCS);
	if (CommandContexts.Num() == 0)
	{
		return new FVulkanCommandListContext((FVulkanDynamicRHI*)GDynamicRHI, this, GfxQueue, ImmediateContext);
	}
	return CommandContexts.Pop(false);
}

void FVulkanDevice::ReleaseDeferredContext(FVulkanCommandListContext* InContext)
{
	check(InContext);
	{
		FScopeLock Lock(&GContextCS);
		CommandContexts.Push(InContext);
	}
}