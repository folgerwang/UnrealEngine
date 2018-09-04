// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimEncoding_PerTrackCompression.cpp: Per-track decompressor
=============================================================================*/ 

#include "AnimEncoding_PerTrackCompression.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Animation/AnimEncodingHeapAllocator.h"
#include "Animation/AnimEncodingDecompressionContext.h"
#include "AnimationCompression.h"


class FAEPerTrackKeyLerpContext : public FAnimEncodingDecompressionContext
{
public:
	static constexpr int32 OffsetNumKeysPairSize = sizeof(uint32) + sizeof(uint16);

	FAEPerTrackKeyLerpContext(const FAnimSequenceDecompressionContext& DecompContext);
	virtual void Seek(const FAnimSequenceDecompressionContext& DecompContext, float SampleAtTime) override;

	inline void GetRotation(FTransform& OutAtom, const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) const;
	inline void GetTranslation(FTransform& OutAtom, const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) const;
	inline void GetScale(FTransform& OutAtom, const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) const;

	// Common
	TArray<int32, FAnimEncodingHeapAllocator> RangeOffsets[2];
	int32 PerTrackStreamFlagOffsets[2];
	int32 RangeDataSize[2];
	uint16 PreviousSegmentIndex[2];

	// Uniform
	TArray<int32, FAnimEncodingHeapAllocator> UniformKeyOffsets[2];
	int32 UniformKeyFrameSize[2];
	int32 UniformDataOffsets[2];

	// Variable common
	TArray<uint8, FAnimEncodingHeapAllocator> TrackStreamKeySizes[2];

	// Variable linear
	float SegmentRelativePos0;
	uint8 TimeMarkerSize[2];	// sizeof(uint8) or sizeof(uint16)
	int32 OffsetNumKeysPairOffsets[2];
	TArray<int32, FAnimEncodingHeapAllocator> NumAnimatedTrackStreams[2];

	// Variable sorted
	struct FCachedKey
	{
		// Linear interpolation only requires 2 keys
		// Index 0 is always the oldest key, index 1 the newest
		int32 RotOffsets[2];
		int32 RotFrameIndices[2];
		int32 TransOffsets[2];
		int32 TransFrameIndices[2];
		int32 ScaleOffsets[2];
		int32 ScaleFrameIndices[2];
	};

	TArray<FCachedKey, FAnimEncodingHeapAllocator> CachedKeys;			// 1 Entry per track

	int32 SegmentStartFrame[2];
	float FramePos;
	float PreviousSampleAtTime;

	const uint8_t* PackedSampleData;		// The current pointer into our data stream
	int32 PreviousFrameIndex;				// The previously read frame index
	int32 CurrentFrameIndex;				// The current frame index
	uint8 CurrentSegmentIndex;				// The current segment index of the sorted data, either 0 or 1

private:
	void CacheSegmentValues(const FAnimSequenceDecompressionContext& DecompContext, const FCompressedSegment& Segment, uint8 SegmentIndex);
	void ResetSortedCache(const FAnimSequenceDecompressionContext& DecompContext);
	void AdvanceSortedCachedKeys(const FAnimSequenceDecompressionContext& DecompContext);
};

// This define controls whether scalar or vector code is used to decompress keys.  Note that not all key decompression code
// is vectorized yet, so some (seldom used) formats will actually get slower (due to extra LHS stalls) when enabled.
// The code also relies on a flexible permute instruction being available (e.g., PPC vperm)
#define USE_VECTOR_PTC_DECOMPRESSOR 0

#if USE_VECTOR_PTC_DECOMPRESSOR

// 32767, stored in 1X, plus biasing is 0
// Perm_Zeros takes the first 4 bytes of the second argument (which should be VectorZero() for this table use)
#define Perm_Zeros 0x10111213

#define Perm_X1 0x00010203
#define Perm_Y1 0x04050607
#define Perm_Z1 0x08090A0B
#define Perm_W2 0x1C1D1E1F

// One float96 key can be stored with implicit 0.0f components, this table (when indexed by format flags 0-3) plus one indicates how many bytes a key contains
static const uint8 Float96KeyBytesMinusOne[16] = { 11, 3, 3, 7, 3, 7, 7, 11, 11, 3, 3, 7, 3, 7, 7, 11 };

// One fixed48 key can be stored with implicit 0.0f components, this table (when indexed by format flags 0-3) plus one indicates how many bytes a key contains
static const uint8 Fixed48KeyBytesMinusOne[16] = { 5, 1, 1, 3, 1, 3, 3, 5, 5, 1, 1, 3, 1, 3, 3, 5 };

// One float96 translation key can be stored with implicit 0.0f components, this table (when indexed by format flags 0-2) indicates how to swizzle the 16 bytes read into one float each in X,Y,Z, and 0.0f in W
static const VectorRegister Trans96OptionalFormatPermMasks[8] =
{
	MakeVectorRegister( Perm_X1, Perm_Y1, Perm_Z1, (uint32)Perm_W2 ),  // 0 = 7 = all three valid
	MakeVectorRegister( Perm_X1, Perm_W2, Perm_W2, (uint32)Perm_W2 ),  // 1 = 001 = X
	MakeVectorRegister( Perm_W2, Perm_X1, Perm_W2, (uint32)Perm_W2 ),  // 2 = 010 = Y
	MakeVectorRegister( Perm_X1, Perm_Y1, Perm_W2, (uint32)Perm_W2 ),  // 3 = 011 = XY
	MakeVectorRegister( Perm_W2, Perm_W2, Perm_X1, (uint32)Perm_W2 ),  // 4 = 100 = Z
	MakeVectorRegister( Perm_X1, Perm_W2, Perm_Y1, (uint32)Perm_W2 ),  // 5 = 101 = XZ
	MakeVectorRegister( Perm_W2, Perm_X1, Perm_Y1, (uint32)Perm_W2 ),  // 6 = 110 = YZ
	MakeVectorRegister( Perm_X1, Perm_Y1, Perm_Z1, (uint32)Perm_W2 ),  // 7 = 111 = XYZ
};

#undef Perm_Zeros
#undef Perm_X1
#undef Perm_Y1
#undef Perm_Z1
#undef Perm_W2

// Perm_Zeros takes the first 4 bytes of the second argument (which should be DecompressPTCConstants, resulting in integer 32767, which becomes 0.0f after scaling and biasing)
#define Perm_Zeros 0x10111213

// Each of these takes two bytes of source data, and two zeros from the W of the second argument (which should be DecompressPTCConstants) to pad it out to a 4 byte int32
#define Perm_Data1 0x1F1F0001
#define Perm_Data2 0x1F1F0203
#define Perm_Data3 0x1F1F0405

// One fixed48 rotation key can be stored with implicit 0.0f components, this table (when indexed by format flags 0-2) indicates how to swizzle the 16 bytes read into one short each in X,Y,Z)
static const VectorRegister Fix48FormatPermMasks[8] =
{
	MakeVectorRegister( Perm_Data1, Perm_Data2, Perm_Data3, (uint32)Perm_Zeros ),  // 0 = 7 = all three valid
	MakeVectorRegister( Perm_Data1, Perm_Zeros, Perm_Zeros, (uint32)Perm_Zeros ),  // 1 = 001 = X
	MakeVectorRegister( Perm_Zeros, Perm_Data1, Perm_Zeros, (uint32)Perm_Zeros ),  // 2 = 010 = Y
	MakeVectorRegister( Perm_Data1, Perm_Data2, Perm_Zeros, (uint32)Perm_Zeros ),  // 3 = 011 = XY
	MakeVectorRegister( Perm_Zeros, Perm_Zeros, Perm_Data1, (uint32)Perm_Zeros ),  // 4 = 100 = Z
	MakeVectorRegister( Perm_Data1, Perm_Zeros, Perm_Data2, (uint32)Perm_Zeros ),  // 5 = 101 = XZ
	MakeVectorRegister( Perm_Zeros, Perm_Data1, Perm_Data2, (uint32)Perm_Zeros ),  // 6 = 110 = YZ
	MakeVectorRegister( Perm_Data1, Perm_Data2, Perm_Data3, (uint32)Perm_Zeros ),  // 7 = 111 = XYZ
};

#undef Perm_Zeros
#undef Perm_Data1
#undef Perm_Data2
#undef Perm_Data3

// Constants used when decompressing fixed48 translation keys (pre-scale, pre-bias integer representation of a final output 0.0f)
static const VectorRegister DecompressPTCTransConstants = MakeVectorRegister( 255, 255, 255, (uint32)0 );

// Constants used when decompressing fixed48 rotation keys (pre-scale, pre-bias integer representation of a final output 0.0f)
static const VectorRegister DecompressPTCConstants = MakeVectorRegister( 32767, 32767, 32767, (uint32)0 );

// Scale-bias factors for decompressing fixed48 data (both rotation and translation)
static const VectorRegister BiasFix48Data = MakeVectorRegister( -32767.0f, -32767.0f, -32767.0f, -32767.0f );
static const VectorRegister ScaleRotData = MakeVectorRegister( 3.0518509475997192297128208258309e-5f, 3.0518509475997192297128208258309e-5f, 3.0518509475997192297128208258309e-5f, 1.0f );

//@TODO: Looks like fixed48 for translation is basically broken right now (using 8 bits instead of 16 bits!).  The scale is omitted below because it's all 1's
static const VectorRegister BiasTransData =  MakeVectorRegister( -255.0f, -255.0f, -255.0f, -255.0f );
static const VectorRegister ScaleTransData = MakeVectorRegister( 1.0f, 1.0f, 1.0f, 1.0f );

/** Decompress a single translation key from a single track that was compressed with the PerTrack codec (vectorized) */
static FORCEINLINE_DEBUGGABLE VectorRegister DecompressSingleTrackTranslationVectorized(int32 Format, int32 FormatFlags, const uint8* RESTRICT TopOfStream, const uint8* RESTRICT KeyData)
{
	if( Format == ACF_Float96NoW )
	{
		const VectorRegister KeyJumbled = VectorLoadNPlusOneUnalignedBytes(KeyData, Float96KeyBytesMinusOne[FormatFlags]);
		const VectorRegister XYZ = VectorPermute(KeyJumbled, VectorZero(), Trans96OptionalFormatPermMasks[FormatFlags & 7]);

		return XYZ;
	}
	else if (Format == ACF_Fixed48NoW)
	{
		const VectorRegister KeyJumbled = VectorLoadNPlusOneUnalignedBytes(KeyData, Fixed48KeyBytesMinusOne[FormatFlags]);
		const VectorRegister Key = VectorPermute(KeyJumbled, DecompressPTCTransConstants, Fix48FormatPermMasks[FormatFlags & 7]);
		const VectorRegister FPKey = VectorUitof(Key);

		const VectorRegister BiasedData = VectorAdd(FPKey, BiasTransData);
		//const VectorRegister XYZ = VectorMultiply(BiasedData, ScaleTransData);
		const VectorRegister XYZ = BiasedData;

		return XYZ;
	}
	else if (Format == ACF_IntervalFixed32NoW)
	{
		const float* RESTRICT SourceBounds = (float*)TopOfStream;

		float Mins[3] = {0.0f, 0.0f, 0.0f};
		float Ranges[3] = {0.0f, 0.0f, 0.0f};

		if (FormatFlags & 1)
		{
			Mins[0] = *SourceBounds++;
			Ranges[0] = *SourceBounds++;
		}
		if (FormatFlags & 2)
		{
			Mins[1] = *SourceBounds++;
			Ranges[1] = *SourceBounds++;
		}
		if (FormatFlags & 4)
		{
			Mins[2] = *SourceBounds++;
			Ranges[2] = *SourceBounds++;
		}

		// This one is still used for ~4% of the cases, so making it faster would be nice
		FVector Out;
		((FVectorIntervalFixed32NoW*)KeyData)->ToVector(Out, Mins, Ranges);
		return VectorLoadAligned(&Out);
	}

	return VectorZero();
}

