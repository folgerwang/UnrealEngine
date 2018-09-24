// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanPipelineState.h: Vulkan pipeline state definitions.
=============================================================================*/

#pragma once

#include "VulkanConfiguration.h"
#include "VulkanMemory.h"
#include "VulkanGlobalUniformBuffer.h"
#include "VulkanShaderResources.h"

class FVulkanCommandBufferPool;

struct FUniformBufferGatherInfo
{
	FUniformBufferGatherInfo()
	{
		FMemory::Memzero(CodeHeaders);
	}

	// These maps are used to find UBs that are used on multiple stages
	TMap<uint32, VkShaderStageFlags>	UBLayoutsToUsedStageMap;
	TMap<uint32, VkShaderStageFlags>	CommonUBLayoutsToStageMap;

	const FVulkanShaderHeader*	CodeHeaders[ShaderStage::NumStages];
};

struct FInputAttachmentData
{
	uint16	BindingIndex = UINT16_MAX;
	uint8	DescriptorSet = UINT8_MAX;
	FVulkanShaderHeader::EAttachmentType Type = FVulkanShaderHeader::EAttachmentType::Count;
};

// Information for remapping descriptor sets when combining layouts
struct FDescriptorSetRemappingInfo
{
	struct FRemappingInfo
	{
		uint16	NewDescriptorSet = UINT16_MAX;
		uint16	NewBindingIndex = UINT16_MAX;
	};
	struct FUBRemappingInfo
	{
		// Remapping is only valid if there is constant data
		FRemappingInfo			Remapping;

		bool					bHasConstantData = false;
		//bool					bIsRedundant = false;
	};

	struct FSetInfo
	{
		TArray<VkDescriptorType>	Types;
		uint16						NumImageInfos = 0;
		uint16						NumBufferInfos = 0;
	};
	TArray<FSetInfo>	SetInfos;

	struct FStageInfo
	{
		TArray<FRemappingInfo>		Globals;
		TArray<FUBRemappingInfo>	UniformBuffers;
		TArray<uint16>				PackedUBBindingIndices;
		uint16						PackedUBDescriptorSet = UINT16_MAX;
		uint16						Pad0 = 0;

		inline bool IsEmpty() const
		{
			if (Globals.Num() != 0)
			{
				return false;
			}

			if (UniformBuffers.Num() != 0)
			{
				return false;
			}

			if (PackedUBBindingIndices.Num() != 0)
			{
				return false;
			}

			return true;
		}
	};
	TStaticArray<FStageInfo, ShaderStage::NumStages>	StageInfos;

	TArray<FInputAttachmentData>						InputAttachmentData;

	inline bool IsEmpty() const
	{
		if (SetInfos.Num() == 0)
		{
			for (int32 Index = 0; Index < ShaderStage::NumStages; ++Index)
			{
				if (!StageInfos[Index].IsEmpty())
				{
					return false;
				}
			}

			return true;
		}

		return false;
	}

