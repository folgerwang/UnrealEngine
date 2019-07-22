// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHIDefinitions.h"
#include "Templates/RefCounting.h"
#include "Stats/Stats.h"

class FRHICommandListImmediate;
class FRHIShaderResourceView;
class FRHITexture;

union FVirtualTextureProducerHandle
{
	FVirtualTextureProducerHandle() : PackedValue(0u) {}
	explicit FVirtualTextureProducerHandle(uint32 InPackedValue) : PackedValue(InPackedValue) {}
	FVirtualTextureProducerHandle(uint32 InIndex, uint32 InMagic) : Index(InIndex), Magic(InMagic) {}

	uint32 PackedValue;
	struct
	{
		uint32 Index : 22;
		uint32 Magic : 10;
	};
};
static_assert(sizeof(FVirtualTextureProducerHandle) == sizeof(uint32), "Bad packing");

inline bool operator==(const FVirtualTextureProducerHandle& Lhs, const FVirtualTextureProducerHandle& Rhs) { return Lhs.PackedValue == Rhs.PackedValue; }
inline bool operator!=(const FVirtualTextureProducerHandle& Lhs, const FVirtualTextureProducerHandle& Rhs) { return Lhs.PackedValue != Rhs.PackedValue; }

/** Maximum number of layers that can be allocated in a single VT page table */
#define VIRTUALTEXTURE_SPACE_MAXLAYERS 8

/** Maximum dimension of VT page table texture (Duplicated in PageTableUpdate.usf, keep in sync) */
#define VIRTUALTEXTURE_LOG2_MAX_PAGETABLE_SIZE 11u
#define VIRTUALTEXTURE_MAX_PAGETABLE_SIZE (1u << VIRTUALTEXTURE_LOG2_MAX_PAGETABLE_SIZE)

/**
 * Parameters needed to create an IAllocatedVirtualTexture
 * Describes both page table and physical texture size, format, and layout
 */
struct FAllocatedVTDescription
{
	FVirtualTextureProducerHandle ProducerHandle[VIRTUALTEXTURE_SPACE_MAXLAYERS];
	uint32 TileSize = 0u;
	uint32 TileBorderSize = 0u;
	uint8 Dimensions = 0u;
	uint8 NumLayers = 0u;
	union
	{
		uint8 PackedFlags = 0u;
		struct
		{
			/**
			 * Should the AllocatedVT create its own dedicated page table allocation?  The system only supports a limited number of unique page tables, so this must be used carefully
			 */
			uint8 bPrivateSpace : 1;
		};
	};
	
	/**
	 * This allows arbitrary layers of each IVirtualTexture producer to map to each layer of the allocated VT
	 */
	uint8 LocalLayerToProduce[VIRTUALTEXTURE_SPACE_MAXLAYERS] = { 0u };
};

inline bool operator==(const FAllocatedVTDescription& Lhs, const FAllocatedVTDescription& Rhs)
{
	if (Lhs.TileSize != Rhs.TileSize ||
		Lhs.TileBorderSize != Rhs.TileBorderSize ||
		Lhs.Dimensions != Rhs.Dimensions ||
		Lhs.NumLayers != Rhs.NumLayers ||
		Lhs.PackedFlags != Rhs.PackedFlags)
	{
		return false;
	}
	for (uint32 LayerIndex = 0u; LayerIndex < Lhs.NumLayers; ++LayerIndex)
	{
		if (Lhs.ProducerHandle[LayerIndex] != Rhs.ProducerHandle[LayerIndex] ||
			Lhs.LocalLayerToProduce[LayerIndex] != Rhs.LocalLayerToProduce[LayerIndex])
		{
			return false;
		}
	}
	return true;
}
inline bool operator!=(const FAllocatedVTDescription& Lhs, const FAllocatedVTDescription& Rhs)
{
	return !operator==(Lhs, Rhs);
}

