// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanGlobals.h: Global Vulkan RHI definitions.
=============================================================================*/

#pragma once 

DECLARE_LOG_CATEGORY_EXTERN(LogVulkan, Display, All);

template< class T >
static FORCEINLINE void ZeroVulkanStruct(T& Struct, VkStructureType Type)
{
	static_assert(!TIsPointer<T>::Value, "Don't use a pointer!");
	static_assert(STRUCT_OFFSET(T, sType) == 0, "Assumes sType is the first member in the Vulkan type!");
	Struct.sType = Type;
	FMemory::Memzero(((uint8*)&Struct) + sizeof(VkStructureType), sizeof(T) - sizeof(VkStructureType));
}
