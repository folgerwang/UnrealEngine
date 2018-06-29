// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanPipelineState.h: Vulkan pipeline state definitions.
=============================================================================*/

#pragma once

#include "VulkanConfiguration.h"
#include "VulkanMemory.h"
#include "VulkanGlobalUniformBuffer.h"

class FVulkanCommandBufferPool;

// Information for the layout of descriptor sets; does not hold runtime objects
class FVulkanDescriptorSetsLayoutInfo
{
public:
	FVulkanDescriptorSetsLayoutInfo()
	{
		FMemory::Memzero(LayoutTypes);
	}

	inline uint32 GetTypesUsed(VkDescriptorType Type) const
	{
		return LayoutTypes[Type];
	}

	struct FSetLayout
	{
		TArray<VkDescriptorSetLayoutBinding> LayoutBindings;
	};

	const TArray<FSetLayout>& GetLayouts() const
	{
		return SetLayouts;
	}

	void AddBindingsForStage(VkShaderStageFlagBits StageFlags, DescriptorSet::EStage DescSet, const FVulkanCodeHeader& CodeHeader);

	friend uint32 GetTypeHash(const FVulkanDescriptorSetsLayoutInfo& In)
	{
		return In.Hash;
	}

	inline bool operator == (const FVulkanDescriptorSetsLayoutInfo& In) const
	{
		if (In.SetLayouts.Num() != SetLayouts.Num())
		{
			return false;
		}

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
		if (In.TypesUsageID != TypesUsageID)
		{
			return false;
		}
#endif
		for (int32 Index = 0; Index < In.SetLayouts.Num(); ++Index)
		{
			int32 NumBindings = SetLayouts[Index].LayoutBindings.Num();
			if (In.SetLayouts[Index].LayoutBindings.Num() != NumBindings)
			{
				return false;
			}

			if (NumBindings != 0 && FMemory::Memcmp(&In.SetLayouts[Index].LayoutBindings[0], &SetLayouts[Index].LayoutBindings[0], NumBindings * sizeof(VkDescriptorSetLayoutBinding)))
			{
				return false;
			}
		}

		return true;
	}

	void CopyFrom(const FVulkanDescriptorSetsLayoutInfo& Info)
	{
		FMemory::Memcpy(LayoutTypes, Info.LayoutTypes, sizeof(LayoutTypes));
		Hash = Info.Hash;
#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
		TypesUsageID = Info.TypesUsageID;
#endif
		SetLayouts = Info.SetLayouts;
	}

	inline const uint32* GetLayoutTypes() const
	{
		return LayoutTypes;
	}

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	inline uint32 GetTypesUsageID() const
	{
		return TypesUsageID;
	}
#endif
protected:
	uint32 LayoutTypes[VK_DESCRIPTOR_TYPE_RANGE_SIZE];
	TArray<FSetLayout> SetLayouts;

	uint32 Hash = 0;

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	uint32 TypesUsageID = ~0;

	void CompileTypesUsageID();
#endif
	void AddDescriptor(int32 DescriptorSetIndex, const VkDescriptorSetLayoutBinding& Descriptor, int32 BindingIndex);

	friend class FVulkanPipelineStateCacheManager;
};

// The actual run-time descriptor set layouts
class FVulkanDescriptorSetsLayout : public FVulkanDescriptorSetsLayoutInfo
{
public:
	FVulkanDescriptorSetsLayout(FVulkanDevice* InDevice);
	~FVulkanDescriptorSetsLayout();

	// Can be called only once, the idea is that the Layout remains fixed.
	void Compile();

	inline const TArray<VkDescriptorSetLayout>& GetHandles() const
	{
		return LayoutHandles;
	}

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	inline const VkDescriptorSetAllocateInfo& GetAllocateInfo() const
	{
		return DescriptorSetAllocateInfo;
	}
#endif

	inline uint32 GetHash() const
	{
		return Hash;
	}

private:
	FVulkanDevice* Device;
	//uint32 Hash = 0;
	TArray<VkDescriptorSetLayout> LayoutHandles;
#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo;
#endif
};

class FVulkanDescriptorPool
{
public:
#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	FVulkanDescriptorPool(FVulkanDevice* InDevice, const FVulkanDescriptorSetsLayout& Layout);
#else
	FVulkanDescriptorPool(FVulkanDevice* InDevice);
#endif
	~FVulkanDescriptorPool();

