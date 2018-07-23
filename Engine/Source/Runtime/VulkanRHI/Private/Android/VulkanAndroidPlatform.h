// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#define VK_USE_PLATFORM_ANDROID_KHR					1

#define VULKAN_HAS_PHYSICAL_DEVICE_PROPERTIES2		0
#define VULKAN_ENABLE_DUMP_LAYER					0
#define VULKAN_COMMANDWRAPPERS_ENABLE				0
#define VULKAN_DYNAMICALLYLOADED					1
#define VULKAN_SHOULD_ENABLE_DRAW_MARKERS			(UE_BUILD_DEVELOPMENT || UE_BUILD_DEBUG)
#define VULKAN_USE_DESCRIPTOR_POOL_MANAGER			0
#define VULKAN_USE_IMAGE_ACQUIRE_FENCES				0
#define VULKAN_USE_CREATE_ANDROID_SURFACE			1


#define ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(EnumMacro)

#define ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(EnumMacro) \
	EnumMacro(PFN_vkCreateAndroidSurfaceKHR, vkCreateAndroidSurfaceKHR)

#include "../VulkanLoader.h"

// and now, include the GenericPlatform class
#include "../VulkanGenericPlatform.h"



class FVulkanAndroidPlatform : public FVulkanGenericPlatform
{
public:
	static bool LoadVulkanLibrary();
	static bool LoadVulkanInstanceFunctions(VkInstance inInstance);
	static void FreeVulkanLibrary();

	static void GetInstanceExtensions(TArray<const ANSICHAR*>& OutExtensions);
	static void GetDeviceExtensions(TArray<const ANSICHAR*>& OutExtensions);

	static void CreateSurface(void* WindowHandle, VkInstance Instance, VkSurfaceKHR* OutSurface);

	static bool SupportsBCTextureFormats() { return false; }
	static bool SupportsASTCTextureFormats() { return true; }
	static bool SupportsQuerySurfaceProperties() { return false; }

	static bool SupportsStandardSwapchain();
	static EPixelFormat GetPixelFormatForNonDefaultSwapchain();

	static bool SupportsDepthFetchDuringDepthTest() { return true; }
	static bool SupportsTimestampRenderQueries() { return false; }

	static bool RequiresMobileRenderer() { return true; }
	static void OverrideCrashHandlers();

	//#todo-rco: Detect Mali?
	static bool RequiresPresentLayoutFix() { return true; }

	static bool HasUnifiedMemory() { return true; }

	static bool RegisterGPUWork() { return false; }

protected:
	static void* VulkanLib;
	static bool bAttemptedLoad;
};

typedef FVulkanAndroidPlatform FVulkanPlatform;
