// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanUniformBuffer.cpp: Vulkan Constant buffer implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanLLM.h"

enum
{
#if PLATFORM_DESKTOP
	PackedUniformsRingBufferSize = 16 * 1024 * 1024,
#else
	PackedUniformsRingBufferSize = 8 * 1024 * 1024,
#endif
};

#if 0
const int NUM_SAFE_FRAMES = 5;
static TArray<TRefCountPtr<FVulkanBuffer>> GUBPool[NUM_SAFE_FRAMES];

static TRefCountPtr<FVulkanBuffer> AllocateBufferFromPool(FVulkanDevice& Device, uint32 ConstantBufferSize, EUniformBufferUsage Usage)
{
	if (Usage == UniformBuffer_MultiFrame)
	{
		return new FVulkanBuffer(Device, ConstantBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false, __FILE__, __LINE__);
	}

	int32 BufferIndex = GFrameNumberRenderThread % NUM_SAFE_FRAMES;

	GUBPool[BufferIndex].Add(new FVulkanBuffer(Device, ConstantBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, false, __FILE__, __LINE__));
	return GUBPool[BufferIndex].Last();
}

void CleanupUniformBufferPool()
{
	int32 BufferIndex = (GFrameNumberRenderThread  + 1) % NUM_SAFE_FRAMES;
	GUBPool[BufferIndex].Reset(0);
}
#endif

/*-----------------------------------------------------------------------------
	Uniform buffer RHI object
-----------------------------------------------------------------------------*/

static FRHIResourceCreateInfo GEmptyCreateInfo;

static inline EBufferUsageFlags UniformBufferToBufferUsage(EUniformBufferUsage Usage)
{
	switch (Usage)
	{
	default:
		ensure(0);
		// fall through...
	case UniformBuffer_SingleDraw:
		return BUF_Volatile;
	case UniformBuffer_SingleFrame:
		return BUF_Volatile;
	case UniformBuffer_MultiFrame:
		return BUF_Static;
	}
}

FVulkanUniformBuffer::FVulkanUniformBuffer(const FRHIUniformBufferLayout& InLayout, const void* Contents, EUniformBufferUsage Usage, bool bCopyIntoConstantData)
	: FRHIUniformBuffer(InLayout)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUniformBufferCreateTime);
#endif

	// Verify the correctness of our thought pattern how the resources are delivered
	//	- If we have at least one resource, we also expect ResourceOffset to have an offset
	//	- Meaning, there is always a uniform buffer with a size specified larged than 0 bytes
	check(InLayout.Resources.Num() > 0 || InLayout.ConstantBufferSize > 0);

	// Contents might be null but size > 0 as the data doesn't need a CPU copy (it lives inside the 'real' VkBuffer)
	if (bCopyIntoConstantData && InLayout.ConstantBufferSize)
	{
		// Create uniform buffer, which is stored on the CPU, the buffer is uploaded to a correct GPU buffer in UpdateDescriptorSets()
		ConstantData.AddUninitialized(InLayout.ConstantBufferSize);
		if (Contents)
		{
			FMemory::Memcpy(ConstantData.GetData(), Contents, InLayout.ConstantBufferSize);
		}
	}

	// Parse Sampler and Texture resources, if necessary
	const uint32 NumResources = InLayout.Resources.Num();
	if (NumResources > 0)
	{
		// Transfer the resource table to an internal resource-array
		ResourceTable.Empty(NumResources);
		ResourceTable.AddZeroed(NumResources);
		for (uint32 Index = 0; Index < NumResources; Index++)
		{
			FRHIResource* Resource = *(FRHIResource**)((uint8*)Contents + InLayout.ResourceOffsets[Index]);

			// Allow null SRV's in uniform buffers for feature levels that don't support SRV's in shaders
			if (!(GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1 && InLayout.Resources[Index] == UBMT_SRV))
			{
				checkf(Resource, TEXT("Invalid resource entry creating uniform buffer, %s.Resources[%u], ResourceType 0x%x."), *InLayout.GetDebugName().ToString(), Index, InLayout.Resources[Index]);
			}
			ResourceTable[Index] = Resource;
		}
	}
}

FVulkanRealUniformBuffer::FVulkanRealUniformBuffer(FVulkanDevice& Device, const FRHIUniformBufferLayout& InLayout, const void* Contents, EUniformBufferUsage Usage)
	: FVulkanUniformBuffer(InLayout, Contents, Usage, false)
	, FVulkanResourceMultiBuffer(&Device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, InLayout.ConstantBufferSize, UniformBufferToBufferUsage(Usage), GEmptyCreateInfo)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUniformBufferCreateTime);
#endif

	if (InLayout.ConstantBufferSize)
	{
		//#todo-rco: Optimize
		const bool bRT = IsInRenderingThread();
		void* Data = Lock(bRT, RLM_WriteOnly, InLayout.ConstantBufferSize, 0);
		FMemory::Memcpy(Data, Contents, InLayout.ConstantBufferSize);
		Unlock(bRT);
	}
}

FUniformBufferRHIRef FVulkanDynamicRHI::RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanUniformBuffers);

	static TConsoleVariableData<int32>* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Vulkan.UseRealUBs"));
	const bool bHasRealUBs = FVulkanPlatform::UseRealUBsOptimization(CVar && CVar->GetValueOnAnyThread() > 0);
	if (bHasRealUBs)
	{
		return new FVulkanRealUniformBuffer(*Device, Layout, Contents, Usage);
	}
	else
	{
		// Parts of the buffer are later on copied for each shader stage into the packed uniform buffer
		return new FVulkanUniformBuffer(Layout, Contents, Usage, true);
	}
}


FVulkanUniformBufferUploader::FVulkanUniformBufferUploader(FVulkanDevice* InDevice)
	: VulkanRHI::FDeviceChild(InDevice)
	, CPUBuffer(nullptr)
{
	if (Device->HasUnifiedMemory())
	{
		CPUBuffer = new FVulkanRingBuffer(InDevice, PackedUniformsRingBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}
	else
	{
		if (FVulkanPlatform::SupportsDeviceLocalHostVisibleWithNoPenalty() &&
			InDevice->GetMemoryManager().SupportsMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
		{
			CPUBuffer = new FVulkanRingBuffer(InDevice, PackedUniformsRingBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		}
		else
		{
			CPUBuffer = new FVulkanRingBuffer(InDevice, PackedUniformsRingBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		}
	}
}

FVulkanUniformBufferUploader::~FVulkanUniformBufferUploader()
{
	delete CPUBuffer;
}
