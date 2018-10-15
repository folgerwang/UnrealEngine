// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanResources.h: Vulkan resource RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanConfiguration.h"
#include "VulkanState.h"
#include "VulkanUtil.h"
#include "BoundShaderStateCache.h"
#include "VulkanShaderResources.h"
#include "VulkanState.h"
#include "VulkanMemory.h"

class FVulkanDevice;
class FVulkanQueue;
class FVulkanCmdBuffer;
class FVulkanBuffer;
class FVulkanBufferCPU;
struct FVulkanTextureBase;
class FVulkanTexture2D;
struct FVulkanBufferView;
class FVulkanResourceMultiBuffer;
class FVulkanLayout;
class FVulkanOcclusionQuery;

namespace VulkanRHI
{
	class FDeviceMemoryAllocation;
	class FOldResourceAllocation;
	struct FPendingBufferLock;
	class FStagingBuffer;
}

enum
{
	NUM_OCCLUSION_QUERIES_PER_POOL = 4096,

	NUM_TIMESTAMP_QUERIES_PER_POOL = 1024,
};

struct FSamplerYcbcrConversionInitializer
{
	VkFormat Format;
	uint64 ExternalFormat;
	VkComponentMapping Components;
	VkSamplerYcbcrModelConversion Model;
	VkSamplerYcbcrRange Range;
	VkChromaLocation XOffset;
	VkChromaLocation YOffset;
};

/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class FVulkanVertexDeclaration : public FRHIVertexDeclaration
{
public:
	FVertexDeclarationElementList Elements;

	FVulkanVertexDeclaration(const FVertexDeclarationElementList& InElements);

	virtual bool GetInitializer(FVertexDeclarationElementList& Out) final override
	{
		Out = Elements;
		return true;
	}

	static void EmptyCache();
};


class FVulkanShader : public IRefCountedObject
{
public:
	FVulkanShader(FVulkanDevice* InDevice, EShaderFrequency InFrequency, VkShaderStageFlagBits InStageFlag)
		: ShaderKey(0)
		, StageFlag(InStageFlag)
		, Frequency(InFrequency)
		, Device(InDevice)
	{
	}

	virtual ~FVulkanShader();

	void PurgeShaderModules();

	void Setup(const TArray<uint8>& InShaderHeaderAndCode, uint64 InShaderKey);

	VkShaderModule GetOrCreateHandle(const FVulkanLayout* Layout, uint32 LayoutHash)
	{
		VkShaderModule* Found = ShaderModules.Find(LayoutHash);
		if (Found)
		{
			return *Found;
		}

		return CreateHandle(Layout, LayoutHash);
	}

#if VULKAN_ENABLE_SHADER_DEBUG_NAMES
	inline const FString& GetDebugName() const
	{
		return CodeHeader.DebugName;
	}
#endif

	FORCEINLINE const FVulkanShaderHeader& GetCodeHeader() const
	{
		return CodeHeader;
	}

	inline uint64 GetShaderKey() const
	{
		return ShaderKey;
	}

protected:
	uint64							ShaderKey;

	/** External bindings for this shader. */
	FVulkanShaderHeader				CodeHeader;
	TMap<uint32, VkShaderModule>	ShaderModules;
	const VkShaderStageFlagBits		StageFlag;
	EShaderFrequency				Frequency;

	TArray<uint32>					Spirv;

	FVulkanDevice*					Device;

	VkShaderModule CreateHandle(const FVulkanLayout* Layout, uint32 LayoutHash);

	friend class FVulkanCommandListContext;
	friend class FVulkanPipelineStateCacheManager;
	friend class FVulkanComputeShaderState;
	friend class FVulkanComputePipeline;
	friend class FVulkanShaderFactory;
};

/** This represents a vertex shader that hasn't been combined with a specific declaration to create a bound shader. */
template<typename BaseResourceType, EShaderFrequency ShaderType, VkShaderStageFlagBits StageFlagBits>
class TVulkanBaseShader : public BaseResourceType, public FVulkanShader
{
private:
	TVulkanBaseShader(FVulkanDevice* InDevice) :
		FVulkanShader(InDevice, ShaderType, StageFlagBits)
	{
	}
	friend class FVulkanShaderFactory;
public:
	enum { StaticFrequency = ShaderType };

	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}
};

typedef TVulkanBaseShader<FRHIVertexShader, SF_Vertex, VK_SHADER_STAGE_VERTEX_BIT>					FVulkanVertexShader;
typedef TVulkanBaseShader<FRHIPixelShader, SF_Pixel, VK_SHADER_STAGE_FRAGMENT_BIT>					FVulkanPixelShader;
typedef TVulkanBaseShader<FRHIHullShader, SF_Hull, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT>		FVulkanHullShader;
typedef TVulkanBaseShader<FRHIDomainShader, SF_Domain, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT>	FVulkanDomainShader;
typedef TVulkanBaseShader<FRHIComputeShader, SF_Compute, VK_SHADER_STAGE_COMPUTE_BIT>				FVulkanComputeShader;
typedef TVulkanBaseShader<FRHIGeometryShader, SF_Geometry, VK_SHADER_STAGE_GEOMETRY_BIT>			FVulkanGeometryShader;

class FVulkanShaderFactory
{
public:
	~FVulkanShaderFactory();
	
	template <typename ShaderType> 
	ShaderType* CreateShader(const TArray<uint8>& Code, FVulkanDevice* Device);
	
	void OnDeleteShader(const FVulkanShader& Shader);

private:
	FRWLock Lock;
	TMap<uint64, FVulkanShader*> ShaderMap[SF_NumFrequencies];
};

class FVulkanBoundShaderState : public FRHIBoundShaderState
{
public:
	FVulkanBoundShaderState(
		FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
		FVertexShaderRHIParamRef InVertexShaderRHI,
		FPixelShaderRHIParamRef InPixelShaderRHI,
		FHullShaderRHIParamRef InHullShaderRHI,
		FDomainShaderRHIParamRef InDomainShaderRHI,
		FGeometryShaderRHIParamRef InGeometryShaderRHI
	);

	virtual ~FVulkanBoundShaderState();

