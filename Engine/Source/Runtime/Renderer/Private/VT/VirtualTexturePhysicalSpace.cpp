// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VirtualTexturePhysicalSpace.h"
#include "VirtualTextureSystem.h"
#include "RenderTargetPool.h"
#include "VisualizeTexture.h"
#include "Stats/Stats.h"
#include "VT/VirtualTexturePoolConfig.h"

FVirtualTexturePhysicalSpace::FVirtualTexturePhysicalSpace(const FVTPhysicalSpaceDescription& InDesc, uint16 InID)
	: Description(InDesc)
	, NumRefs(0u)
	, ID(InID)
	, bPageTableLimit(false)
	, bGpuTextureLimit(false)
{
	const bool bForce16BitPageTable = false;
	uint32 PoolSizeInBytes = 0;
	if (!InDesc.bZooxMeshTileVT)
	{
		UVirtualTexturePoolConfig* PoolConfig = GetMutableDefault<UVirtualTexturePoolConfig>();
		const FVirtualTextureSpacePoolConfig* Config = PoolConfig->FindPoolConfig(InDesc.TileSize, InDesc.Format);
		PoolSizeInBytes = Config->SizeInMegabyte * 1024u * 1024u;
	}
	else
	{
		PoolSizeInBytes = 256 * 1024u * 1024u;
	}

	const FPixelFormatInfo& FormatInfo = GPixelFormats[InDesc.Format];
	check(InDesc.TileSize % FormatInfo.BlockSizeX == 0);
	check(InDesc.TileSize % FormatInfo.BlockSizeY == 0);
	const SIZE_T TileSizeBytes = CalculateImageBytes(InDesc.TileSize, InDesc.TileSize, 0, InDesc.Format);
	const uint32 MaxTiles = (uint32)(PoolSizeInBytes / TileSizeBytes);
	
	TextureSizeInTiles = FMath::FloorToInt(FMath::Sqrt((float)MaxTiles));
	if (bForce16BitPageTable)
	{
		// 16 bit page tables support max size of 64x64 (4096 tiles)
		TextureSizeInTiles = FMath::Min(64u, TextureSizeInTiles);
	}

	const uint32 TextureSize = TextureSizeInTiles * InDesc.TileSize;
	if (TextureSize > GetMax2DTextureDimension())
	{
		// A good option to support extremely large caches would be to allow additional slices in an array here for caches...
		// Just try to use the maximum texture size for now
		TextureSizeInTiles = GetMax2DTextureDimension() / InDesc.TileSize;
		bGpuTextureLimit = true;
	}

	Pool.Initialize(GetNumTiles());

#if STATS
	const FString LongName = FString::Printf(TEXT("WorkingSet %s %%"), FormatInfo.Name);
	WorkingSetSizeStatID = FDynamicStats::CreateStatIdDouble<STAT_GROUP_TO_FStatGroup(STATGROUP_VirtualTexturing)>(LongName);
#endif // STATS
}

FVirtualTexturePhysicalSpace::~FVirtualTexturePhysicalSpace()
{
}

void FVirtualTexturePhysicalSpace::InitRHI()
{
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	const uint32 TextureSize = GetTextureSize();
	const FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
		FIntPoint(TextureSize, TextureSize),
		Description.Format,
		FClearValueBinding::None,
		TexCreate_None,
		TexCreate_ShaderResource | (Description.bCreateRenderTarget ? (TexCreate_RenderTargetable | TexCreate_UAV) : 0),
		false);

	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, PooledRenderTarget, TEXT("PhysicalTexture"));
	FRHITexture* TextureRHI = PooledRenderTarget->GetRenderTargetItem().ShaderResourceTexture;

	// Create sRGB/non-sRGB views into the physical texture
	FRHITextureSRVCreateInfo ViewInfo;
	TextureView = RHICreateShaderResourceView(TextureRHI, ViewInfo);

	ViewInfo.SRGBOverride = SRGBO_ForceEnable;
	TextureSRGBView = RHICreateShaderResourceView(TextureRHI, ViewInfo);
}

void FVirtualTexturePhysicalSpace::ReleaseRHI()
{
	GRenderTargetPool.FreeUnusedResource(PooledRenderTarget);
}

uint32 FVirtualTexturePhysicalSpace::GetSizeInBytes() const
{
	const SIZE_T TileSizeBytes = CalculateImageBytes(Description.TileSize, Description.TileSize, 0, Description.Format);
	return GetNumTiles() * TileSizeBytes;
}

#if STATS
void FVirtualTexturePhysicalSpace::UpdateWorkingSetStat()
{
	const double Value = (double)WorkingSetSize.GetValue() / (double)GetNumTiles() * 100.0;
	FThreadStats::AddMessage(WorkingSetSizeStatID.GetName(), EStatOperation::Set, Value);
}
#endif // STATS
