// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanTexture.cpp: Vulkan texture RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanMemory.h"
#include "VulkanContext.h"
#include "VulkanPendingState.h"
#include "Containers/ResourceArray.h"
#include "VulkanLLM.h"


int32 GVulkanSubmitOnTextureUnlock = 1;
static FAutoConsoleVariableRef CVarVulkanSubmitOnTextureUnlock(
	TEXT("r.Vulkan.SubmitOnTextureUnlock"),
	GVulkanSubmitOnTextureUnlock,
	TEXT("Whether to submit upload cmd buffer on each texture unlock.\n")
	TEXT(" 0: Do not submit\n")
	TEXT(" 1: Submit (default)"),
	ECVF_Default
);

static FCriticalSection GTextureMapLock;

struct FTextureLock
{
	FRHIResource* Texture;
	uint32 MipIndex;
	uint32 LayerIndex;

	FTextureLock(FRHIResource* InTexture, uint32 InMipIndex, uint32 InLayerIndex = 0)
		: Texture(InTexture)
		, MipIndex(InMipIndex)
		, LayerIndex(InLayerIndex)
	{
	}
};

inline bool operator == (const FTextureLock& A, const FTextureLock& B)
{
	return A.Texture == B.Texture && A.MipIndex == B.MipIndex && A.LayerIndex == B.LayerIndex;
}

inline uint32 GetTypeHash(const FTextureLock& Lock)
{
	return GetTypeHash(Lock.Texture) ^ (Lock.MipIndex << 16) ^ (Lock.LayerIndex << 8);
}

static TMap<FTextureLock, VulkanRHI::FStagingBuffer*> GPendingLockedBuffers;

static const VkImageTiling GVulkanViewTypeTilingMode[VK_IMAGE_VIEW_TYPE_RANGE_SIZE] =
{
	VK_IMAGE_TILING_LINEAR,		// VK_IMAGE_VIEW_TYPE_1D
	VK_IMAGE_TILING_OPTIMAL,	// VK_IMAGE_VIEW_TYPE_2D
	VK_IMAGE_TILING_OPTIMAL,	// VK_IMAGE_VIEW_TYPE_3D
	VK_IMAGE_TILING_OPTIMAL,	// VK_IMAGE_VIEW_TYPE_CUBE
	VK_IMAGE_TILING_LINEAR,		// VK_IMAGE_VIEW_TYPE_1D_ARRAY
	VK_IMAGE_TILING_OPTIMAL,	// VK_IMAGE_VIEW_TYPE_2D_ARRAY
	VK_IMAGE_TILING_OPTIMAL,	// VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
};

static TStatId GetVulkanStatEnum(bool bIsCube, bool bIs3D, bool bIsRT)
{
#if STATS
	if (bIsRT == false)
	{
		// normal texture
		if (bIsCube)
		{
			return GET_STATID(STAT_TextureMemoryCube);
		}
		else if (bIs3D)
		{
			return GET_STATID(STAT_TextureMemory3D);
		}
		else
		{
			return GET_STATID(STAT_TextureMemory2D);
		}
	}
	else
	{
		// render target
		if (bIsCube)
		{
			return GET_STATID(STAT_RenderTargetMemoryCube);
		}
		else if (bIs3D)
		{
			return GET_STATID(STAT_RenderTargetMemory3D);
		}
		else
		{
			return GET_STATID(STAT_RenderTargetMemory2D);
		}
	}
#endif
	return TStatId();
}

static void UpdateVulkanTextureStats(int64 TextureSize, bool bIsCube, bool bIs3D, bool bIsRT)
{
	const int64 AlignedSize = (TextureSize > 0) ? Align(TextureSize, 1024) / 1024 : -(Align(-TextureSize, 1024) / 1024);
	if (bIsRT == false)
	{
		FPlatformAtomics::InterlockedAdd(&GCurrentTextureMemorySize, AlignedSize);
	}
	else
	{
		FPlatformAtomics::InterlockedAdd(&GCurrentRendertargetMemorySize, AlignedSize);
	}

	INC_MEMORY_STAT_BY_FName(GetVulkanStatEnum(bIsCube, bIs3D, bIsRT).GetName(), TextureSize);
}

static void VulkanTextureAllocated(uint64 Size, VkImageViewType ImageType, bool bIsRT)
{
	bool bIsCube = ImageType == VK_IMAGE_VIEW_TYPE_CUBE || ImageType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	bool bIs3D = ImageType == VK_IMAGE_VIEW_TYPE_3D ;

	UpdateVulkanTextureStats(Size, bIsCube, bIs3D, bIsRT);
}

static void VulkanTextureDestroyed(uint64 Size, VkImageViewType ImageTupe, bool bIsRT)
{
	bool bIsCube = ImageTupe == VK_IMAGE_VIEW_TYPE_CUBE || ImageTupe == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	bool bIs3D = ImageTupe == VK_IMAGE_VIEW_TYPE_3D;

	UpdateVulkanTextureStats(-(int64)Size, bIsCube, bIs3D, bIsRT);
}

