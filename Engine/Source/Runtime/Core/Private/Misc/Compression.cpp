// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Misc/Compression.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Logging/LogMacros.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Stats/Stats.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CompressedGrowableBuffer.h"
#include "Misc/ICompressionFormat.h"

// #include "TargetPlatformBase.h"
THIRD_PARTY_INCLUDES_START
#include "ThirdParty/zlib/zlib-1.2.5/Inc/zlib.h"
THIRD_PARTY_INCLUDES_END

DECLARE_LOG_CATEGORY_EXTERN(LogCompression, Log, All);
DEFINE_LOG_CATEGORY(LogCompression);

DECLARE_STATS_GROUP( TEXT( "Compression" ), STATGROUP_Compression, STATCAT_Advanced );

TMap<FName, struct ICompressionFormat*> FCompression::CompressionFormats;


static void *zalloc(void *opaque, unsigned int size, unsigned int num)
{
	return FMemory::Malloc(size * num);
}

static void zfree(void *opaque, void *p)
{
	FMemory::Free(p);
}


/**
 * Thread-safe abstract compression routine. Compresses memory from uncompressed buffer and writes it to compressed
 * buffer. Updates CompressedSize with size of compressed data.
 *
 * @param	CompressedBuffer			Buffer compressed data is going to be written to
 * @param	CompressedSize	[in/out]	Size of CompressedBuffer, at exit will be size of compressed data
 * @param	UncompressedBuffer			Buffer containing uncompressed data
 * @param	UncompressedSize			Size of uncompressed data in bytes
 * @param	BitWindow					Bit window to use in compression
 * @return true if compression succeeds, false if it fails because CompressedBuffer was too small or other reasons
 */
static bool appCompressMemoryZLIB(void* CompressedBuffer, int32& CompressedSize, const void* UncompressedBuffer, int32 UncompressedSize, int32 BitWindow, int32 CompLevel)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Compress Memory ZLIB"), STAT_appCompressMemoryZLIB, STATGROUP_Compression);

	ensureMsgf(CompLevel >= Z_DEFAULT_COMPRESSION, TEXT("CompLevel must be >= Z_DEFAULT_COMPRESSION"));
	ensureMsgf(CompLevel <= Z_BEST_COMPRESSION, TEXT("CompLevel must be <= Z_BEST_COMPRESSION"));

	CompLevel = FMath::Clamp(CompLevel, Z_DEFAULT_COMPRESSION, Z_BEST_COMPRESSION);

	// Zlib wants to use unsigned long.
	unsigned long ZCompressedSize = CompressedSize;
	unsigned long ZUncompressedSize = UncompressedSize;
	bool bOperationSucceeded = false;

	// Compress data
	// If using the default Zlib bit window, use the zlib routines, otherwise go manual with deflate2
	if (BitWindow == 0 || BitWindow == DEFAULT_ZLIB_BIT_WINDOW)
	{
		bOperationSucceeded = compress2((uint8*)CompressedBuffer, &ZCompressedSize, (const uint8*)UncompressedBuffer, ZUncompressedSize, CompLevel) == Z_OK ? true : false;
	}
	else
	{
		z_stream stream;
		stream.next_in = (Bytef*)UncompressedBuffer;
		stream.avail_in = (uInt)ZUncompressedSize;
		stream.next_out = (Bytef*)CompressedBuffer;
		stream.avail_out = (uInt)ZCompressedSize;
		stream.zalloc = &zalloc;
		stream.zfree = &zfree;
		stream.opaque = Z_NULL;

		if (ensure(Z_OK == deflateInit2(&stream, CompLevel, Z_DEFLATED, BitWindow, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY)))
		{
			if (ensure(Z_STREAM_END == deflate(&stream, Z_FINISH)))
			{
				ZCompressedSize = stream.total_out;
				if (ensure(Z_OK == deflateEnd(&stream)))
				{
					bOperationSucceeded = true;
				}
			}
			else
			{
				deflateEnd(&stream);
			}
		}
	}

	// Propagate compressed size from intermediate variable back into out variable.
	CompressedSize = ZCompressedSize;
	return bOperationSucceeded;
}

