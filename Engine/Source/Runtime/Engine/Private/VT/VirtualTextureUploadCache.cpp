// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureUploadCache.h"
#include "VirtualTextureChunkManager.h"
#include "RHI.h"

// Stage to persist mapped GPU buffer then GPU copy into texture
// this is fast where supported
#if PLATFORM_PS4
static const bool ALLOW_COPY_FROM_BUFFER = true;
#else
static const bool ALLOW_COPY_FROM_BUFFER = false;
#endif

// allow uploading CPU buffer directly to GPU texture
// this is slow under D3D11
// Should be pretty decent on D3D12X...UpdateTexture does make an extra copy of the data, but Lock/Unlock texture also buffers an extra copy of texture on this platform
// Might also be worth enabling this path on PC D3D12, need to measure
// 'ALLOW_COPY_FROM_BUFFER' would still be better, but involves more Xbox-specific RHI work
#if PLATFORM_XBOXONE
static const bool ALLOW_UPDATE_TEXTURE = true;
#else
static const bool ALLOW_UPDATE_TEXTURE = false;
#endif

DECLARE_MEMORY_STAT_POOL(TEXT("Total GPU Upload Memory"), STAT_TotalGPUUploadSize, STATGROUP_VirtualTextureMemory, FPlatformMemory::MCR_GPU);
DECLARE_MEMORY_STAT(TEXT("Total CPU Upload Memory"), STAT_TotalCPUUploadSize, STATGROUP_VirtualTextureMemory);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Upload Entries"), STAT_NumUploadEntries, STATGROUP_VirtualTextureMemory);

FVirtualTextureUploadCache::FTileEntry::FTileEntry() {}
FVirtualTextureUploadCache::FTileEntry::~FTileEntry() {}

FVirtualTextureUploadCache::FVirtualTextureUploadCache()
{
	Tiles.AddDefaulted(LIST_COUNT);
	for (int i = 0; i < LIST_COUNT; ++i)
	{
		FTileEntry& Entry = Tiles[i];
		Entry.NextIndex = Entry.PrevIndex = i;
	}
}

FVirtualTextureUploadCache::~FVirtualTextureUploadCache()
{
}

int32 FVirtualTextureUploadCache::GetOrCreatePoolIndex(EPixelFormat InFormat, uint32 InTileSize)
{
	for (int32 i = 0; i < Pools.Num(); ++i)
	{
		const FPoolEntry& Entry = Pools[i];
		if (Entry.Format == InFormat && Entry.TileSize == InTileSize)
		{
			return i;
		}
	}

	const int32 PoolIndex = Pools.AddDefaulted();
	FPoolEntry& Entry = Pools[PoolIndex];
	Entry.Format = InFormat;
	Entry.TileSize = InTileSize;
	Entry.FreeTileListHead = CreateTileEntry(PoolIndex);
	Entry.SubmitTileListHead = CreateTileEntry(PoolIndex);

	return PoolIndex;
}

