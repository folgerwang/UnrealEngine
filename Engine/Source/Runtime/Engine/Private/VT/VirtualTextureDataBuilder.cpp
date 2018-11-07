#if WITH_EDITOR

#include "VirtualTextureDataBuilder.h"
#include "Modules/ModuleManager.h"
#include "ImageCore.h"
#include "VT/VirtualTextureSpace.h"
#include "Modules/ModuleManager.h"
#include "TextureCompressorModule.h"

#ifndef CRUNCH_SUPPORT
#define CRUNCH_SUPPORT 0
#endif

#if CRUNCH_SUPPORT
#include "CrunchCompression.h"
#endif

// Debugging aid to dump tiles to disc as png files
#define SAVE_TILES 0
#if SAVE_TILES
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "HAL/FileManager.h"
#include "FileHelper.h"
#endif

static TAutoConsoleVariable<int32> CVarVTCompressCrunch(
	TEXT("r.VT.EnableCompressCrunch"),
	0,
	TEXT("Enables Crunch compression")
);

int32 GetSourceTextureFormatPixelSize(ETextureSourceFormat Format)
{
	// Similar to FTextureSource::GetBytesPerPixel()
	// maybe move this to a static function on FTextureSource ?!?
	// instead of a per instance one
	switch (Format)
	{
	case TSF_Invalid:
		checkf(false, TEXT("Invalid source texture format!"));
		break;
	case TSF_G8:
		return 1;
	case TSF_BGRA8:
		return 4;
	case TSF_BGRE8:
		return 4;
	case TSF_RGBA16:
		return 8;
	case TSF_RGBA16F:
		return 8;
	case TSF_RGBA8:
		return 4;
	case TSF_RGBE8:
		return 4;
	case TSF_MAX:
	default:
		checkf(false, TEXT("Unknown source texture format!"));
		break;
	}
	return -1;
}


ERawImageFormat::Type ImageFormatFromSourceFormat(ETextureSourceFormat Format)
{
	// Duplicated from FTextureCacheDerivedDataWorker::FTextureSourceData::Init
	// maybe move this to a static function on FTextureSource ?!?
	switch (Format)
	{
	case TSF_G8:		return ERawImageFormat::G8;
	case TSF_BGRA8:		return ERawImageFormat::BGRA8;
	case TSF_BGRE8:		return ERawImageFormat::BGRE8;
	case TSF_RGBA16:	return ERawImageFormat::RGBA16;
	case TSF_RGBA16F:	return ERawImageFormat::RGBA16F;
	default:
		checkf(false, TEXT("Invalid texture source format"));
		return ERawImageFormat::BGRA8;
	}
}

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
	void CopyRectancle(int32 DestX, int32 DestY, FPixelDataRectangle &Source, int32 SourceX, int32 SourceY, int32 RectWidth, int32 RectHeight)
	{
		checkf(Format == Source.Format, TEXT("Formats need to match"));
		checkf(DestX >= 0 && DestX < RectWidth, TEXT("Destination location out of bounds"));
		checkf(DestY >= 0 && DestY < RectHeight, TEXT("Destination location out of bounds"));

		int32 PixelSize = GetSourceTextureFormatPixelSize(Source.Format);
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
		uint8 *SrcScanline = Source.Data + SourceX * PixelSize + SourceY * SrcScanlineSize;

		for (int Y = 0; Y < ClampedHeight; Y++)
		{
			FMemory::Memcpy(DstScanline, SrcScanline, ClampedScanlineSize);
			DstScanline += DstScanlineSize;
			SrcScanline += SrcScanlineSize;
		}
	}

	void Clear()
	{
		FMemory::Memset(Data, 0xFF, GetSourceTextureFormatPixelSize(Format) * Width * Height);
	}

