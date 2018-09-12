// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanPipeline.cpp: Vulkan device RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPipeline.h"
#include "Misc/FileHelper.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "GlobalShader.h"
#include "VulkanLLM.h"

static const double HitchTime = 1.0 / 1000.0;

static FCriticalSection GShaderHashToGfxEntriesMapCS;

#if VULKAN_ENABLE_LRU_CACHE
TAutoConsoleVariable<int32> CVarEnableLRU(
	TEXT("r.Vulkan.EnablePipelineLRUCache"),
	0,
	TEXT("Pipeline LRU cache.\n")
	TEXT("0: disable LRU\n")
	TEXT("1: Enable LRU"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

TAutoConsoleVariable<int32> CVarLRUMaxPipelineSize(
	TEXT("r.Vulkan.PipelineLRUSize"),
	10 * 1024 * 1024,
	TEXT("Maximum size of shader memory ."),
	ECVF_RenderThreadSafe);

static bool IsUsePipelineLRU()
{
	static int32 bUse = -1;
	if (bUse == -1)
	{
		bUse = CVarEnableLRU.GetValueOnAnyThread();
	}
	return bUse == 1;
}
#endif

template <typename TRHIType, typename TVulkanType>
static inline FSHAHash GetShaderHash(TRHIType* RHIShader)
{
	if (RHIShader)
	{
		const TVulkanType* VulkanShader = ResourceCast<TRHIType>(RHIShader);
		const FVulkanShader* Shader = static_cast<const FVulkanShader*>(VulkanShader);
		check(Shader);
		return Shader->GetCodeHeader().SourceHash;
	}

	FSHAHash Dummy;
	return Dummy;
}

static inline FSHAHash GetShaderHashForStage(const FGraphicsPipelineStateInitializer& PSOInitializer, ShaderStage::EStage Stage)
{
	switch (Stage)
	{
	case ShaderStage::Vertex:		return GetShaderHash<FRHIVertexShader, FVulkanVertexShader>(PSOInitializer.BoundShaderState.VertexShaderRHI);
	case ShaderStage::Pixel:		return GetShaderHash<FRHIPixelShader, FVulkanPixelShader>(PSOInitializer.BoundShaderState.PixelShaderRHI);
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
	case ShaderStage::Geometry:		return GetShaderHash<FRHIGeometryShader, FVulkanGeometryShader>(PSOInitializer.BoundShaderState.GeometryShaderRHI);
#endif
	//case ShaderStage::Hull:		return GetShaderHash<FRHIHullShader, FVulkanHullShader>(PSOInitializer.BoundShaderState.HullShaderRHI);
	//case ShaderStage::Domain:		return GetShaderHash<FRHIDomainShader, FVulkanDomainShader>(PSOInitializer.BoundShaderState.DomainShaderRHI);
	default:			check(0);	break;
	}

	FSHAHash Dummy;
	return Dummy;
}

FVulkanPipeline::FVulkanPipeline(FVulkanDevice* InDevice)
	: Device(InDevice)
	, Pipeline(VK_NULL_HANDLE)
	, Layout(nullptr)
{
}

FVulkanPipeline::~FVulkanPipeline()
{
	Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::Pipeline, Pipeline);
	Pipeline = VK_NULL_HANDLE;
	/* we do NOT own Layout !*/
}

FVulkanComputePipeline::FVulkanComputePipeline(FVulkanDevice* InDevice)
	: FVulkanPipeline(InDevice)
	, ComputeShader(nullptr)
{
}

FVulkanComputePipeline::~FVulkanComputePipeline()
{
	Device->NotifyDeletedComputePipeline(this);
}

#if VULKAN_ENABLE_LRU_CACHE
FVulkanGfxPipeline::FVulkanGfxPipeline(FVulkanDevice* InDevice, uint32 InGfxEntryHash, uint32 InShaderHash)
	: FVulkanPipeline(InDevice)
	, bRuntimeObjectsValid(false)
	, GfxEntryHash(InGfxEntryHash)
	, PipelineCacheSize(0)
	, ShaderHash(InShaderHash)
#else
FVulkanGfxPipeline::FVulkanGfxPipeline(FVulkanDevice* InDevice)
	: FVulkanPipeline(InDevice)
	, bRuntimeObjectsValid(false)
#endif
{
}

void FVulkanGfxPipeline::CreateRuntimeObjects(const FGraphicsPipelineStateInitializer& InPSOInitializer)
{
	const FBoundShaderStateInput& BSI = InPSOInitializer.BoundShaderState;
	
	check(BSI.VertexShaderRHI);
	FVulkanVertexShader* VS = ResourceCast(BSI.VertexShaderRHI);
	const FVulkanShaderHeader& VSHeader = VS->GetCodeHeader();

	VertexInputState.Generate(ResourceCast(InPSOInitializer.BoundShaderState.VertexDeclarationRHI), VSHeader.InOutMask);
	bRuntimeObjectsValid = true;
}

FVulkanRHIGraphicsPipelineState::~FVulkanRHIGraphicsPipelineState()
{
	if (Pipeline)
	{
		Pipeline->Device->NotifyDeletedGfxPipeline(this);
		Pipeline = nullptr;
	}
}


static TAutoConsoleVariable<int32> GEnablePipelineCacheLoadCvar(
	TEXT("r.Vulkan.PipelineCacheLoad"),
	1,
	TEXT("0 to disable loading the pipeline cache")
	TEXT("1 to enable using pipeline cache")
	);

static int32 GEnablePipelineCacheCompression = 1;
static FAutoConsoleVariableRef GEnablePipelineCacheCompressionCvar(
	TEXT("r.Vulkan.PipelineCacheCompression"),
	GEnablePipelineCacheCompression,
	TEXT("Enable/disable compression on the Vulkan pipeline cache disk file\n"),
	ECVF_Default | ECVF_RenderThreadSafe
);


FVulkanPipelineStateCacheManager::FGfxPipelineEntry::~FGfxPipelineEntry()
{
	check(!bLoaded);
}

FVulkanPipelineStateCacheManager::FComputePipelineEntry::~FComputePipelineEntry()
{
	check(!bLoaded);
}

FVulkanPipelineStateCacheManager::FVulkanPipelineStateCacheManager(FVulkanDevice* InDevice)
	: Device(InDevice)
	, PipelineCache(VK_NULL_HANDLE)
{
}


FVulkanPipelineStateCacheManager::~FVulkanPipelineStateCacheManager()
{
	DestroyCache();

	// Only destroy layouts when quitting
	for (auto& Pair : LayoutMap)
	{
		delete Pair.Value;
	}

#if VULKAN_ENABLE_LRU_CACHE
	PipelineLRU.Empty();
#endif

	VulkanRHI::vkDestroyPipelineCache(Device->GetInstanceHandle(), PipelineCache, VULKAN_CPU_ALLOCATOR);
	PipelineCache = VK_NULL_HANDLE;
}

void FVulkanPipelineStateCacheManager::Load(const TArray<FString>& CacheFilenames)
{
	// Try to load device cache first
	for (const FString& CacheFilename : CacheFilenames)
	{
		const VkPhysicalDeviceProperties& DeviceProperties = Device->GetDeviceProperties();
		double BeginTime = FPlatformTime::Seconds();
		FString BinaryCacheFilename = FString::Printf(TEXT("%s.%x.%x"), *CacheFilename, DeviceProperties.vendorID, DeviceProperties.deviceID);
		TArray<uint8> DeviceCache;
		if (FFileHelper::LoadFileToArray(DeviceCache, *BinaryCacheFilename, FILEREAD_Silent))
		{
			if (BinaryCacheMatches(Device, DeviceCache))
			{
				VkPipelineCacheCreateInfo PipelineCacheInfo;
				ZeroVulkanStruct(PipelineCacheInfo, VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
				PipelineCacheInfo.initialDataSize = DeviceCache.Num();
				PipelineCacheInfo.pInitialData = DeviceCache.GetData();

				if (PipelineCache == VK_NULL_HANDLE)
				{
					// if we don't have one already, then create our main cache (PipelineCache)
					VERIFYVULKANRESULT(VulkanRHI::vkCreatePipelineCache(Device->GetInstanceHandle(), &PipelineCacheInfo, VULKAN_CPU_ALLOCATOR, &PipelineCache));
				}
				else
				{
					// if we have one already, create a temp one and merge into the main cache
					VkPipelineCache TempPipelineCache;
					VERIFYVULKANRESULT(VulkanRHI::vkCreatePipelineCache(Device->GetInstanceHandle(), &PipelineCacheInfo, VULKAN_CPU_ALLOCATOR, &TempPipelineCache));
					VERIFYVULKANRESULT(VulkanRHI::vkMergePipelineCaches(Device->GetInstanceHandle(), PipelineCache, 1, &TempPipelineCache));
					VulkanRHI::vkDestroyPipelineCache(Device->GetInstanceHandle(), TempPipelineCache, VULKAN_CPU_ALLOCATOR);
				}

				double EndTime = FPlatformTime::Seconds();
				UE_LOG(LogVulkanRHI, Display, TEXT("Loaded binary pipeline cache %s in %.3f seconds"), *BinaryCacheFilename, (float)(EndTime - BeginTime));
			}
		}
	}

#if VULKAN_ENABLE_LRU_CACHE
	for (const FString& CacheFilename : CacheFilenames)
	{
		const VkPhysicalDeviceProperties& DeviceProperties = Device->GetDeviceProperties();
		double BeginTime = FPlatformTime::Seconds();
		FString LruCacheFilename = FString::Printf(TEXT("%s.%x.%x.lru"), *CacheFilename, DeviceProperties.vendorID, DeviceProperties.deviceID);
		TArray<uint8> MemFile;
		if (FFileHelper::LoadFileToArray(MemFile, *LruCacheFilename, FILEREAD_Silent))
		{
			FMemoryReader Ar(MemFile);

			FVulkanLRUCacheFile File;
			bool Valid = File.Load(Ar);
			if (!Valid)
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to load lru pipeline cache '%s'"), *LruCacheFilename);
				continue;
			}

			for (int32 Index = 0; Index < File.PipelineSizes.Num(); ++Index)
			{
				PipelineSizeList.Add(File.PipelineSizes[Index]->ShaderHash, File.PipelineSizes[Index]);
			}
		}
	}
#endif

#if VULKAN_ENABLE_GENERIC_PIPELINE_CACHE_FILE
	for (const FString& CacheFilename : CacheFilenames)
	{
		TArray<uint8> MemFile;
		if (FFileHelper::LoadFileToArray(MemFile, *CacheFilename, FILEREAD_Silent))
		{
			FMemoryReader Ar(MemFile);

			FVulkanPipelineStateCacheFile File;

			FShaderUCodeCache::TDataMap FileShaderCacheData;
			File.ShaderCache = &FileShaderCacheData;

			bool Valid = File.Load(Ar, *CacheFilename);
			if (!Valid)
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to load pipeline cache '%s'"), *CacheFilename);
				continue;
			}

			// Create the binary cache if we haven't already
			if (PipelineCache == VK_NULL_HANDLE)
			{
				VkPipelineCacheCreateInfo PipelineCacheInfo;
				ZeroVulkanStruct(PipelineCacheInfo, VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
				VERIFYVULKANRESULT(VulkanRHI::vkCreatePipelineCache(Device->GetInstanceHandle(), &PipelineCacheInfo, VULKAN_CPU_ALLOCATOR, &PipelineCache));
			}

			// Not using TMap::Append to avoid copying duplicate microcode
			for (auto& Pair : FileShaderCacheData)
			{
				if (!ShaderCache.Data.Find(Pair.Key))
				{
					ShaderCache.Data.Add(Pair.Key, Pair.Value);
				}
			}

			double BeginTime = FPlatformTime::Seconds();
			{
				for (int32 Index = 0; Index < File.GfxPipelineEntries.Num(); ++Index)
				{
					FGfxPipelineEntry* GfxEntry = File.GfxPipelineEntries[Index];

					FShaderHashes ShaderHashes;
					for (int32 i = 0; i < ShaderStage::NumStages; ++i)
					{
						ShaderHashes.Stages[i] = GfxEntry->ShaderHashes[i];

						GfxEntry->ShaderMicrocodes[i] = ShaderCache.Get(GfxEntry->ShaderHashes[i]);
					}
					ShaderHashes.Finalize();

					uint32 EntryHash = GfxEntry->GetEntryHash();
					if (GfxPipelineEntries.Find(EntryHash))
					{
						delete GfxEntry;
					}
					else
					{
						FHashToGfxPipelinesMap* Found = ShaderHashToGfxPipelineMap.Find(ShaderHashes);
						if (!Found)
						{
							Found = &ShaderHashToGfxPipelineMap.Add(ShaderHashes);
						}

						CreatGfxEntryRuntimeObjects(GfxEntry);
#if VULKAN_ENABLE_LRU_CACHE
						FVulkanGfxPipeline* Pipeline = new FVulkanGfxPipeline(Device, EntryHash, ShaderHashes.Hash);
#else
						FVulkanGfxPipeline* Pipeline = new FVulkanGfxPipeline(Device);
#endif
						CreateGfxPipelineFromEntry(GfxEntry, Pipeline);

						Found->Add(EntryHash, Pipeline);
						GfxPipelineEntries.Add(EntryHash, GfxEntry);
					}
				}
			}

			for (int32 Index = 0; Index < File.ComputePipelineEntries.Num(); ++Index)
			{
				FComputePipelineEntry* ComputeEntry = File.ComputePipelineEntries[Index];
				ComputeEntry->ShaderMicrocode = ShaderCache.Get(ComputeEntry->ShaderHash);
				ComputeEntry->CalculateEntryHash();

				if (ComputePipelineEntries.Find(ComputeEntry->EntryHash))
				{
					delete ComputeEntry;
				}
				else
				{
					CreateComputeEntryRuntimeObjects(ComputeEntry);

					FVulkanComputePipeline* Pipeline = CreateComputePipelineFromEntry(ComputeEntry);
					ComputeEntryHashToPipelineMap.Add(ComputeEntry->EntryHash, Pipeline);
					ComputePipelineEntries.Add(ComputeEntry->EntryHash, ComputeEntry);
					Pipeline->AddRef();
				}
			}

			double EndTime = FPlatformTime::Seconds();
			UE_LOG(LogVulkanRHI, Display, TEXT("Loaded pipeline cache in %.2f seconds"), (float)(EndTime - BeginTime));
		}
		else
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to load pipeline cache '%s'"), *CacheFilename);
		}
	}

#endif

#if VULKAN_ENABLE_GENERIC_PIPELINE_CACHE_FILE
	if (ShaderCache.Data.Num())
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Pipeline cache: %d Gfx Pipelines, %d Compute Pipelines, %d Microcodes"), GfxPipelineEntries.Num(), ComputePipelineEntries.Num(), ShaderCache.Data.Num());
	}
	else
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Pipeline cache: No pipeline cache(s) loaded"));
	}
