// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanShaders.cpp: Vulkan shader RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "GlobalShader.h"
#include "Serialization/MemoryReader.h"
#include "VulkanLLM.h"

TAutoConsoleVariable<int32> GDynamicGlobalUBs(
	TEXT("r.Vulkan.DynamicGlobalUBs"),
	1,
	TEXT("2 to treat ALL uniform buffers as dynamic\n")\
	TEXT("1 to treat global/packed uniform buffers as dynamic [default]\n")\
	TEXT("0 to treat them as regular"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);


static TAutoConsoleVariable<int32> GDescriptorSetLayoutMode(
	TEXT("r.Vulkan.DescriptorSetLayoutMode"),
	0,
	TEXT("0 to not change layouts (eg Set 0 = Vertex, 1 = Pixel, etc\n")\
	TEXT("1 to use a new set for common Uniform Buffers\n")\
	TEXT("2 to collapse all sets into Set 0\n"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

FVulkanShaderFactory::~FVulkanShaderFactory()
{
	for (auto& Map : ShaderMap)
	{
		Map.Empty();
	}
}

template <typename ShaderType> 
ShaderType* FVulkanShaderFactory::CreateShader(const TArray<uint8>& Code, FVulkanDevice* Device)
{
	uint32 ShaderCodeLen = Code.Num();
	uint32 ShaderCodeCRC = FCrc::MemCrc32(Code.GetData(), Code.Num());
	uint64 ShaderKey = ((uint64)ShaderCodeLen | ((uint64)ShaderCodeCRC << 32));

	ShaderType* RetShader = LookupShader<ShaderType>(ShaderKey);
	if (RetShader == nullptr)
	{
		RetShader = new ShaderType(Device);
		RetShader->Setup(Code, ShaderKey);
			
		FRWScopeLock ScopedLock(Lock, SLT_Write);
		ShaderMap[ShaderType::StaticFrequency].Add(ShaderKey, RetShader);
	}
	return RetShader;
}

void FVulkanShaderFactory::LookupShaders(const uint64 InShaderKeys[ShaderStage::NumStages], FVulkanShader* OutShaders[ShaderStage::NumStages]) const
{
	FRWScopeLock ScopedLock(Lock, SLT_ReadOnly);
	
	for (int32 Idx = 0; Idx < ShaderStage::NumStages; ++Idx)
	{
		uint64 ShaderKey = InShaderKeys[Idx];
		if (ShaderKey)
		{
			EShaderFrequency ShaderFrequency = ShaderStage::GetFrequencyForGfxStage((ShaderStage::EStage)Idx);
			FVulkanShader* const * FoundShaderPtr = ShaderMap[ShaderFrequency].Find(ShaderKey);
			if (FoundShaderPtr)
			{
				OutShaders[Idx] = *FoundShaderPtr;
			}
		}
	}
}

void FVulkanShaderFactory::OnDeleteShader(const FVulkanShader& Shader)
{
	FRWScopeLock ScopedLock(Lock, SLT_Write);
	uint64 ShaderKey = Shader.GetShaderKey(); 
	ShaderMap[Shader.Frequency].Remove(ShaderKey);
}

void FVulkanShader::Setup(const TArray<uint8>& InShaderHeaderAndCode, uint64 InShaderKey)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanShaders);
	check(Device);

	ShaderKey = InShaderKey;

	FMemoryReader Ar(InShaderHeaderAndCode, true);

	Ar << CodeHeader;

	Ar << Spirv;
#if VULKAN_ENABLE_SHADER_DEBUG_NAMES
	checkf(Spirv.Num() != 0, TEXT("Empty SPIR-V!%s"), *CodeHeader.DebugName);
#else
	checkf(Spirv.Num() != 0, TEXT("Empty SPIR-V!"));
#endif

	if (CodeHeader.bHasRealUBs)
	{
		check(CodeHeader.UniformBufferSpirvInfos.Num() == CodeHeader.UniformBuffers.Num());
	}
	else
	{
		checkSlow(CodeHeader.UniformBufferSpirvInfos.Num() == 0);
	}
	check(CodeHeader.GlobalSpirvInfos.Num() == CodeHeader.Globals.Num());

#if VULKAN_ENABLE_SHADER_DEBUG_NAMES
	// main_00000000_00000000
	ANSICHAR EntryPoint[24];
	GetEntryPoint(EntryPoint);
	DebugEntryPoint = EntryPoint;
#endif
}

VkShaderModule FVulkanShader::CreateHandle(const FVulkanLayout* Layout, uint32 LayoutHash)
{
	VkShaderModule Module = Layout->CreatePatchedPatchSpirvModule(Spirv, Frequency, CodeHeader, StageFlag);
	ShaderModules.Add(LayoutHash, Module);
	return Module;
}

FVulkanShader::~FVulkanShader()
{
	PurgeShaderModules();
	Device->GetShaderFactory().OnDeleteShader(*this);
}

void FVulkanShader::PurgeShaderModules()
{
	for (const auto& Pair : ShaderModules)
	{
		VkShaderModule ShaderModule = Pair.Value;
		Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::ShaderModule, ShaderModule);
	}
	ShaderModules.Empty(0);
}