void FVirtualTextureUploadCache::Finalize(FRHICommandListImmediate& RHICmdList)
{
	SCOPE_CYCLE_COUNTER(STAT_VTP_FlushUpload)

	check(IsInRenderingThread());

	for (int PoolIndex = 0; PoolIndex < Pools.Num(); ++PoolIndex)
	{
		FPoolEntry& PoolEntry = Pools[PoolIndex];
		const uint32 BatchCount = PoolEntry.BatchCount;
		if (BatchCount == 0u)
		{
			continue;
		}

		const FPixelFormatInfo& FormatInfo = GPixelFormats[PoolEntry.Format];
		const uint32 TileSize = PoolEntry.TileSize;
		const uint32 TileWidthInBlocks = FMath::DivideAndRoundUp(TileSize, (uint32)FormatInfo.BlockSizeX);
		const uint32 TileHeightInBlocks = FMath::DivideAndRoundUp(TileSize, (uint32)FormatInfo.BlockSizeY);

		const uint32 TextureIndex = PoolEntry.BatchTextureIndex;
		PoolEntry.BatchTextureIndex = (PoolEntry.BatchTextureIndex + 1u) % NUM_STAGING_TEXTURES;
		FStagingTexture& StagingTexture = PoolEntry.StagingTexture[TextureIndex];

		if (BatchCount > StagingTexture.BatchCapacity)
		{
			const uint32 MaxSizeInTiles = FMath::DivideAndRoundDown(GetMax2DTextureDimension(), TileSize);
			const uint32 MaxCapacity = MaxSizeInTiles * MaxSizeInTiles;
			check(BatchCount <= MaxCapacity);

			// Try to create roughly square staging texture
			// Stacking tiles on top of each other is potentially more cache efficient, since 'stride' will be smaller
			// However, we're typically creating this texture with a tile size of 136, which on most GPUs will round up to next multiple of 32 (8x8 tiles of 4x4 BC compressed blocks) internally
			// This means we'll waste less memory overall if width is larger
			// Also, if we only stack vertically, we run into GPU limit of 16k texture dimension for large upload buffers
			const uint32 NewCapacity = FMath::Clamp(BatchCount * 3u / 2u, 64u, MaxCapacity);
			const uint32 WidthInTiles = FMath::FloorToInt(FMath::Sqrt((float)NewCapacity));
			check(WidthInTiles > 0u);
			const uint32 HeightInTiles = (NewCapacity + WidthInTiles - 1u) / WidthInTiles;

			if (StagingTexture.RHITexture)
			{
				DEC_MEMORY_STAT_BY(STAT_TotalGPUUploadSize, CalcTextureSize(StagingTexture.RHITexture->GetSizeX(), StagingTexture.RHITexture->GetSizeY(), PoolEntry.Format, 1u));
			}

			FRHIResourceCreateInfo CreateInfo;
			StagingTexture.RHITexture = RHICmdList.CreateTexture2D(TileSize * WidthInTiles, TileSize * HeightInTiles, PoolEntry.Format, 1, 1, TexCreate_CPUWritable, CreateInfo);
			StagingTexture.WidthInTiles = WidthInTiles;
			StagingTexture.BatchCapacity = WidthInTiles * HeightInTiles;
			INC_MEMORY_STAT_BY(STAT_TotalGPUUploadSize, CalcTextureSize(TileSize * WidthInTiles, TileSize * HeightInTiles, PoolEntry.Format, 1u));
		}

		uint32 BatchStride = 0u;
		void* BatchMemory = RHICmdList.LockTexture2D(StagingTexture.RHITexture, 0, RLM_WriteOnly, BatchStride, false, false);

		// copy all tiles to the staging texture
		const int32 SubmitListHead = PoolEntry.SubmitTileListHead;
		int32 Index = Tiles[SubmitListHead].NextIndex;
		while (Index != SubmitListHead)
		{
			const FTileEntry& Entry = Tiles[Index];
			const int32 NextIndex = Entry.NextIndex;
			const uint32_t SrcTileX = Entry.SubmitBatchIndex % StagingTexture.WidthInTiles;
			const uint32_t SrcTileY = Entry.SubmitBatchIndex / StagingTexture.WidthInTiles;

			uint8* BatchDst = (uint8*)BatchMemory + TileHeightInBlocks * SrcTileY * BatchStride + TileWidthInBlocks * SrcTileX * FormatInfo.BlockBytes;
			for (uint32 y = 0u; y < TileHeightInBlocks; ++y)
			{
				FMemory::Memcpy(
					BatchDst + y * BatchStride,
					(uint8*)Entry.Memory + y * Entry.Stride,
					TileWidthInBlocks * FormatInfo.BlockBytes);
			}

			Index = NextIndex;
		}

		RHICmdList.UnlockTexture2D(StagingTexture.RHITexture, 0u, false, false);

		// upload each tile from staging texture to physical texture
		Index = Tiles[SubmitListHead].NextIndex;
		while (Index != SubmitListHead)
		{
			FTileEntry& Entry = Tiles[Index];
			const int32 NextIndex = Entry.NextIndex;
			const uint32_t SrcTileX = Entry.SubmitBatchIndex % StagingTexture.WidthInTiles;
			const uint32_t SrcTileY = Entry.SubmitBatchIndex / StagingTexture.WidthInTiles;

			const uint32 SkipBorderSize = Entry.SubmitSkipBorderSize;
			const uint32 SubmitTileSize = TileSize - SkipBorderSize * 2;
			const FVector2D SourceBoxStart(SrcTileX * TileSize + SkipBorderSize, SrcTileY * TileSize + SkipBorderSize);
			const FVector2D DestinationBoxStart(Entry.SubmitDestX * SubmitTileSize, Entry.SubmitDestY * SubmitTileSize);
			const FBox2D SourceBox(SourceBoxStart, SourceBoxStart + FVector2D(SubmitTileSize, SubmitTileSize));
			const FBox2D DestinationBox(DestinationBoxStart, DestinationBoxStart + FVector2D(SubmitTileSize, SubmitTileSize));

			RHICmdList.CopySubTextureRegion(StagingTexture.RHITexture, Entry.RHISubmitTexture, SourceBox, DestinationBox);
			Entry.RHISubmitTexture = nullptr;
			Entry.SubmitBatchIndex = 0u;
			Entry.SubmitDestX = 0;
			Entry.SubmitDestY = 0;
			Entry.SubmitSkipBorderSize = 0;

			RemoveFromList(Index);
			AddToList(PoolEntry.FreeTileListHead, Index);
			Index = NextIndex;
		}

		PoolEntry.BatchCount = 0u;
	}
}

