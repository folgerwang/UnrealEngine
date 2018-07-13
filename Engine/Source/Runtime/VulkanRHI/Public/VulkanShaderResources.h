// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanResources.h: Vulkan resource RHI definitions.
=============================================================================*/

#pragma once

#include "BoundShaderStateCache.h"
#include "CrossCompilerCommon.h"

class FVulkanShaderSerializedBindings : public CrossCompiler::FShaderBindings
{
public:
	FVulkanShaderSerializedBindings()
	{
		InOutMask = 0;
		NumSamplers = 0;
		NumUniformBuffers = 0;
		NumUAVs = 0;
		bHasRegularUniformBuffers = 0;
	}
};

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderSerializedBindings& Bindings)
{
	Ar << Bindings.PackedUniformBuffers;
	Ar << Bindings.PackedGlobalArrays;
	Ar << Bindings.ShaderResourceTable.ResourceTableBits;
	Ar << Bindings.ShaderResourceTable.MaxBoundResourceTable;
	Ar << Bindings.ShaderResourceTable.TextureMap;
	Ar << Bindings.ShaderResourceTable.ShaderResourceViewMap;
	Ar << Bindings.ShaderResourceTable.SamplerMap;
	Ar << Bindings.ShaderResourceTable.UnorderedAccessViewMap;
	Ar << Bindings.ShaderResourceTable.ResourceTableLayoutHashes;

	Ar << Bindings.InOutMask;
	Ar << Bindings.NumSamplers;
	Ar << Bindings.NumUniformBuffers;
	Ar << Bindings.NumUAVs;

	return Ar;
}

class FNEWVulkanShaderDescriptorInfo
{
public:
	TArray<VkDescriptorType> DescriptorTypes;
	uint16 NumImageInfos;
	uint16 NumBufferInfos;
};

inline FArchive& operator<<(FArchive& Ar, FNEWVulkanShaderDescriptorInfo& Info)
{
	int32 NumDescriptorTypes = Info.DescriptorTypes.Num();
	Ar << NumDescriptorTypes;
	if (Ar.IsLoading())
	{
		Info.DescriptorTypes.Empty(NumDescriptorTypes);
		Info.DescriptorTypes.AddUninitialized(NumDescriptorTypes);
		for (int32 Index = 0; Index < NumDescriptorTypes; ++Index)
		{
			int32 Type;
			Ar << Type;
			Info.DescriptorTypes[Index] = (VkDescriptorType)Type;
		}
	}
	else
	{
		for (int32 Index = 0; Index < NumDescriptorTypes; ++Index)
		{
			int32 Type = (int32)Info.DescriptorTypes[Index];
			Ar << Type;
		}
	}
	Ar << Info.NumImageInfos;
	Ar << Info.NumBufferInfos;
	return Ar;
}


struct FVulkanCodeHeader
{
	FVulkanShaderSerializedBindings SerializedBindings;

	FNEWVulkanShaderDescriptorInfo NEWDescriptorInfo;

	struct FPackedUBToVulkanBindingIndex
	{
		CrossCompiler::EPackedTypeName	TypeName;
		uint8							VulkanBindingIndex;
	};
	TArray<FPackedUBToVulkanBindingIndex> NEWPackedUBToVulkanBindingIndices;

	// List of memory copies from RHIUniformBuffer to packed uniforms when emulating UB's
	TArray<CrossCompiler::FUniformBufferCopyInfo> UniformBuffersCopyInfo;

	FString ShaderName;
	FSHAHash SourceHash;

	uint64 UniformBuffersWithDescriptorMask;

	// Number of uniform buffers (not including PackedGlobalUBs) UNUSED
	uint32 UNUSED_NumNonGlobalUBs;

	// (Separated to improve cache) if this is non-zero, then we can assume all UBs are emulated
	TArray<uint32> NEWPackedGlobalUBSizes;

	// Number of copies per emulated buffer source index (to skip searching among UniformBuffersCopyInfo). Upper uint16 is the index, Lower uint16 is the count
	TArray<uint32> NEWEmulatedUBCopyRanges;
};