inline void FVulkanSurface::InternalLockWrite(FVulkanCommandListContext& Context, FVulkanSurface* Surface, const VkImageSubresourceRange& SubresourceRange, const VkBufferImageCopy& Region, VulkanRHI::FStagingBuffer* StagingBuffer)
{
	FVulkanCmdBuffer* CmdBuffer = Context.GetCommandBufferManager()->GetUploadCmdBuffer();
	ensure(CmdBuffer->IsOutsideRenderPass());
	VkCommandBuffer StagingCommandBuffer = CmdBuffer->GetHandle();

	VulkanRHI::ImagePipelineBarrier(StagingCommandBuffer, Surface->Image, EImageLayoutBarrier::Undefined, EImageLayoutBarrier::TransferDest, SubresourceRange);

	VulkanRHI::vkCmdCopyBufferToImage(StagingCommandBuffer, StagingBuffer->GetHandle(), Surface->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

	VulkanRHI::ImagePipelineBarrier(StagingCommandBuffer, Surface->Image, EImageLayoutBarrier::TransferDest, EImageLayoutBarrier::PixelShaderRead, SubresourceRange);

	Context.GetTransitionAndLayoutManager().FindOrAddLayoutRW(Surface->Image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Surface->Device->GetStagingManager().ReleaseBuffer(CmdBuffer, StagingBuffer);

	if (GVulkanSubmitOnTextureUnlock != 0)
	{
		Context.GetCommandBufferManager()->SubmitUploadCmdBuffer();
	}
}

struct FRHICommandLockWriteTexture final : public FRHICommand<FRHICommandLockWriteTexture>
{
	FVulkanSurface* Surface;
	VkImageSubresourceRange SubresourceRange;
	VkBufferImageCopy Region;
	VulkanRHI::FStagingBuffer* StagingBuffer;

	FRHICommandLockWriteTexture(FVulkanSurface* InSurface, const VkImageSubresourceRange& InSubresourceRange, const VkBufferImageCopy& InRegion, VulkanRHI::FStagingBuffer* InStagingBuffer)
		: Surface(InSurface)
		, SubresourceRange(InSubresourceRange)
		, Region(InRegion)
		, StagingBuffer(InStagingBuffer)
	{
	}

	void Execute(FRHICommandListBase& RHICmdList)
	{
		FVulkanSurface::InternalLockWrite((FVulkanCommandListContext&)RHICmdList.GetContext(), Surface, SubresourceRange, Region, StagingBuffer);
	}
};

VkImage FVulkanSurface::CreateImage(
	FVulkanDevice& InDevice,
	VkImageViewType ResourceType,
	EPixelFormat InFormat,
	uint32 SizeX, uint32 SizeY, uint32 SizeZ,
	bool bArray, uint32 ArraySize,
	uint32 NumMips,
	uint32 NumSamples,
	uint32 UEFlags,
	VkMemoryRequirements& OutMemoryRequirements,
	VkFormat* OutStorageFormat,
	VkFormat* OutViewFormat,
	VkImageCreateInfo* OutInfo,
	bool bForceLinearTexture)
{
	const VkPhysicalDeviceProperties& DeviceProperties = InDevice.GetDeviceProperties();
	const FPixelFormatInfo& FormatInfo = GPixelFormats[InFormat];
	VkFormat TextureFormat = (VkFormat)FormatInfo.PlatformFormat;

	checkf(TextureFormat != VK_FORMAT_UNDEFINED, TEXT("PixelFormat %d, is not supported for images"), (int32)InFormat);

	VkImageCreateInfo TmpCreateInfo;
	VkImageCreateInfo* ImageCreateInfoPtr = OutInfo ? OutInfo : &TmpCreateInfo;
	VkImageCreateInfo& ImageCreateInfo = *ImageCreateInfoPtr;
	ZeroVulkanStruct(ImageCreateInfo, VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);

	switch(ResourceType)
	{
	case VK_IMAGE_VIEW_TYPE_1D:
		ImageCreateInfo.imageType = VK_IMAGE_TYPE_1D;
		check(SizeX <= DeviceProperties.limits.maxImageDimension1D);
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE:
	case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
		check(SizeX == SizeY);
		check(SizeX <= DeviceProperties.limits.maxImageDimensionCube);
		check(SizeY <= DeviceProperties.limits.maxImageDimensionCube);
		ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		break;
	case VK_IMAGE_VIEW_TYPE_2D:
	case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		check(SizeX <= DeviceProperties.limits.maxImageDimension2D);
		check(SizeY <= DeviceProperties.limits.maxImageDimension2D);
		ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		break;
	case VK_IMAGE_VIEW_TYPE_3D:
		check(SizeY <= DeviceProperties.limits.maxImageDimension3D);
		ImageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
		break;
	default:
		checkf(false, TEXT("Unhandled image type %d"), (int32)ResourceType);
		break;
	}

	ImageCreateInfo.format = UEToVkTextureFormat(InFormat, false);

	checkf(ImageCreateInfo.format != VK_FORMAT_UNDEFINED, TEXT("Pixel Format %d not defined!"), (int32)InFormat);
	if (OutStorageFormat)
	{
		*OutStorageFormat = ImageCreateInfo.format;
	}

	if (OutViewFormat)
	{
		VkFormat ViewFormat = UEToVkTextureFormat(InFormat, (UEFlags & TexCreate_SRGB) == TexCreate_SRGB);
		*OutViewFormat = ViewFormat;
		ImageCreateInfo.format = ViewFormat;
	}

	ImageCreateInfo.extent.width = SizeX;
	ImageCreateInfo.extent.height = SizeY;
	ImageCreateInfo.extent.depth = ResourceType == VK_IMAGE_VIEW_TYPE_3D ? SizeZ : 1;
	ImageCreateInfo.mipLevels = NumMips;
	uint32 LayerCount = (ResourceType == VK_IMAGE_VIEW_TYPE_CUBE || ResourceType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) ? 6 : 1;
	ImageCreateInfo.arrayLayers = (bArray ? ArraySize : 1) * LayerCount;
	check(ImageCreateInfo.arrayLayers <= DeviceProperties.limits.maxImageArrayLayers);

	ImageCreateInfo.flags = (ResourceType == VK_IMAGE_VIEW_TYPE_CUBE || ResourceType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	if ((UEFlags & TexCreate_SRGB) == TexCreate_SRGB)
	{
		ImageCreateInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	}

#if VULKAN_SUPPORTS_MAINTENANCE_LAYER1
	if (InDevice.GetOptionalExtensions().HasKHRMaintenance1 && ImageCreateInfo.imageType == VK_IMAGE_TYPE_3D)
	{
		ImageCreateInfo.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR;
	}
#endif

	ImageCreateInfo.tiling = bForceLinearTexture ? VK_IMAGE_TILING_LINEAR : GVulkanViewTypeTilingMode[ResourceType];

	ImageCreateInfo.usage = 0;
	ImageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	//@TODO: should everything be created with the source bit?
	ImageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	ImageCreateInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

	if (UEFlags & TexCreate_Presentable)
	{
		ImageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;		
	}
	else if (UEFlags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable))
	{
		if ((UEFlags & TexCreate_InputAttachmentRead) == TexCreate_InputAttachmentRead)
		{
			ImageCreateInfo.usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		}
		ImageCreateInfo.usage |= ((UEFlags & TexCreate_RenderTargetable) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
		ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	}
	else if (UEFlags & (TexCreate_DepthStencilResolveTarget))
	{
		ImageCreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	}
	else if (UEFlags & TexCreate_ResolveTargetable)
	{
		ImageCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	}
	
	if (UEFlags & TexCreate_UAV)
	{
		ImageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	}

	//#todo-rco: If using CONCURRENT, make sure to NOT do so on render targets as that kills DCC compression
	ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ImageCreateInfo.queueFamilyIndexCount = 0;
	ImageCreateInfo.pQueueFamilyIndices = nullptr;

	if (ImageCreateInfo.tiling == VK_IMAGE_TILING_LINEAR && NumSamples > 1)
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("Not allowed to create Linear textures with %d samples, reverting to 1 sample"), NumSamples);
		NumSamples = 1;
	}

	switch (NumSamples)
	{
	case 1:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		break;
	case 2:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_2_BIT;
		break;
	case 4:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_4_BIT;
		break;
	case 8:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_8_BIT;
		break;
	case 16:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_16_BIT;
		break;
	case 32:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_32_BIT;
		break;
	case 64:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_64_BIT;
		break;
	default:
		checkf(0, TEXT("Unsupported number of samples %d"), NumSamples);
		break;
	}

//#todo-rco: Verify flags work on newer Android drivers
#if !PLATFORM_ANDROID
	if (ImageCreateInfo.tiling == VK_IMAGE_TILING_LINEAR)
	{
		VkFormatFeatureFlags FormatFlags = InDevice.GetFormatProperties()[ImageCreateInfo.format].linearTilingFeatures;
		if ((FormatFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_SAMPLED_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_STORAGE_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_STORAGE_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
	}
	else if (ImageCreateInfo.tiling == VK_IMAGE_TILING_OPTIMAL)
	{
		VkFormatFeatureFlags FormatFlags = InDevice.GetFormatProperties()[ImageCreateInfo.format].optimalTilingFeatures;
		if ((FormatFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_SAMPLED_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_STORAGE_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_STORAGE_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
	}
#endif

	VkImage Image;
	VERIFYVULKANRESULT(VulkanRHI::vkCreateImage(InDevice.GetInstanceHandle(), &ImageCreateInfo, VULKAN_CPU_ALLOCATOR, &Image));

	// Fetch image size
	VulkanRHI::vkGetImageMemoryRequirements(InDevice.GetInstanceHandle(), Image, &OutMemoryRequirements);

	return Image;
}


struct FRHICommandInitialClearTexture final : public FRHICommand<FRHICommandInitialClearTexture>
{
	FVulkanSurface* Surface;
	FClearValueBinding ClearValueBinding;
	bool bTransitionToPresentable;

	FRHICommandInitialClearTexture(FVulkanSurface* InSurface, const FClearValueBinding& InClearValueBinding, bool bInTransitionToPresentable)
		: Surface(InSurface)
		, ClearValueBinding(InClearValueBinding)
		, bTransitionToPresentable(bInTransitionToPresentable)
	{
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		Surface->InitialClear((FVulkanCommandListContext&)CmdList.GetContext(), ClearValueBinding, bTransitionToPresentable);
	}
};

struct FRHICommandRegisterImageLayout final : public FRHICommand<FRHICommandRegisterImageLayout>
{
	VkImage Image;
	VkImageLayout ImageLayout;

	FRHICommandRegisterImageLayout(VkImage InImage, VkImageLayout InImageLayout)
		: Image(InImage)
		, ImageLayout(InImageLayout)
	{
	}

	void Execute(FRHICommandListBase& RHICmdList)
	{
		((FVulkanCommandListContext&)RHICmdList.GetContext()).FindOrAddLayout(Image, ImageLayout);
	}
};

static void InsertInitialImageLayout(FVulkanDevice& Device, VkImage InImage, VkImageLayout InLayout)
{
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	const bool bIsInRenderingThread = IsInRenderingThread();
	if (!bIsInRenderingThread || (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread()))
	{
		Device.GetImmediateContext().FindOrAddLayout(InImage, InLayout);
	}
	else
	{
		check(IsInRenderingThread());
		new (RHICmdList.AllocCommand<FRHICommandRegisterImageLayout>()) FRHICommandRegisterImageLayout(InImage, InLayout);
	}

	if (bIsInRenderingThread)
	{
		// Insert the RHI thread lock fence. This stops any parallel translate tasks running until the command above has completed on the RHI thread.
		RHICmdList.RHIThreadFence(true);
	}
}

struct FRHICommandOnDestroyImage final : public FRHICommand<FRHICommandOnDestroyImage>
{
	VkImage Image;
	FVulkanDevice* Device;

	FRHICommandOnDestroyImage(VkImage InImage, FVulkanDevice* InDevice)
		: Image(InImage)
		, Device(InDevice)
	{
	}

	void Execute(FRHICommandListBase& RHICmdList)
	{
		Device->NotifyDeletedImage(Image);
	}
};


FVulkanSurface::FVulkanSurface(FVulkanDevice& InDevice, VkImageViewType ResourceType, EPixelFormat InFormat,
								uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize, uint32 InNumMips,
								uint32 InNumSamples, uint32 InUEFlags, const FRHIResourceCreateInfo& CreateInfo)
	: Device(&InDevice)
	, Image(VK_NULL_HANDLE)
	, StorageFormat(VK_FORMAT_UNDEFINED)
	, ViewFormat(VK_FORMAT_UNDEFINED)
	, Width(SizeX)
	, Height(SizeY)
	, Depth(SizeZ)
	, PixelFormat(InFormat)
	, UEFlags(InUEFlags)
	, MemProps(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	, NumArrayLevels(0)
	, Tiling(VK_IMAGE_TILING_MAX_ENUM)	// Can be expanded to a per-platform definition
	, ViewType(ResourceType)
	, bIsImageOwner(true)
	, NumMips(InNumMips)
	, NumSamples(InNumSamples)
	, FullAspectMask(0)
	, PartialAspectMask(0)
{
	VkImageCreateInfo ImageCreateInfo;	// Zeroed inside CreateImage
	Image = FVulkanSurface::CreateImage(InDevice, ResourceType,
		InFormat, SizeX, SizeY, SizeZ,
		bArray, ArraySize, NumMips, NumSamples, UEFlags, MemoryRequirements,
		&StorageFormat, &ViewFormat,
		&ImageCreateInfo);

	uint32 LayerCount = (ResourceType == VK_IMAGE_VIEW_TYPE_CUBE || ResourceType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) ? 6 : 1;
	NumArrayLevels = (bArray ? ArraySize : 1) * LayerCount;

	FullAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(PixelFormat, true, true);
	PartialAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(PixelFormat, false, true);

	// If VK_IMAGE_TILING_OPTIMAL is specified,
	// memoryTypeBits in vkGetImageMemoryRequirements will become 1
	// which does not support VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT.
	if (ImageCreateInfo.tiling != VK_IMAGE_TILING_OPTIMAL)
	{
		MemProps |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	}

	const bool bRenderTarget = (UEFlags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable | TexCreate_ResolveTargetable)) != 0;
	const bool bCPUReadback = (UEFlags & TexCreate_CPUReadback) != 0;
	const bool bDynamic = (UEFlags & TexCreate_Dynamic) != 0;

#if VULKAN_SUPPORTS_DEDICATED_ALLOCATION
	// Per https://developer.nvidia.com/what%E2%80%99s-your-vulkan-memory-type
	VkDeviceSize SizeToBeConsideredForDedicated = 16 * 1024 * 1024;
	if ((bRenderTarget || MemoryRequirements.size >= SizeToBeConsideredForDedicated) && InDevice.GetOptionalExtensions().HasKHRDedicatedAllocation)
	{
		ResourceAllocation = InDevice.GetResourceHeapManager().AllocateDedicatedImageMemory(Image, MemoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, __FILE__, __LINE__);
	}
	else
#endif
	{
		ResourceAllocation = InDevice.GetResourceHeapManager().AllocateImageMemory(MemoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, __FILE__, __LINE__);
	}
	ResourceAllocation->BindImage(Device, Image);

	// update rhi stats
	VulkanTextureAllocated(MemoryRequirements.size, ResourceType, bRenderTarget);

	Tiling = ImageCreateInfo.tiling;
	check(Tiling == VK_IMAGE_TILING_LINEAR || Tiling == VK_IMAGE_TILING_OPTIMAL);

	if ((ImageCreateInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT) && (UEFlags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable)))
	{
		const bool bTransitionToPresentable = ((UEFlags & TexCreate_Presentable) == TexCreate_Presentable);

		FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		if (!IsInRenderingThread() || (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread()))
		{
			InitialClear(Device->GetImmediateContext(), CreateInfo.ClearValueBinding, bTransitionToPresentable);
		}
		else
		{
			check(IsInRenderingThread());
			ALLOC_COMMAND_CL(RHICmdList, FRHICommandInitialClearTexture)(this, CreateInfo.ClearValueBinding, bTransitionToPresentable);
		}
	}
}

// This is usually used for the framebuffer image
FVulkanSurface::FVulkanSurface(FVulkanDevice& InDevice, VkImageViewType ResourceType, EPixelFormat InFormat,
								uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 InNumMips, uint32 InNumSamples,
								VkImage InImage, uint32 InUEFlags, const FRHIResourceCreateInfo& CreateInfo)
	: Device(&InDevice)
	, Image(InImage)
	, StorageFormat(VK_FORMAT_UNDEFINED)
	, ViewFormat(VK_FORMAT_UNDEFINED)
	, Width(SizeX)
	, Height(SizeY)
	, Depth(SizeZ)
	, PixelFormat(InFormat)
	, UEFlags(InUEFlags)
	, MemProps(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	, NumArrayLevels(0)
	, Tiling(VK_IMAGE_TILING_MAX_ENUM)	// Can be expanded to a per-platform definition
	, ViewType(ResourceType)
	, bIsImageOwner(false)
	, NumMips(InNumMips)
	, NumSamples(InNumSamples)
	, FullAspectMask(0)
	, PartialAspectMask(0)
{
	StorageFormat = (VkFormat)GPixelFormats[PixelFormat].PlatformFormat;
	check((UEFlags & TexCreate_SRGB) == 0);
	checkf(PixelFormat == PF_Unknown || StorageFormat != VK_FORMAT_UNDEFINED, TEXT("PixelFormat %d, is not supported for images"), (int32)PixelFormat);

	ViewFormat = StorageFormat;
	FullAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(PixelFormat, true, true);
	PartialAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(PixelFormat, false, true);

	// Purely informative patching, we know that "TexCreate_Presentable" uses optimal tiling
	if ((UEFlags & TexCreate_Presentable) == TexCreate_Presentable && GetTiling() == VK_IMAGE_TILING_MAX_ENUM)
	{
		Tiling = VK_IMAGE_TILING_OPTIMAL;
	}
	
	if (Image != VK_NULL_HANDLE)
	{
		if (UEFlags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable))
		{
			const bool bTransitionToPresentable = ((UEFlags & TexCreate_Presentable) == TexCreate_Presentable);
			InitialClear(InDevice.GetImmediateContext(), CreateInfo.ClearValueBinding, bTransitionToPresentable);
		}
	}
}

FVulkanSurface::~FVulkanSurface()
{
	Destroy();
}

void FVulkanSurface::Destroy()
{
	// An image can be instances.
	// - Instances VkImage has "bIsImageOwner" set to "false".
	// - Owner of VkImage has "bIsImageOwner" set to "true".
	if (bIsImageOwner)
	{
		FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		if (!IsInRenderingThread() || (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread()))
		{
			Device->NotifyDeletedImage(Image);
		}
		else
		{
			check(IsInRenderingThread());
			new (RHICmdList.AllocCommand<FRHICommandOnDestroyImage>()) FRHICommandOnDestroyImage(Image, Device);
		}
				
		bIsImageOwner = false;

		uint64 Size = 0;

		if (Image != VK_NULL_HANDLE)
		{
			Size = GetMemorySize();
			Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::Image, Image);
			Image = VK_NULL_HANDLE;
		}

		const bool bRenderTarget = (UEFlags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable | TexCreate_ResolveTargetable)) != 0;
		VulkanTextureDestroyed(Size, ViewType, bRenderTarget);
	}
}

#if 0
void* FVulkanSurface::Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride)
{
	DestStride = 0;

	check((MemProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0 ? Tiling == VK_IMAGE_TILING_OPTIMAL : Tiling == VK_IMAGE_TILING_LINEAR);

	// Verify all buffers are unmapped
	auto& Data = MipMapMapping.FindOrAdd(MipIndex);
	checkf(Data == nullptr, TEXT("The buffer needs to be unmapped, before it can be mapped"));

	// Get the layout of the subresource
	VkImageSubresource ImageSubResource;
	FMemory::Memzero(ImageSubResource);

	ImageSubResource.aspectMask = GetAspectMask();
	ImageSubResource.mipLevel = MipIndex;
	ImageSubResource.arrayLayer = ArrayIndex;

	// Get buffer size
	// Pitch can be only retrieved from linear textures.
	VkSubresourceLayout SubResourceLayout;
	VulkanRHI::vkGetImageSubresourceLayout(Device->GetInstanceHandle(), Image, &ImageSubResource, &SubResourceLayout);

	// Set linear row-pitch
	GetMipStride(MipIndex, DestStride);

	if(Tiling == VK_IMAGE_TILING_LINEAR)
	{
		// Verify pitch if linear
		check(DestStride == SubResourceLayout.rowPitch);

		// Map buffer to a pointer
		Data = Allocation->Map(SubResourceLayout.size, SubResourceLayout.offset);
		return Data;
	}

	// From here on, the code is dedicated to optimal textures

	// Verify all buffers are unmapped
	TRefCountPtr<FVulkanBuffer>& LinearBuffer = MipMapBuffer.FindOrAdd(MipIndex);
	checkf(LinearBuffer == nullptr, TEXT("The buffer needs to be unmapped, before it can be mapped"));

	// Create intermediate buffer which is going to be used to perform buffer to image copy
	// The copy buffer is always one face and single mip level.
	const uint32 Layers = 1;
	const uint32 Bytes = SubResourceLayout.size * Layers;

	VkBufferUsageFlags Usage = 0;
	Usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VkMemoryPropertyFlags Flags = 0;
	Flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT;

	LinearBuffer = new FVulkanBuffer(*Device, Bytes, Usage, Flags, false, __FILE__, __LINE__);

	void* DataPtr = LinearBuffer->Lock(Bytes);
	check(DataPtr);

	return DataPtr;
}

void FVulkanSurface::Unlock(uint32 MipIndex, uint32 ArrayIndex)
{
	check((MemProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0 ? Tiling == VK_IMAGE_TILING_OPTIMAL : Tiling == VK_IMAGE_TILING_LINEAR);

	if(Tiling == VK_IMAGE_TILING_LINEAR)
	{
		void*& Data = MipMapMapping.FindOrAdd(MipIndex);
		checkf(Data != nullptr, TEXT("The buffer needs to be mapped, before it can be unmapped"));

		Allocation->Unmap();
		Data = nullptr;
		return;
	}

	TRefCountPtr<FVulkanBuffer>& LinearBuffer = MipMapBuffer.FindOrAdd(MipIndex);
	checkf(LinearBuffer != nullptr, TEXT("The buffer needs to be mapped, before it can be unmapped"));
	LinearBuffer->Unlock();

	VkImageSubresource ImageSubResource;
	FMemory::Memzero(ImageSubResource);
	ImageSubResource.aspectMask = GetAspectMask();
	ImageSubResource.mipLevel = MipIndex;
	ImageSubResource.arrayLayer = ArrayIndex;

	VkSubresourceLayout SubResourceLayout;
	
	VulkanRHI::vkGetImageSubresourceLayout(Device->GetInstanceHandle(), Image, &ImageSubResource, &SubResourceLayout);

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	Region.bufferOffset = 0;
	Region.bufferRowLength = (Width >> MipIndex);
	Region.bufferImageHeight = (Height >> MipIndex);

	// The data/image is always parsed per one face.
	// Meaning that a cubemap will have atleast 6 locks/unlocks
	Region.imageSubresource.baseArrayLayer = ArrayIndex;	// Layer/face copy destination
	Region.imageSubresource.layerCount = 1;	// Indicates number of arrays in the buffer, this is also the amount of "faces/layers" to be copied
	Region.imageSubresource.aspectMask = GetAspectMask();
	Region.imageSubresource.mipLevel = MipIndex;

	Region.imageExtent.width = Region.bufferRowLength;
	Region.imageExtent.height = Region.bufferImageHeight;
	Region.imageExtent.depth = 1;

	LinearBuffer->CopyTo(*this, Region, nullptr);

	// Release buffer
	LinearBuffer = nullptr;
}
#endif

void FVulkanSurface::GetMipStride(uint32 MipIndex, uint32& Stride)
{
	// Calculate the width of the MipMap.
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 MipSizeX = FMath::Max(Width >> MipIndex, BlockSizeX);
	uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;

	if (PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4)
	{
		// PVRTC has minimum 2 blocks width
		NumBlocksX = FMath::Max<uint32>(NumBlocksX, 2);
	}

	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;

	Stride = NumBlocksX * BlockBytes;
}

void FVulkanSurface::GetMipOffset(uint32 MipIndex, uint32& Offset)
{
	uint32 offset = Offset = 0;
	for(uint32 i = 0; i < MipIndex; i++)
	{
		GetMipSize(i, offset);
		Offset += offset;
	}
}

void FVulkanSurface::GetMipSize(uint32 MipIndex, uint32& MipBytes)
{
	// Calculate the dimensions of mip-map level.
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	const uint32 MipSizeX = FMath::Max(Width >> MipIndex, BlockSizeX);
	const uint32 MipSizeY = FMath::Max(Height >> MipIndex, BlockSizeY);
	uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
	uint32 NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;

	if (PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4)
	{
		// PVRTC has minimum 2 blocks width and height
		NumBlocksX = FMath::Max<uint32>(NumBlocksX, 2);
		NumBlocksY = FMath::Max<uint32>(NumBlocksY, 2);
	}

	// Size in bytes
	MipBytes = NumBlocksX * NumBlocksY * BlockBytes;
/*
#if VULKAN_HAS_DEBUGGING_ENABLED
	VkImageSubresource SubResource;
	FMemory::Memzero(SubResource);
	SubResource.aspectMask = FullAspectMask;
	SubResource.mipLevel = MipIndex;
	//SubResource.arrayLayer = 0;
	VkSubresourceLayout OutLayout;
	VulkanRHI::vkGetImageSubresourceLayout(Device->GetInstanceHandle(), Image, &SubResource, &OutLayout);
	ensure(MipBytes >= OutLayout.size);
#endif
*/
}

void FVulkanSurface::InitialClear(FVulkanCommandListContext& Context,const FClearValueBinding& ClearValueBinding, bool bTransitionToPresentable)
{
	// Can't use TransferQueue as Vulkan requires that queue to also have Gfx or Compute capabilities...
	//#todo-rco: This function is only used during loading currently, if used for regular RHIClear then use the ActiveCmdBuffer
	FVulkanCmdBuffer* CmdBuffer = Context.GetCommandBufferManager()->GetUploadCmdBuffer();
	ensure(CmdBuffer->IsOutsideRenderPass());

	VulkanRHI::FPendingBarrier Barrier;
	int32 BarrierIndex = Barrier.AddImageBarrier(Image, FullAspectMask, NumMips);
	Barrier.GetSubresource(BarrierIndex).layerCount = (ViewType == VK_IMAGE_VIEW_TYPE_CUBE ? 6 : 1);

	// Undefined -> Dest Optimal
	Barrier.SetTransition(BarrierIndex, EImageLayoutBarrier::Undefined, EImageLayoutBarrier::TransferDest);
	Barrier.Execute(CmdBuffer);

	if (FullAspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
	{
		VkClearColorValue Color;
		FMemory::Memzero(Color);
		Color.float32[0] = ClearValueBinding.Value.Color[0];
		Color.float32[1] = ClearValueBinding.Value.Color[1];
		Color.float32[2] = ClearValueBinding.Value.Color[2];
		Color.float32[3] = ClearValueBinding.Value.Color[3];

		// Clear
		VulkanRHI::vkCmdClearColorImage(CmdBuffer->GetHandle(), Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &Color, 1, &Barrier.GetSubresource(BarrierIndex));

		// Transfer to Present or Color
		Barrier.ResetStages();
		Barrier.SetTransition(BarrierIndex, EImageLayoutBarrier::TransferDest, bTransitionToPresentable ? EImageLayoutBarrier::Present : EImageLayoutBarrier::ColorAttachment);
		Barrier.Execute(CmdBuffer);
	}
	else
	{
		check(IsDepthOrStencilAspect());
		ensure(!bTransitionToPresentable);
		VkClearDepthStencilValue Value;
		FMemory::Memzero(Value);
		Value.depth = ClearValueBinding.Value.DSValue.Depth;
		Value.stencil = ClearValueBinding.Value.DSValue.Stencil;

		// Clear
		VulkanRHI::vkCmdClearDepthStencilImage(CmdBuffer->GetHandle(), Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &Value, 1, &Barrier.GetSubresource(BarrierIndex));

		// General -> DepthStencil
		Barrier.ResetStages();
		Barrier.SetTransition(BarrierIndex, EImageLayoutBarrier::TransferDest, EImageLayoutBarrier::DepthStencilAttachment);
		Barrier.Execute(CmdBuffer);
	}

	VkImageLayout FinalLayout = Barrier.GetDestLayout(BarrierIndex);
	Context.FindOrAddLayoutRW(Image, FinalLayout) = FinalLayout;
}

/*-----------------------------------------------------------------------------
	Texture allocator support.
-----------------------------------------------------------------------------*/

void FVulkanDynamicRHI::RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats)
{
	check(Device);
	const uint64 TotalGPUMemory = Device->GetMemoryManager().GetTotalMemory(true);
	const uint64 TotalCPUMemory = Device->GetMemoryManager().GetTotalMemory(false);

	OutStats.DedicatedVideoMemory = TotalGPUMemory;
	OutStats.DedicatedSystemMemory = TotalCPUMemory;
	OutStats.SharedSystemMemory = -1;
	OutStats.TotalGraphicsMemory = TotalGPUMemory ? TotalGPUMemory : -1;

	OutStats.AllocatedMemorySize = int64(GCurrentTextureMemorySize) * 1024;
	OutStats.LargestContiguousAllocation = OutStats.AllocatedMemorySize;
	OutStats.TexturePoolSize = GTexturePoolSize;
	OutStats.PendingMemoryAdjustment = 0;
}

bool FVulkanDynamicRHI::RHIGetTextureMemoryVisualizeData( FColor* /*TextureData*/, int32 /*SizeX*/, int32 /*SizeY*/, int32 /*Pitch*/, int32 /*PixelSize*/ )
{
	VULKAN_SIGNAL_UNIMPLEMENTED();

	return false;
}

uint32 FVulkanDynamicRHI::RHIComputeMemorySize(FTextureRHIParamRef TextureRHI)
{
	if(!TextureRHI)
	{
		return 0;
	}

	return FVulkanTextureBase::Cast(TextureRHI)->Surface.GetMemorySize();
}

/*-----------------------------------------------------------------------------
	2D texture support.
-----------------------------------------------------------------------------*/

FTexture2DRHIRef FVulkanDynamicRHI::RHICreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)

{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	return new FVulkanTexture2D(*Device, (EPixelFormat)Format, SizeX, SizeY, NumMips, NumSamples, Flags, CreateInfo);
}

FTexture2DRHIRef FVulkanDynamicRHI::RHIAsyncCreateTexture2D(uint32 SizeX,uint32 SizeY,uint8 Format,uint32 NumMips,uint32 Flags,void** InitialMipData,uint32 NumInitialMips)
{
	UE_LOG(LogVulkan, Fatal, TEXT("RHIAsyncCreateTexture2D is not supported"));
	VULKAN_SIGNAL_UNIMPLEMENTED(); // Unsupported atm
	return FTexture2DRHIRef();
}

void FVulkanDynamicRHI::RHICopySharedMips(FTexture2DRHIParamRef DestTexture2D,FTexture2DRHIParamRef SrcTexture2D)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

FTexture2DArrayRHIRef FVulkanDynamicRHI::RHICreateTexture2DArray(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	return new FVulkanTexture2DArray(*Device, (EPixelFormat)Format, SizeX, SizeY, SizeZ, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
}

FTexture3DRHIRef FVulkanDynamicRHI::RHICreateTexture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTexture3D* Tex3d = new FVulkanTexture3D(*Device, (EPixelFormat)Format, SizeX, SizeY, SizeZ, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);

	return Tex3d;
}

void FVulkanDynamicRHI::RHIGetResourceInfo(FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo)
{
	FVulkanTextureBase* Base = (FVulkanTextureBase*)Ref->GetTextureBaseRHI();
	OutInfo.VRamAllocation.AllocationSize = Base->Surface.GetMemorySize();
}

static void DoAsyncReallocateTexture2D(FVulkanCommandListContext& Context, FVulkanTexture2D* OldTexture, FVulkanTexture2D* NewTexture, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	//QUICK_SCOPE_CYCLE_COUNTER(STAT_FRHICommandGnmAsyncReallocateTexture2D_Execute);
	check(Context.IsImmediate());

	// figure out what mips to copy from/to
	const uint32 NumSharedMips = FMath::Min(OldTexture->GetNumMips(), NewTexture->GetNumMips());
	const uint32 SourceFirstMip = OldTexture->GetNumMips() - NumSharedMips;
	const uint32 DestFirstMip = NewTexture->GetNumMips() - NumSharedMips;

	FVulkanCmdBuffer* CmdBuffer = Context.GetCommandBufferManager()->GetUploadCmdBuffer();
	ensure(CmdBuffer->IsOutsideRenderPass());

	VkCommandBuffer StagingCommandBuffer = CmdBuffer->GetHandle();

	VkImageCopy Regions[MAX_TEXTURE_MIP_COUNT];
	check(NumSharedMips <= MAX_TEXTURE_MIP_COUNT);
	FMemory::Memzero(&Regions[0], sizeof(VkImageCopy) * NumSharedMips);
	for (uint32 Index = 0; Index < NumSharedMips; ++Index)
	{
		uint32 MipWidth = FMath::Max<uint32>(NewSizeX >> (DestFirstMip + Index), 1u);
		uint32 MipHeight = FMath::Max<uint32>(NewSizeY >> (DestFirstMip + Index), 1u);

		VkImageCopy& Region = Regions[Index];
		Region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Region.srcSubresource.mipLevel = SourceFirstMip + Index;
		Region.srcSubresource.baseArrayLayer = 0;
		Region.srcSubresource.layerCount = 1;
		//Region.srcOffset
		Region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Region.dstSubresource.mipLevel = DestFirstMip + Index;
		Region.dstSubresource.baseArrayLayer = 0;
		Region.dstSubresource.layerCount = 1;
		//Region.dstOffset
		Region.extent.width = MipWidth;
		Region.extent.height = MipHeight;
		Region.extent.depth = 1;
	}

	{
		// Pre-copy barriers
		FPendingBarrier Barrier;
		{
			int32 BarrierIndex = Barrier.AddImageBarrier(NewTexture->Surface.Image, VK_IMAGE_ASPECT_COLOR_BIT, NumSharedMips);
			Barrier.GetSubresource(BarrierIndex).baseMipLevel = DestFirstMip;
			Barrier.SetTransition(BarrierIndex, EImageLayoutBarrier::Undefined, VulkanRHI::EImageLayoutBarrier::TransferDest);
		}

		VkImageLayout OldTextureLayout = Context.GetTransitionAndLayoutManager().FindOrAddLayout(OldTexture->Surface.Image, VK_IMAGE_LAYOUT_UNDEFINED);
		ensure(OldTextureLayout != VK_IMAGE_LAYOUT_UNDEFINED);
		if (OldTextureLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			int32 BarrierIndex = Barrier.AddImageBarrier(OldTexture->Surface.Image, VK_IMAGE_ASPECT_COLOR_BIT, NumSharedMips);
			Barrier.GetSubresource(BarrierIndex).baseMipLevel = SourceFirstMip;
			Barrier.SetTransition(BarrierIndex, VulkanRHI::GetImageLayoutFromVulkanLayout(OldTextureLayout), VulkanRHI::EImageLayoutBarrier::TransferSource);
		}

		Barrier.Execute(CmdBuffer);
	}
	Context.GetTransitionAndLayoutManager().FindOrAddLayoutRW(OldTexture->Surface.Image, VK_IMAGE_LAYOUT_UNDEFINED) = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

	VulkanRHI::vkCmdCopyImage(StagingCommandBuffer, OldTexture->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, NewTexture->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, NumSharedMips, Regions);

	{
		// Post-copy barriers
		FPendingBarrier Barrier;
		int32 BarrierIndex = Barrier.AddImageBarrier(NewTexture->Surface.Image, VK_IMAGE_ASPECT_COLOR_BIT, NumSharedMips);
		Barrier.GetSubresource(BarrierIndex).baseMipLevel = DestFirstMip;
		Barrier.SetTransition(BarrierIndex, EImageLayoutBarrier::TransferDest, VulkanRHI::EImageLayoutBarrier::PixelShaderRead);
		Barrier.Execute(CmdBuffer);
	}
	Context.GetTransitionAndLayoutManager().FindOrAddLayoutRW(NewTexture->Surface.Image, VK_IMAGE_LAYOUT_UNDEFINED) = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// request is now complete
	RequestStatus->Decrement();

	// the next unlock for this texture can't block the GPU (it's during runtime)
	//NewTexture->Surface.bSkipBlockOnUnlock = true;
}

struct FRHICommandVulkanAsyncReallocateTexture2D final : public FRHICommand<FRHICommandVulkanAsyncReallocateTexture2D>
{
	FVulkanCommandListContext& Context;
	FVulkanTexture2D* OldTexture;
	FVulkanTexture2D* NewTexture;
	int32 NewMipCount;
	int32 NewSizeX;
	int32 NewSizeY;
	FThreadSafeCounter* RequestStatus;

	FORCEINLINE_DEBUGGABLE FRHICommandVulkanAsyncReallocateTexture2D(FVulkanCommandListContext& InContext, FVulkanTexture2D* InOldTexture, FVulkanTexture2D* InNewTexture, int32 InNewMipCount, int32 InNewSizeX, int32 InNewSizeY, FThreadSafeCounter* InRequestStatus)
		: Context(InContext)
		, OldTexture(InOldTexture)
		, NewTexture(InNewTexture)
		, NewMipCount(InNewMipCount)
		, NewSizeX(InNewSizeX)
		, NewSizeY(InNewSizeY)
		, RequestStatus(InRequestStatus)
	{
	}

	void Execute(FRHICommandListBase& RHICmdList)
	{
		ensure(&((FVulkanCommandListContext&)RHICmdList.GetContext()) == &Context);
		DoAsyncReallocateTexture2D(Context, OldTexture, NewTexture, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
	}
};

FTexture2DRHIRef FVulkanDynamicRHI::AsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef OldTextureRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	if (RHICmdList.Bypass())
	{
		return FDynamicRHI::AsyncReallocateTexture2D_RenderThread(RHICmdList, OldTextureRHI, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
	}

	FVulkanTexture2D* OldTexture = ResourceCast(OldTextureRHI);

	FRHIResourceCreateInfo CreateInfo;
	FVulkanTexture2D* NewTexture = new FVulkanTexture2D(*Device, OldTexture->GetFormat(), NewSizeX, NewSizeY, NewMipCount, OldTexture->GetNumSamples(), OldTexture->GetFlags(), CreateInfo);

	ALLOC_COMMAND_CL(RHICmdList, FRHICommandVulkanAsyncReallocateTexture2D)(Device->GetImmediateContext(), OldTexture, NewTexture, NewMipCount, NewSizeX, NewSizeY, RequestStatus);

	return NewTexture;
}

FTexture2DRHIRef FVulkanDynamicRHI::RHIAsyncReallocateTexture2D(FTexture2DRHIParamRef OldTextureRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTexture2D* OldTexture = ResourceCast(OldTextureRHI);

	FRHIResourceCreateInfo CreateInfo;
	FVulkanTexture2D* NewTexture = new FVulkanTexture2D(*Device, OldTexture->GetFormat(), NewSizeX, NewSizeY, NewMipCount, OldTexture->GetNumSamples(), OldTexture->GetFlags(), CreateInfo);

	DoAsyncReallocateTexture2D(Device->GetImmediateContext(), OldTexture, NewTexture, NewMipCount, NewSizeX, NewSizeY, RequestStatus);

	return NewTexture;
}

ETextureReallocationStatus FVulkanDynamicRHI::RHIFinalizeAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
{
	return TexRealloc_Succeeded;
}

ETextureReallocationStatus FVulkanDynamicRHI::RHICancelAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
{
	return TexRealloc_Succeeded;
}

void* FVulkanDynamicRHI::RHILockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTexture2D* Texture = ResourceCast(TextureRHI);
	check(Texture);

	VulkanRHI::FStagingBuffer** StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		StagingBuffer = &GPendingLockedBuffers.FindOrAdd(FTextureLock(TextureRHI, MipIndex));
		checkf(!*StagingBuffer, TEXT("Can't lock the same texture twice!"));
	}

	// No locks for read allowed yet
	check(LockMode == RLM_WriteOnly);

	uint32 BufferSize = 0;
	DestStride = 0;
	Texture->Surface.GetMipSize(MipIndex, BufferSize);
	Texture->Surface.GetMipStride(MipIndex, DestStride);
	*StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize);

	void* Data = (*StagingBuffer)->GetMappedPointer();
	return Data;
}

