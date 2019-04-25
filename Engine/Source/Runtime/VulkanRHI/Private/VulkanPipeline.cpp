// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanPipeline.cpp: Vulkan device RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPipeline.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "GlobalShader.h"
#include "VulkanLLM.h"
#include "Misc/ScopeRWLock.h"

static const double HitchTime = 1.0 / 1000.0;

static FCriticalSection EntryKeyToGfxPipelineMapCS;

TAutoConsoleVariable<int32> CVarPipelineLRUCacheEvictBinaryPreloadScreen(
	TEXT("r.Vulkan.PipelineLRUCacheEvictBinaryPreloadScreen"),
	0,
	TEXT("1: Use a preload screen while loading preevicted PSOs ala r.Vulkan.PipelineLRUCacheEvictBinary"),
	ECVF_RenderThreadSafe);

#if VULKAN_ENABLE_LRU_CACHE
TAutoConsoleVariable<int32> CVarEnableLRU(
	TEXT("r.Vulkan.EnablePipelineLRUCache"),
	0,
	TEXT("Pipeline LRU cache.\n")
	TEXT("0: disable LRU\n")
	TEXT("1: Enable LRU"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

TAutoConsoleVariable<int32> CVarPipelineLRUCacheEvictBinary(
	TEXT("r.Vulkan.PipelineLRUCacheEvictBinary"),
	0,
	TEXT("0: create pipelines in from the binary PSO cache and binary shader cache and evict them only as it fills up.\n")
	TEXT("1: don't create pipelines....just immediately evict them"),
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
	case ShaderStage::Hull:			return GetShaderHash<FRHIHullShader, FVulkanHullShader>(PSOInitializer.BoundShaderState.HullShaderRHI);
	case ShaderStage::Domain:		return GetShaderHash<FRHIDomainShader, FVulkanDomainShader>(PSOInitializer.BoundShaderState.DomainShaderRHI);
#endif
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
	if (Pipeline != VK_NULL_HANDLE)
	{
		Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::Pipeline, Pipeline);
		Pipeline = VK_NULL_HANDLE;
	}
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
FVulkanGfxPipeline::FVulkanGfxPipeline(FVulkanDevice* InDevice, TGfxPipelineEntrySharedPtr InGfxPipelineEntry)
	: FVulkanPipeline(InDevice)
	, GfxPipelineEntry(MoveTemp(InGfxPipelineEntry))
	, PipelineCacheSize(0)
	, RecentFrame(0)
	, bTrackedByLRU(false)
	, bRuntimeObjectsValid(false)
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

static TAutoConsoleVariable<int32> GPipelineCacheFromShaderPipelineCacheCvar(
	TEXT("r.Vulkan.PipelineCacheFromShaderPipelineCache"),
	PLATFORM_ANDROID && !(PLATFORM_LUMIN || PLATFORM_LUMINGL4),
	TEXT("0 look for a pipeline cache in the normal locations with the normal names.")
	TEXT("1 tie the vulkan pipeline cache to the shader pipeline cache, use the PSOFC guid as part of the filename, etc."),
	ECVF_ReadOnly
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

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::GetOrCreateShaderModules(FVulkanShader*const* Shaders)
{
	for (int32 Index = 0; Index < ShaderStage::NumStages; ++Index)
	{
		FVulkanShader* Shader = Shaders[Index];
		if (Shader)
		{
			ShaderModules[Index] = Shader->GetOrCreateHandle(Layout, Layout->GetDescriptorSetLayoutHash());
		}
	}
}

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::PurgeShaderModules(FVulkanShader*const* Shaders)
{
	check(!bLoaded);

	for (int32 Index = 0; Index < ShaderStage::NumStages; ++Index)
	{
		FVulkanShader* Shader = Shaders[Index];
		if (Shader)
		{
			Shader->PurgeShaderModules();
			ShaderModules[Index] = VK_NULL_HANDLE;
		}
	}
}

void FVulkanPipelineStateCacheManager::FGfxPipelineEntry::PurgeLoadedShaderModules(FVulkanDevice* InDevice)
{
	check(bLoaded);

	for (int32 Index = 0; Index < ShaderStage::NumStages; ++Index)
	{
		if (ShaderModules[Index] != VK_NULL_HANDLE)
		{
			VulkanRHI::vkDestroyShaderModule(InDevice->GetInstanceHandle(), ShaderModules[Index], VULKAN_CPU_ALLOCATOR);
			ShaderModules[Index] = VK_NULL_HANDLE;
		}
	}

	bLoaded = false;
}


FVulkanPipelineStateCacheManager::FVulkanPipelineStateCacheManager(FVulkanDevice* InDevice)
	: Device(InDevice)
	, bEvictImmediately(false)
	, bLinkedToPSOFC(false)
	, bLinkedToPSOFCSucessfulLoaded(false)
	, PipelineCache(VK_NULL_HANDLE)
{
}


FVulkanPipelineStateCacheManager::~FVulkanPipelineStateCacheManager()
{

	if (bLinkedToPSOFC)
	{
		if (OnShaderPipelineCacheOpenedDelegate.IsValid())
		{
			FShaderPipelineCache::GetCacheOpenedDelegate().Remove(OnShaderPipelineCacheOpenedDelegate);
		}

		if (OnShaderPipelineCachePrecompilationCompleteDelegate.IsValid())
		{
			FShaderPipelineCache::GetPrecompilationCompleteDelegate().Remove(OnShaderPipelineCachePrecompilationCompleteDelegate);
		}
	}
	DestroyCache();

	// Only destroy layouts when quitting
	for (auto& Pair : LayoutMap)
	{
		delete Pair.Value;
	}
	for (auto& Pair : DSetLayoutMap)
	{
		VulkanRHI::vkDestroyDescriptorSetLayout(Device->GetInstanceHandle(), Pair.Value.Handle, VULKAN_CPU_ALLOCATOR);
	}

	VulkanRHI::vkDestroyPipelineCache(Device->GetInstanceHandle(), PipelineCache, VULKAN_CPU_ALLOCATOR);
	PipelineCache = VK_NULL_HANDLE;
}

bool FVulkanPipelineStateCacheManager::Load(const TArray<FString>& CacheFilenames)
{
	bool bResult = false;
	// Try to load device cache first
	for (const FString& CacheFilename : CacheFilenames)
	{
		const VkPhysicalDeviceProperties& DeviceProperties = Device->GetDeviceProperties();
		double BeginTime = FPlatformTime::Seconds();
		FString BinaryCacheAppendage = FString::Printf(TEXT(".%x.%x"), DeviceProperties.vendorID, DeviceProperties.deviceID);
		FString BinaryCacheFilename = CacheFilename;
		if (!CacheFilename.EndsWith(BinaryCacheAppendage))
		{
			BinaryCacheFilename += BinaryCacheAppendage;
		}
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
				UE_LOG(LogVulkanRHI, Display, TEXT("FVulkanPipelineStateCacheManager: Loaded binary pipeline cache %s in %.3f seconds"), *BinaryCacheFilename, (float)(EndTime - BeginTime));
				bResult = true;
			}
			else
			{
				UE_LOG(LogVulkanRHI, Error, TEXT("FVulkanPipelineStateCacheManager: Mismatched binary pipeline cache %s"), *BinaryCacheFilename);
			}
		}
		else
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("FVulkanPipelineStateCacheManager: Binary pipeline cache '%s' not found."), *BinaryCacheFilename);
		}
	}