FVTUploadTileHandle FVirtualTextureUploadCache::PrepareTileForUpload(FVTUploadTileBuffer& OutBuffer, EPixelFormat InFormat, uint32 InTileSize)
{
	SCOPE_CYCLE_COUNTER(STAT_VTP_StageTile)

	checkSlow(IsInRenderingThread());

	const int32 PoolIndex = GetOrCreatePoolIndex(InFormat, InTileSize);
	const FPoolEntry& PoolEntry = Pools[PoolIndex];

	int32 Index = Tiles[PoolEntry.FreeTileListHead].NextIndex;
	if (Index == PoolEntry.FreeTileListHead)
	{
		Index = CreateTileEntry(PoolIndex);
		FTileEntry& NewEntry = Tiles[Index];

		const FPixelFormatInfo& FormatInfo = GPixelFormats[InFormat];
		const uint32 TileWidthInBlocks = FMath::DivideAndRoundUp(InTileSize, (uint32)FormatInfo.BlockSizeX);
		const uint32 TileHeightInBlocks = FMath::DivideAndRoundUp(InTileSize, (uint32)FormatInfo.BlockSizeY);
		const uint32 Stride = TileWidthInBlocks * FormatInfo.BlockBytes;
		const uint32 MemorySize = Stride * TileHeightInBlocks;

		// We support several different methods for staging tile data to GPU textures
		// On some platforms, CPU can write linear texture data to persist mapped buffer, then this can be uploaded directly to GPU...this is fastest method
		// Otherwise, CPU writes texture data to temp buffer, then this is copied to GPU via a batched staging texture...this involves more copying, but is best method under default D3D11
		// Can potentially write each tile to a separate staging texture, but this has too much lock/unlock overhead
		NewEntry.Stride = Stride;
		if (ALLOW_COPY_FROM_BUFFER)
		{
			FRHIResourceCreateInfo CreateInfo;
			NewEntry.RHIStagingBuffer = RHICreateStructuredBuffer(FormatInfo.BlockBytes, MemorySize, BUF_ShaderResource | BUF_Static, CreateInfo);
			NewEntry.Memory = RHILockStructuredBuffer(NewEntry.RHIStagingBuffer, 0u, MemorySize, RLM_WriteOnly);
			INC_MEMORY_STAT_BY(STAT_TotalGPUUploadSize, MemorySize);
		}
		else
		{
			NewEntry.Memory = FMemory::Malloc(MemorySize);
			INC_MEMORY_STAT_BY(STAT_TotalCPUUploadSize, MemorySize);
		}
		INC_DWORD_STAT(STAT_NumUploadEntries);
	}
	else
	{
		RemoveFromList(Index);
	}

	FTileEntry& Entry = Tiles[Index];
	++NumPendingTiles;
	
	OutBuffer.Memory = Entry.Memory;
	OutBuffer.Stride = Entry.Stride;
	return FVTUploadTileHandle(Index);
}