void FVulkanDynamicRHI::InternalUnlockTexture2D(bool bFromRenderingThread, FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,bool bLockWithinMiptail)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTexture2D* Texture = ResourceCast(TextureRHI);
	check(Texture);

	VkDevice LogicalDevice = Device->GetInstanceHandle();

	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		bool bFound = GPendingLockedBuffers.RemoveAndCopyValue(FTextureLock(TextureRHI, MipIndex), StagingBuffer);
		checkf(bFound, TEXT("Texture was not locked!"));
	}

	EPixelFormat Format = Texture->Surface.PixelFormat;
	uint32 MipWidth = FMath::Max<uint32>(Texture->Surface.Width >> MipIndex, GPixelFormats[Format].BlockSizeX);
	uint32 MipHeight = FMath::Max<uint32>(Texture->Surface.Height >> MipIndex, GPixelFormats[Format].BlockSizeY);

	VkImageSubresourceRange SubresourceRange;
	FMemory::Memzero(SubresourceRange);
	SubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	SubresourceRange.baseMipLevel = MipIndex;
	SubresourceRange.levelCount = 1;
	//SubresourceRange.baseArrayLayer = 0;
	SubresourceRange.layerCount = 1;

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	//#todo-rco: Might need an offset here?
	//Region.bufferOffset = 0;
	//Region.bufferRowLength = 0;
	//Region.bufferImageHeight = 0;
	Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Region.imageSubresource.mipLevel = MipIndex;
	//Region.imageSubresource.baseArrayLayer = 0;
	Region.imageSubresource.layerCount = 1;
	Region.imageExtent.width = MipWidth;
	Region.imageExtent.height = MipHeight;
	Region.imageExtent.depth = 1;

	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (!bFromRenderingThread || (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread()))
	{
		FVulkanSurface::InternalLockWrite(Device->GetImmediateContext(), &Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
	else
	{
		check(IsInRenderingThread());
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandLockWriteTexture)(&Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
}

void* FVulkanDynamicRHI::RHILockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTexture2DArray* Texture = ResourceCast(TextureRHI);
	check(Texture);

	VulkanRHI::FStagingBuffer** StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		StagingBuffer = &GPendingLockedBuffers.FindOrAdd(FTextureLock(TextureRHI, MipIndex, TextureIndex));
		checkf(!*StagingBuffer, TEXT("Can't lock the same texture twice!"));
	}

	uint32 BufferSize = 0;
	DestStride = 0;
	Texture->Surface.GetMipSize(MipIndex, BufferSize);
	Texture->Surface.GetMipStride(MipIndex, DestStride);
	*StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize);

	void* Data = (*StagingBuffer)->GetMappedPointer();
	return Data;
}

