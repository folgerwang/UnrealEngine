// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DeferredShadingRenderer.cpp: Top level rendering loop for deferred shading
=============================================================================*/

#include "MeshTileVirtualTexture.h"
#include "RendererModule.h"
#include "EngineModule.h"
#include "VT/VirtualTextureSystem.h"
#include "Components/ZooxCameraCaptureComponent.h"
#include "RuntimeVirtualTextureProducer.h"
#include "RendererInterface.h"
#include "ScenePrivate.h"

DECLARE_MEMORY_STAT_POOL(TEXT("Total Physical Memory"), STAT_TotalPhysicalMemory, STATGROUP_VirtualTextureMemory, FPlatformMemory::MCR_GPU);
DECLARE_MEMORY_STAT_POOL(TEXT("Total Pagetable Memory"), STAT_TotalPagetableMemory, STATGROUP_VirtualTextureMemory, FPlatformMemory::MCR_GPU);

//MeshTileVirtualTextureManager gMeshTIleVirtualManager;

FMeshTileVirtualTextureFinalizer::FMeshTileVirtualTextureFinalizer(FVTProducerDescription const& InDesc, FSceneInterface* InScene, FTransform const& InUVToWorld)
	: Desc(InDesc)
	, Scene(InScene)
	, UVToWorld(InUVToWorld)
{
}

bool FMeshTileVirtualTextureFinalizer::IsReady()
{
	//todo[vt]: 
	// Test if we have everything we need to render (shaders loaded etc).
	// Current test for GPUScene.PrimitiveBuffer is a nasty thing to prevent a checkf triggering if no PrimitiveBuffer is bound. It feels like it requires too much knowledge of the renderer internals...
	return Scene != nullptr && Scene->GetRenderScene() != nullptr && Scene->GetRenderScene()->GPUScene.PrimitiveBuffer.Buffer != nullptr;
}

void FMeshTileVirtualTextureFinalizer::AddTile(FTileEntry& Tile)
{
	Tiles.Add(Tile);
}

struct FStagingTexture
{
	TRefCountPtr<FRHITexture2D> RHITexture;
	uint32_t WidthInTiles = 0u;
	uint32_t BatchCapacity = 0u;
};

void FMeshTileVirtualTextureFinalizer::Finalize(FRHICommandListImmediate& RHICmdList)
{
	const int32 TileSize = Desc.TileSize + 2 * Desc.TileBorderSize;
	uint32* SrcTmpBuffer = new uint32[TileSize * TileSize];

	for (auto Entry : Tiles)
	{
		const FVector2D DestinationBoxStart0(Entry.Layers[0].DestX * TileSize, Entry.Layers[0].DestY * TileSize);
		const FBox2D DestinationBox0(DestinationBoxStart0, DestinationBoxStart0 + FVector2D(TileSize, TileSize));

		const FVector2D DestinationBoxStart1(Entry.Layers[1].DestX * TileSize, Entry.Layers[1].DestY * TileSize);
		const FBox2D DestinationBox1(DestinationBoxStart1, DestinationBoxStart1 + FVector2D(TileSize, TileSize));

		const uint32 X = FMath::ReverseMortonCode2(Entry.vAddress);
		const uint32 Y = FMath::ReverseMortonCode2(Entry.vAddress >> 1);
		const uint32 DivisorX = Desc.WidthInTiles >> Entry.vLevel;
		const uint32 DivisorY = Desc.HeightInTiles >> Entry.vLevel;

		const FVector2D UV((float)X / (float)DivisorX, (float)Y / (float)DivisorY);
		const FVector2D UVSize(1.f / (float)DivisorX, 1.f / (float)DivisorY);
		const FVector2D UVBorder = UVSize * ((float)Desc.TileBorderSize / (float)Desc.TileSize);
		const FBox2D UVRange(UV - UVBorder, UV + UVSize + UVBorder);

		if (Entry.vLevel > 0)
		{
			int hit = 1;
		}

		uint32* TmpBuffer = SrcTmpBuffer;
		for (int32 y = 0; y < TileSize; y++)
		{
			for (int32 x = 0; x < TileSize; x++)
			{
				uint8 r = (UV.X + UVSize.X * x / TileSize) * 255.0f;
				uint8 g = (UV.Y + UVSize.Y * y / TileSize) * 255.0f;
				uint8 b = Entry.vLevel / 2.0f * 255.0f;
				TmpBuffer[y * TileSize + x] = (0xff << 24) | (b) | (g << 8) | (r << 16);
			}
		}

		static FStagingTexture StagingTexture;
		{
			FRHIResourceCreateInfo CreateInfo;
			StagingTexture.RHITexture = RHICmdList.CreateTexture2D(TileSize * 1, TileSize * 1, PF_B8G8R8A8, 1, 1, TexCreate_CPUWritable, CreateInfo);
			StagingTexture.WidthInTiles = 1;
			StagingTexture.BatchCapacity = 1 * 1;

			uint32 BatchStride = 0u;
			void* BatchMemory = RHICmdList.LockTexture2D(StagingTexture.RHITexture, 0, RLM_WriteOnly, BatchStride, false, false);

			uint8* BatchDst = (uint8*)BatchMemory;
			for (int y = 0; y < TileSize; y++)
			{
				FMemory::Memcpy(BatchDst, (uint8*)TmpBuffer, BatchStride);
				BatchDst += BatchStride;
				TmpBuffer += TileSize;
			}

			RHICmdList.UnlockTexture2D(StagingTexture.RHITexture, 0u, false, false);
		}

		const uint32 SkipBorderSize = 4;
		const uint32 SubmitTileSize = TileSize - SkipBorderSize * 2;
		const FVector2D SourceBoxStart(SkipBorderSize, SkipBorderSize);
		const FVector2D DestinationBoxStart(Entry.Layers[0].DestX * TileSize + SkipBorderSize, Entry.Layers[0].DestY * TileSize + SkipBorderSize);
		const FBox2D SourceBox(SourceBoxStart, SourceBoxStart + FVector2D(SubmitTileSize, SubmitTileSize));
		const FBox2D DestinationBox(DestinationBoxStart, DestinationBoxStart + FVector2D(SubmitTileSize, SubmitTileSize));

		RHICmdList.CopySubTextureRegion(StagingTexture.RHITexture, Entry.Layers[0].Texture, SourceBox, DestinationBox);
	}

	delete[]SrcTmpBuffer;
	Tiles.SetNumUnsafeInternal(0);
}