inline FArchive& operator<<(FArchive& Ar, FVulkanCodeHeader& Header)
{
	Ar << Header.SerializedBindings;
	Ar << Header.NEWDescriptorInfo;
	{
		int32 NumInfos = Header.NEWPackedUBToVulkanBindingIndices.Num();
		Ar << NumInfos;
		if (Ar.IsSaving())
		{
			for (int32 Index = 0; Index < NumInfos; ++Index)
			{
				Ar << Header.NEWPackedUBToVulkanBindingIndices[Index].TypeName;
				Ar << Header.NEWPackedUBToVulkanBindingIndices[Index].VulkanBindingIndex;
			}
		}
		else if (Ar.IsLoading())
		{
			Header.NEWPackedUBToVulkanBindingIndices.Empty(NumInfos);
			Header.NEWPackedUBToVulkanBindingIndices.AddUninitialized(NumInfos);
			for (int32 Index = 0; Index < NumInfos; ++Index)
			{
				Ar << Header.NEWPackedUBToVulkanBindingIndices[Index].TypeName;
				Ar << Header.NEWPackedUBToVulkanBindingIndices[Index].VulkanBindingIndex;
			}
		}
	}
	Ar << Header.UNUSED_NumNonGlobalUBs;
	Ar << Header.NEWPackedGlobalUBSizes;
	Ar << Header.NEWEmulatedUBCopyRanges;
	{
		int32 NumInfos = Header.UniformBuffersCopyInfo.Num();
		Ar << NumInfos;
		if (Ar.IsSaving())
		{
			for (int32 Index = 0; Index < NumInfos; ++Index)
			{
				Ar << Header.UniformBuffersCopyInfo[Index];
			}
		}
		else if (Ar.IsLoading())
		{
			Header.UniformBuffersCopyInfo.Empty(NumInfos);
			for (int32 Index = 0; Index < NumInfos; ++Index)
			{
				CrossCompiler::FUniformBufferCopyInfo Info;
				Ar << Info;
				Header.UniformBuffersCopyInfo.Add(Info);
			}
		}
	}
	Ar << Header.ShaderName;
	Ar << Header.UniformBuffersWithDescriptorMask;
	Ar << Header.SourceHash;
	return Ar;
}

static inline VkDescriptorType BindingToDescriptorType(EVulkanBindingType::EType Type)
{
	// Make sure these do NOT alias EPackedTypeName*
	switch (Type)
	{
	case EVulkanBindingType::PackedUniformBuffer:	return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case EVulkanBindingType::UniformBuffer:			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case EVulkanBindingType::CombinedImageSampler:	return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	case EVulkanBindingType::Sampler:				return VK_DESCRIPTOR_TYPE_SAMPLER;
	case EVulkanBindingType::Image:					return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	case EVulkanBindingType::UniformTexelBuffer:	return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
	case EVulkanBindingType::StorageImage:			return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	case EVulkanBindingType::StorageTexelBuffer:	return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
	case EVulkanBindingType::StorageBuffer:			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	default:
		check(0);
		break;
	}

	return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

static inline EVulkanBindingType::EType DescriptorTypeToBinding(VkDescriptorType Type, bool bUsePacked = false)
{
	switch (Type)
	{
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:			return bUsePacked ? EVulkanBindingType::PackedUniformBuffer : EVulkanBindingType::UniformBuffer;
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:	return EVulkanBindingType::CombinedImageSampler;
	case VK_DESCRIPTOR_TYPE_SAMPLER:				return EVulkanBindingType::Sampler;
	case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:			return EVulkanBindingType::Image;
	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:	return EVulkanBindingType::UniformTexelBuffer;
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:			return EVulkanBindingType::StorageImage;
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:	return EVulkanBindingType::StorageTexelBuffer;
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:			return EVulkanBindingType::StorageBuffer;
	default:
		check(0);
		break;
	}

	return EVulkanBindingType::Count;
}