static bool appCompressMemoryGZIP(void* CompressedBuffer, int32& CompressedSize, const void* UncompressedBuffer, int32 UncompressedSize)
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "Compress Memory GZIP" ), STAT_appCompressMemoryGZIP, STATGROUP_Compression );

	z_stream gzipstream;
	gzipstream.zalloc = &zalloc;
	gzipstream.zfree = &zfree;
	gzipstream.opaque = Z_NULL;

	// Setup input buffer
	gzipstream.next_in = (uint8*)UncompressedBuffer;
	gzipstream.avail_in = UncompressedSize;

	// Init deflate settings to use GZIP
	int windowsBits = 15;
	int GZIP_ENCODING = 16;
	deflateInit2(
		&gzipstream,
		Z_DEFAULT_COMPRESSION,
		Z_DEFLATED,
		windowsBits | GZIP_ENCODING,
		MAX_MEM_LEVEL,
		Z_DEFAULT_STRATEGY);

	// Setup output buffer
	const unsigned long GzipHeaderLength = 12;
	// This is how much memory we may need, however the consumer is allocating memory for us without knowing the required length.
	//unsigned long CompressedMaxSize = deflateBound(&gzipstream, gzipstream.avail_in) + GzipHeaderLength;
	gzipstream.next_out = (uint8*)CompressedBuffer;
	gzipstream.avail_out = UncompressedSize;

	int status = 0;
	bool bOperationSucceeded = false;
	while ((status = deflate(&gzipstream, Z_FINISH)) == Z_OK);
	if (status == Z_STREAM_END)
	{
		bOperationSucceeded = true;
		deflateEnd(&gzipstream);
	}

	// Propagate compressed size from intermediate variable back into out variable.
	CompressedSize = gzipstream.total_out;
	return bOperationSucceeded;
}

/**
 * Thread-safe abstract compression routine. Compresses memory from uncompressed buffer and writes it to compressed
 * buffer. Updates CompressedSize with size of compressed data.
 *
 * @param	UncompressedBuffer			Buffer containing uncompressed data
 * @param	UncompressedSize			Size of uncompressed data in bytes
 * @param	CompressedBuffer			Buffer compressed data is going to be read from
 * @param	CompressedSize				Size of CompressedBuffer data in bytes
 * @return true if compression succeeds, false if it fails because CompressedBuffer was too small or other reasons
 */
bool appUncompressMemoryZLIB( void* UncompressedBuffer, int32 UncompressedSize, const void* CompressedBuffer, int32 CompressedSize, int32 BitWindow )
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "Uncompress Memory ZLIB" ), STAT_appUncompressMemoryZLIB, STATGROUP_Compression );

	// Zlib wants to use unsigned long.
	unsigned long ZCompressedSize	= CompressedSize;
	unsigned long ZUncompressedSize	= UncompressedSize;
	
	z_stream stream;
	stream.zalloc = &zalloc;
	stream.zfree = &zfree;
	stream.opaque = Z_NULL;
	stream.next_in = (uint8*)CompressedBuffer;
	stream.avail_in = ZCompressedSize;
	stream.next_out = (uint8*)UncompressedBuffer;
	stream.avail_out = ZUncompressedSize;

	if (BitWindow == 0)
	{
		BitWindow = DEFAULT_ZLIB_BIT_WINDOW;
	}

	int32 Result = inflateInit2(&stream, BitWindow);

	if(Result != Z_OK)
		return false;

	// Uncompress data.
	Result = inflate(&stream, Z_FINISH);
	if(Result == Z_STREAM_END)
	{
		ZUncompressedSize = stream.total_out;
	}

	int32 EndResult = inflateEnd(&stream);
	if (Result >= Z_OK)
	{
		Result = EndResult;
	}

	// These warnings will be compiled out in shipping.
	UE_CLOG(Result == Z_MEM_ERROR, LogCompression, Warning, TEXT("appUncompressMemoryZLIB failed: Error: Z_MEM_ERROR, not enough memory!"));
	UE_CLOG(Result == Z_BUF_ERROR, LogCompression, Warning, TEXT("appUncompressMemoryZLIB failed: Error: Z_BUF_ERROR, not enough room in the output buffer!"));
	UE_CLOG(Result == Z_DATA_ERROR, LogCompression, Warning, TEXT("appUncompressMemoryZLIB failed: Error: Z_DATA_ERROR, input data was corrupted or incomplete!"));

	bool bOperationSucceeded = (Result == Z_OK);

	// Sanity check to make sure we uncompressed as much data as we expected to.
	if( UncompressedSize != ZUncompressedSize )
	{
		UE_LOG( LogCompression, Warning, TEXT("appUncompressMemoryZLIB failed: Mismatched uncompressed size. Expected: %d, Got:%d. Result: %d"), UncompressedSize, ZUncompressedSize, Result );
		bOperationSucceeded = false;
	}
	return bOperationSucceeded;
}

