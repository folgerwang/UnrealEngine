// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanLayers.cpp: Vulkan device layers implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "IHeadMountedDisplayModule.h"

#if VULKAN_HAS_DEBUGGING_ENABLED
bool GRenderDocFound = false;
#endif

#if VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
#include "Runtime/HeadMountedDisplay/Public/IHeadMountedDisplayModule.h"
#endif

#if VULKAN_HAS_DEBUGGING_ENABLED
TAutoConsoleVariable<int32> GValidationCvar(
	TEXT("r.Vulkan.EnableValidation"),
	0,
	TEXT("0 to disable validation layers (default)\n")
	TEXT("1 to enable errors\n")
	TEXT("2 to enable errors & warnings\n")
	TEXT("3 to enable errors, warnings & performance warnings\n")
	TEXT("4 to enable errors, warnings, performance & information messages\n")
	TEXT("5 to enable all messages"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> GStandardValidationCvar(
	TEXT("r.Vulkan.StandardValidation"),
	1,
	TEXT("1 to use VK_LAYER_LUNARG_standard_validation (default) if available\n")
	TEXT("0 to use individual layers"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

#if VULKAN_ENABLE_DRAW_MARKERS
	#define RENDERDOC_LAYER_NAME		"VK_LAYER_RENDERDOC_Capture"
#endif

#define STANDARD_VALIDATION_LAYER_NAME	"VK_LAYER_LUNARG_standard_validation"

static const ANSICHAR* GIndividualValidationLayers[] =
{
	"VK_LAYER_GOOGLE_threading",
	"VK_LAYER_LUNARG_parameter_validation",
	"VK_LAYER_LUNARG_object_tracker",
	"VK_LAYER_LUNARG_core_validation",
	"VK_LAYER_GOOGLE_unique_objects",
	nullptr
};

#endif // VULKAN_HAS_DEBUGGING_ENABLED

// Instance Extensions to enable for all platforms
static const ANSICHAR* GInstanceExtensions[] =
{
#if VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
	VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
	VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
#endif
#if VULKAN_SUPPORTS_VALIDATION_CACHE
	VK_EXT_VALIDATION_CACHE_EXTENSION_NAME,
#endif
	nullptr
};

// Device Extensions to enable
static const ANSICHAR* GDeviceExtensions[] =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,

	//"VK_KHX_device_group",
#if VULKAN_SUPPORTS_MAINTENANCE_LAYER1
	VK_KHR_MAINTENANCE1_EXTENSION_NAME,
#endif
#if VULKAN_SUPPORTS_MAINTENANCE_LAYER2
	VK_KHR_MAINTENANCE2_EXTENSION_NAME,
#endif
#if VULKAN_SUPPORTS_VALIDATION_CACHE
	VK_EXT_VALIDATION_CACHE_EXTENSION_NAME,
#endif
	//VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME,
	nullptr
};

TSharedPtr< IHeadMountedDisplayVulkanExtensions, ESPMode::ThreadSafe > FVulkanDynamicRHI::HMDVulkanExtensions;

struct FLayerExtension
{
	FLayerExtension()
	{
		FMemory::Memzero(LayerProps);
	}

	void AddUniqueExtensionNames(TArray<FString>& Out)
	{
		for (int32 ExtIndex = 0; ExtIndex < ExtensionProps.Num(); ++ExtIndex)
		{
			Out.AddUnique(ANSI_TO_TCHAR(ExtensionProps[ExtIndex].extensionName));
		}
	}

	void AddAnsiExtensionNames(TArray<const char*>& Out)
	{
		for (int32 ExtIndex = 0; ExtIndex < ExtensionProps.Num(); ++ExtIndex)
		{
			Out.AddUnique(ExtensionProps[ExtIndex].extensionName);
		}
	}

	VkLayerProperties LayerProps;
	TArray<VkExtensionProperties> ExtensionProps;
};

static inline void EnumerateInstanceExtensionProperties(const ANSICHAR* LayerName, FLayerExtension& OutLayer)
{
	VkResult Result;
	do
	{
		uint32 Count = 0;
		Result = VulkanRHI::vkEnumerateInstanceExtensionProperties(LayerName, &Count, nullptr);
		check(Result >= VK_SUCCESS);

		if (Count > 0)
		{
			OutLayer.ExtensionProps.Empty(Count);
			OutLayer.ExtensionProps.AddUninitialized(Count);
			Result = VulkanRHI::vkEnumerateInstanceExtensionProperties(LayerName, &Count, OutLayer.ExtensionProps.GetData());
			check(Result >= VK_SUCCESS);
		}
	}
	while (Result == VK_INCOMPLETE);
}

static inline void EnumerateDeviceExtensionProperties(VkPhysicalDevice Device, const ANSICHAR* LayerName, FLayerExtension& OutLayer)
{
	VkResult Result;
	do
	{
		uint32 Count = 0;
		Result = VulkanRHI::vkEnumerateDeviceExtensionProperties(Device, LayerName, &Count, nullptr);
		check(Result >= VK_SUCCESS);

		if (Count > 0)
		{
			OutLayer.ExtensionProps.Empty(Count);
			OutLayer.ExtensionProps.AddUninitialized(Count);
			Result = VulkanRHI::vkEnumerateDeviceExtensionProperties(Device, LayerName, &Count, OutLayer.ExtensionProps.GetData());
			check(Result >= VK_SUCCESS);
		}
	}
	while (Result == VK_INCOMPLETE);
}


static inline void TrimDuplicates(TArray<const ANSICHAR*>& Array)
{
	for (int32 OuterIndex = Array.Num() - 1; OuterIndex >= 0; --OuterIndex)
	{
		bool bFound = false;
		for (int32 InnerIndex = OuterIndex - 1; InnerIndex >= 0; --InnerIndex)
		{
			if (!FCStringAnsi::Strcmp(Array[OuterIndex], Array[InnerIndex]))
			{
				bFound = true;
				break;
			}
		}

		if (bFound)
		{
			Array.RemoveAtSwap(OuterIndex, 1, false);
		}
	}
}

static inline int32 FindLayerIndexInList(const TArray<FLayerExtension>& List, const char* LayerName)
{
	// 0 is reserved for NULL/instance
	for (int32 Index = 1; Index < List.Num(); ++Index)
	{
		if (!FCStringAnsi::Strcmp(List[Index].LayerProps.layerName, LayerName))
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

static inline bool FindLayerInList(const TArray<FLayerExtension>& List, const char* LayerName)
{
	return FindLayerIndexInList(List, LayerName) != INDEX_NONE;
}

static inline bool FindLayerExtensionInList(const TArray<FLayerExtension>& List, const char* ExtensionName, const char*& FoundLayer)
{
	for (int32 Index = 0; Index < List.Num(); ++Index)
	{
		for (int32 ExtIndex = 0; ExtIndex < List[Index].ExtensionProps.Num(); ++ExtIndex)
		{
			if (!FCStringAnsi::Strcmp(List[Index].ExtensionProps[ExtIndex].extensionName, ExtensionName))
			{
				FoundLayer = List[Index].LayerProps.layerName;
				return true;
			}
		}
	}

	return false;
}

static inline bool FindLayerExtensionInList(const TArray<FLayerExtension>& List, const char* ExtensionName)
{
	const char* Dummy = nullptr;
	return FindLayerExtensionInList(List, ExtensionName, Dummy);
}


void FVulkanDynamicRHI::GetInstanceLayersAndExtensions(TArray<const ANSICHAR*>& OutInstanceExtensions, TArray<const ANSICHAR*>& OutInstanceLayers, bool& bOutDebugUtils)
{
	bOutDebugUtils = false;

	TArray<FLayerExtension> GlobalLayerExtensions;
	// 0 is reserved for NULL/instance
	GlobalLayerExtensions.AddDefaulted();

	VkResult Result;

	// Global extensions
	EnumerateInstanceExtensionProperties(nullptr, GlobalLayerExtensions[0]);

	TArray<FString> FoundUniqueExtensions;
	TArray<FString> FoundUniqueLayers;
	for (int32 Index = 0; Index < GlobalLayerExtensions[0].ExtensionProps.Num(); ++Index)
	{
		FoundUniqueExtensions.AddUnique(ANSI_TO_TCHAR(GlobalLayerExtensions[0].ExtensionProps[Index].extensionName));
	}

	{
		TArray<VkLayerProperties> GlobalLayerProperties;
		do
		{
			uint32 InstanceLayerCount = 0;
			Result = VulkanRHI::vkEnumerateInstanceLayerProperties(&InstanceLayerCount, nullptr);
			check(Result >= VK_SUCCESS);

			if (InstanceLayerCount > 0)
			{
				GlobalLayerProperties.AddZeroed(InstanceLayerCount);
				Result = VulkanRHI::vkEnumerateInstanceLayerProperties(&InstanceLayerCount, &GlobalLayerProperties[GlobalLayerProperties.Num() - InstanceLayerCount]);
				check(Result >= VK_SUCCESS);
			}
		}
		while (Result == VK_INCOMPLETE);

		for (int32 Index = 0; Index < GlobalLayerProperties.Num(); ++Index)
		{
			FLayerExtension* Layer = new(GlobalLayerExtensions) FLayerExtension;
			Layer->LayerProps = GlobalLayerProperties[Index];
			EnumerateInstanceExtensionProperties(GlobalLayerProperties[Index].layerName, *Layer);
			Layer->AddUniqueExtensionNames(FoundUniqueExtensions);
			FoundUniqueLayers.AddUnique(ANSI_TO_TCHAR(GlobalLayerProperties[Index].layerName));
		}
	}

	FoundUniqueLayers.Sort();
	for (const FString& Name : FoundUniqueLayers)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("- Found instance layer %s"), *Name);
	}

	FoundUniqueExtensions.Sort();
	for (const FString& Name : FoundUniqueExtensions)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("- Found instance extension %s"), *Name);
	}

	FVulkanPlatform::NotifyFoundInstanceLayersAndExtensions(FoundUniqueLayers, FoundUniqueExtensions);

	bool bVkTrace = false;
	if (FParse::Param(FCommandLine::Get(), TEXT("vktrace")))
	{
		const char* VkTraceName = "VK_LAYER_LUNARG_vktrace";
		if (FindLayerInList(GlobalLayerExtensions, VkTraceName))
		{
			OutInstanceLayers.Add(VkTraceName);
			bVkTrace = true;
		}
	}

#if VULKAN_HAS_DEBUGGING_ENABLED
#if VULKAN_ENABLE_API_DUMP
	if (!bVkTrace)
	{
		const char* VkApiDumpName = "VK_LAYER_LUNARG_api_dump";
		bool bApiDumpFound = FindLayerInList(GlobalLayerExtensions, VkApiDumpName);
		if (bApiDumpFound)
		{
			OutInstanceLayers.Add(VkApiDumpName);
		}
		else
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to find Vulkan instance layer %s"), ANSI_TO_TCHAR(VkApiDumpName));
		}
	}
#endif	// VULKAN_ENABLE_API_DUMP

	int32 VulkanValidationOption = GValidationCvar.GetValueOnAnyThread();
	if (FParse::Param(FCommandLine::Get(), TEXT("vulkandebug")))
	{
		// Match D3D and GL
		GValidationCvar->Set(2, ECVF_SetByCommandline);
	}
	else if (FParse::Value(FCommandLine::Get(), TEXT("vulkanvalidation="), VulkanValidationOption))
	{
		GValidationCvar->Set(VulkanValidationOption, ECVF_SetByCommandline);
	}

	if (!bVkTrace && VulkanValidationOption > 0)
	{
		bool bStandardAvailable = false;
		if (GStandardValidationCvar.GetValueOnAnyThread() != 0)
		{
			bStandardAvailable = FindLayerInList(GlobalLayerExtensions, STANDARD_VALIDATION_LAYER_NAME);
			if (bStandardAvailable)
			{
				OutInstanceLayers.Add(STANDARD_VALIDATION_LAYER_NAME);
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to find Vulkan instance validation layer %s; trying individual layers..."), TEXT(STANDARD_VALIDATION_LAYER_NAME));
			}
		}

		if (!bStandardAvailable)
		{
			// Verify that all requested debugging device-layers are available
			for (uint32 LayerIndex = 0; GIndividualValidationLayers[LayerIndex] != nullptr; ++LayerIndex)
			{
				const ANSICHAR* CurrValidationLayer = GIndividualValidationLayers[LayerIndex];
				bool bValidationFound = FindLayerInList(GlobalLayerExtensions, CurrValidationLayer);
				if (bValidationFound)
				{
					OutInstanceLayers.Add(CurrValidationLayer);
				}
				else
				{
					UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to find Vulkan instance validation layer '%s'"), ANSI_TO_TCHAR(CurrValidationLayer));
				}
			}
		}
	}

#if VULKAN_SUPPORTS_DEBUG_UTILS
	if (!bVkTrace && GValidationCvar.GetValueOnAnyThread() > 0)
	{
		const char* FoundDebugUtilsLayer = nullptr;
		bOutDebugUtils = FindLayerExtensionInList(GlobalLayerExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME, FoundDebugUtilsLayer);
		if (bOutDebugUtils && *FoundDebugUtilsLayer)
		{
			OutInstanceLayers.Add(FoundDebugUtilsLayer);
		}
	}
#endif
#endif	// VULKAN_HAS_DEBUGGING_ENABLED

	// Check to see if the HMD requires any specific Vulkan extensions to operate
	if (IHeadMountedDisplayModule::IsAvailable())
	{
		HMDVulkanExtensions = IHeadMountedDisplayModule::Get().GetVulkanExtensions();
	
		if (HMDVulkanExtensions.IsValid())
		{
			if (!HMDVulkanExtensions->GetVulkanInstanceExtensionsRequired(OutInstanceExtensions))
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Trying to use Vulkan with an HMD, but required extensions aren't supported!"));
			}
		}
	}

	TArray<const ANSICHAR*> PlatformExtensions;
	FVulkanPlatform::GetInstanceExtensions(PlatformExtensions);

	for (const ANSICHAR* PlatformExtension : PlatformExtensions)
	{
		if (FindLayerExtensionInList(GlobalLayerExtensions, PlatformExtension))
		{
			OutInstanceExtensions.Add(PlatformExtension);
		}
	}

	for (int32 j = 0; GInstanceExtensions[j] != nullptr; j++)
	{
		if (FindLayerExtensionInList(GlobalLayerExtensions, GInstanceExtensions[j]))
		{
			OutInstanceExtensions.Add(GInstanceExtensions[j]);
		}
	}

