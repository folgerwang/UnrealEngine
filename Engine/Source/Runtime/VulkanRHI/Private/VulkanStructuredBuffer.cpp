// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "VulkanRHIPrivate.h"
#include "Containers/ResourceArray.h"

FVulkanStructuredBuffer::FVulkanStructuredBuffer(FVulkanDevice* InDevice, uint32 InStride, uint32 InSize, FRHIResourceCreateInfo& CreateInfo, uint32 InUsage)
	: FRHIStructuredBuffer(InStride, InSize, InUsage)
	, FVulkanResourceMultiBuffer(InDevice, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, InSize, InUsage, CreateInfo)
{
}

FVulkanStructuredBuffer::~FVulkanStructuredBuffer()
{
}


FStructuredBufferRHIRef FVulkanDynamicRHI::RHICreateStructuredBuffer(uint32 InStride, uint32 InSize, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	return new FVulkanStructuredBuffer(Device, InStride, InSize, CreateInfo, InUsage);
}

void* FVulkanDynamicRHI::RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI,uint32 Offset,uint32 Size,EResourceLockMode LockMode)
{
	FVulkanStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	return StructuredBuffer->Lock(false, LockMode, Size, Offset);
}

void FVulkanDynamicRHI::RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	FVulkanStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	StructuredBuffer->Unlock(false);
}