#if SAVE_TILES
	void Save(FString BaseFileName)
	{
		IFileManager* FileManager = &IFileManager::Get();
		IImageWrapperModule* ImageWrapperModule = FModuleManager::LoadModulePtr<IImageWrapperModule>(FName("ImageWrapper"));
		auto ImageWrapper = ImageWrapperModule->CreateImageWrapper(EImageFormat::PNG);
		int BytesPerPixel = GetSourceTextureFormatPixelSize(Format);

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

FIntRect FVirtualTextureBuilderLayerSettings::GetRectangle()
{
	if (SourceRectangle.IsEmpty())
	{
		if (Source)
		{
			return FIntRect(0, 0, Source->GetSizeX(), Source->GetSizeY());
		}
		else
		{
			return FIntRect();
		}
	}
	else
	{
		return SourceRectangle;
	}
}


#define TEXTURE_COMPRESSOR_MODULENAME "TextureCompressor"

FVirtualTextureDataBuilder::FVirtualTextureDataBuilder(FVirtualTextureBuiltData &SetOutData) :
	OutData(SetOutData)
{
	Compressor = &FModuleManager::LoadModuleChecked<ITextureCompressorModule>(TEXTURE_COMPRESSOR_MODULENAME);
	ImageWrapper = &FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	//dummy value for testing
	ChunkHeader.Dummy = Dummy_Magic;
}

FVirtualTextureDataBuilder::~FVirtualTextureDataBuilder()
{
	FreeSourcePixels();
}

void FVirtualTextureDataBuilder::Build(const FVirtualTextureBuilderSettings &SetSettings)
{
	Settings = SetSettings;
	checkf(Settings.Layers.Num() <= MAX_NUM_LAYERS, TEXT("The maximum amount of layers is exceeded."));
	checkf(Settings.Layers.Num() > 0, TEXT("No layers to build."));
	checkf(Settings.Layers[0].Source != nullptr, TEXT("Layer 0 has no valid source data."));

	Width = Settings.Layers[0].GetRectangle().Width();
	Height = Settings.Layers[0].GetRectangle().Height();

	int32 NumTilesXL0 = FMath::DivideAndRoundUp(Width, Settings.TileWidth);
	int32 NumTilesYL0 = FMath::DivideAndRoundUp(Height, Settings.TileHeight);

	TotalTileWidth = Settings.TileWidth + Settings.Border * 2;
	TotalTileHeight = Settings.TileHeight + Settings.Border * 2;

	OutData.Border = Settings.Border;
	OutData.NumTilesX = NumTilesXL0;
	OutData.NumTilesY = NumTilesYL0;
	OutData.TileWidth = Settings.TileWidth;
	OutData.TileHeight = Settings.TileHeight;
	OutData.Width = Width;
	OutData.Height = Height;

	OutData.Tiles.Empty();
	OutData.Chunks.Empty();
	OutData.LayerTypes.SetNum(Settings.Layers.Num());

	//Run over the mip levels and initialize per tile defaults
	int MipTilesX = NumTilesXL0;
	int MipTilesY = NumTilesYL0;
	int Level = 0;
	//NumTiles = 0;
	while (MipTilesX != 0 && MipTilesY != 0)
	{
		NumTilesX.Add(MipTilesX);
		NumTilesY.Add(MipTilesY);
	//	NumTiles += MipTilesX*MipTilesY;
		OutData.Tiles.AddDefaulted(1);
		OutData.Tiles[Level].SetNum(MipTilesX * MipTilesY);
		MipTilesX = MipTilesX >> 1;
		MipTilesY = MipTilesY >> 1;
		Level++;
	}
	NumMips = Level;

	BuildSourcePixels();
	BuildPagesMacroBlocks();
	BuildMipTails();
	FreeSourcePixels();
}

void FVirtualTextureDataBuilder::BuildPagesMacroBlocks()
{
	TArray<TileId, TFixedAllocator<NUM_TILES_IN_CHUCK>> ActiveTileList;
	//int tt = 0;
	
	auto Flush = [&]() {
		LayerDataArray LayerData;
		LayerData.AddDefaulted(Settings.Layers.Num());

		for (int32 Layer = 0; Layer < Settings.Layers.Num(); Layer++)
		{
			BuildTiles(ActiveTileList, Layer, LayerData[Layer]);
		}
		
		PushDataToChunk(ActiveTileList, LayerData);
		ActiveTileList.Empty();
	};
	

	for (int32 Mip = 0; Mip < NumMips; Mip++)
	{
		int32 MipNumTilesX = NumTilesX[Mip];
		int32 MipNumTilesY = NumTilesY[Mip];

		int numBlocksX = FMath::DivideAndRoundUp(MipNumTilesX, NUM_TILES_IN_CHUNK_ON_X_AXIS);
		int numBlocksY = FMath::DivideAndRoundUp(MipNumTilesY, NUM_TILES_IN_CHUNK_ON_Y_AXIS);

		for (int32 blockY = 0; blockY < numBlocksY; ++blockY)
		{
			int32 tileBeginY = blockY * NUM_TILES_IN_CHUNK_ON_Y_AXIS;
			for (int32 blockX = 0; blockX < numBlocksX; ++blockX)
			{
				int32 tileBeginX = blockX * NUM_TILES_IN_CHUNK_ON_X_AXIS;
				for (int32 TileY = tileBeginY; TileY < tileBeginY + NUM_TILES_IN_CHUNK_ON_Y_AXIS; TileY++)
				{
					if (TileY >= MipNumTilesY)
					{
						break;
					}

					for (int32 TileX = tileBeginX; TileX < tileBeginX + NUM_TILES_IN_CHUNK_ON_X_AXIS; TileX++)
					{
						if (TileX >= MipNumTilesX)
						{
							break;
						}

						TileId Tile(TileX, TileY, Mip);

						if (ActiveTileList.Num() == NUM_TILES_IN_CHUCK)
						{
							Flush();
						}

						ActiveTileList.Add(Tile);
					}
				}
			}
		}
	}
	Flush();

	//ensure(tt == NumTiles);
}

void FVirtualTextureDataBuilder::BuildTiles(const MacroTileArray& TileList, uint32 LayerIndex, FLayerData& GeneratedData)
{
	TArray<FImage> ImageList;
	ImageList.Reserve(TileList.Num());

	ERawImageFormat::Type ImageFormat = ImageFormatFromSourceFormat(SourcePixelFormats[LayerIndex]);

	// Collect all raw tile image data
	for (auto Tile : TileList)
	{
		FImage *TileImage = new(ImageList) FImage(TotalTileWidth, TotalTileHeight, ImageFormat, Settings.Layers[LayerIndex].GammaSpace);

		FPixelDataRectangle TileData(SourcePixelFormats[LayerIndex], TotalTileWidth, TotalTileHeight, TileImage->RawData.GetData());

		FPixelDataRectangle SourceData(SourcePixelFormats[LayerIndex],
			SourcePixels[LayerIndex][Tile.Mip]->SizeX,
			SourcePixels[LayerIndex][Tile.Mip]->SizeY,
			SourcePixels[LayerIndex][Tile.Mip]->RawData.GetData());

		TileData.Clear();
		TileData.CopyRectancle(0, 0, SourceData,
			Tile.X * Settings.TileWidth - Settings.Border,
			Tile.Y * Settings.TileHeight - Settings.Border,
			TotalTileWidth,
			TotalTileHeight);

#if SAVE_TILES
		{
			FString BasePath = FPaths::ProjectUserDir();
			FString TileFileName = BasePath / FString::Format(TEXT("{0}_{1}_{2}_{3}_{4}"), TArray<FStringFormatArg>({ Settings.DebugName, Tile.X, Tile.Y, Tile.Mip, LayerIndex }));
			TileData.Save(TileFileName);
			FString TileSourceFileName = BasePath / FString::Format(TEXT("{0}_{1}_{2}_{3}_{4}_Src"), TArray<FStringFormatArg>({ Settings.DebugName, Tile.X, Tile.Y, Tile.Mip, LayerIndex }));
			SourceData.Save(TileSourceFileName);
		}
#endif
	}

	bool CompressionResult = false;
	EPixelFormat CompressedFormat = PF_Unknown;

#if CRUNCH_SUPPORT
	const bool UseCrunch = CVarVTCompressCrunch.GetValueOnAnyThread() > 0 && CrunchCompression::IsValidFormat(Settings.Layers[LayerIndex].SourceBuildSettings.TextureFormatName);
	if (UseCrunch)
	{	
		CompressionResult = CrunchCompression::Encode(ImageList, Settings.Layers[LayerIndex].SourceBuildSettings.TextureFormatName, 
			GeneratedData.CodecPayload, GeneratedData.Data, GeneratedData.TileInfos);
		GeneratedData.Codec = EVirtualTextureCodec::Crunch;


		static FName NameDXT1(TEXT("DXT1"));
		static FName NameDXT5(TEXT("DXT5"));

		if (Settings.Layers[LayerIndex].SourceBuildSettings.TextureFormatName == NameDXT1)			CompressedFormat = PF_DXT1;
		else if (Settings.Layers[LayerIndex].SourceBuildSettings.TextureFormatName == NameDXT5)		CompressedFormat = PF_DXT5;
		else																						CompressedFormat = PF_Unknown;
		
#if SAVE_TILES
		{
			FString BasePath = FPaths::ProjectUserDir();
			FString TileFileName = BasePath / FString::Format(TEXT("{0}_{1}_{2}_{3}_{4}.crn"), TArray<FStringFormatArg>({ Settings.DebugName, Tile.X, Tile.Y, Tile.Mip, LayerIndex }));
			FFileHelper::SaveArrayToFile(Compressed->RawData, *TileFileName);
		}
#endif
	}
	else
#endif
	{
		// Create settings for building the tile. These should be simple, "clean" settings
		// just compressing the style to a GPU format not adding things like colour correction, ... 
		// as these settings were already baked into the SourcePixels.
		FTextureBuildSettings TBSettings;
		TBSettings.MaxTextureResolution = TNumericLimits<uint32>::Max();
		TBSettings.TextureFormatName = Settings.Layers[LayerIndex].SourceBuildSettings.TextureFormatName;
		TBSettings.MipGenSettings = TMGS_NoMipmaps;
 
		TArray<FImage> EmptyList;
		TArray<FImage> TileImages;
		TArray<FCompressedImage2D> CompressedMip;
		EPixelFormat TileFormat = PF_Unknown;

		for(int32 TileIdx = 0; TileIdx < TileList.Num(); ++TileIdx)
		{
			TileImages.Empty();
			TileImages.Add(ImageList[TileIdx]);
			EmptyList.Empty();
			CompressedMip.Empty();

			CompressionResult = Compressor->BuildTexture(TileImages, EmptyList, TBSettings, CompressedMip);

			ensure(CompressionResult);
			ensure(TileFormat == PF_Unknown || TileFormat == CompressedMip[0].PixelFormat);
			TileFormat = (EPixelFormat)CompressedMip[0].PixelFormat;

			uint32 offset = GeneratedData.Data.Num() * GeneratedData.Data.GetTypeSize();
			uint32 size = CompressedMip[0].RawData.Num() * CompressedMip[0].RawData.GetTypeSize();
			GeneratedData.TileInfos.Add(FLayerData::FTileInfo(offset, size));

			GeneratedData.Data.Append(CompressedMip[0].RawData);
			GeneratedData.Codec = EVirtualTextureCodec::RawGPU;
 		}
		CompressedFormat = TileFormat;
	}

	if (OutData.LayerTypes[LayerIndex] == EPixelFormat::PF_Unknown)
	{
		OutData.LayerTypes[LayerIndex] = CompressedFormat;
	}
	else
	{
		checkf(OutData.LayerTypes[LayerIndex] == CompressedFormat, TEXT("The texture compressor used a different pixel format for some tiles."));
	}

	if (CompressionResult == false)
	{
		GeneratedData.Data.Empty();
		GeneratedData.CodecPayload.Empty();
		GeneratedData.Codec = EVirtualTextureCodec::Max;
		UE_LOG(LogVirtualTexturingModule, Fatal, TEXT("Failed build tile"));
	}
}

void FVirtualTextureDataBuilder::PushDataToChunk(const MacroTileArray &Tiles, const LayerDataArray &LayerData)
{
	TilesBuffer.Empty();

	// reserver to total memory up front so we can have a FVirtualTextureChunkLayerInfo* into that data
	uint32 totalSize = sizeof(ChunkHeader);
	totalSize += Settings.Layers.Num() * sizeof(FVirtualTextureChunkLayerInfo);
	for (int32 Layer = 0; Layer < Settings.Layers.Num(); ++Layer)
	{
		totalSize += LayerData[Layer].CodecPayload.Num() + LayerData[Layer].Data.Num();
	}
	TilesBuffer.Reserve(totalSize);

	//Chunk header
	TilesBuffer.Append((uint8 *)&ChunkHeader, sizeof(ChunkHeader));
	
	//Fixed size chunk layer info
	const uint32 InfoPosition = TilesBuffer.Num() * TilesBuffer.GetTypeSize();
	TilesBuffer.AddUninitialized(sizeof(FVirtualTextureChunkLayerInfo) * Settings.Layers.Num());
 	FVirtualTextureChunkLayerInfo* infos = (FVirtualTextureChunkLayerInfo*)(TilesBuffer.GetData() + InfoPosition);

	// codec payloads
	for (int32 Layer = 0; Layer < Settings.Layers.Num(); ++Layer)
	{
		infos[Layer].CodecPayloadOffset = TilesBuffer.Num() * TilesBuffer.GetTypeSize();
		infos[Layer].CodecPayloadSize = LayerData[Layer].CodecPayload.Num() * LayerData[Layer].CodecPayload.GetTypeSize();
		infos[Layer].Codec = (uint32)LayerData[Layer].Codec;
		TilesBuffer.Append(LayerData[Layer].CodecPayload);
	}
	
	const uint32 HeaderSize = TilesBuffer.Num() * TilesBuffer.GetTypeSize();

	for (int32 TileIdx = 0; TileIdx < Tiles.Num(); ++TileIdx)
	{
		const TileId& Tile = Tiles[TileIdx];
		const int32 IndexToInsert = Tile.GetMorton();
		if (IndexToInsert >= OutData.Tiles[Tile.Mip].Num())
		{
			OutData.Tiles[Tile.Mip].SetNumZeroed(IndexToInsert + 1);
		}

		OutData.Tiles[Tile.Mip][IndexToInsert].Chunk = OutData.Chunks.Num(); //The next chunk should still be added
		OutData.Tiles[Tile.Mip][IndexToInsert].Offset = TilesBuffer.Num() * TilesBuffer.GetTypeSize();
		
		for (int32 Layer = 0; Layer < Settings.Layers.Num(); ++Layer)
		{
			const uint8* Data = LayerData[Layer].Data.GetData() + LayerData[Layer].TileInfos[TileIdx].Key;
			const uint32 size = LayerData[Layer].TileInfos[TileIdx].Value;

			OutData.Tiles[Tile.Mip][IndexToInsert].Size.Add(size);
			TilesBuffer.Append(Data, size);
		}
	}

	// make the actual chunk
	FByteBulkData *Chunk = new (OutData.Chunks) FByteBulkData();
	ensure(OutData.Chunks.Num() < (1 << FVirtualTextureTileInfo_ChunkBits));
	Chunk->Lock(LOCK_READ_WRITE);
	void* NewChunkData = Chunk->Realloc(TilesBuffer.Num());
	FMemory::Memcpy(NewChunkData, TilesBuffer.GetData(), TilesBuffer.Num());
	Chunk->Unlock();
	Chunk->SetBulkDataFlags(BULKDATA_Force_NOT_InlinePayload);

	// update the header sizes
	OutData.ChunkHeaderSizes.Add(HeaderSize);
}

// Extract an image from the source data this takes into account the current source
// rectangle settings
// If the source is not available an correctly sized black image will be returned instead
// and false will be returned
bool FVirtualTextureDataBuilder::ExtractSourcePixels(int32 Layer, int32 Level, FImage &OutImage)
{
	if (Settings.Layers[Layer].Source != nullptr && Level < Settings.Layers[Layer].Source->GetNumMips() )
	{
		ETextureSourceFormat SourceFormat = Settings.Layers[Layer].Source->GetFormat();
		ERawImageFormat::Type ImageFormat = ImageFormatFromSourceFormat(SourceFormat);
		OutImage.Init(Width >> Level, Height >> Level, ImageFormat, Settings.Layers[Layer].GammaSpace); // TODO: Set gamma correctly here

		// We have to use GetMipData here not map/unmap as map/unmap always assumes the source gets written over
		// and generates a new GUId causing the texture the source belongs to to be rebuilt. GetMipData avoids 
		// this issue.
		TArray<uint8> SourceData;
		Settings.Layers[Layer].Source->GetMipData(SourceData, Level, ImageWrapper);

		// Copy the specified rect
		FIntRect SourceSubRect = Settings.Layers[Layer].GetRectangle();
		FPixelDataRectangle SourcePDR(SourceFormat, Settings.Layers[Layer].Source->GetSizeX() >> Level, Settings.Layers[Layer].Source->GetSizeY() >> Level, SourceData.GetData());
		FPixelDataRectangle DestPDR(SourceFormat, Width >> Level, Height >> Level, OutImage.RawData.GetData());
		DestPDR.CopyRectancle(0, 0, SourcePDR, SourceSubRect.Min.X >> Level, SourceSubRect.Min.Y >> Level, SourceSubRect.Width() >> Level, SourceSubRect.Height() >> Level);
		return true;
	}
	else
	{
		// Just create a uniform black image
		OutImage.Init(Width >> Level, Height >> Level, ERawImageFormat::BGRA8, Settings.Layers[Layer].GammaSpace); // TODO: Set gamma correctly here
		FMemory::Memzero(OutImage.RawData.GetData(), OutImage.RawData.Num());
		return false;
	}
}

// This builds an uncompressed version of the texture containing all other build settings baked in
// color corrections, mip sharpening, ....
void FVirtualTextureDataBuilder::BuildSourcePixels()
{
	SourcePixels.AddDefaulted(Settings.Layers.Num());
	SourcePixelFormats.AddDefaulted(Settings.Layers.Num());

	for (int32 Layer = 0; Layer < Settings.Layers.Num(); Layer++)
	{
		TArray<FImage> SourceList;
		TArray<FImage> EmptyList;

		if (Settings.Layers[Layer].Source != nullptr)
		{
			// Extract all the source data mip levels we have
			FIntRect SourceSubRect = Settings.Layers[Layer].GetRectangle();
			int32 SourceWidth = SourceSubRect.Width();
			int32 SourceHeight = SourceSubRect.Height();
			for (int MipLevel = 0; MipLevel < Settings.Layers[Layer].Source->GetNumMips() && SourceWidth != 0 && SourceHeight != 0; MipLevel++)
			{				
				FImage *LevelImage = new(SourceList) FImage();
				ExtractSourcePixels(Layer, MipLevel, *LevelImage);
				SourceWidth = SourceWidth >> 1;
				SourceHeight = SourceHeight >> 1;
			}
		}
		else
		{
			FImage *BlackImage = new(SourceList) FImage();
			ExtractSourcePixels(Layer, 0, *BlackImage);
		}

		// Adjust the build settings to generate an uncompressed texture with mips but leave other settings
		// like color correction, ... in place
		FTextureBuildSettings TBSettings = Settings.Layers[Layer].SourceBuildSettings;
		TBSettings.MaxTextureResolution = TNumericLimits<uint32>::Max();
		TBSettings.TextureFormatName = "BGRA8";

		// Use the texture compressor module to do all the hard work
		TArray<FCompressedImage2D> CompressedMips;
		if (!Compressor->BuildTexture(SourceList, EmptyList, TBSettings, CompressedMips))
		{
			check(false);
		}

		// Store generated data for later use by the tiler
		for (int Mip = 0; Mip < CompressedMips.Num(); Mip++)
		{
			check(CompressedMips[Mip].PixelFormat == EPixelFormat::PF_B8G8R8A8);
			FImage *Image = new FImage(CompressedMips[Mip].SizeX, CompressedMips[Mip].SizeY, ERawImageFormat::BGRA8, Settings.Layers[Layer].GammaSpace);
			Image->RawData = CompressedMips[Mip].RawData;
			SourcePixels[Layer].Add(Image);
		}

		SourcePixelFormats[Layer] = TSF_BGRA8;

	}
}

void FVirtualTextureDataBuilder::FreeSourcePixels()
{
	for (int32 Layer = 0; Layer < SourcePixels.Num(); Layer++)
	{
		for (int32 Mip = 0; Mip < SourcePixels[Layer].Num(); Mip++)
		{
			if (SourcePixels[Layer][Mip])
			{
				delete SourcePixels[Layer][Mip];
			}
		}
	}
	SourcePixels.Empty();
}

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
		TBSettings.MipGenSettings = TMGS_LeaveExistingMips;

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

#endif