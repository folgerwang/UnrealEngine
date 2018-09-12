// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

// moved the setup from VulkanRHIPrivate.h to the platform headers
#include "Windows/WindowsHWrapper.h"
#include "RHI.h"

#define VK_USE_PLATFORM_WIN32_KHR				1
#define VK_USE_PLATFORM_WIN32_KHX				1

#define	VULKAN_SHOULD_ENABLE_DRAW_MARKERS		(UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
#define VULKAN_HAS_PHYSICAL_DEVICE_PROPERTIES2	1
#define VULKAN_USE_CREATE_WIN32_SURFACE			1
#define VULKAN_DYNAMICALLYLOADED				1
#define VULKAN_ENABLE_DESKTOP_HMD_SUPPORT		1
#define VULKAN_SIGNAL_UNIMPLEMENTED()			checkf(false, TEXT("Unimplemented vulkan functionality: %s"), TEXT(__FUNCTION__))

#define	VULKAN_SUPPORTS_DEDICATED_ALLOCATION	0

// 32-bit windows has warnings on custom mem mgr callbacks
#define VULKAN_SHOULD_USE_LLM					(UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT) && !PLATFORM_32BITS

#define ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(EnumMacro)

#define ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(EnumMacro)	\
	EnumMacro(PFN_vkCreateWin32SurfaceKHR, vkCreateWin32SurfaceKHR) \
	EnumMacro(PFN_vkGetPhysicalDeviceProperties2KHR, vkGetPhysicalDeviceProperties2KHR) \
	EnumMacro(PFN_vkGetImageMemoryRequirements2KHR , vkGetImageMemoryRequirements2KHR) \
	EnumMacro(PFN_vkCmdWriteBufferMarkerAMD, vkCmdWriteBufferMarkerAMD) \
	EnumMacro(PFN_vkGetBufferMemoryRequirements2KHR , vkGetBufferMemoryRequirements2KHR)

#define ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(EnumMacro)

#include "../VulkanLoader.h"

// and now, include the GenericPlatform class
#include "../VulkanGenericPlatform.h"

class FVulkanWindowsPlatform : public FVulkanGenericPlatform
{
public:
	static bool LoadVulkanLibrary();
	static bool LoadVulkanInstanceFunctions(VkInstance inInstance);
	static void FreeVulkanLibrary();

	static void GetInstanceExtensions(TArray<const ANSICHAR*>& OutExtensions);
	static void GetDeviceExtensions(TArray<const ANSICHAR*>& OutExtensions);

	static void CreateSurface(void* WindowHandle, VkInstance Instance, VkSurfaceKHR* OutSurface);

	static bool SupportsDeviceLocalHostVisibleWithNoPenalty();

	static void WriteBufferMarkerAMD(VkCommandBuffer CmdBuffer, VkBuffer DestBuffer, const TArrayView<uint32>& Entries, bool bAdding);

	// Some platforms only support real or non-real UBs, so this function can optimize it out
	static bool UseRealUBsOptimization(bool bCodeHeaderUseRealUBs)
	{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		static auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Vulkan.UseRealUBs"));
		return (CVar && CVar->GetValueOnAnyThread() == 0) ? false : bCodeHeaderUseRealUBs;
#else
		return GMaxRHIFeatureLevel >= ERHIFeatureLevel::ES3_1 ? bCodeHeaderUseRealUBs : false;
#endif
	}
};

typedef FVulkanWindowsPlatform FVulkanPlatform;