#endif

	// Lazily create the cache in case the load failed
	if (PipelineCache == VK_NULL_HANDLE)
	{
		VkPipelineCacheCreateInfo PipelineCacheInfo;
		ZeroVulkanStruct(PipelineCacheInfo, VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
		VERIFYVULKANRESULT(VulkanRHI::vkCreatePipelineCache(Device->GetInstanceHandle(), &PipelineCacheInfo, VULKAN_CPU_ALLOCATOR, &PipelineCache));
	}
}

void FVulkanPipelineStateCacheManager::DestroyPipeline(FVulkanGfxPipeline* Pipeline)
{
	ensure(0);
/*
	if (Pipeline->Release() == 0)
	{
		const FVulkanGfxPipelineStateKey* Key = KeyToGfxPipelineMap.FindKey(Pipeline);
		check(Key);
		KeyToGfxPipelineMap.Remove(*Key);
	}*/
}

void FVulkanPipelineStateCacheManager::InitAndLoad(const TArray<FString>& CacheFilenames)
{
	if (GEnablePipelineCacheLoadCvar.GetValueOnAnyThread() == 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Not loading pipeline cache per r.Vulkan.PipelineCacheLoad=0"));
	}
	else
	{
		Load(CacheFilenames);
	}

	// Lazily create the cache in case the load failed
	if (PipelineCache == VK_NULL_HANDLE)
	{
		VkPipelineCacheCreateInfo PipelineCacheInfo;
		ZeroVulkanStruct(PipelineCacheInfo, VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
		VERIFYVULKANRESULT(VulkanRHI::vkCreatePipelineCache(Device->GetInstanceHandle(), &PipelineCacheInfo, VULKAN_CPU_ALLOCATOR, &PipelineCache));
	}
}

void FVulkanPipelineStateCacheManager::Save(const FString& CacheFilename)
{
	FScopeLock Lock(&InitializerToPipelineMapCS);

	// First save Device Cache
	size_t Size = 0;
	VERIFYVULKANRESULT(VulkanRHI::vkGetPipelineCacheData(Device->GetInstanceHandle(), PipelineCache, &Size, nullptr));
	// 16 is HeaderSize + HeaderVersion
	if (Size >= 16 + VK_UUID_SIZE)
	{
		TArray<uint8> DeviceCache;
		DeviceCache.AddUninitialized(Size);
		VkResult Result = VulkanRHI::vkGetPipelineCacheData(Device->GetInstanceHandle(), PipelineCache, &Size, DeviceCache.GetData());
		if (Result == VK_SUCCESS)
		{
			const VkPhysicalDeviceProperties& DeviceProperties = Device->GetDeviceProperties();

			FString BinaryCacheFilename = FString::Printf(TEXT("%s.%x.%x"), *CacheFilename, DeviceProperties.vendorID, DeviceProperties.deviceID);

			if (FFileHelper::SaveArrayToFile(DeviceCache, *BinaryCacheFilename))
			{
				UE_LOG(LogVulkanRHI, Display, TEXT("Saved device pipeline cache file '%s', %d bytes"), *BinaryCacheFilename, DeviceCache.Num());
			}
		}
		else if (Result == VK_INCOMPLETE || Result == VK_ERROR_OUT_OF_HOST_MEMORY)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Failed to get Vulkan pipeline cache data."));

			VulkanRHI::vkDestroyPipelineCache(Device->GetInstanceHandle(), PipelineCache, nullptr);
			VkPipelineCacheCreateInfo PipelineCacheInfo;
			ZeroVulkanStruct(PipelineCacheInfo, VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
			VERIFYVULKANRESULT(VulkanRHI::vkCreatePipelineCache(Device->GetInstanceHandle(), &PipelineCacheInfo, nullptr, &PipelineCache));
		}
		else
		{
			VERIFYVULKANRESULT(Result);
		}
	}

#if VULKAN_ENABLE_LRU_CACHE
	if (IsUsePipelineLRU())
	{
		// LRU cache file
		TArray<uint8> MemFile;
		FMemoryWriter Ar(MemFile);
		FVulkanLRUCacheFile File;
		File.Header.Version = FVulkanLRUCacheFile::LRU_CACHE_VERSION;
		File.Header.SizeOfPipelineSizes = (int32)sizeof(FPipelineSize);
		PipelineSizeList.GenerateValueArray(File.PipelineSizes);
		File.Save(Ar);

		const VkPhysicalDeviceProperties& DeviceProperties = Device->GetDeviceProperties();
		FString LruCacheFilename = FString::Printf(TEXT("%s.%x.%x.lru"), *CacheFilename, DeviceProperties.vendorID, DeviceProperties.deviceID);

		if (FFileHelper::SaveArrayToFile(MemFile, *LruCacheFilename))
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("Saved pipeline lru pipeline cache file '%s', %d hashes, %d bytes"), *LruCacheFilename, PipelineSizeList.Num(), MemFile.Num());
		}
	}
#endif

#if VULKAN_ENABLE_GENERIC_PIPELINE_CACHE_FILE
	{
		// Now the generic cache
		TArray<uint8> MemFile;
		FMemoryWriter Ar(MemFile);
		FVulkanPipelineStateCacheFile File;

		File.Header.Version = VERSION;
		File.Header.SizeOfGfxEntry = (int32)sizeof(FGfxPipelineEntry);
		File.Header.SizeOfComputeEntry = (int32)sizeof(FComputePipelineEntry);
		File.Header.UncompressedSize = 0;

		// Shader ucode cache
		File.ShaderCache = &ShaderCache.Data;

		// Then Gfx entries
		GfxPipelineEntries.GenerateValueArray(File.GfxPipelineEntries);

		// And Compute entries
		ComputePipelineEntries.GenerateValueArray(File.ComputePipelineEntries);

		File.Save(Ar);

		if (FFileHelper::SaveArrayToFile(MemFile, *CacheFilename))
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("Saved pipeline cache file '%s', %d Gfx Pipelines, %d Compute Pipelines, %d Microcodes, %d bytes"), *CacheFilename, GfxPipelineEntries.Num(), ComputePipelineEntries.Num(), ShaderCache.Data.Num(), MemFile.Num());
		}
	}

#endif
}

#if VULKAN_ENABLE_LRU_CACHE
FVulkanRHIGraphicsPipelineState* FVulkanPipelineStateCacheManager::CreateAndAdd(const FGraphicsPipelineStateInitializer& PSOInitializer, uint32 PSOInitializerHash, FGfxPipelineEntry* GfxEntry, uint32 ShaderHash)
#else
FVulkanRHIGraphicsPipelineState* FVulkanPipelineStateCacheManager::CreateAndAdd(const FGraphicsPipelineStateInitializer& PSOInitializer, uint32 PSOInitializerHash, FGfxPipelineEntry* GfxEntry)
#endif
{
	check(GfxEntry);

#if VULKAN_ENABLE_LRU_CACHE
	uint32 EntryHash = GfxEntry->GetEntryHash();
	FVulkanGfxPipeline* Pipeline = new FVulkanGfxPipeline(Device, EntryHash, ShaderHash);
	{
		FScopeLock ScopeLock(&GfxPipelineEntriesCS);
		GfxPipelineEntries.Add(EntryHash, GfxEntry);
	}
#else
	FVulkanGfxPipeline* Pipeline = new FVulkanGfxPipeline(Device);
	{
		FScopeLock ScopeLock(&GfxPipelineEntriesCS);
		GfxPipelineEntries.Add(GfxEntry->GetEntryHash(), GfxEntry);
	}
#endif
	

	// Create the pipeline
	double BeginTime = FPlatformTime::Seconds();
	CreateGfxPipelineFromEntry(GfxEntry, Pipeline);

	// Recover if we failed to create the pipeline.
	if (!Pipeline->Pipeline)
	{
		delete Pipeline;
		return nullptr;
	}

	Pipeline->CreateRuntimeObjects(PSOInitializer);
	double EndTime = FPlatformTime::Seconds();
	double Delta = EndTime - BeginTime;
	if (Delta > HitchTime)
	{
		UE_LOG(LogVulkanRHI, Verbose, TEXT("Hitchy gfx pipeline (%.3f ms)"), (float)(Delta * 1000.0));
	}

	FVulkanRHIGraphicsPipelineState* PipelineState = new FVulkanRHIGraphicsPipelineState(PSOInitializer, Pipeline);
	PipelineState->AddRef();

	{
		FScopeLock Lock(&InitializerToPipelineMapCS);
		InitializerToPipelineMap.Add(PSOInitializerHash, PipelineState);
	}

	return PipelineState;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FBlendAttachment& Attachment)
{
	// Modify VERSION if serialization changes
	Ar << Attachment.bBlend;
	Ar << Attachment.ColorBlendOp;
	Ar << Attachment.SrcColorBlendFactor;
	Ar << Attachment.DstColorBlendFactor;
	Ar << Attachment.AlphaBlendOp;
	Ar << Attachment.SrcAlphaBlendFactor;
	Ar << Attachment.DstAlphaBlendFactor;
	Ar << Attachment.ColorWriteMask;
	return Ar;
}

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FBlendAttachment::ReadFrom(const VkPipelineColorBlendAttachmentState& InState)
{
	bBlend =				InState.blendEnable != VK_FALSE;
	ColorBlendOp =			(uint8)InState.colorBlendOp;
	SrcColorBlendFactor =	(uint8)InState.srcColorBlendFactor;
	DstColorBlendFactor =	(uint8)InState.dstColorBlendFactor;
	AlphaBlendOp =			(uint8)InState.alphaBlendOp;
	SrcAlphaBlendFactor =	(uint8)InState.srcAlphaBlendFactor;
	DstAlphaBlendFactor =	(uint8)InState.dstAlphaBlendFactor;
	ColorWriteMask =		(uint8)InState.colorWriteMask;
}

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FBlendAttachment::WriteInto(VkPipelineColorBlendAttachmentState& Out) const
{
	Out.blendEnable =			bBlend ? VK_TRUE : VK_FALSE;
	Out.colorBlendOp =			(VkBlendOp)ColorBlendOp;
	Out.srcColorBlendFactor =	(VkBlendFactor)SrcColorBlendFactor;
	Out.dstColorBlendFactor =	(VkBlendFactor)DstColorBlendFactor;
	Out.alphaBlendOp =			(VkBlendOp)AlphaBlendOp;
	Out.srcAlphaBlendFactor =	(VkBlendFactor)SrcAlphaBlendFactor;
	Out.dstAlphaBlendFactor =	(VkBlendFactor)DstAlphaBlendFactor;
	Out.colorWriteMask =		(VkColorComponentFlags)ColorWriteMask;
}


