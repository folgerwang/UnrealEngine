// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "ImageCore.h"
#endif

namespace CrunchCompression
{
#if WITH_EDITOR
	CRUNCHCOMPRESSION_API bool IsValidFormat(const FName& Format);
	CRUNCHCOMPRESSION_API bool Encode(const TArray<FImage>& UncompressedSrc, const FName& OutputFormat, TArray<uint8>& OutCodecPayload, TArray<uint8>& OutCompressedData, TArray< TPair<uint32, uint32>>& OutTileInfos);
#endif

	CRUNCHCOMPRESSION_API void* InitializeDecoderContext(uint8* HeaderData, size_t HeaderDataSize);
	CRUNCHCOMPRESSION_API bool Decode(void* Context, uint8* CompressedPixelData, uint32 Slice, uint8* OutUncompressedData, size_t OutDataSize, size_t OutUncompressedDataPitch);
	CRUNCHCOMPRESSION_API void DestroyDecoderContext(void* Context);
}
	
