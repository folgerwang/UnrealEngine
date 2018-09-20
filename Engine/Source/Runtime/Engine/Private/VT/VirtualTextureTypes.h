#pragma once

#include "Runtime/Renderer/Public/VirtualTexturing.h"
#include "CoreMinimal.h"

using ChunkID = uint64;
using TileID = uint64;

#define INVALID_CHUNK_ID (~0)
#define INVALID_TILE_ID (~0)

union TileUnion
{
	TileID packed;
	struct _loose
	{
		uint32 VTextureID : 24;
		uint32 vLevel : 8;
		uint32 VAddress : 32;
	} loose;
};
static_assert(sizeof(TileUnion) == sizeof(uint64), "");

inline TileID GetTileID(IVirtualTexture* vTexture, uint8 vLevel, uint64 vAddress)
{
	// TODO atm vAddress is also 64bit but the upper bits are not used so highjack them
	// figure out if they are really unused 
	ensure((vAddress >> 32) == 0);
	TileUnion u;
	u.loose.vLevel = vLevel;

	// rescale vAddress to the correct vLevel tile
	vAddress = vAddress >> (vLevel * 2); //times 2 because vAddress is morton2d

	u.loose.VAddress = (uint32)vAddress;
	u.loose.VTextureID = vTexture->UniqueId;
	ensure(u.packed != INVALID_TILE_ID);
	return u.packed;
}

inline void FromTileID(const TileID& id, uint8& vLevel, uint64& vAddress)
{
	TileUnion u;
	u.packed = id;
	vLevel = u.loose.vLevel;
	vAddress = u.loose.VAddress;
}

union ChunkUnion
{
	ChunkID packed;
	struct _loose
	{
		uint32 VTextureID;
		uint32 ChunkIndex;
	} loose;
};
static_assert(sizeof(ChunkUnion) == sizeof(uint64), "");

inline ChunkID LocalChunkIdToGlobal(int32 id, IVirtualTexture* vTexture)
{
	ChunkUnion u;
	u.loose.VTextureID = vTexture->UniqueId;
	u.loose.ChunkIndex = id;
	ensure(u.packed != INVALID_CHUNK_ID);
	return u.packed;
}

inline void GlobalChunkIdToLocal(const ChunkID& id, int32& idx)
{
	ChunkUnion u;
	u.packed = id;
	idx = u.loose.ChunkIndex;
}