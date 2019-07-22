// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "PixelFormat.h"
#include "Serialization/BulkData.h"
#include "Engine/Texture.h"
#include "HAL/ThreadSafeBool.h"

// This header contains the all the structs and classes pertaining to the
// virtual texture on-disc file format.

/** Max number of layers that can be stored in a VT asset, may be lower than number of VT layers that can be stored in page table */
#define VIRTUALTEXTURE_DATA_MAXLAYERS 8u

/** Max number of mips that can be stored in a VT asset */
#define VIRTUALTEXTURE_DATA_MAXMIPS 16u

enum class EVirtualTextureCodec : uint8
{
	Black,			//Special case codec, always outputs black pixels 0,0,0,0
	OpaqueBlack,	//Special case codec, always outputs opaque black pixels 0,0,0,255
	White,			//Special case codec, always outputs white pixels 255,255,255,255
	Flat,			//Special case codec, always outputs 128,125,255,255 (flat normal map)
	RawGPU,			//Uncompressed data in an GPU-ready format (e.g R8G8B8A8, BC7, ASTC, ...)
	ZippedGPU,		//Same as RawGPU but with the data zipped
	Crunch,			//Use the Crunch library to compress data
	Max,			// Add new codecs before this entry
};

struct FVirtualTextureDataChunk
{
	FByteBulkData BulkData;
	uint32 SizeInBytes;
	uint32 CodecPayloadSize;
	uint16 CodecPayloadOffset[VIRTUALTEXTURE_DATA_MAXLAYERS];
	EVirtualTextureCodec CodecType[VIRTUALTEXTURE_DATA_MAXLAYERS];

	inline FVirtualTextureDataChunk()
		: SizeInBytes(0u)
		, CodecPayloadSize(0u)
	{
		FMemory::Memzero(CodecPayloadOffset);
		FMemory::Memzero(CodecType);
	}

	inline uint32 GetMemoryFootprint() const
	{
		// Don't include editor-only data
		static constexpr uint32 MemoryFootprint = uint32(sizeof(BulkData) + sizeof(SizeInBytes) + sizeof(CodecPayloadOffset) + sizeof(CodecPayloadSize) + sizeof(CodecType));
		return MemoryFootprint;
	}

#if WITH_EDITORONLY_DATA
	/** Key if stored in the derived data cache. */
	FString DerivedDataKey;

	// Cached short key for VT DDC cache (not serialized)
	FString ShortDerivedDataKey;
	bool ShortenKey(const FString& CacheKey, FString& Result);
	FThreadSafeBool bFileAvailableInVTDDCDache;

	uint32 StoreInDerivedDataCache(const FString& InDerivedDataKey);
#endif // WITH_EDITORONLY_DATA
};

struct FVirtualTextureBuiltData
{
	uint32 NumLayers;
	uint32 NumMips;
	uint32 Width; // Width of the texture in pixels. Note the physical width may be larger due to tiling
	uint32 Height; // Height of the texture in pixels. Note the physical height may be larger due to tiling
	uint32 WidthInBlocks; // Number of UDIM blocks that make up the texture, used to compute UV scaling factor
	uint32 HeightInBlocks;
	uint32 TileSize;  // Tile size excluding borders
	uint32 TileBorderSize; // A BorderSize pixel border will be added around all tiles

	/**
	 * The pixel format output of the data on the i'th layer. The actual data
	 * may still be compressed but will decompress to this pixel format (e.g. zipped DXT5 data).
	 */
	TEnumAsByte<EPixelFormat> LayerTypes[VIRTUALTEXTURE_DATA_MAXLAYERS];

	/**
	 * Tile data is packed into separate chunks, typically there is 1 mip level in each chunk for high resolution mips.
	 * After a certain threshold, all remaining low resolution mips will be packed into one final chunk.
	 */
	TArray<FVirtualTextureDataChunk> Chunks;

	/** Index of the first tile within each chunk */
	TArray<uint32> TileIndexPerChunk;

	/** Index of the first tile within each mip level */
	TArray<uint32> TileIndexPerMip;

	/**
	 * Info for the tiles organized per level. Within a level tile info is organized in Morton order.
	 * This is in morton order which can waste a lot of space in this array for non-square images
	 * e.g.:
	 * - An 8x1 tile image will allocate 8x4 indexes in this array.
	 * - An 1x8 tile image will allocate 8x8 indexes in this array.
	 */
	TArray<uint32> TileOffsetInChunk;

	FVirtualTextureBuiltData()
		: NumLayers(0u)
		, NumMips(0u)
		, Width(0u)
		, Height(0u)
		, WidthInBlocks(0u)
		, HeightInBlocks(0u)
		, TileSize(0u)
		, TileBorderSize(0u)
	{
		FMemory::Memzero(LayerTypes);
	}

	inline bool IsInitialized() const { return TileSize != 0u; }
	inline uint32 GetNumMips() const { return NumMips; }
	inline uint32 GetNumLayers() const { return NumLayers; }
	inline uint32 GetPhysicalTileSize() const { return TileSize + TileBorderSize * 2u; }
	inline uint32 GetWidthInTiles() const { return FMath::DivideAndRoundUp(Width, TileSize); }
	inline uint32 GetHeightInTiles() const { return FMath::DivideAndRoundUp(Height, TileSize); }

	uint32 GetMemoryFootprint() const;
	uint32 GetTileMemoryFootprint() const;
	uint64 GetDiskMemoryFootprint() const;
	uint32 GetNumTileHeaders() const;
	void Serialize(FArchive& Ar, UObject* Owner, int32 FirstMipToSerialize);

	/**
	 * Return the index of the given tile
	 */
	inline uint32 GetTileIndex(uint8 vLevel, uint32 vAddress) const
	{
		check(vLevel < NumMips);
		const uint32 TileIndex = TileIndexPerMip[vLevel] + vAddress * NumLayers;
//		check(TileIndex < TileIndexPerMip[vLevel + 1]);
		return TileIndex;
	}

	/**
	 * Return the index of the chunk that contains the given tile
	 */
	inline int32 GetChunkIndex(uint32 TileIndex) const
	{
		if (TileIndex >= (uint32)TileOffsetInChunk.Num())
		{
			return -1;
		}

		for (int ChunkIndex = 0; ChunkIndex < TileIndexPerChunk.Num(); ++ChunkIndex)
		{
			if (TileIndex < TileIndexPerChunk[ChunkIndex + 1])
			{
				const uint32 NextTileOffset = GetTileOffset(ChunkIndex, TileIndex + GetNumLayers());
				if (TileOffsetInChunk[TileIndex] == NextTileOffset)
				{
					// Size of the tile is 0, this means the given tile is not valid for this virtual texture
					// This can happen since tile offset are stored with morton encoding, so non-square VTs will have some empty tiles allocated
					return -1;
				}

				return ChunkIndex;
			}
		}

		checkNoEntry();
		return -1;
	}

	/**
	* Return the offset of this tile within the chunk
	*/
	inline uint32 GetTileOffset(uint32 ChunkIndex, uint32 TileIndex) const
	{
		check(TileIndex >= TileIndexPerChunk[ChunkIndex]);
		if (TileIndex < TileIndexPerChunk[ChunkIndex + 1])
		{
			return TileOffsetInChunk[TileIndex];
		}

		// If TileIndex is past the end of chunk, return the size of chunk
		// This allows us to determine size of region by asking for start/end offsets
		return Chunks[ChunkIndex].SizeInBytes;
	}
};
