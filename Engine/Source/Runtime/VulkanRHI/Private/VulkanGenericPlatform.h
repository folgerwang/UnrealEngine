// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "PixelFormat.h"
#include "Containers/ArrayView.h"

// the platform interface, and empty implementations for platforms that don't need em
class FVulkanGenericPlatform 
{
public:
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

	static bool SupportsStandardSwapchain() { return true; }
	static EPixelFormat GetPixelFormatForNonDefaultSwapchain()
	{
		checkf(0, TEXT("Platform Requires Standard Swapchain!"));
		return PF_Unknown;
	}

	static bool SupportsDepthFetchDuringDepthTest() { return true; }
	static bool SupportsTimestampRenderQueries() { return true; }

	static bool RequiresMobileRenderer() { return false; }
	static void OverrideCrashHandlers() {}

	// Some platforms have issues with the access flags for the Present layout
	static bool RequiresPresentLayoutFix() { return false; }

	static bool ForceEnableDebugMarkers() { return false; }

	static bool SupportsDeviceLocalHostVisibleWithNoPenalty() { return false; }

	static bool HasUnifiedMemory() { return false; }

	static bool RegisterGPUWork() { return true; }

	// Does the platform only support Vertex & Pixel stages (not Geometry or Tessellation)?
	static bool IsVSPSOnly() { return false; }

	static void WriteBufferMarkerAMD(VkCommandBuffer CmdBuffer, VkBuffer DestBuffer, const TArrayView<uint32>& Entries, bool bAdding) {}

	// allow the platform code to restrict the device features
	static void RestrictEnabledPhysicalDeviceFeatures(const VkPhysicalDeviceFeatures& DeviceFeatures, VkPhysicalDeviceFeatures& FeaturesToEnable)
	{ 
		FeaturesToEnable = DeviceFeatures; 

		// disable everything sparse-related
		FeaturesToEnable.shaderResourceResidency	= VK_FALSE;
		FeaturesToEnable.shaderResourceMinLod		= VK_FALSE;
		FeaturesToEnable.sparseBinding				= VK_FALSE;
		FeaturesToEnable.sparseResidencyBuffer		= VK_FALSE;
		FeaturesToEnable.sparseResidencyImage2D		= VK_FALSE;
		FeaturesToEnable.sparseResidencyImage3D		= VK_FALSE;
		FeaturesToEnable.sparseResidency2Samples	= VK_FALSE;
		FeaturesToEnable.sparseResidency4Samples	= VK_FALSE;
		FeaturesToEnable.sparseResidency8Samples	= VK_FALSE;
		FeaturesToEnable.sparseResidencyAliased		= VK_FALSE;
	}
};