/** Time spent compressing data in cycles. */
TAtomic<uint64> FCompression::CompressorTimeCycles(0);
/** Number of bytes before compression.		*/
TAtomic<uint64> FCompression::CompressorSrcBytes(0);
/** Nubmer of bytes after compression.		*/
TAtomic<uint64> FCompression::CompressorDstBytes(0);

static ECompressionFlags CheckGlobalCompressionFlags(ECompressionFlags Flags)
{
	static bool GAlwaysBiasCompressionForSize = false;
	if(FPlatformProperties::HasEditorOnlyData())
	{
		static bool GTestedCmdLine = false;
		if(!GTestedCmdLine && FCommandLine::IsInitialized())
		{
			GTestedCmdLine = true;
			// Override compression settings wrt size.
			GAlwaysBiasCompressionForSize = FParse::Param(FCommandLine::Get(),TEXT("BIASCOMPRESSIONFORSIZE"));
		}
	}

	// Always bias for speed if option is set.
	if(GAlwaysBiasCompressionForSize)
	{
		int32 NewFlags = Flags;
		NewFlags &= ~COMPRESS_BiasSpeed;
		NewFlags |= COMPRESS_BiasMemory;
		Flags = (ECompressionFlags)NewFlags;
	}

	return Flags;
}



ICompressionFormat* FCompression::GetCompressionFormat(FName FormatName, bool bErrorOnFailure)
{
	ICompressionFormat** ExistingFormat = CompressionFormats.Find(FormatName);
	if (ExistingFormat == nullptr)
	{
		TArray<ICompressionFormat*> Features = IModularFeatures::Get().GetModularFeatureImplementations<ICompressionFormat>(COMPRESSION_FORMAT_FEATURE_NAME);

		for (ICompressionFormat* CompressionFormat : Features)
		{
			// is this format the right one?
			if (CompressionFormat->GetCompressionFormatName() == FormatName)
			{
				// remember it in our format map
				ExistingFormat = &CompressionFormats.Add(FormatName, CompressionFormat);
				break;
			}
		}

		if (ExistingFormat == nullptr)
		{
			if (bErrorOnFailure)
			{
				UE_LOG(LogCompression, Error, TEXT("FCompression::GetCompressionFormat - Unable to find a module or plugin for compression format %s"), *FormatName.ToString());
			}
			else
			{
				UE_LOG(LogCompression, Display, TEXT("FCompression::GetCompressionFormat - Unable to find a module or plugin for compression format %s"), *FormatName.ToString());
			}
			return nullptr;
		}
	}

	return *ExistingFormat;

}

FName FCompression::GetCompressionFormatFromDeprecatedFlags(ECompressionFlags Flags)
{
	switch (Flags & COMPRESS_DeprecatedFormatFlagsMask)
	{
		case COMPRESS_ZLIB:
			return NAME_Zlib;
		case COMPRESS_GZIP:
			return NAME_Gzip;
		// COMPRESS_Custom was a temporary solution to third party compression before we had plugins working, and it was only ever used with oodle, we just assume Oodle with Custom
		case COMPRESS_Custom:
			return TEXT("Oodle");
	}

	return NAME_None;
}


