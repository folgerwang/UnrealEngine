// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "VirtualTextureDataBuilder.h"
#include "Modules/ModuleManager.h"
#include "VT/VirtualTexture.h"
#include "Modules/ModuleManager.h"
#include "CrunchCompression.h"
#include "Async/TaskGraphInterfaces.h"
#include "Async/ParallelFor.h"
#include "TextureCompressorModule.h"

// Debugging aid to dump tiles to disc as png files
#define SAVE_TILES 0
#if SAVE_TILES
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#endif

static TAutoConsoleVariable<int32> CVarVTParallelTileCompression(
	TEXT("r.VT.ParallelTileCompression"),
	1,
	TEXT("Enables parallel compression of macro tiles")
);

/*
 * Just a simple helper struct wrapping a pointer to an image in some source format.
 * This class does not own the memory it simply wraps some useful functionality around it
 * This functionality should ideally be part of something like FImage but it's not.
 */
struct FPixelDataRectangle
{
	ETextureSourceFormat Format;
	int32 Width;
	int32 Height;
	uint8 *Data;

	FPixelDataRectangle(ETextureSourceFormat SetFormat, int32 SetWidth, int32 SetHeight, uint8* SetData) :
		Format(SetFormat),
		Width(SetWidth),
		Height(SetHeight),
		Data(SetData)
	{}

	/*
	 * Copies a Width x Height rectangle located at SourceX, SourceY in the source image to location DestX,DestY in this image.
	 * If the requested rectangle is outside the source image it will be clipped to the source and the smaller clipped rectangle will be copied instead.
	 */
	void CopyRectangle(int32 DestX, int32 DestY, FPixelDataRectangle &Source, int32 SourceX, int32 SourceY, int32 RectWidth, int32 RectHeight)
	{
		checkf(Format == Source.Format, TEXT("Formats need to match"));
		checkf(DestX >= 0 && DestX < Width, TEXT("Destination location out of bounds"));
		checkf(DestY >= 0 && DestY < Height, TEXT("Destination location out of bounds"));

		int32 PixelSize = FTextureSource::GetBytesPerPixel(Source.Format);
		int32 SrcScanlineSize = Source.Width * PixelSize;
		int32 DstScanlineSize = Width * PixelSize;

		// Handle source position having negative coordinates in source image
		if (SourceX < 0)
		{
			DestX = DestX - SourceX;
			RectWidth = RectWidth + SourceX;
			SourceX = 0;
		}


		if (SourceY < 0)
		{
			DestY = DestY - SourceY;
			RectHeight = RectHeight + SourceY;
			SourceY = 0;
		}

		// Handle source position our width being beyond the boundaries of the source image
		int32 ClampedWidth = FMath::Max(FMath::Min(RectWidth, Source.Width - SourceX),0);
		int32 ClampedHeight = FMath::Max(FMath::Min(RectHeight, Source.Height - SourceY),0);
		int32 ClampedScanlineSize = ClampedWidth * PixelSize;

		// Copy the data a scan line at a time

		uint8 *DstScanline = Data + DestX * PixelSize + DestY * DstScanlineSize;
		const uint8 *SrcScanline = Source.Data + SourceX * PixelSize + SourceY * SrcScanlineSize;

		for (int Y = 0; Y < ClampedHeight; Y++)
		{
			FMemory::Memcpy(DstScanline, SrcScanline, ClampedScanlineSize);
			DstScanline += DstScanlineSize;
			SrcScanline += SrcScanlineSize;
		}
	}

	static int32 ApplyBorderMode(int32 x, int32 Width, TextureAddress Mode)
	{
		switch (Mode)
		{
		case TA_Wrap:
			return (x % Width + Width) % Width; // Make sure it's a proper module for negative numbers ....
		case TA_Clamp:
			return FMath::Max(FMath::Min(x, Width-1), 0);
		case TA_Mirror:
			int32 DoubleWidth = Width + Width;
			int32 DoubleWrap = (x % DoubleWidth + DoubleWidth) % DoubleWidth;
			return (DoubleWrap < Width) ? DoubleWrap : (Width-1) - (DoubleWrap - Width);
		}
		return x;
	}

	/*
	* Copies a Width x Height rectangle located at SourceX, SourceY in the source image to location DestX,DestY in this image.
	* If the requested rectangle is outside the source image it will be clipped to the source and the smaller clipped rectangle will be copied instead.
	*/
	void CopyRectangleBordered(int32 DestX, int32 DestY, FPixelDataRectangle &Source, int32 SourceX, int32 SourceY, int32 RectWidth, int32 RectHeight, TextureAddress BorderX, TextureAddress BorderY)
	{
		checkf(Format == Source.Format, TEXT("Formats need to match"));
		checkf(DestX >= 0 && DestX < Width, TEXT("Destination location out of bounds"));
		checkf(DestY >= 0 && DestY < Height, TEXT("Destination location out of bounds"));

		// Fast copy of regular pixels
		CopyRectangle(DestX, DestY, Source, SourceX, SourceY, RectWidth, RectHeight);

		size_t pixelSize = FTextureSource::GetBytesPerPixel(Format);

		// Special case the out of bounds pixels loop over all oob pixels and get the properly adjusted values
		if (SourceX < 0 ||
			SourceY < 0 ||
			SourceX + RectWidth > Source.Width ||
			SourceY + RectHeight > Source.Height)
		{
			// Top border and adjacent corners
			for (int32 y = SourceY; y < 0; y++)
			{
				for (int32 x = SourceX; x < SourceX + RectWidth; x++)
				{
					int32 xb = ApplyBorderMode(x, Source.Width, BorderX);
					int32 yb = ApplyBorderMode(y, Source.Height, BorderY);
					SetPixel(x - SourceX + DestX, y - SourceY + DestY, Source.GetPixel(xb, yb, pixelSize), pixelSize);
				}
			}

			// Bottom border and adjacent corners
			for (int32 y = Source.Height; y < SourceY + RectHeight; y++)
			{
				for (int32 x = SourceX; x < SourceX + RectWidth; x++)
				{
					int32 xb = ApplyBorderMode(x, Source.Width, BorderX);
					int32 yb = ApplyBorderMode(y, Source.Height, BorderY);
					SetPixel(x - SourceX + DestX, y - SourceY + DestY, Source.GetPixel(xb, yb, pixelSize), pixelSize);
				}
			}

			// Left border (w.o. corners)
			for (int32 x = SourceX; x < 0; x++)
			{
				//for (int32 y = SourceY; y < SourceY + RectHeight; y++)
				for (int32 y = FMath::Max(0, SourceY); y < FMath::Min(SourceY + RectHeight, Source.Height); y++)
				{
					int32 xb = ApplyBorderMode(x, Source.Width, BorderX);
					int32 yb = ApplyBorderMode(y, Source.Height, BorderY);
					SetPixel(x - SourceX + DestX, y - SourceY + DestY, Source.GetPixel(xb, yb, pixelSize), pixelSize);
				}
			}

			// Right border (w.o. corners)
			for (int32 x = Source.Width; x < SourceX + RectWidth; x++)
			{
				//for (int32 y = SourceY; y < SourceY + RectHeight; y++)
				for (int32 y = FMath::Max(0, SourceY); y < FMath::Min(SourceY + RectHeight, Source.Height); y++)
				{
					int32 xb = ApplyBorderMode(x, Source.Width, BorderX);
					int32 yb = ApplyBorderMode(y, Source.Height, BorderY);
					SetPixel(x - SourceX + DestX, y - SourceY + DestY, Source.GetPixel(xb, yb, pixelSize), pixelSize);
				}
			}
		}
	}

