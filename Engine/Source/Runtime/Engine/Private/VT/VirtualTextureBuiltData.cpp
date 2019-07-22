// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureBuiltData.h"
#include "Serialization/CustomVersion.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/CoreMisc.h"
#include "DerivedDataCacheInterface.h"

uint64 FVirtualTextureBuiltData::GetDiskMemoryFootprint() const
{
	uint64 result = 0;
	for (int32 ChunkId = 0; ChunkId < Chunks.Num(); ChunkId++)
	{
		result += Chunks[ChunkId].SizeInBytes;
	}
	return result;
}

uint32 FVirtualTextureBuiltData::GetMemoryFootprint() const
{
	uint32 TotalSize = sizeof(*this);

	TotalSize += Chunks.GetAllocatedSize();
	for (const FVirtualTextureDataChunk& Chunk : Chunks)
	{
		TotalSize += Chunk.GetMemoryFootprint();
	}

	TotalSize += GetTileMemoryFootprint();

	return TotalSize;
}

uint32 FVirtualTextureBuiltData::GetTileMemoryFootprint() const
{
	return TileOffsetInChunk.GetAllocatedSize() + TileIndexPerChunk.GetAllocatedSize() + TileIndexPerMip.GetAllocatedSize();
}

uint32 FVirtualTextureBuiltData::GetNumTileHeaders() const
{
	return TileOffsetInChunk.Num();
}

void FVirtualTextureBuiltData::Serialize(FArchive& Ar, UObject* Owner, int32 FirstMipToSerialize)
{
	check(FirstMipToSerialize == 0 || Ar.IsSaving());
	const bool bStripMips = (FirstMipToSerialize > 0);
	uint32 NumChunksToStrip = 0u;

	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	Ar << NumLayers;
	Ar << WidthInBlocks;
	Ar << HeightInBlocks;
	Ar << TileSize;
	Ar << TileBorderSize;

	if (!bStripMips)
	{
		Ar << NumMips;
		Ar << Width;
		Ar << Height;
		Ar << TileIndexPerChunk;
		Ar << TileIndexPerMip;
		Ar << TileOffsetInChunk;
	}
	else
	{
		check((uint32)FirstMipToSerialize < NumMips);
		const uint32 NumTilesToStrip = TileIndexPerMip[FirstMipToSerialize];
		check(NumTilesToStrip < (uint32)TileOffsetInChunk.Num());
		for (int32 ChunkIndex = 0; ChunkIndex < Chunks.Num(); ++ChunkIndex)
		{
			if (TileIndexPerChunk[ChunkIndex + 1] <= NumTilesToStrip)
			{
				++NumChunksToStrip;
			}
			else
			{
				break;
			}
		}

		uint32 NumMipsToSerialize = NumMips - FirstMipToSerialize;
		uint32 WidthToSerialize = Width >> FirstMipToSerialize;
		uint32 HeightToSerialize = Height >> FirstMipToSerialize;
		TArray<uint32> StrippedTileIndexPerChunk;
		TArray<uint32> StrippedTileIndexPerMip;
		TArray<uint32> StrippedTileOffsetInChunk;

		StrippedTileIndexPerChunk.Reserve(TileIndexPerChunk.Num() - NumChunksToStrip);
		for (int32 i = NumChunksToStrip; i < TileIndexPerChunk.Num(); ++i)
		{
			// Since we can only exclude data by chunk, it's possible that the first chunk we need to include will contain some initial tiles from mip that's been exclude
			StrippedTileIndexPerChunk.Add(TileIndexPerChunk[i] - FMath::Min(NumTilesToStrip, TileIndexPerChunk[i]));
		}

		StrippedTileIndexPerMip.Reserve(TileIndexPerMip.Num() - FirstMipToSerialize);
		for (int32 i = FirstMipToSerialize; i < TileIndexPerMip.Num(); ++i)
		{
			check(TileIndexPerMip[i] >= NumTilesToStrip);
			StrippedTileIndexPerMip.Add(TileIndexPerMip[i] - NumTilesToStrip);
		}

		StrippedTileOffsetInChunk.Reserve(TileOffsetInChunk.Num() - NumTilesToStrip);
		for (int32 i = NumTilesToStrip; i < TileOffsetInChunk.Num(); ++i)
		{
			// offsets within each chunk are unchanged...we are removing chunks that are no longer referenced, but not truncating any existing chunks
			StrippedTileOffsetInChunk.Add(TileOffsetInChunk[i]);
		}

		Ar << NumMipsToSerialize;
		Ar << WidthToSerialize;
		Ar << HeightToSerialize;
		Ar << StrippedTileIndexPerChunk;
		Ar << StrippedTileIndexPerMip;
		Ar << StrippedTileOffsetInChunk;
	}

	// Serialize the layer pixel formats.
	// Pixel formats are serialized as strings to protect against enum changes
	const UEnum* PixelFormatEnum = UTexture::GetPixelFormatEnum();
	if (Ar.IsLoading())
	{
		checkf(NumLayers <= VIRTUALTEXTURE_DATA_MAXLAYERS, TEXT("Trying to load FVirtualTextureBuiltData with %d layers, only %d layers supported"),
			NumLayers, VIRTUALTEXTURE_DATA_MAXLAYERS);
		for (uint32 Layer = 0; Layer < NumLayers; Layer++)
		{
			FString PixelFormatString;
			Ar << PixelFormatString;
			LayerTypes[Layer] = (EPixelFormat)PixelFormatEnum->GetValueByName(*PixelFormatString);
		}
	}
	else if (Ar.IsSaving())
	{
		for (uint32 Layer = 0; Layer < NumLayers; Layer++)
		{
			FString PixelFormatString = PixelFormatEnum->GetNameByValue(LayerTypes[Layer]).GetPlainNameString();
			Ar << PixelFormatString;
		}
	}
	
	// Serialize the chunks
	int32 NumChunksToSerialize = Chunks.Num() - NumChunksToStrip;
	Ar << NumChunksToSerialize;

	if (Ar.IsLoading())
	{
		Chunks.SetNum(NumChunksToSerialize);
	}

	int32 SerialzeChunkId = 0;
	for (int32 ChunkId = NumChunksToStrip; ChunkId < Chunks.Num(); ChunkId++)
	{
		FVirtualTextureDataChunk& Chunk = Chunks[ChunkId];

		Ar << Chunk.SizeInBytes;
		Ar << Chunk.CodecPayloadSize;
		for (uint32 LayerIndex = 0u; LayerIndex < NumLayers; ++LayerIndex)
		{
			Ar << Chunk.CodecType[LayerIndex];
			Ar << Chunk.CodecPayloadOffset[LayerIndex];
		}

		Chunk.BulkData.Serialize(Ar, Owner, SerialzeChunkId);

#if WITH_EDITORONLY_DATA
		if (!bCooked)
		{
			Ar << Chunk.DerivedDataKey;
			if (Ar.IsLoading() && !Ar.IsCooking())
			{
				Chunk.ShortenKey(Chunk.DerivedDataKey, Chunk.ShortDerivedDataKey);
			}
		}
#endif // WITH_EDITORONLY_DATA

		SerialzeChunkId++;
	}
}