void FVulkanDynamicRHI::RHIUnlockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTexture2DArray* Texture = ResourceCast(TextureRHI);
	check(Texture);

	VkDevice LogicalDevice = Device->GetInstanceHandle();

	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		bool bFound = GPendingLockedBuffers.RemoveAndCopyValue(FTextureLock(TextureRHI, MipIndex, TextureIndex), StagingBuffer);
		checkf(bFound, TEXT("Texture was not locked!"));
	}

	EPixelFormat Format = Texture->Surface.PixelFormat;
	uint32 MipWidth = FMath::Max<uint32>(Texture->Surface.Width >> MipIndex, GPixelFormats[Format].BlockSizeX);
	uint32 MipHeight = FMath::Max<uint32>(Texture->Surface.Height >> MipIndex, GPixelFormats[Format].BlockSizeY);

	VkImageSubresourceRange SubresourceRange;
	FMemory::Memzero(SubresourceRange);
	SubresourceRange.aspectMask = Texture->Surface.GetPartialAspectMask();
	SubresourceRange.baseMipLevel = MipIndex;
	SubresourceRange.levelCount = 1;
	SubresourceRange.baseArrayLayer = TextureIndex;
	SubresourceRange.layerCount = 1;

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	//#todo-rco: Might need an offset here?
	//Region.bufferOffset = 0;
	//Region.bufferRowLength = 0;
	//Region.bufferImageHeight = 0;
	Region.imageSubresource.aspectMask = Texture->Surface.GetPartialAspectMask();
	Region.imageSubresource.mipLevel = MipIndex;
	Region.imageSubresource.baseArrayLayer = TextureIndex;
	Region.imageSubresource.layerCount = 1;
	Region.imageExtent.width = MipWidth;
	Region.imageExtent.height = MipHeight;
	Region.imageExtent.depth = 1;

	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		FVulkanSurface::InternalLockWrite(Device->GetImmediateContext(), &Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
	else
	{
		check(IsInRenderingThread());
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandLockWriteTexture)(&Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
}

void FVulkanDynamicRHI::InternalUpdateTexture2D(bool bFromRenderingThread, FTexture2DRHIParamRef TextureRHI, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourceRowPitch, const uint8* SourceData)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTexture2D* Texture = ResourceCast(TextureRHI);

	EPixelFormat PixelFormat = Texture->GetFormat();
	const int32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const int32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const int32 BlockSizeZ = GPixelFormats[PixelFormat].BlockSizeZ;
	const int32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	VkFormat Format = UEToVkTextureFormat(PixelFormat, false);

	ensure(BlockSizeZ == 1);

	FVulkanCommandListContext& Context = Device->GetImmediateContext();
	const VkPhysicalDeviceLimits& Limits = Device->GetLimits();

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	const uint32 NumBlocksX = (uint32)FMath::DivideAndRoundUp<int32>(UpdateRegion.Width, (uint32)BlockSizeX);
	const uint32 NumBlocksY = (uint32)FMath::DivideAndRoundUp<int32>(UpdateRegion.Height, (uint32)BlockSizeY);
	ensure(NumBlocksX * BlockBytes <= SourceRowPitch);

	const uint32 DestRowPitch = NumBlocksX * BlockBytes;
	const uint32 DestSlicePitch = DestRowPitch * NumBlocksY;

	const uint32 BufferSize = Align(DestSlicePitch, Limits.minMemoryMapAlignment);
	StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize);
	void* RESTRICT Memory = StagingBuffer->GetMappedPointer();

	VkImageSubresourceRange SubresourceRange;
	FMemory::Memzero(SubresourceRange);
	SubresourceRange.aspectMask = Texture->Surface.GetFullAspectMask();
	SubresourceRange.baseMipLevel = MipIndex;
	SubresourceRange.levelCount = 1;
	//SubresourceRange.baseArrayLayer = 0;
	SubresourceRange.layerCount = 1;

	uint8* RESTRICT DestData = (uint8*)Memory;
	uint8* RESTRICT SourceRowData = (uint8*)SourceData;
	for (uint32 Height = 0; Height < NumBlocksY; ++Height)
	{
		FMemory::Memcpy(DestData, SourceRowData, NumBlocksX * BlockBytes);
		DestData += DestRowPitch;
		SourceRowData += SourceRowPitch;
	}

	//Region.bufferOffset = 0;
	// Set these to zero to assume tightly packed buffer
	//Region.bufferRowLength = 0;
	//Region.bufferImageHeight = 0;
	Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Region.imageSubresource.mipLevel = MipIndex;
	//Region.imageSubresource.baseArrayLayer = 0;
	Region.imageSubresource.layerCount = 1;
	Region.imageOffset.x = UpdateRegion.DestX;
	Region.imageOffset.y = UpdateRegion.DestY;
	//Region.imageOffset.z = 0;
	Region.imageExtent.width = UpdateRegion.Width;
	Region.imageExtent.height = UpdateRegion.Height;
	Region.imageExtent.depth = 1;

	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (!bFromRenderingThread || (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread()))
	{
		FVulkanSurface::InternalLockWrite(Device->GetImmediateContext(), &Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
	else
	{
		check(IsInRenderingThread());
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandLockWriteTexture)(&Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
}

void FVulkanDynamicRHI::InternalUpdateTexture3D(bool bFromRenderingThread, FTexture3DRHIParamRef TextureRHI, uint32 MipIndex, const FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTexture3D* Texture = ResourceCast(TextureRHI);

	const EPixelFormat PixelFormat = Texture->GetFormat();
	const int32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const int32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const int32 BlockSizeZ = GPixelFormats[PixelFormat].BlockSizeZ;
	const int32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	const VkFormat Format = UEToVkTextureFormat(PixelFormat, false);

	ensure(BlockSizeZ == 1);

	FVulkanCommandListContext& Context = Device->GetImmediateContext();
	const VkPhysicalDeviceLimits& Limits = Device->GetLimits();

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	const uint32 NumBlocksX = (uint32)FMath::DivideAndRoundUp<int32>(UpdateRegion.Width, (uint32)BlockSizeX);
	const uint32 NumBlocksY = (uint32)FMath::DivideAndRoundUp<int32>(UpdateRegion.Height, (uint32)BlockSizeY);
	check(NumBlocksX * BlockBytes <= SourceRowPitch);
	check(NumBlocksX * BlockBytes * NumBlocksY <= SourceDepthPitch);

	const uint32 DestRowPitch = NumBlocksX * BlockBytes;
	const uint32 DestSlicePitch = DestRowPitch * NumBlocksY;

	const uint32 BufferSize = Align(DestSlicePitch * UpdateRegion.Depth, Limits.minMemoryMapAlignment);
	StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize);
	void* RESTRICT Memory = StagingBuffer->GetMappedPointer();

	VkImageSubresourceRange SubresourceRange;
	FMemory::Memzero(SubresourceRange);
	SubresourceRange.aspectMask = Texture->Surface.GetFullAspectMask();
	SubresourceRange.baseMipLevel = MipIndex;
	SubresourceRange.levelCount = 1;
	//SubresourceRange.baseArrayLayer = 0;
	SubresourceRange.layerCount = 1;

	ensure(UpdateRegion.SrcX == 0);
	ensure(UpdateRegion.SrcY == 0);

	uint8* RESTRICT DestData = (uint8*)Memory;
	for (uint32 Depth = 0; Depth < UpdateRegion.Depth; Depth++)
	{
		uint8* RESTRICT SourceRowData = (uint8*)SourceData + SourceDepthPitch * Depth;
		for (uint32 Height = 0; Height < NumBlocksY; ++Height)
		{
			FMemory::Memcpy(DestData, SourceRowData, NumBlocksX * BlockBytes);
			DestData += DestRowPitch;
			SourceRowData += SourceRowPitch;
		}
	}

	//Region.bufferOffset = 0;
	// Set these to zero to assume tightly packed buffer
	//Region.bufferRowLength = 0;
	//Region.bufferImageHeight = 0;
	Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Region.imageSubresource.mipLevel = MipIndex;
	//Region.imageSubresource.baseArrayLayer = 0;
	Region.imageSubresource.layerCount = 1;
	Region.imageOffset.x = UpdateRegion.DestX;
	Region.imageOffset.y = UpdateRegion.DestY;
	Region.imageOffset.z = UpdateRegion.DestZ;
	Region.imageExtent.width = UpdateRegion.Width;
	Region.imageExtent.height = UpdateRegion.Height;
	Region.imageExtent.depth = UpdateRegion.Depth;

	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (!bFromRenderingThread || (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread()))
	{
		FVulkanSurface::InternalLockWrite(Device->GetImmediateContext(), &Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
	else
	{
		check(IsInRenderingThread());
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandLockWriteTexture)(&Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
}


VkImageView FVulkanTextureView::StaticCreate(FVulkanDevice& Device, VkImage InImage, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat UEFormat, VkFormat Format, uint32 FirstMip, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices, bool bUseIdentitySwizzle, const FSamplerYcbcrConversionInitializer* ConversionInitializer)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	VkImageView OutView = VK_NULL_HANDLE;

	VkImageViewCreateInfo ViewInfo;
	ZeroVulkanStruct(ViewInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
	ViewInfo.image = InImage;
	ViewInfo.viewType = ViewType;
	ViewInfo.format = Format;
	if (bUseIdentitySwizzle)
	{
		// VK_COMPONENT_SWIZZLE_IDENTITY == 0 and this was memzero'd already
	}
	else
	{
		ViewInfo.components = Device.GetFormatComponentMapping(UEFormat);
	}

#if VULKAN_SUPPORTS_COLOR_CONVERSIONS
	VkSamplerYcbcrConversionInfo ConversionInfo;
	if (ConversionInitializer != nullptr)
	{
		VkSamplerYcbcrConversionCreateInfo ConversionCreateInfo;
		FMemory::Memzero(&ConversionCreateInfo, sizeof(VkSamplerYcbcrConversionCreateInfo));
		ConversionCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
		ConversionCreateInfo.format = ConversionInitializer->Format;

		ConversionCreateInfo.components.a = ConversionInitializer->Components.a;
		ConversionCreateInfo.components.r = ConversionInitializer->Components.r;
		ConversionCreateInfo.components.g = ConversionInitializer->Components.g;
		ConversionCreateInfo.components.b = ConversionInitializer->Components.b;

		ConversionCreateInfo.ycbcrModel = ConversionInitializer->Model;
		ConversionCreateInfo.ycbcrRange = ConversionInitializer->Range;
		ConversionCreateInfo.xChromaOffset = ConversionInitializer->XOffset;
		ConversionCreateInfo.yChromaOffset = ConversionInitializer->YOffset;
		ConversionCreateInfo.chromaFilter = VK_FILTER_NEAREST;
		ConversionCreateInfo.forceExplicitReconstruction = VK_FALSE;

		check(ConversionInitializer->Format != VK_FORMAT_UNDEFINED); // No support for VkExternalFormatANDROID yet.

		FMemory::Memzero(&ConversionInfo, sizeof(VkSamplerYcbcrConversionInfo));
		ConversionInfo.conversion = Device.CreateSamplerColorConversion(ConversionCreateInfo);
		ConversionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO;
		ViewInfo.pNext = &ConversionInfo;
	}
#endif

	ViewInfo.subresourceRange.aspectMask = AspectFlags;
	ViewInfo.subresourceRange.baseMipLevel = FirstMip;
	ensure(NumMips != 0xFFFFFFFF);
	ViewInfo.subresourceRange.levelCount = NumMips;

	auto CheckUseNvidiaWorkaround = []() -> bool
	{
		if (IsRHIDeviceNVIDIA())
		{
			if(FParse::Param(FCommandLine::Get(), TEXT("rtx20xxmipworkaround")))
			{
				// Workaround for 20xx family not copying last mips correctly, so instead the view is created without the last 1x1 and 2x2 mips
				if (GRHIAdapterName.Contains(TEXT("RTX 20")))
				{
					return true;
				}
			}
		}
		return false;
	};
	static bool NvidiaWorkaround = CheckUseNvidiaWorkaround();
	if(NvidiaWorkaround && Format >= VK_FORMAT_BC1_RGB_UNORM_BLOCK && Format <= VK_FORMAT_BC7_SRGB_BLOCK && NumMips > 1)
	{
		ViewInfo.subresourceRange.levelCount = FMath::Max(1, int32(NumMips) - 2);
	}

	ensure(ArraySliceIndex != 0xFFFFFFFF);
	ViewInfo.subresourceRange.baseArrayLayer = ArraySliceIndex;
	ensure(NumArraySlices != 0xFFFFFFFF);
	switch (ViewType)
	{
	case VK_IMAGE_VIEW_TYPE_3D:
		ViewInfo.subresourceRange.layerCount = 1;
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE:
		ensure(NumArraySlices == 1);
		ViewInfo.subresourceRange.layerCount = 6;
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
		ViewInfo.subresourceRange.layerCount = 6 * NumArraySlices;
		break;
	case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
	case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		ViewInfo.subresourceRange.layerCount = NumArraySlices;
		break;
	default:
		ViewInfo.subresourceRange.layerCount = 1;
		break;
	}

	//HACK.  DX11 on PC currently uses a D24S8 depthbuffer and so needs an X24_G8 SRV to visualize stencil.
	//So take that as our cue to visualize stencil.  In the future, the platform independent code will have a real format
	//instead of PF_DepthStencil, so the cross-platform code could figure out the proper format to pass in for this.
	if (UEFormat == PF_X24_G8)
	{
		ensure(ViewInfo.format == VK_FORMAT_UNDEFINED);
		ViewInfo.format = (VkFormat)GPixelFormats[PF_DepthStencil].PlatformFormat;
		ensure(ViewInfo.format != VK_FORMAT_UNDEFINED);
		ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	INC_DWORD_STAT(STAT_VulkanNumImageViews);
	VERIFYVULKANRESULT(VulkanRHI::vkCreateImageView(Device.GetInstanceHandle(), &ViewInfo, VULKAN_CPU_ALLOCATOR, &OutView));

	return OutView;
}

void FVulkanTextureView::Create(FVulkanDevice& Device, VkImage InImage, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat UEFormat, VkFormat Format, uint32 FirstMip, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices, bool bUseIdentitySwizzle)
{
	View = StaticCreate(Device, InImage, ViewType, AspectFlags, UEFormat, Format, FirstMip, NumMips, ArraySliceIndex, NumArraySlices, bUseIdentitySwizzle, nullptr);
	Image = InImage;
	
	if (UseVulkanDescriptorCache())
	{
		ViewId = ++GVulkanImageViewHandleIdCounter;
	}
/*
	switch (AspectFlags)
	{
	case VK_IMAGE_ASPECT_DEPTH_BIT:
	case VK_IMAGE_ASPECT_STENCIL_BIT:
	case VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT:
		Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		break;
	default:
		ensure(0);
	case VK_IMAGE_ASPECT_COLOR_BIT:
		Layout = VK_IMAGE_LAYOUT_GENERAL;
		break;
	}
*/
}

void FVulkanTextureView::Create(FVulkanDevice& Device, VkImage InImage, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat UEFormat, VkFormat Format, uint32 FirstMip, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices, FSamplerYcbcrConversionInitializer& ConversionInitializer, bool bUseIdentitySwizzle)
{
	View = StaticCreate(Device, InImage, ViewType, AspectFlags, UEFormat, Format, FirstMip, NumMips, ArraySliceIndex, NumArraySlices, bUseIdentitySwizzle, &ConversionInitializer);
	Image = InImage;
	
	if (UseVulkanDescriptorCache())
	{
		ViewId = ++GVulkanImageViewHandleIdCounter;
	}
}

void FVulkanTextureView::Destroy(FVulkanDevice& Device)
{
	if (View)
	{
		DEC_DWORD_STAT(STAT_VulkanNumImageViews);
		Device.GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::ImageView, View);
		Image = VK_NULL_HANDLE;
		View = VK_NULL_HANDLE;
		ViewId = 0;
	}
}

FVulkanTextureBase::FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat InFormat, uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo)
	#if !VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	: Surface(Device, ResourceType, InFormat, SizeX, SizeY, SizeZ, bArray, ArraySize, NumMips, NumSamples, UEFlags, CreateInfo)
	, PartialView(nullptr)
	#else
	: Surface(Device, ResourceType, InFormat, SizeX, SizeY, SizeZ, bArray, ArraySize, NumMips, (UEFlags & TexCreate_DepthStencilTargetable) ? NumSamples : 1, UEFlags, CreateInfo)
	, PartialView(nullptr)
	, MSAASurface(nullptr)
	#endif
	, bIsAliased(false)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	if (Surface.ViewFormat == VK_FORMAT_UNDEFINED)
	{
		Surface.StorageFormat = UEToVkTextureFormat(InFormat, false);
		Surface.ViewFormat = UEToVkTextureFormat(InFormat, (UEFlags & TexCreate_SRGB) == TexCreate_SRGB);
		checkf(Surface.StorageFormat != VK_FORMAT_UNDEFINED, TEXT("Pixel Format %d not defined!"), (int32)InFormat);
	}

	if (ResourceType != VK_IMAGE_VIEW_TYPE_MAX_ENUM)
	{
		DefaultView.Create(Device, Surface.Image, ResourceType, Surface.GetFullAspectMask(), Surface.PixelFormat, Surface.ViewFormat, 0, FMath::Max(NumMips, 1u), 0, bArray ? FMath::Max(1u, ArraySize) : FMath::Max(1u, SizeZ));
	}

#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	// Create MSAA surface. The surface above is the resolve target
	if (NumSamples > 1 && (UEFlags & TexCreate_RenderTargetable) && !(UEFlags & TexCreate_DepthStencilTargetable))
	{
		MSAASurface = new FVulkanSurface(Device, ResourceType, InFormat, SizeX, SizeY, SizeZ, /*bArray=*/ false, 1, NumMips, NumSamples, UEFlags, CreateInfo);
		if (ResourceType != VK_IMAGE_VIEW_TYPE_MAX_ENUM)
		{
			MSAAView.Create(Device, MSAASurface->Image, ResourceType, MSAASurface->GetFullAspectMask(), MSAASurface->PixelFormat, MSAASurface->ViewFormat, 0, FMath::Max(NumMips, 1u), 0, bArray ? FMath::Max(1u, ArraySize) : FMath::Max(1u, SizeZ));
		}
	}
#endif

	if (Surface.FullAspectMask == Surface.PartialAspectMask)
	{
		PartialView = &DefaultView;
	}
	else
	{
		PartialView = new FVulkanTextureView;
		PartialView->Create(Device, Surface.Image, Surface.ViewType, Surface.PartialAspectMask, Surface.PixelFormat, Surface.ViewFormat, 0, FMath::Max(NumMips, 1u), 0, bArray ? FMath::Max(1u, ArraySize) : FMath::Max(1u, SizeZ));
	}

	if (!CreateInfo.BulkData)
	{
		FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

		// No initial data, so undefined
		InsertInitialImageLayout(Device, Surface.Image, VK_IMAGE_LAYOUT_UNDEFINED);
		return;
	}

	// Transfer bulk data
	VulkanRHI::FStagingBuffer* StagingBuffer = Device.GetStagingManager().AcquireBuffer(CreateInfo.BulkData->GetResourceBulkDataSize());
	void* Data = StagingBuffer->GetMappedPointer();

	// Do copy
	FMemory::Memcpy(Data, CreateInfo.BulkData->GetResourceBulkData(), CreateInfo.BulkData->GetResourceBulkDataSize());
	CreateInfo.BulkData->Discard();

	uint32 LayersPerArrayIndex = (ResourceType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY || ResourceType == VK_IMAGE_VIEW_TYPE_CUBE) ? 6 : 1;

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	//#todo-rco: Use real Buffer offset when switching to suballocations!
	Region.bufferOffset = 0;
	Region.bufferRowLength = Surface.Width;
	Region.bufferImageHeight = Surface.Height;
	
	Region.imageSubresource.mipLevel = 0;
	Region.imageSubresource.baseArrayLayer = 0;
	Region.imageSubresource.layerCount = ArraySize * LayersPerArrayIndex;
	Region.imageSubresource.aspectMask = Surface.GetFullAspectMask();

	Region.imageExtent.width = Region.bufferRowLength;
	Region.imageExtent.height = Region.bufferImageHeight;
	Region.imageExtent.depth = Surface.Depth;

	VkImageSubresourceRange SubresourceRange;
	FMemory::Memzero(SubresourceRange);
	SubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//SubresourceRange.baseMipLevel = 0;
	SubresourceRange.levelCount = Surface.GetNumMips();
	//SubresourceRange.baseArrayLayer = 0;
	SubresourceRange.layerCount = ArraySize * LayersPerArrayIndex;

	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		FVulkanSurface::InternalLockWrite(Device.GetImmediateContext(), &Surface, SubresourceRange, Region, StagingBuffer);
	}
	else
	{
		check(IsInRenderingThread());
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandLockWriteTexture)(&Surface, SubresourceRange, Region, StagingBuffer);
	}
}