/** Decompress a single Scale key from a single track that was compressed with the PerTrack codec (vectorized) */
static FORCEINLINE_DEBUGGABLE VectorRegister DecompressSingleTrackScaleVectorized(int32 Format, int32 FormatFlags, const uint8* RESTRICT TopOfStream, const uint8* RESTRICT KeyData)
{
	if( Format == ACF_Float96NoW )
	{
		const VectorRegister KeyJumbled = VectorLoadNPlusOneUnalignedBytes(KeyData, Float96KeyBytesMinusOne[FormatFlags]);
		const VectorRegister XYZ = VectorPermute(KeyJumbled, VectorZero(), Trans96OptionalFormatPermMasks[FormatFlags & 7]);

		return XYZ;
	}
	else if (Format == ACF_Fixed48NoW)
	{
		const VectorRegister KeyJumbled = VectorLoadNPlusOneUnalignedBytes(KeyData, Fixed48KeyBytesMinusOne[FormatFlags]);
		const VectorRegister Key = VectorPermute(KeyJumbled, DecompressPTCTransConstants, Fix48FormatPermMasks[FormatFlags & 7]);
		const VectorRegister FPKey = VectorUitof(Key);

		const VectorRegister BiasedData = VectorAdd(FPKey, BiasTransData);
		//const VectorRegister XYZ = VectorMultiply(BiasedData, ScaleTransData);
		const VectorRegister XYZ = BiasedData;

		return XYZ;
	}
	else if (Format == ACF_IntervalFixed32NoW)
	{
		const float* RESTRICT SourceBounds = (float*)TopOfStream;

		float Mins[3] = {0.0f, 0.0f, 0.0f};
		float Ranges[3] = {0.0f, 0.0f, 0.0f};

		if (FormatFlags & 1)
		{
			Mins[0] = *SourceBounds++;
			Ranges[0] = *SourceBounds++;
		}
		if (FormatFlags & 2)
		{
			Mins[1] = *SourceBounds++;
			Ranges[1] = *SourceBounds++;
		}
		if (FormatFlags & 4)
		{
			Mins[2] = *SourceBounds++;
			Ranges[2] = *SourceBounds++;
		}

		// This one is still used for ~4% of the cases, so making it faster would be nice
		FVector Out;
		((FVectorIntervalFixed32NoW*)KeyData)->ToVector(Out, Mins, Ranges);
		return VectorLoadAligned(&Out);
	}

	return VectorZero();
}

/** Decompress a single rotation key from a single track that was compressed with the PerTrack codec (vectorized) */
static FORCEINLINE_DEBUGGABLE VectorRegister DecompressSingleTrackRotationVectorized(int32 Format, int32 FormatFlags, const uint8* RESTRICT TopOfStream, const uint8* RESTRICT KeyData)
{
	if (Format == ACF_Fixed48NoW)
	{
		const VectorRegister KeyJumbled = VectorLoadNPlusOneUnalignedBytes(KeyData, Fixed48KeyBytesMinusOne[FormatFlags]);
		const VectorRegister Key = VectorPermute(KeyJumbled, DecompressPTCConstants, Fix48FormatPermMasks[FormatFlags & 7]);
		const VectorRegister FPKey = VectorUitof(Key);

		const VectorRegister BiasedData = VectorAdd(FPKey, BiasFix48Data);
		const VectorRegister XYZ = VectorMultiply(BiasedData, ScaleRotData);
		const VectorRegister LengthSquared = VectorDot3(XYZ, XYZ);

		const VectorRegister WSquared = VectorSubtract(VectorOne(), LengthSquared);
		const VectorRegister WSquaredSqrt = VectorReciprocal(VectorReciprocalSqrt(WSquared));
		const VectorRegister WWWW = VectorSelect(VectorCompareGT(WSquared, VectorZero()), WSquaredSqrt, VectorZero());

		const VectorRegister XYZW = VectorMergeVecXYZ_VecW(XYZ, WWWW);

		return XYZW;
	}
	else if (Format == ACF_Float96NoW)
	{
		const VectorRegister XYZ = VectorLoadFloat3(KeyData);
		const VectorRegister LengthSquared = VectorDot3(XYZ, XYZ);

		const VectorRegister WSquared = VectorSubtract(VectorOne(), LengthSquared);
		const VectorRegister WSquaredSqrt = VectorReciprocal(VectorReciprocalSqrt(WSquared));
		const VectorRegister WWWW = VectorSelect(VectorCompareGT(WSquared, VectorZero()), WSquaredSqrt, VectorZero());

		const VectorRegister XYZW = VectorMergeVecXYZ_VecW(XYZ, WWWW);

		return XYZW;
	}
	else if ( Format == ACF_IntervalFixed32NoW )
	{
		const float* RESTRICT SourceBounds = (float*)TopOfStream;

		float Mins[3] = {0.0f, 0.0f, 0.0f};
		float Ranges[3] = {0.0f, 0.0f, 0.0f};

		if (FormatFlags & 1)
		{
			Mins[0] = *SourceBounds++;
			Ranges[0] = *SourceBounds++;
		}
		if (FormatFlags & 2)
		{
			Mins[1] = *SourceBounds++;
			Ranges[1] = *SourceBounds++;
		}
		if (FormatFlags & 4)
		{
			Mins[2] = *SourceBounds++;
			Ranges[2] = *SourceBounds++;
		}

		// This one is still used for ~4% of the cases, so making it faster would be nice
		FQuat Out;
		((FQuatIntervalFixed32NoW*)KeyData)->ToQuat( Out, Mins, Ranges );
		return VectorLoadAligned(&Out);
	}
	else if ( Format == ACF_Float32NoW )
	{
		// This isn't used for compression anymore so making it fast isn't very important
		FQuat Out;
		((FQuatFloat32NoW*)KeyData)->ToQuat( Out );
		return VectorLoadAligned(&Out);
	}
	else if (Format == ACF_Fixed32NoW)
	{
		// This isn't used for compression anymore so making it fast isn't very important
		FQuat Out;
		((FQuatFixed32NoW*)KeyData)->ToQuat(Out);
		return VectorLoadAligned(&Out);
	}

	return VectorLoadAligned(&(FQuat::Identity));
}

#endif // USE_VECTOR_PTC_DECOMPRESSOR

/**
 * Handles Byte-swapping a single track of animation data from a MemoryReader or to a MemoryWriter
 *
 * @param	Seq					The Animation Sequence being operated on.
 * @param	MemoryStream		The MemoryReader or MemoryWriter object to read from/write to.
 * @param	Offset				The starting offset into the compressed byte stream for this track (can be INDEX_NONE to indicate an identity track)
 */
template<class TArchive>
void AEFPerTrackCompressionCodec::ByteSwapOneTrack(UAnimSequence& Seq, TArchive& MemoryStream, int32 Offset)
{
	// Translation data.
	if (Offset != INDEX_NONE)
	{
		checkSlow( (Offset % 4) == 0 && "CompressedByteStream not aligned to four bytes" );

		uint8* TrackData = Seq.CompressedByteStream.GetData() + Offset;

		// Read the header
		AC_UnalignedSwap(MemoryStream, TrackData, sizeof(int32));

		const int32 Header = *(reinterpret_cast<int32*>(TrackData - sizeof(int32)));


		int32 KeyFormat;
		int32 NumKeys;
		int32 FormatFlags;
		FAnimationCompression_PerTrackUtils::DecomposeHeader(Header, /*OUT*/ KeyFormat, /*OUT*/ NumKeys, /*OUT*/ FormatFlags);

		int32 FixedComponentSize = 0;
		int32 FixedComponentCount = 0;
		int32 KeyComponentSize = 0;
		int32 KeyComponentCount = 0;
		FAnimationCompression_PerTrackUtils::GetAllSizesFromFormat(KeyFormat, FormatFlags, /*OUT*/ KeyComponentCount, /*OUT*/ KeyComponentSize, /*OUT*/ FixedComponentCount, /*OUT*/ FixedComponentSize);

		// Handle per-track metadata
		for (int32 i = 0; i < FixedComponentCount; ++i)
		{
			AC_UnalignedSwap(MemoryStream, TrackData, FixedComponentSize);
		}

		// Handle keys
		for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			for (int32 i = 0; i < KeyComponentCount; ++i)
			{
				AC_UnalignedSwap(MemoryStream, TrackData, KeyComponentSize);
			}
		}

		// Handle the key frame table if present
		if ((FormatFlags & 0x8) != 0)
		{
			// Make sure the key->frame table is 4 byte aligned
			PreservePadding(TrackData, MemoryStream);

			const int32 FrameTableEntrySize = (Seq.NumFrames <= 0xFF) ? sizeof(uint8) : sizeof(uint16);
			for (int32 i = 0; i < NumKeys; ++i)
			{
				AC_UnalignedSwap(MemoryStream, TrackData, FrameTableEntrySize);
			}
		}

		// Make sure the next track is 4 byte aligned
		PreservePadding(TrackData, MemoryStream);
	}
}

template void AEFPerTrackCompressionCodec::ByteSwapOneTrack(UAnimSequence& Seq, FMemoryReader& MemoryStream, int32 Offset);
template void AEFPerTrackCompressionCodec::ByteSwapOneTrack(UAnimSequence& Seq, FMemoryWriter& MemoryStream, int32 Offset);


/**
 * Preserves 4 byte alignment within a stream
 *
 * @param	TrackData [inout]	The current data offset (will be returned four byte aligned from the start of the compressed byte stream)
 * @param	MemoryStream		The MemoryReader or MemoryWriter object to read from/write to.
 */
void AEFPerTrackCompressionCodec::PreservePadding(uint8*& TrackData, FMemoryArchive& MemoryStream)
{
	// Preserve padding
	const PTRINT ByteStreamLoc = (PTRINT) TrackData;
	const int32 PadCount = static_cast<int32>( Align(ByteStreamLoc, 4) - ByteStreamLoc );
	if (MemoryStream.IsSaving())
	{
		const uint8 PadSentinel = 85; // (1<<1)+(1<<3)+(1<<5)+(1<<7)

		for (int32 PadByteIndex = 0; PadByteIndex < PadCount; ++PadByteIndex)
		{
			MemoryStream.Serialize( (void*)&PadSentinel, sizeof(uint8) );
		}
		TrackData += PadCount;
	}
	else
	{
		MemoryStream.Serialize(TrackData, PadCount);
		TrackData += PadCount;
	}
}

/**
 * Handles Byte-swapping incoming animation data from a MemoryReader
 *
 * @param	Seq					An Animation Sequence to contain the read data.
 * @param	MemoryReader		The MemoryReader object to read from.
 */
void AEFPerTrackCompressionCodec::ByteSwapIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader)
{
	int32 OriginalNumBytes = MemoryReader.TotalSize();
	Seq.CompressedByteStream.Empty(OriginalNumBytes);
	Seq.CompressedByteStream.AddUninitialized(OriginalNumBytes);

	if (Seq.CompressedSegments.Num() != 0)
	{
#if !PLATFORM_LITTLE_ENDIAN
#error "Byte swapping needs to be implemented here to support big-endian platforms"
#endif

		// TODO: Byte swap the new format
		MemoryReader.Serialize(Seq.CompressedByteStream.GetData(), Seq.CompressedByteStream.Num());
		return;
	}

	const int32 NumTracks = Seq.CompressedTrackOffsets.Num() / 2;
	const bool bHasScaleData = Seq.CompressedScaleOffsets.IsValid();

	for ( int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex )
	{
		const int32 OffsetTrans = Seq.CompressedTrackOffsets[TrackIndex*2+0];
		ByteSwapOneTrack(Seq, MemoryReader, OffsetTrans);

		const int32 OffsetRot = Seq.CompressedTrackOffsets[TrackIndex*2+1];
		ByteSwapOneTrack(Seq, MemoryReader, OffsetRot);
		if (bHasScaleData)
		{
			const int32 OffsetScale = Seq.CompressedScaleOffsets.GetOffsetData(TrackIndex, 0);
			ByteSwapOneTrack(Seq, MemoryReader, OffsetScale);
		}
	}
}


/**
 * Handles Byte-swapping outgoing animation data to an array of BYTEs
 *
 * @param	Seq					An Animation Sequence to write.
 * @param	SerializedData		The output buffer.
 * @param	ForceByteSwapping	true is byte swapping is not optional.
 */
