// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VirtualTextureShared.h"
#include "TexturePagePool.h"
#include "VirtualTextureAllocator.h"
#include "RendererInterface.h"
#include "VirtualTexturing.h"

// Virtual memory address space mapped by a page table texture
class FVirtualTextureSpace final : public IVirtualTextureSpace
{
public:
						FVirtualTextureSpace( const FVirtualTextureSpaceDesc& desc );
						~FVirtualTextureSpace();

	// FRenderResource interface
	virtual void		InitDynamicRHI() override;
	virtual void		ReleaseDynamicRHI() override;
	
	// IVirtualTextureSpace interface
	virtual uint64 AllocateVirtualTexture(IVirtualTexture* VirtualTexture) override
	{
		return Allocator.Alloc(VirtualTexture);
	}
	virtual void FreeVirtualTexture(IVirtualTexture* VirtualTexture) override
	{
		//Need to free all pages of the texture allocated in the pool ?!?
		//In theory we could wait until they are all evicted but as pages
		//do not keep a reference to the texture they belong to they could come
		//from an old texture that was given the same virtual in this space ID addresses
		Allocator.Free(VirtualTexture);
		
		// FIXME: This will free all pages in the space maybe make this 
		// more intelligent and free only pages belonging to this VT? 
		Pool->EvictPages(ID);
	}
	virtual uint32 GetSpaceID() const override
	{
		return ID;
	}
	virtual FRHITexture* GetPageTableTexture() const	
	{
		return PageTable->GetRenderTargetItem().ShaderResourceTexture;
	}
	
	virtual uint64 GetPhysicalAddress(uint32 vLevel, uint64 vAddr) const override
	{
		return Pool->FindPage(ID, vLevel, vAddr);
	}

	void				QueueUpdate( uint8 vLogSize, uint64 vAddress, uint8 vLevel, uint16 pAddress );
	void				ApplyUpdates( FRHICommandList& RHICmdList );

	void				QueueUpdateEntirePageTable();

	virtual FTextureRHIRef		GetPhysicalTexture(uint32 layer) const override { checkf(layer < VIRTUALTEXTURESPACE_MAXLAYERS, TEXT("%i"), layer); check(PhysicalTextures[layer].IsValid()); return PhysicalTextures[layer]->GetRenderTargetItem().ShaderResourceTexture; }
	virtual EPixelFormat		GetPhysicalTextureFormat(uint32 layer) override { checkf(layer < VIRTUALTEXTURESPACE_MAXLAYERS, TEXT("%i"), layer); return PhysicalTextureFormats[layer]; }
	virtual FIntPoint			Get2DPhysicalTextureSize() const override { return PhysicalTextureSize; }

	uint32				ID;
	uint32				PageTableSize;
	uint32				PageTableLevels;
	EPixelFormat		PageTableFormat;
	uint8				Dimensions;
	
	FVirtualTextureAllocator	Allocator;

	FTexturePagePool*	GetPool() const { return Pool; }

private:
	TRefCountPtr< IPooledRenderTarget >	PageTable;
	
	TArray< FPageUpdate >		PageTableUpdates;

	FStructuredBufferRHIRef		UpdateBuffer;
	FShaderResourceViewRHIRef	UpdateBufferSRV;

	bool						bForceEntireUpdate;

	TRefCountPtr<IPooledRenderTarget> PhysicalTextures[VIRTUALTEXTURESPACE_MAXLAYERS];
	EPixelFormat PhysicalTextureFormats[VIRTUALTEXTURESPACE_MAXLAYERS];
	FIntPoint PhysicalTextureSize;
	FTexturePagePool* Pool;
};
