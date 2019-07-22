// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VirtualTextureShared.h"
#include "TexturePagePool.h"
#include "RendererInterface.h"
#include "VirtualTexturing.h"

struct FVTPhysicalSpaceDescription
{
	uint32 TileSize;
	TEnumAsByte<EPixelFormat> Format;
	uint8 Dimensions;
	uint8 bContinuousUpdate : 1;
	uint8 bCreateRenderTarget : 1;
	uint8 bZooxMeshTileVT : 1;
	uint8 ZooxMeshTileVTLayerIndex : 2;
};

inline bool operator==(const FVTPhysicalSpaceDescription& Lhs, const FVTPhysicalSpaceDescription& Rhs)
{
	return Lhs.TileSize == Rhs.TileSize && 
		Lhs.Format == Rhs.Format && 
		Lhs.Dimensions == Rhs.Dimensions && 
		Lhs.bContinuousUpdate == Rhs.bContinuousUpdate && 
		Lhs.bCreateRenderTarget == Rhs.bCreateRenderTarget &&
		Lhs.bZooxMeshTileVT == Rhs.bZooxMeshTileVT &&
		Lhs.ZooxMeshTileVTLayerIndex == Rhs.ZooxMeshTileVTLayerIndex;
}

inline bool operator!=(const FVTPhysicalSpaceDescription& Lhs, const FVTPhysicalSpaceDescription& Rhs)
{
	return !operator==(Lhs, Rhs);
}

class FVirtualTexturePhysicalSpace : public FRenderResource
{
public:
	FVirtualTexturePhysicalSpace(const FVTPhysicalSpaceDescription& InDesc, uint16 InID);
	virtual ~FVirtualTexturePhysicalSpace();

	inline const FVTPhysicalSpaceDescription& GetDescription() const { return Description; }
	inline EPixelFormat GetFormat() const { return Description.Format; }
	inline uint16 GetID() const { return ID; }
	inline uint32 GetNumTiles() const { return TextureSizeInTiles * TextureSizeInTiles; }
	inline uint32 GetSizeInTiles() const { return TextureSizeInTiles; }
	inline uint32 GetTextureSize() const { return TextureSizeInTiles * Description.TileSize; }
	inline FIntVector GetPhysicalLocation(uint16 pAddress) const { return FIntVector(pAddress % TextureSizeInTiles, pAddress / TextureSizeInTiles, 0); }

	// 16bit page tables allocate 6bits to address TileX/Y, so can only address tiles from 0-63
	inline bool DoesSupport16BitPageTable() const { return false; }// TextureSizeInTiles <= 64u;

	uint32 GetSizeInBytes() const;

	inline const FTexturePagePool& GetPagePool() const { return Pool; }
	inline FTexturePagePool& GetPagePool() { return Pool; }

	// FRenderResource interface
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;

	inline uint32 AddRef() { return ++NumRefs; }
	inline uint32 Release() { check(NumRefs > 0u); return --NumRefs; }
	inline uint32 GetRefCount() const { return NumRefs; }

	FRHITexture* GetPhysicalTexture() const
	{
		check(PooledRenderTarget.IsValid());
		return PooledRenderTarget->GetRenderTargetItem().ShaderResourceTexture;
	}

	TRefCountPtr<IPooledRenderTarget> GetPhysicalTexturePooledRenderTarget() const
	{
		check(PooledRenderTarget.IsValid());
		check(Description.bCreateRenderTarget);
		return PooledRenderTarget;
	}

	FRHIShaderResourceView* GetPhysicalTextureView(bool bSRGB) const
	{
		return bSRGB ? TextureSRGBView : TextureView;
	}

#if STATS
	inline void ResetWorkingSetSize() { WorkingSetSize.Reset(); }
	inline void IncrementWorkingSetSize(int32 Amount) { WorkingSetSize.Add(Amount); }
	void UpdateWorkingSetStat();
#else // STATS
	inline void ResetWorkingSetSize() {}
	inline void IncrementWorkingSetSize(int32 Amount) {}
	inline void UpdateWorkingSetStat() {}
#endif // !STATS

protected:
	FVTPhysicalSpaceDescription Description;
	FTexturePagePool Pool;
	TRefCountPtr<IPooledRenderTarget> PooledRenderTarget;
	FShaderResourceViewRHIRef TextureView;
	FShaderResourceViewRHIRef TextureSRGBView;

	uint32 TextureSizeInTiles;
	uint32 NumRefs;
	uint16 ID;
	bool bPageTableLimit; // True if the physical size was limited by the page table format requested
	bool bGpuTextureLimit; // True if the physical size was limited by the maximum GPU texture size

#if STATS
	TStatId WorkingSetSizeStatID;
	FThreadSafeCounter WorkingSetSize;
#endif // STATS
};