	inline VkDescriptorPool GetHandle() const
	{
		return DescriptorPool;
	}

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	inline bool CanAllocate(const FVulkanDescriptorSetsLayout& InLayout) const
#else
	inline bool CanAllocate(const FVulkanDescriptorSetsLayout& Layout) const
#endif
	{
#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
		return MaxDescriptorSets > NumAllocatedDescriptorSets + InLayout.GetLayouts().Num();
#else
		for (uint32 TypeIndex = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; TypeIndex < VK_DESCRIPTOR_TYPE_END_RANGE; ++TypeIndex)
		{
			if (NumAllocatedTypes[TypeIndex] +	(int32)Layout.GetTypesUsed((VkDescriptorType)TypeIndex) > MaxAllocatedTypes[TypeIndex])
			{
				return false;
			}
		}
		return true;
#endif
	}

	void TrackAddUsage(const FVulkanDescriptorSetsLayout& Layout);
	void TrackRemoveUsage(const FVulkanDescriptorSetsLayout& Layout);

	inline bool IsEmpty() const
	{
		return NumAllocatedDescriptorSets == 0;
	}

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	void Reset();
	bool AllocateDescriptorSets(const VkDescriptorSetAllocateInfo& InDescriptorSetAllocateInfo, VkDescriptorSet* OutSets);
	inline uint32 GetNumAllocatedDescriptorSets() const
	{
		return NumAllocatedDescriptorSets;
	}
#endif

private:
	FVulkanDevice* Device;

	uint32 MaxDescriptorSets;
	uint32 NumAllocatedDescriptorSets;
	uint32 PeakAllocatedDescriptorSets;

	// Tracks number of allocated types, to ensure that we are not exceeding our allocated limit
#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	const FVulkanDescriptorSetsLayout& Layout;
#else
	int32 MaxAllocatedTypes[VK_DESCRIPTOR_TYPE_RANGE_SIZE];
	int32 NumAllocatedTypes[VK_DESCRIPTOR_TYPE_RANGE_SIZE];
	int32 PeakAllocatedTypes[VK_DESCRIPTOR_TYPE_RANGE_SIZE];
#endif
	VkDescriptorPool DescriptorPool;

	friend class FVulkanCommandListContext;
};


#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
class FVulkanTypedDescriptorPoolSet
{
	typedef TList<FVulkanDescriptorPool*> FPoolList;

	FVulkanDescriptorPool* GetFreePool(bool bForceNewPool = false);
	FVulkanDescriptorPool* PushNewPool();

protected:
	friend class FVulkanDescriptorPoolSetContainer;

	FVulkanTypedDescriptorPoolSet(FVulkanDevice* InDevice, class FVulkanDescriptorPoolSetContainer* InOwner, const FVulkanDescriptorSetsLayout& InLayout)
		: Device(InDevice)
		, Owner(InOwner)
		, Layout(InLayout)
	{
		PushNewPool();
	};

	~FVulkanTypedDescriptorPoolSet();

	void Reset();

public:
	bool AllocateDescriptorSets(const FVulkanDescriptorSetsLayout& Layout, VkDescriptorSet* OutSets);

	class FVulkanDescriptorPoolSetContainer* GetOwner() const
	{
		return Owner;
	}

private:
	FVulkanDevice* Device;
	class FVulkanDescriptorPoolSetContainer* Owner;
	const FVulkanDescriptorSetsLayout& Layout;

	FPoolList* PoolListHead = nullptr;
	FPoolList* PoolListCurrent = nullptr;
};

class FVulkanDescriptorPoolSetContainer
{
public:
	FVulkanDescriptorPoolSetContainer(FVulkanDevice* InDevice)
		: Device(InDevice)
		, LastFrameUsed(GFrameNumberRenderThread)
		, bUsed(true)
	{
	}

	~FVulkanDescriptorPoolSetContainer();

	FVulkanTypedDescriptorPoolSet* AcquireTypedPoolSet(const FVulkanDescriptorSetsLayout& Layout);

	void Reset();

	void SetUsed(bool bInUsed)
	{
		bUsed = bInUsed;
		LastFrameUsed = bUsed ? GFrameNumberRenderThread : LastFrameUsed;
	}

	bool IsUnused() const
	{
		return !bUsed;
	}

	uint32 GetLastFrameUsed() const
	{
		return LastFrameUsed;
	}

private:
	FVulkanDevice* Device;

	TMap<uint32, FVulkanTypedDescriptorPoolSet*> TypedDescriptorPools;

	uint32 LastFrameUsed;
	bool bUsed;
};