VkShaderModule FVulkanLayout::CreatePatchedPatchSpirvModule(TArray<uint32>& Spirv, EShaderFrequency Frequency, const FVulkanShaderHeader& CodeHeader, VkShaderStageFlagBits InStageFlag) const
{
	VkShaderModule ShaderModule;
	VkShaderModuleCreateInfo ModuleCreateInfo;
	ZeroVulkanStruct(ModuleCreateInfo, VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
	//ModuleCreateInfo.flags = 0;

	//#todo-rco: Do we need an actual copy of the SPIR-V?
	ShaderStage::EStage Stage = ShaderStage::GetStageForFrequency(Frequency);
	const FDescriptorSetRemappingInfo::FStageInfo& StageInfo = DescriptorSetLayout.RemappingInfo.StageInfos[Stage];
	if (CodeHeader.bHasRealUBs)
	{
		checkSlow(StageInfo.UniformBuffers.Num() == CodeHeader.UniformBufferSpirvInfos.Num());
		for (int32 Index = 0; Index < CodeHeader.UniformBufferSpirvInfos.Num(); ++Index)
		{
			if (StageInfo.UniformBuffers[Index].bHasConstantData)
			{
				const uint32 OffsetDescriptorSet = CodeHeader.UniformBufferSpirvInfos[Index].DescriptorSetOffset;
				const uint32 OffsetBindingIndex = CodeHeader.UniformBufferSpirvInfos[Index].BindingIndexOffset;
				check(OffsetDescriptorSet != UINT32_MAX && OffsetBindingIndex != UINT32_MAX);
				uint16 NewDescriptorSet = StageInfo.UniformBuffers[Index].Remapping.NewDescriptorSet;
				Spirv[OffsetDescriptorSet] = NewDescriptorSet;
				uint16 NewBindingIndex = StageInfo.UniformBuffers[Index].Remapping.NewBindingIndex;
				Spirv[OffsetBindingIndex] = NewBindingIndex;
			}
		}
	}

	checkSlow(StageInfo.Globals.Num() == CodeHeader.GlobalSpirvInfos.Num());
	for (int32 Index = 0; Index < CodeHeader.GlobalSpirvInfos.Num(); ++Index)
	{
		const uint32 OffsetDescriptorSet = CodeHeader.GlobalSpirvInfos[Index].DescriptorSetOffset;
		const uint32 OffsetBindingIndex = CodeHeader.GlobalSpirvInfos[Index].BindingIndexOffset;
		check(OffsetDescriptorSet != UINT32_MAX && OffsetBindingIndex != UINT32_MAX);
		uint16 NewDescriptorSet = StageInfo.Globals[Index].NewDescriptorSet;
		Spirv[OffsetDescriptorSet] = NewDescriptorSet;
		uint16 NewBindingIndex = StageInfo.Globals[Index].NewBindingIndex;
		Spirv[OffsetBindingIndex] = NewBindingIndex;
	}

	checkSlow(StageInfo.PackedUBBindingIndices.Num() == CodeHeader.PackedUBs.Num());
	for (int32 Index = 0; Index < CodeHeader.PackedUBs.Num(); ++Index)
	{
		const uint32 OffsetDescriptorSet = CodeHeader.PackedUBs[Index].SPIRVDescriptorSetOffset;
		const uint32 OffsetBindingIndex = CodeHeader.PackedUBs[Index].SPIRVBindingIndexOffset;
		check(OffsetDescriptorSet != UINT32_MAX && OffsetBindingIndex != UINT32_MAX);
		Spirv[OffsetDescriptorSet] = StageInfo.PackedUBDescriptorSet;
		Spirv[OffsetBindingIndex] = StageInfo.PackedUBBindingIndices[Index];
	}

	ModuleCreateInfo.codeSize = Spirv.Num() * sizeof(uint32);
	ModuleCreateInfo.pCode = Spirv.GetData();

#if VULKAN_SUPPORTS_VALIDATION_CACHE
	VkShaderModuleValidationCacheCreateInfoEXT ValidationInfo;
	if (Device->GetOptionalExtensions().HasEXTValidationCache)
	{
		ZeroVulkanStruct(ValidationInfo, VK_STRUCTURE_TYPE_SHADER_MODULE_VALIDATION_CACHE_CREATE_INFO_EXT);
		ValidationInfo.validationCache = Device->GetValidationCache();
		ModuleCreateInfo.pNext = &ValidationInfo;
	}
#endif

	VERIFYVULKANRESULT(VulkanRHI::vkCreateShaderModule(Device->GetInstanceHandle(), &ModuleCreateInfo, VULKAN_CPU_ALLOCATOR, &ShaderModule));
	return ShaderModule;
}


FVertexShaderRHIRef FVulkanDynamicRHI::RHICreateVertexShader(const TArray<uint8>& Code)
{
	return Device->GetShaderFactory().CreateShader<FVulkanVertexShader>(Code, Device);
}

FPixelShaderRHIRef FVulkanDynamicRHI::RHICreatePixelShader(const TArray<uint8>& Code)
{
	return Device->GetShaderFactory().CreateShader<FVulkanPixelShader>(Code, Device);
}

FHullShaderRHIRef FVulkanDynamicRHI::RHICreateHullShader(const TArray<uint8>& Code) 
{ 
	return Device->GetShaderFactory().CreateShader<FVulkanHullShader>(Code, Device);
}

FDomainShaderRHIRef FVulkanDynamicRHI::RHICreateDomainShader(const TArray<uint8>& Code) 
{ 
	return Device->GetShaderFactory().CreateShader<FVulkanDomainShader>(Code, Device);
}

FGeometryShaderRHIRef FVulkanDynamicRHI::RHICreateGeometryShader(const TArray<uint8>& Code) 
{ 
	return Device->GetShaderFactory().CreateShader<FVulkanGeometryShader>(Code, Device);
}

FGeometryShaderRHIRef FVulkanDynamicRHI::RHICreateGeometryShaderWithStreamOutput(const TArray<uint8>& Code, const FStreamOutElementList& ElementList,
	uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
	return nullptr;
}

FComputeShaderRHIRef FVulkanDynamicRHI::RHICreateComputeShader(const TArray<uint8>& Code) 
{ 
	return Device->GetShaderFactory().CreateShader<FVulkanComputeShader>(Code, Device);
}


FVulkanLayout::FVulkanLayout(FVulkanDevice* InDevice)
	: VulkanRHI::FDeviceChild(InDevice)
	, DescriptorSetLayout(Device)
	, PipelineLayout(VK_NULL_HANDLE)
{
}

FVulkanLayout::~FVulkanLayout()
{
	if (PipelineLayout != VK_NULL_HANDLE)
	{
		Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::PipelineLayout, PipelineLayout);
		PipelineLayout = VK_NULL_HANDLE;
	}
}

