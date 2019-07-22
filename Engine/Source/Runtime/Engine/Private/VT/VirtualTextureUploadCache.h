// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "RendererInterface.h"
#include "PixelFormat.h"

class FRHITexture2D;
class FRHIStructuredBuffer;

struct FVTUploadTileBuffer
{
	void* Memory = nullptr;
	uint32 Stride = 0u;
};

//
struct FVTUploadTileHandle
{
	explicit FVTUploadTileHandle(uint32 InIndex = 0u) : Index(InIndex) {}

	inline bool IsValid() const { return Index != 0u; }

	uint32 Index;
};

class FVirtualTextureUploadCache : public IVirtualTextureFinalizer
{
public:
	FVirtualTextureUploadCache();
	virtual ~FVirtualTextureUploadCache();

	virtual void Finalize(FRHICommandListImmediate& RHICmdList) override;

	uint32 GetNumPendingTiles() const { return NumPendingTiles; }

	FVTUploadTileHandle PrepareTileForUpload(FVTUploadTileBuffer& OutBuffer, EPixelFormat InFormat, uint32 InTileSize);
	void SubmitTile(FRHICommandListImmediate& RHICmdList, const FVTUploadTileHandle& InHandle, FRHITexture2D* InDestTexture, int InDestX, int InDestY, int InSkipBorderSize);
	void CancelTile(const FVTUploadTileHandle& InHandle);

	void UpdateFreeList();

private:
	enum EListType
	{
		LIST_SUBMITTED,

		LIST_COUNT,
	};

	static const uint32 NUM_STAGING_TEXTURES = 3u;

	struct FStagingTexture
	{
		TRefCountPtr<FRHITexture2D> RHITexture;
		uint32_t WidthInTiles = 0u;
		uint32_t BatchCapacity = 0u;
	};

	struct FPoolEntry
	{
		FStagingTexture StagingTexture[NUM_STAGING_TEXTURES];
		EPixelFormat Format = PF_Unknown;
		uint32 TileSize = 0u;
		uint32 BatchTextureIndex = 0u;
		uint32 BatchCount = 0u;
		int32 FreeTileListHead = 0;
		int32 SubmitTileListHead = 0;
	};

	struct FTileEntry
	{
		FTileEntry();
		~FTileEntry();

		TRefCountPtr<FRHIStructuredBuffer> RHIStagingBuffer;
		FRHITexture2D* RHISubmitTexture = nullptr;
		void* Memory = nullptr;
		uint32 Stride = 0u;
		uint32 FrameSubmitted = 0u;
		uint32 SubmitBatchIndex = 0u;
		int32 SubmitDestX = 0;
		int32 SubmitDestY = 0;
		int32 SubmitSkipBorderSize = 0;
		int32 PoolIndex = 0;
		int32 NextIndex = 0;
		int32 PrevIndex = 0;
	};

	int32 GetOrCreatePoolIndex(EPixelFormat InFormat, uint32 InTileSize);

	int32 CreateTileEntry(int32 PoolIndex)
	{
		const int32 Index = Tiles.AddDefaulted();
		FTileEntry& Entry = Tiles[Index];
		Entry.NextIndex = Entry.PrevIndex = Index;
		Entry.PoolIndex = PoolIndex;
		return Index;
	}

	void RemoveFromList(int32 Index)
	{
		FTileEntry& Entry = Tiles[Index];
		checkSlow(Index >= LIST_COUNT); // if we're trying to remove a list head, something is corrupt
		Tiles[Entry.PrevIndex].NextIndex = Entry.NextIndex;
		Tiles[Entry.NextIndex].PrevIndex = Entry.PrevIndex;
		Entry.NextIndex = Entry.PrevIndex = Index;
	}

	void AddToList(int32 HeadIndex, int32 Index)
	{
		FTileEntry& Head = Tiles[HeadIndex];
		FTileEntry& Entry = Tiles[Index];

		// make sure we're not currently in any list
		check(Entry.NextIndex == Index);
		check(Entry.PrevIndex == Index);

		Entry.NextIndex = HeadIndex;
		Entry.PrevIndex = Head.PrevIndex;
		Tiles[Head.PrevIndex].NextIndex = Index;
		Head.PrevIndex = Index;
	}

	TArray<FPoolEntry> Pools;
	TArray<FTileEntry> Tiles;
	uint32 NumPendingTiles = 0u;
};