int32 FCompression::CompressMemoryBound(FName FormatName, int32 UncompressedSize, ECompressionFlags Flags, int32 CompressionData)
{
	int32 CompressionBound = UncompressedSize;

	if (FormatName == NAME_Zlib)
	{
		// Zlib's compressBounds gives a better (smaller) value, but only for the default bit window.
		if (CompressionData == 0 || CompressionData == DEFAULT_ZLIB_BIT_WINDOW)
		{
			CompressionBound = compressBound(UncompressedSize);
		}
		else
		{
			// Calculate pessimistic bounds for compression. This value is calculated based on the algorithm used in deflate2.
			CompressionBound = UncompressedSize + ((UncompressedSize + 7) >> 3) + ((UncompressedSize + 63) >> 6) + 5 + 6;
		}
	}
	else if (FormatName == NAME_Gzip)
	{
		// CompressionBound = deflateBound(&gzipstream, gzipstream.avail_in) + GzipHeaderLength;
		UE_LOG(LogCompression, Fatal, TEXT("FCompression::CompressMemoryBound - GZip is not supported yet"));
	}
	else
	{
		ICompressionFormat* Format = GetCompressionFormat(FormatName);
		if (Format)
		{
			CompressionBound = Format->GetCompressedBufferSize(UncompressedSize, CompressionData);
		}
	}

	return CompressionBound;
}

bool FCompression::CompressMemory(FName FormatName, void* CompressedBuffer, int32& CompressedSize, const void* UncompressedBuffer, int32 UncompressedSize, ECompressionFlags Flags, int32 CompressionData)
{
	uint64 CompressorStartTime = FPlatformTime::Cycles64();

	bool bCompressSucceeded = false;

	Flags = CheckGlobalCompressionFlags(Flags);

	if (FormatName == NAME_Zlib)
	{
		// hardcoded zlib
		bCompressSucceeded = appCompressMemoryZLIB(CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize, CompressionData, Z_DEFAULT_COMPRESSION);
	}
	else if (FormatName == NAME_Gzip)
	{
		// hardcoded gzip
		bCompressSucceeded = appCompressMemoryGZIP(CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize);
	}
	else
	{
		// let the format module compress it
		ICompressionFormat* Format = GetCompressionFormat(FormatName);
		if (Format)
		{
			bCompressSucceeded = Format->Compress(CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize, CompressionData);
		}
	}

	// Keep track of compression time and stats.
	CompressorTimeCycles += FPlatformTime::Cycles64() - CompressorStartTime;
	if( bCompressSucceeded )
	{
		CompressorSrcBytes += UncompressedSize;
		CompressorDstBytes += CompressedSize;
	}

	return bCompressSucceeded;
}

DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Uncompressor total time"),STAT_UncompressorTime,STATGROUP_Compression);

bool FCompression::UncompressMemory(FName FormatName, void* UncompressedBuffer, int32 UncompressedSize, const void* CompressedBuffer, int32 CompressedSize, ECompressionFlags Flags, int32 CompressionData)
{
	SCOPED_NAMED_EVENT(FCompression_UncompressMemory, FColor::Cyan);
	// Keep track of time spent uncompressing memory.
	STAT(double UncompressorStartTime = FPlatformTime::Seconds();)
	
	bool bUncompressSucceeded = false;

	if (FormatName == NAME_Zlib)
	{
		// hardcoded zlib
		bUncompressSucceeded = appUncompressMemoryZLIB(UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressedSize, CompressionData);
	}
	else if (FormatName == NAME_Gzip)
	{
		// @todo buh?
	}
	else
	{
		// let the format module compress it
		ICompressionFormat* Format = GetCompressionFormat(FormatName);
		if (Format)
		{
			bUncompressSucceeded = Format->Uncompress(UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressedSize, CompressionData);
		}
	}

	if (!bUncompressSucceeded)
	{
		// This is only to skip serialization errors caused by asset corruption 
		// that can be fixed during re-save, should never be disabled by default!
		static struct FFailOnUncompressErrors
		{
			bool Value;
			FFailOnUncompressErrors()
				: Value(true) // fail by default
			{
				GConfig->GetBool(TEXT("Core.System"), TEXT("FailOnUncompressErrors"), Value, GEngineIni);
			}
		} FailOnUncompressErrors;
		if (!FailOnUncompressErrors.Value)
		{
			bUncompressSucceeded = true;
		}
		// Always log an error
		UE_LOG(LogCompression, Error, TEXT("FCompression::UncompressMemory - Failed to uncompress memory (%d/%d) using format %s, this may indicate the asset is corrupt!"), CompressedSize, UncompressedSize, *FormatName.ToString());
	}

#if	STATS
	if (FThreadStats::IsThreadingReady())
	{
		INC_FLOAT_STAT_BY( STAT_UncompressorTime, (float)(FPlatformTime::Seconds() - UncompressorStartTime) )
	}
#endif // STATS
	
	return bUncompressSucceeded;
}