FVulkanTextureBase::FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, VkImage InImage, VkDeviceMemory InMem, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo)
	: Surface(Device, ResourceType, Format, SizeX, SizeY, SizeZ, InNumMips, InNumSamples, InImage, UEFlags, CreateInfo)
	, PartialView(nullptr)
	#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	, MSAASurface(nullptr)
	#endif
	, bIsAliased(false)
{
	check(InMem == VK_NULL_HANDLE);
	if (ResourceType != VK_IMAGE_VIEW_TYPE_MAX_ENUM && Surface.Image != VK_NULL_HANDLE)
	{
		DefaultView.Create(Device, Surface.Image, ResourceType, Surface.GetFullAspectMask(), Format, Surface.ViewFormat, 0, FMath::Max(Surface.NumMips, 1u), 0, 1u);
	}

#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	// Create MSAA surface. The surface above is the resolve target
	if (InNumSamples == 1 && InNumSamplesTileMem > 1 && (UEFlags & TexCreate_RenderTargetable) && !(UEFlags & TexCreate_DepthStencilTargetable))
	{
		MSAASurface = new FVulkanSurface(Device, ResourceType, Format, SizeX, SizeY, SizeZ, /*bArray=*/ false, 1, InNumMips, InNumSamplesTileMem, UEFlags, CreateInfo);
		if (ResourceType != VK_IMAGE_VIEW_TYPE_MAX_ENUM)
		{
			MSAAView.Create(Device, MSAASurface->Image, ResourceType, MSAASurface->GetFullAspectMask(), MSAASurface->PixelFormat, MSAASurface->ViewFormat, 0, FMath::Max(InNumMips, 1u), 0, FMath::Max(1u, SizeZ));
		}
	}
#endif

	if (Surface.FullAspectMask == Surface.PartialAspectMask)
	{
		PartialView = &DefaultView;
	}
	else
	{
		PartialView = new FVulkanTextureView;
		PartialView->Create(Device, Surface.Image, Surface.ViewType, Surface.PartialAspectMask, Surface.PixelFormat, Surface.ViewFormat, 0, FMath::Max(InNumMips, 1u), 0, FMath::Max(1u, SizeZ));
	}
}