void AEFPerTrackCompressionCodec::ByteSwapOut(
	UAnimSequence& Seq,
	TArray<uint8>& SerializedData, 
	bool ForceByteSwapping)
{
	FMemoryWriter MemoryWriter(SerializedData, true);
	MemoryWriter.SetByteSwapping(ForceByteSwapping);

	if (Seq.CompressedSegments.Num() != 0)
	{
		// TODO: Byte swap the new format
		MemoryWriter.Serialize(Seq.CompressedByteStream.GetData(), Seq.CompressedByteStream.Num());
		return;
	}

	const int32 NumTracks = Seq.CompressedTrackOffsets.Num() / 2;
	const bool bHasScaleData = Seq.CompressedScaleOffsets.IsValid();
	for ( int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex )
	{
		const int32 OffsetTrans = Seq.CompressedTrackOffsets[TrackIndex*2+0];
		ByteSwapOneTrack(Seq, MemoryWriter, OffsetTrans);

		const int32 OffsetRot = Seq.CompressedTrackOffsets[TrackIndex*2+1];
		ByteSwapOneTrack(Seq, MemoryWriter, OffsetRot);
		if (bHasScaleData)
		{
			const int32 OffsetScale = Seq.CompressedScaleOffsets.GetOffsetData(TrackIndex, 0);
			ByteSwapOneTrack(Seq, MemoryWriter, OffsetScale);
		}
	}
}



/**
 * Extracts a single BoneAtom from an Animation Sequence.
 *
 * @param	OutAtom			The BoneAtom to fill with the extracted result.
 * @param	DecompContext	The decompression context to use.
 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
 */
void AEFPerTrackCompressionCodec::GetBoneAtom(
	FTransform& OutAtom,
	FAnimSequenceDecompressionContext& DecompContext,
	int32 TrackIndex)
{
	// Initialize to identity to set the scale and in case of a missing rotation or translation codec
	OutAtom.SetIdentity();

	GetBoneAtomTranslation(OutAtom, DecompContext, TrackIndex);
	GetBoneAtomRotation(OutAtom, DecompContext, TrackIndex);

	if (DecompContext.bHasScale)
	{
		GetBoneAtomScale(OutAtom, DecompContext, TrackIndex);
	}
}

void AEFPerTrackCompressionCodec::GetBoneAtomRotation(
	FTransform& OutAtom,
	FAnimSequenceDecompressionContext& DecompContext,
	int32 TrackIndex)
{
#if USE_SEGMENTING_CONTEXT
	if (DecompContext.AnimSeq->CompressedSegments.Num() != 0)
	{
		const FTrivialAnimKeyHandle TrivialKeyHandle = DecompContext.GetTrivialRotationKeyHandle(TrackIndex);
		if (TrivialKeyHandle.IsValid())
		{
			DecompContext.GetTrivialRotation(OutAtom, TrivialKeyHandle);
		}
		else
		{
			const FAEPerTrackKeyLerpContext& EncodingContext = *static_cast<const FAEPerTrackKeyLerpContext*>(DecompContext.EncodingContext);
			EncodingContext.GetRotation(OutAtom, DecompContext, TrackIndex);
		}

		return;
	}
#endif

	const int32* RESTRICT TrackOffsetData = DecompContext.GetCompressedTrackOffsets() + (TrackIndex * 2);
	const int32 RotKeysOffset = TrackOffsetData[1];

	if (RotKeysOffset != INDEX_NONE)
	{
		const uint8* RESTRICT TrackData = DecompContext.GetCompressedByteStream() + RotKeysOffset + 4;
		const int32 Header = *((int32*)(DecompContext.GetCompressedByteStream() + RotKeysOffset));

		int32 KeyFormat;
		int32 NumKeys;
		int32 FormatFlags;
		int32 BytesPerKey;
		int32 FixedBytes;
		FAnimationCompression_PerTrackUtils::DecomposeHeader(Header, /*OUT*/ KeyFormat, /*OUT*/ NumKeys, /*OUT*/ FormatFlags, /*OUT*/BytesPerKey, /*OUT*/ FixedBytes);

		// Figure out the key indexes
		int32 Index0 = 0;
		int32 Index1 = 0;

		// Alpha is volatile to force the compiler to store it to memory immediately, so it is ready to be loaded into a vector register without a LHS after decompressing a track 
		volatile float Alpha = 0.0f;

		if (NumKeys > 1)
		{
			if ((FormatFlags & 0x8) == 0)
			{
				Alpha = TimeToIndex(*DecompContext.AnimSeq, DecompContext.RelativePos, NumKeys, Index0, Index1);
			}
			else
			{
				const uint8* RESTRICT FrameTable = Align(TrackData + FixedBytes + BytesPerKey * NumKeys, 4);
				Alpha = TimeToIndex(*DecompContext.AnimSeq, FrameTable, DecompContext.RelativePos, NumKeys, Index0, Index1);
			}
		}

		// Unpack the first key
		const uint8* RESTRICT KeyData0 = TrackData + FixedBytes + (Index0 * BytesPerKey);

#if USE_VECTOR_PTC_DECOMPRESSOR
		const VectorRegister R0 = DecompressSingleTrackRotationVectorized(KeyFormat, FormatFlags, TrackData, KeyData0);
#else
		FQuat R0;
		FAnimationCompression_PerTrackUtils::DecompressRotation(KeyFormat, FormatFlags, R0, TrackData, KeyData0);
#endif

		// If there is a second key, figure out the lerp between the two of them
		if (Index0 != Index1)
		{
			const uint8* RESTRICT KeyData1 = TrackData + FixedBytes + (Index1 * BytesPerKey);

#if USE_VECTOR_PTC_DECOMPRESSOR
			ScalarRegister VAlpha(static_cast<float>(Alpha));

			const VectorRegister R1 = DecompressSingleTrackRotationVectorized(KeyFormat, FormatFlags, TrackData, KeyData1);

			const VectorRegister BlendedQuat = VectorLerpQuat(R0, R1, VAlpha);
			const VectorRegister BlendedNormalizedQuat = VectorNormalizeQuaternion(BlendedQuat);
			OutAtom.SetRotation(BlendedNormalizedQuat);
#else
			FQuat R1;
			FAnimationCompression_PerTrackUtils::DecompressRotation(KeyFormat, FormatFlags, R1, TrackData, KeyData1);

			// Fast linear quaternion interpolation.
			FQuat BlendedQuat = FQuat::FastLerp(R0, R1, Alpha);
			OutAtom.SetRotation( BlendedQuat );
			OutAtom.NormalizeRotation();
#endif
		}
		else // (Index0 == Index1)
		{
			OutAtom.SetRotation( R0 );
			OutAtom.NormalizeRotation();
		}
	}
	else
	{
		// Identity track
		OutAtom.SetRotation(FQuat::Identity);
	}
}


void AEFPerTrackCompressionCodec::GetBoneAtomTranslation(
	FTransform& OutAtom,
	FAnimSequenceDecompressionContext& DecompContext,
	int32 TrackIndex)
{
#if USE_SEGMENTING_CONTEXT
	if (DecompContext.AnimSeq->CompressedSegments.Num() != 0)
	{
		const FTrivialAnimKeyHandle TrivialKeyHandle = DecompContext.GetTrivialTranslationKeyHandle(TrackIndex);
		if (TrivialKeyHandle.IsValid())
		{
			DecompContext.GetTrivialTranslation(OutAtom, TrivialKeyHandle);
		}
		else
		{
			const FAEPerTrackKeyLerpContext& EncodingContext = *static_cast<const FAEPerTrackKeyLerpContext*>(DecompContext.EncodingContext);
			EncodingContext.GetTranslation(OutAtom, DecompContext, TrackIndex);
		}

		return;
	}
#endif

	const int32* RESTRICT TrackOffsetData = DecompContext.GetCompressedTrackOffsets() + (TrackIndex * 2);
	const int32 PosKeysOffset = TrackOffsetData[0];

	if (PosKeysOffset != INDEX_NONE)
	{
		const uint8* RESTRICT TrackData = DecompContext.GetCompressedByteStream() + PosKeysOffset + 4;
		const int32 Header = *((int32*)(DecompContext.GetCompressedByteStream() + PosKeysOffset));

		int32 KeyFormat;
		int32 NumKeys;
		int32 FormatFlags;
		int32 BytesPerKey;
		int32 FixedBytes;
		FAnimationCompression_PerTrackUtils::DecomposeHeader(Header, /*OUT*/ KeyFormat, /*OUT*/ NumKeys, /*OUT*/ FormatFlags, /*OUT*/BytesPerKey, /*OUT*/ FixedBytes);

		checkf(KeyFormat != ACF_None, TEXT("[%s] contians invalid keyformat. NumKeys (%d), FormatFlags (%d), BytesPerKeys (%d), FixedBytes (%d)"), *DecompContext.AnimSeq->GetName(), NumKeys, FormatFlags, BytesPerKey, FixedBytes);

		// Figure out the key indexes
		int32 Index0 = 0;
		int32 Index1 = 0;

		// Alpha is volatile to force the compiler to store it to memory immediately, so it is ready to be loaded into a vector register without a LHS after decompressing a track 
		volatile float Alpha = 0.0f;

		if (NumKeys > 1)
		{
			if ((FormatFlags & 0x8) == 0)
			{
				Alpha = TimeToIndex(*DecompContext.AnimSeq, DecompContext.RelativePos, NumKeys, Index0, Index1);
			}
			else
			{
				const uint8* RESTRICT FrameTable = Align(TrackData + FixedBytes + BytesPerKey * NumKeys, 4);
				Alpha = TimeToIndex(*DecompContext.AnimSeq, FrameTable, DecompContext.RelativePos, NumKeys, Index0, Index1);
			}
		}

		// Unpack the first key
		const uint8* RESTRICT KeyData0 = TrackData + FixedBytes + (Index0 * BytesPerKey);

#if USE_VECTOR_PTC_DECOMPRESSOR
		const VectorRegister R0 = DecompressSingleTrackTranslationVectorized(KeyFormat, FormatFlags, TrackData, KeyData0);
#else
		FVector R0;
		FAnimationCompression_PerTrackUtils::DecompressTranslation(KeyFormat, FormatFlags, R0, TrackData, KeyData0);
#endif

		// If there is a second key, figure out the lerp between the two of them
		if (Index0 != Index1)
		{
			const uint8* RESTRICT KeyData1 = TrackData + FixedBytes + (Index1 * BytesPerKey);

#if USE_VECTOR_PTC_DECOMPRESSOR
			ScalarRegister VAlpha(static_cast<float>(Alpha));

			const VectorRegister R1 = DecompressSingleTrackTranslationVectorized(KeyFormat, FormatFlags, TrackData, KeyData1);

			const VectorRegister BlendedTranslation = FMath::Lerp(R0, R1, VAlpha);
			OutAtom.SetTranslation(BlendedTranslation);
#else
			FVector R1;
			FAnimationCompression_PerTrackUtils::DecompressTranslation(KeyFormat, FormatFlags, R1, TrackData, KeyData1);

			OutAtom.SetTranslation(FMath::Lerp(R0, R1, Alpha));
#endif
		}
		else // (Index0 == Index1)
		{
			OutAtom.SetTranslation(R0);
		}
	}
	else
	{
		// Identity track
#if USE_VECTOR_PTC_DECOMPRESSOR
		OutAtom.SetTranslation(VectorZero());
#else
		OutAtom.SetTranslation(FVector::ZeroVector);
#endif
	}
}


