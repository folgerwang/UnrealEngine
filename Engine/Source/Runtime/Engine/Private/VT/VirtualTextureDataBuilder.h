// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "TextureCompressorModule.h"
#include "VirtualTextureBuiltData.h"
#include "TextureDerivedDataTask.h"
#include "ImageCore.h"

struct FImage;
class ITextureCompressorModule;
class IImageWrapperModule;
struct FTextureSourceData;

struct FVTSourceTileEntry
{
	int32 BlockIndex;
	int32 TileIndex;
	int32 MipIndexInBlock;
	int32 TileInBlockX;
	int32 TileInBlockY;
};

struct FLayerData
{
	TArray<TArray<uint8>> TilePayload;
	TArray<uint8> CodecPayload;
	EVirtualTextureCodec Codec = EVirtualTextureCodec::Max;
};

struct FVirtualTextureSourceLayerData
{
	// All of these should refer to the same format
	ERawImageFormat::Type ImageFormat;
	ETextureSourceFormat SourceFormat;
	EPixelFormat PixelFormat;
	FName FormatName;

	EGammaSpace GammaSpace;
	bool bHasAlpha;
};

/**
 * Helper class for building virtual texture data. This works on a set of FTextureSource objects. The idea is that if needed we can create
 * FTextureSource without creating actual UTextures. This is why the builder should stay independent of UTexture. Things it does:
 * - Splits texture into tiles
 * - Preprocesses the tiles
 * - Bakes mips
 * - Does compression
 * Note: Most of the heavy pixel processing itself is internally deferred to the TextureCompressorModule.
 *
 * Data is cached in the BuilderObject so the BuildLayer call is not thread safe between calls. Create separate FVirtualTextureDataBuilder
 * instances for each thread instead!
 *
 * Current assumptions:
 * - We can keep "at least" all the source data in memory. We do not do "streaming" conversions of source data.
 * - Output can be "streaming" we don't have to keep all the data output in memory
 */
class FVirtualTextureDataBuilder
{
public:
	FVirtualTextureDataBuilder(FVirtualTextureBuiltData &SetOutData, ITextureCompressorModule *InCompressor = nullptr, IImageWrapperModule* InImageWrapper = nullptr);
	~FVirtualTextureDataBuilder();

	void Build(const FTextureSourceData& InSourceData, const FTextureSourceData& InCompositeSourceData, const FTextureBuildSettings* InSettingsPerLayer, bool bAllowAsync);

private:
	friend struct FAsyncMacroBlockTask;

	void BuildPagesMacroBlocks(bool bAllowAsync);
	void BuildPagesForChunk(const TArray<FVTSourceTileEntry>& ActiveTileList, bool bAllowAsync);
	void BuildTiles(const TArray<FVTSourceTileEntry>& TileList, uint32 layer, FLayerData& GeneratedData, bool bAllowAsync);
	void PushDataToChunk(const TArray<FVTSourceTileEntry> &Tiles, const TArray<FLayerData>& LayerData);

	int32 FindSourceBlockIndex(int32 MipIndex, int32 BlockX, int32 BlockY) const;

	// Build the source data including mipmaps etc
	void BuildSourcePixels(const FTextureSourceData& SourceData, const FTextureSourceData& CompositeSourceData);
	// Release the source pixels
	void FreeSourcePixels();
	
	// Cached inside this object
	TArray<FTextureBuildSettings> SettingsPerLayer;
	FVirtualTextureBuiltData &OutData;

	// Some convenience variables (mostly derived from the passed in build settings)
	int32 SizeInBlocksX;
	int32 SizeInBlocksY;
	int32 BlockSizeX;
	int32 BlockSizeY;
	int32 SizeX;
	int32 SizeY;

	TArray<FVirtualTextureSourceLayerData> SourceLayers;       
	TArray<FTextureSourceBlockData> SourceBlocks;
	//FTextureSourceBlockData SourceMiptailBlock;

	ITextureCompressorModule *Compressor;
	IImageWrapperModule *ImageWrapper;

	bool DetectAlphaChannel(const FImage &image);
};