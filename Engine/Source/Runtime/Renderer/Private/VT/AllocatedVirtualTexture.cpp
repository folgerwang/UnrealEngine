// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AllocatedVirtualTexture.h"
#include "VirtualTextureSystem.h"
#include "VirtualTextureSpace.h"
#include "VirtualTexturePhysicalSpace.h"

FAllocatedVirtualTexture::FAllocatedVirtualTexture(uint32 InFrame,
	const FAllocatedVTDescription& InDesc,
	FVirtualTextureSpace* InSpace,
	FVirtualTextureProducer* const* InProducers,
	uint32 InWidthInTiles,
	uint32 InHeightInTiles,
	uint32 InDepthInTiles)
	: IAllocatedVirtualTexture(InDesc, InSpace->GetID(), InSpace->GetDescription().Format, InWidthInTiles, InHeightInTiles, InDepthInTiles)
	, Space(InSpace)
	, RefCount(1)
	, FrameAllocated(InFrame)
	, NumUniqueProducers(0u)
{
	check(IsInRenderingThread());
	FMemory::Memzero(PhysicalSpace);
	FMemory::Memzero(UniqueProducerHandles);
	FMemory::Memzero(UniqueProducerMipBias);

	for (uint32 LayerIndex = 0u; LayerIndex < Description.NumLayers; ++LayerIndex)
	{
		FVirtualTextureProducer* Producer = InProducers[LayerIndex];
		if (Producer)
		{
			PhysicalSpace[LayerIndex] = Producer->GetPhysicalSpace(InDesc.LocalLayerToProduce[LayerIndex]);
			UniqueProducerIndexForLayer[LayerIndex] = AddUniqueProducer(InDesc.ProducerHandle[LayerIndex], Producer);
		}
		else
		{
			UniqueProducerIndexForLayer[LayerIndex] = 0xff;
		}
	}

	// must have at least 1 valid layer/producer
	check(NumUniqueProducers > 0u);

	// make sure max level calculation is valid (don't end up with more mips than supported by size)
	check(MaxLevel <= FMath::CeilLogTwo(FMath::Max(InWidthInTiles, InHeightInTiles)));

	VirtualAddress = Space->AllocateVirtualTexture(this);
}

FAllocatedVirtualTexture::~FAllocatedVirtualTexture()
{
}

void FAllocatedVirtualTexture::Destroy(FVirtualTextureSystem* System)
{
	const int32 NewRefCount = RefCount.Decrement();
	check(NewRefCount >= 0);
	if (NewRefCount == 0)
	{
		System->ReleaseVirtualTexture(this);
	}
}

void FAllocatedVirtualTexture::Release(FVirtualTextureSystem* System)
{
	check(IsInRenderingThread());
	check(RefCount.GetValue() == 0);

	for (uint32 LayerIndex = 0u; LayerIndex < Description.NumLayers; ++LayerIndex)
	{
		// Physical pool needs to evict all pages that belong to this VT's space
		// TODO - could improve this to only evict pages belonging to this VT
		if (PhysicalSpace[LayerIndex])
		{
			FTexturePageMap& PageMap = Space->GetPageMap(LayerIndex);
			PhysicalSpace[LayerIndex]->GetPagePool().UnmapAllPagesForSpace(System, Space->GetID());
			PageMap.VerifyPhysicalSpaceUnmapped(PhysicalSpace[LayerIndex]->GetID());
		}
	}

	Space->FreeVirtualTexture(this);
	System->RemoveAllocatedVT(this);
	System->ReleaseSpace(Space);

	delete this;
}

uint32 FAllocatedVirtualTexture::AddUniqueProducer(const FVirtualTextureProducerHandle& InHandle, FVirtualTextureProducer* InProducer)
{
	for (uint32 Index = 0u; Index < NumUniqueProducers; ++Index)
	{
		if (UniqueProducerHandles[Index] == InHandle)
		{
			return Index;
		}
	}
	const uint32 Index = NumUniqueProducers++;
	check(Index < VIRTUALTEXTURE_SPACE_MAXLAYERS);
	
	const FVTProducerDescription& ProducerDesc = InProducer->GetDescription();

	// maybe these values should just be set by producers, rather than also set on AllocatedVT desc
	check(ProducerDesc.Dimensions == Description.Dimensions);
	check(ProducerDesc.TileSize == Description.TileSize);
	check(ProducerDesc.TileBorderSize == Description.TileBorderSize);

	const uint32 SizeInTiles = FMath::Max(WidthInTiles, HeightInTiles);
	const uint32 ProducerSizeInTiles = FMath::Max(ProducerDesc.WidthInTiles, ProducerDesc.HeightInTiles);
	const uint32 MipBias = FMath::CeilLogTwo(SizeInTiles / ProducerSizeInTiles);

	check((SizeInTiles / ProducerSizeInTiles) * ProducerSizeInTiles == SizeInTiles);
	check(ProducerDesc.WidthInTiles << MipBias == WidthInTiles);
	check(ProducerDesc.HeightInTiles << MipBias == HeightInTiles);

	MaxLevel = FMath::Max<uint32>(MaxLevel, ProducerDesc.MaxLevel + MipBias);

	UniqueProducerHandles[Index] = InHandle;
	UniqueProducerMipBias[Index] = MipBias;
	
	return Index;
}

FRHITexture* FAllocatedVirtualTexture::GetPageTableTexture(uint32 InPageTableIndex) const
{
	return Space->GetPageTableTexture(InPageTableIndex);
}

FRHITexture* FAllocatedVirtualTexture::GetPhysicalTexture(uint32 InLayerIndex) const
{
	if (InLayerIndex < Description.NumLayers)
	{
		const FVirtualTexturePhysicalSpace* LayerSpace = PhysicalSpace[InLayerIndex];
		return LayerSpace ? LayerSpace->GetPhysicalTexture() : nullptr;
	}
	return nullptr;
}

FRHIShaderResourceView* FAllocatedVirtualTexture::GetPhysicalTextureView(uint32 InLayerIndex, bool bSRGB) const
{
	if (InLayerIndex < Description.NumLayers)
	{
		const FVirtualTexturePhysicalSpace* LayerSpace = PhysicalSpace[InLayerIndex];
		return LayerSpace ? LayerSpace->GetPhysicalTextureView(bSRGB) : nullptr;
	}
	return nullptr;
}

uint32 FAllocatedVirtualTexture::GetPhysicalTextureSize(uint32 InLayerIndex) const
{
	if (InLayerIndex < Description.NumLayers)
	{
		const FVirtualTexturePhysicalSpace* LayerSpace = PhysicalSpace[InLayerIndex];
		return LayerSpace ? LayerSpace->GetTextureSize() : 0u;
	}
	return 0u;
}