	uint32 AddGlobal(uint32 Stage, int32 GlobalIndex, uint32 NewDescriptorSet, VkDescriptorType InType, uint16 CombinedSamplerStateAlias)
	{
		// Combined Image Samplers point both the texture and the sampler to the same descriptor
		check(CombinedSamplerStateAlias == UINT16_MAX || InType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

		uint32 NewBindingIndex = CombinedSamplerStateAlias == UINT16_MAX ? SetInfos[NewDescriptorSet].Types.Add(InType) : StageInfos[Stage].Globals[CombinedSamplerStateAlias].NewBindingIndex;

		int32 RemappingIndex = StageInfos[Stage].Globals.AddDefaulted();
		check(RemappingIndex == GlobalIndex);
		FDescriptorSetRemappingInfo::FRemappingInfo& Remapping = StageInfos[Stage].Globals[RemappingIndex];
		Remapping.NewDescriptorSet = NewDescriptorSet;
		Remapping.NewBindingIndex = NewBindingIndex;

		return NewBindingIndex;
	}

	uint32 AddPackedUB(uint32 Stage, int32 PackUBIndex, uint32 NewDescriptorSet, VkDescriptorType InType)
	{
		uint32 NewBindingIndex = SetInfos[NewDescriptorSet].Types.Add(InType);
		if (StageInfos[Stage].PackedUBDescriptorSet == UINT16_MAX)
		{
			StageInfos[Stage].PackedUBDescriptorSet = NewDescriptorSet;
		}
		else
		{
			ensure(StageInfos[Stage].PackedUBDescriptorSet == NewDescriptorSet);
		}
		int32 RemappingIndex = StageInfos[Stage].PackedUBBindingIndices.Add(NewBindingIndex);
		check(RemappingIndex == PackUBIndex);

		return NewBindingIndex;
	}

	FDescriptorSetRemappingInfo::FUBRemappingInfo AddUBWithData(uint32 Stage, int32 UniformBufferIndex, uint32 NewDescriptorSet, VkDescriptorType InType, uint32& OutNewBindingIndex)
	{
		OutNewBindingIndex = SetInfos[NewDescriptorSet].Types.Add(InType);

		int32 UBRemappingIndex = StageInfos[Stage].UniformBuffers.AddDefaulted();
		check(UBRemappingIndex == UniformBufferIndex);
		FDescriptorSetRemappingInfo::FUBRemappingInfo& UBRemapping = StageInfos[Stage].UniformBuffers[UBRemappingIndex];
		UBRemapping.bHasConstantData = true;
		UBRemapping.Remapping.NewDescriptorSet = NewDescriptorSet;
		UBRemapping.Remapping.NewBindingIndex = OutNewBindingIndex;

		return UBRemapping;
	}

	void AddRedundantUB(uint32 Stage, int32 UniformBufferIndex, const FDescriptorSetRemappingInfo::FUBRemappingInfo* InExistingUBInfo)
	{
		int32 UBRemappingIndex = StageInfos[Stage].UniformBuffers.AddDefaulted();
		check(UBRemappingIndex == UniformBufferIndex);
		FDescriptorSetRemappingInfo::FUBRemappingInfo& UBRemapping = StageInfos[Stage].UniformBuffers[UBRemappingIndex];
		//UBRemapping.bIsRedundant = true;
		UBRemapping.bHasConstantData = InExistingUBInfo->bHasConstantData;
/*
		UBRemapping.EntriesRemappingInfo = InExistingUBInfo->EntriesRemappingInfo;
*/
		UBRemapping.Remapping = InExistingUBInfo->Remapping;
	}

	void AddUBResourceOnly(uint32 Stage, int32 UniformBufferIndex)
	{
		int32 UBRemappingIndex = StageInfos[Stage].UniformBuffers.AddDefaulted();
		check(UBRemappingIndex == UniformBufferIndex);
		FDescriptorSetRemappingInfo::FUBRemappingInfo& UBRemapping = StageInfos[Stage].UniformBuffers[UBRemappingIndex];
		UBRemapping.bHasConstantData = false;
	}

/*
	uint32 AddUBResourceEntryToSetAndRemapping(int32 Stage, int32 UniformBufferIndex, int32 ResourceEntryIndex, uint32 NewDescriptorSet, VkDescriptorType InType, uint8 CombinedSamplerStateAlias)
	{
		// Combined Image Samplers point both the texture and the sampler to the same descriptor
		check(CombinedSamplerStateAlias == UINT8_MAX || InType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		uint32 NewBindingIndex = CombinedSamplerStateAlias == UINT8_MAX ? SetInfos[NewDescriptorSet].Types.Add(InType) : StageInfos[Stage].UniformBuffers[UniformBufferIndex].EntriesRemappingInfo[CombinedSamplerStateAlias].NewBindingIndex;

		int32 UBEntryRemappingIndex = StageInfos[Stage].UniformBuffers[UniformBufferIndex].EntriesRemappingInfo.AddDefaulted();
		check(UBEntryRemappingIndex == ResourceEntryIndex);
		FDescriptorSetRemappingInfo::FRemappingInfo& UBEntryRemapping = StageInfos[Stage].UniformBuffers[UniformBufferIndex].EntriesRemappingInfo[UBEntryRemappingIndex];
		UBEntryRemapping.NewDescriptorSet = NewDescriptorSet;
		UBEntryRemapping.NewBindingIndex = NewBindingIndex;

		return NewBindingIndex;
	}
*/
};

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