#if VULKAN_SUPPORTS_DEBUG_UTILS
	if (!bVkTrace && bOutDebugUtils && FindLayerExtensionInList(GlobalLayerExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
	{
		OutInstanceExtensions.Add(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
#endif
#if VULKAN_HAS_DEBUGGING_ENABLED
	if (!bVkTrace && !bOutDebugUtils && GValidationCvar.GetValueOnAnyThread() > 0)
	{
		if (FindLayerExtensionInList(GlobalLayerExtensions, VK_EXT_DEBUG_REPORT_EXTENSION_NAME))
		{
			OutInstanceExtensions.Add(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}
	}
#endif

	TrimDuplicates(OutInstanceLayers);
	if (OutInstanceLayers.Num() > 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Using instance layers"));
		for (const ANSICHAR* Layer : OutInstanceLayers)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("* %s"), ANSI_TO_TCHAR(Layer));
		}
	}
	else
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Not using instance layers"));
	}

	TrimDuplicates(OutInstanceExtensions);
	if (OutInstanceExtensions.Num() > 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Using instance extensions"));
		for (const ANSICHAR* Extension : OutInstanceExtensions)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("* %s"), ANSI_TO_TCHAR(Extension));
		}
	}
	else
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Not using instance extensions"));
	}
}

void FVulkanDevice::GetDeviceExtensionsAndLayers(TArray<const ANSICHAR*>& OutDeviceExtensions, TArray<const ANSICHAR*>& OutDeviceLayers, bool& bOutDebugMarkers)
{
	bOutDebugMarkers = false;

	TArray<FLayerExtension> DeviceLayerExtensions;
	// 0 is reserved for regular device
	DeviceLayerExtensions.AddDefaulted();
	{
		uint32 Count = 0;
		TArray<VkLayerProperties> Properties;
		VERIFYVULKANRESULT(VulkanRHI::vkEnumerateDeviceLayerProperties(Gpu, &Count, nullptr));
		Properties.AddZeroed(Count);
		VERIFYVULKANRESULT(VulkanRHI::vkEnumerateDeviceLayerProperties(Gpu, &Count, Properties.GetData()));
		check(Count == Properties.Num());
		for (const VkLayerProperties& Property : Properties)
		{
			DeviceLayerExtensions[DeviceLayerExtensions.AddDefaulted()].LayerProps = Property;
		}
	}

	TArray<FString> FoundUniqueLayers;
	TArray<FString> FoundUniqueExtensions;

	for (int32 Index = 0; Index < DeviceLayerExtensions.Num(); ++Index)
	{
		if (Index == 0)
		{
			EnumerateDeviceExtensionProperties(Gpu, nullptr, DeviceLayerExtensions[Index]);
		}
		else
		{
			FoundUniqueLayers.AddUnique(ANSI_TO_TCHAR(DeviceLayerExtensions[Index].LayerProps.layerName));
			EnumerateDeviceExtensionProperties(Gpu, DeviceLayerExtensions[Index].LayerProps.layerName, DeviceLayerExtensions[Index]);
		}

		DeviceLayerExtensions[Index].AddUniqueExtensionNames(FoundUniqueExtensions);
	}

	FoundUniqueLayers.Sort();
	for (const FString& Name : FoundUniqueLayers)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("- Found device layer %s"), *Name);
	}

	FoundUniqueExtensions.Sort();
	for (const FString& Name : FoundUniqueExtensions)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("- Found device extension %s"), *Name);
	}

	FVulkanPlatform::NotifyFoundDeviceLayersAndExtensions(Gpu, FoundUniqueLayers, FoundUniqueExtensions);

	TArray<FString> UniqueUsedDeviceExtensions;
	auto AddDeviceLayers = [&](const char* LayerName)
	{
		int32 LayerIndex = FindLayerIndexInList(DeviceLayerExtensions, LayerName);
		if (LayerIndex != INDEX_NONE)
		{
			DeviceLayerExtensions[LayerIndex].AddUniqueExtensionNames(UniqueUsedDeviceExtensions);
		}
	};