FVulkanTextureBase::FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 NumSamples, VkImage InImage, VkDeviceMemory InMem, FSamplerYcbcrConversionInitializer& ConversionInitializer, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo)
	: Surface(Device, ResourceType, Format, SizeX, SizeY, SizeZ, NumMips, NumSamples, InImage, UEFlags, CreateInfo)
	, PartialView(nullptr)
#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	, MSAASurface(nullptr)
#endif
	, bIsAliased(false)
{
	check(InMem == VK_NULL_HANDLE);

	Surface.ViewFormat = ConversionInitializer.Format;
	Surface.StorageFormat = ConversionInitializer.Format;

	if (ResourceType != VK_IMAGE_VIEW_TYPE_MAX_ENUM && Surface.Image != VK_NULL_HANDLE)
	{
		DefaultView.Create(Device, Surface.Image, ResourceType, Surface.GetFullAspectMask(), Format, Surface.ViewFormat, 0, FMath::Max(Surface.NumMips, 1u), 0, 1u, ConversionInitializer);
	}

	// No MSAA support
	check(NumSamples == 1);
	check(!(UEFlags & TexCreate_RenderTargetable));

	if (Surface.FullAspectMask == Surface.PartialAspectMask)
	{
		PartialView = &DefaultView;
	}
	else
	{
		PartialView = new FVulkanTextureView;
		PartialView->Create(Device, Surface.Image, Surface.ViewType, Surface.PartialAspectMask, Surface.PixelFormat, Surface.ViewFormat, 0, FMath::Max(NumMips, 1u), 0, FMath::Max(1u, SizeZ), ConversionInitializer);
	}

	// Since this is provided from an external image, assume it's ready for read
	InsertInitialImageLayout(Device, InImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

FVulkanTextureBase::~FVulkanTextureBase()
{
	DestroyViews();

	if (PartialView != &DefaultView)
	{
		delete PartialView;
	}

#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	if (MSAASurface)
	{
		delete MSAASurface;
		MSAASurface = nullptr;
	}
#endif
}

void FVulkanTextureBase::AliasTextureResources(const FVulkanTextureBase* SrcTexture)
{
	DestroyViews();

	Surface.Destroy();
	Surface.Image = SrcTexture->Surface.Image;
	DefaultView.View = SrcTexture->DefaultView.View;
	DefaultView.Image = SrcTexture->DefaultView.Image;
	DefaultView.ViewId = SrcTexture->DefaultView.ViewId;

	if (PartialView != &DefaultView)
	{
		PartialView->View = SrcTexture->PartialView->View;
		PartialView->Image = SrcTexture->PartialView->Image;
		PartialView->ViewId = SrcTexture->PartialView->ViewId;
	}

#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	if (MSAASurface)
	{
		MSAASurface->Destroy();
		MSAASurface->Image = SrcTexture->MSAASurface->Image;
		MSAAView.View = SrcTexture->MSAAView.View;
		MSAAView.Image = SrcTexture->MSAAView.Image;
		MSAAView.ViewId = SrcTexture->MSAAView.ViewId;
	}
#endif

	bIsAliased = true;
}

void FVulkanTextureBase::DestroyViews()
{
	if (!bIsAliased)
	{
		DefaultView.Destroy(*Surface.Device);

		if (PartialView != &DefaultView)
		{
			PartialView->Destroy(*Surface.Device);
		}

#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
		MSAAView.Destroy(*Surface.Device);
#endif
	}
}


FVulkanTexture2D::FVulkanTexture2D(FVulkanDevice& Device, EPixelFormat InFormat, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo)
:	FRHITexture2D(SizeX, SizeY, FMath::Max(NumMips, 1u), NumSamples, InFormat, UEFlags, CreateInfo.ClearValueBinding)
,	FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_2D, InFormat, SizeX, SizeY, 1, /*bArray=*/ false, 1, FMath::Max(NumMips, 1u), NumSamples, UEFlags, CreateInfo)
{
}

FVulkanTexture2D::FVulkanTexture2D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 NumSamplesTileMem, VkImage Image, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo)
:	FRHITexture2D(SizeX, SizeY, NumMips, NumSamples, Format, UEFlags, CreateInfo.ClearValueBinding)
,	FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_2D, Format, SizeX, SizeY, 1, NumMips, NumSamples, NumSamplesTileMem, Image, VK_NULL_HANDLE, UEFlags)
{
}

