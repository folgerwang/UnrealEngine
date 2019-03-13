// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanDevice.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanMemory.h"

class FVulkanDescriptorSetCache;
class FVulkanDescriptorPool;
class FVulkanDescriptorPoolsManager;
class FVulkanCommandListContextImmediate;
#if VULKAN_USE_NEW_QUERIES
class FVulkanOcclusionQueryPool;
#else
class FOLDVulkanQueryPool;
#endif

struct FOptionalVulkanDeviceExtensions
{
	uint32 HasKHRMaintenance1 : 1;
	uint32 HasKHRMaintenance2 : 1;
	//uint32 HasMirrorClampToEdge : 1;
	uint32 HasKHRExternalMemoryCapabilities : 1;
	uint32 HasKHRGetPhysicalDeviceProperties2 : 1;
	uint32 HasKHRDedicatedAllocation : 1;
	uint32 HasEXTValidationCache : 1;
	uint32 HasAMDBufferMarker : 1;
	uint32 HasNVDiagnosticCheckpoints : 1;
	uint32 HasGoogleDisplayTiming : 1;
	uint32 HasYcbcrSampler : 1;

	inline bool HasGPUCrashDumpExtensions() const
	{
		return HasAMDBufferMarker || HasNVDiagnosticCheckpoints;
	}
};

class FVulkanDevice
{
public:
	FVulkanDevice(VkPhysicalDevice Gpu);

	~FVulkanDevice();

	// Returns true if this is a viable candidate for main GPU
	bool QueryGPU(int32 DeviceIndex);

	void InitGPU(int32 DeviceIndex);

	void CreateDevice();

	void PrepareForDestroy();
	void Destroy();

	void WaitUntilIdle();

	inline bool HasAsyncComputeQueue() const
	{
		return bAsyncComputeQueue;
	}

	inline bool CanPresentOnComputeQueue() const
	{
		return bPresentOnComputeQueue;
	}

	inline bool IsRealAsyncComputeContext(const FVulkanCommandListContext* InContext) const
	{
		if (bAsyncComputeQueue)
		{
			ensure((FVulkanCommandListContext*)ImmediateContext != ComputeContext);
			return InContext == ComputeContext;
		}
	
		return false;
	}

	inline FVulkanQueue* GetGraphicsQueue()
	{
		return GfxQueue;
	}

	inline FVulkanQueue* GetComputeQueue()
	{
		return ComputeQueue;
	}

	inline FVulkanQueue* GetTransferQueue()
	{
		return TransferQueue;
	}

	inline FVulkanQueue* GetPresentQueue()
	{
		return PresentQueue;
	}

	inline VkPhysicalDevice GetPhysicalHandle() const
	{
		return Gpu;
	}

	inline const VkPhysicalDeviceProperties& GetDeviceProperties() const
	{
		return GpuProps;
	}

	inline const VkPhysicalDeviceLimits& GetLimits() const
	{
		return GpuProps.limits;
	}

#if VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
	inline const VkPhysicalDeviceIDPropertiesKHR& GetDeviceIdProperties() const
	{
		check(GetOptionalExtensions().HasKHRGetPhysicalDeviceProperties2);
		return GpuIdProps;
	}
#endif

#if VULKAN_SUPPORTS_VALIDATION_CACHE
	inline VkValidationCacheEXT GetValidationCache() const
	{
		return ValidationCache;
	}
#endif

	inline const VkPhysicalDeviceFeatures& GetPhysicalFeatures() const
	{
		return PhysicalFeatures;
	}

	inline bool HasUnifiedMemory() const
	{
		return MemoryManager.HasUnifiedMemory();
	}

	inline uint64 GetTimestampValidBitsMask() const
	{
		return TimestampValidBitsMask;
	}

	bool IsTextureFormatSupported(VkFormat Format) const;
	bool IsBufferFormatSupported(VkFormat Format) const;

	const VkComponentMapping& GetFormatComponentMapping(EPixelFormat UEFormat) const;

	inline VkDevice GetInstanceHandle() const
	{
		return Device;
	}

	inline const FVulkanSamplerState& GetDefaultSampler() const
	{
		return *DefaultSampler;
	}

	inline const FVulkanTextureView& GetDefaultImageView() const
	{
		return DefaultTextureView;
	}

	inline const VkFormatProperties* GetFormatProperties() const
	{
		return FormatProperties;
	}

	inline VulkanRHI::FDeviceMemoryManager& GetMemoryManager()
	{
		return MemoryManager;
	}

