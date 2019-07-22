// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VirtualTextureShared.h"
#include "VirtualTexturePhysicalSpace.h"
#include "TexturePageMap.h"
#include "VirtualTextureAllocator.h"
#include "RendererInterface.h"
#include "VirtualTexturing.h"

class FAllocatedVirtualTexture;
class FVirtualTextureSystem;

struct FVTSpaceDescription
{
	uint32 TileSize = 0u;
	uint32 TileBorderSize = 0u;
	EVTPageTableFormat Format = EVTPageTableFormat::UInt16;
	uint8 Dimensions = 0u;
	uint8 NumLayers = 0u;
	uint8 bPrivateSpace : 1;
};

inline bool operator==(const FVTSpaceDescription& Lhs, const FVTSpaceDescription& Rhs)
{
	return Lhs.Dimensions == Rhs.Dimensions &&
		Lhs.TileSize == Rhs.TileSize &&
		Lhs.TileBorderSize == Rhs.TileBorderSize &&
		Lhs.NumLayers == Rhs.NumLayers &&
		Lhs.Format == Rhs.Format &&
		Lhs.bPrivateSpace == Rhs.bPrivateSpace;
}
inline bool operator!=(const FVTSpaceDescription& Lhs, const FVTSpaceDescription& Rhs)
{
	return !operator==(Lhs, Rhs);
}

// Virtual memory address space mapped by a page table texture
class FVirtualTextureSpace final : public FRenderResource
{
public:
	static const uint32 LayersPerPageTableTexture = IAllocatedVirtualTexture::LayersPerPageTableTexture;

	FVirtualTextureSpace(FVirtualTextureSystem* InSystem, uint8 InID, const FVTSpaceDescription& InDesc, uint32 InSizeNeeded);
	virtual ~FVirtualTextureSpace();

	inline const FVTSpaceDescription& GetDescription() const { return Description; }
	inline uint32 GetPageTableSize() const { return PageTableSize; }
	inline uint32 GetNumLayers() const { return Description.NumLayers; }
	inline uint8 GetDimensions() const { return Description.Dimensions; }
	inline uint32 GetNumPageTableTextures() const { return (Description.NumLayers + LayersPerPageTableTexture - 1u) / LayersPerPageTableTexture; }
	inline uint8 GetID() const { return ID; }

	inline uint32 GetNumPageTableLevels() const { return NumPageTableLevels; }
	inline FVirtualTextureAllocator& GetAllocator() { return Allocator; }
	inline const FVirtualTextureAllocator& GetAllocator() const { return Allocator; }
	inline FTexturePageMap& GetPageMap(uint32 LayerIndex) { check(LayerIndex < Description.NumLayers); return PhysicalPageMap[LayerIndex]; }
	inline const FTexturePageMap& GetPageMap(uint32 LayerIndex) const { check(LayerIndex < Description.NumLayers); return PhysicalPageMap[LayerIndex]; }

	// FRenderResource interface
	virtual void		InitRHI() override;
	virtual void		ReleaseRHI() override;

	inline uint32 AddRef() { return ++NumRefs; }
	inline uint32 Release() { check(NumRefs > 0u); return --NumRefs; }
	inline uint32 GetRefCount() const { return NumRefs; }

	uint32 GetSizeInBytes() const;

	uint32 AllocateVirtualTexture(FAllocatedVirtualTexture* VirtualTexture);

	void FreeVirtualTexture(FAllocatedVirtualTexture* VirtualTexture);

	FRHITextureReference* GetPageTableTexture(uint32 PageTableIndex) const
	{
		check(PageTableIndex < GetNumPageTableTextures());
		return PageTable[PageTableIndex].TextureReferenceRHI.GetReference();
	}

	void				QueueUpdate( uint8 Layer, uint8 vLogSize, uint32 vAddress, uint8 vLevel, const FPhysicalTileLocation& pTileLocation);
	void				AllocateTextures(FRHICommandList& RHICmdList);
	void				ApplyUpdates(FVirtualTextureSystem* System, FRHICommandList& RHICmdList);
	void				QueueUpdateEntirePageTable();

	void DumpToConsole(bool verbose);

private:
	static const uint32 TextureCapacity = (VIRTUALTEXTURE_SPACE_MAXLAYERS + LayersPerPageTableTexture - 1u) / LayersPerPageTableTexture;

	struct FTextureEntry
	{
		TRefCountPtr<IPooledRenderTarget> RenderTarget;
		FTextureReferenceRHIRef TextureReferenceRHI;
	};

	FVTSpaceDescription Description;
	
	FVirtualTextureAllocator Allocator;
	FTexturePageMap PhysicalPageMap[VIRTUALTEXTURE_SPACE_MAXLAYERS];

	FTextureEntry PageTable[TextureCapacity];
	TEnumAsByte<EPixelFormat> TexturePixelFormat[TextureCapacity];
	
	TArray<FPageTableUpdate> PageTableUpdates[VIRTUALTEXTURE_SPACE_MAXLAYERS];

	FVertexBufferRHIRef UpdateBuffer;
	FShaderResourceViewRHIRef UpdateBufferSRV;

	uint32 PageTableSize;
	uint32 NumPageTableLevels;
	uint32 NumRefs;

	uint8 ID;
	bool bNeedToAllocatePageTable;
	bool bForceEntireUpdate;
};