struct FVTProducerDescription
{
	FName Name; /** Will be name of UTexture for streaming VTs, mostly here for debugging */
	bool bPersistentHighestMip = true;
	bool bContinuousUpdate = false;
	bool bCreateRenderTarget = false;
	bool bZooxMeshTileVT = false;
	uint32 TileSize = 0u;
	uint32 TileBorderSize = 0u;
	uint32 WidthInTiles = 0u;
	uint32 HeightInTiles = 0u;
	uint32 DepthInTiles = 0u;
	uint8 Dimensions = 0u;
	uint8 NumLayers = 0u;
	uint8 MaxLevel = 0u;
	TEnumAsByte<EPixelFormat> LayerFormat[VIRTUALTEXTURE_SPACE_MAXLAYERS] = { PF_Unknown };
};

class IVirtualTextureFinalizer
{
public:
	virtual void Finalize(FRHICommandListImmediate& RHICmdList) = 0;
};

enum class EVTRequestPageStatus
{
	/** The request is invalid and no data will ever be available */
	Invalid,

	/**
	* Requested data is not being produced, and a request can't be started as some part of the system is at capacity.
	* Requesting the same data at a later time should succeed.
	*/
	Saturated,

	/**
	* Requested data is currently being produced, but is not yet ready.
	* It's valid to produce this data, but doing so may block until data is ready.
	*/
	Pending,

	/** Requested data is available */
	Available,
};

/** Check to see there is data available (possibly requiring waiting) given the current status */
FORCEINLINE bool VTRequestPageStatus_HasData(EVTRequestPageStatus InStatus) { return InStatus == EVTRequestPageStatus::Pending || InStatus == EVTRequestPageStatus::Available; }

enum class EVTRequestPagePriority
{
	Normal,
	High,
};

enum class EVTProducePageFlags : uint8
{
	None = 0u,
	SkipPageBorders = (1u << 0),
};
ENUM_CLASS_FLAGS(EVTProducePageFlags);

struct FVTRequestPageResult
{
	FVTRequestPageResult(EVTRequestPageStatus InStatus = EVTRequestPageStatus::Invalid, uint64 InHandle = 0u) : Handle(InHandle), Status(InStatus) {}

	/** Opaque handle to the request, must be passed to 'ProducePageData'.  Only valid if status is Pending/Available */
	uint64 Handle;

	/** Status of the request */
	EVTRequestPageStatus Status;
};

/** Describes a location to write a single layer of a VT tile */
struct FVTProduceTargetLayer
{
	/** The texture to write to */
	FRHITexture* TextureRHI = nullptr;

	TRefCountPtr<struct IPooledRenderTarget> PooledRenderTarget;

	/** Location within the texture to write */
	FIntVector pPageLocation;
};

/**
* This is the interface that can produce tiles of virtual texture data
* This can be extended to represent different ways of generating VT, such as disk streaming, runtime compositing, or whatever
* It's provided to the renderer module
*/
class IVirtualTexture
{
public:
	inline IVirtualTexture() {}
	virtual	~IVirtualTexture() {}

	/**
	 * Gives a localized mip bias for the given local vAddress.
	 * This is used to implement sparse VTs, the bias is number of mip levels to add to reach a resident page
	 * Must be thread-safe, may be called from any thread
	 * @param vLevel The mipmap level to check
	 * @param vAddress Virtual address to check
	 * @return Mip bias to be added to vLevel to reach a resident page at the given address
	 */
	virtual uint32 GetLocalMipBias(uint8 vLevel, uint32 vAddress) const { return 0u; }

	/**
	* Makes a request for the given page data.
	* For data sources that can generate data immediately, it's reasonable for this method to do nothing, and simply return 'Available'
	* Only called from render thread
	* @param ProducerHandle Handle to this producer, can be used as a UID for this producer for any internal caching mechanisms
	* @param LayerMask Mask of requested layers
	* @param vLevel The mipmap level of the data
	* @param vAddress Bit-interleaved x,y page indexes
	* @param Priority Priority of the request, used to drive async IO/task priority needed to generate data for request
	* @return FVTRequestPageResult describing the availability of the request
	*/
	virtual FVTRequestPageResult RequestPageData(const FVirtualTextureProducerHandle& ProducerHandle, uint8 LayerMask, uint8 vLevel, uint32 vAddress, EVTRequestPagePriority Priority) = 0;