	inline const VkPhysicalDeviceMemoryProperties& GetDeviceMemoryProperties() const
	{
		return MemoryManager.GetMemoryProperties();
	}

	inline VulkanRHI::FResourceHeapManager& GetResourceHeapManager()
	{
		return ResourceHeapManager;
	}

	inline VulkanRHI::FDeferredDeletionQueue& GetDeferredDeletionQueue()
	{
		return DeferredDeletionQueue;
	}

	inline VulkanRHI::FStagingManager& GetStagingManager()
	{
		return StagingManager;
	}

	inline VulkanRHI::FFenceManager& GetFenceManager()
	{
		return FenceManager;
	}

	inline FVulkanDescriptorSetCache& GetDescriptorSetCache()
	{
		return *DescriptorSetCache;
	}

	inline FVulkanDescriptorPoolsManager& GetDescriptorPoolsManager()
	{
		return *DescriptorPoolsManager;
	}

	inline TMap<uint32, FSamplerStateRHIRef>& GetSamplerMap()
	{
		return SamplerMap;
	}

	inline FVulkanShaderFactory& GetShaderFactory()
	{
		return ShaderFactory;
	}

	FVulkanCommandListContextImmediate& GetImmediateContext();

	inline FVulkanCommandListContext& GetImmediateComputeContext()
	{
		return *ComputeContext;
	}

	void NotifyDeletedRenderTarget(VkImage Image);
	void NotifyDeletedImage(VkImage Image);

#if VULKAN_ENABLE_DRAW_MARKERS
	inline PFN_vkCmdDebugMarkerBeginEXT GetCmdDbgMarkerBegin() const
	{
		return DebugMarkers.CmdBegin;
	}

	inline PFN_vkCmdDebugMarkerEndEXT GetCmdDbgMarkerEnd() const
	{
		return DebugMarkers.CmdEnd;
	}

	inline PFN_vkDebugMarkerSetObjectNameEXT GetDebugMarkerSetObjectName() const
	{
		return DebugMarkers.CmdSetObjectName;
	}

#if 0//VULKAN_SUPPORTS_DEBUG_UTILS
	inline PFN_vkCmdBeginDebugUtilsLabelEXT GetCmdBeginDebugLabel() const
	{
		return DebugMarkers.CmdBeginDebugLabel;
	}

	inline PFN_vkCmdEndDebugUtilsLabelEXT GetCmdEndDebugLabel() const
	{
		return DebugMarkers.CmdEndDebugLabel;
	}

	inline PFN_vkSetDebugUtilsObjectNameEXT GetSetDebugName() const
	{
		return DebugMarkers.SetDebugName;
	}
#endif

#endif

	void PrepareForCPURead();

	void SubmitCommandsAndFlushGPU();

	FVulkanOcclusionQueryPool* AcquireOcclusionQueryPool(uint32 NumQueries);
	void ReleaseUnusedOcclusionQueryPools();

	inline class FVulkanPipelineStateCacheManager* GetPipelineStateCache()
	{
		return PipelineStateCache;
	}

	void NotifyDeletedGfxPipeline(class FVulkanRHIGraphicsPipelineState* Pipeline);
	void NotifyDeletedComputePipeline(class FVulkanComputePipeline* Pipeline);

	FVulkanCommandListContext* AcquireDeferredContext();
	void ReleaseDeferredContext(FVulkanCommandListContext* InContext);

	inline const FOptionalVulkanDeviceExtensions& GetOptionalExtensions() const
	{
		return OptionalDeviceExtensions;
	}

#if VULKAN_SUPPORTS_GPU_CRASH_DUMPS
	VkBuffer GetCrashMarkerBuffer() const
	{
		return CrashMarker.Buffer;
	}

	void* GetCrashMarkerMappedPointer() const
	{
		return CrashMarker.Allocation->GetMappedPointer();
	}
#endif

	void SetupPresentQueue(VkSurfaceKHR Surface);

#if VULKAN_SUPPORTS_COLOR_CONVERSIONS
	VkSamplerYcbcrConversion CreateSamplerColorConversion(const VkSamplerYcbcrConversionCreateInfo& CreateInfo);
#endif

	void*	Hotfix;

private:
	void MapFormatSupport(EPixelFormat UEFormat, VkFormat VulkanFormat);
	void MapFormatSupportWithFallback(EPixelFormat UEFormat, VkFormat VulkanFormat, TArrayView<const VkFormat> FallbackTextureFormats);
	void MapFormatSupport(EPixelFormat UEFormat, VkFormat VulkanFormat, int32 BlockBytes);
	void SetComponentMapping(EPixelFormat UEFormat, VkComponentSwizzle r, VkComponentSwizzle g, VkComponentSwizzle b, VkComponentSwizzle a);