	void Clear()
	{
		FMemory::Memzero(Data, FTextureSource::GetBytesPerPixel(Format) * Width * Height);
	}

	inline void SetPixel(int32 x, int32 y, void *Value, size_t PixelSize)
	{
		void *DestPixelData = GetPixel(x, y, PixelSize);
		FMemory::Memcpy(DestPixelData, Value, PixelSize);
	}

	inline void *GetPixel(int32 x, int32 y, size_t PixelSize)
	{
		check(x >= 0);
		check(y >= 0);
		check(x < Width);
		check(y < Height);
		return Data + (((y * Width) + x) * PixelSize);
	}

#if SAVE_TILES
	void Save(FString BaseFileName, IImageWrapperModule* ImageWrapperModule)
	{
		IFileManager* FileManager = &IFileManager::Get();
		auto ImageWrapper = ImageWrapperModule->CreateImageWrapper(EImageFormat::PNG);
		int BytesPerPixel = FTextureSource::GetBytesPerPixel(Format);

		switch (Format)
		{
		case TSF_G8:
			ImageWrapper->SetRaw(Data, BytesPerPixel * Width * Height, Width, Height, ERGBFormat::Gray, 8);
			break;
		case TSF_BGRA8:
			ImageWrapper->SetRaw(Data, BytesPerPixel * Width * Height, Width, Height, ERGBFormat::BGRA, 8);
			break;
		case TSF_BGRE8:
			ImageWrapper->SetRaw(Data, BytesPerPixel * Width * Height, Width, Height, ERGBFormat::BGRA, 8);
			break;
		case TSF_RGBA16:
			ImageWrapper->SetRaw(Data, BytesPerPixel * Width * Height, Width, Height, ERGBFormat::RGBA, 16);
			break;
		case TSF_RGBA16F:
			ImageWrapper->SetRaw(Data, BytesPerPixel * Width * Height, Width, Height, ERGBFormat::RGBA, 16);
			break;
		case TSF_RGBA8:
			ImageWrapper->SetRaw(Data, BytesPerPixel * Width * Height, Width, Height, ERGBFormat::RGBA, 8);
			break;
		case TSF_RGBE8:
			ImageWrapper->SetRaw(Data, BytesPerPixel * Width * Height, Width, Height, ERGBFormat::RGBA, 8);
			break;
		default:
			return;
		}

		// Make sure it has the png extension
		FString NewExtension = TEXT(".png");
		FString Filename = FPaths::GetBaseFilename(BaseFileName, false) + NewExtension;

		// Compress and write image
		FArchive* Ar = FileManager->CreateFileWriter(*Filename);
		if (Ar != nullptr)
		{
			const TArray<uint8>& CompressedData = ImageWrapper->GetCompressed((int32)EImageCompressionQuality::Uncompressed);
			Ar->Serialize((void *)CompressedData.GetData(), CompressedData.Num());
			delete Ar;
		}
	}
#endif

};

#define TEXTURE_COMPRESSOR_MODULENAME "TextureCompressor"

FVirtualTextureDataBuilder::FVirtualTextureDataBuilder(FVirtualTextureBuiltData &SetOutData, ITextureCompressorModule *InCompressor, IImageWrapperModule* InImageWrapper)
	: OutData(SetOutData)
	, SizeInBlocksX(0)
	, SizeInBlocksY(0)
	, BlockSizeX(0)
	, BlockSizeY(0)
	, SizeX(0)
	, SizeY(0)
{
	Compressor = InCompressor ? InCompressor : &FModuleManager::LoadModuleChecked<ITextureCompressorModule>(TEXTURE_COMPRESSOR_MODULENAME);
	ImageWrapper = InImageWrapper ? InImageWrapper : &FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
}

FVirtualTextureDataBuilder::~FVirtualTextureDataBuilder()
{
	FreeSourcePixels();
}

void FVirtualTextureDataBuilder::Build(const FTextureSourceData& InSourceData, const FTextureSourceData& InCompositeSourceData, const FTextureBuildSettings* InSettingsPerLayer, bool bAllowAsync)
{
	const int32 NumLayers = InSourceData.Layers.Num();
	checkf(NumLayers <= (int32)VIRTUALTEXTURE_DATA_MAXLAYERS, TEXT("The maximum amount of layers is exceeded."));
	checkf(NumLayers > 0, TEXT("No layers to build."));

	SettingsPerLayer.AddUninitialized(NumLayers);
	FMemory::Memcpy(&SettingsPerLayer[0], InSettingsPerLayer, sizeof(FTextureBuildSettings) * NumLayers);

	BlockSizeX = InSourceData.BlockSizeX;
	BlockSizeY = InSourceData.BlockSizeY;

	// BlockSize is potentially adjusted by rounding to power of 2
	switch (SettingsPerLayer[0].PowerOfTwoMode)
	{
	case ETexturePowerOfTwoSetting::None:
		break;
	case ETexturePowerOfTwoSetting::PadToPowerOfTwo:
		BlockSizeX = FMath::RoundUpToPowerOfTwo(BlockSizeX);
		BlockSizeY = FMath::RoundUpToPowerOfTwo(BlockSizeY);
		break;
	case ETexturePowerOfTwoSetting::PadToSquarePowerOfTwo:
		BlockSizeX = FMath::RoundUpToPowerOfTwo(BlockSizeX);
		BlockSizeY = FMath::RoundUpToPowerOfTwo(BlockSizeY);
		BlockSizeX = FMath::Max(BlockSizeX, BlockSizeY);
		BlockSizeY = BlockSizeX;
		break;
	default:
		checkNoEntry();
		break;
	}

	constexpr uint32_t vt_size_scale = 1;
	SizeInBlocksX = InSourceData.SizeInBlocksX * vt_size_scale;
	SizeInBlocksY = InSourceData.SizeInBlocksY * vt_size_scale;
	SizeX = BlockSizeX * SizeInBlocksX;
	SizeY = BlockSizeY * SizeInBlocksY;

	// We require VT blocks (UDIM pages) to be PoT, but multi block textures may have full logical dimension that's not PoT
	check(FMath::IsPowerOfTwo(BlockSizeX));
	check(FMath::IsPowerOfTwo(BlockSizeY));

	const FTextureBuildSettings& BuildSettingsLayer0 = SettingsPerLayer[0];
	const int32 TileSize = BuildSettingsLayer0.VirtualTextureTileSize;

	//NOTE: OutData may point to a previously build data so it is important to
	//properly initialize all fields and not assume this is a freshly constructed object

	OutData.TileBorderSize = BuildSettingsLayer0.VirtualTextureBorderSize;
	OutData.TileSize = TileSize;
	OutData.NumLayers = NumLayers;
	OutData.Width = SizeX;
	OutData.Height = SizeY;
	OutData.WidthInBlocks = SizeInBlocksX;// InSourceData.SizeInBlocksX;
	OutData.HeightInBlocks = SizeInBlocksY;// InSourceData.SizeInBlocksY;

	OutData.TileIndexPerChunk.Empty();
	OutData.TileIndexPerMip.Empty();
	OutData.TileOffsetInChunk.Empty();
	OutData.Chunks.Empty();

	const uint32 Size = FMath::Max(SizeX, SizeY);
	const uint32 SizeInTiles = FMath::DivideAndRoundUp<uint32>(Size, TileSize);
	const uint32 BlockSize = FMath::Max(BlockSizeX, BlockSizeY);
	const uint32 BlockSizeInTiles = FMath::DivideAndRoundUp<uint32>(BlockSize, TileSize);
	OutData.NumMips = FMath::CeilLogTwo(SizeInTiles) + 1;

	BuildSourcePixels(InSourceData, InCompositeSourceData);

	// override async compression if requested
	bAllowAsync = bAllowAsync && CVarVTParallelTileCompression.GetValueOnAnyThread();

	BuildPagesMacroBlocks(bAllowAsync);
	FreeSourcePixels();
}

