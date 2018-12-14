// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanRHIBridge.h: Utils to interact with the inner RHI.
=============================================================================*/

#pragma once 

class FVulkanDynamicRHI;
class FVulkanDevice;

namespace VulkanRHIBridge
{
	VULKANRHI_API uint64 GetInstance(FVulkanDynamicRHI*);

	VULKANRHI_API FVulkanDevice* GetDevice(FVulkanDynamicRHI*);

	// Returns a VkDevice
	VULKANRHI_API uint64 GetLogicalDevice(FVulkanDevice*);

	// Returns a VkDeviceVkPhysicalDevice
	VULKANRHI_API uint64 GetPhysicalDevice(FVulkanDevice*);
}
