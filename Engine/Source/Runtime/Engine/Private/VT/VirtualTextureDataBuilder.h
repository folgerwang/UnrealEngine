#pragma once
#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "TextureCompressorModule.h"
#include "VirtualTextureBuiltData.h"

struct FImage;
class ITextureCompressorModule;
class IImageWrapperModule;

struct FVirtualTextureBuilderLayerSettings
{
	FTextureSource *Source;
	FTextureBuildSettings SourceBuildSettings; // Build settings to use when building the source data. Note: Some options may be overriden by the virtual texture builder and will not be honoured.
	EGammaSpace GammaSpace;
	FIntRect SourceRectangle; // The specified rectangle will be extracted from the texture. If the rectangle is empty the whole source texture will be used instead.

	FVirtualTextureBuilderLayerSettings() :
		Source(nullptr),
		GammaSpace(EGammaSpace::Linear)
	{}

	FVirtualTextureBuilderLayerSettings(FTextureSource *SetSource) :
		Source(SetSource),
		GammaSpace(EGammaSpace::Linear)
	{}

	FIntRect GetRectangle();
};

struct FVirtualTextureBuilderSettings
{
	FString DebugName; // Name of the thing we're building for debugging purposes
	int32 TileWidth;  // Tile size excluding borders
	int32 TileHeight;  // Tile size excluding borders
	int32 Border; // A BorderSize pixel border will be added around all tiles
	int32 ChunkSize; // Size in bytes of data chunks

	TArray<FVirtualTextureBuilderLayerSettings> Layers;

	FVirtualTextureBuilderSettings() :
		TileWidth(128),
		TileHeight(128),
		Border(4),
		ChunkSize(1024 * 1024)
	{}
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

#define NUM_TILES_IN_CHUNK_ON_X_AXIS 16
#define NUM_TILES_IN_CHUNK_ON_Y_AXIS 15
#define NUM_TILES_IN_CHUCK (NUM_TILES_IN_CHUNK_ON_X_AXIS*NUM_TILES_IN_CHUNK_ON_Y_AXIS)

class FVirtualTextureDataBuilder
{
public:

	FVirtualTextureDataBuilder(FVirtualTextureBuiltData &SetOutData);
	~FVirtualTextureDataBuilder();

	void Build(const FVirtualTextureBuilderSettings &Settings);

private:

	struct TileId
	{
		int32 X;
		int32 Y;
		int32 Mip;

		TileId(int32 SetTileX, int32 SetTileY, int32 SetMipLevel) :
			X(SetTileX),
			Y(SetTileY),
			Mip(SetMipLevel)
		{}

		int32 GetMorton() const
		{
			return FMath::MortonCode2(X) | (FMath::MortonCode2(Y) << 1);
		}

		void SetMorton(int32 MortonPacked)
		{
			X = FMath::ReverseMortonCode2(MortonPacked);
			Y = FMath::ReverseMortonCode2(MortonPacked >> 1);
		}
	};

	struct FLayerData
	{
		TArray<uint8> Data;
		EVirtualTextureCodec Codec = EVirtualTextureCodec::Max;
		TArray<uint8> CodecPayload;

		using FTileInfo = TPair<uint32, uint32>; // [offset, size]
		TArray<FTileInfo> TileInfos;
	};
	using LayerDataArray = TArray<FLayerData, TFixedAllocator<MAX_NUM_LAYERS>>;

	using MacroTileArray = TArray<TileId, TFixedAllocator<NUM_TILES_IN_CHUCK>>;
	void BuildPagesMacroBlocks();
	//void AssembleTileForMacroBlock(const TileId& Tile, int32 layer, TArray<FImage>& TileList);
	void BuildTiles(const MacroTileArray& TileList, uint32 layer, FLayerData& GeneratedData);
	void PushDataToChunk(const MacroTileArray &Tiles, const LayerDataArray &LayerData);

	// Build the source data including mipmaps etc
	void BuildSourcePixels();
	// Release the source pixels
	void FreeSourcePixels();

	bool ExtractSourcePixels(int32 Layer, int32 Level, FImage &OutImage);

	// Build mipmap tail data
	void BuildMipTails();
	
	// Cached inside this object
	FVirtualTextureBuilderSettings Settings;
	FVirtualTextureBuiltData &OutData;

	// Some convenience variables (mostly derived from the passed in build settings)
	int32 Width;
	int32 Height;

	// Width/height in tiles on the miplevels
	TArray<int32, TFixedAllocator<MAX_NUM_MIPS>> NumTilesX;
	TArray<int32, TFixedAllocator<MAX_NUM_MIPS>> NumTilesY;

	int32 TotalTileWidth;
	int32 TotalTileHeight;

	int32 NumMips;

	// Data for the page currently being filled
	TArray<uint8> TilesBuffer;// Actual pixel data concatenated with
	FVirtualTextureChunkInfo ChunkHeader;


	TArray<TArray<FImage*, TFixedAllocator<MAX_NUM_MIPS>>, TFixedAllocator<MAX_NUM_LAYERS>> SourcePixels;
	TArray<ETextureSourceFormat, TFixedAllocator<MAX_NUM_LAYERS>> SourcePixelFormats;

	ITextureCompressorModule *Compressor;
	IImageWrapperModule *ImageWrapper;
};