FVulkanTexture2D::FVulkanTexture2D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, VkImage Image, FSamplerYcbcrConversionInitializer& ConversionInitializer, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo)
	: FRHITexture2D(SizeX, SizeY, NumMips, NumSamples, Format, UEFlags, CreateInfo.ClearValueBinding)
	, FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_2D, Format, SizeX, SizeY, 1, NumMips, NumSamples, Image, VK_NULL_HANDLE, ConversionInitializer, UEFlags)
{
}

FVulkanTexture2D::~FVulkanTexture2D()
{
	if ((Surface.UEFlags & (TexCreate_DepthStencilTargetable | TexCreate_RenderTargetable)) != 0)
	{
		Surface.Device->NotifyDeletedRenderTarget(Surface.Image);
	}
}


FVulkanBackBuffer::FVulkanBackBuffer(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 UEFlags)
	: FVulkanTexture2D(Device, Format, SizeX, SizeY, 1, 1, UEFlags, FRHIResourceCreateInfo())
{
}

FVulkanBackBuffer::FVulkanBackBuffer(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, VkImage Image, uint32 UEFlags)
	: FVulkanTexture2D(Device, Format, SizeX, SizeY, 1, 1, 1, Image, UEFlags, FRHIResourceCreateInfo())
{
}

FVulkanBackBuffer::~FVulkanBackBuffer()
{
	if (Surface.IsImageOwner() == false)
	{
		Surface.Device->NotifyDeletedRenderTarget(Surface.Image);
		// Clear flags so ~FVulkanTexture2D() doesn't try to re-destroy it
		Surface.UEFlags = 0;
		DefaultView.View = VK_NULL_HANDLE;
		DefaultView.ViewId = 0;
		Surface.Image = VK_NULL_HANDLE;
	}
}


FVulkanTexture2DArray::FVulkanTexture2DArray(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
	:	FRHITexture2DArray(SizeX, SizeY, ArraySize, NumMips, Format, Flags, InClearValue)
	,	FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_2D_ARRAY, Format, SizeX, SizeY, 1, /*bArray=*/ true, ArraySize, NumMips, /*NumSamples=*/ 1, Flags, BulkData)
{
}

FVulkanTexture2DArray::FVulkanTexture2DArray(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, VkImage Image, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
	:	FRHITexture2DArray(SizeX, SizeY, ArraySize, NumMips, Format, Flags, InClearValue)
	,	FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_2D_ARRAY, Format, SizeX, SizeY, 1, NumMips, /*NumSamples=*/ 1, /*NumSamplesTileMem=*/ 1, Image, VK_NULL_HANDLE, Flags, BulkData)
{
}


void FVulkanTextureReference::SetReferencedTexture(FRHITexture* InTexture)
{
	FRHITextureReference::SetReferencedTexture(InTexture);
}


FVulkanTextureCube::FVulkanTextureCube(FVulkanDevice& Device, EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
:	 FRHITextureCube(Size, NumMips, Format, Flags, InClearValue)
	//#todo-rco: Array/slices count
,	FVulkanTextureBase(Device, bArray ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE, Format, Size, Size, 1, bArray, ArraySize, NumMips, /*NumSamples=*/ 1, Flags, BulkData)
{
}

FVulkanTextureCube::FVulkanTextureCube(FVulkanDevice& Device, EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, VkImage Image, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
	:	 FRHITextureCube(Size, NumMips, Format, Flags, InClearValue)
	//#todo-rco: Array/slices count
	,	FVulkanTextureBase(Device, bArray ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE, Format, Size, Size, 1, NumMips, /*NumSamples=*/ 1, /*NumSamplesTileMem=*/ 1, Image, VK_NULL_HANDLE, Flags, BulkData)
{
}

FVulkanTextureCube::~FVulkanTextureCube()
{
	if ((GetFlags() & (TexCreate_DepthStencilTargetable | TexCreate_RenderTargetable)) != 0)
	{
		Surface.Device->NotifyDeletedRenderTarget(Surface.Image);
	}
}


FVulkanTexture3D::FVulkanTexture3D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
	: FRHITexture3D(SizeX, SizeY, SizeZ, NumMips, Format, Flags, InClearValue)
	, FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_3D, Format, SizeX, SizeY, SizeZ, /*bArray=*/ false, 1, NumMips, /*NumSamples=*/ 1, Flags, BulkData)
{
}

FVulkanTexture3D::~FVulkanTexture3D()
{
	if ((GetFlags() & (TexCreate_DepthStencilTargetable | TexCreate_RenderTargetable)) != 0)
	{
		Surface.Device->NotifyDeletedRenderTarget(Surface.Image);
	}
}

/*-----------------------------------------------------------------------------
	Cubemap texture support.
-----------------------------------------------------------------------------*/
FTextureCubeRHIRef FVulkanDynamicRHI::RHICreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	return new FVulkanTextureCube(*Device, (EPixelFormat)Format, Size, false, 1, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
}

FTextureCubeRHIRef FVulkanDynamicRHI::RHICreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	return new FVulkanTextureCube(*Device, (EPixelFormat)Format, Size, true, ArraySize, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
}

void* FVulkanDynamicRHI::RHILockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTextureCube* Texture = ResourceCast(TextureCubeRHI);
	check(Texture);

	VulkanRHI::FStagingBuffer** StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		StagingBuffer = &GPendingLockedBuffers.FindOrAdd(FTextureLock(TextureCubeRHI, MipIndex));
		checkf(!*StagingBuffer, TEXT("Can't lock the same texture twice!"));
	}

	uint32 BufferSize = 0;
	DestStride = 0;
	Texture->Surface.GetMipSize(MipIndex, BufferSize);
	Texture->Surface.GetMipStride(MipIndex, DestStride);
	*StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize);

	void* Data = (*StagingBuffer)->GetMappedPointer();
	return Data;
}

void FVulkanDynamicRHI::RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	FVulkanTextureCube* Texture = ResourceCast(TextureCubeRHI);
	check(Texture);

	VkDevice LogicalDevice = Device->GetInstanceHandle();

	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		bool bFound = GPendingLockedBuffers.RemoveAndCopyValue(FTextureLock(TextureCubeRHI, MipIndex), StagingBuffer);
		checkf(bFound, TEXT("Texture was not locked!"));
	}

	EPixelFormat Format = Texture->Surface.PixelFormat;
	uint32 MipWidth = FMath::Max<uint32>(Texture->Surface.Width >> MipIndex, GPixelFormats[Format].BlockSizeX);
	uint32 MipHeight = FMath::Max<uint32>(Texture->Surface.Height >> MipIndex, GPixelFormats[Format].BlockSizeY);

	VkImageSubresourceRange SubresourceRange;
	FMemory::Memzero(SubresourceRange);
	SubresourceRange.aspectMask = Texture->Surface.GetPartialAspectMask();
	SubresourceRange.baseMipLevel = MipIndex;
	SubresourceRange.levelCount = 1;
	SubresourceRange.baseArrayLayer = ArrayIndex * 6 + FaceIndex;
	SubresourceRange.layerCount = 1;

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	//#todo-rco: Might need an offset here?
	//Region.bufferOffset = 0;
	//Region.bufferRowLength = 0;
	//Region.bufferImageHeight = 0;
	Region.imageSubresource.aspectMask = Texture->Surface.GetPartialAspectMask();
	Region.imageSubresource.mipLevel = MipIndex;
	Region.imageSubresource.baseArrayLayer = ArrayIndex * 6 + FaceIndex;
	Region.imageSubresource.layerCount = 1;
	Region.imageExtent.width = MipWidth;
	Region.imageExtent.height = MipHeight;
	Region.imageExtent.depth = 1;

	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		FVulkanSurface::InternalLockWrite(Device->GetImmediateContext(), &Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
	else
	{
		check(IsInRenderingThread());
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandLockWriteTexture)(&Texture->Surface, SubresourceRange, Region, StagingBuffer);
	}
}