#if VULKAN_ENABLE_LRU_CACHE
	if (IsUsePipelineLRU())
	{
		for (const FString& CacheFilename : CacheFilenames)
		{
			const VkPhysicalDeviceProperties& DeviceProperties = Device->GetDeviceProperties();
			double BeginTime = FPlatformTime::Seconds();
			FString BinaryCacheAppendage = FString::Printf(TEXT(".%x.%x"), DeviceProperties.vendorID, DeviceProperties.deviceID);
			FString LruCacheFilename = CacheFilename;
			if (!CacheFilename.EndsWith(BinaryCacheAppendage))
			{
				LruCacheFilename += BinaryCacheAppendage;
			}
			LruCacheFilename += TEXT(".lru");
			LruCacheFilename.ReplaceInline(TEXT("TempScanVulkanPSO_"), TEXT("VulkanPSO_"));  //lru files do not use the rename trick...but are still protected against corruption indirectly

			TArray<uint8> MemFile;
			if (FFileHelper::LoadFileToArray(MemFile, *LruCacheFilename, FILEREAD_Silent))
			{
				FMemoryReader Ar(MemFile);

				FVulkanLRUCacheFile File;
				bool Valid = File.Load(Ar);
				if (!Valid)
				{
					UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to load lru pipeline cache '%s'"), *LruCacheFilename);
					bResult = false;
				}

				for (int32 Index = 0; Index < File.PipelineSizes.Num(); ++Index)
				{
					PipelineSizeList.Add(File.PipelineSizes[Index]->ShaderHash, File.PipelineSizes[Index]);
				}
				UE_LOG(LogVulkanRHI, Display, TEXT("Loaded %d LRU size entries for '%s'"), File.PipelineSizes.Num(), *LruCacheFilename);
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to load lru pipeline cache '%s'"), *LruCacheFilename);
				bResult = false;
			}
		}
	}
#endif

	// Lazily create the cache in case the load failed
	if (PipelineCache == VK_NULL_HANDLE)
	{
		VkPipelineCacheCreateInfo PipelineCacheInfo;
		ZeroVulkanStruct(PipelineCacheInfo, VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
		VERIFYVULKANRESULT(VulkanRHI::vkCreatePipelineCache(Device->GetInstanceHandle(), &PipelineCacheInfo, VULKAN_CPU_ALLOCATOR, &PipelineCache));
	}

	return bResult;
}

void FVulkanPipelineStateCacheManager::InitAndLoad(const TArray<FString>& CacheFilenames)
{
	if (GEnablePipelineCacheLoadCvar.GetValueOnAnyThread() == 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Not loading pipeline cache per r.Vulkan.PipelineCacheLoad=0"));
	}
	else
	{
		if (GPipelineCacheFromShaderPipelineCacheCvar.GetValueOnAnyThread() == 0)
		{
			Load(CacheFilenames);
		}
		else
		{
			bLinkedToPSOFC = true;
			UE_LOG(LogVulkanRHI, Log, TEXT("FVulkanPipelineStateCacheManager will check for loading, etc when ShaderPipelineCache opens its file"));


#if PLATFORM_ANDROID && USE_ANDROID_FILE
			// @todo Lumin: Use that GetPathForExternalWrite or something?
			// BTW, this is totally bad. We should not platform ifdefs like this, rather the HAL needs to be extended!
			extern FString GExternalFilePath;
			LinkedToPSOFCCacheFolderPath = GExternalFilePath / TEXT("VulkanProgramBinaryCache");

#else
			LinkedToPSOFCCacheFolderPath = FPaths::ProjectSavedDir() / TEXT("VulkanProgramBinaryCache");
#endif

			// Remove entire ProgramBinaryCache folder if -ClearOpenGLBinaryProgramCache is specified on command line
			if (FParse::Param(FCommandLine::Get(), TEXT("ClearVulkanBinaryProgramCache")))
			{
				UE_LOG(LogVulkanRHI, Log, TEXT("FVulkanPipelineStateCacheManager: Deleting binary program cache folder for -ClearVulkanBinaryProgramCache: %s"), *LinkedToPSOFCCacheFolderPath);
				FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*LinkedToPSOFCCacheFolderPath);
			}

			OnShaderPipelineCacheOpenedDelegate = FShaderPipelineCache::GetCacheOpenedDelegate().AddRaw(this, &FVulkanPipelineStateCacheManager::OnShaderPipelineCacheOpened);
			OnShaderPipelineCachePrecompilationCompleteDelegate = FShaderPipelineCache::GetPrecompilationCompleteDelegate().AddRaw(this, &FVulkanPipelineStateCacheManager::OnShaderPipelineCachePrecompilationComplete);
		}
	}

	// Lazily create the cache in case the load failed
	if (PipelineCache == VK_NULL_HANDLE)
	{
		VkPipelineCacheCreateInfo PipelineCacheInfo;
		ZeroVulkanStruct(PipelineCacheInfo, VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
		VERIFYVULKANRESULT(VulkanRHI::vkCreatePipelineCache(Device->GetInstanceHandle(), &PipelineCacheInfo, VULKAN_CPU_ALLOCATOR, &PipelineCache));
	}
}