class FVulkanDescriptorPoolsManager
{
	class FVulkanAsyncPoolSetDeletionWorker : public FNonAbandonableTask
	{
		FVulkanDescriptorPoolSetContainer* PoolSet;

	public:
		FVulkanAsyncPoolSetDeletionWorker(FVulkanDescriptorPoolSetContainer* InPoolSet)
			: PoolSet(InPoolSet)
		{};

		void DoWork()
		{
			check(PoolSet != nullptr);

			delete PoolSet;

			PoolSet = nullptr;
		}

		void SetPoolSet(FVulkanDescriptorPoolSetContainer* InPoolSet)
		{
			check(PoolSet == nullptr);
			PoolSet = InPoolSet;
		}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FVulkanAsyncPoolSetDeletionWorker, STATGROUP_ThreadPoolAsyncTasks);
		}
	};

public:
	~FVulkanDescriptorPoolsManager();

	void Init(FVulkanDevice* InDevice)
	{
		Device = InDevice;
	}

	FVulkanDescriptorPoolSetContainer& AcquirePoolSetContainer();
	void ReleasePoolSet(FVulkanDescriptorPoolSetContainer& PoolSet);
	void GC();

private:
	FVulkanDevice* Device = nullptr;
	FAsyncTask<FVulkanAsyncPoolSetDeletionWorker>* AsyncDeletionTask = nullptr;

	FCriticalSection CS;
	TArray<FVulkanDescriptorPoolSetContainer*> PoolSets;
};
#endif

#if !VULKAN_USE_DESCRIPTOR_POOL_MANAGER
// The actual descriptor sets for a given pipeline
class FOLDVulkanDescriptorSets
{
public:
	~FOLDVulkanDescriptorSets();

	typedef TArray<VkDescriptorSet, TInlineAllocator<DescriptorSet::NumGfxStages>> FDescriptorSetArray;

	inline const FDescriptorSetArray& GetHandles() const
	{
		return Sets;
	}

	inline void Bind(VkCommandBuffer CmdBuffer, VkPipelineLayout PipelineLayout, VkPipelineBindPoint BindPoint)
	{
		VulkanRHI::vkCmdBindDescriptorSets(CmdBuffer,
			BindPoint,
			PipelineLayout,
			0, Sets.Num(), Sets.GetData(),
			0, nullptr);
	}

private:
	FOLDVulkanDescriptorSets(FVulkanDevice* InDevice, const FVulkanDescriptorSetsLayout& InLayout, FVulkanCommandListContext* InContext);

	FVulkanDevice* Device;
	FVulkanDescriptorPool* Pool;
	const FVulkanDescriptorSetsLayout& Layout;
	FDescriptorSetArray Sets;

	friend class FVulkanDescriptorPool;
	friend class FOLDVulkanDescriptorSetRingBuffer;
	friend class FVulkanCommandListContext;
};
#endif

// This container holds the actual VkWriteDescriptorSet structures; a Compute pipeline uses the arrays 'as-is', whereas a 
// Gfx PSO will have one big array and chunk it depending on the stage (eg Vertex, Pixel).
struct FVulkanDescriptorSetWriteContainer
{
	TArray<VkDescriptorImageInfo> DescriptorImageInfo;
	TArray<VkDescriptorBufferInfo> DescriptorBufferInfo;
	TArray<VkWriteDescriptorSet> DescriptorWrites;
	TArray<uint8> BindingToDynamicOffsetMap;
};

// Layout for a Pipeline, also includes DescriptorSets layout
class FVulkanLayout : public VulkanRHI::FDeviceChild
{
public:
	FVulkanLayout(FVulkanDevice* InDevice);
	virtual ~FVulkanLayout();

	inline const FVulkanDescriptorSetsLayout& GetDescriptorSetsLayout() const
	{
		return DescriptorSetLayout;
	}

	inline VkPipelineLayout GetPipelineLayout() const
	{
		return PipelineLayout;
	}

	inline bool HasDescriptors() const
	{
		return DescriptorSetLayout.GetLayouts().Num() > 0;
	}

	inline uint32 GetDescriptorSetLayoutHash() const
	{
		return DescriptorSetLayout.GetHash();
	}

protected:
	FVulkanDescriptorSetsLayout DescriptorSetLayout;
	VkPipelineLayout PipelineLayout;

	inline void AddBindingsForStage(VkShaderStageFlagBits StageFlags, DescriptorSet::EStage DescSet, const FVulkanCodeHeader& CodeHeader)
	{
		// Setting descriptor is only allowed prior to compiling the layout
		check(DescriptorSetLayout.GetHandles().Num() == 0);

		DescriptorSetLayout.AddBindingsForStage(StageFlags, DescSet, CodeHeader);
	}

