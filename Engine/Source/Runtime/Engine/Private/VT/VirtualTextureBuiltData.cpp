#include "VirtualTextureBuiltData.h"
#include "Serialization/CustomVersion.h"

const FGuid FVirtualTextureBuiltDataCustomVersion::Key(0x804E3F75, 0x70884B49, 0xA4D68C06, 0x3C7EB6DC);
static FCustomVersionRegistration GVTBuiltDataRegisterVersion(FVirtualTextureBuiltDataCustomVersion::Key, FVirtualTextureBuiltDataCustomVersion::Latest, TEXT("VirtualTextureBuiltDataVersion"));

void FVirtualTextureBuiltData::Serialize(FArchive& Ar, UObject* Owner)
{
	Ar.UsingCustomVersion(FVirtualTextureBuiltDataCustomVersion::Key);
	const int32 Version = Ar.CustomVer(FVirtualTextureBuiltDataCustomVersion::Key);

	Ar << TileWidth;
	Ar << TileHeight;
	Ar << NumTilesX;
	Ar << NumTilesY;
	Ar << Border;
	Ar << Tiles;

	if (Version >= FVirtualTextureBuiltDataCustomVersion::ActualSize)
	{
		Ar << Width;
		Ar << Height;
	}
	else
	{
		Width = TileWidth * NumTilesX;
		Height = TileWidth * NumTilesX;
	}

	// Serialize the layer pixel formats.
	// Pixel formats are serialized as strings to protect against
	// enum changes
	UEnum* PixelFormatEnum = UTexture::GetPixelFormatEnum();
	int32 NumLayers = LayerTypes.Num();
	Ar << NumLayers;

	if (Ar.IsLoading())
	{
		LayerTypes.SetNumUninitialized(NumLayers);
		for (int32 Layer = 0; Layer < NumLayers; Layer++)
		{
			FString PixelFormatString;
			Ar << PixelFormatString;
			LayerTypes[Layer] = (EPixelFormat)PixelFormatEnum->GetValueByName(*PixelFormatString);
		}
	}
	else if (Ar.IsSaving())
	{
		for (int32 Layer = 0; Layer < NumLayers; Layer++)
		{
			FString PixelFormatString = PixelFormatEnum->GetNameByValue(LayerTypes[Layer]).GetPlainNameString();
			Ar << PixelFormatString;
		}
	}

	if (Version >= FVirtualTextureBuiltDataCustomVersion::MipTails)
	{
		Ar << MipTails;
	}
	
	// Serialize the chunks
	int32 NumChunks = Chunks.Num();
	Ar << NumChunks;

	if (Ar.IsLoading())
	{
		Chunks.SetNum(NumChunks);
	}

	for (int32 ChunkId = 0; ChunkId < NumChunks; ChunkId++)
	{
		Chunks[ChunkId].Serialize(Ar, Owner, ChunkId);
	}

	// serialize the chunk header sizes
	if (Version >= FVirtualTextureBuiltDataCustomVersion::MacroBlocks)
	{
		Ar << ChunkHeaderSizes;
	}
}