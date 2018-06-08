// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Misc/Compression.h"

#if HAS_OODLE_SDK
#include "oodle2.h"
#endif

#if HAS_OODLE_SDK

struct FOodleCustomCompressor : ICustomCompressor
{
	OodleLZ_Compressor Compressor;
	OodleLZ_CompressionLevel CompressionLevel;
	OodleLZ_CompressOptions CompressionOptions;

	FOodleCustomCompressor(OodleLZ_Compressor InCompressor, OodleLZ_CompressionLevel InCompressionLevel, int InSpaceSpeedTradeoffBytes)
	{
		Compressor = InCompressor;
		CompressionLevel = InCompressionLevel;
		CompressionOptions = *OodleLZ_CompressOptions_GetDefault(Compressor, CompressionLevel);
		CompressionOptions.spaceSpeedTradeoffBytes = InSpaceSpeedTradeoffBytes;
	}

	virtual bool Compress(void* CompressedBuffer, int32& CompressedSize, const void* UncompressedBuffer, int32 UncompressedSize)
	{
		int32 Result = (int32)OodleLZ_Compress(Compressor, UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressionLevel, &CompressionOptions);
		if (Result > 0)
		{
			CompressedSize = Result;
			return true;
		}
		return false;
	}

	virtual bool Uncompress(void* UncompressedBuffer, int32& UncompressedSize, const void* CompressedBuffer, int32 CompressedSize, int32 BitWindow = 0)
	{
		int32 Result = (int32)OodleLZ_Decompress(CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize, OodleLZ_FuzzSafe_No);
		if (Result > 0)
		{
			UncompressedSize = Result;
			return true;
		}
		return false;
	}

	virtual int32 GetCompressedBufferSize(int32 UncompressedSize)
	{
		return (int32)OodleLZ_GetCompressedBufferSizeNeeded(UncompressedSize);
	}
};

ICustomCompressor* CreateOodleCustomCompressor()
{
	return new FOodleCustomCompressor(OodleLZ_Compressor_Mermaid, OodleLZ_CompressionLevel_Optimal2, 256);
}

#endif