	void ProcessBindingsForStage(VkShaderStageFlagBits StageFlags, ShaderStage::EStage DescSetStage, const FVulkanShaderHeader& CodeHeader, FUniformBufferGatherInfo& OutUBGatherInfo) const;

	template<bool bIsCompute>
	void FinalizeBindings(const FUniformBufferGatherInfo& UBGatherInfo, const TArrayView<const FSamplerStateRHIParamRef>& ImmutableSamplers);

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

		if (In.TypesUsageID != TypesUsageID)
		{
			return false;
		}

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
		TypesUsageID = Info.TypesUsageID;
		SetLayouts = Info.SetLayouts;
		RemappingInfo = Info.RemappingInfo;
	}

	inline const uint32* GetLayoutTypes() const
	{
		return LayoutTypes;
	}

	inline uint32 GetTypesUsageID() const
	{
		return TypesUsageID;
	}

	inline bool HasInputAttachments() const
	{
		return LayoutTypes[VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT] > 0;
	}

protected:
	uint32 LayoutTypes[VK_DESCRIPTOR_TYPE_RANGE_SIZE];
	TArray<FSetLayout> SetLayouts;

	uint32 Hash = 0;

	uint32 TypesUsageID = ~0;

	void CompileTypesUsageID();

	void AddDescriptor(int32 DescriptorSetIndex, const VkDescriptorSetLayoutBinding& Descriptor);

	FDescriptorSetRemappingInfo	RemappingInfo;

	friend class FVulkanPipelineStateCacheManager;
	friend class FVulkanGraphicsPipelineDescriptorState;
	friend class FVulkanComputePipelineDescriptorState;
	friend class FVulkanGfxPipelineDescriptorInfo;
	friend class FVulkanComputePipelineDescriptorInfo;
	friend class FVulkanLayout;
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

	inline const VkDescriptorSetAllocateInfo& GetAllocateInfo() const
	{
		return DescriptorSetAllocateInfo;
	}

	inline uint32 GetHash() const
	{
		return Hash;
	}

private:
	FVulkanDevice* Device;
	//uint32 Hash = 0;
	TArray<VkDescriptorSetLayout> LayoutHandles;
	VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo;
};

class FVulkanDescriptorPool
{
public:
	FVulkanDescriptorPool(FVulkanDevice* InDevice, const FVulkanDescriptorSetsLayout& Layout);
	~FVulkanDescriptorPool();

	inline VkDescriptorPool GetHandle() const
	{
		return DescriptorPool;
	}

	inline bool CanAllocate(const FVulkanDescriptorSetsLayout& InLayout) const
	{
		return MaxDescriptorSets > NumAllocatedDescriptorSets + InLayout.GetLayouts().Num();
	}

	void TrackAddUsage(const FVulkanDescriptorSetsLayout& InLayout);
	void TrackRemoveUsage(const FVulkanDescriptorSetsLayout& InLayout);

	inline bool IsEmpty() const
	{
		return NumAllocatedDescriptorSets == 0;
	}

	void Reset();
	bool AllocateDescriptorSets(const VkDescriptorSetAllocateInfo& InDescriptorSetAllocateInfo, VkDescriptorSet* OutSets);
	inline uint32 GetNumAllocatedDescriptorSets() const
	{
		return NumAllocatedDescriptorSets;
	}

private:
	FVulkanDevice* Device;

	uint32 MaxDescriptorSets;
	uint32 NumAllocatedDescriptorSets;
	uint32 PeakAllocatedDescriptorSets;

	// Tracks number of allocated types, to ensure that we are not exceeding our allocated limit
	const FVulkanDescriptorSetsLayout& Layout;
	VkDescriptorPool DescriptorPool;

	friend class FVulkanCommandListContext;
};