void AEFPerTrackCompressionCodec::GetBoneAtomScale(
	FTransform& OutAtom,
	FAnimSequenceDecompressionContext& DecompContext,
	int32 TrackIndex)
{
#if USE_SEGMENTING_CONTEXT
	if (DecompContext.AnimSeq->CompressedSegments.Num() != 0)
	{
		const FTrivialAnimKeyHandle TrivialKeyHandle = DecompContext.GetTrivialScaleKeyHandle(TrackIndex);
		if (TrivialKeyHandle.IsValid())
		{
			DecompContext.GetTrivialScale(OutAtom, TrivialKeyHandle);
		}
		else
		{
			const FAEPerTrackKeyLerpContext& EncodingContext = *static_cast<const FAEPerTrackKeyLerpContext*>(DecompContext.EncodingContext);
			EncodingContext.GetScale(OutAtom, DecompContext, TrackIndex);
		}

		return;
	}
#endif


	const int32 ScaleKeysOffset = DecompContext.GetCompressedScaleOffsets()->GetOffsetData( TrackIndex, 0 );

	if (ScaleKeysOffset != INDEX_NONE)
	{
		const uint8* RESTRICT TrackData = DecompContext.GetCompressedByteStream() + ScaleKeysOffset + 4;
		const int32 Header = *((int32*)(DecompContext.GetCompressedByteStream() + ScaleKeysOffset));

		int32 KeyFormat;
		int32 NumKeys;
		int32 FormatFlags;
		int32 BytesPerKey;
		int32 FixedBytes;
		FAnimationCompression_PerTrackUtils::DecomposeHeader(Header, /*OUT*/ KeyFormat, /*OUT*/ NumKeys, /*OUT*/ FormatFlags, /*OUT*/BytesPerKey, /*OUT*/ FixedBytes);

		// Figure out the key indexes
		int32 Index0 = 0;
		int32 Index1 = 0;

		// Alpha is volatile to force the compiler to store it to memory immediately, so it is ready to be loaded into a vector register without a LHS after decompressing a track 
		volatile float Alpha = 0.0f;

		if (NumKeys > 1)
		{
			if ((FormatFlags & 0x8) == 0)
			{
				Alpha = TimeToIndex(*DecompContext.AnimSeq, DecompContext.RelativePos, NumKeys, Index0, Index1);
			}
			else
			{
				const uint8* RESTRICT FrameTable = Align(TrackData + FixedBytes + BytesPerKey * NumKeys, 4);
				Alpha = TimeToIndex(*DecompContext.AnimSeq, FrameTable, DecompContext.RelativePos, NumKeys, Index0, Index1);
			}
		}

		// Unpack the first key
		const uint8* RESTRICT KeyData0 = TrackData + FixedBytes + (Index0 * BytesPerKey);

#if USE_VECTOR_PTC_DECOMPRESSOR
		const VectorRegister R0 = DecompressSingleTrackScaleVectorized(KeyFormat, FormatFlags, TrackData, KeyData0);
#else
		FVector R0;
		FAnimationCompression_PerTrackUtils::DecompressScale(KeyFormat, FormatFlags, R0, TrackData, KeyData0);
#endif

		// If there is a second key, figure out the lerp between the two of them
		if (Index0 != Index1)
		{
			const uint8* RESTRICT KeyData1 = TrackData + FixedBytes + (Index1 * BytesPerKey);

#if USE_VECTOR_PTC_DECOMPRESSOR
			ScalarRegister VAlpha(static_cast<float>(Alpha));

			const VectorRegister R1 = DecompressSingleTrackScaleVectorized(KeyFormat, FormatFlags, TrackData, KeyData1);

			const VectorRegister BlendedScale = FMath::Lerp(R0, R1, VAlpha);
			OutAtom.SetScale(BlendedScale);
#else
			FVector R1;
			FAnimationCompression_PerTrackUtils::DecompressScale(KeyFormat, FormatFlags, R1, TrackData, KeyData1);

			OutAtom.SetScale3D(FMath::Lerp(R0, R1, Alpha));
#endif
		}
		else // (Index0 == Index1)
		{
			OutAtom.SetScale3D(R0);
		}
	}
	else
	{
		// Identity track
#if USE_VECTOR_PTC_DECOMPRESSOR
		OutAtom.SetScale(VectorZero());
#else
		OutAtom.SetScale3D(FVector::ZeroVector);
#endif
	}
}

#if USE_ANIMATION_CODEC_BATCH_SOLVER

/**
 * Decompress all requested rotation components from an Animation Sequence
 *
 * @param	Atoms			The FTransform array to fill in.
 * @param	DesiredPairs	Array of requested bone information
 * @param	DecompContext	The decompression context to use.
 */
void AEFPerTrackCompressionCodec::GetPoseRotations(
	FTransformArray& Atoms,
	const BoneTrackArray& DesiredPairs,
	FAnimSequenceDecompressionContext& DecompContext)
{
	const int32 PairCount = DesiredPairs.Num();

	for( int32 PairIndex = 0; PairIndex < PairCount; ++PairIndex )
	{
		const BoneTrackPair& Pair = DesiredPairs[PairIndex];
		const int32 TrackIndex = Pair.TrackIndex;
		const int32 AtomIndex = Pair.AtomIndex;
		FTransform& BoneAtom = Atoms[AtomIndex];

		GetBoneAtomRotation(BoneAtom, DecompContext, TrackIndex);
	}
}

/**
 * Decompress all requested translation components from an Animation Sequence
 *
 * @param	Atoms			The FTransform array to fill in.
 * @param	DesiredPairs	Array of requested bone information
 * @param	DecompContext	The decompression context to use.
 */
void AEFPerTrackCompressionCodec::GetPoseTranslations(
	FTransformArray& Atoms,
	const BoneTrackArray& DesiredPairs,
	FAnimSequenceDecompressionContext& DecompContext)
{
	const int32 PairCount = DesiredPairs.Num();

	for( int32 PairIndex = 0; PairIndex < PairCount; ++PairIndex )
	{
		const BoneTrackPair& Pair = DesiredPairs[PairIndex];
		const int32 TrackIndex = Pair.TrackIndex;
		const int32 AtomIndex = Pair.AtomIndex;
		FTransform& BoneAtom = Atoms[AtomIndex];

		GetBoneAtomTranslation(BoneAtom, DecompContext, TrackIndex);
	}
}

/**
 * Decompress all requested Scale components from an Animation Sequence
 *
 * @param	Atoms			The FTransform array to fill in.
 * @param	DesiredPairs	Array of requested bone information
 * @param	DecompContext	The decompression context to use.
 */
void AEFPerTrackCompressionCodec::GetPoseScales(
	FTransformArray& Atoms,
	const BoneTrackArray& DesiredPairs,
	FAnimSequenceDecompressionContext& DecompContext)
{
	checkSlow(DecompContext.bHasScale);

	const int32 PairCount = DesiredPairs.Num();

	for( int32 PairIndex = 0; PairIndex < PairCount; ++PairIndex )
	{
		const BoneTrackPair& Pair = DesiredPairs[PairIndex];
		const int32 TrackIndex = Pair.TrackIndex;
		const int32 AtomIndex = Pair.AtomIndex;
		FTransform& BoneAtom = Atoms[AtomIndex];

		GetBoneAtomScale(BoneAtom, DecompContext, TrackIndex);
	}
}

#endif // USE_ANIMATION_CODEC_BATCH_SOLVER

#if USE_SEGMENTING_CONTEXT
void AEFPerTrackCompressionCodec::CreateEncodingContext(FAnimSequenceDecompressionContext& DecompContext)
{
	checkSlow(DecompContext.EncodingContext == nullptr);
	DecompContext.EncodingContext = new FAEPerTrackKeyLerpContext(DecompContext);
}

void AEFPerTrackCompressionCodec::ReleaseEncodingContext(FAnimSequenceDecompressionContext& DecompContext)
{
	checkSlow(DecompContext.EncodingContext != nullptr);
	delete DecompContext.EncodingContext;
	DecompContext.EncodingContext = nullptr;
}

FAEPerTrackKeyLerpContext::FAEPerTrackKeyLerpContext(const FAnimSequenceDecompressionContext& DecompContext)
	: PreviousSampleAtTime(FLT_MAX)	// Very large to trigger a reset on the first Seek()
{
	PreviousSegmentIndex[0] = -1;
	PreviousSegmentIndex[1] = -1;

	const int32 NumEntries = DecompContext.NumTracks * DecompContext.NumStreamsPerTrack;
	for (int32 SegmentIndex = 0; SegmentIndex < 2; ++SegmentIndex)
	{
		UniformKeyOffsets[SegmentIndex].Empty(NumEntries);
		UniformKeyOffsets[SegmentIndex].AddUninitialized(NumEntries);
		RangeOffsets[SegmentIndex].Empty(NumEntries);
		RangeOffsets[SegmentIndex].AddUninitialized(NumEntries);

		if (!DecompContext.bIsSorted)
		{
			NumAnimatedTrackStreams[SegmentIndex].Empty(NumEntries);
			NumAnimatedTrackStreams[SegmentIndex].AddUninitialized(NumEntries);
			TrackStreamKeySizes[SegmentIndex].Empty(NumEntries);
			TrackStreamKeySizes[SegmentIndex].AddUninitialized(NumEntries);
		}
	}
}

void FAEPerTrackKeyLerpContext::CacheSegmentValues(const FAnimSequenceDecompressionContext& DecompContext, const FCompressedSegment& Segment, uint8 SegmentIndex)
{
	PerTrackStreamFlagOffsets[SegmentIndex] = Segment.ByteStreamOffset;

	const int32 PerTrackFlagsSize = Align(DecompContext.NumTracks * DecompContext.NumStreamsPerTrack * sizeof(uint8), 4);
	const int32 RangeBaseOffset = Segment.ByteStreamOffset + PerTrackFlagsSize;
	const uint8* PerTrackStreamFlags = DecompContext.CompressedByteStream + Segment.ByteStreamOffset;

	int32 KeyOffset = 0;
	int32 RangeOffset = RangeBaseOffset;
	int32 TotalNumAnimatedTrackStreams = 0;
	for (int32 TrackIndex = 0; TrackIndex < DecompContext.NumTracks; ++TrackIndex)
	{
		const FTrivialTrackFlags TrivialTrackFlags(DecompContext.TrackFlags[TrackIndex]);

		const int32 TranslationValueOffset = DecompContext.GetTranslationValueOffset(TrackIndex);
		UniformKeyOffsets[SegmentIndex][TranslationValueOffset] = KeyOffset;
		RangeOffsets[SegmentIndex][TranslationValueOffset] = RangeOffset;
		NumAnimatedTrackStreams[SegmentIndex][TranslationValueOffset] = TotalNumAnimatedTrackStreams;

		int32 BytesPerKey = 0;
		if (!TrivialTrackFlags.IsTranslationTrivial())
		{
			const FPerTrackFlags TranslationTrackFlags(PerTrackStreamFlags[TranslationValueOffset]);
			const uint8 Format = TranslationTrackFlags.GetFormat();
			const uint8 FormatFlags = TranslationTrackFlags.GetFormatFlags();

			int32 BytesPerRange;
			FAnimationCompression_PerTrackUtils::GetByteSizesFromFormat(Format, FormatFlags, BytesPerKey, BytesPerRange);

			if (TranslationTrackFlags.IsUniform())
			{
				KeyOffset += BytesPerKey;
			}
			else
			{
				TotalNumAnimatedTrackStreams++;
			}

			if (Format == ACF_IntervalFixed32NoW)
			{
				RangeOffset += BytesPerRange;
			}
		}

		TrackStreamKeySizes[SegmentIndex][TranslationValueOffset] = static_cast<uint8>(BytesPerKey);

		const int32 RotationValueOffset = DecompContext.GetRotationValueOffset(TrackIndex);
		UniformKeyOffsets[SegmentIndex][RotationValueOffset] = KeyOffset;
		RangeOffsets[SegmentIndex][RotationValueOffset] = RangeOffset;
		NumAnimatedTrackStreams[SegmentIndex][RotationValueOffset] = TotalNumAnimatedTrackStreams;

		BytesPerKey = 0;
		if (!TrivialTrackFlags.IsRotationTrivial())
		{
			const FPerTrackFlags RotationTrackFlags(PerTrackStreamFlags[RotationValueOffset]);
			const uint8 Format = RotationTrackFlags.GetFormat();
			const uint8 FormatFlags = RotationTrackFlags.GetFormatFlags();

			int32 BytesPerRange;
			FAnimationCompression_PerTrackUtils::GetByteSizesFromFormat(Format, FormatFlags, BytesPerKey, BytesPerRange);

			if (RotationTrackFlags.IsUniform())
			{
				KeyOffset += BytesPerKey;
			}
			else
			{
				TotalNumAnimatedTrackStreams++;
			}

			if (Format == ACF_IntervalFixed32NoW)
			{
				RangeOffset += BytesPerRange;
			}
		}

		TrackStreamKeySizes[SegmentIndex][RotationValueOffset] = static_cast<uint8>(BytesPerKey);

		if (DecompContext.bHasScale)
		{
			const int32 ScaleValueOffset = DecompContext.GetScaleValueOffset(TrackIndex);
			UniformKeyOffsets[SegmentIndex][ScaleValueOffset] = KeyOffset;
			RangeOffsets[SegmentIndex][ScaleValueOffset] = RangeOffset;
			NumAnimatedTrackStreams[SegmentIndex][ScaleValueOffset] = TotalNumAnimatedTrackStreams;

			BytesPerKey = 0;
			if (!TrivialTrackFlags.IsScaleTrivial())
			{
				const FPerTrackFlags ScaleTrackFlags(PerTrackStreamFlags[ScaleValueOffset]);
				const uint8 Format = ScaleTrackFlags.GetFormat();
				const uint8 FormatFlags = ScaleTrackFlags.GetFormatFlags();

				int32 BytesPerRange;
				FAnimationCompression_PerTrackUtils::GetByteSizesFromFormat(Format, FormatFlags, BytesPerKey, BytesPerRange);

				if (ScaleTrackFlags.IsUniform())
				{
					KeyOffset += BytesPerKey;
				}
				else
				{
					TotalNumAnimatedTrackStreams++;
				}

				if (Format == ACF_IntervalFixed32NoW)
				{
					RangeOffset += BytesPerRange;
				}
			}

			TrackStreamKeySizes[SegmentIndex][ScaleValueOffset] = static_cast<uint8>(BytesPerKey);
		}
	}

	const int32 SegmentUniformKeyFrameSize = KeyOffset;
	UniformKeyFrameSize[SegmentIndex] = SegmentUniformKeyFrameSize;
	RangeDataSize[SegmentIndex] = RangeOffset - RangeBaseOffset;

	UniformDataOffsets[SegmentIndex] = Segment.ByteStreamOffset + PerTrackFlagsSize + RangeDataSize[SegmentIndex];

	if (!DecompContext.bIsSorted)
	{
		// Variable linear
		TimeMarkerSize[SegmentIndex] = Segment.NumFrames < 256 ? sizeof(uint8) : sizeof(uint16);

		const int32 UniformDataSize = Align(SegmentUniformKeyFrameSize * Segment.NumFrames, 4);

		OffsetNumKeysPairOffsets[SegmentIndex] = Segment.ByteStreamOffset + PerTrackFlagsSize + RangeDataSize[SegmentIndex] + UniformDataSize;
	}
}