void FVulkanPipelineStateCacheManager::OnShaderPipelineCacheOpened(FString const& Name, EShaderPlatform Platform, uint32 Count, const FGuid& VersionGuid, FShaderPipelineCache::FShaderCachePrecompileContext& ShaderCachePrecompileContext)
{
	check(bLinkedToPSOFC);
	UE_LOG(LogVulkanRHI, Log, TEXT("FVulkanPipelineStateCacheManager::OnShaderPipelineCacheOpened %s %d %s"), *Name, Count, *VersionGuid.ToString());

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	const VkPhysicalDeviceProperties& DeviceProperties = Device->GetDeviceProperties();
	FString BinaryCacheAppendage = FString::Printf(TEXT(".%x.%x"), DeviceProperties.vendorID, DeviceProperties.deviceID);

	LinkedToPSOFCCacheFolderFilename = LinkedToPSOFCCacheFolderPath / TEXT("VulkanPSO_") + VersionGuid.ToString() + BinaryCacheAppendage;
	FString TempName = LinkedToPSOFCCacheFolderPath / TEXT("TempScanVulkanPSO_") + VersionGuid.ToString() + BinaryCacheAppendage;

	bool bSuccess = false;

	if (PlatformFile.FileExists(*LinkedToPSOFCCacheFolderFilename))
	{
		// Try to move the file to a temporary filename before the scan, so we won't try to read it again if it's corrupted
		PlatformFile.DeleteFile(*TempName);
		PlatformFile.MoveFile(*TempName, *LinkedToPSOFCCacheFolderFilename);

		TArray<FString> CacheFilenames;
		CacheFilenames.Add(TempName);
		bSuccess = Load(CacheFilenames);

		// Rename the file back after a successful scan.
		if (bSuccess)
		{
			bLinkedToPSOFCSucessfulLoaded = true;
			PlatformFile.MoveFile(*LinkedToPSOFCCacheFolderFilename, *TempName);

#if VULKAN_ENABLE_LRU_CACHE
			if (CVarPipelineLRUCacheEvictBinary.GetValueOnAnyThread())
			{
				bEvictImmediately = true;
			}
#endif

		}
	}
	else
	{
		UE_LOG(LogVulkanRHI, Log, TEXT("FVulkanPipelineStateCacheManager: %s does not exist."), *LinkedToPSOFCCacheFolderFilename);
	}
	if (!bSuccess)
	{
		UE_LOG(LogVulkanRHI, Log, TEXT("FVulkanPipelineStateCacheManager: No matching vulkan PSO cache found or it failed to load, deleting binary program cache folder: %s"), *LinkedToPSOFCCacheFolderPath);
		FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*LinkedToPSOFCCacheFolderPath);
	}

	{
		if (!bLinkedToPSOFCSucessfulLoaded || (bEvictImmediately && CVarPipelineLRUCacheEvictBinaryPreloadScreen.GetValueOnAnyThread()))
		{
			ShaderCachePrecompileContext.SetPrecompilationIsSlowTask();
		}
	}
}

void FVulkanPipelineStateCacheManager::OnShaderPipelineCachePrecompilationComplete(uint32 Count, double Seconds, const FShaderPipelineCache::FShaderCachePrecompileContext& ShaderCachePrecompileContext)
{
	check(bLinkedToPSOFC);
	UE_LOG(LogVulkanRHI, Log, TEXT("FVulkanPipelineStateCacheManager::OnShaderPipelineCachePrecompilationComplete"));

	bEvictImmediately = false;
	if (!bLinkedToPSOFCSucessfulLoaded)
	{
		Save(LinkedToPSOFCCacheFolderFilename, true);
	}

	// Want to ignore any subsequent Shader Pipeline Cache opening/closing, eg when loading modules
	FShaderPipelineCache::GetCacheOpenedDelegate().Remove(OnShaderPipelineCacheOpenedDelegate);
	FShaderPipelineCache::GetPrecompilationCompleteDelegate().Remove(OnShaderPipelineCachePrecompilationCompleteDelegate);
	OnShaderPipelineCacheOpenedDelegate.Reset();
	OnShaderPipelineCachePrecompilationCompleteDelegate.Reset();
}


void FVulkanPipelineStateCacheManager::Save(const FString& CacheFilename, bool bFromPSOFC)
{
	if (bLinkedToPSOFC && !bFromPSOFC)
	{
		UE_LOG(LogVulkanRHI, Log, TEXT("FVulkanPipelineStateCacheManager: skipped saving because we only save if the PSOFC based one failed to load."));
		return;
	}
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
			FString BinaryCacheAppendage = FString::Printf(TEXT(".%x.%x"), DeviceProperties.vendorID, DeviceProperties.deviceID);
			FString BinaryCacheFilename = CacheFilename;
			if (!BinaryCacheFilename.EndsWith(BinaryCacheAppendage))
			{
				BinaryCacheFilename += BinaryCacheAppendage;
			}

			if (FFileHelper::SaveArrayToFile(DeviceCache, *BinaryCacheFilename))
			{
				UE_LOG(LogVulkanRHI, Display, TEXT("FVulkanPipelineStateCacheManager: Saved device pipeline cache file '%s', %d bytes"), *BinaryCacheFilename, DeviceCache.Num());
			}
			else
			{
				UE_LOG(LogVulkanRHI, Error, TEXT("FVulkanPipelineStateCacheManager: Failed to save device pipeline cache file '%s', %d bytes"), *BinaryCacheFilename, DeviceCache.Num());
			}
		}
		else if (Result == VK_INCOMPLETE || Result == VK_ERROR_OUT_OF_HOST_MEMORY)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Failed to get Vulkan pipeline cache data."));

			VulkanRHI::vkDestroyPipelineCache(Device->GetInstanceHandle(), PipelineCache, VULKAN_CPU_ALLOCATOR);
			VkPipelineCacheCreateInfo PipelineCacheInfo;
			ZeroVulkanStruct(PipelineCacheInfo, VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
			VERIFYVULKANRESULT(VulkanRHI::vkCreatePipelineCache(Device->GetInstanceHandle(), &PipelineCacheInfo, VULKAN_CPU_ALLOCATOR, &PipelineCache));
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
		FString BinaryCacheAppendage = FString::Printf(TEXT(".%x.%x"), DeviceProperties.vendorID, DeviceProperties.deviceID);
		FString LruCacheFilename = CacheFilename;
		if (!CacheFilename.EndsWith(BinaryCacheAppendage))
		{
			LruCacheFilename += BinaryCacheAppendage;
		}
		LruCacheFilename += TEXT(".lru");

		if (FFileHelper::SaveArrayToFile(MemFile, *LruCacheFilename))
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("FVulkanPipelineStateCacheManager: Saved pipeline lru pipeline cache file '%s', %d hashes, %d bytes"), *LruCacheFilename, PipelineSizeList.Num(), MemFile.Num());
		}
		else
		{
			UE_LOG(LogVulkanRHI, Error, TEXT("FVulkanPipelineStateCacheManager: Failed to save pipeline lru pipeline cache file '%s', %d hashes, %d bytes"), *LruCacheFilename, PipelineSizeList.Num(), MemFile.Num());
		}
	}
#endif
}

