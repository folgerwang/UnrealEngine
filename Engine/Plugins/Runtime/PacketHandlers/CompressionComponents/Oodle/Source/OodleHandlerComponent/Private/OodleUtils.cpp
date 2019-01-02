// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OodleUtils.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#if HAS_OODLE_SDK
#include "oodle2.h"
#endif

#if HAS_OODLE_SDK

namespace OodleUtils
{
	bool DecompressReplayData(const TArray<uint8>& InCompressed, TArray< uint8 >& OutBuffer)
	{
		int Size = 0;
		int CompressedSize = 0;

		FMemoryReader Reader(InCompressed);
		Reader << Size;
		Reader << CompressedSize;

		OutBuffer.SetNum(Size, false);

		return (OodleLZ_Decompress(InCompressed.GetData() + Reader.Tell(), (SINTa)CompressedSize, OutBuffer.GetData(), OutBuffer.Num()) == OutBuffer.Num());
	}

	bool CompressReplayData(const TArray<uint8>& InBuffer, TArray< uint8 >& OutCompressed)
	{
		int Size = InBuffer.Num();
		int CompressedSize = 0;

		FMemoryWriter Writer(OutCompressed);
		Writer << Size;
		Writer << CompressedSize;		// Make space

		int64 ReservedBytes = Writer.Tell();

		OutCompressed.SetNum(ReservedBytes + OodleLZ_GetCompressedBufferSizeNeeded(InBuffer.Num()));

		const OodleLZ_Compressor			Compressor = OodleLZ_Compressor_LZB16;
		const OodleLZ_CompressionLevel		Level = OodleLZ_CompressionLevel_VeryFast;

		CompressedSize = OodleLZ_Compress(Compressor, InBuffer.GetData(), (SINTa)InBuffer.Num(), OutCompressed.GetData() + ReservedBytes, Level);

		if (CompressedSize == OODLELZ_FAILED)
		{
			return false;
		}

		Writer.Seek(0);
		Writer << Size;
		Writer << CompressedSize;	// Save compressed size for real

		OutCompressed.SetNum(CompressedSize + ReservedBytes, false);

		return true;
	}
};

#endif