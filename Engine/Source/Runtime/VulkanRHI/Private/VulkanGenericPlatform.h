// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "PixelFormat.h"
#include "Containers/ArrayView.h"
#include "RHI.h"	// for GShaderPlatformForFeatureLevel and its friends

struct FOptionalVulkanDeviceExtensions;

// the platform interface, and empty implementations for platforms that don't need em
class FVulkanGenericPlatform 
{
public:
	static bool IsSupported() { return true; }
	static void CheckDeviceDriver(uint32 DeviceIndex, const VkPhysicalDeviceProperties& Props) {}

	static bool LoadVulkanLibrary() { return true; }
	static bool LoadVulkanInstanceFunctions(VkInstance inInstance) { return true; }
	static void FreeVulkanLibrary() {}

	// Called after querying all the available extensions and layers
	static void NotifyFoundInstanceLayersAndExtensions(const TArray<FString>& Layers, const TArray<FString>& Extensions) {}
	static void NotifyFoundDeviceLayersAndExtensions(VkPhysicalDevice PhysicalDevice, const TArray<FString>& Layers, const TArray<FString>& Extensions) {}

	// Array of required extensions for the platform (Required!)
	static void GetInstanceExtensions(TArray<const ANSICHAR*>& OutExtensions);
	static void GetDeviceExtensions(TArray<const ANSICHAR*>& OutExtensions);

	// create the platform-specific surface object - required
	static void CreateSurface(VkSurfaceKHR* OutSurface);

	// most platforms support BC* but not ASTC*
	static bool SupportsBCTextureFormats() { return true; }
	static bool SupportsASTCTextureFormats() { return false; }

	// most platforms can query the surface for the present mode, and size, etc
	static bool SupportsQuerySurfaceProperties() { return true; }

	static void SetupFeatureLevels();

	static bool SupportsStandardSwapchain() { return true; }
	static EPixelFormat GetPixelFormatForNonDefaultSwapchain()
	{
		checkf(0, TEXT("Platform Requires Standard Swapchain!"));
		return PF_Unknown;
	}

	static bool SupportsDepthFetchDuringDepthTest() { return true; }
	static bool SupportsTimestampRenderQueries() { return true; }

	static bool RequiresMobileRenderer() { return false; }

	// bInit=1 called at RHI init time, bInit=0 at RHI deinit time
	static void OverridePlatformHandlers(bool bInit) {}

	// Some platforms have issues with the access flags for the Present layout
	static bool RequiresPresentLayoutFix() { return false; }

	static bool ForceEnableDebugMarkers() { return false; }

	static bool SupportsDeviceLocalHostVisibleWithNoPenalty() { return false; }

	static bool HasUnifiedMemory() { return false; }

	static bool RegisterGPUWork() { return true; }

	static void WriteCrashMarker(const FOptionalVulkanDeviceExtensions& OptionalExtensions, VkCommandBuffer CmdBuffer, VkBuffer DestBuffer, const TArrayView<uint32>& Entries, bool bAdding) {}

	// Allow the platform code to restrict the device features
	static void RestrictEnabledPhysicalDeviceFeatures(VkPhysicalDeviceFeatures& InOutFeaturesToEnable)
	{ 
		// Disable everything sparse-related
		InOutFeaturesToEnable.shaderResourceResidency	= VK_FALSE;
		InOutFeaturesToEnable.shaderResourceMinLod		= VK_FALSE;
		InOutFeaturesToEnable.sparseBinding				= VK_FALSE;
		InOutFeaturesToEnable.sparseResidencyBuffer		= VK_FALSE;
		InOutFeaturesToEnable.sparseResidencyImage2D	= VK_FALSE;
		InOutFeaturesToEnable.sparseResidencyImage3D	= VK_FALSE;
		InOutFeaturesToEnable.sparseResidency2Samples	= VK_FALSE;
		InOutFeaturesToEnable.sparseResidency4Samples	= VK_FALSE;
		InOutFeaturesToEnable.sparseResidency8Samples	= VK_FALSE;
		InOutFeaturesToEnable.sparseResidencyAliased	= VK_FALSE;
	}

	// Some platforms only support real or non-real UBs, so this function can optimize it out
	static bool UseRealUBsOptimization(bool bCodeHeaderUseRealUBs) { return bCodeHeaderUseRealUBs; }

	static bool SupportParallelRenderingTasks() { return true; }

	// Allow platforms to add extension features to the DeviceInfo pNext chain
	static void EnablePhysicalDeviceFeatureExtensions(VkDeviceCreateInfo& DeviceInfo) {};

	static bool RequiresSwapchainGeneralInitialLayout() { return false; }
	
	// Allow platforms to add extension features to the PresentInfo pNext chain
	static void EnablePresentInfoExtensions(VkPresentInfoKHR& PresentInfo) {}

	// Ensure the last frame completed on the GPU
	static bool RequiresWaitingForFrameCompletionEvent() { return true; }

	// Blocks until hardware window is available
	static void BlockUntilWindowIsAwailable() {};
};