void FVirtualTextureDataBuilder::BuildPagesForChunk(const TArray<FVTSourceTileEntry>& ActiveTileList, bool bAllowAsync)
{
	TArray<FLayerData> LayerData;
	LayerData.AddDefaulted(SourceLayers.Num());

	for (int32 LayerIndex = 0; LayerIndex < LayerData.Num(); LayerIndex++)
	{
		BuildTiles(ActiveTileList, LayerIndex, LayerData[LayerIndex], bAllowAsync);
	}

	PushDataToChunk(ActiveTileList, LayerData);
}

void FVirtualTextureDataBuilder::BuildPagesMacroBlocks(bool bAllowAsync)
{
	static const uint32 MinSizePerChunk = 1024u; // Each chunk will contain a mip level of at least this size (MinSizePerChunk x MinSizePerChunk)
	const uint32 NumLayers = SourceLayers.Num();
	const int32 TileSize = SettingsPerLayer[0].VirtualTextureTileSize;
	const uint32 MinSizePerChunkInTiles = FMath::DivideAndRoundUp<uint32>(MinSizePerChunk, TileSize);
	const uint32 MinTilesPerChunk = MinSizePerChunkInTiles * MinSizePerChunkInTiles;
	const int32 BlockSizeInTilesX = FMath::DivideAndRoundUp(BlockSizeX, TileSize);
	const int32 BlockSizeInTilesY = FMath::DivideAndRoundUp(BlockSizeY, TileSize);

	uint32 MipWidthInTiles = FMath::DivideAndRoundUp(SizeX, TileSize);
	uint32 MipHeightInTiles = FMath::DivideAndRoundUp(SizeY, TileSize);
	uint32 NumTiles = 0u;

	for (uint32 Mip = 0; Mip < OutData.NumMips; ++Mip)
	{
		const uint32 MaxTileInMip = FMath::MortonCode2(MipWidthInTiles - 1) | (FMath::MortonCode2(MipHeightInTiles - 1) << 1);
		NumTiles += (MaxTileInMip + 1u);
		MipWidthInTiles = FMath::DivideAndRoundUp(MipWidthInTiles, 2u);
		MipHeightInTiles = FMath::DivideAndRoundUp(MipHeightInTiles, 2u);
	}

	//
	TArray<FVTSourceTileEntry> TilesInChunk;
	TilesInChunk.Reserve(NumTiles);

	// loop over each macro block and assemble the tiles
	{
		uint32 TileIndex = 0u;
		bool bInFinalChunk = false;

		OutData.TileOffsetInChunk.Init(~0u, NumTiles * NumLayers);
		OutData.TileIndexPerChunk.Reserve(OutData.NumMips + 1);
		OutData.TileIndexPerMip.Reserve(OutData.NumMips + 1);

		OutData.TileIndexPerChunk.Add(TileIndex);

		MipWidthInTiles = FMath::DivideAndRoundUp(SizeX, TileSize);
		MipHeightInTiles = FMath::DivideAndRoundUp(SizeY, TileSize);
		for (uint32 Mip = 0; Mip < OutData.NumMips; ++Mip)
		{
			const int32 MipBlockSizeInTilesX = FMath::Max(BlockSizeInTilesX >> Mip, 1);
			const int32 MipBlockSizeInTilesY = FMath::Max(BlockSizeInTilesY >> Mip, 1);
			const uint32 MaxTileInMip = FMath::MortonCode2(MipWidthInTiles - 1) | (FMath::MortonCode2(MipHeightInTiles - 1) << 1);

			OutData.TileIndexPerMip.Add(TileIndex);

			for (uint32 TileIndexInMip = 0u; TileIndexInMip <= MaxTileInMip; ++TileIndexInMip)
			{
				const uint32 TileX = FMath::ReverseMortonCode2(TileIndexInMip);
				const uint32 TileY = FMath::ReverseMortonCode2(TileIndexInMip >> 1);
				if (TileX < MipWidthInTiles && TileY < MipHeightInTiles)
				{
					const int32 BlockX = TileX / MipBlockSizeInTilesX;
					const int32 BlockY = TileY / MipBlockSizeInTilesY;

					const int32 BlockIndex = FindSourceBlockIndex(Mip, BlockX, BlockY);
					if (BlockIndex != INDEX_NONE)
					{
						const FTextureSourceBlockData& Block = SourceBlocks[BlockIndex];
						FVTSourceTileEntry* TileEntry = new(TilesInChunk) FVTSourceTileEntry;
						TileEntry->BlockIndex = BlockIndex;
						TileEntry->TileIndex = TileIndex;
						TileEntry->MipIndexInBlock = Mip - Block.MipBias;
						TileEntry->TileInBlockX = TileX - Block.BlockX * MipBlockSizeInTilesX;
						TileEntry->TileInBlockY = TileY - Block.BlockY * MipBlockSizeInTilesY;
					}
				}
				TileIndex += NumLayers;
			}

			if (!bInFinalChunk && TilesInChunk.Num() >= (int32)MinTilesPerChunk)
			{
				OutData.TileIndexPerChunk.Add(TileIndex);
				BuildPagesForChunk(TilesInChunk, bAllowAsync);
				TilesInChunk.Reset();
			}
			else
			{
				bInFinalChunk = true;
			}

			MipWidthInTiles = FMath::DivideAndRoundUp(MipWidthInTiles, 2u);
			MipHeightInTiles = FMath::DivideAndRoundUp(MipHeightInTiles, 2u);
		}

		check(TileIndex == NumTiles * NumLayers);
		OutData.TileIndexPerChunk.Add(TileIndex);
		OutData.TileIndexPerMip.Add(TileIndex);

		if (TilesInChunk.Num() > 0)
		{
			BuildPagesForChunk(TilesInChunk, bAllowAsync);
		}
	}

	// Patch holes left in offset array
	for (int32 ChunkIndex = 0; ChunkIndex < OutData.Chunks.Num(); ++ChunkIndex)
	{
		uint32 CurrentOffset = OutData.Chunks[ChunkIndex].SizeInBytes;
		for (int32 TileIndex = OutData.TileIndexPerChunk[ChunkIndex + 1] - 1u; TileIndex >= (int32)OutData.TileIndexPerChunk[ChunkIndex]; --TileIndex)
		{
			const uint32 TileOffset = OutData.TileOffsetInChunk[TileIndex];
			if (TileOffset > CurrentOffset)
			{
				check(TileOffset == ~0u);
				OutData.TileOffsetInChunk[TileIndex] = CurrentOffset;
			}
			else
			{
				CurrentOffset = TileOffset;
			}
		}
	}

	for (int32 TileIndex = 0u; TileIndex < OutData.TileOffsetInChunk.Num(); ++TileIndex)
	{
		const uint32 TileOffset = OutData.TileOffsetInChunk[TileIndex];
		check(TileOffset != ~0u);
	}
}

