// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

//#define VK_USE_PLATFORM_ANDROID_KHR					1

#define VULKAN_HAS_PHYSICAL_DEVICE_PROPERTIES2		0
#define VULKAN_ENABLE_DUMP_LAYER					0
#define VULKAN_DYNAMICALLYLOADED					1
#define VULKAN_SHOULD_ENABLE_DRAW_MARKERS			(UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
#define VULKAN_USE_IMAGE_ACQUIRE_FENCES				0
#define VULKAN_SUPPORTS_COLOR_CONVERSIONS			1
#define VULKAN_SUPPORTS_GEOMETRY_SHADERS			1


#define ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(EnumMacro)

#define ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(EnumMacro) \
	EnumMacro(PFN_vkCreateSamplerYcbcrConversionKHR, vkCreateSamplerYcbcrConversionKHR) \
	EnumMacro(PFN_vkDestroySamplerYcbcrConversionKHR, vkDestroySamplerYcbcrConversionKHR)

#define ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(EnumMacro)

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

	static void SetupFeatureLevels()
	{
		GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2] = SP_VULKAN_ES3_1_LUMIN;
		GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_VULKAN_ES3_1_LUMIN;
		GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4] = SP_NumPlatforms;
		GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = PLATFORM_LUMINGL4 ? SP_VULKAN_SM5_LUMIN : SP_NumPlatforms;
	}

	static bool SupportsStandardSwapchain() { return false; }
	static EPixelFormat GetPixelFormatForNonDefaultSwapchain() { return PF_R8G8B8A8; }

	static bool ForceEnableDebugMarkers();

	static bool HasUnifiedMemory() { return true; }

	static void EnablePhysicalDeviceFeatureExtensions(VkDeviceCreateInfo& DeviceInfo);

protected:
	static void* VulkanLib;
	static bool bAttemptedLoad;
	static VkPhysicalDeviceSamplerYcbcrConversionFeatures SamplerConversion;
};

typedef FVulkanLuminPlatform FVulkanPlatform;
