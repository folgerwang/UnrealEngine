// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DeferredShadingRenderer.h: Scene rendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "RendererInterface.h"

#include "VirtualTexturing.h"

enum class ERuntimeVirtualTextureMaterialType;
class FRHITexture2D;
class FSceneInterface;
class FVirtualTexturePhysicalSpace;

/** IVirtualTextureFinalizer implementation that renders the virtual texture pages on demand. */
class FMeshTileVirtualTextureFinalizer : public IVirtualTextureFinalizer
{
public:
	FMeshTileVirtualTextureFinalizer(FVTProducerDescription const& InDesc, FSceneInterface* InScene, FTransform const& InUVToWorld);
	virtual ~FMeshTileVirtualTextureFinalizer() {}

	/** A description for a single tile to render. */
	struct FTileLayer
	{
		FRHITexture2D* Texture = nullptr;
		int32 DestX = 0;
		int32 DestY = 0;
	};
	struct FTileEntry
	{
		FTileLayer Layers[4];
		uint32 vAddress = 0;
		uint8 vLevel = 0;
	};

	/** Returns false if we don't yet have everything we need to render a VT page. */
	bool IsReady();

	/** Add a tile to the finalize queue. */
	void AddTile(FTileEntry& Tile);

	const FVTProducerDescription& GetDesc() { return Desc;  }

	//~ Begin IVirtualTextureFinalizer Interface.
	virtual void Finalize(FRHICommandListImmediate& RHICmdList) override;
	//~ End IVirtualTextureFinalizer Interface.

private:
	/** Description of our virtual texture. */
	const FVTProducerDescription Desc;
	/** Contents of virtual texture layer stack. */
	FSceneInterface* Scene;
	/** Transform from UV space to world space. */
	FTransform UVToWorld;
	/** Array of tiles in the queue to finalize. */
	TArray<FTileEntry> Tiles;
};

/** IVirtualTexture implementation that is handling runtime rendered page data requests. */
class FMeshTileVirtualTextureProducer : public IVirtualTexture
{
public:
	RENDERER_API FMeshTileVirtualTextureProducer(FVTProducerDescription const& InDesc, FSceneInterface* InScene, FTransform const& InUVToWorld);
	RENDERER_API virtual ~FMeshTileVirtualTextureProducer() {}

	//~ Begin IVirtualTexture Interface.
	virtual FVTRequestPageResult RequestPageData(
		const FVirtualTextureProducerHandle& ProducerHandle,
		uint8 LayerMask,
		uint8 vLevel,
		uint32 vAddress,
		EVTRequestPagePriority Priority
	) override;

	virtual IVirtualTextureFinalizer* ProducePageData(
		FRHICommandListImmediate& RHICmdList,
		ERHIFeatureLevel::Type FeatureLevel,
		EVTProducePageFlags Flags,
		const FVirtualTextureProducerHandle& ProducerHandle,
		uint8 LayerMask,
		uint8 vLevel,
		uint32 vAddress,
		uint64 RequestHandle,
		const FVTProduceTargetLayer* TargetLayers
	) override;
	//~ End IVirtualTexture Interface.

private:
	FMeshTileVirtualTextureFinalizer Finalizer;
};

struct FMeshTileVTDescription
{
	int32 TilePositionX;
	int32 TilePositionY;
	uint32 TileSizeX;
	uint32 TileSizeY;
};

inline bool operator==(const FMeshTileVTDescription& Lhs, const FMeshTileVTDescription& Rhs)
{
	if (Lhs.TilePositionX != Rhs.TilePositionX ||
		Lhs.TilePositionY != Rhs.TilePositionY ||
		Lhs.TileSizeX != Rhs.TileSizeX ||
		Lhs.TileSizeY != Rhs.TileSizeY)
	{
		return false;
	}

	return true;
}

inline bool operator!=(const FMeshTileVTDescription& Lhs, const FMeshTileVTDescription& Rhs)
{
	return !operator==(Lhs, Rhs);
}

extern uint32 GetTypeHash(const FMeshTileVTDescription& Description);

class MeshTileVTInfo
{

};

class MeshTileVirtualTextureManager
{
public:
	MeshTileVirtualTextureManager();

	void UpdateMeshTilesVT(FRHICommandListImmediate& RHICmdList, UZooxCameraCaptureComponent* CaptureComponent = nullptr);

	MeshTileVTInfo* RegisterMeshTileVT(const FMeshTileVTDescription* desc);
	void UnregisterMeshTileVT(const FMeshTileVTDescription* desc);

	const int kNumLayers = 3;
private:
	TMap<FMeshTileVTDescription, MeshTileVTInfo*> MeshTileVTs;
};
