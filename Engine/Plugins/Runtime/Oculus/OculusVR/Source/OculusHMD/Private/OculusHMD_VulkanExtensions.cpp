// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_VulkanExtensions.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMDPrivateRHI.h"

namespace OculusHMD
{


//-------------------------------------------------------------------------------------------------
// FVulkanExtensions
//-------------------------------------------------------------------------------------------------

bool FVulkanExtensions::GetVulkanInstanceExtensionsRequired(TArray<const ANSICHAR*>& Out)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS_VULKAN
	TArray<VkExtensionProperties> Properties;
	{
		uint32_t PropertyCount;
		VulkanRHI::vkEnumerateInstanceExtensionProperties(nullptr, &PropertyCount, nullptr);
		Properties.SetNum(PropertyCount);
		VulkanRHI::vkEnumerateInstanceExtensionProperties(nullptr, &PropertyCount, Properties.GetData());
	}

	TArray<const char*> Extensions;
	{
		int32 ExtensionCount = 0;
		ovrp_GetInstanceExtensionsVk(nullptr, &ExtensionCount);
		Extensions.SetNum(ExtensionCount);
		ovrp_GetInstanceExtensionsVk(Extensions.GetData(), &ExtensionCount);
	}

	int32 ExtensionsFound = 0;
	for (int32 ExtensionIndex = 0; ExtensionIndex < Extensions.Num(); ExtensionIndex++)
	{
		for (int32 PropertyIndex = 0; PropertyIndex < Properties.Num(); PropertyIndex++)
		{
			const char* PropertyExtensionName = Properties[PropertyIndex].extensionName;

			if (!FCStringAnsi::Strcmp(PropertyExtensionName, Extensions[ExtensionIndex]))
			{
				Out.Add(Extensions[ExtensionIndex]);
				ExtensionsFound++;
				break;
			}
		}
	}

	return ExtensionsFound == Extensions.Num();
#endif
	return true;
}


bool FVulkanExtensions::GetVulkanDeviceExtensionsRequired(struct VkPhysicalDevice_T *pPhysicalDevice, TArray<const ANSICHAR*>& Out)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS_VULKAN
	TArray<VkExtensionProperties> Properties;
	{
		uint32_t PropertyCount;
		VulkanRHI::vkEnumerateDeviceExtensionProperties((VkPhysicalDevice) pPhysicalDevice, nullptr, &PropertyCount, nullptr);
		Properties.SetNum(PropertyCount);
		VulkanRHI::vkEnumerateDeviceExtensionProperties((VkPhysicalDevice) pPhysicalDevice, nullptr, &PropertyCount, Properties.GetData());
	}

	TArray<const char*> Extensions;
	{
		int32 ExtensionCount = 0;
		ovrp_GetDeviceExtensionsVk(nullptr, &ExtensionCount);
		Extensions.SetNum(ExtensionCount);
		ovrp_GetDeviceExtensionsVk(Extensions.GetData(), &ExtensionCount);
	}

	int32 ExtensionsFound = 0;
	for (int32 ExtensionIndex = 0; ExtensionIndex < Extensions.Num(); ExtensionIndex++)
	{
		for (int32 PropertyIndex = 0; PropertyIndex < Properties.Num(); PropertyIndex++)
		{
			const char* PropertyExtensionName = Properties[PropertyIndex].extensionName;

			if (!FCStringAnsi::Strcmp(PropertyExtensionName, Extensions[ExtensionIndex]))
			{
				Out.Add(Extensions[ExtensionIndex]);
				ExtensionsFound++;
				break;
			}
		}
	}

	return ExtensionsFound == Extensions.Num();
#endif
	return true;
}


} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS