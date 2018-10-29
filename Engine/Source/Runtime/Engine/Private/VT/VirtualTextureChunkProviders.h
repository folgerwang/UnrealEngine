#pragma once

#include "VirtualTextureTypes.h"
#include "VirtualTextureBuiltData.h"

class IFileCacheReadBuffer;
class IFileCacheHandle;

/**
 * This class provides data streamed from disk to an uploading virtual texture.
 */
class FChunkProvider
{
public:
	FChunkProvider(class UVirtualTexture* inOwner);
	virtual ~FChunkProvider();

	// converted virtual address+level to local pageID
	int32 GetChunkIndex(const TileID& id) const;
	// read a portion of a chunk
	IFileCacheReadBuffer *GetData(ChunkID id, size_t Offset, size_t Size);
	// get the size of the headers, stored in front of the chunk
	int32 GetChunkHeaderSize(uint32 ChunkIndex) const;

	// Get the size for a single tile after transcoding
	size_t GetTileMemSize();
	// Get the memory size of a tile on a specific layer 
	int32 GetTileLayerSize(const TileID& id, int layer) const;
	// Get the memory size of the entire tile
	uint32 GetTileSize(const TileID& id);
	// Get the offset the a tile location
	size_t GetTileOffset(const TileID& id);
	
	// Get the codec of a layer
	// PageDataBuffer is the data read-in from disk containing the header
	uint8 GetCodecId(uint8 *PageDataBuffer, uint32 Layer);
	// Get the codec context payload
	// PageDataBuffer is the data read-in from disk containing the header
	bool GetCodecPayload(uint8 *PageDataBuffer, uint32 Layer, uint8*& outPayload, size_t& outPayloadSize);
		

	uint32 GetVirtualTileSize() const { ensure(Data->TileWidth == Data->TileHeight);	return Data->TileWidth; }
	uint32 GetTileBorderSize() const { return Data->Border; }
	inline uint32 GetTilePixelSize() const { return TileSize + 2 * TileBorder; }
	uint32 GetNumTilesX() const { return Data->NumTilesX; }
	uint32 GetNumTilesY() const { return Data->NumTilesY; }
	IVirtualTextureSpace* GetSpace() const { return Space; }
	uint32 GetNumLayers() const { return Data->GetNumLayers(); }

	uint64 v_Address = ~0u;

	void DumpToConsole();

private:
	uint32 TileSize;
	uint32 TileBorder;

protected:
	IVirtualTextureSpace* Space;
 	FVirtualTextureBuiltData* Data;

	struct ReadRequestData
	{
		class IAsyncReadFileHandle* ReadFileHandle;
		class IAsyncReadRequest* ReadRequest;
	};
	TArray<ReadRequestData> ReadRequests;
	TMap<FString, IFileCacheHandle*> HandleMap;
};
