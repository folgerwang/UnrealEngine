// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VulkanLuminPlatform.h"
#include "../VulkanRHIPrivate.h"
#include <dlfcn.h>

// Vulkan function pointers
#define DEFINE_VK_ENTRYPOINTS(Type,Func) Type VulkanDynamicAPI::Func = NULL;
ENUM_VK_ENTRYPOINTS_ALL(DEFINE_VK_ENTRYPOINTS)

#define CHECK_VK_ENTRYPOINTS(Type,Func) if (VulkanDynamicAPI::Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(LogRHI, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }

static bool GFoundTegraGfxDebugger = false;

void* FVulkanLuminPlatform::VulkanLib = nullptr;
bool FVulkanLuminPlatform::bAttemptedLoad = false;
VkPhysicalDeviceSamplerYcbcrConversionFeatures FVulkanLuminPlatform::SamplerConversion;

bool FVulkanLuminPlatform::LoadVulkanLibrary()
{
	if (bAttemptedLoad)
	{
		return (VulkanLib != nullptr);
	}
	bAttemptedLoad = true;

	// try to load libvulkan.so
	VulkanLib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);

	if (VulkanLib == nullptr)
	{
		return false;
	}

	bool bFoundAllEntryPoints = true;

	// Initialize all of the entry points we have to query manually
#define GET_VK_ENTRYPOINTS(Type,Func) VulkanDynamicAPI::Func = (Type)dlsym(VulkanLib, #Func);
	ENUM_VK_ENTRYPOINTS_BASE(GET_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_BASE(CHECK_VK_ENTRYPOINTS);
	if (!bFoundAllEntryPoints)
	{
		dlclose(VulkanLib);
		VulkanLib = nullptr;
		return false;
	}

	ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(GET_VK_ENTRYPOINTS);
#if UE_BUILD_DEBUG
	ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(CHECK_VK_ENTRYPOINTS);
#endif

#undef GET_VK_ENTRYPOINTS
	return true;
}

bool FVulkanLuminPlatform::LoadVulkanInstanceFunctions(VkInstance inInstance)
{
	bool bFoundAllEntryPoints = true;
#define CHECK_VK_ENTRYPOINTS(Type,Func) if (VulkanDynamicAPI::Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(LogRHI, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }

#define GETINSTANCE_VK_ENTRYPOINTS(Type, Func) VulkanDynamicAPI::Func = (Type)VulkanDynamicAPI::vkGetInstanceProcAddr(inInstance, #Func);
	ENUM_VK_ENTRYPOINTS_INSTANCE(GETINSTANCE_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_INSTANCE(CHECK_VK_ENTRYPOINTS);

	ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(GETINSTANCE_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(CHECK_VK_ENTRYPOINTS);

	ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(GETINSTANCE_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(CHECK_VK_ENTRYPOINTS);

	return bFoundAllEntryPoints;
}

void FVulkanLuminPlatform::FreeVulkanLibrary()
{
	if (VulkanLib != nullptr)
	{
#define CLEAR_VK_ENTRYPOINTS(Type,Func) VulkanDynamicAPI::Func = nullptr;
		ENUM_VK_ENTRYPOINTS_ALL(CLEAR_VK_ENTRYPOINTS);

		dlclose(VulkanLib);
		VulkanLib = nullptr;
	}
	bAttemptedLoad = false;
}

void FVulkanLuminPlatform::CreateSurface(void* WindowHandle, VkInstance Instance, VkSurfaceKHR* OutSurface)
{
	OutSurface = nullptr;
	//VkLuminSurfaceCreateInfoKHR SurfaceCreateInfo;
	//FMemory::Memzero(SurfaceCreateInfo);
	//SurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_Lumin_SURFACE_CREATE_INFO_KHR;
	//SurfaceCreateInfo.window = (ANativeWindow*)WindowHandle;

	//VERIFYVULKANRESULT(vkCreateLuminSurfaceKHR(Instance, &SurfaceCreateInfo, nullptr, OutSurface));
}

void FVulkanLuminPlatform::NotifyFoundInstanceLayersAndExtensions(const TArray<FString>& Layers, const TArray<FString>& Extensions)
{
	if (Extensions.Find(TEXT("VK_LAYER_NV_vgd")))
	{
		GFoundTegraGfxDebugger = true;
	}
}

void FVulkanLuminPlatform::NotifyFoundDeviceLayersAndExtensions(VkPhysicalDevice PhysicalDevice, const TArray<FString>& Layers, const TArray<FString>& Extensions)
{
	if (Extensions.Find(TEXT("VK_LAYER_NV_vgd")))
	{
		GFoundTegraGfxDebugger = true;
	}
}

void FVulkanLuminPlatform::GetInstanceExtensions(TArray<const ANSICHAR*>& OutExtensions)
{
	if (GFoundTegraGfxDebugger)
	{
		OutExtensions.Add("VK_LAYER_NV_vgd");
	}
}

void FVulkanLuminPlatform::GetDeviceExtensions(TArray<const ANSICHAR*>& OutExtensions)
{
	if (GFoundTegraGfxDebugger)
	{
		OutExtensions.Add("VK_LAYER_NV_vgd");
	}
	// YCbCr requires BindMem2 and GetMemReqs2
	OutExtensions.Add(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME);
	OutExtensions.Add(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
	OutExtensions.Add(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);
}

void FVulkanLuminPlatform::SetupFeatureLevels()
{
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2] = SP_VULKAN_ES3_1_LUMIN;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_VULKAN_ES3_1_LUMIN;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4] = SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = SP_VULKAN_SM5_LUMIN;
}

bool FVulkanLuminPlatform::ForceEnableDebugMarkers()
{
	// Preventing VK_EXT_DEBUG_MARKER from being enabled on Lumin, because the device doesn't support it.
	return GFoundTegraGfxDebugger;
}

void FVulkanLuminPlatform::EnablePhysicalDeviceFeatureExtensions(VkDeviceCreateInfo& DeviceInfo)
{
	SamplerConversion.pNext = nullptr;
	SamplerConversion.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;
	SamplerConversion.samplerYcbcrConversion = VK_TRUE;
	DeviceInfo.pNext = &SamplerConversion;
}
