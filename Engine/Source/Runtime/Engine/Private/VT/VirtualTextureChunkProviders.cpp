#include "VirtualTextureChunkProviders.h"
#include "VirtualTextureChunkManager.h"
#include "VT/VirtualTexture.h"
#include "VT/VirtualTextureSpace.h"
#include "VirtualTextureBuiltData.h"
#include "Async/AsyncFileHandle.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFilemanager.h"
#include "Runtime/Renderer/Public/VirtualTexturing.h"
#include "FileCache/FileCache.h"

FChunkProvider::FChunkProvider(UVirtualTexture* inOwner)
{
	Space = inOwner->Space->GetRenderResource();
	Data = inOwner->GetTextureBuildData();
	TileSize = inOwner->Space->TileSize;
	TileBorder = inOwner->Space->BorderWidth;
	FVirtualTextureChunkStreamingManager::Get().AddChunkProvider(this);
	ensure(v_Address != ~((uint64)0));
}

FChunkProvider::~FChunkProvider()
{
	// close any remaining files
	for (auto iter = HandleMap.CreateIterator(); iter; ++iter)
	{
		if (iter.Value())
		{
			delete iter.Value();
		}
	}
	HandleMap.Empty();

	FVirtualTextureChunkStreamingManager::Get().RemoveChunkProvider(this);
}

int32 FChunkProvider::GetChunkIndex(const TileID& id) const
{
	uint8 vLevel = 0;
	uint64 vAddress = 0;
	FromTileID(id, vLevel, vAddress);
	
	int32 idx = Data->GetChunkIndex(vAddress, vLevel);
	return idx;
}

int32 FChunkProvider::GetTileLayerSize(const TileID& id, int Layer) const
{
	uint8 vLevel = 0;
	uint64 vAddress = 0;
	FromTileID(id, vLevel, vAddress);
	return Data->GetTileSize(vAddress, vLevel, Layer);
}

uint32 FChunkProvider::GetTileSize(const TileID& id)
{
	uint32 Size = 0;
	for (uint32 Layer = 0; Layer < Data->GetNumLayers(); ++Layer)
	{
		Size += GetTileLayerSize(id, Layer);
	}
	return Size;
}

int32 FChunkProvider::GetChunkHeaderSize(uint32 ChunkIndex) const
{
	return Data->GetChunkHeaderSize(ChunkIndex);
}

size_t FChunkProvider::GetTileMemSize()
{
	size_t size = 0;
	for (uint32 Layer = 0; Layer < GetNumLayers(); ++Layer)
	{
		size += CalculateImageBytes(GetTilePixelSize(), GetTilePixelSize(), 1, GetSpace()->GetPhysicalTextureFormat(Layer));
	}
	return size;
}

IFileCacheReadBuffer *FChunkProvider::GetData(ChunkID ChunkIdx, size_t Offset, size_t Size)
{
	FByteBulkData& BulkData = Data->Chunks[ChunkIdx];
	ensure(Size <= (size_t)BulkData.GetBulkDataSize());

	// This can happen in the editor if the asset hasn't been saved yet.
	if (BulkData.IsBulkDataLoaded())
	{
		const uint8 *P = (uint8*)BulkData.LockReadOnly() + Offset;		
		IFileCacheReadBuffer *Buffer = new FAllocatedFileCacheReadBuffer(P, Size);
		BulkData.Unlock();
		return Buffer;
	}

	IFileCacheHandle *Handle = HandleMap.FindRef(BulkData.GetFilename());
	if (Handle == NULL)
	{
		Handle = IFileCacheHandle::CreateFileCacheHandle(BulkData.GetFilename());
		checkf(Handle != nullptr, TEXT("Could not create a file cache for '%s'."), *BulkData.GetFilename());
		HandleMap.Add(BulkData.GetFilename(), Handle);
	}

	IFileCacheReadBuffer *Buffer = Handle->ReadData(BulkData.GetBulkDataOffsetInFile() + Offset, Size, AIOP_Normal);
	return Buffer;
}

size_t FChunkProvider::GetTileOffset(const TileID& id)
{
	uint8 vLevel = 0;
	uint64 vAddress = 0;
	FromTileID(id, vLevel, vAddress);
	return Data->GetTileOffset(vAddress, vLevel);
}

uint8 FChunkProvider::GetCodecId(uint8 *PageDataBuffer, uint32 Layer)
{
	return Data->GetCodecID(Layer, PageDataBuffer);
}

bool FChunkProvider::GetCodecPayload(uint8 *PageDataBuffer, uint32 Layer, uint8*& outPayload, size_t& outPayloadSize)
{
	return Data->GetCodecPayload(PageDataBuffer, Layer, outPayload, outPayloadSize);
}

void FChunkProvider::DumpToConsole()
{
	UE_LOG(LogConsoleResponse, Display, TEXT("Disc Page Provider"));
	UE_LOG(LogConsoleResponse, Display, TEXT("Width: %i"), Data->Width);
	UE_LOG(LogConsoleResponse, Display, TEXT("Height: %i"), Data->Height);
	UE_LOG(LogConsoleResponse, Display, TEXT("Tiles X: %i"), Data->NumTilesX);
	UE_LOG(LogConsoleResponse, Display, TEXT("Tiles Y: %i"), Data->NumTilesY);
	UE_LOG(LogConsoleResponse, Display, TEXT("Tile Width: %i"), Data->TileWidth);
	UE_LOG(LogConsoleResponse, Display, TEXT("Tile Height: %i"), Data->TileHeight);
	UE_LOG(LogConsoleResponse, Display, TEXT("Tile Border: %i"), Data->Border);
	UE_LOG(LogConsoleResponse, Display, TEXT("Chunks: %i"), Data->Chunks.Num());

	for (int32 L = 0; L < Data->Tiles.Num(); L++)
	{
		for (int32 T = 0; T < Data->Tiles[L].Num(); T++)
		{
			FVirtualTextureTileInfo &Tile = Data->Tiles[L][T];
			uint64 lX = FMath::ReverseMortonCode2(T);
			uint64 lY = FMath::ReverseMortonCode2(T >> 1);
			
			uint64 SpaceAddress = v_Address + T;
			uint64 vX = FMath::ReverseMortonCode2(SpaceAddress);
			uint64 vY = FMath::ReverseMortonCode2(SpaceAddress >> 1);

			// Check if the tile is resident if so print physical info as well
			uint64 pAddr = Space->GetPhysicalAddress(L, SpaceAddress);
			if (pAddr != ~0u)
			{
				uint64 pX = FMath::ReverseMortonCode2(pAddr);
				uint64 pY = FMath::ReverseMortonCode2(pAddr >> 1);
				UE_LOG(LogConsoleResponse, Display, TEXT(
					"Tile: Level %i, lAddr %i (%i,%i), vAddr %i (%i,%i), pAddr %i (%i,%i), Chunk %i, Offset %i, Size %i %i %i %i"),
					L, T, lX, lY,
					SpaceAddress, vX, vY,
					pAddr, pX, pY,
					Tile.Chunk, Tile.Offset, Tile.Size[0], Tile.Size[1], Tile.Size[2], Tile.Size[3]);
			}
			else
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("Tile: Level %i, lAddr %i (%i,%i), vAddr %i (%i,%i), Chunk %i, Offset %i, Size %i %i %i %i"),
					L, T, lX, lY,
					SpaceAddress, vX, vY,
					Tile.Chunk, Tile.Offset, Tile.Size[0], Tile.Size[1], Tile.Size[2], Tile.Size[3]);
			}
		}
	}

}