FMeshTileVirtualTextureProducer::FMeshTileVirtualTextureProducer(FVTProducerDescription const& InDesc, FSceneInterface* InScene, FTransform const& InUVToWorld)
	: Finalizer(InDesc, InScene, InUVToWorld)
{
}

FVTRequestPageResult FMeshTileVirtualTextureProducer::RequestPageData(
	const FVirtualTextureProducerHandle& ProducerHandle,
	uint8 LayerMask,
	uint8 vLevel,
	uint32 vAddress,
	EVTRequestPagePriority Priority)
{
	//todo[vt]: 
	// Investigate what causes a partial layer mask to be requested.
	// If we can't avoid it then look at ways to handle it efficiently (right now we render all layers even for partial requests).

	//todo[vt]: 
	// Possibly throttle rendering according to performance by returning Saturated here.

	FVTRequestPageResult result;
	result.Handle = 0;
	//todo[vt]:
	// Returning Saturated instead of Pending here because higher level ignores Pending for locked pages. Need to fix that...
	result.Status = Finalizer.IsReady() ? EVTRequestPageStatus::Available : EVTRequestPageStatus::Saturated;
	return result;
}

IVirtualTextureFinalizer* FMeshTileVirtualTextureProducer::ProducePageData(
	FRHICommandListImmediate& RHICmdList,
	ERHIFeatureLevel::Type FeatureLevel,
	EVTProducePageFlags Flags,
	const FVirtualTextureProducerHandle& ProducerHandle,
	uint8 LayerMask,
	uint8 vLevel,
	uint32 vAddress,
	uint64 RequestHandle,
	const FVTProduceTargetLayer* TargetLayers)
{
	FMeshTileVirtualTextureFinalizer::FTileEntry Tile;
	Tile.vAddress = vAddress;
	Tile.vLevel = vLevel;

	for (uint32 LayerIndex = 0; LayerIndex < Finalizer.GetDesc().NumLayers; LayerIndex++)
	{
		uint32 LayerSelect = 1 << LayerIndex;
		if (LayerMask & LayerSelect)
		{
			Tile.Layers[LayerIndex].Texture = TargetLayers[LayerIndex].TextureRHI->GetTexture2D();
			Tile.Layers[LayerIndex].DestX = TargetLayers[LayerIndex].pPageLocation.X;
			Tile.Layers[LayerIndex].DestY = TargetLayers[LayerIndex].pPageLocation.Y;
		}
	}

	Finalizer.AddTile(Tile);

	return &Finalizer;
}

uint32 GetTypeHash(const FMeshTileVTDescription& Description)
{
	return FCrc::MemCrc32(&Description, sizeof(Description));
}

MeshTileVirtualTextureManager::MeshTileVirtualTextureManager()
{
}