void FAEPerTrackKeyLerpContext::ResetSortedCache(const FAnimSequenceDecompressionContext& DecompContext)
{
	CachedKeys.Reset(DecompContext.NumTracks);
	CachedKeys.AddZeroed(DecompContext.NumTracks);

	const int32 PerTrackFlagsSize = Align(DecompContext.NumTracks * DecompContext.NumStreamsPerTrack * sizeof(uint8), 4);
	const int32 UniformDataSize = Align(UniformKeyFrameSize[0] * DecompContext.Segment0->NumFrames, 4);

	PackedSampleData = DecompContext.CompressedByteStream + DecompContext.Segment0->ByteStreamOffset + PerTrackFlagsSize + RangeDataSize[0] + UniformDataSize;
	PreviousFrameIndex = 0;
	CurrentSegmentIndex = 0;
}

static constexpr uint8 AEPerTrackKeyLerpContextCachedKeyStructOffsets[3][2] =
{
	{ offsetof(FAEPerTrackKeyLerpContext::FCachedKey, TransOffsets), offsetof(FAEPerTrackKeyLerpContext::FCachedKey, TransFrameIndices) },
	{ offsetof(FAEPerTrackKeyLerpContext::FCachedKey, RotOffsets), offsetof(FAEPerTrackKeyLerpContext::FCachedKey, RotFrameIndices) },
	{ offsetof(FAEPerTrackKeyLerpContext::FCachedKey, ScaleOffsets), offsetof(FAEPerTrackKeyLerpContext::FCachedKey, ScaleFrameIndices) },
};

void FAEPerTrackKeyLerpContext::AdvanceSortedCachedKeys(const FAnimSequenceDecompressionContext& DecompContext)
{
	while (true)
	{
		const uint8* SampleData = PackedSampleData;
		const FSortedKeyHeader KeyHeader(SampleData);
		if (KeyHeader.IsEndOfStream())
		{
			break;	// Reached the end of the stream
		}
		checkSlow(KeyHeader.TrackIndex < DecompContext.NumTracks);

		const uint8 SampleType = KeyHeader.GetKeyType();
		checkSlow(SampleType <= 2);

		const int32 TimeDelta = KeyHeader.GetTimeDelta();
		const int32 FrameIndex = PreviousFrameIndex + TimeDelta;

		// Swap and update
		FCachedKey& CachedKey = CachedKeys[KeyHeader.TrackIndex];
		int32* DataOffset = reinterpret_cast<int32*>(reinterpret_cast<uint8*>(&CachedKey) + AEPerTrackKeyLerpContextCachedKeyStructOffsets[SampleType][0]);
		int32* FrameIndicesOffset = reinterpret_cast<int32*>(reinterpret_cast<uint8*>(&CachedKey) + AEPerTrackKeyLerpContextCachedKeyStructOffsets[SampleType][1]);
		if (FrameIndex > CurrentFrameIndex && FrameIndicesOffset[1] >= CurrentFrameIndex)
		{
			break;		// Reached a sample we don't need yet, stop for now
		}

		SampleData += KeyHeader.GetSize();

		DataOffset[0] = DataOffset[1];
		DataOffset[1] = static_cast<int32>(SampleData - DecompContext.CompressedByteStream);
		FrameIndicesOffset[0] = FrameIndicesOffset[1];
		FrameIndicesOffset[1] = FrameIndex;

		const uint8 BytesPerKey = TrackStreamKeySizes[CurrentSegmentIndex][(KeyHeader.TrackIndex * DecompContext.NumStreamsPerTrack) + SampleType];

		PreviousFrameIndex = FrameIndex;
		SampleData += BytesPerKey;
		PackedSampleData = SampleData;	// Update the pointer since we consumed the sample
	}
}

void FAEPerTrackKeyLerpContext::Seek(const FAnimSequenceDecompressionContext& DecompContext, float SampleAtTime)
{
	const bool IsSegmentCacheStale0 = PreviousSegmentIndex[0] != DecompContext.SegmentIndex0;
	const bool IsSegmentCacheStale1 = PreviousSegmentIndex[1] != DecompContext.SegmentIndex1;
	if (IsSegmentCacheStale0 || IsSegmentCacheStale1)
	{
		if (IsSegmentCacheStale0 && PreviousSegmentIndex[1] == DecompContext.SegmentIndex0)
		{
			// Forward playback, the new segment 0 is our old segment 1, swap the data and refresh the segment 1
			PerTrackStreamFlagOffsets[0] = PerTrackStreamFlagOffsets[1];
			UniformKeyOffsets[0] = UniformKeyOffsets[1];
			UniformKeyFrameSize[0] = UniformKeyFrameSize[1];
			RangeOffsets[0] = RangeOffsets[1];
			RangeDataSize[0] = RangeDataSize[1];
			UniformDataOffsets[0] = UniformDataOffsets[1];

			// Variable linear
			TimeMarkerSize[0] = TimeMarkerSize[1];
			OffsetNumKeysPairOffsets[0] = OffsetNumKeysPairOffsets[1];
			NumAnimatedTrackStreams[0] = NumAnimatedTrackStreams[1];
			TrackStreamKeySizes[0] = TrackStreamKeySizes[1];

			if (IsSegmentCacheStale1)
			{
				CacheSegmentValues(DecompContext, *DecompContext.Segment1, 1);
			}
		}
		else if (IsSegmentCacheStale1 && PreviousSegmentIndex[0] == DecompContext.SegmentIndex1)
		{
			// Backward playback, the new segment 1 is our old segment 0, swap the data and refresh the segment 0
			PerTrackStreamFlagOffsets[1] = PerTrackStreamFlagOffsets[0];
			UniformKeyOffsets[1] = UniformKeyOffsets[0];
			UniformKeyFrameSize[1] = UniformKeyFrameSize[0];
			RangeOffsets[1] = RangeOffsets[0];
			RangeDataSize[1] = RangeDataSize[0];
			UniformDataOffsets[1] = UniformDataOffsets[0];

			// Variable linear
			TimeMarkerSize[1] = TimeMarkerSize[0];
			OffsetNumKeysPairOffsets[1] = OffsetNumKeysPairOffsets[0];
			NumAnimatedTrackStreams[1] = NumAnimatedTrackStreams[0];
			TrackStreamKeySizes[1] = TrackStreamKeySizes[0];

			if (IsSegmentCacheStale0)
			{
				CacheSegmentValues(DecompContext, *DecompContext.Segment0, 0);
			}
		}
		else
		{
			if (IsSegmentCacheStale0)
			{
				CacheSegmentValues(DecompContext, *DecompContext.Segment0, 0);
			}

			if (IsSegmentCacheStale1)
			{
				CacheSegmentValues(DecompContext, *DecompContext.Segment1, 1);
			}
		}

		PreviousSegmentIndex[0] = DecompContext.SegmentIndex0;
		PreviousSegmentIndex[1] = DecompContext.SegmentIndex1;
	}

	FramePos = DecompContext.RelativePos * float(DecompContext.AnimSeq->NumFrames - 1);

	if (DecompContext.bIsSorted)
	{
		if (SampleAtTime < PreviousSampleAtTime)
		{
			// Seeking backwards is terribly slow because we start over from the start
			ResetSortedCache(DecompContext);
		}
		else if (IsSegmentCacheStale0)
		{
			// We are seeking forward into a new segment, start over
			ResetSortedCache(DecompContext);
		}

		SegmentStartFrame[0] = DecompContext.Segment0->StartFrame;
		SegmentStartFrame[1] = DecompContext.Segment1->StartFrame;

		if (DecompContext.NeedsTwoSegments)
		{
			CurrentFrameIndex = CurrentSegmentIndex == 0 ? DecompContext.SegmentKeyIndex0 : DecompContext.SegmentKeyIndex1;
		}
		else
		{
			CurrentFrameIndex = FMath::Max(DecompContext.SegmentKeyIndex1, 1);
		}

		AdvanceSortedCachedKeys(DecompContext);

		if (DecompContext.NeedsTwoSegments && CurrentSegmentIndex == 0)
		{
			// Switch to our segment 1
			const int32 PerTrackFlagsSize = Align(DecompContext.NumTracks * DecompContext.NumStreamsPerTrack * sizeof(uint8), 4);
			const int32 UniformDataSize = Align(UniformKeyFrameSize[1] * DecompContext.Segment1->NumFrames, 4);

			PackedSampleData = DecompContext.CompressedByteStream + DecompContext.Segment1->ByteStreamOffset + PerTrackFlagsSize + RangeDataSize[1] + UniformDataSize;
			PreviousFrameIndex = 0;
			CurrentFrameIndex = DecompContext.SegmentKeyIndex1;
			CurrentSegmentIndex = 1;

			AdvanceSortedCachedKeys(DecompContext);

			// Any track that is variable in segment 0 but not in segment 1 needs to be manually rotated in the cache
			const uint8* PerTrackStreamFlags0 = DecompContext.CompressedByteStream + DecompContext.Segment0->ByteStreamOffset;
			const uint8* PerTrackStreamFlags1 = DecompContext.CompressedByteStream + DecompContext.Segment1->ByteStreamOffset;

			for (int32 TrackIndex = 0; TrackIndex < DecompContext.NumTracks; ++TrackIndex)
			{
				const FTrivialTrackFlags TrivialTrackFlags(DecompContext.TrackFlags[TrackIndex]);

				if (!TrivialTrackFlags.IsTranslationTrivial())
				{
					const int32 TranslationValueOffset = DecompContext.GetTranslationValueOffset(TrackIndex);
					const FPerTrackFlags TranslationTrackFlags0(PerTrackStreamFlags0[TranslationValueOffset]);
					const FPerTrackFlags TranslationTrackFlags1(PerTrackStreamFlags1[TranslationValueOffset]);
					if (!TranslationTrackFlags0.IsUniform() && TranslationTrackFlags1.IsUniform())
					{
						FCachedKey& CachedKey = CachedKeys[TrackIndex];
						CachedKey.TransFrameIndices[0] = CachedKey.TransFrameIndices[1];
						CachedKey.TransOffsets[0] = CachedKey.TransOffsets[1];
					}
				}

				if (!TrivialTrackFlags.IsRotationTrivial())
				{
					const int32 RotationValueOffset = DecompContext.GetRotationValueOffset(TrackIndex);
					const FPerTrackFlags RotationTrackFlags0(PerTrackStreamFlags0[RotationValueOffset]);
					const FPerTrackFlags RotationTrackFlags1(PerTrackStreamFlags1[RotationValueOffset]);
					if (!RotationTrackFlags0.IsUniform() && RotationTrackFlags1.IsUniform())
					{
						FCachedKey& CachedKey = CachedKeys[TrackIndex];
						CachedKey.RotFrameIndices[0] = CachedKey.RotFrameIndices[1];
						CachedKey.RotOffsets[0] = CachedKey.RotOffsets[1];
					}
				}

				if (DecompContext.bHasScale && !TrivialTrackFlags.IsScaleTrivial())
				{
					const int32 ScaleValueOffset = DecompContext.GetScaleValueOffset(TrackIndex);
					const FPerTrackFlags ScaleTrackFlags0(PerTrackStreamFlags0[ScaleValueOffset]);
					const FPerTrackFlags ScaleTrackFlags1(PerTrackStreamFlags1[ScaleValueOffset]);
					if (!ScaleTrackFlags0.IsUniform() && ScaleTrackFlags1.IsUniform())
					{
						FCachedKey& CachedKey = CachedKeys[TrackIndex];
						CachedKey.ScaleFrameIndices[0] = CachedKey.ScaleFrameIndices[1];
						CachedKey.ScaleOffsets[0] = CachedKey.ScaleOffsets[1];
					}
				}
			}
		}

		PreviousSampleAtTime = SampleAtTime;
	}
	else
	{
		const float SegmentFramePos = FramePos - float(DecompContext.Segment0->StartFrame);
		SegmentRelativePos0 = SegmentFramePos / float(DecompContext.Segment0->NumFrames - 1);
	}
}