	FORCEINLINE FVulkanVertexShader*   GetVertexShader() const { return (FVulkanVertexShader*)CacheLink.GetVertexShader(); }
	FORCEINLINE FVulkanPixelShader*    GetPixelShader() const { return (FVulkanPixelShader*)CacheLink.GetPixelShader(); }
	FORCEINLINE FVulkanHullShader*     GetHullShader() const { return (FVulkanHullShader*)CacheLink.GetHullShader(); }
	FORCEINLINE FVulkanDomainShader*   GetDomainShader() const { return (FVulkanDomainShader*)CacheLink.GetDomainShader(); }
	FORCEINLINE FVulkanGeometryShader* GetGeometryShader() const { return (FVulkanGeometryShader*)CacheLink.GetGeometryShader(); }

	const FVulkanShader* GetShader(ShaderStage::EStage Stage) const
	{
		switch (Stage)
		{
		case ShaderStage::Vertex:		return GetVertexShader();
		//case ShaderStage::Hull:		return GetHullShader();
		//case ShaderStage::Domain:		return GetDomainShader();
		case ShaderStage::Pixel:		return GetPixelShader();
#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
		case ShaderStage::Geometry:	return GetGeometryShader();
#endif
		default: break;
		}
		checkf(0, TEXT("Invalid Shader Frequency %d"), (int32)Stage);
		return nullptr;
	}

private:
	FCachedBoundShaderStateLink_Threadsafe CacheLink;
};

/** Texture/RT wrapper. */
class FVulkanSurface
{
public:

	// Seperate method for creating image, this can be used to measure image size
	// After VkImage is no longer needed, dont forget to destroy/release it 
	static VkImage CreateImage(
		FVulkanDevice& InDevice,
		VkImageViewType ResourceType,
		EPixelFormat InFormat,
		uint32 SizeX, uint32 SizeY, uint32 SizeZ,
		bool bArray, uint32 ArraySize,
		uint32 NumMips,
		uint32 NumSamples,
		uint32 UEFlags,
		VkMemoryRequirements& OutMemoryRequirements,
		VkFormat* OutStorageFormat = nullptr,
		VkFormat* OutViewFormat = nullptr,
		VkImageCreateInfo* OutInfo = nullptr,
		bool bForceLinearTexture = false);

	FVulkanSurface(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format,
					uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize,
					uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);

	// Constructor for externally owned Image
	FVulkanSurface(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format,
					uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 NumSamples,
					VkImage InImage, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);

	virtual ~FVulkanSurface();

	void Destroy();

#if 0
	/**
	 * Locks one of the texture's mip-maps.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 * @return A pointer to the specified texture data.
	 */
	void* Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride);

	/** Unlocks a previously locked mip-map.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 */
	void Unlock(uint32 MipIndex, uint32 ArrayIndex);
#endif

	/**
	 * Returns how much memory is used by the surface
	 */
	uint32 GetMemorySize() const
	{
		return MemoryRequirements.size;
	}

	/**
	 * Returns one of the texture's mip-maps stride.
	 */
	void GetMipStride(uint32 MipIndex, uint32& Stride);

	/*
	 * Returns the memory offset to the texture's mip-map.
	 */
	void GetMipOffset(uint32 MipIndex, uint32& Offset);

	/**
	* Returns how much memory a single mip uses.
	*/
	void GetMipSize(uint32 MipIndex, uint32& MipBytes);

	inline VkImageViewType GetViewType() const { return ViewType; }

	inline VkImageTiling GetTiling() const { return Tiling; }

	inline uint32 GetNumMips() const { return NumMips; }

	inline uint32 GetNumSamples() const { return NumSamples; }

	inline uint32 GetNumberOfArrayLevels() const
	{
		switch (ViewType)
		{
		case VK_IMAGE_VIEW_TYPE_1D:
		case VK_IMAGE_VIEW_TYPE_2D:
		case VK_IMAGE_VIEW_TYPE_3D:
			return 1;
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
			return NumArrayLevels;
		case VK_IMAGE_VIEW_TYPE_CUBE:
			return 6;
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			return 6 * NumArrayLevels;
		default:
			UE_LOG(LogVulkanRHI, Error, TEXT("Invalid ViewType %d"), (uint32)ViewType);
			return 1;
		}
	}

	// Full includes Depth+Stencil
	inline VkImageAspectFlags GetFullAspectMask() const
	{
		return FullAspectMask;
	}

	// Only Depth or Stencil
	inline VkImageAspectFlags GetPartialAspectMask() const
	{
		return PartialAspectMask;
	}

	inline bool IsDepthOrStencilAspect() const
	{
		return (FullAspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;
	}

	inline bool IsImageOwner() const
	{
		return bIsImageOwner;
	}

	inline VkDeviceMemory GetAllocationHandle() const
	{
		if (ResourceAllocation.IsValid())
		{
			return ResourceAllocation->GetHandle();
		}
		else
		{
			return VK_NULL_HANDLE;
		}
	}

	inline uint64 GetAllocationOffset() const
	{
		if (ResourceAllocation.IsValid())
		{
			return ResourceAllocation->GetOffset();
		}
		else
		{
			return 0;
		}
	}

	FVulkanDevice* Device;

	VkImage Image;
	
	// Removes SRGB if requested, used to upload data
	VkFormat StorageFormat;
	// Format for SRVs, render targets
	VkFormat ViewFormat;
	uint32 Width, Height, Depth;
	// UE format
	EPixelFormat PixelFormat;
	uint32 UEFlags;
	VkMemoryPropertyFlags MemProps;
	VkMemoryRequirements MemoryRequirements;
	uint32 NumArrayLevels;

	static void InternalLockWrite(FVulkanCommandListContext& Context, FVulkanSurface* Surface, const VkImageSubresourceRange& SubresourceRange, const VkBufferImageCopy& Region, VulkanRHI::FStagingBuffer* StagingBuffer);

private:

	// Used to clear render-target objects on creation
	void InitialClear(FVulkanCommandListContext& Context, const FClearValueBinding& ClearValueBinding, bool bTransitionToPresentable);
	friend struct FRHICommandInitialClearTexture;

private:
	VkImageTiling Tiling;
	VkImageViewType	ViewType;

	bool bIsImageOwner;
	TRefCountPtr<VulkanRHI::FOldResourceAllocation> ResourceAllocation;

	uint32 NumMips;
	uint32 NumSamples;

	VkImageAspectFlags FullAspectMask;
	VkImageAspectFlags PartialAspectMask;

	friend struct FVulkanTextureBase;
};