void FVirtualTextureDataBuilder::BuildTiles(const TArray<FVTSourceTileEntry>& TileList, uint32 LayerIndex, FLayerData& GeneratedData, bool bAllowAsync)
{
	const FTextureBuildSettings& BuildSettingsLayer0 = SettingsPerLayer[0];
	const FTextureBuildSettings& BuildSettingsForLayer = SettingsPerLayer[LayerIndex];
	const FVirtualTextureSourceLayerData& LayerData = SourceLayers[LayerIndex];

	const int32 TileSize = BuildSettingsLayer0.VirtualTextureTileSize;
	const int32 BorderSize = BuildSettingsLayer0.VirtualTextureBorderSize;
	const int32 PhysicalTileSize = TileSize + BorderSize * 2;

	FThreadSafeBool bCompressionError = false;
	EPixelFormat CompressedFormat = PF_Unknown;

	// Don't want platform specific swizzling for VT tile data, this tends to add extra padding for textures with odd dimensions
	// (VT physical tiles generally not power-of-2 after adding border)
	FName TextureFormatName = BuildSettingsForLayer.TextureFormatName;
	FString BaseTextureFormatName = TextureFormatName.ToString();
	if (BaseTextureFormatName.StartsWith(TEXT("PS4_")))
	{
		TextureFormatName = *BaseTextureFormatName.Replace(TEXT("PS4_"), TEXT(""));
	}
	else if (BaseTextureFormatName.StartsWith("XBOXONE_"))
	{
		TextureFormatName = *BaseTextureFormatName.Replace(TEXT("XBOXONE_"), TEXT(""));
	}

	// We handle AutoDXT specially here since otherwise the texture format compressor would choose a DXT format for every tile
	// individually. Causing tiles in the same VT to use different formats which we don't allow.
	static FName NameDXT1(TEXT("DXT1"));
	static FName NameDXT5(TEXT("DXT5"));
	static FName NameAutoDXT(TEXT("AutoDXT"));
	if (TextureFormatName == NameAutoDXT)
	{
		if (LayerData.bHasAlpha)
		{
			TextureFormatName = NameDXT5;
		}
		else
		{
			TextureFormatName = NameDXT1;
		}
	}

#if WITH_CRUNCH_COMPRESSION
	const bool bUseCrunch = BuildSettingsLayer0.bVirtualTextureEnableCompressCrunch &&
		BuildSettingsLayer0.LossyCompressionAmount != TLCA_None &&
		CrunchCompression::IsValidFormat(TextureFormatName);
	if (bUseCrunch)
	{
		check(LayerData.ImageFormat == ERawImageFormat::BGRA8);

		FCrunchEncodeParameters CrunchParameters;
		CrunchParameters.ImageWidth = PhysicalTileSize;
		CrunchParameters.ImageHeight = PhysicalTileSize;
		CrunchParameters.bIsGammaCorrected = BuildSettingsForLayer.GetGammaSpace() != EGammaSpace::Linear;
		CrunchParameters.OutputFormat = TextureFormatName;
		switch (BuildSettingsLayer0.LossyCompressionAmount)
		{
		case TLCA_Lowest: CrunchParameters.CompressionAmmount = 0.0f; break;
		case TLCA_Low: CrunchParameters.CompressionAmmount = 0.25f; break;
		case TLCA_Medium: CrunchParameters.CompressionAmmount = 0.5f; break;
		case TLCA_High: CrunchParameters.CompressionAmmount = 0.75f; break;
		case TLCA_Highest: CrunchParameters.CompressionAmmount = 1.0f; break;
		default: checkNoEntry(); CrunchParameters.CompressionAmmount = 0.0f; break;
		}

		// We can't split crunch compression into multiple tasks/threads, since all tiles need to compress together in order to generate the codec payload
		// Instead we rely on internal Crunch threading to make this efficient
		// Might be worth modifying Crunch to expose threading callbacks, so this can use UE4 task graph instead of Crunch's internal threadpool
		if (bAllowAsync && FApp::ShouldUseThreadingForPerformance())
		{
			CrunchParameters.NumWorkerThreads = FTaskGraphInterface::Get().GetNumWorkerThreads();
		}

		CrunchParameters.RawImagesRGBA.Reserve(TileList.Num());
		for (const FVTSourceTileEntry& Tile : TileList)
		{
			const FTextureSourceBlockData& Block = SourceBlocks[Tile.BlockIndex];
			const FImage& SourceMip = Block.MipsPerLayer[LayerIndex][Tile.MipIndexInBlock];
			FPixelDataRectangle SourceData(LayerData.SourceFormat,
				SourceMip.SizeX,
				SourceMip.SizeY,
				const_cast<uint8*>(SourceMip.RawData.GetData()));

			TArray<uint32>& RawImage = CrunchParameters.RawImagesRGBA.AddDefaulted_GetRef();
			RawImage.AddUninitialized(PhysicalTileSize * PhysicalTileSize);
			FPixelDataRectangle TileData(LayerData.SourceFormat, PhysicalTileSize, PhysicalTileSize, (uint8*)RawImage.GetData());

			TileData.Clear();
			TileData.CopyRectangleBordered(0, 0, SourceData,
				Tile.TileInBlockX * TileSize - BorderSize,
				Tile.TileInBlockY * TileSize - BorderSize,
				PhysicalTileSize,
				PhysicalTileSize,
				(TextureAddress)BuildSettingsLayer0.VirtualAddressingModeX,
				(TextureAddress)BuildSettingsLayer0.VirtualAddressingModeY);

			// Convert input image to the format expected by Crunch library
			for (int32 PixelIndex = 0u; PixelIndex < RawImage.Num(); ++PixelIndex)
			{
				const FColor Color(RawImage[PixelIndex]);
				RawImage[PixelIndex] = Color.ToPackedABGR();
			}
		}

		if (CrunchCompression::Encode(CrunchParameters, GeneratedData.CodecPayload, GeneratedData.TilePayload))
		{
			static FName NameBC4(TEXT("BC4"));
			static FName NameBC5(TEXT("BC5"));
			GeneratedData.Codec = EVirtualTextureCodec::Crunch;
			if (TextureFormatName == NameDXT1) CompressedFormat = PF_DXT1;
			else if (TextureFormatName == NameDXT5) CompressedFormat = PF_DXT5;
			else if (TextureFormatName == NameBC4) CompressedFormat = PF_BC4;
			else if (TextureFormatName == NameBC5) CompressedFormat = PF_BC5;
			else CompressedFormat = PF_Unknown;
		}
		else
		{
			bCompressionError = true;
		}
	}
	else
#endif // WITH_CRUNCH_COMPRESSION
	{
		// Create settings for building the tile. These should be simple, "clean" settings
		// just compressing the style to a GPU format not adding things like colour correction, ... 
		// as these settings were already baked into the SourcePixels.
		FTextureBuildSettings TBSettings;
		TBSettings.MaxTextureResolution = TNumericLimits<uint32>::Max();
		TBSettings.TextureFormatName = TextureFormatName;
		TBSettings.bSRGB = BuildSettingsForLayer.bSRGB;
		TBSettings.bUseLegacyGamma = BuildSettingsForLayer.bUseLegacyGamma;
		TBSettings.MipGenSettings = TMGS_NoMipmaps;

		check(TBSettings.GetGammaSpace() == BuildSettingsForLayer.GetGammaSpace());
 
		GeneratedData.TilePayload.AddDefaulted(TileList.Num());

		ParallelFor(TileList.Num(), [&](int32 TileIndex)
		{
			const FVTSourceTileEntry& Tile = TileList[TileIndex];

			const FTextureSourceBlockData& Block = SourceBlocks[Tile.BlockIndex];
			const FImage& SourceMip = Block.MipsPerLayer[LayerIndex][Tile.MipIndexInBlock];
			FPixelDataRectangle SourceData(LayerData.SourceFormat,
				SourceMip.SizeX,
				SourceMip.SizeY,
				const_cast<uint8*>(SourceMip.RawData.GetData()));

			TArray<FImage> TileImages;
			FImage* TileImage = new(TileImages) FImage(PhysicalTileSize, PhysicalTileSize, LayerData.ImageFormat, BuildSettingsForLayer.GetGammaSpace());
			FPixelDataRectangle TileData(LayerData.SourceFormat, PhysicalTileSize, PhysicalTileSize, TileImage->RawData.GetData());

			TileData.Clear();
			TileData.CopyRectangleBordered(0, 0, SourceData,
				Tile.TileInBlockX * TileSize - BorderSize,
				Tile.TileInBlockY * TileSize - BorderSize,
				PhysicalTileSize,
				PhysicalTileSize,
				(TextureAddress)BuildSettingsLayer0.VirtualAddressingModeX,
				(TextureAddress)BuildSettingsLayer0.VirtualAddressingModeY);

#if 0//SAVE_TILES
			{
				FString BasePath = FPaths::ProjectUserDir();
				FString TileFileName = BasePath / FString::Format(TEXT("{0}_{1}_{2}_{3}_{4}"), TArray<FStringFormatArg>({ Settings.DebugName, Tile.X, Tile.Y, Tile.Mip, LayerIndex }));
				TileData.Save(TileFileName, ImageWrapper);
				FString TileSourceFileName = BasePath / FString::Format(TEXT("{0}_{1}_{2}_{3}_{4}_Src"), TArray<FStringFormatArg>({ Settings.DebugName, Tile.X, Tile.Y, Tile.Mip, LayerIndex }));
				SourceData.Save(TileSourceFileName, ImageWrapper);
			}
#endif // SAVE_TILES

			TArray<FCompressedImage2D> CompressedMip;
			TArray<FImage> EmptyList;
			if (!ensure(Compressor->BuildTexture(TileImages, EmptyList, TBSettings, CompressedMip)))
			{
				bCompressionError = true;
			}

			check(CompressedMip.Num() == 1);
			check(CompressedFormat == PF_Unknown || CompressedFormat == CompressedMip[0].PixelFormat);
			CompressedFormat = (EPixelFormat)CompressedMip[0].PixelFormat;

			const uint32 SizeRaw = CompressedMip[0].RawData.Num() * CompressedMip[0].RawData.GetTypeSize();
			if (BuildSettingsLayer0.bVirtualTextureEnableCompressZlib)
			{
				TArray<uint8>& TilePayload = GeneratedData.TilePayload[TileIndex];
				int32 CompressedTileSize = FCompression::CompressMemoryBound(NAME_Zlib, SizeRaw);
				TilePayload.AddUninitialized(CompressedTileSize);
				verify(FCompression::CompressMemory(NAME_Zlib, TilePayload.GetData(), CompressedTileSize, CompressedMip[0].RawData.GetData(), SizeRaw));
				check(CompressedTileSize <= TilePayload.Num());

				// Set the correct size of the compressed tile, but avoid reallocating/copying memory
				TilePayload.SetNum(CompressedTileSize, false);
			}
			else
			{
				GeneratedData.TilePayload[TileIndex] = MoveTemp(CompressedMip[0].RawData);
			}
		}, !bAllowAsync); // ParallelFor

		if (BuildSettingsLayer0.bVirtualTextureEnableCompressZlib)
		{
			GeneratedData.Codec = EVirtualTextureCodec::ZippedGPU;
		}
		else
		{
			GeneratedData.Codec = EVirtualTextureCodec::RawGPU;
		}
	}

	if (OutData.LayerTypes[LayerIndex] == EPixelFormat::PF_Unknown)
	{
		OutData.LayerTypes[LayerIndex] = CompressedFormat;
	}
	else
	{
		checkf(OutData.LayerTypes[LayerIndex] == CompressedFormat, TEXT("The texture compressor used a different pixel format for some tiles."));
	}

	if (bCompressionError)
	{
		GeneratedData.TilePayload.Empty();
		GeneratedData.CodecPayload.Empty();
		GeneratedData.Codec = EVirtualTextureCodec::Max;
		UE_LOG(LogVirtualTexturingModule, Fatal, TEXT("Failed build tile"));
	}
}