class FVulkanTypedDescriptorPoolSet
{
	typedef TList<FVulkanDescriptorPool*> FPoolList;

	FVulkanDescriptorPool* GetFreePool(bool bForceNewPool = false);
	FVulkanDescriptorPool* PushNewPool();

protected:
	friend class FVulkanDescriptorPoolSetContainer;
	friend class FVulkanCmdBuffer;

	FVulkanTypedDescriptorPoolSet(FVulkanDevice* InDevice, const FVulkanDescriptorSetsLayout& InLayout)
		: Device(InDevice)
		, Layout(InLayout)
	{
		PushNewPool();
	};

	~FVulkanTypedDescriptorPoolSet();

	void Reset();

public:
	bool AllocateDescriptorSets(const FVulkanDescriptorSetsLayout& Layout, VkDescriptorSet* OutSets);

private:
	FVulkanDevice* Device;
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

	inline void SetUsed(bool bInUsed)
	{
		bUsed = bInUsed;
		LastFrameUsed = bUsed ? GFrameNumberRenderThread : LastFrameUsed;
	}

	inline bool IsUnused() const
	{
		return !bUsed;
	}

	inline uint32 GetLastFrameUsed() const
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

// This container holds the actual VkWriteDescriptorSet structures; a Compute pipeline uses the arrays 'as-is', whereas a 
// Gfx PSO will have one big array and chunk it depending on the stage (eg Vertex, Pixel).
struct FVulkanDescriptorSetWriteContainer
{
	TArray<VkDescriptorImageInfo> DescriptorImageInfo;
	TArray<VkDescriptorBufferInfo> DescriptorBufferInfo;
	TArray<VkWriteDescriptorSet> DescriptorWrites;
	TArray<uint8> BindingToDynamicOffsetMap;
};


// Smaller data structure for runtime information about descriptor sets and bindings for a pipeline
class FVulkanComputePipelineDescriptorInfo
{
public:
	FVulkanComputePipelineDescriptorInfo()
		: RemappingUBInfos(nullptr)
		, RemappingGlobalInfos(nullptr)
		, RemappingPackedUBInfos(nullptr)
		, HasDescriptorsInSetMask(0)
		, RemappingInfo(nullptr)
		, bInitialized(false)
	{
		//		FMemory::Memzero(CodeHeaders);
	}

	inline bool GetDescriptorSetAndBindingIndex(const FVulkanShaderHeader::EType Type, int32 ParameterIndex, uint8& OutDescriptorSet, uint32& OutBindingIndex) const
	{
		switch (Type)
		{
		case FVulkanShaderHeader::UniformBuffer:
			//ensure(RemappingInfo->StageInfos[0].UniformBuffers[ParameterIndex].bHasConstantData);
			ensure(RemappingUBInfos[ParameterIndex].bHasConstantData);
			//OutDescriptorSet = RemappingInfo->StageInfos[0].UniformBuffers[ParameterIndex].Remapping.NewDescriptorSet;
			//OutBindingIndex = RemappingInfo->StageInfos[0].UniformBuffers[ParameterIndex].Remapping.NewBindingIndex;
			OutDescriptorSet = RemappingUBInfos[ParameterIndex].Remapping.NewDescriptorSet;
			OutBindingIndex = RemappingUBInfos[ParameterIndex].Remapping.NewBindingIndex;
			break;
		case FVulkanShaderHeader::Global:
			//OutDescriptorSet = RemappingInfo->StageInfos[0].Globals[ParameterIndex].NewDescriptorSet;
			//OutBindingIndex = RemappingInfo->StageInfos[0].Globals[ParameterIndex].NewBindingIndex;
			OutDescriptorSet = RemappingGlobalInfos[ParameterIndex].NewDescriptorSet;
			OutBindingIndex = RemappingGlobalInfos[ParameterIndex].NewBindingIndex;
			break;
		default:
			check(0);
			return false;
		}
		return true;
	}