MeshTileVTInfo* MeshTileVirtualTextureManager::RegisterMeshTileVT(const FMeshTileVTDescription* desc)
{
	MeshTileVTInfo* res = nullptr;
	if (desc)
	{
		MeshTileVTs.FindOrAdd(*desc);
	}

	return res;
}

void MeshTileVirtualTextureManager::UnregisterMeshTileVT(const FMeshTileVTDescription* desc)
{
	if (desc)
	{
		MeshTileVTs.Remove(*desc);
	}
}

void GetProducerDescription(FVTProducerDescription& OutDesc, uint32 width, uint32 height, uint32 tileSize = 256, uint32 RemoveLowMips = 0, bool bCompressTextures = false)
{
	OutDesc.Name = FName("MeshTileVirtualTexture");
	OutDesc.Dimensions = 2;
	OutDesc.TileSize = tileSize;
	OutDesc.TileBorderSize = 4;
	OutDesc.WidthInTiles = width / tileSize;
	OutDesc.HeightInTiles = height / tileSize;
	OutDesc.MaxLevel = FMath::Max(FMath::CeilLogTwo(FMath::Max(OutDesc.WidthInTiles, OutDesc.HeightInTiles)) - RemoveLowMips, 1u);
	OutDesc.DepthInTiles = 1;
	OutDesc.bZooxMeshTileVT = true;

	OutDesc.NumLayers = 2;
	OutDesc.LayerFormat[0] = bCompressTextures ? PF_DXT1 : PF_B8G8R8A8;
	OutDesc.LayerFormat[1] = bCompressTextures ? PF_DXT5 : PF_B8G8R8A8;
}

void UpdateOneMeshTile(FRHICommandListImmediate& RHICmdList, FVirtualTextureProducerHandle ProducerHandle, uint32 LocalLayerMask, uint32 vAddress, uint32 vLevel)
{
	FVTProduceTargetLayer ProduceTarget[VIRTUALTEXTURE_SPACE_MAXLAYERS];
	uint32 Allocate_pAddress[VIRTUALTEXTURE_SPACE_MAXLAYERS];
	FMemory::Memset(Allocate_pAddress, 0xff);

	FVirtualTextureProducer* Producer = FVirtualTextureSystem::Get().FindProducer(ProducerHandle);
	uint32 Frame = FVirtualTextureSystem::Get().GetFrame();

	// try to allocate a page for each layer we need to load
	bool bProduceTargetValid = true;
	bool bLockTile = false;
	for (uint32 LocalLayerIndex = 0u; LocalLayerIndex < Producer->GetNumLayers(); ++LocalLayerIndex)
	{
		// If mask isn't set, we must already have a physical tile allocated for this layer, don't need to allocate another one
		if (LocalLayerMask & (1u << LocalLayerIndex))
		{
			FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer->GetPhysicalSpace(LocalLayerIndex);
			FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
			if (PagePool.AnyFreeAvailable(Frame))
			{
				const uint32 pAddress = PagePool.Alloc(&FVirtualTextureSystem::Get(), Frame, ProducerHandle, LocalLayerIndex, vAddress, vLevel, bLockTile);
				check(pAddress != ~0u);

				ProduceTarget[LocalLayerIndex].TextureRHI = PhysicalSpace->GetPhysicalTexture();
				if (PhysicalSpace->GetDescription().bCreateRenderTarget)
				{
					ProduceTarget[LocalLayerIndex].PooledRenderTarget = PhysicalSpace->GetPhysicalTexturePooledRenderTarget();
				}
				ProduceTarget[LocalLayerIndex].pPageLocation = PhysicalSpace->GetPhysicalLocation(pAddress);
				Allocate_pAddress[LocalLayerIndex] = pAddress;
			}
			else
			{
				const FPixelFormatInfo& PoolFormatInfo = GPixelFormats[PhysicalSpace->GetFormat()];
				UE_LOG(LogConsoleResponse, Display, TEXT("Failed to allocate VT page from pool PF_%s"), PoolFormatInfo.Name);
				bProduceTargetValid = false;
				break;
			}
		}
	}

	if (bProduceTargetValid)
	{
		// Successfully allocated required pages, now we can make the request
		for (uint32 LocalLayerIndex = 0u; LocalLayerIndex < Producer->GetNumLayers(); ++LocalLayerIndex)
		{
			if (LocalLayerMask & (1u << LocalLayerIndex))
			{
				// Associate the addresses we allocated with this request, so they can be mapped if required
				const uint32 pAddress = Allocate_pAddress[LocalLayerIndex];
				check(pAddress != ~0u);
				//RequestPhysicalAddress[RequestIndex * VIRTUALTEXTURE_SPACE_MAXLAYERS + LocalLayerIndex] = pAddress;
			}
			else
			{
				// Fill in pAddress for layers that are already resident
				FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer->GetPhysicalSpace(LocalLayerIndex);
				FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
				const uint32 pAddress = PagePool.FindPageAddress(ProducerHandle, LocalLayerIndex, vAddress, vLevel);
				check(pAddress != ~0u);
				ProduceTarget[LocalLayerIndex].TextureRHI = PhysicalSpace->GetPhysicalTexture();
				ProduceTarget[LocalLayerIndex].pPageLocation = PhysicalSpace->GetPhysicalLocation(pAddress);
			}
		}

		IVirtualTextureFinalizer* VTFinalizer = Producer->GetVirtualTexture()->ProducePageData(RHICmdList, ERHIFeatureLevel::SM5,
			EVTProducePageFlags::None,
			ProducerHandle, LocalLayerMask, vLevel, vAddress,
			0,//RequestPageResult.Handle,
			ProduceTarget);
		if (VTFinalizer)
		{
			FVirtualTextureSystem::Get().GetFinalizers().AddUnique(VTFinalizer); // we expect the number of unique finalizers to be very limited. if this changes, we might have to do something better then gathering them every update
		}

		//bTileLoaded = true;
		//++NumStacksProduced;
	}
	else
	{
		// Failed to allocate required physical pages for the tile, free any pages we did manage to allocate
		for (uint32 LocalLayerIndex = 0u; LocalLayerIndex < Producer->GetNumLayers(); ++LocalLayerIndex)
		{
			const uint32 pAddress = Allocate_pAddress[LocalLayerIndex];
			if (pAddress != ~0u)
			{
				FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer->GetPhysicalSpace(LocalLayerIndex);
				FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
				PagePool.Free(&FVirtualTextureSystem::Get(), pAddress);
			}
		}
	}
}

