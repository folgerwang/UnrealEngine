// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

//#define VK_USE_PLATFORM_ANDROID_KHR					1

#define VULKAN_HAS_PHYSICAL_DEVICE_PROPERTIES2		0
#define VULKAN_ENABLE_DUMP_LAYER					0
#define VULKAN_COMMANDWRAPPERS_ENABLE				0
#define VULKAN_DYNAMICALLYLOADED					1
#define VULKAN_SHOULD_ENABLE_DRAW_MARKERS			(UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
#define VULKAN_USE_PER_PIPELINE_DESCRIPTOR_POOLS	0
#define VULKAN_USE_DESCRIPTOR_POOL_MANAGER			0
#define VULKAN_USE_IMAGE_ACQUIRE_FENCES				0


#define ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(EnumMacro)

#define ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(EnumMacro)

#include "../VulkanLoader.h"

// and now, include the GenericPlatform class
#include "../VulkanGenericPlatform.h"



class FVulkanLuminPlatform : public FVulkanGenericPlatform
{
public:
	static bool LoadVulkanLibrary();
	static bool LoadVulkanInstanceFunctions(VkInstance inInstance);
	static void FreeVulkanLibrary();

	static void NotifyFoundInstanceLayersAndExtensions(const TArray<FString>& Layers, const TArray<FString>& Extensions);
	static void NotifyFoundDeviceLayersAndExtensions(VkPhysicalDevice PhysicalDevice, const TArray<FString>& Layers, const TArray<FString>& Extensions);

	static void GetInstanceExtensions(TArray<const ANSICHAR*>& OutExtensions);
	static void GetDeviceExtensions(TArray<const ANSICHAR*>& OutExtensions);

	static void CreateSurface(void* WindowHandle, VkInstance Instance, VkSurfaceKHR* OutSurface);

	static bool SupportsBCTextureFormats() { return false; }
	static bool SupportsASTCTextureFormats() { return true; }
	static bool SupportsQuerySurfaceProperties() { return false; }

	static bool SupportsStandardSwapchain() { return false; }
	static EPixelFormat GetPixelFormatForNonDefaultSwapchain() { return PF_R8G8B8A8; }

	static bool ForceEnableDebugMarkers();

	static bool HasUnifiedMemory() { return true; }

protected:
	static void* VulkanLib;
	static bool bAttemptedLoad;
};

typedef FVulkanLuminPlatform FVulkanPlatform;