void FVirtualTextureDataBuilder::PushDataToChunk(const TArray<FVTSourceTileEntry>& Tiles, const TArray<FLayerData>& LayerData)
{
	const int32 NumLayers = SourceLayers.Num();

	uint32 TotalSize = 0u;
	for (int32 Layer = 0; Layer < NumLayers; ++Layer)
	{
		TotalSize += LayerData[Layer].CodecPayload.Num();
		for (const TArray<uint8>& TilePayload : LayerData[Layer].TilePayload)
		{
			TotalSize += TilePayload.Num();
		}
	}

	FVirtualTextureDataChunk& Chunk = OutData.Chunks.AddDefaulted_GetRef();
	Chunk.SizeInBytes = TotalSize;
	FByteBulkData& BulkData = Chunk.BulkData;
	BulkData.Lock(LOCK_READ_WRITE);
	uint8* NewChunkData = (uint8*)BulkData.Realloc(TotalSize);
	uint32 ChunkOffset = 0u;

	// codec payloads
	for (int32 Layer = 0; Layer < NumLayers; ++Layer)
	{
		check(ChunkOffset <= 0xffff); // make sure code offset fit within uint16
		Chunk.CodecPayloadOffset[Layer] = ChunkOffset;
		Chunk.CodecType[Layer] = LayerData[Layer].Codec;
		if (LayerData[Layer].CodecPayload.Num() > 0)
		{
			FMemory::Memcpy(NewChunkData + ChunkOffset, LayerData[Layer].CodecPayload.GetData(), LayerData[Layer].CodecPayload.Num());
			ChunkOffset += LayerData[Layer].CodecPayload.Num();
		}
	}
	Chunk.CodecPayloadSize = ChunkOffset;

	for (int32 TileIdx = 0; TileIdx < Tiles.Num(); ++TileIdx)
	{
		const FVTSourceTileEntry& Tile = Tiles[TileIdx];
		uint32 TileIndex = Tile.TileIndex;
		for (int32 Layer = 0; Layer < NumLayers; ++Layer)
		{
			check(OutData.TileOffsetInChunk[TileIndex] == ~0u);
			OutData.TileOffsetInChunk[TileIndex] = ChunkOffset;
			++TileIndex;

			const TArray<uint8>& TilePayload = LayerData[Layer].TilePayload[TileIdx];
			const uint32 Size = TilePayload.Num();
			check(Size > 0u);

			FMemory::Memcpy(NewChunkData + ChunkOffset, TilePayload.GetData(), Size);
			ChunkOffset += Size;
		}
	}

	check(ChunkOffset == TotalSize);

	BulkData.Unlock();
	BulkData.SetBulkDataFlags(BULKDATA_Force_NOT_InlinePayload);
}