	inline const TArray<FDescriptorSetRemappingInfo::FRemappingInfo>& GetGlobalRemappingInfo() const
	{
		//OutDescriptorSet = RemappingUBInfos[Stage][ParameterIndex].Remapping.NewDescriptorSet;
		return RemappingInfo->StageInfos[0].Globals;
	}

	inline bool IsInitialized() const
	{
		return bInitialized;
	}

	void Initialize(const FDescriptorSetRemappingInfo& InRemappingInfo, FVulkanShader* ComputeShader);

protected:
	// Cached data from FDescriptorSetRemappingInfo
	const FDescriptorSetRemappingInfo::FUBRemappingInfo*	RemappingUBInfos;
	const FDescriptorSetRemappingInfo::FRemappingInfo*		RemappingGlobalInfos;
	const uint16*											RemappingPackedUBInfos;
	uint32													HasDescriptorsInSetMask;
	const FDescriptorSetRemappingInfo*						RemappingInfo;
	bool													bInitialized;

	friend class FVulkanComputePipelineDescriptorState;
};

// Smaller data structure for runtime information about descriptor sets and bindings for a pipeline
class FVulkanGfxPipelineDescriptorInfo
{
public:
	FVulkanGfxPipelineDescriptorInfo()
		: HasDescriptorsInSetMask(0)
		, RemappingInfo(nullptr)
		, bInitialized(false)
	{
		FMemory::Memzero(RemappingUBInfos);
		FMemory::Memzero(RemappingGlobalInfos);
		FMemory::Memzero(RemappingPackedUBInfos);
	}

	inline bool GetDescriptorSetAndBindingIndex(const FVulkanShaderHeader::EType Type, const ShaderStage::EStage Stage, int32 ParameterIndex, uint8& OutDescriptorSet, uint32& OutBindingIndex) const
	{
		switch (Type)
		{
		case FVulkanShaderHeader::UniformBuffer:
			ensure(RemappingUBInfos[Stage][ParameterIndex].bHasConstantData);
			//ensure(RemappingInfo->StageInfos[Stage].UniformBuffers[ParameterIndex].bHasConstantData);
			//OutDescriptorSet = RemappingInfo->StageInfos[Stage].UniformBuffers[ParameterIndex].Remapping.NewDescriptorSet;
			//OutBindingIndex = RemappingInfo->StageInfos[Stage].UniformBuffers[ParameterIndex].Remapping.NewBindingIndex;
			OutDescriptorSet = RemappingUBInfos[Stage][ParameterIndex].Remapping.NewDescriptorSet;
			OutBindingIndex = RemappingUBInfos[Stage][ParameterIndex].Remapping.NewBindingIndex;
			break;
		case FVulkanShaderHeader::Global:
			//OutDescriptorSet = RemappingInfo->StageInfos[Stage].Globals[ParameterIndex].NewDescriptorSet;
			//OutBindingIndex = RemappingInfo->StageInfos[Stage].Globals[ParameterIndex].NewBindingIndex;
			OutDescriptorSet = RemappingGlobalInfos[Stage][ParameterIndex].NewDescriptorSet;
			OutBindingIndex = RemappingGlobalInfos[Stage][ParameterIndex].NewBindingIndex;
			break;
		default:
			check(0);
			return false;
		}
		return true;
	}

	inline const TArray<FDescriptorSetRemappingInfo::FRemappingInfo>& GetGlobalRemappingInfo(ShaderStage::EStage Stage) const
	{
		//OutDescriptorSet = RemappingUBInfos[Stage][ParameterIndex].Remapping.NewDescriptorSet;
		return RemappingInfo->StageInfos[Stage].Globals;
	}

	inline bool IsInitialized() const
	{
		return bInitialized;
	}

	inline const TArray<FInputAttachmentData>& GetInputAttachmentData() const
	{
		return RemappingInfo->InputAttachmentData;
	}

	void Initialize(const FDescriptorSetRemappingInfo& InRemappingInfo, FVulkanShader** Shaders);

protected:
	// Cached data from FDescriptorSetRemappingInfo
	const FDescriptorSetRemappingInfo::FUBRemappingInfo*	RemappingUBInfos[ShaderStage::NumStages];
	const FDescriptorSetRemappingInfo::FRemappingInfo*		RemappingGlobalInfos[ShaderStage::NumStages];
	const uint16*											RemappingPackedUBInfos[ShaderStage::NumStages];
	uint32													HasDescriptorsInSetMask;

