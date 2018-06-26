// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanShaders.cpp: Vulkan shader RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "GlobalShader.h"
#include "Serialization/MemoryReader.h"

TAutoConsoleVariable<int32> GDynamicGlobalUBs(
	TEXT("r.Vulkan.DynamicGlobalUBs"),
	1,
	TEXT("2 to treat ALL uniform buffers as dynamic\n")\
	TEXT("1 to treat global/packed uniform buffers as dynamic [default]\n")\
	TEXT("0 to treat them as regular"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

static void ConvertPackedUBsToDynamic(FVulkanCodeHeader& CodeHeader)
{
	if (GDynamicGlobalUBs.GetValueOnAnyThread() > 1)
	{
		for (VkDescriptorType& Type : CodeHeader.NEWDescriptorInfo.DescriptorTypes)
		{
			if (Type ==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
			{
				Type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			}
		}
	}
	else if (GDynamicGlobalUBs.GetValueOnAnyThread() == 1)
	{
		for (auto Entry : CodeHeader.NEWPackedUBToVulkanBindingIndices)
		{
			uint8 BindingIndex = Entry.VulkanBindingIndex;
			check(CodeHeader.NEWDescriptorInfo.DescriptorTypes[BindingIndex] == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			CodeHeader.NEWDescriptorInfo.DescriptorTypes[BindingIndex] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		}
	}
}


void FVulkanShader::Create(EShaderFrequency Frequency, const TArray<uint8>& InShaderCode)
{
	check(Device);

	FMemoryReader Ar(InShaderCode, true);

	Ar << CodeHeader;

#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	if (GDynamicGlobalUBs.GetValueOnAnyThread() > 0)
	{
		ConvertPackedUBsToDynamic(CodeHeader);
	}
#endif

	TArray<ANSICHAR> DebugNameArray;
	Ar << DebugNameArray;
	DebugName = ANSI_TO_TCHAR(DebugNameArray.GetData());

	Ar << Spirv;
	check(Spirv.Num() != 0);

	int32 CodeOffset = Ar.Tell();

	VkShaderModuleCreateInfo ModuleCreateInfo;
	ZeroVulkanStruct(ModuleCreateInfo, VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
	//ModuleCreateInfo.flags = 0;

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

	VERIFYVULKANRESULT(VulkanRHI::vkCreateShaderModule(Device->GetInstanceHandle(), &ModuleCreateInfo, nullptr, &ShaderModule));
}

#if VULKAN_HAS_DEBUGGING_ENABLED
inline void ValidateBindingPoint(const FVulkanShader& InShader, const uint32 InBindingPoint, const uint8 InSubType)
{
#if 0
	const TArray<FVulkanShaderSerializedBindings::FBindMap>& BindingLayout = InShader.CodeHeader.SerializedBindings.Bindings;
	bool bFound = false;

	for (const auto& Binding : BindingLayout)
	{
		const bool bIsPackedUniform = InSubType == CrossCompiler::PACKED_TYPENAME_HIGHP
			|| InSubType == CrossCompiler::PACKED_TYPENAME_MEDIUMP
			|| InSubType == CrossCompiler::PACKED_TYPENAME_LOWP;

		if (Binding.EngineBindingIndex == InBindingPoint &&
			bIsPackedUniform ? (Binding.Type == EVulkanBindingType::PACKED_UNIFORM_BUFFER) : (Binding.SubType == InSubType)
			)
		{
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		FString SubTypeName = "UNDEFINED";

		switch (InSubType)
		{
		case CrossCompiler::PACKED_TYPENAME_HIGHP: SubTypeName = "HIGH PRECISION UNIFORM PACKED BUFFER";	break;
		case CrossCompiler::PACKED_TYPENAME_MEDIUMP: SubTypeName = "MEDIUM PRECISION UNIFORM PACKED BUFFER";	break;
		case CrossCompiler::PACKED_TYPENAME_LOWP: SubTypeName = "LOW PRECISION UNIFORM PACKED BUFFER";	break;
		case CrossCompiler::PACKED_TYPENAME_INT: SubTypeName = "INT UNIFORM PACKED BUFFER";				break;
		case CrossCompiler::PACKED_TYPENAME_UINT: SubTypeName = "UINT UNIFORM PACKED BUFFER";				break;
		case CrossCompiler::PACKED_TYPENAME_SAMPLER: SubTypeName = "SAMPLER";								break;
		case CrossCompiler::PACKED_TYPENAME_IMAGE: SubTypeName = "IMAGE";									break;
		default:
			break;
		}

		UE_LOG(LogVulkanRHI, Warning,
			TEXT("Setting '%s' resource for an unexpected binding slot UE:%d, for shader '%s'"),
			*SubTypeName, InBindingPoint, *InShader.DebugName);
	}
#endif
}
#endif // VULKAN_HAS_DEBUGGING_ENABLED

template<typename BaseResourceType, EShaderFrequency Frequency>
void TVulkanBaseShader<BaseResourceType, Frequency>::Create(const TArray<uint8>& InCode)
{
	FVulkanShader::Create(Frequency, InCode);
}


FVulkanShader::~FVulkanShader()
{
	if (ShaderModule != VK_NULL_HANDLE)
	{
		Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::ShaderModule, ShaderModule);
		ShaderModule = VK_NULL_HANDLE;
	}
}

FVertexShaderRHIRef FVulkanDynamicRHI::RHICreateVertexShader(const TArray<uint8>& Code)
{
	FVulkanVertexShader* Shader = new FVulkanVertexShader(Device);
	Shader->Create(Code);
	return Shader;
}

FPixelShaderRHIRef FVulkanDynamicRHI::RHICreatePixelShader(const TArray<uint8>& Code)
{
	FVulkanPixelShader* Shader = new FVulkanPixelShader(Device);
	Shader->Create(Code);
	return Shader;
}

FHullShaderRHIRef FVulkanDynamicRHI::RHICreateHullShader(const TArray<uint8>& Code) 
{ 
	FVulkanHullShader* Shader = new FVulkanHullShader(Device);
	Shader->Create(Code);
	return Shader;
}

FDomainShaderRHIRef FVulkanDynamicRHI::RHICreateDomainShader(const TArray<uint8>& Code) 
{ 
	FVulkanDomainShader* Shader = new FVulkanDomainShader(Device);
	Shader->Create(Code);
	return Shader;
}

FGeometryShaderRHIRef FVulkanDynamicRHI::RHICreateGeometryShader(const TArray<uint8>& Code) 
{ 
	FVulkanGeometryShader* Shader = new FVulkanGeometryShader(Device);
	Shader->Create(Code);
	return Shader;
}

FGeometryShaderRHIRef FVulkanDynamicRHI::RHICreateGeometryShaderWithStreamOutput(const TArray<uint8>& Code, const FStreamOutElementList& ElementList,
	uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
	return nullptr;
}

FComputeShaderRHIRef FVulkanDynamicRHI::RHICreateComputeShader(const TArray<uint8>& Code) 
{ 
	FVulkanComputeShader* Shader = new FVulkanComputeShader(Device);
	Shader->Create(Code);
	return Shader;
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

void FVulkanLayout::Compile()
{
	check(PipelineLayout == VK_NULL_HANDLE);

	DescriptorSetLayout.Compile();

	const TArray<VkDescriptorSetLayout>& LayoutHandles = DescriptorSetLayout.GetHandles();

	VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo;
	ZeroVulkanStruct(PipelineLayoutCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
	PipelineLayoutCreateInfo.setLayoutCount = LayoutHandles.Num();
	PipelineLayoutCreateInfo.pSetLayouts = LayoutHandles.GetData();
	//PipelineLayoutCreateInfo.pushConstantRangeCount = 0;

	VERIFYVULKANRESULT(VulkanRHI::vkCreatePipelineLayout(Device->GetInstanceHandle(), &PipelineLayoutCreateInfo, nullptr, &PipelineLayout));
}


#if !VULKAN_USE_DESCRIPTOR_POOL_MANAGER
FOLDVulkanDescriptorSetRingBuffer::FOLDVulkanDescriptorSetRingBuffer(FVulkanDevice* InDevice)
	: VulkanRHI::FDeviceChild(InDevice)
	, CurrDescriptorSets(nullptr)
{
}
#endif

uint32 FVulkanDescriptorSetWriter::SetupDescriptorWrites(const FNEWVulkanShaderDescriptorInfo& Info, VkWriteDescriptorSet* InWriteDescriptors, 
	VkDescriptorImageInfo* InImageInfo, VkDescriptorBufferInfo* InBufferInfo, uint8* InBindingToDynamicOffsetMap)
{
	WriteDescriptors = InWriteDescriptors;
	NumWrites = Info.DescriptorTypes.Num();
	checkf(Info.DescriptorTypes.Num() <= 64, TEXT("Out of bits for Dirty Mask! More than 64 resources in one descriptor set!"));

	BindingToDynamicOffsetMap = InBindingToDynamicOffsetMap;

	BufferViewReferences.Empty(NumWrites);
	BufferViewReferences.AddDefaulted(NumWrites);

	uint32 DynamicOffsetIndex = 0;
	for (int32 Index = 0; Index < Info.DescriptorTypes.Num(); ++Index)
	{
		InWriteDescriptors->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		InWriteDescriptors->dstBinding = Index;
		InWriteDescriptors->descriptorCount = 1;
		InWriteDescriptors->descriptorType = Info.DescriptorTypes[Index];

		switch (Info.DescriptorTypes[Index])
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
			InWriteDescriptors->pImageInfo = InImageInfo++;
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			break;
		default:
			checkf(0, TEXT("Unsupported descriptor type %d"), (int32)Info.DescriptorTypes[Index]);
			break;
		}
		++InWriteDescriptors;
	}

	return DynamicOffsetIndex;
}

void FVulkanDescriptorSetsLayoutInfo::AddBindingsForStage(VkShaderStageFlagBits StageFlags, DescriptorSet::EStage DescSet, const FVulkanCodeHeader& CodeHeader)
{
	//#todo-rco: Mobile assumption!
	int32 DescriptorSetIndex = (int32)DescSet;

	VkDescriptorSetLayoutBinding Binding;
	FMemory::Memzero(Binding);
	Binding.descriptorCount = 1;
	Binding.stageFlags = StageFlags;
	for (int32 Index = 0; Index < CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num(); ++Index)
	{
		Binding.binding = Index;
		Binding.descriptorType = CodeHeader.NEWDescriptorInfo.DescriptorTypes[Index];
		AddDescriptor(DescriptorSetIndex, Binding, Index);
	}
}


#if !VULKAN_USE_DESCRIPTOR_POOL_MANAGER
FOLDVulkanDescriptorSetRingBuffer::FDescriptorSetsPair::~FDescriptorSetsPair()
{
	delete DescriptorSets;
}

FOLDVulkanDescriptorSets* FOLDVulkanDescriptorSetRingBuffer::RequestDescriptorSets(FVulkanCommandListContext* Context, FVulkanCmdBuffer* CmdBuffer, const FVulkanLayout& Layout)
{
	FDescriptorSetsEntry* FoundEntry = nullptr;
	for (FDescriptorSetsEntry* DescriptorSetsEntry : DescriptorSetsEntries)
	{
		if (DescriptorSetsEntry->CmdBuffer == CmdBuffer)
		{
			FoundEntry = DescriptorSetsEntry;
		}
	}

	if (!FoundEntry)
	{
		if (!Layout.HasDescriptors())
		{
			return nullptr;
		}

		FoundEntry = new FDescriptorSetsEntry(CmdBuffer);
		DescriptorSetsEntries.Add(FoundEntry);
	}

	const uint64 CmdBufferFenceSignaledCounter = CmdBuffer->GetFenceSignaledCounter();
	for (int32 Index = 0; Index < FoundEntry->Pairs.Num(); ++Index)
	{
		FDescriptorSetsPair& Entry = FoundEntry->Pairs[Index];
		if (Entry.FenceCounter < CmdBufferFenceSignaledCounter)
		{
			Entry.FenceCounter = CmdBufferFenceSignaledCounter;
			return Entry.DescriptorSets;
		}
	}

	FDescriptorSetsPair* NewEntry = new (FoundEntry->Pairs) FDescriptorSetsPair;
	NewEntry->DescriptorSets = new FOLDVulkanDescriptorSets(Device, Layout.GetDescriptorSetsLayout(), Context);
	NewEntry->FenceCounter = CmdBufferFenceSignaledCounter;
	return NewEntry->DescriptorSets;
}
#endif

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

FVulkanDescriptorPool* FVulkanCommandListContext::AllocateDescriptorSets(const VkDescriptorSetAllocateInfo& InDescriptorSetAllocateInfo, const FVulkanDescriptorSetsLayout& Layout, VkDescriptorSet* OutSets)
{
#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
	FVulkanDescriptorPool* Pool = nullptr;
	VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = InDescriptorSetAllocateInfo;
	VkResult Result = VK_ERROR_OUT_OF_DEVICE_MEMORY;

	const uint32 Hash = VULKAN_HASH_POOLS_WITH_TYPES_USAGE_ID ? Layout.GetTypesUsageID() : GetTypeHash(Layout);
	FDescriptorPoolArray* TypedDescriptorPools = DescriptorPools.Find(Hash);

	if (TypedDescriptorPools != nullptr)
	{
		Pool = TypedDescriptorPools->Last();

		if (Pool->CanAllocate(Layout))
		{
			DescriptorSetAllocateInfo.descriptorPool = Pool->GetHandle();
			Result = VulkanRHI::vkAllocateDescriptorSets(Device->GetInstanceHandle(), &DescriptorSetAllocateInfo, OutSets);
		}
	}
	else
	{
		TypedDescriptorPools = &DescriptorPools.Add(Hash);
	}
#else
	FVulkanDescriptorPool* Pool = DescriptorPools.Last();
	VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = InDescriptorSetAllocateInfo;
	VkResult Result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
	if (Pool->CanAllocate(Layout))
	{
		DescriptorSetAllocateInfo.descriptorPool = Pool->GetHandle();
		Result = VulkanRHI::vkAllocateDescriptorSets(Device->GetInstanceHandle(), &DescriptorSetAllocateInfo, OutSets);
	}
#endif
	if (Result < VK_SUCCESS)
	{
		if (Pool && Pool->IsEmpty())
		{
			VERIFYVULKANRESULT(Result);
		}
		else
		{
			// Spec says any negative value could be due to fragmentation, so create a new Pool. If it fails here then we really are out of memory!
#if VULKAN_USE_DESCRIPTOR_POOL_MANAGER
			Pool = new FVulkanDescriptorPool(Device, Layout);
			TypedDescriptorPools->Add(Pool);
#else
			Pool = new FVulkanDescriptorPool(Device);
			DescriptorPools.Add(Pool);
#endif
			DescriptorSetAllocateInfo.descriptorPool = Pool->GetHandle();
			VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkAllocateDescriptorSets(Device->GetInstanceHandle(), &DescriptorSetAllocateInfo, OutSets));
		}
	}

	return Pool;
}