	void Compile();

	friend class FVulkanComputePipeline;
	friend class FVulkanGfxPipeline;
	friend class FVulkanPipelineStateCacheManager;
};

#if !VULKAN_USE_DESCRIPTOR_POOL_MANAGER
// This class handles allocating/reusing descriptor sets per command list for a specific pipeline layout (each context holds one of this)
class FOLDVulkanDescriptorSetRingBuffer : public VulkanRHI::FDeviceChild
{
public:
	FOLDVulkanDescriptorSetRingBuffer(FVulkanDevice* InDevice);
	virtual ~FOLDVulkanDescriptorSetRingBuffer() {}

	void Reset()
	{
		CurrDescriptorSets = nullptr;
	}

	inline void Bind(VkCommandBuffer CmdBuffer, VkPipelineLayout Layout, VkPipelineBindPoint BindPoint)
	{
		check(CurrDescriptorSets);
		CurrDescriptorSets->Bind(CmdBuffer, Layout, VK_PIPELINE_BIND_POINT_COMPUTE);
	}

protected:
	FOLDVulkanDescriptorSets* CurrDescriptorSets;

	struct FDescriptorSetsPair
	{
		uint64 FenceCounter;
		FOLDVulkanDescriptorSets* DescriptorSets;

		FDescriptorSetsPair()
			: FenceCounter(0)
			, DescriptorSets(nullptr)
		{
		}

		~FDescriptorSetsPair();
	};

	struct FDescriptorSetsEntry
	{
		FVulkanCmdBuffer* CmdBuffer;
		TArray<FDescriptorSetsPair> Pairs;

		FDescriptorSetsEntry(FVulkanCmdBuffer* InCmdBuffer)
			: CmdBuffer(InCmdBuffer)
		{
		}
	};
	TArray<FDescriptorSetsEntry*> DescriptorSetsEntries;

	FOLDVulkanDescriptorSets* RequestDescriptorSets(FVulkanCommandListContext* Context, FVulkanCmdBuffer* CmdBuffer, const FVulkanLayout& Layout);

	friend class FVulkanComputePipelineDescriptorState;
	friend class FVulkanGraphicsPipelineDescriptorState;
};
#endif

// This class encapsulates updating VkWriteDescriptorSet structures (but doesn't own them), and their flags for dirty ranges; it is intended
// to be used to access a sub-region of a long array of VkWriteDescriptorSet (ie FVulkanDescriptorSetWriteContainer)
class FVulkanDescriptorSetWriter
{
public:
	FVulkanDescriptorSetWriter()
		: WriteDescriptors(nullptr)
		, BindingToDynamicOffsetMap(nullptr)
		, DynamicOffsets(nullptr)
		, NumWrites(0)
	{
	}

	bool WriteUniformBuffer(uint32 DescriptorIndex, VkBuffer Buffer, VkDeviceSize Offset, VkDeviceSize Range)
	{
		check(DescriptorIndex < NumWrites);
		check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		VkDescriptorBufferInfo* BufferInfo = const_cast<VkDescriptorBufferInfo*>(WriteDescriptors[DescriptorIndex].pBufferInfo);
		check(BufferInfo);
		bool bChanged = CopyAndReturnNotEqual(BufferInfo->buffer, Buffer);
		bChanged |= CopyAndReturnNotEqual(BufferInfo->offset, Offset);
		bChanged |= CopyAndReturnNotEqual(BufferInfo->range, Range);
		return bChanged;
	}

	bool WriteDynamicUniformBuffer(uint32 DescriptorIndex, VkBuffer Buffer, VkDeviceSize Offset, VkDeviceSize Range, uint32 DynamicOffset)
	{
		check(DescriptorIndex < NumWrites);
		check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
		VkDescriptorBufferInfo* BufferInfo = const_cast<VkDescriptorBufferInfo*>(WriteDescriptors[DescriptorIndex].pBufferInfo);
		check(BufferInfo);
		bool bChanged = CopyAndReturnNotEqual(BufferInfo->buffer, Buffer);
		bChanged |= CopyAndReturnNotEqual(BufferInfo->offset, Offset);
		bChanged |= CopyAndReturnNotEqual(BufferInfo->range, Range);
		const uint8 DynamicOffsetIndex = BindingToDynamicOffsetMap[DescriptorIndex];
		DynamicOffsets[DynamicOffsetIndex] = DynamicOffset;
		return bChanged;
	}

