// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VT/VirtualTextureUtility.h"
#include "RendererInterface.h"

static inline uint32 BitcastFloatToUInt32(float v)
{
	const union
	{
		float FloatValue;
		uint32 UIntValue;
	} u = { v };
	return u.UIntValue;
}

void VTGetPackedPageTableUniform(FUintVector4* Uniform, const IAllocatedVirtualTexture* AllocatedVT)
{
	if (AllocatedVT != nullptr)
	{
		static const auto MaxAnisotropyCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MaxAnisotropy"));

		const uint32 SpaceID = AllocatedVT->GetSpaceID();
		const uint32 vAddress = AllocatedVT->GetVirtualAddress();
		const uint32 vPageX = FMath::ReverseMortonCode2(vAddress);
		const uint32 vPageY = FMath::ReverseMortonCode2(vAddress >> 1);
		const uint32 vPageSize = AllocatedVT->GetVirtualTileSize();
		const uint32 PageBorderSize = AllocatedVT->GetTileBorderSize();
		const uint32 WidthInPages = (float)AllocatedVT->GetWidthInTiles();
		const uint32 HeightInPages = (float)AllocatedVT->GetHeightInTiles();
		const uint32 vPageTableMipBias = VIRTUALTEXTURE_LOG2_MAX_PAGETABLE_SIZE + FMath::FloorLog2(vPageSize);
		const uint32 MaxLevel = AllocatedVT->GetMaxLevel();

		const uint32 MaxAnisotropy = FMath::Clamp<int32>(MaxAnisotropyCVar->GetValueOnRenderThread(), 1, PageBorderSize);
		const uint32 MaxAnisotropyLog2 = FMath::FloorLog2(MaxAnisotropy);

		const float UVScale = 1.0f / float(VIRTUALTEXTURE_MAX_PAGETABLE_SIZE);

		// make sure everything fits in the allocated number of bits
		checkSlow(vPageX < 4096u);
		checkSlow(vPageY < 4096u);
		checkSlow(vPageTableMipBias < 256u);
		checkSlow(MaxLevel < 16u);
		checkSlow(SpaceID < 16u);

		Uniform[0].X = BitcastFloatToUInt32((float)WidthInPages * UVScale);
		Uniform[0].Y = BitcastFloatToUInt32((float)HeightInPages * UVScale);
		Uniform[0].Z = BitcastFloatToUInt32((float)WidthInPages);
		Uniform[0].W = BitcastFloatToUInt32((float)HeightInPages);

		Uniform[1].X = BitcastFloatToUInt32((float)MaxAnisotropyLog2);
		Uniform[1].Y = vPageX | (vPageY << 12) | (vPageTableMipBias << 24);
		Uniform[1].Z = MaxLevel;
		Uniform[1].W = (SpaceID << 28);
	}
	else
	{
		Uniform[0] = Uniform[1] = FUintVector4(ForceInitToZero);
	}
}

void VTGetPackedUniform(FUintVector4* Uniform, const IAllocatedVirtualTexture* AllocatedVT, uint32 LayerIndex)
{
	const uint32 PhysicalTextureSize = AllocatedVT ? AllocatedVT->GetPhysicalTextureSize(LayerIndex) : 0;
	if (PhysicalTextureSize > 0u)
	{
		const uint32 vPageSize = AllocatedVT->GetVirtualTileSize();
		const uint32 PageBorderSize = AllocatedVT->GetTileBorderSize();

		const float RcpPhysicalTextureSize = 1.0f / float(PhysicalTextureSize);
		const uint32 pPageSize = vPageSize + PageBorderSize * 2u;
		Uniform->X = 0u;
		Uniform->Y = BitcastFloatToUInt32((float)vPageSize * RcpPhysicalTextureSize);
		Uniform->Z = BitcastFloatToUInt32((float)PageBorderSize * RcpPhysicalTextureSize);
		Uniform->W = BitcastFloatToUInt32((float)pPageSize * RcpPhysicalTextureSize);
	}
	else
	{
		Uniform->X = 0u;
		Uniform->Y = 0u;
		Uniform->Z = 0u;
		Uniform->W = 0u;
	}
}