void FAEPerTrackKeyLerpContext::GetRotation(FTransform& OutAtom, const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) const
{
	const int32 RotationValueOffset = DecompContext.GetRotationValueOffset(TrackIndex);

	const uint8* PerTrackStreamFlags0 = DecompContext.CompressedByteStream + PerTrackStreamFlagOffsets[0];
	const FPerTrackFlags RotationFlags0(PerTrackStreamFlags0[RotationValueOffset]);

	const uint8 KeyFormat0 = RotationFlags0.GetFormat();
	const uint8 FormatFlags0 = RotationFlags0.GetFormatFlags();

	const int32 RangeOffset0 = RangeOffsets[0][RotationValueOffset];
	const uint8* RangeData0 = DecompContext.CompressedByteStream + RangeOffset0;

	if (DecompContext.NeedsTwoSegments)
	{
		FQuat Rotation0;
		if (RotationFlags0.IsUniform())
		{
			const int32 KeyOffset0 = UniformKeyOffsets[0][RotationValueOffset];
			const int32 FrameStartOffset0 = UniformKeyFrameSize[0] * DecompContext.SegmentKeyIndex0;

			const uint8* KeyData0 = DecompContext.CompressedByteStream + UniformDataOffsets[0] + FrameStartOffset0 + KeyOffset0;

			FAnimationCompression_PerTrackUtils::DecompressRotation<false>(KeyFormat0, FormatFlags0, Rotation0, RangeData0, KeyData0);
		}
		else if (DecompContext.bIsSorted)
		{
			const FCachedKey& CachedKey = CachedKeys[TrackIndex];

			const uint8* KeyData0 = DecompContext.CompressedByteStream + CachedKey.RotOffsets[0];

			FAnimationCompression_PerTrackUtils::DecompressRotation<false>(KeyFormat0, FormatFlags0, Rotation0, RangeData0, KeyData0);
		}
		else
		{
			const int32 NumTrackStreams0 = NumAnimatedTrackStreams[0][RotationValueOffset];

			const uint8* OffsetNumKeysPairs0 = DecompContext.CompressedByteStream + OffsetNumKeysPairOffsets[0];
			const uint8* OffsetNumKeysPair0 = OffsetNumKeysPairs0 + (OffsetNumKeysPairSize * NumTrackStreams0);
			const uint16 NumKeys0 = *reinterpret_cast<const uint16*>(OffsetNumKeysPair0 + sizeof(uint32));

			const uint32 KeysOffset0 = AnimationCompressionUtils::UnalignedRead<uint32>(OffsetNumKeysPair0);
			const int32 TimeMarkersOffset0 = DecompContext.Segment0->ByteStreamOffset + KeysOffset0;

			const int32 TrackDataOffset0 = Align(TimeMarkersOffset0 + (NumKeys0 * TimeMarkerSize[0]), 4);
			const uint8 BytesPerKey0 = TrackStreamKeySizes[0][RotationValueOffset];
			const int32 KeyDataOffset0 = TrackDataOffset0 + ((NumKeys0 - 1) * BytesPerKey0);	// We need the last key of segment 0 and the first key of segment 1
			const uint8* KeyData0 = DecompContext.CompressedByteStream + KeyDataOffset0;

			FAnimationCompression_PerTrackUtils::DecompressRotation<false>(KeyFormat0, FormatFlags0, Rotation0, RangeData0, KeyData0);
		}

		const uint8* PerTrackStreamFlags1 = DecompContext.CompressedByteStream + PerTrackStreamFlagOffsets[1];
		const FPerTrackFlags RotationFlags1(PerTrackStreamFlags1[RotationValueOffset]);

		const uint8 KeyFormat1 = RotationFlags1.GetFormat();
		const uint8 FormatFlags1 = RotationFlags1.GetFormatFlags();

		const int32 RangeOffset1 = RangeOffsets[1][RotationValueOffset];
		const uint8* RangeData1 = DecompContext.CompressedByteStream + RangeOffset1;

		FQuat Rotation1;
		if (RotationFlags1.IsUniform())
		{
			const int32 KeyOffset1 = UniformKeyOffsets[1][RotationValueOffset];
			const int32 FrameStartOffset1 = UniformKeyFrameSize[1] * DecompContext.SegmentKeyIndex1;

			const uint8* KeyData1 = DecompContext.CompressedByteStream + UniformDataOffsets[1] + FrameStartOffset1 + KeyOffset1;

			FAnimationCompression_PerTrackUtils::DecompressRotation<false>(KeyFormat1, FormatFlags1, Rotation1, RangeData1, KeyData1);
		}
		else if (DecompContext.bIsSorted)
		{
			const FCachedKey& CachedKey = CachedKeys[TrackIndex];

			const uint8* KeyData1 = DecompContext.CompressedByteStream + CachedKey.RotOffsets[1];

			FAnimationCompression_PerTrackUtils::DecompressRotation<false>(KeyFormat1, FormatFlags1, Rotation1, RangeData1, KeyData1);
		}
		else
		{
			const int32 NumTrackStreams1 = NumAnimatedTrackStreams[1][RotationValueOffset];

			const uint8* OffsetNumKeysPairs1 = DecompContext.CompressedByteStream + OffsetNumKeysPairOffsets[1];
			const uint8* OffsetNumKeysPair1 = OffsetNumKeysPairs1 + (OffsetNumKeysPairSize * NumTrackStreams1);
			const uint16 NumKeys1 = *reinterpret_cast<const uint16*>(OffsetNumKeysPair1 + sizeof(uint32));

			const uint32 KeysOffset1 = AnimationCompressionUtils::UnalignedRead<uint32>(OffsetNumKeysPair1);
			const int32 TimeMarkersOffset1 = DecompContext.Segment1->ByteStreamOffset + KeysOffset1;

			const int32 TrackDataOffset1 = Align(TimeMarkersOffset1 + (NumKeys1 * TimeMarkerSize[1]), 4);
			// We need the last key of segment 0 and the first key of segment 1
			const int32 KeyDataOffset1 = TrackDataOffset1;	// First key!
			const uint8* KeyData1 = DecompContext.CompressedByteStream + KeyDataOffset1;

			FAnimationCompression_PerTrackUtils::DecompressRotation<false>(KeyFormat1, FormatFlags1, Rotation1, RangeData1, KeyData1);
		}

		// Fast linear quaternion interpolation.
		FQuat BlendedQuat = FQuat::FastLerp(Rotation0, Rotation1, DecompContext.KeyAlpha);
		BlendedQuat.Normalize();

		OutAtom.SetRotation(BlendedQuat);
	}
	else
	{
		if (RotationFlags0.IsUniform())
		{
			const int32 KeyOffset0 = UniformKeyOffsets[0][RotationValueOffset];
			const int32 UniformKeyFrameSize0 = UniformKeyFrameSize[0];
			const int32 FrameStartOffset0 = UniformKeyFrameSize0 * DecompContext.SegmentKeyIndex0;

			const uint8* KeyData0 = DecompContext.CompressedByteStream + UniformDataOffsets[0] + FrameStartOffset0 + KeyOffset0;

			FQuat Rotation;
			FAnimationCompression_PerTrackUtils::DecompressRotation<false>(KeyFormat0, FormatFlags0, Rotation, RangeData0, KeyData0);

			if (DecompContext.NeedsInterpolation)
			{
				const uint8* KeyData1 = KeyData0 + UniformKeyFrameSize0;

				FQuat Rotation1;
				FAnimationCompression_PerTrackUtils::DecompressRotation<false>(KeyFormat0, FormatFlags0, Rotation1, RangeData0, KeyData1);

				// Fast linear quaternion interpolation.
				FQuat BlendedQuat = FQuat::FastLerp(Rotation, Rotation1, DecompContext.KeyAlpha);
				BlendedQuat.Normalize();
				Rotation = BlendedQuat;
			}

			OutAtom.SetRotation(Rotation);
		}
		else if (DecompContext.bIsSorted)
		{
			const FCachedKey& CachedKey = CachedKeys[TrackIndex];

			// compute the blend parameters for the keys we have found
			const int32 FrameIndex0 = SegmentStartFrame[0] + CachedKey.RotFrameIndices[0];
			const int32 FrameIndex1 = SegmentStartFrame[1] + CachedKey.RotFrameIndices[1];

			const int32 Delta = FMath::Max(FrameIndex1 - FrameIndex0, 1);
			const float Remainder = FramePos - float(FrameIndex0);
			const float Alpha = DecompContext.AnimSeq->Interpolation == EAnimInterpolationType::Step ? 0.0f : (Remainder / float(Delta));

			const uint8* KeyData0 = DecompContext.CompressedByteStream + CachedKey.RotOffsets[0];

			FQuat Rotation0;
			FAnimationCompression_PerTrackUtils::DecompressRotation<false>(KeyFormat0, FormatFlags0, Rotation0, RangeData0, KeyData0);

			const uint8* KeyData1 = DecompContext.CompressedByteStream + CachedKey.RotOffsets[1];

			FQuat Rotation1;
			FAnimationCompression_PerTrackUtils::DecompressRotation<false>(KeyFormat0, FormatFlags0, Rotation1, RangeData0, KeyData1);

			// Fast linear quaternion interpolation.
			FQuat BlendedQuat = FQuat::FastLerp(Rotation0, Rotation1, Alpha);
			BlendedQuat.Normalize();

			OutAtom.SetRotation(BlendedQuat);
		}
		else
		{
			const int32 NumTrackStreams0 = NumAnimatedTrackStreams[0][RotationValueOffset];

			const uint8* OffsetNumKeysPairs0 = DecompContext.CompressedByteStream + OffsetNumKeysPairOffsets[0];
			const uint8* OffsetNumKeysPair0 = OffsetNumKeysPairs0 + (OffsetNumKeysPairSize * NumTrackStreams0);
			const uint16 NumKeys0 = *reinterpret_cast<const uint16*>(OffsetNumKeysPair0 + sizeof(uint32));

			const uint32 KeysOffset0 = AnimationCompressionUtils::UnalignedRead<uint32>(OffsetNumKeysPair0);
			const int32 TimeMarkersOffset0 = DecompContext.Segment0->ByteStreamOffset + KeysOffset0;
			const uint8* TimeMarkers0 = DecompContext.CompressedByteStream + TimeMarkersOffset0;

			int32 FrameIndex0;
			int32 FrameIndex1;
			float Alpha = AnimEncoding::TimeToIndex(DecompContext, TimeMarkers0, NumKeys0, DecompContext.Segment0->NumFrames, TimeMarkerSize[0], SegmentRelativePos0, FrameIndex0, FrameIndex1);

			const int32 TrackDataOffset0 = Align(TimeMarkersOffset0 + (NumKeys0 * TimeMarkerSize[0]), 4);
			const uint8 BytesPerKey0 = TrackStreamKeySizes[0][RotationValueOffset];
			const int32 KeyDataOffset0 = TrackDataOffset0 + (FrameIndex0 * BytesPerKey0);
			const uint8* KeyData0 = DecompContext.CompressedByteStream + KeyDataOffset0;

			FQuat Rotation;
			FAnimationCompression_PerTrackUtils::DecompressRotation<false>(KeyFormat0, FormatFlags0, Rotation, RangeData0, KeyData0);

			if (DecompContext.NeedsInterpolation)
			{
				const uint8* KeyData1 = KeyData0 + BytesPerKey0;

				FQuat Rotation1;
				FAnimationCompression_PerTrackUtils::DecompressRotation<false>(KeyFormat0, FormatFlags0, Rotation1, RangeData0, KeyData1);

				// Fast linear quaternion interpolation.
				FQuat BlendedQuat = FQuat::FastLerp(Rotation, Rotation1, Alpha);
				BlendedQuat.Normalize();
				Rotation = BlendedQuat;
			}

			OutAtom.SetRotation(Rotation);
		}
	}
}