FVulkanRHIGraphicsPipelineState* FVulkanPipelineStateCacheManager::CreateAndAdd(const FGraphicsPipelineStateInitializer& PSOInitializer, FGfxPSIKey PSIKey, TGfxPipelineEntrySharedPtr GfxEntry, FGfxEntryKey GfxEntryKey)
{
	check(GfxEntry);

#if VULKAN_ENABLE_LRU_CACHE
	FVulkanGfxPipeline* Pipeline = new FVulkanGfxPipeline(Device,
		(PipelineLRU.IsActive() ? GfxEntry : TGfxPipelineEntrySharedPtr()));
#else
	FVulkanGfxPipeline* Pipeline = new FVulkanGfxPipeline(Device);
#endif

	Pipeline->CreateRuntimeObjects(PSOInitializer);

#if VULKAN_ENABLE_LRU_CACHE
	if (!PSOInitializer.bFromPSOFileCache || !ShouldEvictImmediately())
#endif
	{
		// Create the pipeline
		double BeginTime = FPlatformTime::Seconds();
		FVulkanShader* VulkanShaders[ShaderStage::NumStages];
		GetVulkanShaders(PSOInitializer.BoundShaderState, VulkanShaders);
		CreateGfxPipelineFromEntry(GfxEntry.Get(), VulkanShaders, Pipeline);
	
		// Recover if we failed to create the pipeline.
		if (!Pipeline->GetHandle())
		{
			delete Pipeline;
			return nullptr;
		}

		double EndTime = FPlatformTime::Seconds();
		double Delta = EndTime - BeginTime;
		if (Delta > HitchTime)
		{
			UE_LOG(LogVulkanRHI, Verbose, TEXT("Hitchy gfx pipeline (%.3f ms)"), (float)(Delta * 1000.0));
		}
	}

	FVulkanRHIGraphicsPipelineState* PipelineState = new FVulkanRHIGraphicsPipelineState(PSOInitializer.BoundShaderState, Pipeline, PSOInitializer.PrimitiveType);
	PipelineState->AddRef();

	{
		FScopeLock Lock(&InitializerToPipelineMapCS);
		InitializerToPipelineMap.Add(MoveTemp(PSIKey), PipelineState);
	}

	EntryKeyToGfxPipelineMap.Add(MoveTemp(GfxEntryKey), PipelineState->Pipeline);
#if VULKAN_ENABLE_LRU_CACHE
	PipelineLRU.Add(PipelineState->Pipeline);
#endif

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
	Ar << Entry.ControlPoints;
	Ar << Entry.Topology;

	Ar << Entry.ColorAttachmentStates;

	Ar << Entry.DescriptorSetLayoutBindings;

	Ar << Entry.VertexBindings;
	Ar << Entry.VertexAttributes;
	Ar << Entry.Rasterizer;

	Ar << Entry.DepthStencil;

	for (int32 Index = 0; Index < ARRAY_COUNT(Entry.ShaderHashes.Stages); ++Index)
	{
		Ar << Entry.ShaderHashes.Stages[Index];
	}

	Ar << Entry.RenderTargets;

#if VULKAN_SUPPORTS_COLOR_CONVERSIONS
	for (uint32 Index = 0; Index < MaxImmutableSamplers; ++Index)
	{
		uint64 Sampler = (uint64)Entry.ImmutableSamplers[Index];
		Ar << Sampler;
		Entry.ImmutableSamplers[Index] = (SIZE_T)Sampler;
	}
#endif

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


FGfxEntryKey FVulkanPipelineStateCacheManager::FGfxPipelineEntry::CreateKey() const
{
	FGfxEntryKey Result;
	Result.GenerateFromArchive([this](FArchive& Ar)
		{
			Ar << const_cast<FGfxPipelineEntry&>(*this);
		});
	return Result;
}

void FVulkanPipelineStateCacheManager::CreateGfxPipelineFromEntry(FGfxPipelineEntry* GfxEntry, FVulkanShader* Shaders[ShaderStage::NumStages], FVulkanGfxPipeline* Pipeline)
{
	if (!GfxEntry->bLoaded)
	{
		GfxEntry->GetOrCreateShaderModules(Shaders);
	}
	
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
	MSInfo.rasterizationSamples = (VkSampleCountFlagBits)FMath::Max<uint16>(1u, GfxEntry->RasterizationSamples);

	VkPipelineShaderStageCreateInfo ShaderStages[ShaderStage::NumStages];
	FMemory::Memzero(ShaderStages);
	PipelineInfo.stageCount = 0;
	PipelineInfo.pStages = ShaderStages;
	// main_00000000_00000000
	ANSICHAR EntryPoints[ShaderStage::NumStages][24];
	bool bHasTessellation = false;
	for (int32 ShaderStage = 0; ShaderStage < ShaderStage::NumStages; ++ShaderStage)
	{
		if (!GfxEntry->ShaderModules[ShaderStage])
		{
			continue;
		}
		const ShaderStage::EStage CurrStage = (ShaderStage::EStage)ShaderStage;

		ShaderStages[PipelineInfo.stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		VkShaderStageFlagBits Stage = UEFrequencyToVKStageBit(ShaderStage::GetFrequencyForGfxStage(CurrStage));
		ShaderStages[PipelineInfo.stageCount].stage = Stage;
		bHasTessellation = bHasTessellation || ((Stage & (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)) != 0);
		ShaderStages[PipelineInfo.stageCount].module = GfxEntry->ShaderModules[CurrStage];
		Shaders[ShaderStage]->GetEntryPoint(EntryPoints[PipelineInfo.stageCount]);
		ShaderStages[PipelineInfo.stageCount].pName = EntryPoints[PipelineInfo.stageCount];
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

	VkPipelineTessellationStateCreateInfo TessState;
	if (bHasTessellation)
	{
		ZeroVulkanStruct(TessState, VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO);
		PipelineInfo.pTessellationState = &TessState;
		check(InputAssembly.topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
		TessState.patchControlPoints = GfxEntry->ControlPoints;

		// Workaround for translating HLSL tessellation shaders on Vulkan backend.
		// Vertical flip of <gl_Position> alone is not sufficient here.
		// Usual value for <frontFace> in UE4 is VK_FRONT_FACE_CLOCKWISE; use the opposite value to flip faces.
		RasterizerState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	}

	//#todo-rco: Fix me
	VkResult Result = VK_ERROR_INITIALIZATION_FAILED;
	double BeginTime = FPlatformTime::Seconds();
#if VULKAN_ENABLE_LRU_CACHE
	if (PipelineLRU.IsActive())
	{
		const uint32 ShaderHash = GfxEntry->ShaderHashes.Hash;
		FPipelineSize** Found = PipelineSizeList.Find(ShaderHash);
		size_t PreSize = 0, AfterSize = 0;
		if (Found)
		{
			Pipeline->PipelineCacheSize = (*Found)->PipelineSize;
		}
		else
		{
			VulkanRHI::vkGetPipelineCacheData(Device->GetInstanceHandle(), PipelineCache, &PreSize, nullptr);
		}

		Result = VulkanRHI::vkCreateGraphicsPipelines(Device->GetInstanceHandle(), PipelineCache, 1, &PipelineInfo, VULKAN_CPU_ALLOCATOR, &Pipeline->Pipeline);

		if (!Found && Result == VK_SUCCESS)
		{
			VulkanRHI::vkGetPipelineCacheData(Device->GetInstanceHandle(), PipelineCache, &AfterSize, nullptr);
			uint32 Diff = AfterSize - PreSize;
			if (!Diff)
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Shader size was computed as zero, using 20k instead."));
				Diff = 20 * 1024;
			}
			FPipelineSize* PipelineSize = new FPipelineSize();
			PipelineSize->ShaderHash = ShaderHash;
			PipelineSize->PipelineSize = Diff;
			PipelineSizeList.Add(ShaderHash, PipelineSize);
			Pipeline->PipelineCacheSize = Diff;
		}
	}
	else
#endif
	{
		Result = VulkanRHI::vkCreateGraphicsPipelines(Device->GetInstanceHandle(), PipelineCache, 1, &PipelineInfo, VULKAN_CPU_ALLOCATOR, &Pipeline->Pipeline);
	}

#if VULKAN_PURGE_SHADER_MODULES
	if (GfxEntry->bLoaded)
	{
		GfxEntry->PurgeLoadedShaderModules(Device);
	}
	else
	{
		GfxEntry->PurgeShaderModules(Shaders);
	}
#endif

	if (Result != VK_SUCCESS)
	{
		UE_LOG(LogVulkanRHI, Error, TEXT("Failed to create graphics pipeline."));
		Pipeline->Pipeline = VK_NULL_HANDLE;
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

void FVulkanPipelineStateCacheManager::DestroyCache()
{
	VkDevice DeviceHandle = Device->GetInstanceHandle();

	// Graphics
	{
		for (auto& Pair : InitializerToPipelineMap)
		{
			FVulkanRHIGraphicsPipelineState* Pipeline = Pair.Value;
			//when DestroyCache is called as part of r.Vulkan.RebuildPipelineCache, a pipeline can still be referenced by FVulkanPendingGfxState
			ensure(GIsRHIInitialized || (!GIsRHIInitialized && Pipeline->GetRefCount() == 1));
			Pipeline->Release();
		}
		InitializerToPipelineMap.Reset();

		for (auto& Pair : GfxPipelineEntries)
		{
			FGfxPipelineEntry* Entry = Pair.Value.Get();
			if (Entry->bLoaded)
			{
				Entry->PurgeLoadedShaderModules(Device);
			}
		}
		GfxPipelineEntries.Reset();

#if VULKAN_ENABLE_LRU_CACHE
		for (auto& Pair : PipelineSizeList)
		{
			FPipelineSize* Entry = Pair.Value;
			delete Entry;
		}
		PipelineSizeList.Reset();
#endif

		// This map can simply be cleared as InitializerToPipelineMap already decreased the refcount of the pipeline objects	
		{
			FScopeLock Lock(&EntryKeyToGfxPipelineMapCS);
			EntryKeyToGfxPipelineMap.Reset();
		}
	}

	// Compute pipelines already deleted...
	ComputePipelineEntries.Reset();
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
	Stages[ShaderStage::Hull] = GetShaderHash<FRHIHullShader, FVulkanHullShader>(PSOInitializer.BoundShaderState.HullShaderRHI);
	Stages[ShaderStage::Domain] = GetShaderHash<FRHIDomainShader, FVulkanDomainShader>(PSOInitializer.BoundShaderState.DomainShaderRHI);
#endif
	Finalize();
}

FVulkanPipelineStateCacheManager::FShaderHashes::FShaderHashes()
{
	FMemory::Memzero(Stages);
	Hash = 0;
}

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
	Layout->Compile(DSetLayoutMap);

	LayoutMap.Add(DescriptorSetLayoutInfo, Layout);
	return Layout;
}

FVulkanGfxLayout* FVulkanPipelineStateCacheManager::GetOrGenerateGfxLayout(const FGraphicsPipelineStateInitializer& PSOInitializer,
	FVulkanShader*const* Shaders, FVulkanVertexInputStateInfo& OutVertexInputState)
{
	const FBoundShaderStateInput& BSI = PSOInitializer.BoundShaderState;

	const FVulkanShaderHeader& VSHeader = Shaders[ShaderStage::Vertex]->GetCodeHeader();
	OutVertexInputState.Generate(ResourceCast(PSOInitializer.BoundShaderState.VertexDeclarationRHI), VSHeader.InOutMask);

	FUniformBufferGatherInfo UBGatherInfo;

	// First pass to gather uniform buffer info
	FVulkanDescriptorSetsLayoutInfo DescriptorSetLayoutInfo;
	DescriptorSetLayoutInfo.ProcessBindingsForStage(VK_SHADER_STAGE_VERTEX_BIT, ShaderStage::Vertex, VSHeader, UBGatherInfo);

	if (Shaders[ShaderStage::Pixel])
	{
		const FVulkanShaderHeader& PSHeader = Shaders[ShaderStage::Pixel]->GetCodeHeader();
		DescriptorSetLayoutInfo.ProcessBindingsForStage(VK_SHADER_STAGE_FRAGMENT_BIT, ShaderStage::Pixel, PSHeader, UBGatherInfo);
	}

#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
	if (Shaders[ShaderStage::Geometry])
	{
		const FVulkanShaderHeader& GSHeader = Shaders[ShaderStage::Geometry]->GetCodeHeader();
		DescriptorSetLayoutInfo.ProcessBindingsForStage(VK_SHADER_STAGE_GEOMETRY_BIT, ShaderStage::Geometry, GSHeader, UBGatherInfo);
	}
	
	if (Shaders[ShaderStage::Hull])
	{
		const FVulkanShaderHeader& HSHeader = Shaders[ShaderStage::Hull]->GetCodeHeader();
		const FVulkanShaderHeader& DSHeader = Shaders[ShaderStage::Domain]->GetCodeHeader();
		DescriptorSetLayoutInfo.ProcessBindingsForStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, ShaderStage::Hull, HSHeader, UBGatherInfo);
		DescriptorSetLayoutInfo.ProcessBindingsForStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, ShaderStage::Domain, DSHeader, UBGatherInfo);
	}
#endif


	// Second pass
	const int32 NumImmutableSamplers = PSOInitializer.ImmutableSamplerState.ImmutableSamplers.Num();
	TArrayView<const FSamplerStateRHIParamRef> ImmutableSamplers(NumImmutableSamplers > 0 ? &PSOInitializer.ImmutableSamplerState.ImmutableSamplers[0] : nullptr, NumImmutableSamplers);
	DescriptorSetLayoutInfo.FinalizeBindings<false>(UBGatherInfo, ImmutableSamplers);

	FVulkanLayout* Layout = FindOrAddLayout(DescriptorSetLayoutInfo, true);
	FVulkanGfxLayout* GfxLayout = (FVulkanGfxLayout*)Layout;
	if (!GfxLayout->GfxPipelineDescriptorInfo.IsInitialized())
	{
		GfxLayout->GfxPipelineDescriptorInfo.Initialize(Layout->GetDescriptorSetsLayout().RemappingInfo);
	}

	return GfxLayout;
}

static inline VkPrimitiveTopology UEToVulkanTopologyType(const FVulkanDevice* InDevice, EPrimitiveType PrimitiveType, bool bHasTessellation, uint16& OutControlPoints)
{
	if (bHasTessellation)
	{
		switch (PrimitiveType)
		{
		case PT_TriangleList:
			// This is the case for tessellation without AEN or other buffers, so just flip to 3 CPs
			OutControlPoints = 3;
			return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
		case PT_1_ControlPointPatchList:
		case PT_2_ControlPointPatchList:
		case PT_3_ControlPointPatchList:
		case PT_4_ControlPointPatchList:
		case PT_5_ControlPointPatchList:
		case PT_6_ControlPointPatchList:
		case PT_7_ControlPointPatchList:
		case PT_8_ControlPointPatchList:
		case PT_9_ControlPointPatchList:
		case PT_10_ControlPointPatchList:
		case PT_12_ControlPointPatchList:
		case PT_13_ControlPointPatchList:
		case PT_14_ControlPointPatchList:
		case PT_15_ControlPointPatchList:
		case PT_16_ControlPointPatchList:
		case PT_17_ControlPointPatchList:
		case PT_18_ControlPointPatchList:
		case PT_19_ControlPointPatchList:
		case PT_20_ControlPointPatchList:
		case PT_22_ControlPointPatchList:
		case PT_23_ControlPointPatchList:
		case PT_24_ControlPointPatchList:
		case PT_25_ControlPointPatchList:
		case PT_26_ControlPointPatchList:
		case PT_27_ControlPointPatchList:
		case PT_28_ControlPointPatchList:
		case PT_29_ControlPointPatchList:
		case PT_30_ControlPointPatchList:
		case PT_31_ControlPointPatchList:
		case PT_32_ControlPointPatchList:
			OutControlPoints = (PrimitiveType - PT_1_ControlPointPatchList + 1);
			checkf(
				OutControlPoints <= InDevice->GetLimits().maxTessellationPatchSize,
				TEXT("OutControlPoints (%d) exceeded limit of maximal patch size (%d)"),
				OutControlPoints,
				InDevice->GetLimits().maxTessellationPatchSize
			);
			return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
		default:
			checkf(false, TEXT("Unsupported tessellation EPrimitiveType %d; probably missing a case in FStaticMeshSceneProxy::GetMeshElement()!"), (uint32)PrimitiveType);
			break;
		}
		OutControlPoints = 0;
	}
	else
	{
		OutControlPoints = 0;
		switch (PrimitiveType)
		{
		case PT_PointList:
			return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		case PT_LineList:
			return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		case PT_TriangleList:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		case PT_TriangleStrip:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		default:
			checkf(false, TEXT("Unsupported EPrimitiveType %d"), (uint32)PrimitiveType);
			break;
		}
	}

	return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

FVulkanPipelineStateCacheManager::FGfxPipelineEntry* FVulkanPipelineStateCacheManager::CreateGfxEntry(const FGraphicsPipelineStateInitializer& PSOInitializer)
{
	FGfxPipelineEntry* OutGfxEntry = new FGfxPipelineEntry();

	FVulkanShader* Shaders[ShaderStage::NumStages];
	GetVulkanShaders(PSOInitializer.BoundShaderState, Shaders);

	OutGfxEntry->RenderPass = Device->GetImmediateContext().PrepareRenderPassForPSOCreation(PSOInitializer, OutGfxEntry->Layout->GetDescriptorSetsLayout().RemappingInfo.InputAttachmentData);

	FVulkanVertexInputStateInfo VertexInputState;
	OutGfxEntry->Layout = GetOrGenerateGfxLayout(PSOInitializer, Shaders, VertexInputState);

	const bool bHasTessellation = (PSOInitializer.BoundShaderState.DomainShaderRHI != nullptr);

	OutGfxEntry->RasterizationSamples = OutGfxEntry->RenderPass->GetLayout().GetAttachmentDescriptions()[0].samples;
	ensure(OutGfxEntry->RasterizationSamples == PSOInitializer.NumSamples);
	OutGfxEntry->Topology = (uint32)UEToVulkanTopologyType(Device, PSOInitializer.PrimitiveType, bHasTessellation, OutGfxEntry->ControlPoints);

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
			OutGfxEntry->ShaderHashes.Stages[Index] = Hash;

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

	OutGfxEntry->ShaderHashes.Finalize();
	
	return OutGfxEntry;
}


FVulkanRHIGraphicsPipelineState* FVulkanPipelineStateCacheManager::FindInLoadedLibrary(const FGraphicsPipelineStateInitializer& PSOInitializer, FGfxPSIKey& PSIKey, TGfxPipelineEntrySharedPtr& OutGfxEntry, FGfxEntryKey& OutGfxEntryKey)
{
	OutGfxEntry = nullptr;
	
	FGfxPipelineEntry* GfxEntry = CreateGfxEntry(PSOInitializer);
	FGfxEntryKey GfxEntryKey = GfxEntry->CreateKey();

	FVulkanGfxPipeline** FoundPipeline = EntryKeyToGfxPipelineMap.Find(GfxEntryKey);
	if (FoundPipeline)
	{
		if (!(*FoundPipeline)->IsRuntimeInitialized())
		{
			(*FoundPipeline)->CreateRuntimeObjects(PSOInitializer);
		}
		FVulkanRHIGraphicsPipelineState* PipelineState = new FVulkanRHIGraphicsPipelineState(PSOInitializer.BoundShaderState, *FoundPipeline, PSOInitializer.PrimitiveType);
		{
			FScopeLock Lock2(&InitializerToPipelineMapCS);
			InitializerToPipelineMap.Add(MoveTemp(PSIKey), PipelineState);
		}
		PipelineState->AddRef();
		delete GfxEntry;
#if VULKAN_ENABLE_LRU_CACHE
		PipelineLRU.Touch(PipelineState);
#endif
		return PipelineState;
	}

	OutGfxEntry = TGfxPipelineEntrySharedPtr(GfxEntry);
	OutGfxEntryKey = MoveTemp(GfxEntryKey);
	return nullptr;
}

FVulkanRHIGraphicsPipelineState* FVulkanPipelineStateCacheManager::FindInRuntimeCache(const FGraphicsPipelineStateInitializer& Initializer, FGfxPSIKey& OutKey)
{
	OutKey.GenerateFromArchive([&Initializer](FArchive& Ar)
		{
			FGraphicsPipelineStateInitializer& PSI = const_cast<FGraphicsPipelineStateInitializer&>(Initializer);

			uint64 TempUInt64 = 0;
			int32 TempEnumValue = 0;

			TempUInt64 = GetShaderKey(PSI.BoundShaderState.VertexShaderRHI);
			Ar << TempUInt64;

			TempUInt64 = GetShaderKey(PSI.BoundShaderState.PixelShaderRHI);
			Ar << TempUInt64;

	#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
			TempUInt64 = GetShaderKey(PSI.BoundShaderState.GeometryShaderRHI);
			Ar << TempUInt64;
			TempUInt64 = GetShaderKey(PSI.BoundShaderState.HullShaderRHI);
			Ar << TempUInt64;
			TempUInt64 = GetShaderKey(PSI.BoundShaderState.DomainShaderRHI);
			Ar << TempUInt64;
	#endif
			Ar << ResourceCast(PSI.BoundShaderState.VertexDeclarationRHI)->Elements;
			Ar << ResourceCast(PSI.RasterizerState)->Initializer;
			Ar << ResourceCast(PSI.DepthStencilState)->Initializer;
			{
				FBlendStateInitializerRHI &BlendState = ResourceCast(PSI.BlendState)->Initializer;
				for (uint32 Index = 0; Index < PSI.RenderTargetsEnabled; ++Index)
				{
					Ar << BlendState.RenderTargets[Index];
					TempEnumValue = PSI.RenderTargetFormats[Index];
					Ar << TempEnumValue;
				}
				Ar << BlendState.bUseIndependentRenderTargetBlendStates;
			}

			// TODO To avoid changing of global header. Add getter when submitting
			Ar.Serialize(&PSI.DepthStencilAccess, sizeof(PSI.DepthStencilAccess));

			TempEnumValue = PSI.DepthStencilTargetFormat;
			Ar << TempEnumValue;

			TempEnumValue = PSI.PrimitiveType;
			Ar << TempEnumValue;

#if VULKAN_SUPPORTS_COLOR_CONVERSIONS
			for (uint32 Index = 0; Index < MaxImmutableSamplers; ++Index)
			{
				FRHISamplerState* SamplerState = PSI.ImmutableSamplerState.ImmutableSamplers[Index];
				VkSampler Handle = SamplerState ? ResourceCast(SamplerState)->Sampler : VK_NULL_HANDLE;
				TempUInt64 = (uint64)Handle;
				Ar << TempUInt64;
			}
#endif

			Ar << PSI.bDepthBounds;
			Ar << PSI.RenderTargetsEnabled;
			Ar << PSI.NumSamples;

		},
		256);

	FVulkanRHIGraphicsPipelineState** Found = nullptr;

	{
		FScopeLock Lock(&InitializerToPipelineMapCS);
		Found = InitializerToPipelineMap.Find(OutKey);
	}

	if (Found)
	{
#if VULKAN_ENABLE_LRU_CACHE
		PipelineLRU.Touch(*Found);
#endif
		return *Found;
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
	FGfxPSIKey PSIKey;
	FVulkanRHIGraphicsPipelineState* Found = Device->PipelineStateCache->FindInRuntimeCache(PSOInitializer, PSIKey);
	if (Found)
	{
		return Found;
	}

	// Now try the loaded cache from disk
	FVulkanPipelineStateCacheManager::TGfxPipelineEntrySharedPtr GfxEntry;
	FGfxEntryKey GfxEntryKey;

	{
		FScopeLock Lock(&EntryKeyToGfxPipelineMapCS);

		Found = Device->PipelineStateCache->FindInLoadedLibrary(PSOInitializer, PSIKey, GfxEntry, GfxEntryKey);
		if (Found)
		{
			return Found;
		}

		UE_LOG(LogVulkanRHI, Verbose, TEXT("PSO not found in cache, compiling..."));

		FVulkanRHIGraphicsPipelineState* PipelineState = Device->PipelineStateCache->CreateAndAdd(PSOInitializer, MoveTemp(PSIKey), MoveTemp(GfxEntry), MoveTemp(GfxEntryKey));

		return PipelineState;
	}
}

FVulkanComputePipeline* FVulkanPipelineStateCacheManager::GetOrCreateComputePipeline(FVulkanComputeShader* ComputeShader)
{
	check(ComputeShader);
	const uint64 Key = ComputeShader->GetShaderKey();
	{
		FRWScopeLock ScopeLock(ComputePipelineLock, SLT_ReadOnly);
		FVulkanComputePipeline** ComputePipelinePtr = ComputePipelineEntries.Find(Key);
		if (ComputePipelinePtr)
		{
			return *ComputePipelinePtr;
		}
	}

	// create pipeline of entry + store entry
	double BeginTime = FPlatformTime::Seconds();

	FVulkanComputePipeline* ComputePipeline = CreateComputePipelineFromShader(ComputeShader);
	ComputePipeline->ComputeShader = ComputeShader;

	double EndTime = FPlatformTime::Seconds();
	double Delta = EndTime - BeginTime;
	if (Delta > HitchTime)
	{
		UE_LOG(LogVulkanRHI, Verbose, TEXT("Hitchy compute pipeline key CS (%.3f ms)"), (float)(Delta * 1000.0));
	}

	{
		FRWScopeLock ScopeLock(ComputePipelineLock, SLT_Write);
		ComputePipelineEntries.FindOrAdd(Key) = ComputePipeline;
	}

	return ComputePipeline;
}

FVulkanComputePipeline* FVulkanPipelineStateCacheManager::CreateComputePipelineFromShader(FVulkanComputeShader* Shader)
{
	FVulkanComputePipeline* Pipeline = new FVulkanComputePipeline(Device);

	FVulkanDescriptorSetsLayoutInfo DescriptorSetLayoutInfo;
	const FVulkanShaderHeader& CSHeader = Shader->GetCodeHeader();
	FUniformBufferGatherInfo UBGatherInfo;
	DescriptorSetLayoutInfo.ProcessBindingsForStage(VK_SHADER_STAGE_COMPUTE_BIT, ShaderStage::Compute, CSHeader, UBGatherInfo);
	DescriptorSetLayoutInfo.FinalizeBindings<true>(UBGatherInfo, TArrayView<const FSamplerStateRHIParamRef>());
	FVulkanLayout* Layout = FindOrAddLayout(DescriptorSetLayoutInfo, true);
	FVulkanComputeLayout* ComputeLayout = (FVulkanComputeLayout*)Layout;
	if (!ComputeLayout->ComputePipelineDescriptorInfo.IsInitialized())
	{
		ComputeLayout->ComputePipelineDescriptorInfo.Initialize(Layout->GetDescriptorSetsLayout().RemappingInfo);
	}

	VkShaderModule ShaderModule = Shader->GetOrCreateHandle(Layout, Layout->GetDescriptorSetLayoutHash());

	VkComputePipelineCreateInfo PipelineInfo;
	ZeroVulkanStruct(PipelineInfo, VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);
	PipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	PipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	PipelineInfo.stage.module = ShaderModule;
	// main_00000000_00000000
	ANSICHAR EntryPoint[24];
	Shader->GetEntryPoint(EntryPoint);
	PipelineInfo.stage.pName = EntryPoint;
	PipelineInfo.layout = ComputeLayout->GetPipelineLayout();
		
	VERIFYVULKANRESULT(VulkanRHI::vkCreateComputePipelines(Device->GetInstanceHandle(), PipelineCache, 1, &PipelineInfo, VULKAN_CPU_ALLOCATOR, &Pipeline->Pipeline));

	Pipeline->Layout = ComputeLayout;

	INC_DWORD_STAT(STAT_VulkanNumPSOs);

	return Pipeline;
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
void FVulkanPipelineStateCacheManager::FVKPipelineLRU::DeleteVkPipeline(FVulkanGfxPipeline* GfxPipeline)
{
	GfxPipeline->DeleteVkPipeline(GfxPipeline->RecentFrame + NUM_BUFFERS < GFrameNumberRenderThread);
}

void FVulkanPipelineStateCacheManager::FVKPipelineLRU::EnsureVkPipelineAndAddToLRU(FVulkanRHIGraphicsPipelineState* Pipeline)
{
	FVulkanGfxPipeline* GfxPipeline = Pipeline->Pipeline;
	FGfxPipelineEntry* GfxPipelineEntry = static_cast<FGfxPipelineEntry*>(GfxPipeline->GfxPipelineEntry.Get());
	FVulkanPipelineStateCacheManager* PipelineStateCache = GfxPipeline->Device->GetPipelineStateCache();

	{
		// This CS is used in RHICreateGraphicsPipelineState() to guard pipeline creation from multiple threads
		FScopeLock Lock(&EntryKeyToGfxPipelineMapCS);

		if (!GfxPipeline->GetHandle())
		{
			FVulkanShader* VulkanShaders[ShaderStage::NumStages];
			GetVulkanShaders(GfxPipeline->Device, *Pipeline, VulkanShaders);
			PipelineStateCache->CreateGfxPipelineFromEntry(GfxPipelineEntry, VulkanShaders, GfxPipeline);

			// Failed to create VkPipeline again
			check(GfxPipeline->GetHandle());

			AddToLRU(GfxPipeline);
		}
	}
}

void FVulkanPipelineStateCacheManager::FVKPipelineLRU::AddToLRU(FVulkanGfxPipeline* GfxPipeline)
{
	FScopeLock Lock(&LRUCS);

	while (LRUUsedPipelineSize + GfxPipeline->PipelineCacheSize >(uint32)CVarLRUMaxPipelineSize.GetValueOnAnyThread() || LRU.Num() == LRU.Max())
	{
		EvictFromLRU();
	}

	check(!LRU.Contains(GfxPipeline));
	GfxPipeline->LRUNode = LRU.Add(GfxPipeline, GfxPipeline);
	LRUUsedPipelineSize += GfxPipeline->PipelineCacheSize;
}

void FVulkanPipelineStateCacheManager::FVKPipelineLRU::EvictFromLRU()
{
	FVulkanGfxPipeline* LeastRecentPipeline = LRU.RemoveLeastRecent();
	check(LeastRecentPipeline);
	LeastRecentPipeline->LRUNode = FSetElementId();

	LRUUsedPipelineSize -= LeastRecentPipeline->PipelineCacheSize;

	DeleteVkPipeline(LeastRecentPipeline);
}

void FVulkanPipelineStateCacheManager::FVKPipelineLRU::Add(FVulkanGfxPipeline* GfxPipeline)
{
	if (!bUseLRU)
	{
		return;
	}

	check(!GfxPipeline->bTrackedByLRU);
	GfxPipeline->bTrackedByLRU = true;

	if (!GfxPipeline->GetHandle())
	{
		GfxPipeline->LRUNode = FSetElementId(); // Add as evicted
		return;
	}

	AddToLRU(GfxPipeline);
}

void FVulkanPipelineStateCacheManager::FVKPipelineLRU::Touch(FVulkanRHIGraphicsPipelineState* Pipeline)
{
	if (!bUseLRU)
	{
		return;
	}

	FVulkanGfxPipeline* GfxPipeline = Pipeline->Pipeline;
	check(GfxPipeline->bTrackedByLRU);

	{
		FScopeLock Lock(&LRUCS);
		if (GfxPipeline->LRUNode.IsValidId())
		{
			check(GfxPipeline->GetHandle());
			check(LRU.Contains(GfxPipeline));
			LRU.MarkAsRecent(GfxPipeline->LRUNode);
			GfxPipeline->RecentFrame = GFrameNumberRenderThread;
			return;
		}
	}

	EnsureVkPipelineAndAddToLRU(Pipeline);
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


void GetVulkanShaders(const FBoundShaderStateInput& BSI, FVulkanShader* OutShaders[ShaderStage::NumStages])
{
	FMemory::Memzero(OutShaders, ShaderStage::NumStages * sizeof(*OutShaders));

	OutShaders[ShaderStage::Vertex] = ResourceCast(BSI.VertexShaderRHI);

	if (BSI.PixelShaderRHI)
	{
		OutShaders[ShaderStage::Pixel] = ResourceCast(BSI.PixelShaderRHI);
	}
	else if (GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1)
	{
		// Some mobile devices expect PS stage (S7 Adreno)
		OutShaders[ShaderStage::Pixel] = ResourceCast(TShaderMapRef<FNULLPS>(GetGlobalShaderMap(GMaxRHIFeatureLevel))->GetPixelShader());
	}

#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
	if (BSI.GeometryShaderRHI)
	{
		OutShaders[ShaderStage::Geometry] = ResourceCast(BSI.GeometryShaderRHI);
	}

	if (BSI.HullShaderRHI)
	{
		// Can't have Hull w/o Domain
		check(BSI.DomainShaderRHI);
		OutShaders[ShaderStage::Hull] = ResourceCast(BSI.HullShaderRHI);
		OutShaders[ShaderStage::Domain] = ResourceCast(BSI.DomainShaderRHI);
	}
	else
	{
		// Can't have Domain w/o Hull
		check(BSI.DomainShaderRHI == nullptr);
	}
#else
	if (BSI.GeometryShaderRHI || BSI.HullShaderRHI || BSI.DomainShaderRHI)
	{
		ensureMsgf(0, TEXT("Geometry not supported!"));
	}
#endif
}

void GetVulkanShaders(FVulkanDevice* Device, const FVulkanRHIGraphicsPipelineState& GfxPipelineState, FVulkanShader* OutShaders[ShaderStage::NumStages])
{
	FMemory::Memzero(OutShaders, ShaderStage::NumStages * sizeof(*OutShaders));
	Device->GetShaderFactory().LookupShaders(GfxPipelineState.ShaderKeys, OutShaders);
}