void FVulkanPipelineStateCacheManager::FDescriptorSetLayoutBinding::ReadFrom(const VkDescriptorSetLayoutBinding& InState)
{
	Binding =			InState.binding;
	ensure(InState.descriptorCount == 1);
	//DescriptorCount =	InState.descriptorCount;
	DescriptorType =	InState.descriptorType;
	StageFlags =		InState.stageFlags;
}

void FVulkanPipelineStateCacheManager::FDescriptorSetLayoutBinding::WriteInto(VkDescriptorSetLayoutBinding& Out) const
{
	Out.binding = Binding;
	//Out.descriptorCount = DescriptorCount;
	Out.descriptorType = (VkDescriptorType)DescriptorType;
	Out.stageFlags = StageFlags;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCacheManager::FDescriptorSetLayoutBinding& Binding)
{
	// Modify VERSION if serialization changes
	Ar << Binding.Binding;
	//Ar << Binding.DescriptorCount;
	Ar << Binding.DescriptorType;
	Ar << Binding.StageFlags;
	return Ar;
}

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FVertexBinding::ReadFrom(const VkVertexInputBindingDescription& InState)
{
	Binding =	InState.binding;
	InputRate =	(uint16)InState.inputRate;
	Stride =	InState.stride;
}

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FVertexBinding::WriteInto(VkVertexInputBindingDescription& Out) const
{
	Out.binding =	Binding;
	Out.inputRate =	(VkVertexInputRate)InputRate;
	Out.stride =	Stride;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FVertexBinding& Binding)
{
	// Modify VERSION if serialization changes
	Ar << Binding.Stride;
	Ar << Binding.Binding;
	Ar << Binding.InputRate;
	return Ar;
}

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FVertexAttribute::ReadFrom(const VkVertexInputAttributeDescription& InState)
{
	Binding =	InState.binding;
	Format =	(uint32)InState.format;
	Location =	InState.location;
	Offset =	InState.offset;
}

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FVertexAttribute::WriteInto(VkVertexInputAttributeDescription& Out) const
{
	Out.binding =	Binding;
	Out.format =	(VkFormat)Format;
	Out.location =	Location;
	Out.offset =	Offset;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FVertexAttribute& Attribute)
{
	// Modify VERSION if serialization changes
	Ar << Attribute.Location;
	Ar << Attribute.Binding;
	Ar << Attribute.Format;
	Ar << Attribute.Offset;
	return Ar;
}

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FRasterizer::ReadFrom(const VkPipelineRasterizationStateCreateInfo& InState)
{
	PolygonMode =				InState.polygonMode;
	CullMode =					InState.cullMode;
	DepthBiasSlopeScale =		InState.depthBiasSlopeFactor;
	DepthBiasConstantFactor =	InState.depthBiasConstantFactor;
}

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FRasterizer::WriteInto(VkPipelineRasterizationStateCreateInfo& Out) const
{
	Out.polygonMode =				(VkPolygonMode)PolygonMode;
	Out.cullMode =					(VkCullModeFlags)CullMode;
	Out.frontFace =					VK_FRONT_FACE_CLOCKWISE;
	Out.depthClampEnable =			VK_FALSE;
	Out.depthBiasEnable =			DepthBiasConstantFactor != 0.0f ? VK_TRUE : VK_FALSE;
	Out.rasterizerDiscardEnable =	VK_FALSE;
	Out.depthBiasSlopeFactor =		DepthBiasSlopeScale;
	Out.depthBiasConstantFactor =	DepthBiasConstantFactor;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FRasterizer& Rasterizer)
{
	// Modify VERSION if serialization changes
	Ar << Rasterizer.PolygonMode;
	Ar << Rasterizer.CullMode;
	Ar << Rasterizer.DepthBiasSlopeScale;
	Ar << Rasterizer.DepthBiasConstantFactor;
	return Ar;
}

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FDepthStencil::ReadFrom(const VkPipelineDepthStencilStateCreateInfo& InState)
{
	DepthCompareOp =			(uint8)InState.depthCompareOp;
	bDepthTestEnable =			InState.depthTestEnable != VK_FALSE;
	bDepthWriteEnable =			InState.depthWriteEnable != VK_FALSE;
	bDepthBoundsTestEnable =	InState.depthBoundsTestEnable != VK_FALSE;
	bStencilTestEnable =		InState.stencilTestEnable != VK_FALSE;
	FrontFailOp =				(uint8)InState.front.failOp;
	FrontPassOp =				(uint8)InState.front.passOp;
	FrontDepthFailOp =			(uint8)InState.front.depthFailOp;
	FrontCompareOp =			(uint8)InState.front.compareOp;
	FrontCompareMask =			(uint8)InState.front.compareMask;
	FrontWriteMask =			InState.front.writeMask;
	FrontReference =			InState.front.reference;
	BackFailOp =				(uint8)InState.back.failOp;
	BackPassOp =				(uint8)InState.back.passOp;
	BackDepthFailOp =			(uint8)InState.back.depthFailOp;
	BackCompareOp =				(uint8)InState.back.compareOp;
	BackCompareMask =			(uint8)InState.back.compareMask;
	BackWriteMask =				InState.back.writeMask;
	BackReference =				InState.back.reference;
}

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FDepthStencil::WriteInto(VkPipelineDepthStencilStateCreateInfo& Out) const
{
	Out.depthCompareOp =		(VkCompareOp)DepthCompareOp;
	Out.depthTestEnable =		bDepthTestEnable;
	Out.depthWriteEnable =		bDepthWriteEnable;
	Out.depthBoundsTestEnable =	bDepthBoundsTestEnable;
	Out.stencilTestEnable =		bStencilTestEnable;
	Out.front.failOp =			(VkStencilOp)FrontFailOp;
	Out.front.passOp =			(VkStencilOp)FrontPassOp;
	Out.front.depthFailOp =		(VkStencilOp)FrontDepthFailOp;
	Out.front.compareOp =		(VkCompareOp)FrontCompareOp;
	Out.front.compareMask =		FrontCompareMask;
	Out.front.writeMask =		FrontWriteMask;
	Out.front.reference =		FrontReference;
	Out.back.failOp =			(VkStencilOp)BackFailOp;
	Out.back.passOp =			(VkStencilOp)BackPassOp;
	Out.back.depthFailOp =		(VkStencilOp)BackDepthFailOp;
	Out.back.compareOp =		(VkCompareOp)BackCompareOp;
	Out.back.writeMask =		BackWriteMask;
	Out.back.compareMask =		BackCompareMask;
	Out.back.reference =		BackReference;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FDepthStencil& DepthStencil)
{
	// Modify VERSION if serialization changes
	Ar << DepthStencil.DepthCompareOp;
	Ar << DepthStencil.bDepthTestEnable;
	Ar << DepthStencil.bDepthWriteEnable;
	Ar << DepthStencil.bDepthBoundsTestEnable;
	Ar << DepthStencil.bStencilTestEnable;
	Ar << DepthStencil.FrontFailOp;
	Ar << DepthStencil.FrontPassOp;
	Ar << DepthStencil.FrontDepthFailOp;
	Ar << DepthStencil.FrontCompareOp;
	Ar << DepthStencil.FrontCompareMask;
	Ar << DepthStencil.FrontWriteMask;
	Ar << DepthStencil.FrontReference;
	Ar << DepthStencil.BackFailOp;
	Ar << DepthStencil.BackPassOp;
	Ar << DepthStencil.BackDepthFailOp;
	Ar << DepthStencil.BackCompareOp;
	Ar << DepthStencil.BackCompareMask;
	Ar << DepthStencil.BackWriteMask;
	Ar << DepthStencil.BackReference;
	return Ar;
}

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FRenderTargets::FAttachmentRef::ReadFrom(const VkAttachmentReference& InState)
{
	Attachment =	InState.attachment;
	Layout =		(uint64)InState.layout;
}

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FRenderTargets::FAttachmentRef::WriteInto(VkAttachmentReference& Out) const
{
	Out.attachment =	Attachment;
	Out.layout =		(VkImageLayout)Layout;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FRenderTargets::FAttachmentRef& AttachmentRef)
{
	// Modify VERSION if serialization changes
	Ar << AttachmentRef.Attachment;
	Ar << AttachmentRef.Layout;
	return Ar;
}

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FRenderTargets::FAttachmentDesc::ReadFrom(const VkAttachmentDescription &InState)
{
	Format =			(uint32)InState.format;
	Flags =				(uint8)InState.flags;
	Samples =			(uint8)InState.samples;
	LoadOp =			(uint8)InState.loadOp;
	StoreOp =			(uint8)InState.storeOp;
	StencilLoadOp =		(uint8)InState.stencilLoadOp;
	StencilStoreOp =	(uint8)InState.stencilStoreOp;
	InitialLayout =		(uint64)InState.initialLayout;
	FinalLayout =		(uint64)InState.finalLayout;
}

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FRenderTargets::FAttachmentDesc::WriteInto(VkAttachmentDescription& Out) const
{
	Out.format =			(VkFormat)Format;
	Out.flags =				Flags;
	Out.samples =			(VkSampleCountFlagBits)Samples;
	Out.loadOp =			(VkAttachmentLoadOp)LoadOp;
	Out.storeOp =			(VkAttachmentStoreOp)StoreOp;
	Out.stencilLoadOp =		(VkAttachmentLoadOp)StencilLoadOp;
	Out.stencilStoreOp =	(VkAttachmentStoreOp)StencilStoreOp;
	Out.initialLayout =		(VkImageLayout)InitialLayout;
	Out.finalLayout =		(VkImageLayout)FinalLayout;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FRenderTargets::FAttachmentDesc& AttachmentDesc)
{
	// Modify VERSION if serialization changes
	Ar << AttachmentDesc.Format;
	Ar << AttachmentDesc.Flags;
	Ar << AttachmentDesc.Samples;
	Ar << AttachmentDesc.LoadOp;
	Ar << AttachmentDesc.StoreOp;
	Ar << AttachmentDesc.StencilLoadOp;
	Ar << AttachmentDesc.StencilStoreOp;
	Ar << AttachmentDesc.InitialLayout;
	Ar << AttachmentDesc.FinalLayout;

	return Ar;
}

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FRenderTargets::ReadFrom(const FVulkanRenderTargetLayout& RTLayout)
{
	NumAttachments =			RTLayout.NumAttachmentDescriptions;
	NumColorAttachments =		RTLayout.NumColorAttachments;

	bHasDepthStencil =			RTLayout.bHasDepthStencil != 0;
	bHasResolveAttachments =	RTLayout.bHasResolveAttachments != 0;
	NumUsedClearValues =		RTLayout.NumUsedClearValues;

	RenderPassCompatibleHash =	RTLayout.GetRenderPassCompatibleHash();

	Extent3D.X = RTLayout.Extent.Extent3D.width;
	Extent3D.Y = RTLayout.Extent.Extent3D.height;
	Extent3D.Z = RTLayout.Extent.Extent3D.depth;

	auto CopyAttachmentRefs = [&](TArray<FGfxPipelineEntry::FRenderTargets::FAttachmentRef>& Dest, const VkAttachmentReference* Source, uint32 Count)
	{
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			FGfxPipelineEntry::FRenderTargets::FAttachmentRef* New = new(Dest) FGfxPipelineEntry::FRenderTargets::FAttachmentRef;
			New->ReadFrom(Source[Index]);
		}
	};
	CopyAttachmentRefs(ColorAttachments, RTLayout.ColorReferences, ARRAY_COUNT(RTLayout.ColorReferences));
	CopyAttachmentRefs(ResolveAttachments, RTLayout.ResolveReferences, ARRAY_COUNT(RTLayout.ResolveReferences));
	DepthStencil.ReadFrom(RTLayout.DepthStencilReference);

	Descriptions.AddZeroed(ARRAY_COUNT(RTLayout.Desc));
	for (int32 Index = 0; Index < ARRAY_COUNT(RTLayout.Desc); ++Index)
	{
		Descriptions[Index].ReadFrom(RTLayout.Desc[Index]);
	}
}

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FRenderTargets::WriteInto(FVulkanRenderTargetLayout& Out) const
{
	Out.NumAttachmentDescriptions =	NumAttachments;
	Out.NumColorAttachments =		NumColorAttachments;

	Out.bHasDepthStencil =			bHasDepthStencil;
	Out.bHasResolveAttachments =	bHasResolveAttachments;
	Out.NumUsedClearValues =		NumUsedClearValues;

	ensure(0);
	Out.RenderPassCompatibleHash =	RenderPassCompatibleHash;

	Out.Extent.Extent3D.width =		Extent3D.X;
	Out.Extent.Extent3D.height =	Extent3D.Y;
	Out.Extent.Extent3D.depth =		Extent3D.Z;

	auto CopyAttachmentRefs = [&](const TArray<FGfxPipelineEntry::FRenderTargets::FAttachmentRef>& Source, VkAttachmentReference* Dest, uint32 Count)
	{
		for (uint32 Index = 0; Index < Count; ++Index, ++Dest)
		{
			Source[Index].WriteInto(*Dest);
		}
	};
	CopyAttachmentRefs(ColorAttachments, Out.ColorReferences, ARRAY_COUNT(Out.ColorReferences));
	CopyAttachmentRefs(ResolveAttachments, Out.ResolveReferences, ARRAY_COUNT(Out.ResolveReferences));
	DepthStencil.WriteInto(Out.DepthStencilReference);

	for (int32 Index = 0; Index < ARRAY_COUNT(Out.Desc); ++Index)
	{
		Descriptions[Index].WriteInto(Out.Desc[Index]);
	}
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCacheManager::FGfxPipelineEntry::FRenderTargets& RTs)
{
	// Modify VERSION if serialization changes
	Ar << RTs.NumAttachments;
	Ar << RTs.NumColorAttachments;
	Ar << RTs.NumUsedClearValues;
	Ar << RTs.ColorAttachments;
	Ar << RTs.ResolveAttachments;
	Ar << RTs.DepthStencil;

	Ar << RTs.Descriptions;

	Ar << RTs.bHasDepthStencil;
	Ar << RTs.bHasResolveAttachments;
	Ar << RTs.RenderPassCompatibleHash;
	Ar << RTs.Extent3D;

	return Ar;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCacheManager::FGfxPipelineEntry& Entry)
{
	// Modify VERSION if serialization changes
	Ar << Entry.VertexInputKey;
	Ar << Entry.RasterizationSamples;
	Ar << Entry.Topology;

	Ar << Entry.ColorAttachmentStates;

	Ar << Entry.DescriptorSetLayoutBindings;

	Ar << Entry.VertexBindings;
	Ar << Entry.VertexAttributes;
	Ar << Entry.Rasterizer;

	Ar << Entry.DepthStencil;

	for (int32 Index = 0; Index < ARRAY_COUNT(Entry.ShaderMicrocodes); ++Index)
	{
		Ar << Entry.ShaderHashes[Index];
	}

	Ar << Entry.RenderTargets;

	return Ar;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCacheManager::FGfxPipelineEntry* Entry)
{
	return Ar << (*Entry);
}