void FAEPerTrackKeyLerpContext::GetTranslation(FTransform& OutAtom, const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) const
{
	const int32 TranslationValueOffset = DecompContext.GetTranslationValueOffset(TrackIndex);

	const uint8* PerTrackStreamFlags0 = DecompContext.CompressedByteStream + PerTrackStreamFlagOffsets[0];
	const FPerTrackFlags TranslationFlags0(PerTrackStreamFlags0[TranslationValueOffset]);

	const uint8 KeyFormat0 = TranslationFlags0.GetFormat();
	const uint8 FormatFlags0 = TranslationFlags0.GetFormatFlags();

	const int32 RangeOffset0 = RangeOffsets[0][TranslationValueOffset];
	const uint8* RangeData0 = DecompContext.CompressedByteStream + RangeOffset0;

	if (DecompContext.NeedsTwoSegments)
	{
		FVector Translation0;
		if (TranslationFlags0.IsUniform())
		{
			const int32 KeyOffset0 = UniformKeyOffsets[0][TranslationValueOffset];
			const int32 FrameStartOffset0 = UniformKeyFrameSize[0] * DecompContext.SegmentKeyIndex0;

			const uint8* KeyData0 = DecompContext.CompressedByteStream + UniformDataOffsets[0] + FrameStartOffset0 + KeyOffset0;

			FAnimationCompression_PerTrackUtils::DecompressTranslation<false>(KeyFormat0, FormatFlags0, Translation0, RangeData0, KeyData0);
		}
		else if (DecompContext.bIsSorted)
		{
			const FCachedKey& CachedKey = CachedKeys[TrackIndex];

			const uint8* KeyData0 = DecompContext.CompressedByteStream + CachedKey.TransOffsets[0];

			FAnimationCompression_PerTrackUtils::DecompressTranslation<false>(KeyFormat0, FormatFlags0, Translation0, RangeData0, KeyData0);
		}
		else
		{
			const int32 NumTrackStreams0 = NumAnimatedTrackStreams[0][TranslationValueOffset];

			const uint8* OffsetNumKeysPairs0 = DecompContext.CompressedByteStream + OffsetNumKeysPairOffsets[0];
			const uint8* OffsetNumKeysPair0 = OffsetNumKeysPairs0 + (OffsetNumKeysPairSize * NumTrackStreams0);
			const uint16 NumKeys0 = *reinterpret_cast<const uint16*>(OffsetNumKeysPair0 + sizeof(uint32));

			const uint32 KeysOffset0 = AnimationCompressionUtils::UnalignedRead<uint32>(OffsetNumKeysPair0);
			const int32 TimeMarkersOffset0 = DecompContext.Segment0->ByteStreamOffset + KeysOffset0;

			const int32 TrackDataOffset0 = Align(TimeMarkersOffset0 + (NumKeys0 * TimeMarkerSize[0]), 4);
			const uint8 BytesPerKey0 = TrackStreamKeySizes[0][TranslationValueOffset];
			const int32 KeyDataOffset0 = TrackDataOffset0 + ((NumKeys0 - 1) * BytesPerKey0);	// We need the last key of segment 0 and the first key of segment 1
			const uint8* KeyData0 = DecompContext.CompressedByteStream + KeyDataOffset0;

			FAnimationCompression_PerTrackUtils::DecompressTranslation<false>(KeyFormat0, FormatFlags0, Translation0, RangeData0, KeyData0);
		}

		const uint8* PerTrackStreamFlags1 = DecompContext.CompressedByteStream + PerTrackStreamFlagOffsets[1];
		const FPerTrackFlags TranslationFlags1(PerTrackStreamFlags1[TranslationValueOffset]);

		const uint8 KeyFormat1 = TranslationFlags1.GetFormat();
		const uint8 FormatFlags1 = TranslationFlags1.GetFormatFlags();

		const int32 RangeOffset1 = RangeOffsets[1][TranslationValueOffset];
		const uint8* RangeData1 = DecompContext.CompressedByteStream + RangeOffset1;

		FVector Translation1;
		if (TranslationFlags1.IsUniform())
		{
			const int32 KeyOffset1 = UniformKeyOffsets[1][TranslationValueOffset];
			const int32 FrameStartOffset1 = UniformKeyFrameSize[1] * DecompContext.SegmentKeyIndex1;

			const uint8* KeyData1 = DecompContext.CompressedByteStream + UniformDataOffsets[1] + FrameStartOffset1 + KeyOffset1;

			FAnimationCompression_PerTrackUtils::DecompressTranslation<false>(KeyFormat1, FormatFlags1, Translation1, RangeData1, KeyData1);
		}
		else if (DecompContext.bIsSorted)
		{
			const FCachedKey& CachedKey = CachedKeys[TrackIndex];

			const uint8* KeyData1 = DecompContext.CompressedByteStream + CachedKey.TransOffsets[1];

			FAnimationCompression_PerTrackUtils::DecompressTranslation<false>(KeyFormat1, FormatFlags1, Translation1, RangeData1, KeyData1);
		}
		else
		{
			const int32 NumTrackStreams1 = NumAnimatedTrackStreams[1][TranslationValueOffset];

			const uint8* OffsetNumKeysPairs1 = DecompContext.CompressedByteStream + OffsetNumKeysPairOffsets[1];
			const uint8* OffsetNumKeysPair1 = OffsetNumKeysPairs1 + (OffsetNumKeysPairSize * NumTrackStreams1);
			const uint16 NumKeys1 = *reinterpret_cast<const uint16*>(OffsetNumKeysPair1 + sizeof(uint32));

			const uint32 KeysOffset1 = AnimationCompressionUtils::UnalignedRead<uint32>(OffsetNumKeysPair1);
			const int32 TimeMarkersOffset1 = DecompContext.Segment1->ByteStreamOffset + KeysOffset1;

			const int32 TrackDataOffset1 = Align(TimeMarkersOffset1 + (NumKeys1 * TimeMarkerSize[1]), 4);
			// We need the last key of segment 0 and the first key of segment 1
			const int32 KeyDataOffset1 = TrackDataOffset1;	// First key!
			const uint8* KeyData1 = DecompContext.CompressedByteStream + KeyDataOffset1;

			FAnimationCompression_PerTrackUtils::DecompressTranslation<false>(KeyFormat1, FormatFlags1, Translation1, RangeData1, KeyData1);
		}

		const FVector Translation = FMath::Lerp(Translation0, Translation1, DecompContext.KeyAlpha);
		OutAtom.SetTranslation(Translation);
	}
	else
	{
		if (TranslationFlags0.IsUniform())
		{
			const int32 KeyOffset0 = UniformKeyOffsets[0][TranslationValueOffset];
			const int32 UniformKeyFrameSize0 = UniformKeyFrameSize[0];
			const int32 FrameStartOffset0 = UniformKeyFrameSize0 * DecompContext.SegmentKeyIndex0;

			const uint8* KeyData0 = DecompContext.CompressedByteStream + UniformDataOffsets[0] + FrameStartOffset0 + KeyOffset0;

			FVector Translation;
			FAnimationCompression_PerTrackUtils::DecompressTranslation<false>(KeyFormat0, FormatFlags0, Translation, RangeData0, KeyData0);

			if (DecompContext.NeedsInterpolation)
			{
				const uint8* KeyData1 = KeyData0 + UniformKeyFrameSize0;

				FVector Translation1;
				FAnimationCompression_PerTrackUtils::DecompressTranslation<false>(KeyFormat0, FormatFlags0, Translation1, RangeData0, KeyData1);

				Translation = FMath::Lerp(Translation, Translation1, DecompContext.KeyAlpha);
			}

			OutAtom.SetTranslation(Translation);
		}
		else if (DecompContext.bIsSorted)
		{
			const FCachedKey& CachedKey = CachedKeys[TrackIndex];

			// compute the blend parameters for the keys we have found
			const int32 FrameIndex0 = SegmentStartFrame[0] + CachedKey.TransFrameIndices[0];
			const int32 FrameIndex1 = SegmentStartFrame[1] + CachedKey.TransFrameIndices[1];

			const int32 Delta = FMath::Max(FrameIndex1 - FrameIndex0, 1);
			const float Remainder = FramePos - float(FrameIndex0);
			const float Alpha = DecompContext.AnimSeq->Interpolation == EAnimInterpolationType::Step ? 0.0f : (Remainder / float(Delta));

			const uint8* KeyData0 = DecompContext.CompressedByteStream + CachedKey.TransOffsets[0];

			FVector Translation0;
			FAnimationCompression_PerTrackUtils::DecompressTranslation<false>(KeyFormat0, FormatFlags0, Translation0, RangeData0, KeyData0);

			const uint8* KeyData1 = DecompContext.CompressedByteStream + CachedKey.TransOffsets[1];

			FVector Translation1;
			FAnimationCompression_PerTrackUtils::DecompressTranslation<false>(KeyFormat0, FormatFlags0, Translation1, RangeData0, KeyData1);

			FVector Translation = FMath::Lerp(Translation0, Translation1, Alpha);

			OutAtom.SetTranslation(Translation);
		}
		else
		{
			const int32 NumTrackStreams0 = NumAnimatedTrackStreams[0][TranslationValueOffset];

			const uint8* OffsetNumKeysPairs0 = DecompContext.CompressedByteStream + OffsetNumKeysPairOffsets[0];
			const uint8* OffsetNumKeysPair0 = OffsetNumKeysPairs0 + (OffsetNumKeysPairSize * NumTrackStreams0);
			const uint16 NumKeys0 = *reinterpret_cast<const uint16*>(OffsetNumKeysPair0 + sizeof(uint32));

			const uint32 KeysOffset0 = AnimationCompressionUtils::UnalignedRead<uint32>(OffsetNumKeysPair0);
			const int32 TimeMarkersOffset0 = DecompContext.Segment0->ByteStreamOffset + KeysOffset0;
			const uint8* TimeMarkers0 = DecompContext.CompressedByteStream + TimeMarkersOffset0;

			int32 FrameIndex0;
			int32 FrameIndex1;
			float Alpha = AnimEncoding::TimeToIndex(DecompContext, TimeMarkers0, NumKeys0, DecompContext.Segment0->NumFrames, TimeMarkerSize[0], SegmentRelativePos0, FrameIndex0, FrameIndex1);

			const int32 TrackDataOffset0 = Align(TimeMarkersOffset0 + (NumKeys0 * TimeMarkerSize[0]), 4);
			const uint8 BytesPerKey0 = TrackStreamKeySizes[0][TranslationValueOffset];
			const int32 KeyDataOffset0 = TrackDataOffset0 + (FrameIndex0 * BytesPerKey0);
			const uint8* KeyData0 = DecompContext.CompressedByteStream + KeyDataOffset0;

			FVector Translation;
			FAnimationCompression_PerTrackUtils::DecompressTranslation<false>(KeyFormat0, FormatFlags0, Translation, RangeData0, KeyData0);

			if (DecompContext.NeedsInterpolation)
			{
				const uint8* KeyData1 = KeyData0 + BytesPerKey0;

				FVector Translation1;
				FAnimationCompression_PerTrackUtils::DecompressTranslation<false>(KeyFormat0, FormatFlags0, Translation1, RangeData0, KeyData1);

				Translation = FMath::Lerp(Translation, Translation1, Alpha);
			}

			OutAtom.SetTranslation(Translation);
		}
	}
}