	void SubmitCommands(FVulkanCommandListContext* Context);


	VkDevice Device;

	VulkanRHI::FDeviceMemoryManager MemoryManager;

	VulkanRHI::FResourceHeapManager ResourceHeapManager;

	VulkanRHI::FDeferredDeletionQueue DeferredDeletionQueue;

	VulkanRHI::FStagingManager StagingManager;

	VulkanRHI::FFenceManager FenceManager;

	// Active on ES3.1
	FVulkanDescriptorSetCache* DescriptorSetCache = nullptr;
	// Active on >= SM4
	FVulkanDescriptorPoolsManager* DescriptorPoolsManager = nullptr;

	FVulkanShaderFactory ShaderFactory;

	FVulkanSamplerState* DefaultSampler;
	FVulkanSurface* DefaultImage;
	FVulkanTextureView DefaultTextureView;

	VkPhysicalDevice Gpu;
	VkPhysicalDeviceProperties GpuProps;
#if VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
	VkPhysicalDeviceIDPropertiesKHR GpuIdProps;
#endif
	VkPhysicalDeviceFeatures PhysicalFeatures;

	TArray<VkQueueFamilyProperties> QueueFamilyProps;
	VkFormatProperties FormatProperties[VK_FORMAT_RANGE_SIZE];
	// Info for formats that are not in the core Vulkan spec (i.e. extensions)
	mutable TMap<VkFormat, VkFormatProperties> ExtensionFormatProperties;

	TArray<FVulkanOcclusionQueryPool*> UsedOcclusionQueryPools;
	TArray<FVulkanOcclusionQueryPool*> FreeOcclusionQueryPools;

	uint64 TimestampValidBitsMask = 0;

	FVulkanQueue* GfxQueue;
	FVulkanQueue* ComputeQueue;
	FVulkanQueue* TransferQueue;
	FVulkanQueue* PresentQueue;
	bool bAsyncComputeQueue = false;
	bool bPresentOnComputeQueue = false;

#if VULKAN_SUPPORTS_GPU_CRASH_DUMPS
	struct
	{
		VkBuffer Buffer = VK_NULL_HANDLE;
		VulkanRHI::FDeviceMemoryAllocation* Allocation = nullptr;
	} CrashMarker;
#endif

	VkComponentMapping PixelFormatComponentMapping[PF_MAX];

	TMap<uint32, FSamplerStateRHIRef> SamplerMap;

	FVulkanCommandListContextImmediate* ImmediateContext;
	FVulkanCommandListContext* ComputeContext;
	TArray<FVulkanCommandListContext*> CommandContexts;
#if VULKAN_SUPPORTS_COLOR_CONVERSIONS
	TMap<uint32, VkSamplerYcbcrConversion> SamplerColorConversionMap;
#endif

	void GetDeviceExtensionsAndLayers(TArray<const ANSICHAR*>& OutDeviceExtensions, TArray<const ANSICHAR*>& OutDeviceLayers, bool& bOutDebugMarkers);

	void ParseOptionalDeviceExtensions(const TArray<const ANSICHAR*>& DeviceExtensions);
	FOptionalVulkanDeviceExtensions OptionalDeviceExtensions;

	void SetupFormats();

#if VULKAN_SUPPORTS_VALIDATION_CACHE
	VkValidationCacheEXT ValidationCache = VK_NULL_HANDLE;
#endif

#if VULKAN_ENABLE_DRAW_MARKERS
	struct
	{
		PFN_vkCmdDebugMarkerBeginEXT		CmdBegin = nullptr;
		PFN_vkCmdDebugMarkerEndEXT			CmdEnd = nullptr;
		PFN_vkDebugMarkerSetObjectNameEXT	CmdSetObjectName = nullptr;

#if 0//VULKAN_SUPPORTS_DEBUG_UTILS
		PFN_vkCmdBeginDebugUtilsLabelEXT	CmdBeginDebugLabel = nullptr;
		PFN_vkCmdEndDebugUtilsLabelEXT		CmdEndDebugLabel = nullptr;
		PFN_vkSetDebugUtilsObjectNameEXT	SetDebugName = nullptr;
#endif
	} DebugMarkers;
	friend class FVulkanCommandListContext;
#endif

	class FVulkanPipelineStateCacheManager* PipelineStateCache;
	friend class FVulkanDynamicRHI;
};