	/**
	* Upload page data to the cache, data must have been previously requested, and reported either 'Available' or 'Pending'
	* The system will attempt to call RequestPageData/ProducePageData only once for a given vLevel/vAddress, with all the requested layers set in LayerMask,
	* this is important for certain types of procedural producers that may generate multiple layers of VT data at the same time
	* It's valid to produce 'Pending' page data, but in this case ProducePageData may block until data is ready
	* Only called from render thread
	* @param RHICmdList Used to write any commands required to generate the VT page data
	* @param FeatureLevel The current RHI feature level
	* @param ProducerHandle Handle to this producer
	* @param LayerMask Mask of requested layers; can be used to only produce data for these layers as an optimization, or ignored if all layers are logically produced together
	* @param vLevel The mipmap level of the data
	* @param vAddress Bit-interleaved x,y page indexes
	* @param RequestHandle opaque handle returned from 'RequestPageData'
	* @param TargetLayers Array of 'FVTProduceTargetLayer' structs, gives location where each layer should write data
	* @return a 'IVirtualTextureFinalizer' which must be finalized to complete the operation
	*/
	virtual IVirtualTextureFinalizer* ProducePageData(FRHICommandListImmediate& RHICmdList,
		ERHIFeatureLevel::Type FeatureLevel,
		EVTProducePageFlags Flags,
		const FVirtualTextureProducerHandle& ProducerHandle, uint8 LayerMask, uint8 vLevel, uint32 vAddress,
		uint64 RequestHandle,
		const FVTProduceTargetLayer* TargetLayers) = 0;

	virtual void DumpToConsole(bool verbose) {}
};

enum class EVTPageTableFormat : uint8
{
	UInt16,
	UInt32,
};

/**
* This interface represents a chunk of VT data allocated and owned by the renderer module, backed by both a page table texture, and a physical texture cache for each layer.
* Both page table and physical texture may be shared amongst many different allocated virtual textures.
* Any method that deals with physical texture requires an explicit LayerIndex parameter to identify the physical texture in question,
* methods that don't have LayerIndex parameter refer to properties shared by all textures using the given page table
* These are created with IRendererModule::AllocateVirtualTexture, and destroyed with IRendererModule::DestroyVirtualTexture
* They must be allocated from render thread, but may be destroyed from any thread
*/
class IAllocatedVirtualTexture
{
public:
	static const uint32 LayersPerPageTableTexture = 4u;

	inline IAllocatedVirtualTexture(const FAllocatedVTDescription& InDesc,
		uint32 InSpaceID,
		EVTPageTableFormat InPageTableFormat,
		uint32 InWidthInTiles,
		uint32 InHeightInTiles,
		uint32 InDepthInTiles)
		: Description(InDesc)
		, SpaceID(InSpaceID)
		, WidthInTiles(InWidthInTiles)
		, HeightInTiles(InHeightInTiles)
		, DepthInTiles(InDepthInTiles)
		, PageTableFormat(InPageTableFormat)
		, MaxLevel(0u)
		, VirtualAddress(~0u)
	{}

	virtual FRHITexture* GetPageTableTexture(uint32 InPageTableIndex) const = 0;
	virtual FRHITexture* GetPhysicalTexture(uint32 InLayerIndex) const = 0;
	virtual FRHIShaderResourceView* GetPhysicalTextureView(uint32 InLayerIndex, bool bSRGB) const = 0;
	virtual uint32 GetPhysicalTextureSize(uint32 InLayerIndex) const = 0;
	virtual void DumpToConsole(bool bVerbose) const {}