struct FVulkanTextureView
{
	FVulkanTextureView()
		: View(VK_NULL_HANDLE)
		, Image(VK_NULL_HANDLE)
	{
	}

	static VkImageView StaticCreate(FVulkanDevice& Device, VkImage InImage, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat UEFormat, VkFormat Format, uint32 FirstMip, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices, bool bUseIdentitySwizzle = false, const FSamplerYcbcrConversionInitializer* ConversionInitializer = nullptr);

	void Create(FVulkanDevice& Device, VkImage InImage, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat UEFormat, VkFormat Format, uint32 FirstMip, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices);
	void Create(FVulkanDevice& Device, VkImage InImage, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat UEFormat, VkFormat Format, uint32 FirstMip, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices, FSamplerYcbcrConversionInitializer& ConversionInitializer);
	void Destroy(FVulkanDevice& Device);

	VkImageView View;
	VkImage Image;
};

/** The base class of resources that may be bound as shader resources. */
class FVulkanBaseShaderResource : public IRefCountedObject
{
};

struct FVulkanTextureBase : public FVulkanBaseShaderResource
{
	inline static FVulkanTextureBase* Cast(FRHITexture* Texture)
	{
		check(Texture);
		FVulkanTextureBase* OutTexture = (FVulkanTextureBase*)Texture->GetTextureBaseRHI();
		check(OutTexture);
		return OutTexture;
	}

	FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);
	FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 NumSamples, uint32 NumSamplesTileMem, VkImage InImage, VkDeviceMemory InMem, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo = FRHIResourceCreateInfo());
	FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 NumSamples, VkImage InImage, VkDeviceMemory InMem, FSamplerYcbcrConversionInitializer& ConversionInitializer, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo = FRHIResourceCreateInfo());

	virtual ~FVulkanTextureBase();

	VkImageView CreateRenderTargetView(uint32 MipIndex, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices);
	void AliasTextureResources(const FVulkanTextureBase* SrcTexture);

	FVulkanSurface Surface;

	// View with all mips/layers
	FVulkanTextureView DefaultView;
	// View with all mips/layers, but if it's a Depth/Stencil, only the Depth view
	FVulkanTextureView* PartialView;

#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	// Surface and view for MSAA render target, valid only when created with NumSamples > 1
	FVulkanSurface* MSAASurface;
	FVulkanTextureView MSAAView;
#endif

	bool bIsAliased;

private:
	void DestroyViews();
};

class FVulkanBackBuffer;
class FVulkanTexture2D : public FRHITexture2D, public FVulkanTextureBase
{
public:
	FVulkanTexture2D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);
	FVulkanTexture2D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 NumSamplesTileMem, VkImage Image, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);
	FVulkanTexture2D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, VkImage Image, struct FSamplerYcbcrConversionInitializer& ConversionInitializer, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);

	virtual ~FVulkanTexture2D();

	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}

	virtual FVulkanBackBuffer* GetBackBuffer()
	{
		return nullptr;
	}

	virtual void* GetTextureBaseRHI() override final
	{
		FVulkanTextureBase* Base = static_cast<FVulkanTextureBase*>(this);
		return Base;
	}

	virtual void* GetNativeResource() const
	{
		return (void*)Surface.Image;
	}
};

class FVulkanBackBuffer : public FVulkanTexture2D
{
public:
	FVulkanBackBuffer(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 UEFlags);
	FVulkanBackBuffer(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, VkImage Image, uint32 UEFlags);
	virtual ~FVulkanBackBuffer();

	virtual FVulkanBackBuffer* GetBackBuffer() override final
	{
		return this;
	}
};

class FVulkanTexture2DArray : public FRHITexture2DArray, public FVulkanTextureBase
{
public:
	// Constructor, just calls base and Surface constructor
	FVulkanTexture2DArray(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue);
	FVulkanTexture2DArray(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, VkImage Image, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue);
		
	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}

	virtual void* GetTextureBaseRHI() override final
	{
		return (FVulkanTextureBase*)this;
	}

	virtual void* GetNativeResource() const
	{
		return (void*)Surface.Image;
	}
};

class FVulkanTexture3D : public FRHITexture3D, public FVulkanTextureBase
{
public:
	// Constructor, just calls base and Surface constructor
	FVulkanTexture3D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue);
	FVulkanTexture3D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, VkImage Image, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue);
	virtual ~FVulkanTexture3D();

	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}

	virtual void* GetTextureBaseRHI() override final
	{
		return (FVulkanTextureBase*)this;
	}

	virtual void* GetNativeResource() const
	{
		return (void*)Surface.Image;
	}
};

class FVulkanTextureCube : public FRHITextureCube, public FVulkanTextureBase
{
public:
	FVulkanTextureCube(FVulkanDevice& Device, EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue);
	FVulkanTextureCube(FVulkanDevice& Device, EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, VkImage Image, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue);
	virtual ~FVulkanTextureCube();

	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}

	virtual void* GetTextureBaseRHI() override final
	{
		return (FVulkanTextureBase*)this;
	}

	virtual void* GetNativeResource() const
	{
		return (void*)Surface.Image;
	}
};

class FVulkanTextureReference : public FRHITextureReference, public FVulkanTextureBase
{
public:
	explicit FVulkanTextureReference(FVulkanDevice& Device, FLastRenderTimeContainer* InLastRenderTime)
	:	FRHITextureReference(InLastRenderTime)
	,	FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_MAX_ENUM, PF_Unknown, 0, 0, 0, 1, 1, 1, VK_NULL_HANDLE, VK_NULL_HANDLE, 0)
	{}

	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}

	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}

	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}

	virtual void* GetTextureBaseRHI() override final
	{
		return GetReferencedTexture()->GetTextureBaseRHI();
	}

	virtual void* GetNativeResource() const
	{
		return (void*)Surface.Image;
	}

	void SetReferencedTexture(FRHITexture* InTexture);
};