	bool WriteSampler(uint32 DescriptorIndex, VkSampler Sampler)
	{
		check(DescriptorIndex < NumWrites);
		check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER || WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		VkDescriptorImageInfo* ImageInfo = const_cast<VkDescriptorImageInfo*>(WriteDescriptors[DescriptorIndex].pImageInfo);
		check(ImageInfo);
		bool bChanged = CopyAndReturnNotEqual(ImageInfo->sampler, Sampler);
		return bChanged;
	}

	bool WriteImage(uint32 DescriptorIndex, VkImageView ImageView, VkImageLayout Layout)
	{
		check(DescriptorIndex < NumWrites);
		check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		VkDescriptorImageInfo* ImageInfo = const_cast<VkDescriptorImageInfo*>(WriteDescriptors[DescriptorIndex].pImageInfo);
		check(ImageInfo);
		bool bChanged = CopyAndReturnNotEqual(ImageInfo->imageView, ImageView);
		bChanged |= CopyAndReturnNotEqual(ImageInfo->imageLayout, Layout);
		return bChanged;
	}

	bool WriteStorageImage(uint32 DescriptorIndex, VkImageView ImageView, VkImageLayout Layout)
	{
		check(DescriptorIndex < NumWrites);
		check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		VkDescriptorImageInfo* ImageInfo = const_cast<VkDescriptorImageInfo*>(WriteDescriptors[DescriptorIndex].pImageInfo);
		check(ImageInfo);
		bool bChanged = CopyAndReturnNotEqual(ImageInfo->imageView, ImageView);
		bChanged |= CopyAndReturnNotEqual(ImageInfo->imageLayout, Layout);
		return bChanged;
	}

	bool WriteStorageTexelBuffer(uint32 DescriptorIndex, FVulkanBufferView* View)
	{
		check(DescriptorIndex < NumWrites);
		check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);
		WriteDescriptors[DescriptorIndex].pTexelBufferView = &View->View;
		BufferViewReferences[DescriptorIndex] = View;
		return true;
	}

	bool WriteStorageBuffer(uint32 DescriptorIndex, VkBuffer Buffer, VkDeviceSize Offset, VkDeviceSize Range)
	{
		check(DescriptorIndex < NumWrites);
		check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
		VkDescriptorBufferInfo* BufferInfo = const_cast<VkDescriptorBufferInfo*>(WriteDescriptors[DescriptorIndex].pBufferInfo);
		check(BufferInfo);
		bool bChanged = CopyAndReturnNotEqual(BufferInfo->buffer, Buffer);
		bChanged |= CopyAndReturnNotEqual(BufferInfo->offset, Offset);
		bChanged |= CopyAndReturnNotEqual(BufferInfo->range, Range);
		return bChanged;
	}

	bool WriteUniformTexelBuffer(uint32 DescriptorIndex, FVulkanBufferView* View)
	{
		check(DescriptorIndex < NumWrites);
		check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER);
		WriteDescriptors[DescriptorIndex].pTexelBufferView = &View->View;
		BufferViewReferences[DescriptorIndex] = View;
		return true;
	}

	void ClearBufferView(uint32 DescriptorIndex)
	{
		BufferViewReferences[DescriptorIndex] = nullptr;
	}

	void SetDescriptorSet(VkDescriptorSet DescriptorSet)
	{
		for (uint32 Index = 0; Index < NumWrites; ++Index)
		{
			WriteDescriptors[Index].dstSet = DescriptorSet;
		}
	}

protected:
	// A view into someone else's descriptors
	VkWriteDescriptorSet* WriteDescriptors;

	// A view into the mapping from binding index to dynamic uniform buffer offsets
	uint8* BindingToDynamicOffsetMap;

	// A view into someone else's dynamic uniform buffer offsets
	uint32* DynamicOffsets;

	uint32 NumWrites;
	TArray<TRefCountPtr<FVulkanBufferView>> BufferViewReferences;

	uint32 SetupDescriptorWrites(const FNEWVulkanShaderDescriptorInfo& Info, VkWriteDescriptorSet* InWriteDescriptors, VkDescriptorImageInfo* InImageInfo, 
		VkDescriptorBufferInfo* InBufferInfo, uint8* InBindingToDynamicOffsetMap);

	friend class FVulkanComputePipelineDescriptorState;
	friend class FVulkanGraphicsPipelineDescriptorState;
};