	inline const FAllocatedVTDescription& GetDescription() const { return Description; }
	inline const FVirtualTextureProducerHandle& GetProducerHandle(uint32 InLayerIndex) const { check(InLayerIndex < Description.NumLayers); return Description.ProducerHandle[InLayerIndex]; }
	inline uint32 GetLocalLayerToProduce(uint32 InLayerIndex) const { check(InLayerIndex < Description.NumLayers); return Description.LocalLayerToProduce[InLayerIndex]; }
	
	inline uint32 GetVirtualTileSize() const { return Description.TileSize; }
	inline uint32 GetTileBorderSize() const { return Description.TileBorderSize; }
	inline uint32 GetPhysicalTileSize() const { return Description.TileSize + Description.TileBorderSize * 2u; }
	inline uint32 GetNumLayers() const { return Description.NumLayers; }
	inline uint8 GetDimensions() const { return Description.Dimensions; }
	inline uint32 GetWidthInTiles() const { return WidthInTiles; }
	inline uint32 GetHeightInTiles() const { return HeightInTiles; }
	inline uint32 GetDepthInTiles() const { return DepthInTiles; }
	inline uint32 GetWidthInPixels() const { return WidthInTiles * Description.TileSize; }
	inline uint32 GetHeightInPixels() const { return HeightInTiles * Description.TileSize; }
	inline uint32 GetDepthInPixels() const { return DepthInTiles * Description.TileSize; }
	inline uint32 GetNumPageTableTextures() const { return (Description.NumLayers + LayersPerPageTableTexture - 1u) / LayersPerPageTableTexture; }
	inline uint32 GetSpaceID() const { return SpaceID; }
	inline uint32 GetVirtualAddress() const { return VirtualAddress; }
	inline uint32 GetMaxLevel() const { return MaxLevel; }
	inline EVTPageTableFormat GetPageTableFormat() const { return PageTableFormat; }

protected:
	friend class FVirtualTextureSystem;
	virtual void Destroy(class FVirtualTextureSystem* System) = 0;
	virtual ~IAllocatedVirtualTexture() {}

	FAllocatedVTDescription Description;
	uint32 SpaceID;
	uint32 WidthInTiles;
	uint32 HeightInTiles;
	uint32 DepthInTiles;
	EVTPageTableFormat PageTableFormat;

	// should be set explicitly by derived class constructor
	uint32 MaxLevel;
	uint32 VirtualAddress;
};

/**
 * Identifies a VT tile within a given producer
 */
union FVirtualTextureLocalTile
{
	inline FVirtualTextureLocalTile() {}
	inline FVirtualTextureLocalTile(const FVirtualTextureProducerHandle& InProducerHandle, uint32 InLocal_vAddress, uint8 InLocal_vLevel)
		: PackedProducerHandle(InProducerHandle.PackedValue), Local_vAddress(InLocal_vAddress), Local_vLevel(InLocal_vLevel), Pad(0)
	{}

	inline FVirtualTextureProducerHandle GetProducerHandle() const { return FVirtualTextureProducerHandle(PackedProducerHandle); }

	uint64 PackedValue;
	struct
	{
		uint32 PackedProducerHandle;
		uint32 Local_vAddress : 24;
		uint32 Local_vLevel : 4;
		uint32 Pad : 4;
	};
};
static_assert(sizeof(FVirtualTextureLocalTile) == sizeof(uint64), "Bad packing");

inline uint64 GetTypeHash(const FVirtualTextureLocalTile& T) { return T.PackedValue; }
inline bool operator==(const FVirtualTextureLocalTile& Lhs, const FVirtualTextureLocalTile& Rhs) { return Lhs.PackedValue == Rhs.PackedValue; }
inline bool operator!=(const FVirtualTextureLocalTile& Lhs, const FVirtualTextureLocalTile& Rhs) { return Lhs.PackedValue != Rhs.PackedValue; }

DECLARE_STATS_GROUP(TEXT("Virtual Texturing"), STATGROUP_VirtualTexturing, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Virtual Texture Memory"), STATGROUP_VirtualTextureMemory, STATCAT_Advanced);