/*-----------------------------------------------------------------------------
	FCompressedGrowableBuffer.
-----------------------------------------------------------------------------*/

/**
 * Constructor
 *
 * @param	InMaxPendingBufferSize	Max chunk size to compress in uncompressed bytes
 * @param	InCompressionFlags		Compression flags to compress memory with
 */
FCompressedGrowableBuffer::FCompressedGrowableBuffer(FCompressedGrowableBuffer::EVS2015Redirector, int32 InMaxPendingBufferSize, ECompressionFlags InCompressionFlags)
	: FCompressedGrowableBuffer(InMaxPendingBufferSize, FCompression::GetCompressionFormatFromDeprecatedFlags(InCompressionFlags), InCompressionFlags)
{

}

FCompressedGrowableBuffer::FCompressedGrowableBuffer( int32 InMaxPendingBufferSize, FName InCompressionFormat, ECompressionFlags InCompressionFlags )
:	MaxPendingBufferSize( InMaxPendingBufferSize )
,	CompressionFormat( InCompressionFormat )
,	CompressionFlags(InCompressionFlags)
,	CurrentOffset( 0 )
,	NumEntries( 0 )
,	DecompressedBufferBookKeepingInfoIndex( INDEX_NONE )
{
	PendingCompressionBuffer.Empty( MaxPendingBufferSize );
}

/**
 * Locks the buffer for reading. Needs to be called before calls to Access and needs
 * to be matched up with Unlock call.
 */
void FCompressedGrowableBuffer::Lock()
{
	check( DecompressedBuffer.Num() == 0 );
}

/**
 * Unlocks the buffer and frees temporary resources used for accessing.
 */
void FCompressedGrowableBuffer::Unlock()
{
	DecompressedBuffer.Empty();
	DecompressedBufferBookKeepingInfoIndex = INDEX_NONE;
}

/**
 * Appends passed in data to the buffer. The data needs to be less than the max
 * pending buffer size. The code will assert on this assumption.
 *
 * @param	Data	Data to append
 * @param	Size	Size of data in bytes.
 * @return	Offset of data, used for retrieval later on
 */
