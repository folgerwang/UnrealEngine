// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Atomic.h"
#include "Misc/CompressionFlags.h"

// Define global current platform default to current platform.  
// DEPRECATED, USE NAME_Zlib
#define COMPRESS_Default			COMPRESS_ZLIB

/**
 * Chunk size serialization code splits data into. The loading value CANNOT be changed without resaving all
 * compressed data which is why they are split into two separate defines.
 */
#define LOADING_COMPRESSION_CHUNK_SIZE_PRE_369  32768
#define LOADING_COMPRESSION_CHUNK_SIZE			131072
#define SAVING_COMPRESSION_CHUNK_SIZE			LOADING_COMPRESSION_CHUNK_SIZE

struct FCompression
{
	/** Time spent compressing data in cycles. */
	CORE_API static TAtomic<uint64> CompressorTimeCycles;
	/** Number of bytes before compression.		*/
	CORE_API static TAtomic<uint64> CompressorSrcBytes;
	/** Number of bytes after compression.		*/
	CORE_API static TAtomic<uint64> CompressorDstBytes;

	/**
	 * Thread-safe abstract compression routine to query memory requirements for a compression operation.
	 *
	 * @param	Flags						Flags to control what method to use and optionally control memory vs speed
	 * @param	UncompressedSize			Size of uncompressed data in bytes
	 * @param	BitWindow					Bit window to use in compression
	 * @return The maximum possible bytes needed for compression of data buffer of size UncompressedSize
	 */
	CORE_API static int32 CompressMemoryBound(FName FormatName, int32 UncompressedSize, ECompressionFlags Flags=COMPRESS_NoFlags, int32 CompressionData=0);

	/**
	 * Thread-safe abstract compression routine. Compresses memory from uncompressed buffer and writes it to compressed
	 * buffer. Updates CompressedSize with size of compressed data. Compression controlled by the passed in flags.
	 *
	 * @param	Flags						Flags to control what method to use and optionally control memory vs speed
	 * @param	CompressedBuffer			Buffer compressed data is going to be written to
	 * @param	CompressedSize	[in/out]	Size of CompressedBuffer, at exit will be size of compressed data
	 * @param	UncompressedBuffer			Buffer containing uncompressed data
	 * @param	UncompressedSize			Size of uncompressed data in bytes
	 * @param	BitWindow					Bit window to use in compression
	 * @return true if compression succeeds, false if it fails because CompressedBuffer was too small or other reasons
	 */
	CORE_API static bool CompressMemory(FName FormatName, void* CompressedBuffer, int32& CompressedSize, const void* UncompressedBuffer, int32 UncompressedSize, ECompressionFlags Flags=COMPRESS_NoFlags, int32 CompressionData=0);

	/**
	 * Thread-safe abstract decompression routine. Uncompresses memory from compressed buffer and writes it to uncompressed
	 * buffer. UncompressedSize is expected to be the exact size of the data after decompression.
	 *
	 * @param	Flags						Flags to control what method to use to decompress
	 * @param	UncompressedBuffer			Buffer containing uncompressed data
	 * @param	UncompressedSize			Size of uncompressed data in bytes
	 * @param	CompressedBuffer			Buffer compressed data is going to be read from
	 * @param	CompressedSize				Size of CompressedBuffer data in bytes
	 * @param	bIsSourcePadded		Whether the source memory is padded with a full cache line at the end
	 * @return true if compression succeeds, false if it fails because CompressedBuffer was too small or other reasons
	 */
	CORE_API static bool UncompressMemory(FName FormatName, void* UncompressedBuffer, int32 UncompressedSize, const void* CompressedBuffer, int32 CompressedSize, ECompressionFlags Flags=COMPRESS_NoFlags, int32 CompressionData=0);

	/**
	 * Checks to see if a format will be usable, so that a fallback can be used
	 * @param FormatName The name of the format to test
	 */
	CORE_API static bool IsFormatValid(FName FormatName);

	/**
	 * Verifies if the passed in value represents valid compression flags
	 * @param InCompressionFlags Value to test
	 */
	CORE_API static bool VerifyCompressionFlagsValid(int32 InCompressionFlags);





	UE_DEPRECATED(4.20, "Use the FName based version of CompressMemoryBound")
	CORE_API static int32 CompressMemoryBound(ECompressionFlags Flags, int32 UncompressedSize, int32 BitWindow = DEFAULT_ZLIB_BIT_WINDOW);
	UE_DEPRECATED(4.20, "Use the FName based version of CompressMemory")
	CORE_API static bool CompressMemory(ECompressionFlags Flags, void* CompressedBuffer, int32& CompressedSize, const void* UncompressedBuffer, int32 UncompressedSize, int32 BitWindow = DEFAULT_ZLIB_BIT_WINDOW);
	UE_DEPRECATED(4.20, "Use the FName based version of UncompressMemory")
	CORE_API static bool UncompressMemory(ECompressionFlags Flags, void* UncompressedBuffer, int32 UncompressedSize, const void* CompressedBuffer, int32 CompressedSize, bool bIsSourcePadded = false, int32 BitWindow = DEFAULT_ZLIB_BIT_WINDOW);

	CORE_API static FName GetCompressionFormatFromDeprecatedFlags(ECompressionFlags DeprecatedFlags);
	
private:
	
	/**
	 * Find a compression format module by name, returning nullptr if no module found
	 */
	static struct ICompressionFormat* GetCompressionFormat(FName Method, bool bErrorOnFailure=true);

	/** Mapping of Compression FNames to their compressor objects */
	static TMap<FName, struct ICompressionFormat*> CompressionFormats;
};