/** Given a pointer to a RHI texture that was created by the Vulkan RHI, returns a pointer to the FVulkanTextureBase it encapsulates. */
inline FVulkanTextureBase* GetVulkanTextureFromRHITexture(FRHITexture* Texture)
{
	if (!Texture)
	{
		return NULL;
	}
	else if (Texture->GetTexture2D())
	{
		return static_cast<FVulkanTexture2D*>(Texture);
	}
	else if (Texture->GetTextureReference())
	{
		return static_cast<FVulkanTextureReference*>(Texture);
	}
	else if (Texture->GetTexture2DArray())
	{
		return static_cast<FVulkanTexture2DArray*>(Texture);
	}
	else if (Texture->GetTexture3D())
	{
		return static_cast<FVulkanTexture3D*>(Texture);
	}
	else if (Texture->GetTextureCube())
	{
		return static_cast<FVulkanTextureCube*>(Texture);
	}
	else
	{
		UE_LOG(LogVulkanRHI, Fatal, TEXT("Unknown Vulkan RHI texture type"));
		return NULL;
	}
}

class FVulkanQueryPool : public VulkanRHI::FDeviceChild 
{
public:
	FVulkanQueryPool(FVulkanDevice* InDevice, uint32 InMaxQueries, VkQueryType InQueryType);
	virtual ~FVulkanQueryPool();

	inline uint32 GetMaxQueries() const
	{
		return MaxQueries;
	}

	inline VkQueryPool GetHandle() const
	{
		return QueryPool;
	}

	inline uint64 GetResultValue(uint32 Index) const
	{
		return QueryOutput[Index];
	}

protected:
	VkQueryPool QueryPool;
	uint32 NumUsedQueries = 0;
	const uint32 MaxQueries;
	const VkQueryType QueryType;
	TArray<uint64> QueryOutput;
};

class FVulkanOcclusionQueryPool : public FVulkanQueryPool
{
public:
	FVulkanOcclusionQueryPool(FVulkanDevice* InDevice, uint32 InMaxQueries)
		: FVulkanQueryPool(InDevice, InMaxQueries, VK_QUERY_TYPE_OCCLUSION)
	{
		AcquiredIndices.AddZeroed(Align(InMaxQueries, 64) / 64);
		AllocatedQueries.AddZeroed(InMaxQueries);
	}

	inline uint32 AcquireIndex(FVulkanOcclusionQuery* Query)
	{
		check(NumUsedQueries < MaxQueries);
		const uint32 Index = NumUsedQueries;
		const uint32 Word = Index / 64;
		const uint32 Bit = Index % 64;
		const uint64 Mask = (uint64)1 << (uint64)Bit;
		const uint64& WordValue = AcquiredIndices[Word];
		AcquiredIndices[Word] = WordValue | Mask;
		++NumUsedQueries;
		ensure(AllocatedQueries[Index] == nullptr);
		AllocatedQueries[Index] = Query;
		return Index;
	}

	inline void ReleaseIndex(uint32 Index)
	{
		check(Index < NumUsedQueries);
		const uint32 Word = Index / 64;
		const uint32 Bit = Index % 64;
		const uint64 Mask = (uint64)1 << (uint64)Bit;
		const uint64& WordValue = AcquiredIndices[Word];
		ensure((WordValue & Mask) == Mask);
		AcquiredIndices[Word] = WordValue & (~Mask);
		AllocatedQueries[Index] = nullptr;
	}

	inline void EndBatch(FVulkanCmdBuffer* InCmdBuffer)
	{
		ensure(State == EState::RHIT_PostBeginBatch);
		State = EState::RHIT_PostEndBatch;
		SetFence(InCmdBuffer);
	}

	bool CanBeReused();

	inline bool TryGetResults(bool bWait)
	{
		if (State == RT_PostGetResults)
		{
			return true;
		}

		if (State == RHIT_PostEndBatch)
		{
			return InternalTryGetResults(bWait);
		}

		return false;
	}

	void Reset(FVulkanCmdBuffer* InCmdBuffer, uint32 InFrameNumber);

	bool IsStalePool() const;

	void FlushAllocatedQueries();

	enum EState
	{
		Undefined,
		RHIT_PostBeginBatch,
		RHIT_PostEndBatch,
		RT_PostGetResults,
	};
	EState State = Undefined;

protected:
	TArray<FVulkanOcclusionQuery*> AllocatedQueries;
	TArray<uint64> AcquiredIndices;
	bool InternalTryGetResults(bool bWait);
	void SetFence(FVulkanCmdBuffer* InCmdBuffer);

	FVulkanCmdBuffer* CmdBuffer = nullptr;
	uint64 FenceCounter = UINT64_MAX;
	uint32 FrameNumber = UINT32_MAX;
};

/*
class FVulkanTimestampQueryPool : public FVulkanQueryPool
{
public:
	FVulkanTimestampQueryPool(FVulkanDevice* InDevice, uint32 InMaxQueries)
		: FVulkanQueryPool(InDevice, InMaxQueries, VK_QUERY_TYPE_TIMESTAMP)
	{
		//const uint32 ElementSize = sizeof(decltype(HasResultsMask)::ElementType);
		//HasResultsMask.AddZeroed((InMaxQueries + (ElementSize - 1)) / ElementSize);
	}
		
	bool GetResults(uint32 QueryIndex, bool bWait, uint64& OutResults);

protected:
	//TArray<uint64> HasResultsMask;
	//uint32 LastPresentAllocation = 0;

	//bool GetResults(uint32 QueryIndex, uint32 Word, uint32 Bit, bool bWait, uint64& OutResults);

	friend class FVulkanDevice;
	friend class FVulkanGPUTiming;
};
*/
class FVulkanRenderQuery : public FRHIRenderQuery
{
public:
	FVulkanRenderQuery(ERenderQueryType InType)
		: QueryType(InType)
	{
	}

	virtual ~FVulkanRenderQuery() {}

	const ERenderQueryType QueryType;
	uint64 Result = 0;

	uint32 IndexInPool = UINT32_MAX;
};

