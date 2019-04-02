// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

// moved the setup from VulkanRHIPrivate.h to the platform headers
#include "Windows/WindowsHWrapper.h"
#include "RHI.h"

#define VK_USE_PLATFORM_WIN32_KHR					1
#define VK_USE_PLATFORM_WIN32_KHX					1

#define	VULKAN_SHOULD_ENABLE_DRAW_MARKERS			(UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
#define VULKAN_HAS_PHYSICAL_DEVICE_PROPERTIES2		1
#define VULKAN_USE_CREATE_WIN32_SURFACE				1
#define VULKAN_DYNAMICALLYLOADED					1
#define VULKAN_ENABLE_DESKTOP_HMD_SUPPORT			1
#define VULKAN_SIGNAL_UNIMPLEMENTED()				checkf(false, TEXT("Unimplemented vulkan functionality: %s"), TEXT(__FUNCTION__))
#define VULKAN_SUPPORTS_COLOR_CONVERSIONS			1
#define	VULKAN_SUPPORTS_DEDICATED_ALLOCATION		0
#define VULKAN_SUPPORTS_AMD_BUFFER_MARKER			1
#define VULKAN_SUPPORTS_NV_DIAGNOSTIC_CHECKPOINT	1

#define	UE_VK_API_VERSION							VK_API_VERSION_1_1


// 32-bit windows has warnings on custom mem mgr callbacks
#define VULKAN_SHOULD_USE_LLM					(UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT) && !PLATFORM_32BITS

#define ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(EnumMacro)

#define ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(EnumMacro)	\
	EnumMacro(PFN_vkCreateWin32SurfaceKHR, vkCreateWin32SurfaceKHR) \
	EnumMacro(PFN_vkGetPhysicalDeviceProperties2KHR, vkGetPhysicalDeviceProperties2KHR) \
	EnumMacro(PFN_vkGetImageMemoryRequirements2KHR , vkGetImageMemoryRequirements2KHR) \
	EnumMacro(PFN_vkCmdWriteBufferMarkerAMD, vkCmdWriteBufferMarkerAMD) \
	EnumMacro(PFN_vkCmdSetCheckpointNV, vkCmdSetCheckpointNV) \
	EnumMacro(PFN_vkGetQueueCheckpointDataNV, vkGetQueueCheckpointDataNV) \
	EnumMacro(PFN_vkGetBufferMemoryRequirements2KHR , vkGetBufferMemoryRequirements2KHR)

#define ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(EnumMacro) \
	EnumMacro(PFN_vkCreateSamplerYcbcrConversionKHR, vkCreateSamplerYcbcrConversionKHR) \
	EnumMacro(PFN_vkDestroySamplerYcbcrConversionKHR, vkDestroySamplerYcbcrConversionKHR)

#include "../VulkanLoader.h"

// and now, include the GenericPlatform class
#include "../VulkanGenericPlatform.h"

class FVulkanWindowsPlatform : public FVulkanGenericPlatform
{
public:
	static void CheckDeviceDriver(uint32 DeviceIndex, const VkPhysicalDeviceProperties& Props);

	static bool LoadVulkanLibrary();
	static bool LoadVulkanInstanceFunctions(VkInstance inInstance);
	static void FreeVulkanLibrary();

	static void GetInstanceExtensions(TArray<const ANSICHAR*>& OutExtensions);
	static void GetDeviceExtensions(TArray<const ANSICHAR*>& OutExtensions);

	static void CreateSurface(void* WindowHandle, VkInstance Instance, VkSurfaceKHR* OutSurface);

	static bool SupportsDeviceLocalHostVisibleWithNoPenalty();

	static void WriteCrashMarker(const FOptionalVulkanDeviceExtensions& OptionalExtensions, VkCommandBuffer CmdBuffer, VkBuffer DestBuffer, const TArrayView<uint32>& Entries, bool bAdding);

	// Some platforms only support real or non-real UBs, so this function can optimize it out
	static bool UseRealUBsOptimization(bool bCodeHeaderUseRealUBs)
	{
		static auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Vulkan.UseRealUBs"));
		return (CVar && CVar->GetValueOnAnyThread() == 0) ? false : bCodeHeaderUseRealUBs;
	}
};

typedef FVulkanWindowsPlatform FVulkanPlatform;
