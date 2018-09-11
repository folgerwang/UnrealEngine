// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "VulkanWindowsPlatform.h"
#include "../VulkanRHIPrivate.h"

#include "Windows/AllowWindowsPlatformTypes.h"
static HMODULE GVulkanDLLModule = nullptr;

static PFN_vkGetInstanceProcAddr GGetInstanceProcAddr = nullptr;

// Vulkan function pointers
#define DEFINE_VK_ENTRYPOINTS(Type,Func) VULKANRHI_API Type VulkanDynamicAPI::Func = NULL;
ENUM_VK_ENTRYPOINTS_ALL(DEFINE_VK_ENTRYPOINTS)

#define CHECK_VK_ENTRYPOINTS(Type,Func) if (VulkanDynamicAPI::Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(LogRHI, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }

#pragma warning(push)
#pragma warning(disable : 4191) // warning C4191: 'type cast': unsafe conversion
bool FVulkanWindowsPlatform::LoadVulkanLibrary()
{
	// Try to load the vulkan dll, as not everyone has the sdk installed
	GVulkanDLLModule = ::LoadLibraryW(TEXT("vulkan-1.dll"));

	if (GVulkanDLLModule)
	{
#define GET_VK_ENTRYPOINTS(Type,Func) VulkanDynamicAPI::Func = (Type)FPlatformProcess::GetDllExport(GVulkanDLLModule, L#Func);
		ENUM_VK_ENTRYPOINTS_BASE(GET_VK_ENTRYPOINTS);

		bool bFoundAllEntryPoints = true;
		ENUM_VK_ENTRYPOINTS_BASE(CHECK_VK_ENTRYPOINTS);
		if (!bFoundAllEntryPoints)
		{
			FreeVulkanLibrary();
			return false;
		}

		ENUM_VK_ENTRYPOINTS_OPTIONAL_BASE(GET_VK_ENTRYPOINTS);
#if UE_BUILD_DEBUG
		ENUM_VK_ENTRYPOINTS_OPTIONAL_BASE(CHECK_VK_ENTRYPOINTS);
#endif

		ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(GET_VK_ENTRYPOINTS);
		ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(CHECK_VK_ENTRYPOINTS);

#undef GET_VK_ENTRYPOINTS

		return true;
	}

	return false;
}

bool FVulkanWindowsPlatform::LoadVulkanInstanceFunctions(VkInstance inInstance)
{
	if (!GVulkanDLLModule)
	{
		return false;
	}

	GGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)FPlatformProcess::GetDllExport(GVulkanDLLModule, TEXT("vkGetInstanceProcAddr"));

	if (!GGetInstanceProcAddr)
	{
		return false;
	}

	bool bFoundAllEntryPoints = true;
#define CHECK_VK_ENTRYPOINTS(Type,Func) if (VulkanDynamicAPI::Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(LogRHI, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }

	// Initialize all of the entry points we have to query manually
#define GETINSTANCE_VK_ENTRYPOINTS(Type, Func) VulkanDynamicAPI::Func = (Type)VulkanDynamicAPI::vkGetInstanceProcAddr(inInstance, #Func);
	ENUM_VK_ENTRYPOINTS_INSTANCE(GETINSTANCE_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_INSTANCE(CHECK_VK_ENTRYPOINTS);
	if (!bFoundAllEntryPoints)
	{
		FreeVulkanLibrary();
		return false;
	}

	ENUM_VK_ENTRYPOINTS_OPTIONAL_INSTANCE(GETINSTANCE_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(GETINSTANCE_VK_ENTRYPOINTS);
#if UE_BUILD_DEBUG
	ENUM_VK_ENTRYPOINTS_OPTIONAL_INSTANCE(CHECK_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(CHECK_VK_ENTRYPOINTS);
#endif

	ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(GETINSTANCE_VK_ENTRYPOINTS);
	ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(CHECK_VK_ENTRYPOINTS);
#undef GET_VK_ENTRYPOINTS

	return true;
}
#pragma warning(pop) // restore 4191

void FVulkanWindowsPlatform::FreeVulkanLibrary()
{
	if (GVulkanDLLModule != nullptr)
	{
		::FreeLibrary(GVulkanDLLModule);
		GVulkanDLLModule = nullptr;
	}
}

#include "Windows/HideWindowsPlatformTypes.h"



void FVulkanWindowsPlatform::GetInstanceExtensions(TArray<const ANSICHAR*>& OutExtensions)
{
	// windows surface extension
	OutExtensions.Add(VK_KHR_SURFACE_EXTENSION_NAME);
	OutExtensions.Add(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
}


void FVulkanWindowsPlatform::GetDeviceExtensions(TArray<const ANSICHAR*>& OutExtensions)
{
#if VULKAN_SUPPORTS_DEDICATED_ALLOCATION
	OutExtensions.Add(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
	OutExtensions.Add(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
#endif
	if (GGPUCrashDebuggingEnabled)
	{
		OutExtensions.Add(VK_AMD_BUFFER_MARKER_EXTENSION_NAME);
	}
}

void FVulkanWindowsPlatform::CreateSurface(void* WindowHandle, VkInstance Instance, VkSurfaceKHR* OutSurface)
{
	VkWin32SurfaceCreateInfoKHR SurfaceCreateInfo;
	ZeroVulkanStruct(SurfaceCreateInfo, VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR);
	SurfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
	SurfaceCreateInfo.hwnd = (HWND)WindowHandle;
	VERIFYVULKANRESULT(VulkanDynamicAPI::vkCreateWin32SurfaceKHR(Instance, &SurfaceCreateInfo, VULKAN_CPU_ALLOCATOR, OutSurface));
}

bool FVulkanWindowsPlatform::SupportsDeviceLocalHostVisibleWithNoPenalty()
{
	static bool bIsWin10 = FWindowsPlatformMisc::VerifyWindowsVersion(10, 0) /*Win10*/;
	return (IsRHIDeviceAMD() && bIsWin10);
}


void FVulkanWindowsPlatform::WriteBufferMarkerAMD(VkCommandBuffer CmdBuffer, VkBuffer DestBuffer, const TArrayView<uint32>& Entries, bool bAdding)
{
	ensure(Entries.Num() <= GMaxCrashBufferEntries);
	// AMD API only allows updating one entry at a time. Assume buffer has entry 0 as num entries
	VulkanDynamicAPI::vkCmdWriteBufferMarkerAMD(CmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, DestBuffer, 0, Entries.Num());
	if (bAdding)
	{
		int32 LastIndex = Entries.Num() - 1;
		// +1 size as entries start at index 1
		VulkanDynamicAPI::vkCmdWriteBufferMarkerAMD(CmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, DestBuffer, (1 + LastIndex) * sizeof(uint32), Entries[LastIndex]);
	}
}
