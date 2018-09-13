#pragma once
#include "CoreMinimal.h"
#include "PixelFormat.h"
#include "Serialization/BulkData.h"
#include "Engine/Texture.h"

// This header contains the all the structs and classes pertaining to the
// virtual texture on-disc file format.

#define MAX_NUM_LAYERS 4
#define MAX_NUM_MIPS 16

/** Custom serialization version for FFileCache */
struct FVirtualTextureBuiltDataCustomVersion
{
	static const FGuid Key;
	enum Type {
		Initial,
		MipTails,			// Added support for mipmap tails
		ActualSize,			// Actual size is explicitly stored in the file instead of derived based on tile size
		MacroBlocks,		// Refactor of the Build data to be stored in macroblocks
		Latest = MacroBlocks// Always update this to be equal to the latest version
	};
};

enum class EVirtualTextureCodec
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

/**
* The header stored at the start of every chunk. Actually empty currently could be useful later on.
* Immediately after this struct follow the FVirtualTextureTileInfo structs describing the individual tiles
*/
#define Dummy_Magic 0xf67fd98e
struct FVirtualTextureChunkInfo
{
	int32 Dummy; // atm should always be Dummy_Magic
};

/**
* Stored for every layer.
*/
struct FVirtualTextureChunkLayerInfo
{
	int32 Codec : 8; // CodecID of this tile
	int32 CodecPayloadSize;
	int32 CodecPayloadOffset;
};


/**
* This struct should store any per tile info that should be available all the time. I.e. this info is always kept in memory for every tile.
* As this may run into several megabytes for large VT's this should be as limited as possible. Keep any info that is only needed for
* resident tiles in FVirtualTextureChunkTileInfo.
*/
#define FVirtualTextureTileInfo_ChunkBits 16
#define FVirtualTextureTileInfo_IndexBits 16
struct FVirtualTextureTileInfo
{
	int16 Chunk; // The index of the chunk the tile is stored in	
	int32 Offset;
	TArray<int32, TFixedAllocator<MAX_NUM_LAYERS>> Size;

	friend FArchive& operator<<(FArchive& Ar, FVirtualTextureTileInfo& A)
	{
		Ar << A.Chunk;
		Ar << A.Offset;
		Ar << A.Size;
		return Ar;
	}
};

struct FVirtualTextureMipTail
{
	int32 SizeX;
	int32 SizeY;
	int32 SizeZ;
	TArray<uint8> Data; //The data format is the same as the tile format.

	friend FArchive& operator<<(FArchive& Ar, FVirtualTextureMipTail& A)
	{
		Ar << A.SizeX;
		Ar << A.SizeY;
		Ar << A.SizeZ;
		Ar << A.Data;
		return Ar;
	}
};

struct FVirtualTextureBuiltData
{
	int32 Width; // Width of the texture in pixels. Note the physical width may be larger due to tiling
	int32 Height; // Height of the texture in pixels. Note the physical height may be larger due to tiling
	int32 TileWidth;  // Tile size excluding borders
	int32 TileHeight;  // Tile size excluding borders
	int32 NumTilesX; // Number of tiles on x axis (width in tiles)
	int32 NumTilesY; // Number of tiles on y axis (height in tiles)
	int32 Border; // A BorderSize pixel border will be added around all tiles

	/* The pixel format output of the data on the i'th layer. The actual data
	* may still be compressed but will decompress to this pixel format (e.g. zipped DXT5 data).
	*/
	TArray<EPixelFormat, TFixedAllocator<MAX_NUM_LAYERS>> LayerTypes;

	/* Per miplevel the the data for each tile. Within a miplevel the tiles are added in scanline order.
	* Note: There will most likely be less miplevels in the generated data than in the source data.
	* The number of mips generated ensures the last mip is always at least 1 tile wide. So for textures which are twice as wide as high
	* the last level will contain 2x1 tiles.
	*/
	TArray<FByteBulkData> Chunks;
	TArray<uint32> ChunkHeaderSizes;