#if WITH_EDITORONLY_DATA

bool FVirtualTextureDataChunk::ShortenKey(const FString& CacheKey, FString& Result)
{
#define MAX_BACKEND_KEY_LENGTH (120)

	Result = FString(CacheKey);
	if (Result.Len() <= MAX_BACKEND_KEY_LENGTH)
	{
		return false;
	}

	FSHA1 HashState;
	int32 Length = Result.Len();
	HashState.Update((const uint8*)&Length, sizeof(int32));

	auto ResultSrc = StringCast<UCS2CHAR>(*Result);
	uint32 CRCofPayload(FCrc::MemCrc32(ResultSrc.Get(), Length * sizeof(UCS2CHAR)));

	HashState.Update((const uint8*)&CRCofPayload, sizeof(uint32));
	HashState.Update((const uint8*)ResultSrc.Get(), Length * sizeof(UCS2CHAR));

	HashState.Final();
	uint8 Hash[FSHA1::DigestSize];
	HashState.GetHash(Hash);
	FString HashString = BytesToHex(Hash, FSHA1::DigestSize);

	int32 HashStringSize = HashString.Len();
	int32 OriginalPart = MAX_BACKEND_KEY_LENGTH - HashStringSize - 2;
	Result = Result.Left(OriginalPart) + TEXT("__") + HashString;
	check(Result.Len() == MAX_BACKEND_KEY_LENGTH && Result.Len() > 0);
	return true;
}

uint32 FVirtualTextureDataChunk::StoreInDerivedDataCache(const FString& InDerivedDataKey)
{
	int32 BulkDataSizeInBytes = BulkData.GetBulkDataSize();
	check(BulkDataSizeInBytes > 0);

	TArray<uint8> DerivedData;
	FMemoryWriter Ar(DerivedData, /*bIsPersistent=*/ true);
	Ar << BulkDataSizeInBytes;
	{
		void* BulkChunkData = BulkData.Lock(LOCK_READ_ONLY);
		Ar.Serialize(BulkChunkData, BulkDataSizeInBytes);
		BulkData.Unlock();
	}
	const uint32 Result = DerivedData.Num();
	GetDerivedDataCacheRef().Put(*InDerivedDataKey, DerivedData);
	DerivedDataKey = InDerivedDataKey;
	ShortenKey(DerivedDataKey, ShortDerivedDataKey);

	// remove the actual bulkdata so when we serialize the owning FVirtualTextureBuiltData, this is actually serializing only the meta data
	BulkData.RemoveBulkData();
	return Result;
}
#endif // WITH_EDITORONLY_DATA
