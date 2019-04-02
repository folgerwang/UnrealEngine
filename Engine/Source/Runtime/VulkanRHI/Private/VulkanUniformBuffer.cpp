// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanUniformBuffer.cpp: Vulkan Constant buffer implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanContext.h"
#include "VulkanLLM.h"

enum
{
#if PLATFORM_DESKTOP
	PackedUniformsRingBufferSize = 16 * 1024 * 1024,
#else
	PackedUniformsRingBufferSize = 8 * 1024 * 1024,
#endif
};

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

FVulkanUniformBuffer::FVulkanUniformBuffer(const FRHIUniformBufferLayout& InLayout, const void* Contents, EUniformBufferUsage InUsage, EUniformBufferValidation Validation)
	: FRHIUniformBuffer(InLayout)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUniformBufferCreateTime);
#endif

	// Verify the correctness of our thought pattern how the resources are delivered
	//	- If we have at least one resource, we also expect ResourceOffset to have an offset
	//	- Meaning, there is always a uniform buffer with a size specified larged than 0 bytes
	check(InLayout.Resources.Num() > 0 || InLayout.ConstantBufferSize > 0);

	// Setup resource table
	const uint32 NumResources = InLayout.Resources.Num();
	if (NumResources > 0)
	{
		// Transfer the resource table to an internal resource-array
		ResourceTable.Empty(NumResources);
		ResourceTable.AddZeroed(NumResources);
		for (uint32 Index = 0; Index < NumResources; Index++)
		{
			FRHIResource* Resource = *(FRHIResource**)((uint8*)Contents + InLayout.Resources[Index].MemberOffset);

			// Allow null SRV's in uniform buffers for feature levels that don't support SRV's in shaders
			if (!(GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1 && InLayout.Resources[Index].MemberType == UBMT_SRV) && Validation == EUniformBufferValidation::ValidateResources)
			{
				checkf(Resource, TEXT("Invalid resource entry creating uniform buffer, %s.Resources[%u], ResourceType 0x%x."), *InLayout.GetDebugName().ToString(), Index, (uint8)InLayout.Resources[Index].MemberType);
			}
			ResourceTable[Index] = Resource;
		}
	}
}

void FVulkanUniformBuffer::UpdateResourceTable(const FRHIUniformBufferLayout& InLayout, const void* Contents, int32 ResourceNum)
{
	check(ResourceTable.Num() == ResourceNum);

	for (int32 ResourceIndex = 0; ResourceIndex < ResourceNum; ++ResourceIndex)
	{
		FRHIResource* Resource = *(FRHIResource**)((uint8*)Contents + InLayout.Resources[ResourceIndex].MemberOffset);

		checkf(Resource, TEXT("Invalid resource entry creating uniform buffer, %s.Resources[%u], ResourceType 0x%x."),
			*InLayout.GetDebugName().ToString(),
			ResourceIndex,
			(uint8)InLayout.Resources[ResourceIndex].MemberType);

		ResourceTable[ResourceIndex] = Resource;
	}
}

void FVulkanUniformBuffer::UpdateResourceTable(FRHIResource** Resources, int32 ResourceNum)
{
	check(ResourceTable.Num() == ResourceNum);

	for (int32 ResourceIndex = 0; ResourceIndex < ResourceNum; ++ResourceIndex)
	{
		ResourceTable[ResourceIndex] = Resources[ResourceIndex];
	}
}


FVulkanEmulatedUniformBuffer::FVulkanEmulatedUniformBuffer(const FRHIUniformBufferLayout& InLayout, const void* Contents, EUniformBufferUsage InUsage, EUniformBufferValidation Validation)
	: FVulkanUniformBuffer(InLayout, Contents, InUsage, Validation)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUniformBufferCreateTime);
#endif

	// Contents might be null but size > 0 as the data doesn't need a CPU copy
	if (InLayout.ConstantBufferSize)
	{
		// Create uniform buffer, which is stored on the CPU, the buffer is uploaded to a correct GPU buffer in UpdateDescriptorSets()
		ConstantData.AddUninitialized(InLayout.ConstantBufferSize);
		if (Contents)
		{
			FMemory::Memcpy(ConstantData.GetData(), Contents, InLayout.ConstantBufferSize);
		}
	}

	// Ancestor's constructor will set up the Resource table, so nothing else to do here
}

void FVulkanEmulatedUniformBuffer::UpdateConstantData(const void* Contents, int32 ContentsSize)
{
	checkSlow(ConstantData.Num() * sizeof(ConstantData[0]) == ContentsSize);
	if (ContentsSize > 0)
	{
		FMemory::Memcpy(ConstantData.GetData(), Contents, ContentsSize);
	}
}


FVulkanRealUniformBuffer::FVulkanRealUniformBuffer(FVulkanDevice& Device, const FRHIUniformBufferLayout& InLayout, const void* Contents, EUniformBufferUsage InUsage, EUniformBufferValidation Validation)
	: FVulkanUniformBuffer(InLayout, Contents, InUsage, Validation)
	, FVulkanResourceMultiBuffer(&Device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, InLayout.ConstantBufferSize, UniformBufferToBufferUsage(InUsage), GEmptyCreateInfo)
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

	// Ancestor's constructor will set up the Resource table, so nothing else to do here
}

