// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

// moved the setup from VulkanRHIPrivate.h to the platform headers
#include "Windows/WindowsHWrapper.h"

#define VK_USE_PLATFORM_WIN32_KHR				1
#define VK_USE_PLATFORM_WIN32_KHX				1

#define VULKAN_USE_NEW_QUERIES					0

#define	VULKAN_SHOULD_ENABLE_DRAW_MARKERS		(UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
#define VULKAN_HAS_PHYSICAL_DEVICE_PROPERTIES2	1
#define VULKAN_USE_CREATE_WIN32_SURFACE			1
#define VULKAN_COMMANDWRAPPERS_ENABLE			1
#define VULKAN_DYNAMICALLYLOADED				1
#define VULKAN_ENABLE_DESKTOP_HMD_SUPPORT		1
#define VULKAN_SIGNAL_UNIMPLEMENTED()			checkf(false, TEXT("Unimplemented vulkan functionality: %s"), TEXT(__FUNCTION__))

#define	VULKAN_SUPPORTS_DEDICATED_ALLOCATION	0

#define ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(EnumMacro)

#define ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(EnumMacro)	\
	EnumMacro(PFN_vkCreateWin32SurfaceKHR, vkCreateWin32SurfaceKHR) \
	EnumMacro(PFN_vkGetPhysicalDeviceProperties2KHR, vkGetPhysicalDeviceProperties2KHR) \
	EnumMacro(PFN_vkGetImageMemoryRequirements2KHR , vkGetImageMemoryRequirements2KHR) \
	EnumMacro(PFN_vkCmdWriteBufferMarkerAMD, vkCmdWriteBufferMarkerAMD) \
	EnumMacro(PFN_vkGetBufferMemoryRequirements2KHR , vkGetBufferMemoryRequirements2KHR)

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
};

typedef FVulkanWindowsPlatform FVulkanPlatform;