int32 FCompressedGrowableBuffer::Append( void* Data, int32 Size )
{
	check( DecompressedBuffer.Num() == 0 );
	check( Size <= MaxPendingBufferSize );
	NumEntries++;

	// Data does NOT fit into pending compression buffer. Compress existing data 
	// and purge buffer.
	if( MaxPendingBufferSize - PendingCompressionBuffer.Num() < Size )
	{
		// Allocate temporary buffer to hold compressed data. It is bigger than the uncompressed size as
		// compression is not guaranteed to create smaller data and we don't want to handle that case so 
		// we simply assert if it doesn't fit. For all practical purposes this works out fine and is what
		// other code in the engine does as well.
		int32 CompressedSize = MaxPendingBufferSize * 4 / 3;
		void* TempBuffer = FMemory::Malloc( CompressedSize );

		// Compress the memory. CompressedSize is [in/out]
		verify( FCompression::CompressMemory( CompressionFormat, TempBuffer, CompressedSize, PendingCompressionBuffer.GetData(), PendingCompressionBuffer.Num(), CompressionFlags ) );

		// Append the compressed data to the compressed buffer and delete temporary data.
		int32 StartIndex = CompressedBuffer.AddUninitialized( CompressedSize );
		FMemory::Memcpy( &CompressedBuffer[StartIndex], TempBuffer, CompressedSize );
		FMemory::Free( TempBuffer );

		// Keep track of book keeping info for later access to data.
		FBufferBookKeeping Info;
		Info.CompressedOffset = StartIndex;
		Info.CompressedSize = CompressedSize;
		Info.UncompressedOffset = CurrentOffset - PendingCompressionBuffer.Num();
		Info.UncompressedSize = PendingCompressionBuffer.Num();
		BookKeepingInfo.Add( Info ); 

		// Resize & empty the pending buffer to the default state.
		PendingCompressionBuffer.Empty( MaxPendingBufferSize );
	}

	// Appends the data to the pending buffer. The pending buffer is compressed
	// as needed above.
	int32 StartIndex = PendingCompressionBuffer.AddUninitialized( Size );
	FMemory::Memcpy( &PendingCompressionBuffer[StartIndex], Data, Size );

	// Return start offset in uncompressed memory.
	int32 StartOffset = CurrentOffset;
	CurrentOffset += Size;
	return StartOffset;
}

/**
 * Accesses the data at passed in offset and returns it. The memory is read-only and
 * memory will be freed in call to unlock. The lifetime of the data is till the next
 * call to Unlock, Append or Access
 *
 * @param	Offset	Offset to return corresponding data for
 */
void* FCompressedGrowableBuffer::Access( int32 Offset )
{
	void* UncompressedData = NULL;

	// Check whether the decompressed data is already cached.
	if( DecompressedBufferBookKeepingInfoIndex != INDEX_NONE )
	{
		const FBufferBookKeeping& Info = BookKeepingInfo[DecompressedBufferBookKeepingInfoIndex];
		// Cache HIT.
		if( (Info.UncompressedOffset <= Offset) && (Info.UncompressedOffset + Info.UncompressedSize > Offset) )
		{
			// Figure out index into uncompressed data and set it. DecompressionBuffer (return value) is going 
			// to be valid till the next call to Access or Unlock.
			int32 InternalOffset = Offset - Info.UncompressedOffset;
			UncompressedData = &DecompressedBuffer[InternalOffset];
		}
		// Cache MISS.
		else
		{
			DecompressedBufferBookKeepingInfoIndex = INDEX_NONE;
		}
	}

	// Traverse book keeping info till we find the matching block.
	if( UncompressedData == NULL )
	{
		for( int32 InfoIndex=0; InfoIndex<BookKeepingInfo.Num(); InfoIndex++ )
		{
			const FBufferBookKeeping& Info = BookKeepingInfo[InfoIndex];
			if( (Info.UncompressedOffset <= Offset) && (Info.UncompressedOffset + Info.UncompressedSize > Offset) )
			{
				// Found the right buffer, now decompress it.
				DecompressedBuffer.Empty( Info.UncompressedSize );
				DecompressedBuffer.AddUninitialized( Info.UncompressedSize );
				verify( FCompression::UncompressMemory( CompressionFormat, DecompressedBuffer.GetData(), Info.UncompressedSize, &CompressedBuffer[Info.CompressedOffset], Info.CompressedSize, CompressionFlags ) );

				// Figure out index into uncompressed data and set it. DecompressionBuffer (return value) is going 
				// to be valid till the next call to Access or Unlock.
				int32 InternalOffset = Offset - Info.UncompressedOffset;
				UncompressedData = &DecompressedBuffer[InternalOffset];	

				// Keep track of buffer index for the next call to this function.
				DecompressedBufferBookKeepingInfoIndex = InfoIndex;
				break;
			}
		}
	}

	// If we still haven't found the data it might be in the pending compression buffer.
	if( UncompressedData == NULL )
	{
		int32 UncompressedStartOffset = CurrentOffset - PendingCompressionBuffer.Num();
		if( (UncompressedStartOffset <= Offset) && (CurrentOffset > Offset) )
		{
			// Figure out index into uncompressed data and set it. PendingCompressionBuffer (return value) 
			// is going to be valid till the next call to Access, Unlock or Append.
			int32 InternalOffset = Offset - UncompressedStartOffset;
			UncompressedData = &PendingCompressionBuffer[InternalOffset];
		}
	}

	// Return value is only valid till next call to Access, Unlock or Append!
	check( UncompressedData );
	return UncompressedData;
}