void FAEPerTrackKeyLerpContext::GetScale(FTransform& OutAtom, const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) const
{
	const int32 ScaleValueOffset = DecompContext.GetScaleValueOffset(TrackIndex);

	const uint8* PerTrackStreamFlags0 = DecompContext.CompressedByteStream + PerTrackStreamFlagOffsets[0];
	const FPerTrackFlags ScaleFlags0(PerTrackStreamFlags0[ScaleValueOffset]);

	const uint8 KeyFormat0 = ScaleFlags0.GetFormat();
	const uint8 FormatFlags0 = ScaleFlags0.GetFormatFlags();

	const int32 RangeOffset0 = RangeOffsets[0][ScaleValueOffset];
	const uint8* RangeData0 = DecompContext.CompressedByteStream + RangeOffset0;

	if (DecompContext.NeedsTwoSegments)
	{
		FVector Scale0;
		if (ScaleFlags0.IsUniform())
		{
			const int32 KeyOffset0 = UniformKeyOffsets[0][ScaleValueOffset];
			const int32 FrameStartOffset0 = UniformKeyFrameSize[0] * DecompContext.SegmentKeyIndex0;

			const uint8* KeyData0 = DecompContext.CompressedByteStream + UniformDataOffsets[0] + FrameStartOffset0 + KeyOffset0;

			FAnimationCompression_PerTrackUtils::DecompressScale<false>(KeyFormat0, FormatFlags0, Scale0, RangeData0, KeyData0);
		}
		else if (DecompContext.bIsSorted)
		{
			const FCachedKey& CachedKey = CachedKeys[TrackIndex];

			const uint8* KeyData0 = DecompContext.CompressedByteStream + CachedKey.ScaleOffsets[0];

			FAnimationCompression_PerTrackUtils::DecompressScale<false>(KeyFormat0, FormatFlags0, Scale0, RangeData0, KeyData0);
		}
		else
		{
			const int32 NumTrackStreams0 = NumAnimatedTrackStreams[0][ScaleValueOffset];

			const uint8* OffsetNumKeysPairs0 = DecompContext.CompressedByteStream + OffsetNumKeysPairOffsets[0];
			const uint8* OffsetNumKeysPair0 = OffsetNumKeysPairs0 + (OffsetNumKeysPairSize * NumTrackStreams0);
			const uint16 NumKeys0 = *reinterpret_cast<const uint16*>(OffsetNumKeysPair0 + sizeof(uint32));

			const uint32 KeysOffset0 = AnimationCompressionUtils::UnalignedRead<uint32>(OffsetNumKeysPair0);
			const int32 TimeMarkersOffset0 = DecompContext.Segment0->ByteStreamOffset + KeysOffset0;

			const int32 TrackDataOffset0 = Align(TimeMarkersOffset0 + (NumKeys0 * TimeMarkerSize[0]), 4);
			const uint8 BytesPerKey0 = TrackStreamKeySizes[0][ScaleValueOffset];
			const int32 KeyDataOffset0 = TrackDataOffset0 + ((NumKeys0 - 1) * BytesPerKey0);	// We need the last key of segment 0 and the first key of segment 1
			const uint8* KeyData0 = DecompContext.CompressedByteStream + KeyDataOffset0;

			FAnimationCompression_PerTrackUtils::DecompressScale<false>(KeyFormat0, FormatFlags0, Scale0, RangeData0, KeyData0);
		}

		const uint8* PerTrackStreamFlags1 = DecompContext.CompressedByteStream + PerTrackStreamFlagOffsets[1];
		const FPerTrackFlags ScaleFlags1(PerTrackStreamFlags1[ScaleValueOffset]);

		const uint8 KeyFormat1 = ScaleFlags1.GetFormat();
		const uint8 FormatFlags1 = ScaleFlags1.GetFormatFlags();

		const int32 RangeOffset1 = RangeOffsets[1][ScaleValueOffset];
		const uint8* RangeData1 = DecompContext.CompressedByteStream + RangeOffset1;

		FVector Scale1;
		if (ScaleFlags1.IsUniform())
		{
			const int32 KeyOffset1 = UniformKeyOffsets[1][ScaleValueOffset];
			const int32 FrameStartOffset1 = UniformKeyFrameSize[1] * DecompContext.SegmentKeyIndex1;

			const uint8* KeyData1 = DecompContext.CompressedByteStream + UniformDataOffsets[1] + FrameStartOffset1 + KeyOffset1;

			FAnimationCompression_PerTrackUtils::DecompressScale<false>(KeyFormat1, FormatFlags1, Scale1, RangeData1, KeyData1);
		}
		else if (DecompContext.bIsSorted)
		{
			const FCachedKey& CachedKey = CachedKeys[TrackIndex];

			const uint8* KeyData1 = DecompContext.CompressedByteStream + CachedKey.ScaleOffsets[1];

			FAnimationCompression_PerTrackUtils::DecompressScale<false>(KeyFormat1, FormatFlags1, Scale1, RangeData1, KeyData1);
		}
		else
		{
			const int32 NumTrackStreams1 = NumAnimatedTrackStreams[1][ScaleValueOffset];

			const uint8* OffsetNumKeysPairs1 = DecompContext.CompressedByteStream + OffsetNumKeysPairOffsets[1];
			const uint8* OffsetNumKeysPair1 = OffsetNumKeysPairs1 + (OffsetNumKeysPairSize * NumTrackStreams1);
			const uint16 NumKeys1 = *reinterpret_cast<const uint16*>(OffsetNumKeysPair1 + sizeof(uint32));

			const uint32 KeysOffset1 = AnimationCompressionUtils::UnalignedRead<uint32>(OffsetNumKeysPair1);
			const int32 TimeMarkersOffset1 = DecompContext.Segment1->ByteStreamOffset + KeysOffset1;

			const int32 TrackDataOffset1 = Align(TimeMarkersOffset1 + (NumKeys1 * TimeMarkerSize[1]), 4);
			// We need the last key of segment 0 and the first key of segment 1
			const int32 KeyDataOffset1 = TrackDataOffset1;	// First key!
			const uint8* KeyData1 = DecompContext.CompressedByteStream + KeyDataOffset1;

			FAnimationCompression_PerTrackUtils::DecompressScale<false>(KeyFormat1, FormatFlags1, Scale1, RangeData1, KeyData1);
		}

		const FVector Scale = FMath::Lerp(Scale0, Scale1, DecompContext.KeyAlpha);
		OutAtom.SetScale3D(Scale);
	}
	else
	{
		if (ScaleFlags0.IsUniform())
		{
			const int32 KeyOffset0 = UniformKeyOffsets[0][ScaleValueOffset];
			const int32 UniformKeyFrameSize0 = UniformKeyFrameSize[0];
			const int32 FrameStartOffset0 = UniformKeyFrameSize0 * DecompContext.SegmentKeyIndex0;

			const uint8* KeyData0 = DecompContext.CompressedByteStream + UniformDataOffsets[0] + FrameStartOffset0 + KeyOffset0;

			FVector Scale;
			FAnimationCompression_PerTrackUtils::DecompressScale<false>(KeyFormat0, FormatFlags0, Scale, RangeData0, KeyData0);

			if (DecompContext.NeedsInterpolation)
			{
				const uint8* KeyData1 = KeyData0 + UniformKeyFrameSize0;

				FVector Scale1;
				FAnimationCompression_PerTrackUtils::DecompressScale<false>(KeyFormat0, FormatFlags0, Scale1, RangeData0, KeyData1);

				Scale = FMath::Lerp(Scale, Scale1, DecompContext.KeyAlpha);
			}

			OutAtom.SetScale3D(Scale);
		}
		else if (DecompContext.bIsSorted)
		{
			const FCachedKey& CachedKey = CachedKeys[TrackIndex];

			// compute the blend parameters for the keys we have found
			const int32 FrameIndex0 = SegmentStartFrame[0] + CachedKey.ScaleFrameIndices[0];
			const int32 FrameIndex1 = SegmentStartFrame[1] + CachedKey.ScaleFrameIndices[1];

			const int32 Delta = FMath::Max(FrameIndex1 - FrameIndex0, 1);
			const float Remainder = FramePos - float(FrameIndex0);
			const float Alpha = DecompContext.AnimSeq->Interpolation == EAnimInterpolationType::Step ? 0.0f : (Remainder / float(Delta));

			const uint8* KeyData0 = DecompContext.CompressedByteStream + CachedKey.ScaleOffsets[0];

			FVector Scale0;
			FAnimationCompression_PerTrackUtils::DecompressScale<false>(KeyFormat0, FormatFlags0, Scale0, RangeData0, KeyData0);

			const uint8* KeyData1 = DecompContext.CompressedByteStream + CachedKey.ScaleOffsets[1];

			FVector Scale1;
			FAnimationCompression_PerTrackUtils::DecompressScale<false>(KeyFormat0, FormatFlags0, Scale1, RangeData0, KeyData1);

			FVector Scale = FMath::Lerp(Scale0, Scale1, Alpha);

			OutAtom.SetScale3D(Scale);
		}
		else
		{
			const int32 NumTrackStreams0 = NumAnimatedTrackStreams[0][ScaleValueOffset];

			const uint8* OffsetNumKeysPairs0 = DecompContext.CompressedByteStream + OffsetNumKeysPairOffsets[0];
			const uint8* OffsetNumKeysPair0 = OffsetNumKeysPairs0 + (OffsetNumKeysPairSize * NumTrackStreams0);
			const uint16 NumKeys0 = *reinterpret_cast<const uint16*>(OffsetNumKeysPair0 + sizeof(uint32));

			const uint32 KeysOffset0 = AnimationCompressionUtils::UnalignedRead<uint32>(OffsetNumKeysPair0);
			const int32 TimeMarkersOffset0 = DecompContext.Segment0->ByteStreamOffset + KeysOffset0;
			const uint8* TimeMarkers0 = DecompContext.CompressedByteStream + TimeMarkersOffset0;

			int32 FrameIndex0;
			int32 FrameIndex1;
			float Alpha = AnimEncoding::TimeToIndex(DecompContext, TimeMarkers0, NumKeys0, DecompContext.Segment0->NumFrames, TimeMarkerSize[0], SegmentRelativePos0, FrameIndex0, FrameIndex1);

			const int32 TrackDataOffset0 = Align(TimeMarkersOffset0 + (NumKeys0 * TimeMarkerSize[0]), 4);
			const uint8 BytesPerKey0 = TrackStreamKeySizes[0][ScaleValueOffset];
			const int32 KeyDataOffset0 = TrackDataOffset0 + (FrameIndex0 * BytesPerKey0);
			const uint8* KeyData0 = DecompContext.CompressedByteStream + KeyDataOffset0;

			FVector Scale;
			FAnimationCompression_PerTrackUtils::DecompressScale<false>(KeyFormat0, FormatFlags0, Scale, RangeData0, KeyData0);

			if (DecompContext.NeedsInterpolation)
			{
				const uint8* KeyData1 = KeyData0 + BytesPerKey0;

				FVector Scale1;
				FAnimationCompression_PerTrackUtils::DecompressScale<false>(KeyFormat0, FormatFlags0, Scale1, RangeData0, KeyData1);

				Scale = FMath::Lerp(Scale, Scale1, Alpha);
			}

			OutAtom.SetScale3D(Scale);
		}
	}
}
#endif