	/**
	 * Info for the tiles organized per level. Within a level tile info is organized in Morton order.
	 * FIXME: This is in morton order which can waste a lot of space in this array for non-square images
	 * e.g.:
	 * - An 8x1 tile image will allocate 8x4 indexes in this array.
	 * - An 1x8 tile image will allocate 8x8 indexes in this array.
	 * Solutions?
	 * - Use a hash table here to avoid allocating space for every code
	 * - Store in scanline order
	 */
	TArray<TArray<FVirtualTextureTileInfo>, TFixedAllocator<MAX_NUM_MIPS>> Tiles;

	/**
	 * Mip tail data for every layer in the vt.
	 */
	TArray<TArray<FVirtualTextureMipTail>, TFixedAllocator<MAX_NUM_LAYERS>> MipTails;


	FVirtualTextureBuiltData() :
		TileWidth(0),
		TileHeight(0),
		NumTilesX(0),
		NumTilesY(0),
		Border(0)
	{}

	void Serialize(FArchive& Ar, UObject* Owner);

	bool IsInitialized() const
	{
		return TileWidth != 0;
	}

	inline uint32 GetPhysicalWidth(uint32 level) const
	{
		checkf(level < (uint32)Tiles.Num(), TEXT("Invalid level"));
		return (TileWidth*NumTilesX) >> level;
	}

	inline uint32 GetPhysicalHeight(uint32 level) const
	{
		checkf(level < (uint32)Tiles.Num(), TEXT("Invalid level"));
		return (TileHeight*NumTilesY) >> level;
	}

	inline uint32 GetNumLayers() const 
	{
		return LayerTypes.Num();
	}


	/*
	 * Return the index of the chunk this tile's data is in.
	 */
	int32 GetChunkIndex(int32 vPage, int32 vLevel)
	{
		if (vLevel >= Tiles.Num())
		{
			return -1;
		}
		if (vPage >= Tiles[vLevel].Num())
		{
			return -1;
		}
		return Tiles[vLevel][vPage].Chunk;
	}

	/*
	* Return the offset of this tile within the chunk
	*/
	int32 GetTileOffset(int32 vPage, int32 vLevel /*,uint8 *ChunkData, uint32 Layer*/)
	{
		if (vLevel >= Tiles.Num())
		{
			return -1;
		}
		if (vPage >= Tiles[vLevel].Num())
		{
			return -1;
		}
		return  Tiles[vLevel][vPage].Offset;
	}

	/*
	* Return the size of this tile on a specific layer
	*/
	int32 GetTileSize(int32 vPage, int32 vLevel, uint32 Layer)
	{
		if (vLevel >= Tiles.Num())
		{
			return -1;
		}
		if (vPage >= Tiles[vLevel].Num())
		{
			return -1;
		}
		return Tiles[vLevel][vPage].Size[Layer];
	}

	/*
	* Return the codec used by a layer
	*/
	uint8 GetCodecID(uint32 Layer, uint8* ChunkData)
	{
		FVirtualTextureChunkInfo *ChunkHeader = (FVirtualTextureChunkInfo *)ChunkData;
		ensure(ChunkHeader->Dummy == Dummy_Magic);

		FVirtualTextureChunkLayerInfo* Infos = (FVirtualTextureChunkLayerInfo*)(ChunkData + sizeof(FVirtualTextureChunkInfo));
		return Infos[Layer].Codec;
	}

	/*
	* Return the codec payload for a specific layer
	*/
	bool GetCodecPayload(uint8* ChunkData, uint32 Layer, uint8*& outPayload, size_t& outPayloadSize)
	{
		FVirtualTextureChunkInfo *ChunkHeader = (FVirtualTextureChunkInfo *)ChunkData;
		ensure(ChunkHeader->Dummy == Dummy_Magic);

		FVirtualTextureChunkLayerInfo* Infos = (FVirtualTextureChunkLayerInfo*)(ChunkData + sizeof(FVirtualTextureChunkInfo));
		outPayload = ChunkData + Infos[Layer].CodecPayloadOffset;
		outPayloadSize = Infos[Layer].CodecPayloadSize;
		return true;
	}

	/*
	* Return the size of the codec header
	*/
	size_t GetChunkHeaderSize(uint32 id) const
	{
		return ChunkHeaderSizes[id];
	}

};