class FVulkanOcclusionQuery : public FVulkanRenderQuery
{
public:
	FVulkanOcclusionQuery();
	virtual ~FVulkanOcclusionQuery();

	enum class EState
	{
		Undefined,
		RHI_PostBegin,
		RHI_PostEnd,
		RT_GotResults,
		FlushedFromPoolHadResults,
	};

	FVulkanOcclusionQueryPool* Pool = nullptr;
	uint64 Result = 0;

	void ReleaseFromPool();

	EState State = EState::Undefined;
};
/*
class FVulkanRenderQuery : public FRHIRenderQuery
{
public:
	FVulkanRenderQuery(FVulkanDevice* Device, ERenderQueryType InQueryType);
	virtual ~FVulkanRenderQuery();

	inline bool HasQueryBeenEmitted() const
	{
		return State == EState::InEnd;
	}

	uint32 LastPoolReset = 0;

private:
	int32 QueryIndex = INT32_MAX;

	const ERenderQueryType QueryType;

	FVulkanCmdBuffer* BeginCmdBuffer = nullptr;

	friend class FVulkanDynamicRHI;
	friend class FVulkanCommandListContext;
	friend class FVulkanGPUTiming;

	FVulkanQueryPool* Pool = nullptr;
	enum class EState
	{
		Reset,
		InBegin,
		InEnd,
		HasResults,
	};
	EState State = EState::Reset;
	uint64 Result = 0;

	void Reset(FVulkanQueryPool* InPool, int32 InQueryIndex)
	{
		QueryIndex = InQueryIndex;
		Pool = InPool;
		State = EState::Reset;
	}
	void Begin(FVulkanCmdBuffer* InCmdBuffer);
	void End(FVulkanCmdBuffer* InCmdBuffer);

	bool GetResult(FVulkanDevice* Device, uint64& OutResult, bool bWait);
};
*/
/*
class FVulkanRenderQuery : public FRHIRenderQuery
{
public:
	FVulkanRenderQuery(FVulkanDevice* Device, ERenderQueryType InQueryType);
	virtual ~FVulkanRenderQuery();

	inline bool HasQueryBeenEmitted() const
	{
		return State == EState::InEnd;
	}

	uint32 LastPoolReset = 0;

private:
};
*/

struct FVulkanBufferView : public FRHIResource, public VulkanRHI::FDeviceChild
{
	FVulkanBufferView(FVulkanDevice* InDevice)
		: VulkanRHI::FDeviceChild(InDevice)
		, View(VK_NULL_HANDLE)
		, Flags(0)
		, Offset(0)
		, Size(0)
	{
	}

	virtual ~FVulkanBufferView()
	{
		Destroy();
	}

	void Create(FVulkanBuffer& Buffer, EPixelFormat Format, uint32 InOffset, uint32 InSize);
	void Create(FVulkanResourceMultiBuffer* Buffer, EPixelFormat Format, uint32 InOffset, uint32 InSize);
	void Create(VkFormat Format, FVulkanResourceMultiBuffer* Buffer, uint32 InOffset, uint32 InSize);
	void Destroy();

	VkBufferView View;
	VkFlags Flags;
	uint32 Offset;
	uint32 Size;
};

class FVulkanBuffer : public FRHIResource
{
public:
	FVulkanBuffer(FVulkanDevice& Device, uint32 InSize, VkFlags InUsage, VkMemoryPropertyFlags InMemPropertyFlags, bool bAllowMultiLock, const char* File, int32 Line);
	virtual ~FVulkanBuffer();

	inline VkBuffer GetBufferHandle() const { return Buf; }

	inline uint32 GetSize() const { return Size; }

	void* Lock(uint32 InSize, uint32 InOffset = 0);

	void Unlock();

	inline VkFlags GetFlags() const { return Usage; }

private:
	FVulkanDevice& Device;
	VkBuffer Buf;
	VulkanRHI::FDeviceMemoryAllocation* Allocation;
	uint32 Size;
	VkFlags Usage;

	void* BufferPtr;	
	VkMappedMemoryRange MappedRange;

	bool bAllowMultiLock;
	int32 LockStack;
};

struct FVulkanRingBuffer : public VulkanRHI::FDeviceChild
{
public:
	FVulkanRingBuffer(FVulkanDevice* InDevice, uint64 TotalSize, VkFlags Usage, VkMemoryPropertyFlags MemPropertyFlags);
	~FVulkanRingBuffer();

	// Allocate some space in the ring buffer
	inline uint64 AllocateMemory(uint64 Size, uint32 Alignment, FVulkanCmdBuffer* InCmdBuffer)
	{
		Alignment = FMath::Max(Alignment, MinAlignment);
		uint64 AllocationOffset = Align<uint64>(BufferOffset, Alignment);
		if (AllocationOffset + Size <= BufferSize)
		{
			BufferOffset = AllocationOffset + Size;
			return AllocationOffset;
		}

		return WrapAroundAllocateMemory(Size, Alignment, InCmdBuffer);
	}

	inline uint32 GetBufferOffset() const
	{
		return BufferSuballocation->GetOffset();
	}

	inline VkBuffer GetHandle() const
	{
		return BufferSuballocation->GetHandle();
	}

	inline void* GetMappedPointer()
	{
		return BufferSuballocation->GetMappedPointer();
	}

protected:
	uint64 BufferSize;
	uint64 BufferOffset;
	uint32 MinAlignment;
	VulkanRHI::FBufferSuballocation* BufferSuballocation;

	// Fence for wrapping around
	FVulkanCmdBuffer* FenceCmdBuffer = nullptr;
	uint64 FenceCounter = 0;

	uint64 WrapAroundAllocateMemory(uint64 Size, uint32 Alignment, FVulkanCmdBuffer* InCmdBuffer);
};

struct FVulkanUniformBufferUploader : public VulkanRHI::FDeviceChild
{
public:
	FVulkanUniformBufferUploader(FVulkanDevice* InDevice);
	~FVulkanUniformBufferUploader();

	uint8* GetCPUMappedPointer()
	{
		return (uint8*)CPUBuffer->GetMappedPointer();
	}