void FVulkanDynamicRHI::RHIBindDebugLabelName(FTextureRHIParamRef TextureRHI, const TCHAR* Name)
{
#if VULKAN_ENABLE_IMAGE_TRACKING_LAYER
	{
		FVulkanTextureBase* Base = (FVulkanTextureBase*)TextureRHI->GetTextureBaseRHI();
		VulkanRHI::BindDebugLabelName(Base->Surface.Image, Name);
	}
#endif

#if VULKAN_ENABLE_DUMP_LAYER || VULKAN_ENABLE_API_DUMP
	{
// TODO: this dies in the printf on android. Needs investigation.
#if !PLATFORM_ANDROID
		FVulkanTextureBase* Base = (FVulkanTextureBase*)TextureRHI->GetTextureBaseRHI();
#if VULKAN_ENABLE_DUMP_LAYER
		VulkanRHI::PrintfBegin
#elif VULKAN_ENABLE_API_DUMP
		FPlatformMisc::LowLevelOutputDebugStringf
#endif
			(*FString::Printf(TEXT("vkDebugMarkerSetObjectNameEXT(0x%p=%s)\n"), Base->Surface.Image, Name));
#endif
	}
#endif

#if VULKAN_ENABLE_DRAW_MARKERS
#if 0//VULKAN_SUPPORTS_DEBUG_UTILS
	if (auto* SetDebugName = Device->GetSetDebugName())
	{
		FVulkanTextureBase* Base = (FVulkanTextureBase*)TextureRHI->GetTextureBaseRHI();
		FTCHARToUTF8 Converter(Name);
		VulkanRHI::SetDebugName(SetDebugName, Device->GetInstanceHandle(), Base->Surface.Image, Converter.Get());
	}
	else
#endif
	if (auto* SetObjectName = Device->GetDebugMarkerSetObjectName())
	{
		FVulkanTextureBase* Base = (FVulkanTextureBase*)TextureRHI->GetTextureBaseRHI();
		FTCHARToUTF8 Converter(Name);
		VulkanRHI::SetDebugMarkerName(SetObjectName, Device->GetInstanceHandle(), Base->Surface.Image, Converter.Get());
	}
#endif
	FName DebugName(Name);
	TextureRHI->SetName(DebugName);
}

void FVulkanDynamicRHI::RHIBindDebugLabelName(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const TCHAR* Name)
{
#if VULKAN_ENABLE_DUMP_LAYER || VULKAN_ENABLE_API_DUMP
	//if (Device->SupportsDebugMarkers())
	{
		//if (FRHITexture2D* Tex2d = UnorderedAccessViewRHI->GetTexture2D())
		//{
		//	FVulkanTexture2D* VulkanTexture = (FVulkanTexture2D*)Tex2d;
		//	VkDebugMarkerObjectTagInfoEXT Info;
		//	ZeroVulkanStruct(Info, VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT);
		//	Info.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT;
		//	Info.object = VulkanTexture->Surface.Image;
		//	vkDebugMarkerSetObjectNameEXT(Device->GetInstanceHandle(), &Info);
		//}
	}
#endif
}


void FVulkanDynamicRHI::RHIVirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanDynamicRHI::RHIVirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

static VkMemoryRequirements FindOrCalculateTexturePlatformSize(FVulkanDevice* Device, VkImageViewType ViewType, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags)
{
	// Adjust number of mips as UTexture can request non-valid # of mips
	NumMips = FMath::Min(FMath::FloorLog2(FMath::Max(SizeX, FMath::Max(SizeY, SizeZ))) + 1, NumMips);

	struct FTexturePlatformSizeKey
	{
		VkImageViewType ViewType;
		uint32 SizeX;
		uint32 SizeY;
		uint32 SizeZ;
		uint32 Format;
		uint32 NumMips;
		uint32 NumSamples;
		uint32 Flags;
	};

	static TMap<uint32, VkMemoryRequirements> TextureSizes;
	static FCriticalSection TextureSizesLock;

	const FTexturePlatformSizeKey Key = { ViewType, SizeX, SizeY, SizeZ, Format, NumMips, NumSamples, Flags};
	const uint32 Hash = FCrc::MemCrc32(&Key, sizeof(FTexturePlatformSizeKey));

	VkMemoryRequirements* Found = nullptr;
	{
		FScopeLock Lock(&TextureSizesLock);
		Found = TextureSizes.Find(Hash);
		if (Found)
		{
			return *Found;
		}
	}

	VkFormat InternalStorageFormat, InternalViewFormat;
	VkImageCreateInfo CreateInfo;
	VkMemoryRequirements MemReq;
	EPixelFormat PixelFormat = (EPixelFormat)Format;

	// Create temporary image to measure the memory requirements
	VkImage TmpImage = FVulkanSurface::CreateImage(*Device, ViewType,
		PixelFormat, SizeX, SizeY, SizeZ, false, 0, NumMips, NumSamples,
		Flags, MemReq, &InternalStorageFormat, &InternalViewFormat, &CreateInfo, false);

	VulkanRHI::vkDestroyImage(Device->GetInstanceHandle(), TmpImage, VULKAN_CPU_ALLOCATOR);

	{
		FScopeLock Lock(&TextureSizesLock);
		TextureSizes.Add(Hash, MemReq);
	}
	
	return MemReq;
}



uint64 FVulkanDynamicRHI::RHICalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign)
{
	const VkMemoryRequirements MemReq = FindOrCalculateTexturePlatformSize(Device, VK_IMAGE_VIEW_TYPE_2D, SizeX, SizeY, 1, Format, NumMips, NumSamples, Flags);
	OutAlign = MemReq.alignment;
	return MemReq.size;
}

uint64 FVulkanDynamicRHI::RHICalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	const VkMemoryRequirements MemReq = FindOrCalculateTexturePlatformSize(Device, VK_IMAGE_VIEW_TYPE_3D, SizeX, SizeY, SizeZ, Format, NumMips, 1, Flags);
	OutAlign = MemReq.alignment;
	return MemReq.size;
}

uint64 FVulkanDynamicRHI::RHICalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	const VkMemoryRequirements MemReq = FindOrCalculateTexturePlatformSize(Device, VK_IMAGE_VIEW_TYPE_CUBE, Size, Size, 1, Format, NumMips, 1, Flags);
	OutAlign = MemReq.alignment;
	return MemReq.size;
}

FTextureReferenceRHIRef FVulkanDynamicRHI::RHICreateTextureReference(FLastRenderTimeContainer* LastRenderTime)
{
	return new FVulkanTextureReference(*Device, LastRenderTime);
}

void FVulkanCommandListContext::RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRef, FTextureRHIParamRef NewTexture)
{
	//#todo-rco: Implementation needs to be verified
	FVulkanTextureReference* VulkanTextureRef = (FVulkanTextureReference*)TextureRef;
	if (VulkanTextureRef)
	{
		VulkanTextureRef->SetReferencedTexture(NewTexture);
	}
}

void FVulkanCommandListContext::RHICopyTexture(FTextureRHIParamRef SourceTexture, FTextureRHIParamRef DestTexture, const FRHICopyTextureInfo& CopyInfo)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);
	check(SourceTexture && DestTexture);

	FVulkanTextureBase* Source = static_cast<FVulkanTextureBase*>(SourceTexture->GetTextureBaseRHI());
	FVulkanTextureBase* Dest = static_cast<FVulkanTextureBase*>(DestTexture->GetTextureBaseRHI());

	FVulkanSurface& SrcSurface = Source->Surface;
	FVulkanSurface& DstSurface = Dest->Surface;

	VkImageLayout SrcLayout = TransitionAndLayoutManager.FindLayoutChecked(SrcSurface.Image);
	bool bIsDepth = DstSurface.IsDepthOrStencilAspect();
	VkImageLayout& DstLayoutRW = TransitionAndLayoutManager.FindOrAddLayoutRW(DstSurface.Image, VK_IMAGE_LAYOUT_UNDEFINED);
	bool bCopyIntoCPUReadable = (DstSurface.UEFlags & TexCreate_CPUReadback) == TexCreate_CPUReadback;

	FVulkanCmdBuffer* InCmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	check(InCmdBuffer->IsOutsideRenderPass());

	VkCommandBuffer CmdBuffer = InCmdBuffer->GetHandle();

	FPendingBarrier Barrier;
	int32 SourceBarrierIndex = Barrier.AddImageBarrier(SrcSurface.Image, SrcSurface.GetFullAspectMask(), 1);;
	int32 DestBarrierIndex = Barrier.AddImageBarrier(DstSurface.Image, DstSurface.GetFullAspectMask(), 1);
	{
		VkImageSubresourceRange& Range = Barrier.GetSubresource(SourceBarrierIndex);
		Range.baseMipLevel = CopyInfo.SourceMipIndex;
		Range.levelCount = CopyInfo.NumMips;
		Range.baseArrayLayer = CopyInfo.SourceSliceIndex;
		Range.layerCount = CopyInfo.NumSlices;
		Barrier.SetTransition(SourceBarrierIndex, VulkanRHI::GetImageLayoutFromVulkanLayout(SrcLayout), EImageLayoutBarrier::TransferSource);
	}
	{
		VkImageSubresourceRange& Range = Barrier.GetSubresource(DestBarrierIndex);
		Range.baseMipLevel = CopyInfo.DestMipIndex;
		Range.levelCount = CopyInfo.NumMips;
		Range.baseArrayLayer = CopyInfo.DestSliceIndex;
		Range.layerCount = CopyInfo.NumSlices;
		Barrier.SetTransition(DestBarrierIndex, EImageLayoutBarrier::Undefined, EImageLayoutBarrier::TransferDest);
	}

	Barrier.Execute(InCmdBuffer);

	VkImageCopy Region;
	FMemory::Memzero(Region);
	ensure(SrcSurface.Width == DstSurface.Width && SrcSurface.Height == DstSurface.Height);
	Region.extent.width = FMath::Max(1u, SrcSurface.Width>> CopyInfo.SourceMipIndex);
	Region.extent.height = FMath::Max(1u, SrcSurface.Height >> CopyInfo.SourceMipIndex);
	Region.extent.depth = 1;
	Region.srcSubresource.aspectMask = SrcSurface.GetFullAspectMask();
	Region.srcSubresource.baseArrayLayer = CopyInfo.SourceSliceIndex;
	Region.srcSubresource.layerCount = CopyInfo.NumSlices;
	Region.srcSubresource.mipLevel = CopyInfo.SourceMipIndex;
	Region.dstSubresource.aspectMask = DstSurface.GetFullAspectMask();
	Region.dstSubresource.baseArrayLayer = CopyInfo.DestSliceIndex;
	Region.dstSubresource.layerCount = CopyInfo.NumSlices;
	Region.dstSubresource.mipLevel = CopyInfo.DestMipIndex;

	for (uint32 Index = 0; Index < CopyInfo.NumMips; ++Index)
	{
		VulkanRHI::vkCmdCopyImage(CmdBuffer,
			SrcSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			DstSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &Region);
		Region.extent.width = FMath::Max(1u, Region.extent.width / 2);
		Region.extent.height = FMath::Max(1u, Region.extent.height / 2);
		++Region.srcSubresource.mipLevel;
		++Region.dstSubresource.mipLevel;
	}

	Barrier.ResetStages();
	Barrier.SetTransition(SourceBarrierIndex, EImageLayoutBarrier::TransferSource, VulkanRHI::GetImageLayoutFromVulkanLayout(SrcLayout));

	if (bCopyIntoCPUReadable)
	{
		Barrier.SetTransition(DestBarrierIndex, EImageLayoutBarrier::TransferDest, EImageLayoutBarrier::PixelGeneralRW);
		DstLayoutRW = VK_IMAGE_LAYOUT_GENERAL;
	}
	else
	{
		Barrier.SetTransition(DestBarrierIndex, EImageLayoutBarrier::TransferDest, bIsDepth ? EImageLayoutBarrier::PixelDepthStencilRead : EImageLayoutBarrier::PixelShaderRead);
		DstLayoutRW = bIsDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	Barrier.Execute(InCmdBuffer);
}