void MeshTileVirtualTextureManager::UpdateMeshTilesVT(FRHICommandListImmediate& RHICmdList, UZooxCameraCaptureComponent* CaptureComponent/* = nullptr*/)
{
	FVTProducerDescription ProducerDesc;
	GetProducerDescription(ProducerDesc, 1024, 1024);

	{
		check(IsInRenderingThread());

		IVirtualTexture* Producer = new FMeshTileVirtualTextureProducer(ProducerDesc, nullptr, FTransform());

		CaptureComponent->ProducerHandle = GetRendererModule().RegisterVirtualTextureProducer(ProducerDesc, Producer);

		if (CaptureComponent->AllocatedVT == nullptr)
		{
			FAllocatedVTDescription VTDesc;
			VTDesc.Dimensions = ProducerDesc.Dimensions;
			VTDesc.TileSize = ProducerDesc.TileSize;
			VTDesc.TileBorderSize = ProducerDesc.TileBorderSize;
			VTDesc.NumLayers = ProducerDesc.NumLayers;
			VTDesc.bPrivateSpace = true; // Dedicated page table allocation for runtime VTs

			for (uint32 LayerIndex = 0u; LayerIndex < VTDesc.NumLayers; ++LayerIndex)
			{
				VTDesc.ProducerHandle[LayerIndex] = CaptureComponent->ProducerHandle;
				VTDesc.LocalLayerToProduce[LayerIndex] = LayerIndex;
			}

			CaptureComponent->AllocatedVT = GetRendererModule().AllocateVirtualTexture(VTDesc);
		}
	}

	IAllocatedVirtualTexture* AllocatedVT = CaptureComponent->AllocatedVT;
	FRHIShaderResourceView* PhysicalView = AllocatedVT->GetPhysicalTextureView((uint32)0, false);

	static bool sUpdated = false;

	if (!sUpdated)
	{
		FVTProduceTargetLayer TargetLayers[2];

		for (uint32 l = 0; l <= ProducerDesc.MaxLevel; l++)
		{
			uint32 SizeY = FMath::Max(ProducerDesc.HeightInTiles >> l, 1u);
			uint32 SizeX = FMath::Max(ProducerDesc.WidthInTiles >> l, 1u);
			for (uint32 y = 0; y < SizeY; y++)
			{
				for (uint32 x = 0; x < SizeX; x++)
				{
					const uint32 vPageX = x;
					const uint32 vPageY = y;
					const uint32 vLevel = l;
					const uint32 vPosition = FMath::MortonCode2(vPageX) | (FMath::MortonCode2(vPageY) << 1);

					const uint32 vDimensions = 2;
					const uint32 vAddress = vPosition;// << (vLevel * vDimensions);

					uint32 LayerMask = (1 << ProducerDesc.NumLayers) - 1;
					UpdateOneMeshTile(RHICmdList, CaptureComponent->ProducerHandle, LayerMask, vAddress, vLevel);
				}
			}
		}

		sUpdated = true;
	}
}