	uint64 AllocateMemory(uint64 Size, uint32 Alignment, FVulkanCmdBuffer* InCmdBuffer)
	{
		return CPUBuffer->AllocateMemory(Size, Alignment, InCmdBuffer);
	}

	VkBuffer GetCPUBufferHandle() const
	{
		return CPUBuffer->GetHandle();
	}

	inline uint32 GetCPUBufferOffset() const
	{
		return CPUBuffer->GetBufferOffset();
	}

protected:
	FVulkanRingBuffer* CPUBuffer;
	friend class FVulkanCommandListContext;
};

class FVulkanResourceMultiBuffer : public VulkanRHI::FDeviceChild
{
public:
	FVulkanResourceMultiBuffer(FVulkanDevice* InDevice, VkBufferUsageFlags InBufferUsageFlags, uint32 InSize, uint32 InUEUsage, FRHIResourceCreateInfo& CreateInfo, class FRHICommandListImmediate* InRHICmdList = nullptr);
	virtual ~FVulkanResourceMultiBuffer();

	inline VkBuffer GetHandle() const
	{
		return Current.Handle;
	}

	inline bool IsDynamic() const
	{
		return NumBuffers > 1;
	}

	inline int32 GetDynamicIndex() const
	{
		return DynamicBufferIndex;
	}

	inline bool IsVolatile() const
	{
		return NumBuffers == 0;
	}

	inline uint32 GetVolatileLockCounter() const
	{
		check(IsVolatile());
		return VolatileLockInfo.LockCounter;
	}

	inline int32 GetNumBuffers() const
	{
		return NumBuffers;
	}

	// Offset used for Binding a VkBuffer
	inline uint32 GetOffset() const
	{
		return Current.Offset;
	}

	inline VkBufferUsageFlags GetBufferUsageFlags() const
	{
		return BufferUsageFlags;
	}

	void* Lock(bool bFromRenderingThread, EResourceLockMode LockMode, uint32 Size, uint32 Offset);
	void Unlock(bool bFromRenderingThread);

protected:
	uint32 UEUsage;
	VkBufferUsageFlags BufferUsageFlags;
	uint32 NumBuffers;
	uint32 DynamicBufferIndex;

	enum
	{
		NUM_BUFFERS = 3,
	};

	TRefCountPtr<VulkanRHI::FBufferSuballocation> Buffers[NUM_BUFFERS];
	struct
	{
		VulkanRHI::FBufferSuballocation* SubAlloc = nullptr;
		VkBuffer Handle = VK_NULL_HANDLE;
		uint64 Offset = 0;
	} Current;
	VulkanRHI::FTempFrameAllocationBuffer::FTempAllocInfo VolatileLockInfo;

	static void InternalUnlock(FVulkanCommandListContext& Context, VulkanRHI::FPendingBufferLock& PendingLock, FVulkanResourceMultiBuffer* MultiBuffer, int32 InDynamicBufferIndex);

	friend class FVulkanCommandListContext;
	friend struct FRHICommandMultiBufferUnlock;
};

class FVulkanIndexBuffer : public FRHIIndexBuffer, public FVulkanResourceMultiBuffer
{
public:
	FVulkanIndexBuffer(FVulkanDevice* InDevice, uint32 InStride, uint32 InSize, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, class FRHICommandListImmediate* InRHICmdList);

	inline VkIndexType GetIndexType() const
	{
		return IndexType;
	}

private:
	VkIndexType IndexType;
};

class FVulkanVertexBuffer : public FRHIVertexBuffer, public FVulkanResourceMultiBuffer
{
public:
	FVulkanVertexBuffer(FVulkanDevice* InDevice, uint32 InSize, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, class FRHICommandListImmediate* InRHICmdList);
};

class FVulkanUniformBuffer : public FRHIUniformBuffer
{
public:
	FVulkanUniformBuffer(const FRHIUniformBufferLayout& InLayout, const void* Contents, EUniformBufferUsage Usage, bool bCopyIntoConstantData);

	TArray<uint8> ConstantData;

	const TArray<TRefCountPtr<FRHIResource>>& GetResourceTable() const { return ResourceTable; }

private:
	TArray<TRefCountPtr<FRHIResource>> ResourceTable;
};

class FVulkanRealUniformBuffer : public FVulkanUniformBuffer, public FVulkanResourceMultiBuffer
{
public:
	FVulkanRealUniformBuffer(FVulkanDevice& Device, const FRHIUniformBufferLayout& InLayout, const void* Contents, EUniformBufferUsage Usage);

private:
	TArray<TRefCountPtr<FRHIResource>> ResourceTable;
};

class FVulkanStructuredBuffer : public FRHIStructuredBuffer, public FVulkanResourceMultiBuffer
{
public:
	FVulkanStructuredBuffer(FVulkanDevice* InDevice, uint32 Stride, uint32 Size, FRHIResourceCreateInfo& CreateInfo, uint32 InUsage);

	~FVulkanStructuredBuffer();

};



class FVulkanUnorderedAccessView : public FRHIUnorderedAccessView, public VulkanRHI::FDeviceChild
{
public:
	// the potential resources to refer to with the UAV object
	TRefCountPtr<FVulkanStructuredBuffer> SourceStructuredBuffer;

	FVulkanUnorderedAccessView(FVulkanDevice* Device)
		: VulkanRHI::FDeviceChild(Device)
		, MipLevel(0)
		, BufferViewFormat(PF_Unknown)
		, VolatileLockCounter(MAX_uint32)
	{
	}

	~FVulkanUnorderedAccessView();

	void UpdateView();

	// The texture that this UAV come from
	TRefCountPtr<FRHITexture> SourceTexture;
	FVulkanTextureView TextureView;
	uint32 MipLevel;

	// The vertex buffer this UAV comes from (can be null)
	TRefCountPtr<FVulkanVertexBuffer> SourceVertexBuffer;
	TRefCountPtr<FVulkanIndexBuffer> SourceIndexBuffer;
	TRefCountPtr<FVulkanBufferView> BufferView;
	EPixelFormat BufferViewFormat;

protected:
	// Used to check on volatile buffers if a new BufferView is required
	uint32 VolatileLockCounter;
};


