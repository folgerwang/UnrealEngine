// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanGlobalUniformBuffer.h: Vulkan Global uniform definitions.
=============================================================================*/

#pragma once

#include "VulkanResources.h"

class FGlobalUniformBufferManager
{
public:

protected:
	struct FBufferEntry
	{
		VulkanRHI::FBufferAllocation* BufferAlloc = nullptr;
		FVulkanCmdBuffer* StartCmdBuffer = nullptr;
		uint64 StartFence = 0;
	};

	struct
	{
		FBufferEntry Entry;
		TArray<uint32> Offsets;
		TArray<uint32> Sizes;
	} CurrentGfx;

	TArray<FBufferEntry> UsedEntries;
	TArray<VulkanRHI::FBufferAllocation*> FreeEntries;
};