#if VULKAN_HAS_DEBUGGING_ENABLED
	GRenderDocFound = false;
	#if VULKAN_ENABLE_DRAW_MARKERS
	{
		int32 LayerIndex = FindLayerIndexInList(DeviceLayerExtensions, RENDERDOC_LAYER_NAME);
		if (LayerIndex != INDEX_NONE)
		{
			GRenderDocFound = true;
			DeviceLayerExtensions[LayerIndex].AddUniqueExtensionNames(UniqueUsedDeviceExtensions);
		}
	}
	#endif

	// Verify that all requested debugging device-layers are available. Skip validation layers under RenderDoc
	if (!GRenderDocFound && GValidationCvar.GetValueOnAnyThread() > 0)
	{
		bool bStandardAvailable = false;
		if (GStandardValidationCvar.GetValueOnAnyThread() != 0)
		{
			bStandardAvailable = FindLayerInList(DeviceLayerExtensions, STANDARD_VALIDATION_LAYER_NAME);
			if (bStandardAvailable)
			{
				OutDeviceLayers.Add(STANDARD_VALIDATION_LAYER_NAME);
			}
		}

		if (!bStandardAvailable)
		{
			for (uint32 LayerIndex = 0; GIndividualValidationLayers[LayerIndex] != nullptr; ++LayerIndex)
			{
				bool bValidationFound = false;
				const ANSICHAR* CurrValidationLayer = GIndividualValidationLayers[LayerIndex];
				for (int32 Index = 1; Index < DeviceLayerExtensions.Num(); ++Index)
				{
					if (!FCStringAnsi::Strcmp(DeviceLayerExtensions[Index].LayerProps.layerName, CurrValidationLayer))
					{
						bValidationFound = true;
						OutDeviceLayers.Add(CurrValidationLayer);
						break;
					}
				}

				if (!bValidationFound)
				{
					UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to find Vulkan device validation layer '%s'"), ANSI_TO_TCHAR(CurrValidationLayer));
				}
			}
		}
	}