class FVulkanShaderResourceView : public FRHIShaderResourceView, public VulkanRHI::FDeviceChild
{
public:
	FVulkanShaderResourceView(FVulkanDevice* Device, FRHIResource* InRHIBuffer, FVulkanResourceMultiBuffer* InSourceBuffer, uint32 InSize, EPixelFormat InFormat);

	FVulkanShaderResourceView(FVulkanDevice* Device, FRHITexture* InSourceTexture, uint32 InMipLevel, int32 InNumMips, EPixelFormat InFormat)
		: VulkanRHI::FDeviceChild(Device)
		, BufferViewFormat(InFormat)
		, SourceTexture(InSourceTexture)
		, SourceStructuredBuffer(nullptr)
		, MipLevel(InMipLevel)
		, NumMips(InNumMips)
		, Size(0)
		, SourceBuffer(nullptr)
		, VolatileLockCounter(MAX_uint32)
	{
	}

	FVulkanShaderResourceView(FVulkanDevice* Device, FVulkanStructuredBuffer* InStructuredBuffer)
		: VulkanRHI::FDeviceChild(Device)
		, BufferViewFormat(PF_Unknown)
		, SourceTexture(nullptr)
		, SourceStructuredBuffer(InStructuredBuffer)
		, MipLevel(0)
		, NumMips(0)
		, Size(InStructuredBuffer->GetSize())
		, SourceBuffer(nullptr)
		, VolatileLockCounter(MAX_uint32)
	{
	}


	void UpdateView();

	inline FVulkanBufferView* GetBufferView()
	{
		return BufferViews[BufferIndex];
	}

	EPixelFormat BufferViewFormat;

	// The texture that this SRV come from
	TRefCountPtr<FRHITexture> SourceTexture;
	FVulkanTextureView TextureView;
	FVulkanStructuredBuffer* SourceStructuredBuffer;
	uint32 MipLevel;
	uint32 NumMips;

	~FVulkanShaderResourceView();

	TArray<TRefCountPtr<FVulkanBufferView>> BufferViews;
	uint32 BufferIndex = 0;
	uint32 Size;
	// The buffer this SRV comes from (can be null)
	FVulkanResourceMultiBuffer* SourceBuffer;
	// To keep a reference
	TRefCountPtr<FRHIResource> SourceRHIBuffer;

protected:
	// Used to check on volatile buffers if a new BufferView is required
	uint32 VolatileLockCounter;
};

class FVulkanComputeFence : public FRHIComputeFence, public VulkanRHI::FGPUEvent
{
	bool bWriteEvent = false;

public:
	FVulkanComputeFence(FVulkanDevice* InDevice, FName InName);
	virtual ~FVulkanComputeFence();

	void WriteCmd(VkCommandBuffer CmdBuffer, bool bInWriteEvent);
	void WriteWaitEvent(VkCommandBuffer CmdBuffer);
};


class FVulkanVertexInputStateInfo
{
public:
	FVulkanVertexInputStateInfo();

	void Generate(FVulkanVertexDeclaration* VertexDeclaration, uint32 VertexHeaderInOutAttributeMask);

	inline uint32 GetHash() const
	{
		check(Info.sType == VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
		return Hash;
	}

	inline const VkPipelineVertexInputStateCreateInfo& GetInfo() const
	{
		return Info;
	}

protected:
	VkPipelineVertexInputStateCreateInfo Info;
	uint32 Hash;

	uint32 BindingsNum;
	uint32 BindingsMask;

	//#todo-rco: Remove these TMaps
	TMap<uint32, uint32> BindingToStream;
	TMap<uint32, uint32> StreamToBinding;
	VkVertexInputBindingDescription Bindings[MaxVertexElementCount];

	uint32 AttributesNum;
	VkVertexInputAttributeDescription Attributes[MaxVertexElementCount];

	friend class FVulkanPendingGfxState;
	friend class FVulkanPipelineStateCacheManager;
};

// This class holds the staging area for packed global uniform buffers for a given shader
class FPackedUniformBuffers
{
public:
	// One buffer is a chunk of bytes
	typedef TArray<uint8> FPackedBuffer;

	void Init(const FVulkanShaderHeader& InCodeHeader, uint64& OutPackedUniformBufferStagingMask)
	{
		PackedUniformBuffers.AddDefaulted(InCodeHeader.PackedUBs.Num());
		for (int32 Index = 0; Index < InCodeHeader.PackedUBs.Num(); ++Index)
		{
			PackedUniformBuffers[Index].AddUninitialized(InCodeHeader.PackedUBs[Index].SizeInBytes);
		}

		OutPackedUniformBufferStagingMask = ((uint64)1 << (uint64)InCodeHeader.PackedUBs.Num()) - 1;
		EmulatedUBsCopyInfo = InCodeHeader.EmulatedUBsCopyInfo;
		EmulatedUBsCopyRanges = InCodeHeader.EmulatedUBCopyRanges;
	}

	inline void SetPackedGlobalParameter(uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* RESTRICT NewValue, uint64& InOutPackedUniformBufferStagingDirty)
	{
		FPackedBuffer& StagingBuffer = PackedUniformBuffers[BufferIndex];
		check(ByteOffset + NumBytes <= (uint32)StagingBuffer.Num());
		check((NumBytes & 3) == 0 && (ByteOffset & 3) == 0);
		uint32* RESTRICT RawDst = (uint32*)(StagingBuffer.GetData() + ByteOffset);
		uint32* RESTRICT RawSrc = (uint32*)NewValue;
		uint32* RESTRICT RawSrcEnd = RawSrc + (NumBytes >> 2);

		bool bChanged = false;
		do
		{
			bChanged |= CopyAndReturnNotEqual(*RawDst++, *RawSrc++);
		}
		while (RawSrc != RawSrcEnd);

		InOutPackedUniformBufferStagingDirty = InOutPackedUniformBufferStagingDirty | ((uint64)(bChanged ? 1 : 0) << (uint64)BufferIndex);
	}

