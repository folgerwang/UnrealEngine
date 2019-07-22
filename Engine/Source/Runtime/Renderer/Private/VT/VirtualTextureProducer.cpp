// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureProducer.h"
#include "VirtualTextureSystem.h"
#include "VirtualTexturePhysicalSpace.h"

void FVirtualTextureProducer::Release(FVirtualTextureSystem* System, const FVirtualTextureProducerHandle& HandleToSelf)
{
	if (Description.bPersistentHighestMip)
	{
		const uint32 RootWidthInTiles = FMath::Max(Description.WidthInTiles >> Description.MaxLevel, 1u);
		const uint32 RootHeightInTiles = FMath::Max(Description.HeightInTiles >> Description.MaxLevel, 1u);
		for (uint32 TileY = 0u; TileY < RootHeightInTiles; ++TileY)
		{
			for (uint32 TileX = 0u; TileX < RootWidthInTiles; ++TileX)
			{
				const uint32 Local_vAddress = FMath::MortonCode2(TileX) | (FMath::MortonCode2(TileY) << 1);
				const FVirtualTextureLocalTile TileToUnlock(HandleToSelf, Local_vAddress, Description.MaxLevel);
				System->UnlockTile(TileToUnlock);
			}
		}
	}

	for (uint32 LayerIndex = 0u; LayerIndex < Description.NumLayers; ++LayerIndex)
	{
		FVirtualTexturePhysicalSpace* Space = PhysicalSpace[LayerIndex];
		PhysicalSpace[LayerIndex] = nullptr;
		Space->GetPagePool().EvictPages(System, HandleToSelf);
		System->ReleasePhysicalSpace(Space);
	}

	delete VirtualTexture;
	VirtualTexture = nullptr;
	Description = FVTProducerDescription();
}

FVirtualTextureProducerCollection::FVirtualTextureProducerCollection()
{
	Producers.AddDefaulted(1);
}

FVirtualTextureProducerHandle FVirtualTextureProducerCollection::RegisterProducer(FVirtualTextureSystem* System, const FVTProducerDescription& InDesc, IVirtualTexture* InProducer)
{
	check(IsInRenderingThread());
	check(InDesc.MaxLevel <= FMath::CeilLogTwo(FMath::Max(InDesc.WidthInTiles, InDesc.HeightInTiles)));
	check(InProducer);

	const uint32 Index = AcquireEntry();
	FProducerEntry& Entry = Producers[Index];
	Entry.Producer.Description = InDesc;
	Entry.Producer.VirtualTexture = InProducer;

	check(InDesc.NumLayers <= VIRTUALTEXTURE_SPACE_MAXLAYERS);
	for (uint32 LayerIndex = 0u; LayerIndex < InDesc.NumLayers; ++LayerIndex)
	{
		FVTPhysicalSpaceDescription PhysicalSpaceDesc;
		PhysicalSpaceDesc.Dimensions = InDesc.Dimensions;
		PhysicalSpaceDesc.TileSize = InDesc.TileSize + InDesc.TileBorderSize * 2u;
		PhysicalSpaceDesc.Format = InDesc.LayerFormat[LayerIndex];
		PhysicalSpaceDesc.bContinuousUpdate = InDesc.bContinuousUpdate;
		PhysicalSpaceDesc.bCreateRenderTarget = InDesc.bCreateRenderTarget;
		PhysicalSpaceDesc.bZooxMeshTileVT = InDesc.bZooxMeshTileVT;
		PhysicalSpaceDesc.ZooxMeshTileVTLayerIndex = InDesc.bZooxMeshTileVT ? LayerIndex : 0;
		Entry.Producer.PhysicalSpace[LayerIndex] = System->AcquirePhysicalSpace(PhysicalSpaceDesc);
	}

	const FVirtualTextureProducerHandle Handle(Index, Entry.Magic);

	if (InDesc.bPersistentHighestMip)
	{
		const uint32 MaxLevel = InDesc.MaxLevel;
		const uint32 RootWidthInTiles = FMath::Max(InDesc.WidthInTiles >> MaxLevel, 1u);
		const uint32 RootHeightInTiles = FMath::Max(InDesc.HeightInTiles >> MaxLevel, 1u);
		for (uint32 TileY = 0u; TileY < RootHeightInTiles; ++TileY)
		{
			for (uint32 TileX = 0u; TileX < RootWidthInTiles; ++TileX)
			{
				const uint32 Local_vAddress = FMath::MortonCode2(TileX) | (FMath::MortonCode2(TileY) << 1);
				const FVirtualTextureLocalTile TileToLock(Handle, Local_vAddress, MaxLevel);
				System->LockTile(TileToLock);
			}
		}
	}

	return Handle;
}

void FVirtualTextureProducerCollection::ReleaseProducer(FVirtualTextureSystem* System, const FVirtualTextureProducerHandle& Handle)
{
	check(IsInRenderingThread());

	if (FProducerEntry* Entry = GetEntry(Handle))
	{
		Entry->Producer.Release(System, Handle);
		Entry->Magic = (Entry->Magic + 1u) & 1023u;
		ReleaseEntry(Handle.Index);
	}
}

FVirtualTextureProducer* FVirtualTextureProducerCollection::FindProducer(const FVirtualTextureProducerHandle& Handle)
{
	FProducerEntry* Entry = GetEntry(Handle);
	return Entry ? &Entry->Producer : nullptr;
}

FVirtualTextureProducer& FVirtualTextureProducerCollection::GetProducer(const FVirtualTextureProducerHandle& Handle)
{
	const uint32 Index = Handle.Index;
	check(Index < (uint32)Producers.Num());
	FProducerEntry& Entry = Producers[Index];
	check(Entry.Magic == Handle.Magic);
	return Entry.Producer;
}

FVirtualTextureProducerCollection::FProducerEntry* FVirtualTextureProducerCollection::GetEntry(const FVirtualTextureProducerHandle& Handle)
{
	const uint32 Index = Handle.Index;
	if (Index < (uint32)Producers.Num())
	{
		FProducerEntry& Entry = Producers[Index];
		if (Entry.Magic == Handle.Magic)
		{
			return &Entry;
		}
	}
	return nullptr;
}