void FVirtualTextureUploadCache::SubmitTile(FRHICommandListImmediate& RHICmdList, const FVTUploadTileHandle& InHandle, FTexture2DRHIParamRef InDestTexture, int InDestX, int InDestY, int InSkipBorderSize)
{
	checkSlow(IsInRenderingThread());
	check(NumPendingTiles > 0u);
	--NumPendingTiles;

	const int32 Index = InHandle.Index;
	FTileEntry& Entry = Tiles[Index];
	Entry.FrameSubmitted = GFrameNumberRenderThread;

	FPoolEntry& PoolEntry = Pools[Entry.PoolIndex];
	const uint32 TileSize = PoolEntry.TileSize - InSkipBorderSize * 2;

	if (Entry.RHIStagingBuffer)
	{
		const FUpdateTextureRegion2D UpdateRegion(InDestX * TileSize, InDestY * TileSize, InSkipBorderSize, InSkipBorderSize, TileSize, TileSize);
		RHICmdList.UpdateFromBufferTexture2D(InDestTexture, 0u, UpdateRegion, Entry.Stride, Entry.RHIStagingBuffer, 0u);

		// move to pending list, so we won't re-use this buffer until the GPU has finished the copy
		// (we're using persist mapped buffer here, so this is the only synchronization method in place...without this delay we'd get corrupt textures)
		AddToList(LIST_SUBMITTED, Index);
	}
	else if(ALLOW_UPDATE_TEXTURE)
	{
		const FUpdateTextureRegion2D UpdateRegion(InDestX * TileSize, InDestY * TileSize, InSkipBorderSize, InSkipBorderSize, TileSize, TileSize);
		RHICmdList.UpdateTexture2D(InDestTexture, 0u, UpdateRegion, Entry.Stride, (uint8*)Entry.Memory);

		// UpdateTexture2D makes internal copy of data, no need to wait before re-using tile
		AddToList(PoolEntry.FreeTileListHead, Index);
	}
	else
	{
		Entry.RHISubmitTexture = InDestTexture;
		Entry.SubmitDestX = InDestX;
		Entry.SubmitDestY = InDestY;
		Entry.SubmitSkipBorderSize = InSkipBorderSize;
		Entry.SubmitBatchIndex = PoolEntry.BatchCount++;

		// move to list of batched updates for the current pool
		AddToList(PoolEntry.SubmitTileListHead, Index);
	}
}

void FVirtualTextureUploadCache::CancelTile(const FVTUploadTileHandle& InHandle)
{
	checkSlow(IsInRenderingThread());
	check(NumPendingTiles > 0u);
	--NumPendingTiles;

	const int32 Index = InHandle.Index;
	FTileEntry& Entry = Tiles[Index];
	FPoolEntry& PoolEntry = Pools[Entry.PoolIndex];

	AddToList(PoolEntry.FreeTileListHead, Index);
}

void FVirtualTextureUploadCache::UpdateFreeList()
{
	check(IsInRenderingThread());
	const uint32 CurrentFrame = GFrameNumberRenderThread;

	int32 Index = Tiles[LIST_SUBMITTED].NextIndex;
	while (Index != LIST_SUBMITTED)
	{
		FTileEntry& Entry = Tiles[Index];
		const int32 NextIndex = Entry.NextIndex;

		check(CurrentFrame >= Entry.FrameSubmitted);
		const uint32 FramesSinceSubmitted = CurrentFrame - Entry.FrameSubmitted;
		if (FramesSinceSubmitted < 2u)
		{
			break;
		}

		const FPoolEntry& PoolEntry = Pools[Entry.PoolIndex];
		RemoveFromList(Index);
		AddToList(PoolEntry.FreeTileListHead, Index);

		Index = NextIndex;
	}
}