#endif	// VULKAN_HAS_DEBUGGING_ENABLED

	if (FVulkanDynamicRHI::HMDVulkanExtensions.IsValid())
	{
		if (!FVulkanDynamicRHI::HMDVulkanExtensions->GetVulkanDeviceExtensionsRequired( Gpu, OutDeviceExtensions))
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT( "Trying to use Vulkan with an HMD, but required extensions aren't supported on the selected device!"));
		}
	}

	// Now gather the actually used extensions based on the enabled layers
	TArray<const ANSICHAR*> AvailableExtensions;
	{
		// All global
		for (int32 ExtIndex = 0; ExtIndex < DeviceLayerExtensions[0].ExtensionProps.Num(); ++ExtIndex)
		{
			AvailableExtensions.Add(DeviceLayerExtensions[0].ExtensionProps[ExtIndex].extensionName);
		}

		// Now only find enabled layers
		for (int32 LayerIndex = 0; LayerIndex < OutDeviceLayers.Num(); ++LayerIndex)
		{
			// Skip 0 as it's the null layer
			int32 FindLayerIndex;
			for (FindLayerIndex = 1; FindLayerIndex < DeviceLayerExtensions.Num(); ++FindLayerIndex)
			{
				if (!FCStringAnsi::Strcmp(DeviceLayerExtensions[FindLayerIndex].LayerProps.layerName, OutDeviceLayers[LayerIndex]))
				{
					break;
				}
			}

			if (FindLayerIndex < DeviceLayerExtensions.Num())
			{
				DeviceLayerExtensions[FindLayerIndex].AddAnsiExtensionNames(AvailableExtensions);
			}
		}
	}
	TrimDuplicates(AvailableExtensions);

	auto ListContains = [](const TArray<const ANSICHAR*>& InList, const ANSICHAR* Name)
	{
		for (const ANSICHAR* Element : InList)
		{
			if (!FCStringAnsi::Strcmp(Element, Name))
			{
				return true;
			}
		}

		return false;
	};

	// Now go through the actual requested lists
	TArray<const ANSICHAR*> PlatformExtensions;
	FVulkanPlatform::GetDeviceExtensions(PlatformExtensions);
	for (const ANSICHAR* PlatformExtension : PlatformExtensions)
	{
		if (ListContains(AvailableExtensions, PlatformExtension))
		{
			OutDeviceExtensions.Add(PlatformExtension);
			break;
		}
	}

	for (uint32 Index = 0; Index < ARRAY_COUNT(GDeviceExtensions) && GDeviceExtensions[Index] != nullptr; ++Index)
	{
		if (ListContains(AvailableExtensions, GDeviceExtensions[Index]))
		{
			OutDeviceExtensions.Add(GDeviceExtensions[Index]);
		}
	}