int32 FVirtualTextureDataBuilder::FindSourceBlockIndex(int32 MipIndex, int32 BlockX, int32 BlockY) const
{
	for(int32 BlockIndex = 0; BlockIndex < SourceBlocks.Num(); ++BlockIndex)
	{
		const FTextureSourceBlockData& Block = SourceBlocks[BlockIndex];
		if (BlockX >= Block.BlockX && BlockX < Block.BlockX + Block.SizeInBlocksX &&
			BlockY >= Block.BlockY && BlockY < Block.BlockY + Block.SizeInBlocksY &&
			MipIndex >= Block.MipBias &&
			(MipIndex - Block.MipBias) < Block.NumMips)
		{
			return BlockIndex;
		}
	}
	return INDEX_NONE;
}

// This builds an uncompressed version of the texture containing all other build settings baked in
// color corrections, mip sharpening, ....
void FVirtualTextureDataBuilder::BuildSourcePixels(const FTextureSourceData& SourceData, const FTextureSourceData& CompositeSourceData)
{
	static const TArray<FImage> EmptyImageArray;

	const int32 TileSize = SettingsPerLayer[0].VirtualTextureTileSize;
	const int32 NumBlocks = SourceData.Blocks.Num();
	const int32 NumLayers = SourceData.Layers.Num();

	SourceLayers.AddDefaulted(NumLayers);
	for (int32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		const FTextureBuildSettings& BuildSettingsForLayer = SettingsPerLayer[LayerIndex];
		FVirtualTextureSourceLayerData& LayerData = SourceLayers[LayerIndex];

		const FName TextureFormatName = BuildSettingsForLayer.TextureFormatName;
		const bool bIsHdr = BuildSettingsForLayer.bHDRSource || TextureFormatName == "BC6H" || TextureFormatName == "RGBA16F";

		LayerData.FormatName = "BGRA8";
		LayerData.PixelFormat = PF_B8G8R8A8;
		LayerData.SourceFormat = TSF_BGRA8;
		LayerData.ImageFormat = ERawImageFormat::BGRA8;
		LayerData.GammaSpace = BuildSettingsForLayer.GetGammaSpace();
		LayerData.bHasAlpha = false;

		if (bIsHdr)
		{
			LayerData.FormatName = "RGBA16F";
			LayerData.PixelFormat = PF_FloatRGBA;
			LayerData.SourceFormat = TSF_RGBA16F;
			LayerData.ImageFormat = ERawImageFormat::RGBA16F;
		}
	}

	SourceBlocks.AddDefaulted(NumBlocks);
	for (int32 BlockIndex = 0; BlockIndex < NumBlocks; ++BlockIndex)
	{
		const FTextureSourceBlockData& SourceBlockData = SourceData.Blocks[BlockIndex];

		FTextureSourceBlockData& BlockData = SourceBlocks[BlockIndex];
		BlockData.BlockX = SourceBlockData.BlockX;
		BlockData.BlockY = SourceBlockData.BlockY;
		BlockData.NumMips = SourceBlockData.NumMips;
		BlockData.NumSlices = SourceBlockData.NumSlices;
		BlockData.MipBias = SourceBlockData.MipBias;
		BlockData.SizeX = 0u;
		BlockData.SizeY = 0u;
		BlockData.MipsPerLayer.AddDefaulted(NumLayers);
		for (int32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
		{
			const FTextureBuildSettings& BuildSettingsForLayer = SettingsPerLayer[LayerIndex];
			FVirtualTextureSourceLayerData& LayerData = SourceLayers[LayerIndex];

			const TArray<FImage>& SourceMips = SourceBlockData.MipsPerLayer[LayerIndex];
			const TArray<FImage>* CompositeSourceMips = &EmptyImageArray;
			if (CompositeSourceData.Blocks.Num() > 0)
			{
				CompositeSourceMips = &CompositeSourceData.Blocks[BlockIndex].MipsPerLayer[LayerIndex];
			}

			// Adjust the build settings to generate an uncompressed texture with mips but leave other settings
			// like color correction, ... in place
			FTextureBuildSettings TBSettings = SettingsPerLayer[0];
			//TBSettings.MaxTextureResolution = TNumericLimits<uint32>::Max();
			TBSettings.TextureFormatName = LayerData.FormatName;
			TBSettings.bSRGB = BuildSettingsForLayer.bSRGB;
			TBSettings.bUseLegacyGamma = BuildSettingsForLayer.bUseLegacyGamma;

			// Make sure the output of the texture builder is in the same gamma space as we expect it.
			check(TBSettings.GetGammaSpace() == BuildSettingsForLayer.GetGammaSpace());

			// Leave original mip settings alone unless it's none at which point we will just generate them using a simple average
			if (TBSettings.MipGenSettings == TMGS_NoMipmaps)
			{
				if (FMath::IsPowerOfTwo(SourceMips[0].SizeX) &&
					FMath::IsPowerOfTwo(SourceMips[0].SizeY))
				{
					TBSettings.MipGenSettings = TMGS_SimpleAverage;
				}
				else
				{
					//checkf(false, TEXT("Non power of two textures cannot generate mips. Only existing mips can be used."));
					TBSettings.MipGenSettings = TMGS_SimpleAverage;
					TBSettings.PowerOfTwoMode = ETexturePowerOfTwoSetting::PadToPowerOfTwo;
				}
			}

			// Use the texture compressor module to do all the hard work
			TArray<FCompressedImage2D> CompressedMips;
			if (!Compressor->BuildTexture(SourceMips, *CompositeSourceMips, TBSettings, CompressedMips))
			{
				check(false);
			}

			// Get size of block from Compressor output, since it may have been padded/adjusted
			BlockData.SizeX = CompressedMips[0].SizeX;
			BlockData.SizeY = CompressedMips[0].SizeY;

			const uint32 BlockSize = FMath::Max(BlockData.SizeX, BlockData.SizeY);
			const uint32 BlockSizeInTiles = FMath::DivideAndRoundUp<uint32>(BlockSize, TileSize);
			const uint32 MaxMipInBlock = FMath::CeilLogTwo(BlockSizeInTiles);

			BlockData.NumMips = FMath::Min<int32>(CompressedMips.Num(), MaxMipInBlock + 1);
			BlockData.MipsPerLayer[LayerIndex].Reserve(BlockData.NumMips);
			for (int32 MipIndex = 0; MipIndex < BlockData.NumMips; ++MipIndex)
			{
				FCompressedImage2D& CompressedMip = CompressedMips[MipIndex];
				check(CompressedMip.PixelFormat == LayerData.PixelFormat);
				FImage* Image = new(BlockData.MipsPerLayer[LayerIndex]) FImage();
				Image->SizeX = CompressedMip.SizeX;
				Image->SizeY = CompressedMip.SizeY;
				Image->Format = LayerData.ImageFormat;
				Image->GammaSpace = BuildSettingsForLayer.GetGammaSpace();
				Image->NumSlices = 1;
				Image->RawData = MoveTemp(CompressedMip.RawData);
			}

			if (!LayerData.bHasAlpha && DetectAlphaChannel(BlockData.MipsPerLayer[LayerIndex][0]))
			{
				LayerData.bHasAlpha = true;
			}
		}
	}

	// If we have more than 1 block, need to create miptail that contains mips made from multiple blocks
	if (NumBlocks > 1)
	{
		const uint32 BlockSize = FMath::Max(BlockSizeX, BlockSizeY);
		const uint32 BlockSizeInTiles = FMath::DivideAndRoundUp<uint32>(BlockSize, TileSize);
		const uint32 MaxMipInBlock = FMath::CeilLogTwo(BlockSizeInTiles);
		const uint32 MipWidthInBlock = FMath::Max<uint32>(BlockSizeX >> MaxMipInBlock, 1);
		const uint32 MipHeightInBlock = FMath::Max<uint32>(BlockSizeY >> MaxMipInBlock, 1);
		const uint32 MipInputSizeX = FMath::RoundUpToPowerOfTwo(SizeInBlocksX * MipWidthInBlock);
		const uint32 MipInputSizeY = FMath::RoundUpToPowerOfTwo(SizeInBlocksY * MipHeightInBlock);
		const uint32 MipInputSize = FMath::Max(MipInputSizeX, MipInputSizeY);
		const uint32 MipInputSizeInTiles = FMath::DivideAndRoundUp<uint32>(MipInputSize, TileSize);

		FTextureSourceBlockData& SourceMiptailBlock = SourceBlocks.AddDefaulted_GetRef();
		SourceMiptailBlock.BlockX = 0;
		SourceMiptailBlock.BlockY = 0;
		SourceMiptailBlock.SizeInBlocksX = SizeInBlocksX; // miptail block covers the entire logical source texture
		SourceMiptailBlock.SizeInBlocksY = SizeInBlocksY;
		SourceMiptailBlock.SizeX = FMath::Max(MipInputSizeX >> 1, 1u);
		SourceMiptailBlock.SizeY = FMath::Max(MipInputSizeY >> 1, 1u);
		SourceMiptailBlock.NumMips = FMath::CeilLogTwo(MipInputSizeInTiles); // Don't add 1, since 'MipInputSizeInTiles' is one mip larger
		SourceMiptailBlock.NumSlices = 1; // TODO?
		SourceMiptailBlock.MipBias = MaxMipInBlock + 1;
		SourceMiptailBlock.MipsPerLayer.AddDefaulted(NumLayers);
		check(SourceMiptailBlock.NumMips > 0);

		// Total number of mips should be equal to number of mips per block plus number of miptail mips
		check(MaxMipInBlock + SourceMiptailBlock.NumMips + 1 == OutData.NumMips);

		TArray<FImage> MiptailInputImages;
		for (int32 LayerIndex = 0u; LayerIndex < NumLayers; ++LayerIndex)
		{
			const FTextureBuildSettings& BuildSettingsForLayer = SettingsPerLayer[LayerIndex];
			const FVirtualTextureSourceLayerData& LayerData = SourceLayers[LayerIndex];

			MiptailInputImages.Reset(1);
			FImage* MiptailInputImage = new(MiptailInputImages) FImage();
			MiptailInputImage->Init(MipInputSizeX, MipInputSizeY, LayerData.ImageFormat, LayerData.GammaSpace);
			FPixelDataRectangle DstPixelData(LayerData.SourceFormat, MipInputSizeX, MipInputSizeY, MiptailInputImage->RawData.GetData());
			DstPixelData.Clear();

			for (int32 BlockIndex = 0; BlockIndex < NumBlocks; ++BlockIndex)
			{
				const FTextureSourceBlockData& BlockData = SourceBlocks[BlockIndex];
				const FImage& SrcMipImage = BlockData.MipsPerLayer[LayerIndex][MaxMipInBlock - BlockData.MipBias];
				check(SrcMipImage.SizeX == MipWidthInBlock);
				check(SrcMipImage.SizeY == MipHeightInBlock);

				FPixelDataRectangle SrcPixelData(LayerData.SourceFormat, SrcMipImage.SizeX, SrcMipImage.SizeY, const_cast<uint8*>(SrcMipImage.RawData.GetData()));
				DstPixelData.CopyRectangle(BlockData.BlockX * MipWidthInBlock, BlockData.BlockY * MipHeightInBlock, SrcPixelData, 0, 0, MipWidthInBlock, MipHeightInBlock);
			}

#if SAVE_TILES
			{
				const FString BasePath = FPaths::ProjectUserDir();
				const FString MipFileName = BasePath / FString::Format(TEXT("{0}_{1}"), TArray<FStringFormatArg>({ SourceData.TextureName.ToString(), LayerIndex }));
				DstPixelData.Save(MipFileName, ImageWrapper);
			}
#endif // SAVE_TILES

			// Adjust the build settings to generate an uncompressed texture with mips but leave other settings
			// like color correction, ... in place
			FTextureBuildSettings TBSettings = SettingsPerLayer[0];
			//TBSettings.MaxTextureResolution = TNumericLimits<uint32>::Max();
			TBSettings.TextureFormatName = LayerData.FormatName;
			TBSettings.bSRGB = BuildSettingsForLayer.bSRGB;
			TBSettings.bUseLegacyGamma = BuildSettingsForLayer.bUseLegacyGamma;

			// Make sure the output of the texture builder is in the same gamma space as we expect it.
			check(TBSettings.GetGammaSpace() == BuildSettingsForLayer.GetGammaSpace());

			// Leave original mip settings alone unless it's none at which point we will just generate them using a simple average
			if (TBSettings.MipGenSettings == TMGS_NoMipmaps || TBSettings.MipGenSettings == TMGS_LeaveExistingMips)
			{
				TBSettings.MipGenSettings = TMGS_SimpleAverage;
			}

			// Use the texture compressor module to do all the hard work
			// TODO - composite images?
			TArray<FCompressedImage2D> CompressedMips;
			if (!Compressor->BuildTexture(MiptailInputImages, EmptyImageArray, TBSettings, CompressedMips))
			{
				check(false);
			}

			// We skip the first compressed mip output, since that will just be a copy of the input
			check(CompressedMips.Num() >= SourceMiptailBlock.NumMips + 1);
			check(SourceMiptailBlock.SizeX == CompressedMips[1].SizeX);
			check(SourceMiptailBlock.SizeY == CompressedMips[1].SizeY);

			SourceMiptailBlock.MipsPerLayer[LayerIndex].Reserve(CompressedMips.Num() - 1);
			for(int32 MipIndex = 1; MipIndex < SourceMiptailBlock.NumMips + 1; ++MipIndex)
			{
				FCompressedImage2D& CompressedMip = CompressedMips[MipIndex];
				check(CompressedMip.PixelFormat == LayerData.PixelFormat);
				FImage* Image = new(SourceMiptailBlock.MipsPerLayer[LayerIndex]) FImage();
				Image->SizeX = CompressedMip.SizeX;
				Image->SizeY = CompressedMip.SizeY;
				Image->Format = LayerData.ImageFormat;
				Image->GammaSpace = BuildSettingsForLayer.GetGammaSpace();
				Image->NumSlices = 1;
				Image->RawData = MoveTemp(CompressedMip.RawData);
			}
		}
	}
}

void FVirtualTextureDataBuilder::FreeSourcePixels()
{
	SourceBlocks.Empty();
	SourceLayers.Empty();
}

// Leaving this code here for now, in case we want to build a new/better system for creating/storing miptails
#if 0
void FVirtualTextureDataBuilder::BuildMipTails()
{
	OutData.MipTails.SetNum(Settings.Layers.Num());

	for (int32 Layer = 0; Layer < Settings.Layers.Num(); Layer++)
	{
		TArray<FImage> SourceList;
		TArray<FImage> EmptyList;

		int32 NumTailMips = SourcePixels[Layer].Num() - NumMips;

		// Make a list of mips to pass to the compressor
		for (int TailMip = 0; TailMip < NumTailMips; TailMip++)
		{
			FImage *TailMipImage = new (SourceList) FImage(*SourcePixels[Layer][TailMip + NumMips]);
		}

		// Adjust the build settings
		// The pixels we have already include things like color correction, mip blurring, ... so we just start
		// from pristine build settings here
		FTextureBuildSettings TBSettings;
		TBSettings.MaxTextureResolution = TNumericLimits<uint32>::Max();
		TBSettings.TextureFormatName = "BGRA8";
		TBSettings.bSRGB = Settings.Layers[Layer].SourceBuildSettings.bSRGB;
		TBSettings.bUseLegacyGamma = Settings.Layers[Layer].SourceBuildSettings.bUseLegacyGamma;
		TBSettings.MipGenSettings = TMGS_LeaveExistingMips;

		check(TBSettings.GetGammaSpace() == Settings.Layers[Layer].GammaSpace);

		TArray<FCompressedImage2D> CompressedMips;
		if (!Compressor->BuildTexture(SourceList, EmptyList, TBSettings, CompressedMips))
		{
			check(false);
		}

		OutData.MipTails[Layer].Empty();

		for (int Mip = 0; Mip < CompressedMips.Num(); Mip++)
		{
			check(CompressedMips[Mip].PixelFormat == EPixelFormat::PF_B8G8R8A8);
			OutData.MipTails[Layer].AddDefaulted();
			OutData.MipTails[Layer].Last().SizeX = CompressedMips[Mip].SizeX;
			OutData.MipTails[Layer].Last().SizeY = CompressedMips[Mip].SizeY;
			OutData.MipTails[Layer].Last().SizeZ = 1;
			OutData.MipTails[Layer].Last().Data = CompressedMips[Mip].RawData;
		}
	}
}
#endif // 0

bool FVirtualTextureDataBuilder::DetectAlphaChannel(const FImage &Image)
{
	if (Image.Format == ERawImageFormat::BGRA8)
	{
		const FColor *SrcColors = Image.AsBGRA8();
		const FColor* LastColor = SrcColors + (Image.SizeX * Image.SizeY * Image.NumSlices);
		while (SrcColors < LastColor)
		{
			if (SrcColors->A < 255)
			{
				return true;
			}
			++SrcColors;
		}
		return false;
	}
	else if (Image.Format == ERawImageFormat::RGBA16F)
	{
		const FFloat16Color *SrcColors = Image.AsRGBA16F();
		const FFloat16Color* LastColor = SrcColors + (Image.SizeX * Image.SizeY * Image.NumSlices);
		while (SrcColors < LastColor)
		{
			if (SrcColors->A <  (1.0f - SMALL_NUMBER))
			{
				return true;
			}
			++SrcColors;
		}
		return false;
	}
	else
	{
		check(false);
		return true;
	}
}

#endif // WITH_EDITOR