	// Copies a 'real' constant buffer into the packed globals uniform buffer (only the used ranges)
	inline void SetEmulatedUniformBufferIntoPacked(uint32 BindPoint, const TArray<uint8>& ConstantData, uint64& NEWPackedUniformBufferStagingDirty)
	{
		// Emulated UBs. Assumes UniformBuffersCopyInfo table is sorted by CopyInfo.SourceUBIndex
		if (BindPoint < (uint32)EmulatedUBsCopyRanges.Num())
		{
			uint32 Range = EmulatedUBsCopyRanges[BindPoint];
			uint16 Start = (Range >> 16) & 0xffff;
			uint16 Count = Range & 0xffff;
			const uint8* RESTRICT SourceData = ConstantData.GetData();
			for (int32 Index = Start; Index < Start + Count; ++Index)
			{
				const CrossCompiler::FUniformBufferCopyInfo& CopyInfo = EmulatedUBsCopyInfo[Index];
				check(CopyInfo.SourceUBIndex == BindPoint);
				FPackedBuffer& StagingBuffer = PackedUniformBuffers[(int32)CopyInfo.DestUBIndex];
				//check(ByteOffset + NumBytes <= (uint32)StagingBuffer.Num());
				bool bChanged = false;
				uint32* RESTRICT RawDst = (uint32*)(StagingBuffer.GetData() + CopyInfo.DestOffsetInFloats * 4);
				uint32* RESTRICT RawSrc = (uint32*)(SourceData + CopyInfo.SourceOffsetInFloats * 4);
				uint32* RESTRICT RawSrcEnd = RawSrc + CopyInfo.SizeInFloats;
				do
				{
					bChanged |= CopyAndReturnNotEqual(*RawDst++, *RawSrc++);
				}
				while (RawSrc != RawSrcEnd);
				NEWPackedUniformBufferStagingDirty = NEWPackedUniformBufferStagingDirty | ((uint64)(bChanged ? 1 : 0) << (uint64)CopyInfo.DestUBIndex);
			}
		}
	}

	inline const FPackedBuffer& GetBuffer(int32 Index) const
	{
		return PackedUniformBuffers[Index];
	}

protected:
	TArray<FPackedBuffer>									PackedUniformBuffers;

	// Copies to Shader Code Header (shaders may be deleted when we use this object again)
	TArray<CrossCompiler::FUniformBufferCopyInfo>			EmulatedUBsCopyInfo;
	TArray<uint32>											EmulatedUBsCopyRanges;
};

class FVulkanStagingBuffer : public FRHIStagingBuffer
{
public:
	FVulkanStagingBuffer(FVertexBufferRHIRef InBuffer)
		: FRHIStagingBuffer(InBuffer)
	{
	}

	virtual ~FVulkanStagingBuffer();

	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	uint32 QueuedOffset = 0;
	uint32 QueuedNumBytes = 0;
};

class FVulkanGPUFence : public FRHIGPUFence
{
public:
	FVulkanGPUFence(FName InName)
		: FRHIGPUFence(InName)
	{
	}

	virtual bool Poll() const final override;

protected:
	FVulkanCmdBuffer*	CmdBuffer = nullptr;
	uint64				FenceSignaledCounter = 0;

	friend class FVulkanCommandListContext;
};

template<class T>
struct TVulkanResourceTraits
{
};
template<>
struct TVulkanResourceTraits<FRHIVertexDeclaration>
{
	typedef FVulkanVertexDeclaration TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIVertexShader>
{
	typedef FVulkanVertexShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIGeometryShader>
{
	typedef FVulkanGeometryShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIHullShader>
{
	typedef FVulkanHullShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIDomainShader>
{
	typedef FVulkanDomainShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIPixelShader>
{
	typedef FVulkanPixelShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIComputeShader>
{
	typedef FVulkanComputeShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHITexture3D>
{
	typedef FVulkanTexture3D TConcreteType;
};
//template<>
//struct TVulkanResourceTraits<FRHITexture>
//{
//	typedef FVulkanTexture TConcreteType;
//};
template<>
struct TVulkanResourceTraits<FRHITexture2D>
{
	typedef FVulkanTexture2D TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHITexture2DArray>
{
	typedef FVulkanTexture2DArray TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHITextureCube>
{
	typedef FVulkanTextureCube TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIRenderQuery>
{
	typedef FVulkanRenderQuery TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIUniformBuffer>
{
	typedef FVulkanUniformBuffer TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIIndexBuffer>
{
	typedef FVulkanIndexBuffer TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIStructuredBuffer>
{
	typedef FVulkanStructuredBuffer TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIVertexBuffer>
{
	typedef FVulkanVertexBuffer TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIShaderResourceView>
{
	typedef FVulkanShaderResourceView TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIUnorderedAccessView>
{
	typedef FVulkanUnorderedAccessView TConcreteType;
};

template<>
struct TVulkanResourceTraits<FRHISamplerState>
{
	typedef FVulkanSamplerState TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIRasterizerState>
{
	typedef FVulkanRasterizerState TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIDepthStencilState>
{
	typedef FVulkanDepthStencilState TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIBlendState>
{
	typedef FVulkanBlendState TConcreteType;
};

template<>
struct TVulkanResourceTraits<FRHIComputeFence>
{
	typedef FVulkanComputeFence TConcreteType;
};

template<>
struct TVulkanResourceTraits<FRHIBoundShaderState>
{
	typedef FVulkanBoundShaderState TConcreteType;
};

template<>
struct TVulkanResourceTraits<FRHIStagingBuffer>
{
	typedef FVulkanStagingBuffer TConcreteType;
};

template<>
struct TVulkanResourceTraits<FRHIGPUFence>
{
	typedef FVulkanGPUFence TConcreteType;
};

template<typename TRHIType>
static FORCEINLINE typename TVulkanResourceTraits<TRHIType>::TConcreteType* ResourceCast(TRHIType* Resource)
{
	return static_cast<typename TVulkanResourceTraits<TRHIType>::TConcreteType*>(Resource);
}

template<typename TRHIType>
static FORCEINLINE typename TVulkanResourceTraits<TRHIType>::TConcreteType* ResourceCast(const TRHIType* Resource)
{
	return static_cast<const typename TVulkanResourceTraits<TRHIType>::TConcreteType*>(Resource);
}