void FVulkanLayout::Compile(FVulkanDescriptorSetLayoutMap& DSetLayoutMap)
{
	check(PipelineLayout == VK_NULL_HANDLE);

	DescriptorSetLayout.Compile(DSetLayoutMap);

	const TArray<VkDescriptorSetLayout>& LayoutHandles = DescriptorSetLayout.GetHandles();

	VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo;
	ZeroVulkanStruct(PipelineLayoutCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
	PipelineLayoutCreateInfo.setLayoutCount = LayoutHandles.Num();
	PipelineLayoutCreateInfo.pSetLayouts = LayoutHandles.GetData();
	//PipelineLayoutCreateInfo.pushConstantRangeCount = 0;

	VERIFYVULKANRESULT(VulkanRHI::vkCreatePipelineLayout(Device->GetInstanceHandle(), &PipelineLayoutCreateInfo, VULKAN_CPU_ALLOCATOR, &PipelineLayout));
}


uint32 FVulkanDescriptorSetWriter::SetupDescriptorWrites(
	const TArray<VkDescriptorType>& Types, FVulkanHashableDescriptorInfo* InHashableDescriptorInfos,
	VkWriteDescriptorSet* InWriteDescriptors, VkDescriptorImageInfo* InImageInfo, VkDescriptorBufferInfo* InBufferInfo, uint8* InBindingToDynamicOffsetMap,
	const FVulkanSamplerState& DefaultSampler, const FVulkanTextureView& DefaultImageView)
{
	HashableDescriptorInfos = InHashableDescriptorInfos;
	WriteDescriptors = InWriteDescriptors;
	NumWrites = Types.Num();
	checkf(Types.Num() <= 64, TEXT("Out of bits for Dirty Mask! More than 64 resources in one descriptor set!"));

	BindingToDynamicOffsetMap = InBindingToDynamicOffsetMap;

	BufferViewReferences.Empty(NumWrites);
	BufferViewReferences.AddDefaulted(NumWrites);

	uint32 DynamicOffsetIndex = 0;

	for (int32 Index = 0; Index < Types.Num(); ++Index)
	{
		InWriteDescriptors->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		InWriteDescriptors->dstBinding = Index;
		InWriteDescriptors->descriptorCount = 1;
		InWriteDescriptors->descriptorType = Types[Index];

		switch (Types[Index])
		{
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
			BindingToDynamicOffsetMap[Index] = DynamicOffsetIndex;
			++DynamicOffsetIndex;
			InWriteDescriptors->pBufferInfo = InBufferInfo++;
			break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			InWriteDescriptors->pBufferInfo = InBufferInfo++;
			break;
		case VK_DESCRIPTOR_TYPE_SAMPLER:
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			// Texture.Load() still requires a default sampler...
			if (InHashableDescriptorInfos) // UseVulkanDescriptorCache()
			{
				InHashableDescriptorInfos[Index].Image.SamplerId = DefaultSampler.SamplerId;
				InHashableDescriptorInfos[Index].Image.ImageViewId = DefaultImageView.ViewId;
				InHashableDescriptorInfos[Index].Image.ImageLayout = static_cast<uint32>(VK_IMAGE_LAYOUT_GENERAL);
			}
			InImageInfo->sampler = DefaultSampler.Sampler;
			InImageInfo->imageView = DefaultImageView.View;
			InImageInfo->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			InWriteDescriptors->pImageInfo = InImageInfo++;
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			break;
		default:
			checkf(0, TEXT("Unsupported descriptor type %d"), (int32)Types[Index]);
			break;
		}
		++InWriteDescriptors;
	}

	return DynamicOffsetIndex;
}

void FVulkanDescriptorSetsLayoutInfo::ProcessBindingsForStage(VkShaderStageFlagBits StageFlags, ShaderStage::EStage DescSetStage, const FVulkanShaderHeader& CodeHeader, FUniformBufferGatherInfo& OutUBGatherInfo) const
{
	const bool bMoveCommonUBsToExtraSet = GDescriptorSetLayoutMode.GetValueOnAnyThread() == 1 || GDescriptorSetLayoutMode.GetValueOnAnyThread() == 2;

	// Find all common UBs from different stages
	for (const FVulkanShaderHeader::FUniformBufferInfo& UBInfo : CodeHeader.UniformBuffers)
	{
		if (bMoveCommonUBsToExtraSet)
		{
			VkShaderStageFlags* Found = OutUBGatherInfo.CommonUBLayoutsToStageMap.Find(UBInfo.LayoutHash);
			if (Found)
			{
				*Found = *Found | StageFlags;
			}
			else
			{
				//#todo-rco: Only process constant data part of the UB
				Found = (UBInfo.ConstantDataOriginalBindingIndex == UINT16_MAX) ? nullptr : OutUBGatherInfo.UBLayoutsToUsedStageMap.Find(UBInfo.LayoutHash);
				if (Found)
				{
					// Move from per stage to common UBs
					VkShaderStageFlags PrevStage = (VkShaderStageFlags)0;
					bool bFound = OutUBGatherInfo.UBLayoutsToUsedStageMap.RemoveAndCopyValue(UBInfo.LayoutHash, PrevStage);
					check(bFound);
					check(OutUBGatherInfo.CommonUBLayoutsToStageMap.Find(UBInfo.LayoutHash) == nullptr);
					OutUBGatherInfo.CommonUBLayoutsToStageMap.Add(UBInfo.LayoutHash, PrevStage | (VkShaderStageFlags)StageFlags);
				}
				else
				{
					OutUBGatherInfo.UBLayoutsToUsedStageMap.Add(UBInfo.LayoutHash, (VkShaderStageFlags)StageFlags);
				}
			}
		}
		else
		{
			OutUBGatherInfo.UBLayoutsToUsedStageMap.Add(UBInfo.LayoutHash, (VkShaderStageFlags)StageFlags);
		}
	}

	OutUBGatherInfo.CodeHeaders[DescSetStage] = &CodeHeader;
}

template<bool bIsCompute>
void FVulkanDescriptorSetsLayoutInfo::FinalizeBindings(const FUniformBufferGatherInfo& UBGatherInfo, const TArrayView<const FSamplerStateRHIParamRef>& ImmutableSamplers)
{
	checkSlow(RemappingInfo.IsEmpty());

	TMap<uint32, FDescriptorSetRemappingInfo::FUBRemappingInfo> AlreadyProcessedUBs;

	// We'll be reusing this struct
	VkDescriptorSetLayoutBinding Binding;
	FMemory::Memzero(Binding);
	Binding.descriptorCount = 1;

	const bool bConvertAllUBsToDynamic = (GDynamicGlobalUBs.GetValueOnAnyThread() > 1);
	const bool bConvertPackedUBsToDynamic = bConvertAllUBsToDynamic || (GDynamicGlobalUBs.GetValueOnAnyThread() == 1);
	const bool bConsolidateAllIntoOneSet = GDescriptorSetLayoutMode.GetValueOnAnyThread() == 2;

	uint8	DescriptorStageToSetMapping[ShaderStage::NumStages];
	FMemory::Memset(DescriptorStageToSetMapping, UINT8_MAX);

	const bool bMoveCommonUBsToExtraSet = (UBGatherInfo.CommonUBLayoutsToStageMap.Num() > 0) || bConsolidateAllIntoOneSet;
	const uint32 CommonUBDescriptorSet = bMoveCommonUBsToExtraSet ? RemappingInfo.SetInfos.AddDefaulted() : UINT32_MAX;

	auto FindOrAddDescriptorSet = [&](int32 Stage) -> uint8
	{
		if (bConsolidateAllIntoOneSet)
		{
			return 0;
		}

		if (DescriptorStageToSetMapping[Stage] == UINT8_MAX)
		{
			uint32 NewSet = RemappingInfo.SetInfos.AddDefaulted();
			DescriptorStageToSetMapping[Stage] = (uint8)NewSet;
			return NewSet;
		}

		return DescriptorStageToSetMapping[Stage];
	};

	int32 CurrentImmutableSampler = 0;
	for (int32 Stage = 0; Stage < (bIsCompute ? 1 : ShaderStage::NumStages); ++Stage)
	{
		if (const FVulkanShaderHeader* ShaderHeader = UBGatherInfo.CodeHeaders[Stage])
		{
			VkShaderStageFlags StageFlags = UEFrequencyToVKStageBit(bIsCompute ? SF_Compute : ShaderStage::GetFrequencyForGfxStage((ShaderStage::EStage)Stage));
			Binding.stageFlags = StageFlags;

			RemappingInfo.StageInfos[Stage].PackedUBBindingIndices.Reserve(ShaderHeader->PackedUBs.Num());
			for (int32 Index = 0; Index < ShaderHeader->PackedUBs.Num(); ++Index)
			{
				int32 DescriptorSet = FindOrAddDescriptorSet(Stage);
				VkDescriptorType Type = bConvertPackedUBsToDynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				uint32 NewBindingIndex = RemappingInfo.AddPackedUB(Stage, Index, DescriptorSet, Type);

				Binding.binding = NewBindingIndex;
				Binding.descriptorType = Type;
				AddDescriptor(DescriptorSet, Binding);
			}

			if (ShaderHeader->bHasRealUBs)
			{
				RemappingInfo.StageInfos[Stage].UniformBuffers.Reserve(ShaderHeader->UniformBuffers.Num());
				for (int32 Index = 0; Index < ShaderHeader->UniformBuffers.Num(); ++Index)
				{
					VkDescriptorType Type = bConvertAllUBsToDynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					// Here we might mess up with the stageFlags, so reset them every loop
					Binding.stageFlags = StageFlags;
					Binding.descriptorType = Type;
					const FVulkanShaderHeader::FUniformBufferInfo& UBInfo = ShaderHeader->UniformBuffers[Index];
					const uint32 LayoutHash = UBInfo.LayoutHash;
					const bool bUBHasConstantData = UBInfo.ConstantDataOriginalBindingIndex != UINT16_MAX;
					if (bUBHasConstantData)
					{
						bool bProcessRegularUB = true;
						const VkShaderStageFlags* FoundFlags = bMoveCommonUBsToExtraSet ? UBGatherInfo.CommonUBLayoutsToStageMap.Find(LayoutHash) : nullptr;
						if (FoundFlags)
						{
							if (const FDescriptorSetRemappingInfo::FUBRemappingInfo* UBRemapInfo = AlreadyProcessedUBs.Find(LayoutHash))
							{
								RemappingInfo.AddRedundantUB(Stage, Index, UBRemapInfo);
							}
							else
							{
								//#todo-rco: Only process constant data part of the UB
								check(bUBHasConstantData);

								Binding.stageFlags = *FoundFlags;
								uint32 NewBindingIndex;
								AlreadyProcessedUBs.Add(LayoutHash, RemappingInfo.AddUBWithData(Stage, Index, CommonUBDescriptorSet, Type, NewBindingIndex));
								Binding.binding = NewBindingIndex;

								AddDescriptor(CommonUBDescriptorSet, Binding);
							}
							bProcessRegularUB = false;
						}

						if (bProcessRegularUB)
						{
							int32 DescriptorSet = FindOrAddDescriptorSet(Stage);
							uint32 NewBindingIndex;
							RemappingInfo.AddUBWithData(Stage, Index, DescriptorSet, Type, NewBindingIndex);
							Binding.binding = NewBindingIndex;

							AddDescriptor(FindOrAddDescriptorSet(Stage), Binding);
						}
					}
					else
					{
						RemappingInfo.AddUBResourceOnly(Stage, Index);
					}
				}
			}

			RemappingInfo.StageInfos[Stage].Globals.Reserve(ShaderHeader->Globals.Num());
			Binding.stageFlags = StageFlags;
			for (int32 Index = 0; Index < ShaderHeader->Globals.Num(); ++Index)
			{
				const FVulkanShaderHeader::FGlobalInfo& GlobalInfo = ShaderHeader->Globals[Index];
				int32 DescriptorSet = FindOrAddDescriptorSet(Stage);
				VkDescriptorType Type = ShaderHeader->GlobalDescriptorTypes[GlobalInfo.TypeIndex];
				uint16 CombinedSamplerStateAlias = GlobalInfo.CombinedSamplerStateAliasIndex;
				uint32 NewBindingIndex = RemappingInfo.AddGlobal(Stage, Index, DescriptorSet, Type, CombinedSamplerStateAlias);
				Binding.binding = NewBindingIndex;
				Binding.descriptorType = Type;
				if (CombinedSamplerStateAlias == UINT16_MAX)
				{
					if (GlobalInfo.bImmutableSampler)
					{
						if (CurrentImmutableSampler < ImmutableSamplers.Num())
						{
							const FVulkanSamplerState* SamplerState = ResourceCast(ImmutableSamplers[CurrentImmutableSampler]);
							if (SamplerState && SamplerState->Sampler != VK_NULL_HANDLE)
							{
								Binding.pImmutableSamplers = &SamplerState->Sampler;
							}
							++CurrentImmutableSampler;
						}
					}

					AddDescriptor(DescriptorSet, Binding);
				}

				Binding.pImmutableSamplers = nullptr;
			}

			if (ShaderHeader->InputAttachments.Num())
			{
				int32 DescriptorSet = FindOrAddDescriptorSet(Stage);
				check(Stage == ShaderStage::Pixel);
				for (int32 SrcIndex = 0; SrcIndex < ShaderHeader->InputAttachments.Num(); ++SrcIndex)
				{
					int32 OriginalGlobalIndex = ShaderHeader->InputAttachments[SrcIndex].GlobalIndex;
					const FVulkanShaderHeader::FGlobalInfo& OriginalGlobalInfo = ShaderHeader->Globals[OriginalGlobalIndex];
					check(ShaderHeader->GlobalDescriptorTypes[OriginalGlobalInfo.TypeIndex] == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
					int32 RemappingIndex = RemappingInfo.InputAttachmentData.AddDefaulted();
					FInputAttachmentData& AttachmentData = RemappingInfo.InputAttachmentData[RemappingIndex];
					AttachmentData.BindingIndex = RemappingInfo.StageInfos[Stage].Globals[OriginalGlobalIndex].NewBindingIndex;
					AttachmentData.DescriptorSet = (uint8)DescriptorSet;
					AttachmentData.Type = ShaderHeader->InputAttachments[SrcIndex].Type;
				}
			}
		}
	}

	CompileTypesUsageID();
	GenerateHash(ImmutableSamplers);

	// Validate no empty sets were made
	for (int32 Index = 0; Index < RemappingInfo.SetInfos.Num(); ++Index)
	{
		check(RemappingInfo.SetInfos[Index].Types.Num() > 0);
	}

	// Consolidated only has to have one Set
	check(!bConsolidateAllIntoOneSet || RemappingInfo.SetInfos.Num() == 1);
}

void FVulkanComputePipelineDescriptorInfo::Initialize(const FDescriptorSetRemappingInfo& InRemappingInfo)
{
	check(!bInitialized);

	RemappingGlobalInfos = InRemappingInfo.StageInfos[0].Globals.GetData();
	RemappingUBInfos = InRemappingInfo.StageInfos[0].UniformBuffers.GetData();
	RemappingPackedUBInfos = InRemappingInfo.StageInfos[0].PackedUBBindingIndices.GetData();

	RemappingInfo = &InRemappingInfo;

	for (int32 Index = 0; Index < InRemappingInfo.SetInfos.Num(); ++Index)
	{
		const FDescriptorSetRemappingInfo::FSetInfo& SetInfo = InRemappingInfo.SetInfos[Index];
		if (SetInfo.Types.Num() > 0)
		{
			check(Index < sizeof(HasDescriptorsInSetMask) * 8);
			HasDescriptorsInSetMask = HasDescriptorsInSetMask | (1 << Index);
		}
		else
		{
			ensure(0);
		}
	}

	bInitialized = true;
}

void FVulkanGfxPipelineDescriptorInfo::Initialize(const FDescriptorSetRemappingInfo& InRemappingInfo)
{
	check(!bInitialized);

	for (int32 StageIndex = 0; StageIndex < ShaderStage::NumStages; ++StageIndex)
	{
		//#todo-rco: Enable this!
		RemappingUBInfos[StageIndex] = InRemappingInfo.StageInfos[StageIndex].UniformBuffers.GetData();
		RemappingGlobalInfos[StageIndex] = InRemappingInfo.StageInfos[StageIndex].Globals.GetData();
		RemappingPackedUBInfos[StageIndex] = InRemappingInfo.StageInfos[StageIndex].PackedUBBindingIndices.GetData();
	}

	RemappingInfo = &InRemappingInfo;

	for (int32 Index = 0; Index < InRemappingInfo.SetInfos.Num(); ++Index)
	{
		const FDescriptorSetRemappingInfo::FSetInfo& SetInfo = InRemappingInfo.SetInfos[Index];
		if (SetInfo.Types.Num() > 0)
		{
			check(Index < sizeof(HasDescriptorsInSetMask) * 8);
			HasDescriptorsInSetMask = HasDescriptorsInSetMask | (1 << Index);
		}
		else
		{
			ensure(0);
		}
	}

	bInitialized = true;
}


FVulkanBoundShaderState::FVulkanBoundShaderState(FVertexDeclarationRHIParamRef InVertexDeclarationRHI, FVertexShaderRHIParamRef InVertexShaderRHI,
	FPixelShaderRHIParamRef InPixelShaderRHI, FHullShaderRHIParamRef InHullShaderRHI,
	FDomainShaderRHIParamRef InDomainShaderRHI, FGeometryShaderRHIParamRef InGeometryShaderRHI)
	: CacheLink(InVertexDeclarationRHI, InVertexShaderRHI, InPixelShaderRHI, InHullShaderRHI, InDomainShaderRHI, InGeometryShaderRHI, this)
{
	CacheLink.AddToCache();
}

FVulkanBoundShaderState::~FVulkanBoundShaderState()
{
	CacheLink.RemoveFromCache();
}

FBoundShaderStateRHIRef FVulkanDynamicRHI::RHICreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI, 
	FVertexShaderRHIParamRef VertexShaderRHI, 
	FHullShaderRHIParamRef HullShaderRHI, 
	FDomainShaderRHIParamRef DomainShaderRHI, 
	FPixelShaderRHIParamRef PixelShaderRHI,
	FGeometryShaderRHIParamRef GeometryShaderRHI
	)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanShaders);
	FBoundShaderStateRHIRef CachedBoundShaderState = GetCachedBoundShaderState_Threadsafe(
		VertexDeclarationRHI,
		VertexShaderRHI,
		PixelShaderRHI,
		HullShaderRHI,
		DomainShaderRHI,
		GeometryShaderRHI
	);
	if (CachedBoundShaderState.GetReference())
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderState;
	}

	return new FVulkanBoundShaderState(VertexDeclarationRHI, VertexShaderRHI, PixelShaderRHI, HullShaderRHI, DomainShaderRHI, GeometryShaderRHI);
}


template void FVulkanDescriptorSetsLayoutInfo::FinalizeBindings<true>(const FUniformBufferGatherInfo& UBGatherInfo, const TArrayView<const FSamplerStateRHIParamRef>& ImmutableSamplers);
template void FVulkanDescriptorSetsLayoutInfo::FinalizeBindings<false>(const FUniformBufferGatherInfo& UBGatherInfo, const TArrayView<const FSamplerStateRHIParamRef>& ImmutableSamplers);