bool FCompression::IsFormatValid(FName FormatName)
{
	// build in formats are always valid
	if (FormatName == NAME_Zlib || FormatName == NAME_Gzip)
	{
		return true;
	}

	// otherwise, if we can get the format class, we are good!
	return GetCompressionFormat(FormatName, false) != nullptr;
}

bool FCompression::VerifyCompressionFlagsValid(int32 InCompressionFlags)
{
	const int32 CompressionFlagsMask = COMPRESS_DeprecatedFormatFlagsMask | COMPRESS_OptionsFlagsMask;
	if (InCompressionFlags & (~CompressionFlagsMask))
	{
		return false;
	}
	// @todo: check the individual flags here
	return true;
}



/***********************
  Deprecated functions
***********************/

int32 FCompression::CompressMemoryBound(ECompressionFlags Flags, int32 UncompressedSize, int32 BitWindow)
{
	switch (Flags & COMPRESS_DeprecatedFormatFlagsMask)
	{
		case COMPRESS_ZLIB:
			return CompressMemoryBound(NAME_Zlib, UncompressedSize, (ECompressionFlags)(Flags & COMPRESS_OptionsFlagsMask), BitWindow);
		case COMPRESS_GZIP:
			return CompressMemoryBound(NAME_Gzip, UncompressedSize, (ECompressionFlags)(Flags & COMPRESS_OptionsFlagsMask));
		case COMPRESS_Custom:
			return CompressMemoryBound(TEXT("Oodle"), UncompressedSize, (ECompressionFlags)(Flags & COMPRESS_OptionsFlagsMask));
	}

	return 0;
}

bool FCompression::CompressMemory(ECompressionFlags Flags, void* CompressedBuffer, int32& CompressedSize, const void* UncompressedBuffer, int32 UncompressedSize, int32 BitWindow)
{
	switch (Flags & COMPRESS_DeprecatedFormatFlagsMask)
	{
		case COMPRESS_ZLIB:
			return CompressMemory(NAME_Zlib, CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize, (ECompressionFlags)(Flags & COMPRESS_OptionsFlagsMask), BitWindow);
		case COMPRESS_GZIP:
			return CompressMemory(NAME_Gzip, CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize, (ECompressionFlags)(Flags & COMPRESS_OptionsFlagsMask));
		case COMPRESS_Custom:
			return CompressMemory(TEXT("Oodle"), CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize, (ECompressionFlags)(Flags & COMPRESS_OptionsFlagsMask));
	}

	return false;
}

bool FCompression::UncompressMemory(ECompressionFlags Flags, void* UncompressedBuffer, int32 UncompressedSize, const void* CompressedBuffer, int32 CompressedSize, bool bIsSourcePadded, int32 BitWindow)
{
	if (bIsSourcePadded)
	{
		Flags = (ECompressionFlags)(Flags | COMPRESS_SourceIsPadded);
	}

	switch (Flags & COMPRESS_DeprecatedFormatFlagsMask)
	{
		case COMPRESS_ZLIB:
			return UncompressMemory(NAME_Zlib, UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressedSize, (ECompressionFlags)(Flags & COMPRESS_OptionsFlagsMask), BitWindow);
		case COMPRESS_GZIP:
			return UncompressMemory(NAME_Gzip, UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressedSize, (ECompressionFlags)(Flags & COMPRESS_OptionsFlagsMask));
		case COMPRESS_Custom:
			return UncompressMemory(TEXT("Oodle"), UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressedSize, (ECompressionFlags)(Flags & COMPRESS_OptionsFlagsMask));
	}

	return false;
}