	const FDescriptorSetRemappingInfo*						RemappingInfo;
	bool													bInitialized;

	friend class FVulkanGraphicsPipelineDescriptorState;
};


// Layout for a Pipeline, also includes DescriptorSets layout
class FVulkanLayout : public VulkanRHI::FDeviceChild
{
public:
	FVulkanLayout(FVulkanDevice* InDevice);
	virtual ~FVulkanLayout();

	virtual bool IsGfxLayout() const = 0;

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

	VkShaderModule CreatePatchedPatchSpirvModule(TArray<uint32>& Spirv, EShaderFrequency Frequency, const FVulkanShaderHeader& CodeHeader, VkShaderStageFlagBits InStageFlag) const;

protected:
	FVulkanDescriptorSetsLayout	DescriptorSetLayout;
	VkPipelineLayout			PipelineLayout;

	template <bool bIsCompute>
	inline void FinalizeBindings(const FUniformBufferGatherInfo& UBGatherInfo)
	{
		// Setting descriptor is only allowed prior to compiling the layout
		check(DescriptorSetLayout.GetHandles().Num() == 0);

		DescriptorSetLayout.FinalizeBindings<bIsCompute>(UBGatherInfo);
	}

	inline void ProcessBindingsForStage(VkShaderStageFlagBits StageFlags, ShaderStage::EStage DescSet, const FVulkanShaderHeader& CodeHeader, FUniformBufferGatherInfo& OutUBGatherInfo) const
	{
		// Setting descriptor is only allowed prior to compiling the layout
		check(DescriptorSetLayout.GetHandles().Num() == 0);

		DescriptorSetLayout.ProcessBindingsForStage(StageFlags, DescSet, CodeHeader, OutUBGatherInfo);
	}

	void Compile();

	friend class FVulkanComputePipeline;
	friend class FVulkanGfxPipeline;
	friend class FVulkanPipelineStateCacheManager;
};

class FVulkanGfxLayout : public FVulkanLayout
{
public:
	FVulkanGfxLayout(FVulkanDevice* InDevice)
		: FVulkanLayout(InDevice)
	{
	}

	virtual bool IsGfxLayout() const final override
	{
		return true;
	}

	inline const FVulkanGfxPipelineDescriptorInfo& GetGfxPipelineDescriptorInfo() const
	{
		return GfxPipelineDescriptorInfo;
	}

protected:
	FVulkanGfxPipelineDescriptorInfo		GfxPipelineDescriptorInfo;
	friend class FVulkanPipelineStateCacheManager;
};

class FVulkanComputeLayout : public FVulkanLayout
{
public:
	FVulkanComputeLayout(FVulkanDevice* InDevice)
		: FVulkanLayout(InDevice)
	{
	}

	virtual bool IsGfxLayout() const final override
	{
		return false;
	}

	inline const FVulkanComputePipelineDescriptorInfo& GetComputePipelineDescriptorInfo() const
	{
		return ComputePipelineDescriptorInfo;
	}

protected:
	FVulkanComputePipelineDescriptorInfo		ComputePipelineDescriptorInfo;
	friend class FVulkanPipelineStateCacheManager;
};

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

	bool WriteInputAttachment(uint32 DescriptorIndex, VkImageView ImageView, VkImageLayout Layout)
	{
		check(DescriptorIndex < NumWrites);
		check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
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

	uint32 SetupDescriptorWrites(const TArray<VkDescriptorType>& Types, VkWriteDescriptorSet* InWriteDescriptors, VkDescriptorImageInfo* InImageInfo,
		VkDescriptorBufferInfo* InBufferInfo, uint8* InBindingToDynamicOffsetMap);

	friend class FVulkanComputePipelineDescriptorState;
	friend class FVulkanGraphicsPipelineDescriptorState;
};
