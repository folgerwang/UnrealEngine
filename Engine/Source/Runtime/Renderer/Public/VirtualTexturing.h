
// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RendererInterface.h"
#include "Math/Vector2D.h"
#include "Math/Matrix.h"

class FTexturePagePool;

class RENDERER_API IVirtualTextureSpace : public FRenderResource
{
public:
	virtual uint64 AllocateVirtualTexture(IVirtualTexture* VirtualTexture) = 0;
	virtual void FreeVirtualTexture(IVirtualTexture* VirtualTexture) = 0;
	virtual uint32 GetSpaceID() const = 0;
	virtual FRHITexture* GetPageTableTexture() const = 0;
	
	// Returns the physical address of the given virtual adress
	virtual uint64 GetPhysicalAddress(uint32 vLevel, uint64 vAddr) const = 0;

	static IVirtualTextureSpace* Create(const FVirtualTextureSpaceDesc& desc);
	static void Delete(IVirtualTextureSpace*& Space);

	virtual FTextureRHIRef		GetPhysicalTexture(uint32 layer) const = 0;
	virtual EPixelFormat		GetPhysicalTextureFormat(uint32 layer) = 0;
	virtual FIntPoint			Get2DPhysicalTextureSize() const = 0;
	
protected:
};

// struct containing all data the GPU needs to perform a lookup/feedback request
struct FVirtualTextureUniformData
{
	int SpaceID;
	float PageTableSize;
	float vPageSize;
	float pPageBorder;
	FVector2D pTextureSize;
	float MaxAnisotropic;
	int MaxAssetLevel;

	FMatrix Pack() const
	{
		FMatrix Data(ForceInitToZero);

		Data.M[0][0] = SpaceID;
		Data.M[0][1] = PageTableSize;
		Data.M[0][2] = vPageSize;
		Data.M[0][3] = pPageBorder;

		Data.M[1][0] = pTextureSize.X;
		Data.M[1][1] = pTextureSize.Y;
		Data.M[1][2] = FMath::Log2(MaxAnisotropic);
		Data.M[1][3] = MaxAssetLevel;

		return Data;
	}

	static FMatrix Invalid;
};
static_assert(sizeof(FVirtualTextureUniformData) <= sizeof(FMatrix), "FVirtualTextureUniformData is unable to pack");