#if VULKAN_ENABLE_LRU_CACHE	
FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCacheManager::FPipelineSize& PS)
{
	Ar << PS.ShaderHash;
	Ar << PS.PipelineSize;

	return Ar;
}
#endif

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCacheManager::FComputePipelineEntry& Entry)
{
	// Modify VERSION if serialization changes
	Ar << Entry.ShaderHash;

	Ar << Entry.DescriptorSetLayoutBindings;

	return Ar;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCacheManager::FComputePipelineEntry* Entry)
{
	return Ar << (*Entry);
}

uint32 FVulkanPipelineStateCacheManager::FGfxPipelineEntry::GetEntryHash(uint32 Crc /*= 0*/)
{
	TArray<uint8> MemFile;
	FMemoryWriter Ar(MemFile);
	Ar << this;

	uint32 Result = FCrc::MemCrc32(MemFile.GetData(), MemFile.GetTypeSize() * MemFile.Num(), Crc);

#if VULKAN_SUPPORTS_COLOR_CONVERSIONS
	for (uint32 Index = 0; Index < MaxImmutableSamplers; ++Index)
	{
		Result = FCrc::MemCrc32(&ImmutableSamplers[Index], sizeof(SIZE_T), Result);
	}
#endif

	return Result;
}

void FVulkanPipelineStateCacheManager::CreateGfxPipelineFromEntry(const FGfxPipelineEntry* GfxEntry, FVulkanGfxPipeline* Pipeline)
{
	// Pipeline
	VkGraphicsPipelineCreateInfo PipelineInfo;
	ZeroVulkanStruct(PipelineInfo, VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
	PipelineInfo.layout = GfxEntry->Layout->GetPipelineLayout();

	// Color Blend
	VkPipelineColorBlendStateCreateInfo CBInfo;
	ZeroVulkanStruct(CBInfo, VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
	CBInfo.attachmentCount = GfxEntry->ColorAttachmentStates.Num();
	VkPipelineColorBlendAttachmentState BlendStates[MaxSimultaneousRenderTargets];
	FMemory::Memzero(BlendStates);
	for (int32 Index = 0; Index < GfxEntry->ColorAttachmentStates.Num(); ++Index)
	{
		GfxEntry->ColorAttachmentStates[Index].WriteInto(BlendStates[Index]);
	}
	CBInfo.pAttachments = BlendStates;
	CBInfo.blendConstants[0] = 1.0f;
	CBInfo.blendConstants[1] = 1.0f;
	CBInfo.blendConstants[2] = 1.0f;
	CBInfo.blendConstants[3] = 1.0f;

	// Viewport
	VkPipelineViewportStateCreateInfo VPInfo;
	ZeroVulkanStruct(VPInfo, VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
	VPInfo.viewportCount = 1;
	VPInfo.scissorCount = 1;

	// Multisample
	VkPipelineMultisampleStateCreateInfo MSInfo;
	ZeroVulkanStruct(MSInfo, VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
	MSInfo.rasterizationSamples = (VkSampleCountFlagBits)FMath::Max(1u, GfxEntry->RasterizationSamples);

	VkPipelineShaderStageCreateInfo ShaderStages[ShaderStage::NumStages];
	FMemory::Memzero(ShaderStages);
	PipelineInfo.stageCount = 0;
	PipelineInfo.pStages = ShaderStages;
	for (int32 ShaderStage = 0; ShaderStage < ShaderStage::NumStages; ++ShaderStage)
	{
		if (GfxEntry->ShaderMicrocodes[ShaderStage] == nullptr)
		{
			continue;
		}
		const ShaderStage::EStage CurrStage = (ShaderStage::EStage)ShaderStage;

		ShaderStages[PipelineInfo.stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		ShaderStages[PipelineInfo.stageCount].stage = UEFrequencyToVKStageBit(ShaderStage::GetFrequencyForGfxStage(CurrStage));
		ShaderStages[PipelineInfo.stageCount].module = GfxEntry->ShaderModules[CurrStage];
		ShaderStages[PipelineInfo.stageCount].pName = "main";
		PipelineInfo.stageCount++;
	}

	check(PipelineInfo.stageCount != 0);

	// Vertex Input. The structure is mandatory even without vertex attributes.
	VkPipelineVertexInputStateCreateInfo VBInfo;
	ZeroVulkanStruct(VBInfo, VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
	TArray<VkVertexInputBindingDescription> VBBindings;
	for (const FGfxPipelineEntry::FVertexBinding& SourceBinding : GfxEntry->VertexBindings)
	{
		VkVertexInputBindingDescription* Binding = new(VBBindings) VkVertexInputBindingDescription;
		SourceBinding.WriteInto(*Binding);
	}
	VBInfo.vertexBindingDescriptionCount = VBBindings.Num();
	VBInfo.pVertexBindingDescriptions = VBBindings.GetData();
	TArray<VkVertexInputAttributeDescription> VBAttributes;
	for (const FGfxPipelineEntry::FVertexAttribute& SourceAttr : GfxEntry->VertexAttributes)
	{
		VkVertexInputAttributeDescription* Attr = new(VBAttributes) VkVertexInputAttributeDescription;
		SourceAttr.WriteInto(*Attr);
	}
	VBInfo.vertexAttributeDescriptionCount = VBAttributes.Num();
	VBInfo.pVertexAttributeDescriptions = VBAttributes.GetData();
	PipelineInfo.pVertexInputState = &VBInfo;

	PipelineInfo.pColorBlendState = &CBInfo;
	PipelineInfo.pMultisampleState = &MSInfo;
	PipelineInfo.pViewportState = &VPInfo;

	PipelineInfo.renderPass = GfxEntry->RenderPass->GetHandle();
	PipelineInfo.subpass = 0;

	VkPipelineInputAssemblyStateCreateInfo InputAssembly;
	ZeroVulkanStruct(InputAssembly, VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
	InputAssembly.topology = (VkPrimitiveTopology)GfxEntry->Topology;

	PipelineInfo.pInputAssemblyState = &InputAssembly;

	VkPipelineRasterizationStateCreateInfo RasterizerState;
	FVulkanRasterizerState::ResetCreateInfo(RasterizerState);
	GfxEntry->Rasterizer.WriteInto(RasterizerState);

	VkPipelineDepthStencilStateCreateInfo DepthStencilState;
	ZeroVulkanStruct(DepthStencilState, VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
	GfxEntry->DepthStencil.WriteInto(DepthStencilState);

	PipelineInfo.pRasterizationState = &RasterizerState;
	PipelineInfo.pDepthStencilState = &DepthStencilState;

	VkPipelineDynamicStateCreateInfo DynamicState;
	ZeroVulkanStruct(DynamicState, VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
	VkDynamicState DynamicStatesEnabled[VK_DYNAMIC_STATE_RANGE_SIZE];
	DynamicState.pDynamicStates = DynamicStatesEnabled;
	FMemory::Memzero(DynamicStatesEnabled);
	DynamicStatesEnabled[DynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
	DynamicStatesEnabled[DynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;
	DynamicStatesEnabled[DynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
	DynamicStatesEnabled[DynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_BOUNDS;

	PipelineInfo.pDynamicState = &DynamicState;

	//#todo-rco: Fix me
	double BeginTime = FPlatformTime::Seconds();
#if VULKAN_ENABLE_LRU_CACHE
	FPipelineSize** Found = PipelineSizeList.Find(Pipeline->ShaderHash);
	size_t PreSize = 0, AfterSize = 0;
	if (Found)
	{
		Pipeline->PipelineCacheSize = (*Found)->PipelineSize;
	}
	else
	{
		VulkanRHI::vkGetPipelineCacheData(Device->GetInstanceHandle(), PipelineCache, &PreSize, nullptr);
	}

	VkResult Result = VulkanRHI::vkCreateGraphicsPipelines(Device->GetInstanceHandle(), PipelineCache, 1, &PipelineInfo, VULKAN_CPU_ALLOCATOR, &Pipeline->Pipeline);

	if (!Found && Result == VK_SUCCESS)
	{
		VulkanRHI::vkGetPipelineCacheData(Device->GetInstanceHandle(), PipelineCache, &AfterSize, nullptr);
		uint32 Diff = AfterSize - PreSize;
		FPipelineSize* PipelineSize = new FPipelineSize();
		PipelineSize->ShaderHash = Pipeline->ShaderHash;
		PipelineSize->PipelineSize = Diff;
		PipelineSizeList.Add(Pipeline->ShaderHash, PipelineSize);
		Pipeline->PipelineCacheSize = Diff;
	}
#else
	VkResult Result = VulkanRHI::vkCreateGraphicsPipelines(Device->GetInstanceHandle(), PipelineCache, 1, &PipelineInfo, VULKAN_CPU_ALLOCATOR, &Pipeline->Pipeline);
#endif

	if (Result != VK_SUCCESS)
	{
		UE_LOG(LogVulkanRHI, Error, TEXT("Failed to create graphics pipeline."));
		Pipeline->Pipeline = VkPipeline(0);
		return;
	}

	double EndTime = FPlatformTime::Seconds();
	double Delta = EndTime - BeginTime;
	if (Delta > HitchTime)
	{
		UE_LOG(LogVulkanRHI, Verbose, TEXT("Hitchy gfx pipeline key CS (%.3f ms)"), (float)(Delta * 1000.0));
	}

	INC_DWORD_STAT(STAT_VulkanNumPSOs);

	Pipeline->Layout = GfxEntry->Layout;
}

void FVulkanPipelineStateCacheManager::CreatGfxEntryRuntimeObjects(FGfxPipelineEntry* GfxEntry)
{
	{
		// Descriptor Set Layouts
		check(!GfxEntry->Layout);

		FVulkanDescriptorSetsLayoutInfo Info;
		for (int32 SetIndex = 0; SetIndex < GfxEntry->DescriptorSetLayoutBindings.Num(); ++SetIndex)
		{
			for (int32 Index = 0; Index < GfxEntry->DescriptorSetLayoutBindings[SetIndex].Num(); ++Index)
			{
				VkDescriptorSetLayoutBinding Binding;
				Binding.descriptorCount = 1;
				Binding.pImmutableSamplers = nullptr;
				GfxEntry->DescriptorSetLayoutBindings[SetIndex][Index].WriteInto(Binding);
				Info.AddDescriptor(SetIndex, Binding);
			}
		}

		ensure(0);
/*
		GfxEntry->Layout = FindOrAddLayout(Info, true);
*/
	}

	{
		// Shaders
		for (int32 Index = 0; Index < ARRAY_COUNT(GfxEntry->ShaderMicrocodes); ++Index)
		{
			if (GfxEntry->ShaderMicrocodes[Index] != nullptr)
			{
				VkShaderModuleCreateInfo ModuleCreateInfo;
				ZeroVulkanStruct(ModuleCreateInfo, VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
				ModuleCreateInfo.codeSize = GfxEntry->ShaderMicrocodes[Index]->Num();
				ModuleCreateInfo.pCode = (uint32*)GfxEntry->ShaderMicrocodes[Index]->GetData();
				VERIFYVULKANRESULT(VulkanRHI::vkCreateShaderModule(Device->GetInstanceHandle(), &ModuleCreateInfo, VULKAN_CPU_ALLOCATOR, &GfxEntry->ShaderModules[Index]));
			}
		}
	}

	{
		ensure(0);
/*
		// Render Pass
		FVulkanRenderTargetLayout RTLayout;
		GfxEntry->RenderTargets.WriteInto(RTLayout);
		GfxEntry->RenderPass = Device->GetImmediateContext().PrepareRenderPassForPSOCreation(RTLayout, GfxEntry->Layout->GetDescriptorSetsLayout().RemappingInfo.InputAttachmentData);
*/
	}

	GfxEntry->bLoaded = true;
}


void FVulkanPipelineStateCacheManager::DestroyCache()
{
	VkDevice DeviceHandle = Device->GetInstanceHandle();

	// Graphics
	{
		for (auto Pair : InitializerToPipelineMap)
		{
			FVulkanRHIGraphicsPipelineState* Pipeline = Pair.Value;
			//when DestroyCache is called as part of r.Vulkan.RebuildPipelineCache, a pipeline can still be referenced by FVulkanPendingGfxState
			ensure(GIsRHIInitialized || (!GIsRHIInitialized && Pipeline->GetRefCount() == 1));
			Pipeline->Release();
		}
		InitializerToPipelineMap.Reset();

		for (auto& Pair : GfxPipelineEntries)
		{
			FGfxPipelineEntry* Entry = Pair.Value;
			Entry->RenderPass = nullptr;
			if (Entry->bLoaded)
			{
				for (int32 Index = 0; Index < ARRAY_COUNT(Entry->ShaderModules); ++Index)
				{
					if (Entry->ShaderModules[Index] != VK_NULL_HANDLE)
					{
						VulkanRHI::vkDestroyShaderModule(DeviceHandle, Entry->ShaderModules[Index], VULKAN_CPU_ALLOCATOR);
					}
				}
				Entry->bLoaded = false;
			}
			delete Entry;
		}
		GfxPipelineEntries.Reset();

#if VULKAN_ENABLE_LRU_CACHE
		for (auto& Pair : PipelineSizeList)
		{
			FPipelineSize* Entry = Pair.Value;
			delete Entry;
		}
		PipelineSizeList.Reset();
		PipelineLRU.Empty();
#endif

		// This map can simply be cleared as InitializerToPipelineMap already decreased the refcount of the pipeline objects	
		{
			FScopeLock Lock(&GShaderHashToGfxEntriesMapCS);
			ShaderHashToGfxPipelineMap.Reset();
		}
	}

	// Compute
	{
		for (auto Pair : ComputeEntryHashToPipelineMap)
		{
			FVulkanComputePipeline* Pipeline = Pair.Value;
			//when DestroyCache is called as part of r.Vulkan.RebuildPipelineCache, a pipeline can still be referenced by FVulkanPendingComputeState
			ensure(GIsRHIInitialized || (!GIsRHIInitialized && Pipeline->GetRefCount() == 1));
			Pipeline->Release();
		}
		ComputeEntryHashToPipelineMap.Reset();
		ComputeShaderToPipelineMap.Reset();

		for (auto& Pair : ComputePipelineEntries)
		{
			FComputePipelineEntry* Entry = Pair.Value;
			if (Entry->bLoaded)
			{
				if (Entry->ShaderModule != VK_NULL_HANDLE)
				{
					VulkanRHI::vkDestroyShaderModule(DeviceHandle, Entry->ShaderModule, VULKAN_CPU_ALLOCATOR);
				}
				Entry->bLoaded = false;
			}
			delete Entry;
		}
		ComputePipelineEntries.Reset();
	}
}

void FVulkanPipelineStateCacheManager::RebuildCache()
{
	UE_LOG(LogVulkanRHI, Warning, TEXT("Rebuilding pipeline cache; ditching %d entries"), GfxPipelineEntries.Num() + ComputePipelineEntries.Num());

	if (IsInGameThread())
	{
		FlushRenderingCommands();
	}
	DestroyCache();
}

FVulkanPipelineStateCacheManager::FShaderHashes::FShaderHashes(const FGraphicsPipelineStateInitializer& PSOInitializer)
{
	Stages[ShaderStage::Vertex] = GetShaderHash<FRHIVertexShader, FVulkanVertexShader>(PSOInitializer.BoundShaderState.VertexShaderRHI);
	Stages[ShaderStage::Pixel] = GetShaderHash<FRHIPixelShader, FVulkanPixelShader>(PSOInitializer.BoundShaderState.PixelShaderRHI);
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
	Stages[ShaderStage::Geometry] = GetShaderHash<FRHIGeometryShader, FVulkanGeometryShader>(PSOInitializer.BoundShaderState.GeometryShaderRHI);
#endif
	//Stages[ShaderStage::Hull] = GetShaderHash<FRHIHullShader, FVulkanHullShader>(PSOInitializer.BoundShaderState.HullShaderRHI);
	//Stages[ShaderStage::Domain] = GetShaderHash<FRHIDomainShader, FVulkanDomainShader>(PSOInitializer.BoundShaderState.DomainShaderRHI);
	Finalize();
}

FVulkanPipelineStateCacheManager::FShaderHashes::FShaderHashes()
{
	FMemory::Memzero(Stages);
	Hash = 0;
}

#if VULKAN_ENABLE_LRU_CACHE
inline FVulkanLayout* FVulkanPipelineStateCacheManager::FindOrAddLayoutForGfx(uint32 PSOHash, const FVulkanDescriptorSetsLayoutInfo& DescriptorSetLayoutInfo, bool bGfxLayout)
{
	FScopeLock Lock(&LayoutMapCS);
	if (FVulkanLayout** FoundLayout = LayoutMapGfx.Find(PSOHash))
	{
		check(bGfxLayout == (*FoundLayout)->IsGfxLayout());
		return *FoundLayout;
	}

	FVulkanLayout* Layout = nullptr;

	if (bGfxLayout)
	{
		Layout = new FVulkanGfxLayout(Device);
	}
	else
	{
		Layout = new FVulkanComputeLayout(Device);
	}

	Layout->DescriptorSetLayout.CopyFrom(DescriptorSetLayoutInfo);
	Layout->Compile();

	LayoutMapGfx.Add(PSOHash, Layout);
	return Layout;
}
#endif

inline FVulkanLayout* FVulkanPipelineStateCacheManager::FindOrAddLayout(const FVulkanDescriptorSetsLayoutInfo& DescriptorSetLayoutInfo, bool bGfxLayout)
{
	FScopeLock Lock(&LayoutMapCS);
	if (FVulkanLayout** FoundLayout = LayoutMap.Find(DescriptorSetLayoutInfo))
	{
		check(bGfxLayout == (*FoundLayout)->IsGfxLayout());
		return *FoundLayout;
	}

	FVulkanLayout* Layout = nullptr;

	if (bGfxLayout)
	{
		Layout = new FVulkanGfxLayout(Device);
	}
	else
	{
		Layout = new FVulkanComputeLayout(Device);
	}
	
	Layout->DescriptorSetLayout.CopyFrom(DescriptorSetLayoutInfo);
	Layout->Compile();

	LayoutMap.Add(Layout->DescriptorSetLayout, Layout);
	return Layout;
}

#if VULKAN_ENABLE_LRU_CACHE
FVulkanGfxLayout* FVulkanPipelineStateCacheManager::GetOrGenerateGfxLayout(const FGraphicsPipelineStateInitializer& PSOInitializer,
	FVulkanShader** OutShaders, FVulkanVertexInputStateInfo& OutVertexInputState, uint32 PSOHash)
#else
FVulkanGfxLayout* FVulkanPipelineStateCacheManager::GetOrGenerateGfxLayout(const FGraphicsPipelineStateInitializer& PSOInitializer,
	FVulkanShader** OutShaders, FVulkanVertexInputStateInfo& OutVertexInputState)
#endif
{
	const FBoundShaderStateInput& BSI = PSOInitializer.BoundShaderState;

	FVulkanVertexShader* VS = ResourceCast(BSI.VertexShaderRHI);
	const FVulkanShaderHeader& VSHeader = VS->GetCodeHeader();
	OutShaders[ShaderStage::Vertex] = VS;
	OutVertexInputState.Generate(ResourceCast(PSOInitializer.BoundShaderState.VertexDeclarationRHI), VSHeader.InOutMask);

	FUniformBufferGatherInfo UBGatherInfo;

	// First pass to gather uniform buffer info
	FVulkanDescriptorSetsLayoutInfo DescriptorSetLayoutInfo;
	DescriptorSetLayoutInfo.ProcessBindingsForStage(VK_SHADER_STAGE_VERTEX_BIT, ShaderStage::Vertex, VSHeader, UBGatherInfo);

	FVulkanPixelShader* PS = nullptr;
	if (BSI.PixelShaderRHI)
	{
		PS = ResourceCast(BSI.PixelShaderRHI);
	}
	else if (GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1)
	{
		// Some mobile devices expect PS stage (S7 Adreno)
		PS = ResourceCast(TShaderMapRef<FNULLPS>(GetGlobalShaderMap(GMaxRHIFeatureLevel))->GetPixelShader());
	}

	if (PS)
	{
		OutShaders[ShaderStage::Pixel] = PS;
		const FVulkanShaderHeader& PSHeader = PS->GetCodeHeader();
		DescriptorSetLayoutInfo.ProcessBindingsForStage(VK_SHADER_STAGE_FRAGMENT_BIT, ShaderStage::Pixel, PSHeader, UBGatherInfo);
	}

	if (BSI.GeometryShaderRHI)
	{
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
		FVulkanGeometryShader* GS = ResourceCast(BSI.GeometryShaderRHI);
		OutShaders[ShaderStage::Geometry] = GS;
		const FVulkanShaderHeader& GSHeader = GS->GetCodeHeader();
		DescriptorSetLayoutInfo.ProcessBindingsForStage(VK_SHADER_STAGE_GEOMETRY_BIT, ShaderStage::Geometry, GSHeader, UBGatherInfo);
#else
		ensureMsgf(0, TEXT("Geometry not supported!"));
#endif
	}

	if (BSI.HullShaderRHI)
	{
		ensureMsgf(0, TEXT("Tessellation not supported yet!"));
		/*
		// Can't have Hull w/o Domain
		check(BSI.DomainShaderRHI);
		FVulkanHullShader* HS = ResourceCast(BSI.HullShaderRHI);
		FVulkanDomainShader* DS = ResourceCast(BSI.DomainShaderRHI);
		Shaders[ShaderStage::Hull] = HS;
		Shaders[ShaderStage::Domain] = DS;
		const FVulkanCodeHeader& HSHeader = HS->GetCodeHeader();
		const FVulkanCodeHeader& DSHeader = DS->GetCodeHeader();
		DescriptorSetLayoutInfo.AddBindingsForStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, ShaderStage::Hull, HSHeader);
		DescriptorSetLayoutInfo.AddBindingsForStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, ShaderStage::Domain, DSHeader);
		*/
	}
	else
	{
		// Can't have Domain w/o Hull
		check(BSI.DomainShaderRHI == nullptr);
	}

	// Second pass
	const int32 NumImmutableSamplers = PSOInitializer.ImmutableSamplerState.ImmutableSamplers.Num();
	TArrayView<const FSamplerStateRHIParamRef> ImmutableSamplers(NumImmutableSamplers > 0 ? &PSOInitializer.ImmutableSamplerState.ImmutableSamplers[0] : nullptr, NumImmutableSamplers);
	DescriptorSetLayoutInfo.FinalizeBindings<false>(UBGatherInfo, ImmutableSamplers);

#if VULKAN_ENABLE_LRU_CACHE
	FVulkanLayout* Layout = FindOrAddLayoutForGfx(PSOHash, DescriptorSetLayoutInfo, true);
	FVulkanGfxLayout* GfxLayout = (FVulkanGfxLayout*)Layout;
	if (!GfxLayout->GfxPipelineDescriptorInfo.IsInitialized())
	{
		GfxLayout->GfxPipelineDescriptorInfo.Initialize(Layout->GetDescriptorSetsLayout().RemappingInfo, OutShaders);
	}
#else
	FVulkanLayout* Layout = FindOrAddLayout(DescriptorSetLayoutInfo, true);
	FVulkanGfxLayout* GfxLayout = (FVulkanGfxLayout*)Layout;
	check(!GfxLayout->GfxPipelineDescriptorInfo.IsInitialized());
	GfxLayout->GfxPipelineDescriptorInfo.Initialize(Layout->GetDescriptorSetsLayout().RemappingInfo, OutShaders);
#endif
	return GfxLayout;
}

#if VULKAN_ENABLE_LRU_CACHE
FVulkanPipelineStateCacheManager::FGfxPipelineEntry* FVulkanPipelineStateCacheManager::CreateGfxEntry(const FGraphicsPipelineStateInitializer& PSOInitializer, uint32 PSOHash)
#else
FVulkanPipelineStateCacheManager::FGfxPipelineEntry* FVulkanPipelineStateCacheManager::CreateGfxEntry(const FGraphicsPipelineStateInitializer& PSOInitializer)
#endif
{
	FGfxPipelineEntry* OutGfxEntry = new FGfxPipelineEntry();

	FVulkanShader* Shaders[ShaderStage::NumStages];
	FMemory::Memzero(Shaders);

	OutGfxEntry->RenderPass = Device->GetImmediateContext().PrepareRenderPassForPSOCreation(PSOInitializer, OutGfxEntry->Layout->GetDescriptorSetsLayout().RemappingInfo.InputAttachmentData);

	FVulkanVertexInputStateInfo VertexInputState;
#if VULKAN_ENABLE_LRU_CACHE
	OutGfxEntry->Layout = GetOrGenerateGfxLayout(PSOInitializer, Shaders, VertexInputState, PSOHash);
#else
	OutGfxEntry->Layout = GetOrGenerateGfxLayout(PSOInitializer, Shaders, VertexInputState);
#endif

	OutGfxEntry->RasterizationSamples = OutGfxEntry->RenderPass->GetLayout().GetAttachmentDescriptions()[0].samples;
	ensure(OutGfxEntry->RasterizationSamples == PSOInitializer.NumSamples);
	OutGfxEntry->Topology = (uint32)UEToVulkanType(PSOInitializer.PrimitiveType);

	OutGfxEntry->ColorAttachmentStates.AddUninitialized(OutGfxEntry->RenderPass->GetLayout().GetNumColorAttachments());
	for (int32 Index = 0; Index < OutGfxEntry->ColorAttachmentStates.Num(); ++Index)
	{
		OutGfxEntry->ColorAttachmentStates[Index].ReadFrom(ResourceCast(PSOInitializer.BlendState)->BlendStates[Index]);
	}

	{
		const VkPipelineVertexInputStateCreateInfo& VBInfo = VertexInputState.GetInfo();
		OutGfxEntry->VertexBindings.AddUninitialized(VBInfo.vertexBindingDescriptionCount);
		for (uint32 Index = 0; Index < VBInfo.vertexBindingDescriptionCount; ++Index)
		{
			OutGfxEntry->VertexBindings[Index].ReadFrom(VBInfo.pVertexBindingDescriptions[Index]);
		}

		OutGfxEntry->VertexAttributes.AddUninitialized(VBInfo.vertexAttributeDescriptionCount);
		for (uint32 Index = 0; Index < VBInfo.vertexAttributeDescriptionCount; ++Index)
		{
			OutGfxEntry->VertexAttributes[Index].ReadFrom(VBInfo.pVertexAttributeDescriptions[Index]);
		}
	}

	const TArray<FVulkanDescriptorSetsLayout::FSetLayout>& Layouts = OutGfxEntry->Layout->GetDescriptorSetsLayout().GetLayouts();
	OutGfxEntry->DescriptorSetLayoutBindings.AddDefaulted(Layouts.Num());
	for (int32 Index = 0; Index < Layouts.Num(); ++Index)
	{
		for (int32 SubIndex = 0; SubIndex < Layouts[Index].LayoutBindings.Num(); ++SubIndex)
		{
			FDescriptorSetLayoutBinding* Binding = new(OutGfxEntry->DescriptorSetLayoutBindings[Index]) FDescriptorSetLayoutBinding;
			Binding->ReadFrom(Layouts[Index].LayoutBindings[SubIndex]);
		}
	}

	OutGfxEntry->Rasterizer.ReadFrom(ResourceCast(PSOInitializer.RasterizerState)->RasterizerState);
	{
		VkPipelineDepthStencilStateCreateInfo DSInfo;
		ResourceCast(PSOInitializer.DepthStencilState)->SetupCreateInfo(PSOInitializer, DSInfo);
		OutGfxEntry->DepthStencil.ReadFrom(DSInfo);
	}

	int32 NumShaders = 0;
	for (int32 Index = 0; Index < ShaderStage::NumStages; ++Index)
	{
		FVulkanShader* Shader = Shaders[Index];
		if (Shader)
		{
			check(Shader->Spirv.Num() != 0);

			FSHAHash Hash = GetShaderHashForStage(PSOInitializer, (ShaderStage::EStage)Index);
			OutGfxEntry->ShaderHashes[Index] = Hash;

			OutGfxEntry->ShaderModules[Index] = Shader->GetOrCreateHandle(OutGfxEntry->Layout, OutGfxEntry->Layout->GetDescriptorSetLayoutHash());

			OutGfxEntry->ShaderMicrocodes[Index] = ShaderCache.Get(Hash);
			if (OutGfxEntry->ShaderMicrocodes[Index] == nullptr)
			{
				OutGfxEntry->ShaderMicrocodes[Index] = ShaderCache.Add(Hash, Shader);
			}
			++NumShaders;
		}
	}
	check(NumShaders > 0);

#if VULKAN_SUPPORTS_COLOR_CONVERSIONS
	for (uint32 Index = 0; Index < MaxImmutableSamplers; ++Index)
	{
		OutGfxEntry->ImmutableSamplers[Index] = reinterpret_cast<SIZE_T>(PSOInitializer.ImmutableSamplerState.ImmutableSamplers[Index]);
	}
#endif

	OutGfxEntry->RenderTargets.ReadFrom(OutGfxEntry->RenderPass->GetLayout());

	return OutGfxEntry;
}


FVulkanRHIGraphicsPipelineState* FVulkanPipelineStateCacheManager::FindInLoadedLibrary(const FGraphicsPipelineStateInitializer& PSOInitializer, uint32 PSOInitializerHash, const FShaderHashes& ShaderHashes, FGfxPipelineEntry*& OutGfxEntry, FVulkanPipelineStateCacheManager::FHashToGfxPipelinesMap*& OutHashToGfxPipelinesMap)
{
	OutGfxEntry = nullptr;
	OutHashToGfxPipelinesMap = nullptr;

	FHashToGfxPipelinesMap* FoundHashToGfxPipelinesMap = ShaderHashToGfxPipelineMap.Find(ShaderHashes);
	if (!FoundHashToGfxPipelinesMap)
	{
		FoundHashToGfxPipelinesMap = &ShaderHashToGfxPipelineMap.Add(ShaderHashes);
	}
	
#if VULKAN_ENABLE_LRU_CACHE
	FGfxPipelineEntry* GfxEntry = CreateGfxEntry(PSOInitializer, PSOInitializerHash);
#else
	FGfxPipelineEntry* GfxEntry = CreateGfxEntry(PSOInitializer);
#endif
	uint32 EntryHash = GfxEntry->GetEntryHash();

	FVulkanGfxPipeline** FoundPipeline = FoundHashToGfxPipelinesMap->Find(EntryHash);
	if (FoundPipeline)
	{
		if (!(*FoundPipeline)->IsRuntimeInitialized())
		{
			(*FoundPipeline)->CreateRuntimeObjects(PSOInitializer);
		}
		FVulkanRHIGraphicsPipelineState* PipelineState = new FVulkanRHIGraphicsPipelineState(PSOInitializer, *FoundPipeline);
		{
			FScopeLock Lock2(&InitializerToPipelineMapCS);
			InitializerToPipelineMap.Add(PSOInitializerHash, PipelineState);
		}
		PipelineState->AddRef();
		return PipelineState;
	}

	OutGfxEntry = GfxEntry;
	OutHashToGfxPipelinesMap = FoundHashToGfxPipelinesMap;
	return nullptr;
}

inline FVulkanRHIGraphicsPipelineState* FVulkanPipelineStateCacheManager::FindInRuntimeCache(const FGraphicsPipelineStateInitializer& Initializer, uint32& OutHash)
{
	FPSOHashable Hashable;
	FMemory::Memzero(Hashable);
	Hashable.VertexDeclaration = ResourceCast(Initializer.BoundShaderState.VertexDeclarationRHI);
	Hashable.Shaders[ShaderStage::Vertex] = GetDefaultShaderModule(Initializer.BoundShaderState.VertexShaderRHI);
	Hashable.Shaders[ShaderStage::Pixel] = GetDefaultShaderModule(Initializer.BoundShaderState.PixelShaderRHI);
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
	Hashable.Shaders[ShaderStage::Geometry] = GetDefaultShaderModule(Initializer.BoundShaderState.GeometryShaderRHI);
	//Hashable.Shaders[ShaderStage::Hull] = GetDefaultShaderModule(Initializer.BoundShaderState.HullShaderRHI);
	//Hashable.Shaders[ShaderStage::Domain] = GetDefaultShaderModule(Initializer.BoundShaderState.DomainShaderRHI);
#endif
	Hashable.BlendState = Initializer.BlendState;
	Hashable.RasterizerState = Initializer.RasterizerState;
	Hashable.DepthStencilState = Initializer.DepthStencilState;
	Hashable.DepthStencilAccess = Initializer.DepthStencilAccess;
	Hashable.DepthStencilTargetFormat = Initializer.DepthStencilTargetFormat;
	Hashable.bDepthBounds = Initializer.bDepthBounds ? 1 : 0;
	Hashable.PrimitiveType = Initializer.PrimitiveType;
	Hashable.RenderTargetsEnabled = (uint8)Initializer.RenderTargetsEnabled;
	Hashable.NumSamples = (uint8)Initializer.NumSamples;

#if VULKAN_SUPPORTS_COLOR_CONVERSIONS
	for (int32 Index = 0; Index < Initializer.ImmutableSamplerState.ImmutableSamplers.Num(); ++Index)
	{
		FRHISamplerState* SamplerState = Initializer.ImmutableSamplerState.ImmutableSamplers[Index];
		Hashable.ImmutableSamplers[Index] = SamplerState ? ResourceCast(SamplerState)->Sampler : VK_NULL_HANDLE;
	}
#endif

	OutHash = FCrc::MemCrc32(&Hashable, sizeof(Hashable));
	OutHash = FCrc::MemCrc32(&Initializer.RenderTargetFormats[0], sizeof(Initializer.RenderTargetFormats[0]) * Initializer.RenderTargetsEnabled, OutHash);

	{
		FScopeLock Lock(&InitializerToPipelineMapCS);
		FVulkanRHIGraphicsPipelineState** Found = InitializerToPipelineMap.Find(OutHash);
		if (Found)
		{
			return *Found;
		}
	}

	return nullptr;
}

FGraphicsPipelineStateRHIRef FVulkanDynamicRHI::RHICreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& PSOInitializer)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanGetOrCreatePipeline);
#endif

	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanShaders);

	// First try the hash based off runtime objects
	uint32 PSOInitializerHash = 0;
	FVulkanRHIGraphicsPipelineState* Found = Device->PipelineStateCache->FindInRuntimeCache(PSOInitializer, PSOInitializerHash);
	if (Found)
	{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		const FGraphicsPipelineStateInitializer& FoundInitializer = Found->PipelineStateInitializer;
		ensure(ResourceCast(FoundInitializer.BoundShaderState.VertexDeclarationRHI) == ResourceCast(PSOInitializer.BoundShaderState.VertexDeclarationRHI) &&
			GetDefaultShaderModule(FoundInitializer.BoundShaderState.VertexShaderRHI) == GetDefaultShaderModule(PSOInitializer.BoundShaderState.VertexShaderRHI) &&
			GetDefaultShaderModule(FoundInitializer.BoundShaderState.PixelShaderRHI) == GetDefaultShaderModule(PSOInitializer.BoundShaderState.PixelShaderRHI));
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
		ensure(GetDefaultShaderModule(FoundInitializer.BoundShaderState.GeometryShaderRHI) == GetDefaultShaderModule(PSOInitializer.BoundShaderState.GeometryShaderRHI));
/*
		ensure(GetDefaultShaderModule(FoundInitializer.BoundShaderState.HullShaderRHI) == GetDefaultShaderModule(PSOInitializer.BoundShaderState.HullShaderRHI) &&
		GetDefaultShaderModule(FoundInitializer.BoundShaderState.DomainShaderRHI) == GetDefaultShaderModule(PSOInitializer.BoundShaderState.DomainShaderRHI));
*/
#endif
		ensure(FoundInitializer.BlendState == PSOInitializer.BlendState &&
			FoundInitializer.RasterizerState == PSOInitializer.RasterizerState &&
			FoundInitializer.DepthStencilState == PSOInitializer.DepthStencilState &&
			FoundInitializer.bDepthBounds == PSOInitializer.bDepthBounds &&
			FoundInitializer.PrimitiveType == PSOInitializer.PrimitiveType);
		ensure(FoundInitializer.RenderTargetsEnabled == PSOInitializer.RenderTargetsEnabled &&
			FMemory::Memcmp(&FoundInitializer.RenderTargetFormats[0], &PSOInitializer.RenderTargetFormats[0], FoundInitializer.RenderTargetsEnabled * sizeof(PSOInitializer.RenderTargetFormats[0])) == 0 &&
			FoundInitializer.DepthStencilTargetFormat == PSOInitializer.DepthStencilTargetFormat &&
			FoundInitializer.DepthStencilAccess == PSOInitializer.DepthStencilAccess &&
			FoundInitializer.NumSamples == PSOInitializer.NumSamples);
#if VULKAN_SUPPORTS_COLOR_CONVERSIONS
		ensure(FoundInitializer.ImmutableSamplerState == PSOInitializer.ImmutableSamplerState);
#endif
#endif
#if VULKAN_ENABLE_LRU_CACHE
		Device->GetPipelineStateCache()->PipelineLRU.Touch(Device, Found->Pipeline, Device->GetPipelineStateCache()->GfxPipelineEntries);
#endif
		return Found;
	}

	FVulkanPipelineStateCacheManager::FShaderHashes ShaderHashes(PSOInitializer);

	// Now try the loaded cache from disk
	FVulkanPipelineStateCacheManager::FGfxPipelineEntry* GfxEntry = nullptr;
	FVulkanPipelineStateCacheManager::FHashToGfxPipelinesMap* HashToGfxPipelinesMap = nullptr;

	{
		FScopeLock Lock(&GShaderHashToGfxEntriesMapCS);

		Found = Device->PipelineStateCache->FindInLoadedLibrary(PSOInitializer, PSOInitializerHash, ShaderHashes, GfxEntry, HashToGfxPipelinesMap);
		if (Found)
		{
#if VULKAN_ENABLE_LRU_CACHE
			Device->GetPipelineStateCache()->PipelineLRU.Touch(Device, Found->Pipeline, Device->GetPipelineStateCache()->GfxPipelineEntries);
#endif
			delete GfxEntry;	// not deleting-> may lead memory leak
			return Found;
		}

		UE_LOG(LogVulkanRHI, Verbose, TEXT("PSO not found in cache, compiling..."));

		// Not found, need to actually create one, so prepare a compatible render pass
		FVulkanRenderPass* RenderPass = Device->GetImmediateContext().PrepareRenderPassForPSOCreation(PSOInitializer, GfxEntry->Layout->GetGfxPipelineDescriptorInfo().GetInputAttachmentData());

		// Have we made a matching state object yet?
#if VULKAN_ENABLE_LRU_CACHE
		FVulkanRHIGraphicsPipelineState* PipelineState = Device->GetPipelineStateCache()->CreateAndAdd(PSOInitializer, PSOInitializerHash, GfxEntry, ShaderHashes.Hash);

		if (PipelineState)
		{
			Device->GetPipelineStateCache()->PipelineLRU.Add(Device, PipelineState->Pipeline, Device->GetPipelineStateCache()->GfxPipelineEntries);
		}
#else
		FVulkanRHIGraphicsPipelineState* PipelineState = Device->GetPipelineStateCache()->CreateAndAdd(PSOInitializer, PSOInitializerHash, GfxEntry);
#endif

		if (!PipelineState)
		{
			return nullptr;
		}

		HashToGfxPipelinesMap->Add(GfxEntry->GetEntryHash(), PipelineState->Pipeline);
		return PipelineState;
	}
}

FVulkanComputePipeline* FVulkanPipelineStateCacheManager::GetOrCreateComputePipeline(FVulkanComputeShader* ComputeShader)
{
	FScopeLock ScopeLock(&CreateComputePipelineCS);

	// Fast path, try based on FVulkanComputeShader pointer
	FVulkanComputePipeline** ComputePipelinePtr = ComputeShaderToPipelineMap.Find(ComputeShader);
	if (ComputePipelinePtr)
	{
		return *ComputePipelinePtr;
	}

	// create entry based on shader
	FComputePipelineEntry * ComputeEntry = CreateComputeEntry(ComputeShader);

	// Find pipeline based on entry
	ComputePipelinePtr = ComputeEntryHashToPipelineMap.Find(ComputeEntry->EntryHash);
	if (ComputePipelinePtr)
	{
		if ((*ComputePipelinePtr)->ComputeShader == nullptr) //if loaded from disk, link it to actual shader (1 time initialize step)
		{
			(*ComputePipelinePtr)->ComputeShader = ComputeShader;
		}
		ComputeShaderToPipelineMap.Add(ComputeShader, *ComputePipelinePtr);
		return *ComputePipelinePtr;
	}

	// create pipeline of entry + store entry
	double BeginTime = FPlatformTime::Seconds();

	FVulkanComputePipeline* ComputePipeline = CreateComputePipelineFromEntry(ComputeEntry);
	ComputePipeline->ComputeShader = ComputeShader;

	double EndTime = FPlatformTime::Seconds();
	double Delta = EndTime - BeginTime;
	if (Delta > HitchTime)
	{
		UE_LOG(LogVulkanRHI, Verbose, TEXT("Hitchy compute pipeline key CS (%.3f ms)"), (float)(Delta * 1000.0));
	}

	ComputePipeline->AddRef();
	ComputeEntryHashToPipelineMap.Add(ComputeEntry->EntryHash, ComputePipeline);
	ComputeShaderToPipelineMap.Add(ComputeShader, ComputePipeline);
	ComputePipelineEntries.Add(ComputeEntry->EntryHash, ComputeEntry);

	return ComputePipeline;
}

void FVulkanPipelineStateCacheManager::FComputePipelineEntry::CalculateEntryHash()
{
	TArray<uint8> MemFile;
	FMemoryWriter Ar(MemFile);
	Ar << this;
	EntryHash = FCrc::MemCrc32(MemFile.GetData(), MemFile.GetTypeSize() * MemFile.Num());
	EntryHash = FCrc::MemCrc32(&ShaderHash, sizeof(ShaderHash), EntryHash);
}

FVulkanPipelineStateCacheManager::FComputePipelineEntry* FVulkanPipelineStateCacheManager::CreateComputeEntry(FVulkanComputeShader* ComputeShader)
{
	FComputePipelineEntry* OutComputeEntry = new FComputePipelineEntry();

	OutComputeEntry->ShaderHash = ComputeShader->GetHash();
	OutComputeEntry->ShaderMicrocode = ShaderCache.Get(ComputeShader->GetHash());
	if (!OutComputeEntry->ShaderMicrocode)
	{
		OutComputeEntry->ShaderMicrocode = ShaderCache.Add(ComputeShader->GetHash(), ComputeShader);
	}

	FUniformBufferGatherInfo UBGatherInfo;
	FVulkanDescriptorSetsLayoutInfo DescriptorSetLayoutInfo;
	DescriptorSetLayoutInfo.ProcessBindingsForStage(VK_SHADER_STAGE_COMPUTE_BIT, ShaderStage::Compute, ComputeShader->GetCodeHeader(), UBGatherInfo);
	DescriptorSetLayoutInfo.FinalizeBindings<true>(UBGatherInfo, TArrayView<const FSamplerStateRHIParamRef>());
	FVulkanLayout* Layout = FindOrAddLayout(DescriptorSetLayoutInfo, false);
	check(!Layout->IsGfxLayout());
	FVulkanComputeLayout* ComputeLayout = (FVulkanComputeLayout*)Layout;
	OutComputeEntry->Layout = ComputeLayout;
	check(!ComputeLayout->ComputePipelineDescriptorInfo.IsInitialized());
	ComputeLayout->ComputePipelineDescriptorInfo.Initialize(Layout->GetDescriptorSetsLayout().RemappingInfo, ComputeShader);

	OutComputeEntry->ShaderModule = ComputeShader->GetOrCreateHandle(OutComputeEntry->Layout, OutComputeEntry->Layout->GetDescriptorSetLayoutHash());

	const TArray<FVulkanDescriptorSetsLayout::FSetLayout>& Layouts = DescriptorSetLayoutInfo.GetLayouts();
	OutComputeEntry->DescriptorSetLayoutBindings.AddDefaulted(Layouts.Num());
	for (int32 Index = 0; Index < Layouts.Num(); ++Index)
	{
		for (int32 SubIndex = 0; SubIndex < Layouts[Index].LayoutBindings.Num(); ++SubIndex)
		{
			FDescriptorSetLayoutBinding* Binding = new(OutComputeEntry->DescriptorSetLayoutBindings[Index]) FDescriptorSetLayoutBinding;
			Binding->ReadFrom(Layouts[Index].LayoutBindings[SubIndex]);
		}
	}

	OutComputeEntry->CalculateEntryHash();
	return OutComputeEntry;
}

FVulkanComputePipeline* FVulkanPipelineStateCacheManager::CreateComputePipelineFromEntry(const FComputePipelineEntry* ComputeEntry)
{
	FVulkanComputePipeline* Pipeline = new FVulkanComputePipeline(Device);

	VkComputePipelineCreateInfo PipelineInfo;
	ZeroVulkanStruct(PipelineInfo, VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);
	PipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	PipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	PipelineInfo.stage.module = ComputeEntry->ShaderModule;
	PipelineInfo.stage.pName = "main";
	PipelineInfo.layout = ComputeEntry->Layout->GetPipelineLayout();
		
	VERIFYVULKANRESULT(VulkanRHI::vkCreateComputePipelines(Device->GetInstanceHandle(), PipelineCache, 1, &PipelineInfo, VULKAN_CPU_ALLOCATOR, &Pipeline->Pipeline));

	Pipeline->Layout = ComputeEntry->Layout;

	INC_DWORD_STAT(STAT_VulkanNumPSOs);

	return Pipeline;
}

void FVulkanPipelineStateCacheManager::CreateComputeEntryRuntimeObjects(FComputePipelineEntry* ComputeEntry)
{
	{
		// Descriptor Set Layouts
		check(!ComputeEntry->Layout);

		FVulkanDescriptorSetsLayoutInfo Info;
		for (int32 SetIndex = 0; SetIndex < ComputeEntry->DescriptorSetLayoutBindings.Num(); ++SetIndex)
		{
			for (int32 Index = 0; Index < ComputeEntry->DescriptorSetLayoutBindings[SetIndex].Num(); ++Index)
			{
				VkDescriptorSetLayoutBinding Binding;
				Binding.descriptorCount = 1;
				Binding.pImmutableSamplers = nullptr;
				ComputeEntry->DescriptorSetLayoutBindings[SetIndex][Index].WriteInto(Binding);
				Info.AddDescriptor(SetIndex, Binding);
			}
		}

		FVulkanLayout* Layout = FindOrAddLayout(Info, false);
		check(!Layout->IsGfxLayout());
		ComputeEntry->Layout = (FVulkanComputeLayout*)Layout;
	}

	{
		// Shader
		if (ComputeEntry->ShaderMicrocode != nullptr)
		{
			VkShaderModuleCreateInfo ModuleCreateInfo;
			ZeroVulkanStruct(ModuleCreateInfo, VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
			ModuleCreateInfo.codeSize = ComputeEntry->ShaderMicrocode->Num();
			ModuleCreateInfo.pCode = (uint32*)ComputeEntry->ShaderMicrocode->GetData();
			VERIFYVULKANRESULT(VulkanRHI::vkCreateShaderModule(Device->GetInstanceHandle(), &ModuleCreateInfo, VULKAN_CPU_ALLOCATOR, &ComputeEntry->ShaderModule));
		}		
	}

	ComputeEntry->bLoaded = true;
}

template<typename T>
inline void SerializeArray(FArchive& Ar, TArray<T*>& Array)
{
	int32 Num = Array.Num();
	Ar << Num;
	if (Ar.IsLoading())
	{
		Array.SetNum(Num);
		for (int32 Index = 0; Index < Num; ++Index)
		{
			T* Entry = new T;
			Array[Index] = Entry;
			Ar << *Entry;
		}
	}
	else
	{
		for (int32 Index = 0; Index < Num; ++Index)
		{
			Ar << *(Array[Index]);
		}
	}
}

#if VULKAN_ENABLE_GENERIC_PIPELINE_CACHE_FILE
void FVulkanPipelineStateCacheManager::FVulkanPipelineStateCacheFile::Save(FArchive& Ar)
{
	check(ShaderCache);

	// Modify VERSION if serialization changes
	TArray<uint8> DataBuffer;
	FMemoryWriter DataAr(DataBuffer);

	DataAr << *ShaderCache;
	SerializeArray(DataAr, GfxPipelineEntries);
	SerializeArray(DataAr, ComputePipelineEntries);

	// compress the data buffer
	TArray<uint8> CompressedDataBuffer = DataBuffer;
	if (GEnablePipelineCacheCompression)
	{
		Header.UncompressedSize = DataBuffer.Num() * DataBuffer.GetTypeSize();
		int32 CompressedSize = CompressedDataBuffer.Num();
		if (FCompression::CompressMemory(CompressionFlags, CompressedDataBuffer.GetData(), CompressedSize,
			DataBuffer.GetData(), Header.UncompressedSize))
		{
			CompressedDataBuffer.SetNum(CompressedSize);
		}
		CompressedDataBuffer.Shrink();
	}

	Ar << Header.Version;
	Ar << Header.SizeOfGfxEntry;
	Ar << Header.SizeOfComputeEntry;
	Ar << Header.UncompressedSize;

	Ar << CompressedDataBuffer;
}


bool FVulkanPipelineStateCacheManager::FVulkanPipelineStateCacheFile::Load(FArchive& Ar, const TCHAR* Filename)
{
	check(ShaderCache);

	// Modify VERSION if serialization changes
	Ar << Header.Version;
	if (Header.Version != VERSION)
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to load shader cache due to mismatched Version %d != %d"), Header.Version, (int32)VERSION);
		return false;
	}

	Ar << Header.SizeOfGfxEntry;
	if (Header.SizeOfGfxEntry != (int32)(sizeof(FGfxPipelineEntry)))
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to load shader cache due to mismatched size of FGfxEntry %d != %d; forgot to bump up VERSION?"), Header.SizeOfGfxEntry, (int32)sizeof(FGfxPipelineEntry));
		return false;
	}

	Ar << Header.SizeOfComputeEntry;
	if (Header.SizeOfComputeEntry != (int32)(sizeof(FComputePipelineEntry)))
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to load shader cache due to mismatched size of FComputePipelineEntry %d != %d; forgot to bump up VERSION?"), Header.SizeOfComputeEntry, (int32)sizeof(FComputePipelineEntry));
		return false;
	}

	Ar << Header.UncompressedSize;

	TArray<uint8> CompressedDataBuffer;
	Ar << CompressedDataBuffer;

	TArray<uint8> UncompressedDataBuffer;
	if (Header.UncompressedSize != 0)
	{
		const uint32 CompressedSize = CompressedDataBuffer.Num() * CompressedDataBuffer.GetTypeSize();
		UncompressedDataBuffer.SetNum(Header.UncompressedSize);
		if (!FCompression::UncompressMemory(CompressionFlags, UncompressedDataBuffer.GetData(), Header.UncompressedSize, CompressedDataBuffer.GetData(), CompressedSize))
		{
			UE_LOG(LogVulkanRHI, Error, TEXT("Failed to uncompress data for pipeline cache file %s!"), Filename);
			return false;
		}
	}
	else
	{
		UncompressedDataBuffer = CompressedDataBuffer;
	}

	FMemoryReader DataAr(UncompressedDataBuffer);
	DataAr << *ShaderCache;

	SerializeArray(DataAr, GfxPipelineEntries);

	SerializeArray(DataAr, ComputePipelineEntries);

	return true;
}
#endif

bool FVulkanPipelineStateCacheManager::BinaryCacheMatches(FVulkanDevice* InDevice, const TArray<uint8>& DeviceCache)
{
	if (DeviceCache.Num() > 4)
	{
		uint32* Data = (uint32*)DeviceCache.GetData();
		uint32 HeaderSize = *Data++;
		// 16 is HeaderSize + HeaderVersion
		if (HeaderSize == 16 + VK_UUID_SIZE)
		{
			uint32 HeaderVersion = *Data++;
			if (HeaderVersion == VK_PIPELINE_CACHE_HEADER_VERSION_ONE)
			{
				uint32 VendorID = *Data++;
				const VkPhysicalDeviceProperties& DeviceProperties = InDevice->GetDeviceProperties();
				if (VendorID == DeviceProperties.vendorID)
				{
					uint32 DeviceID = *Data++;
					if (DeviceID == DeviceProperties.deviceID)
					{
						uint8* Uuid = (uint8*)Data;
						if (FMemory::Memcmp(DeviceProperties.pipelineCacheUUID, Uuid, VK_UUID_SIZE) == 0)
						{
							// This particular binary cache matches this device
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}

#if VULKAN_ENABLE_LRU_CACHE
void FVulkanPipelineStateCacheManager::FVKPipelineLRU::FEvictedVkPipeline::RestoreVkPipeline()
{
	Device->GetPipelineStateCache()->CreateGfxPipelineFromEntry(GfxEntry, GfxPipeline);
}

void FVulkanPipelineStateCacheManager::FVulkanLRUCacheFile::Save(FArchive& Ar)
{
	// Modify VERSION if serialization changes
	Ar << Header.Version;
	Ar << Header.SizeOfPipelineSizes;

	SerializeArray(Ar, PipelineSizes);
}

bool FVulkanPipelineStateCacheManager::FVulkanLRUCacheFile::Load(FArchive& Ar)
{
	// Modify VERSION if serialization changes
	Ar << Header.Version;
	if (Header.Version != LRU_CACHE_VERSION)
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to load lru pipeline cache due to mismatched Version %d != %d"), Header.Version, (int32)LRU_CACHE_VERSION);
		return false;
	}

	Ar << Header.SizeOfPipelineSizes;
	if (Header.SizeOfPipelineSizes != (int32)(sizeof(FPipelineSize)))
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to load lru pipeline cache due to mismatched size of FPipelineSize %d != %d; forgot to bump up LRU_CACHE_VERSION?"), Header.SizeOfPipelineSizes, (int32)sizeof(FPipelineSize));
		return false;
	}

	SerializeArray(Ar, PipelineSizes);
	return true;
}
#endif