#if VULKAN_ENABLE_DRAW_MARKERS && VULKAN_HAS_DEBUGGING_ENABLED
	if (!bOutDebugMarkers &&
		(((GRenderDocFound || GValidationCvar.GetValueOnAnyThread() == 0) && ListContains(AvailableExtensions, VK_EXT_DEBUG_MARKER_EXTENSION_NAME)) || FVulkanPlatform::ForceEnableDebugMarkers()))
	{
		// HACK: Lumin Nvidia driver unofficially supports this extension, but will return false if we try to load it explicitly.
#if !PLATFORM_LUMIN
		OutDeviceExtensions.Add(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
#endif
		bOutDebugMarkers = true;
	}
#endif

	if (OutDeviceExtensions.Num() > 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Using device extensions"));
		for (const ANSICHAR* Extension : OutDeviceExtensions)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("* %s"), ANSI_TO_TCHAR(Extension));
		}
	}

	if (OutDeviceLayers.Num() > 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Using device layers"));
		for (const ANSICHAR* Layer : OutDeviceLayers)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("* %s"), ANSI_TO_TCHAR(Layer));
		}
	}
}


void FVulkanDevice::ParseOptionalDeviceExtensions(const TArray<const ANSICHAR *>& DeviceExtensions)
{
	FMemory::Memzero(OptionalDeviceExtensions);

	auto HasExtension = [&DeviceExtensions](const ANSICHAR* InName) -> bool
	{
		return DeviceExtensions.ContainsByPredicate(
			[&InName](const ANSICHAR* InExtension) -> bool
			{
				return FCStringAnsi::Strcmp(InExtension, InName) == 0;
			}
		);
	};
#if VULKAN_SUPPORTS_MAINTENANCE_LAYER1
	OptionalDeviceExtensions.HasKHRMaintenance1 = HasExtension(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
#endif
#if VULKAN_SUPPORTS_MAINTENANCE_LAYER2
	OptionalDeviceExtensions.HasKHRMaintenance2 = HasExtension(VK_KHR_MAINTENANCE2_EXTENSION_NAME);
#endif
	//OptionalDeviceExtensions.HasMirrorClampToEdge = HasExtension(VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME);

#if VULKAN_SUPPORTS_DEDICATED_ALLOCATION
	OptionalDeviceExtensions.HasKHRDedicatedAllocation = HasExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) && HasExtension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
#endif

#if VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
	OptionalDeviceExtensions.HasKHRExternalMemoryCapabilities = HasExtension(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
	OptionalDeviceExtensions.HasKHRGetPhysicalDeviceProperties2 = HasExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif

#if VULKAN_SUPPORTS_VALIDATION_CACHE
	OptionalDeviceExtensions.HasEXTValidationCache = HasExtension(VK_EXT_VALIDATION_CACHE_EXTENSION_NAME);
#endif

	bool bHasAnyCrashExtension = false;
#if VULKAN_SUPPORTS_AMD_BUFFER_MARKER
	if (GGPUCrashDebuggingEnabled)
	{
		OptionalDeviceExtensions.HasAMDBufferMarker = HasExtension(VK_AMD_BUFFER_MARKER_EXTENSION_NAME);
		bHasAnyCrashExtension = bHasAnyCrashExtension || !OptionalDeviceExtensions.HasAMDBufferMarker;
	}
#endif

#if VULKAN_SUPPORTS_NV_DIAGNOSTIC_CHECKPOINT
	if (GGPUCrashDebuggingEnabled)
	{
		OptionalDeviceExtensions.HasNVDiagnosticCheckpoints = HasExtension(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);
		bHasAnyCrashExtension = bHasAnyCrashExtension || !OptionalDeviceExtensions.HasNVDiagnosticCheckpoints;
	}
#endif

	if (GGPUCrashDebuggingEnabled && !bHasAnyCrashExtension)
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("Tried to enable GPU crash debugging but no extension found!"));
	}

#if VULKAN_SUPPORTS_GOOGLE_DISPLAY_TIMING
	OptionalDeviceExtensions.HasGoogleDisplayTiming = HasExtension(VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME);
#endif

#if VULKAN_SUPPORTS_COLOR_CONVERSIONS
	OptionalDeviceExtensions.HasYcbcrSampler = HasExtension(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME) && HasExtension(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME) && HasExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
#endif
}