void FVulkanRealUniformBuffer::Update(const void* Contents, int32 ContentsSize)
{
	if (ContentsSize > 0)
	{
		FVulkanCmdBuffer* Cmd = Device->GetImmediateContext().GetCommandBufferManager()->GetActiveCmdBuffer();

		const VkCommandBuffer CmdBuffer = Cmd->GetHandle();

		VulkanRHI::vkCmdUpdateBuffer(CmdBuffer, GetHandle(), GetOffset(), ContentsSize, Contents);
	}
}

FUniformBufferRHIRef FVulkanDynamicRHI::RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage, EUniformBufferValidation Validation)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanUniformBuffers);

	static TConsoleVariableData<int32>* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Vulkan.UseRealUBs"));
	const bool bHasRealUBs = FVulkanPlatform::UseRealUBsOptimization(CVar && CVar->GetValueOnAnyThread() > 0);
	if (bHasRealUBs)
	{
		return new FVulkanRealUniformBuffer(*Device, Layout, Contents, Usage, Validation);
	}
	else
	{
		// Parts of the buffer are later on copied for each shader stage into the packed uniform buffer
		return new FVulkanEmulatedUniformBuffer(Layout, Contents, Usage, Validation);
	}
}

template <bool bRealUBs>
inline void FVulkanDynamicRHI::UpdateUniformBuffer(FVulkanUniformBuffer* UniformBuffer, const void* Contents)
{
	const FRHIUniformBufferLayout& Layout = UniformBuffer->GetLayout();

	const int32 ConstantBufferSize = Layout.ConstantBufferSize;
	const int32 NumResources = Layout.Resources.Num();

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	FVulkanRealUniformBuffer* RealUniformBuffer = bRealUBs ? (FVulkanRealUniformBuffer*)UniformBuffer : nullptr;
	FVulkanEmulatedUniformBuffer* EmulatedUniformBuffer = bRealUBs ? nullptr : (FVulkanEmulatedUniformBuffer*)UniformBuffer;

	if (RHICmdList.Bypass())
	{
		if (bRealUBs)
		{
			RealUniformBuffer->Update(Contents, ConstantBufferSize);
		}
		else
		{
			EmulatedUniformBuffer->UpdateConstantData(Contents, ConstantBufferSize);
		}
		UniformBuffer->UpdateResourceTable(Layout, Contents, NumResources);
	}
	else
	{
		FRHIResource** CmdListResources = nullptr;
		void* CmdListConstantBufferData = nullptr;

		if (NumResources > 0)
		{
			CmdListResources = (FRHIResource**)RHICmdList.Alloc(sizeof(FRHIResource*) * NumResources, alignof(FRHIResource*));

			for (int32 ResourceIndex = 0; ResourceIndex < NumResources; ++ResourceIndex)
			{
				FRHIResource* Resource = *(FRHIResource**)((uint8*)Contents + Layout.Resources[ResourceIndex].MemberOffset);

				checkf(Resource, TEXT("Invalid resource entry creating uniform buffer, %s.Resources[%u], ResourceType 0x%x."),
					*Layout.GetDebugName().ToString(),
					ResourceIndex,
					(uint8)Layout.Resources[ResourceIndex].MemberType);

				CmdListResources[ResourceIndex] = Resource;
			}
		}

		if (ConstantBufferSize > 0)
		{
			// Can be optimized creating a new Vulkan buffer here instead of extra memcpy, but would require refactoring entire Vulkan uniform buffer code.
			CmdListConstantBufferData = (void*)RHICmdList.Alloc(ConstantBufferSize, 256);
			FMemory::Memcpy(CmdListConstantBufferData, Contents, ConstantBufferSize);
		}

		RHICmdList.EnqueueLambda([RealUniformBuffer, EmulatedUniformBuffer, CmdListResources, NumResources, CmdListConstantBufferData, ConstantBufferSize](FRHICommandList&)
		{
			if (bRealUBs)
			{
				RealUniformBuffer->Update(CmdListConstantBufferData, ConstantBufferSize);
				RealUniformBuffer->UpdateResourceTable(CmdListResources, NumResources);
			}
			else
			{
				EmulatedUniformBuffer->UpdateConstantData(CmdListConstantBufferData, ConstantBufferSize);
				EmulatedUniformBuffer->UpdateResourceTable(CmdListResources, NumResources);
			}
		});
		RHICmdList.RHIThreadFence(true);
	}
}


void FVulkanDynamicRHI::RHIUpdateUniformBuffer(FUniformBufferRHIParamRef UniformBufferRHI, const void* Contents)
{
	static TConsoleVariableData<int32>* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Vulkan.UseRealUBs"));
	const bool bHasRealUBs = FVulkanPlatform::UseRealUBsOptimization(CVar && CVar->GetValueOnAnyThread() > 0);
	FVulkanUniformBuffer* UniformBuffer = ResourceCast(UniformBufferRHI);
	if (bHasRealUBs)
	{
		UpdateUniformBuffer<true>(UniformBuffer, Contents);
	}
	else
	{
		UpdateUniformBuffer<false>(UniformBuffer, Contents);
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
