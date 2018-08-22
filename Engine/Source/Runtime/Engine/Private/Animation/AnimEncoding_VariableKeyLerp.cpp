// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimEncoding_VariableKeyLerp.cpp: Skeletal mesh animation functions.
=============================================================================*/ 

#include "AnimEncoding_VariableKeyLerp.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

/**
 * Handles the ByteSwap of compressed rotation data on import
 *
 * @param	Seq				The UAnimSequence container.
 * @param	MemoryReader	The FMemoryReader to read from.
 * @param	TrackData		The compressed data stream.
 * @param	NumKeys			The number of keys present in the stream.
 */
void AEFVariableKeyLerpShared::ByteSwapRotationIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader,
	uint8*& TrackData,
	int32 NumKeys)
{
	AEFConstantKeyLerpShared::ByteSwapRotationIn(Seq, MemoryReader, TrackData, NumKeys);

	// Load the track table if present
	if (NumKeys > 1)
	{
		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryReader(&MemoryReader, TrackData, 4); 

		// swap the track table
		const size_t EntryStride = (Seq.NumFrames > 0xFF) ? sizeof(uint16) : sizeof(uint8);
		for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, EntryStride);
		}
	}
}

/**
 * Handles the ByteSwap of compressed translation data on import
 *
 * @param	Seq				The UAnimSequence container.
 * @param	MemoryReader	The FMemoryReader to read from.
 * @param	TrackData		The compressed data stream.
 * @param	NumKeys			The number of keys present in the stream.
 */
void AEFVariableKeyLerpShared::ByteSwapTranslationIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader,
	uint8*& TrackData,
	int32 NumKeys)
{
	AEFConstantKeyLerpShared::ByteSwapTranslationIn(Seq, MemoryReader, TrackData, NumKeys);

	// Load the track table if present
	if (NumKeys > 1)
	{
		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryReader(&MemoryReader, TrackData, 4); 

		// swap the track table
		const size_t EntryStride = (Seq.NumFrames > 0xFF) ? sizeof(uint16) : sizeof(uint8);
		for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, EntryStride);
		}
	}
}

/**
 * Handles the ByteSwap of compressed Scale data on import
 *
 * @param	Seq				The UAnimSequence container.
 * @param	MemoryReader	The FMemoryReader to read from.
 * @param	TrackData		The compressed data stream.
 * @param	NumKeys			The number of keys present in the stream.
 */
void AEFVariableKeyLerpShared::ByteSwapScaleIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader,
	uint8*& TrackData,
	int32 NumKeys)
{
	AEFConstantKeyLerpShared::ByteSwapScaleIn(Seq, MemoryReader, TrackData, NumKeys);

	// Load the track table if present
	if (NumKeys > 1)
	{
		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryReader(&MemoryReader, TrackData, 4); 

		// swap the track table
		const size_t EntryStride = (Seq.NumFrames > 0xFF) ? sizeof(uint16) : sizeof(uint8);
		for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, EntryStride);
		}
	}
}
/**
 * Handles the ByteSwap of compressed rotation data on export
 *
 * @param	Seq				The UAnimSequence container.
 * @param	MemoryWriter	The FMemoryWriter to write to.
 * @param	TrackData		The compressed data stream.
 * @param	NumKeys			The number of keys to write to the stream.
 */
void AEFVariableKeyLerpShared::ByteSwapRotationOut(
	UAnimSequence& Seq, 
	FMemoryWriter& MemoryWriter,
	uint8*& TrackData,
	int32 NumKeys)
{
	AEFConstantKeyLerpShared::ByteSwapRotationOut(Seq, MemoryWriter, TrackData, NumKeys);

	// Store the track table if needed
	if (NumKeys > 1)
	{
		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryWriter(&MemoryWriter, TrackData, 4);

		// swap the track table
		const size_t EntryStride = (Seq.NumFrames > 0xFF) ? sizeof(uint16) : sizeof(uint8);
		for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, EntryStride);
		}
	}
}

/**
 * Handles the ByteSwap of compressed translation data on export
 *
 * @param	Seq				The UAnimSequence container.
 * @param	MemoryWriter	The FMemoryWriter to write to.
 * @param	TrackData		The compressed data stream.
 * @param	NumKeys			The number of keys to write to the stream.
 */
void AEFVariableKeyLerpShared::ByteSwapTranslationOut(
	UAnimSequence& Seq, 
	FMemoryWriter& MemoryWriter,
	uint8*& TrackData,
	int32 NumKeys)
{
	AEFConstantKeyLerpShared::ByteSwapTranslationOut(Seq, MemoryWriter, TrackData, NumKeys);

	// Store the track table if needed
	if (NumKeys > 1)
	{
		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryWriter(&MemoryWriter, TrackData, 4);

		// swap the track table
		const size_t EntryStride = (Seq.NumFrames > 0xFF) ? sizeof(uint16) : sizeof(uint8);
		for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, EntryStride);
		}
	}
}


/**
 * Handles the ByteSwap of compressed Scale data on export
 *
 * @param	Seq				The UAnimSequence container.
 * @param	MemoryWriter	The FMemoryWriter to write to.
 * @param	TrackData		The compressed data stream.
 * @param	NumKeys			The number of keys to write to the stream.
 */
void AEFVariableKeyLerpShared::ByteSwapScaleOut(
	UAnimSequence& Seq, 
	FMemoryWriter& MemoryWriter,
	uint8*& TrackData,
	int32 NumKeys)
{
	AEFConstantKeyLerpShared::ByteSwapScaleOut(Seq, MemoryWriter, TrackData, NumKeys);

	// Store the track table if needed
	if (NumKeys > 1)
	{
		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryWriter(&MemoryWriter, TrackData, 4);

		// swap the track table
		const size_t EntryStride = (Seq.NumFrames > 0xFF) ? sizeof(uint16) : sizeof(uint8);
		for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, EntryStride);
		}
	}
}

#if USE_SEGMENTING_CONTEXT
void AEFVariableKeyLerpShared::CreateEncodingContext(FAnimSequenceDecompressionContext& DecompContext)
{
	checkSlow(DecompContext.EncodingContext == nullptr);
	const FAnimSequenceCompressionHeader* Header = reinterpret_cast<const FAnimSequenceCompressionHeader*>(DecompContext.CompressedByteStream);
	if (Header->bIsSorted)
	{
		DecompContext.EncodingContext = new FAEVariableKeyLerpSortedContext(DecompContext);
	}
	else
	{
		DecompContext.EncodingContext = new FAEVariableKeyLerpLinearContext(DecompContext);
	}
}

void AEFVariableKeyLerpShared::ReleaseEncodingContext(FAnimSequenceDecompressionContext& DecompContext)
{
	checkSlow(DecompContext.EncodingContext != nullptr);
	delete DecompContext.EncodingContext;
	DecompContext.EncodingContext = nullptr;
}

static constexpr uint8 AEVariableKeyLerpSortedContextCachedKeyStructOffsets[3][2] =
{
	{ offsetof(FAEVariableKeyLerpSortedContext::FCachedKey, TransOffsets), offsetof(FAEVariableKeyLerpSortedContext::FCachedKey, TransFrameIndices) },
	{ offsetof(FAEVariableKeyLerpSortedContext::FCachedKey, RotOffsets), offsetof(FAEVariableKeyLerpSortedContext::FCachedKey, RotFrameIndices) },
	{ offsetof(FAEVariableKeyLerpSortedContext::FCachedKey, ScaleOffsets), offsetof(FAEVariableKeyLerpSortedContext::FCachedKey, ScaleFrameIndices) },
};

static void AdvanceCachedKeys(const FAnimSequenceDecompressionContext& DecompContext, FAEVariableKeyLerpSortedContext& EncodingContext)
{
	const int32 SampleSizes[3] = { DecompContext.PackedTranslationSize0, DecompContext.PackedRotationSize0, DecompContext.PackedScaleSize0 };

	while (true)
	{
		const uint8* PackedSampleData = EncodingContext.PackedSampleData;
		const FSortedKeyHeader KeyHeader(PackedSampleData);
		if (KeyHeader.IsEndOfStream())
		{
			break;	// Reached the end of the stream
		}
		checkSlow(KeyHeader.TrackIndex < DecompContext.NumTracks);

		const uint8 SampleType = KeyHeader.GetKeyType();
		checkSlow(SampleType <= 2);

		const int32 TimeDelta = KeyHeader.GetTimeDelta();
		const int32 FrameIndex = EncodingContext.PreviousFrameIndex + TimeDelta;

		// Swap and update
		FAEVariableKeyLerpSortedContext::FCachedKey& CachedKey = EncodingContext.CachedKeys[KeyHeader.TrackIndex];
		int32* DataOffset = reinterpret_cast<int32*>(reinterpret_cast<uint8*>(&CachedKey) + AEVariableKeyLerpSortedContextCachedKeyStructOffsets[SampleType][0]);
		int32* FrameIndicesOffset = reinterpret_cast<int32*>(reinterpret_cast<uint8*>(&CachedKey) + AEVariableKeyLerpSortedContextCachedKeyStructOffsets[SampleType][1]);
		if (FrameIndex > EncodingContext.CurrentFrameIndex && FrameIndicesOffset[1] >= EncodingContext.CurrentFrameIndex)
		{
			break;		// Reached a sample we don't need yet, stop for now
		}

		PackedSampleData += KeyHeader.GetSize();

		DataOffset[0] = DataOffset[1];
		DataOffset[1] = static_cast<int32>(PackedSampleData - DecompContext.CompressedByteStream);
		FrameIndicesOffset[0] = FrameIndicesOffset[1];
		FrameIndicesOffset[1] = FrameIndex;

		EncodingContext.PreviousFrameIndex = FrameIndex;
		PackedSampleData += SampleSizes[SampleType];
		EncodingContext.PackedSampleData = PackedSampleData;	// Update the pointer since we consumed the sample
	}
}

FAEVariableKeyLerpSortedContext::FAEVariableKeyLerpSortedContext(const FAnimSequenceDecompressionContext& DecompContext)
	: PreviousSampleAtTime(FLT_MAX)	// Very large to trigger a reset on the first Seek()
{
	CachedKeys.Empty(DecompContext.NumTracks);
}

static void Reset(const FAnimSequenceDecompressionContext& DecompContext, FAEVariableKeyLerpSortedContext& EncodingContext)
{
	EncodingContext.CachedKeys.Empty(DecompContext.NumTracks);
	EncodingContext.CachedKeys.AddZeroed(DecompContext.NumTracks);

	EncodingContext.PackedSampleData = DecompContext.CompressedByteStream + DecompContext.Segment0->ByteStreamOffset + DecompContext.RangeDataSize0;
	EncodingContext.PreviousFrameIndex = 0;
	EncodingContext.PreviousSegmentIndex = DecompContext.SegmentIndex0;
}

void FAEVariableKeyLerpSortedContext::Seek(const FAnimSequenceDecompressionContext& DecompContext, float SampleAtTime)
{
	if (SampleAtTime < PreviousSampleAtTime)
	{
		// Seeking backwards is terribly slow because we start over from the start
		Reset(DecompContext, *this);
	}
	else if (PreviousSegmentIndex != DecompContext.SegmentIndex0)
	{
		// We are seeking forward into a new segment, start over
		Reset(DecompContext, *this);
	}

	SegmentStartFrame0 = DecompContext.Segment0->StartFrame;
	SegmentStartFrame1 = DecompContext.Segment1->StartFrame;

	FramePos = DecompContext.RelativePos * float(DecompContext.AnimSeq->NumFrames - 1);

	if (DecompContext.NeedsTwoSegments)
	{
		CurrentFrameIndex = PreviousSegmentIndex == 0 ? DecompContext.SegmentKeyIndex0 : DecompContext.SegmentKeyIndex1;
	}
	else
	{
		CurrentFrameIndex = FMath::Max(DecompContext.SegmentKeyIndex1, 1);
	}

	AdvanceCachedKeys(DecompContext, *this);

	if (DecompContext.NeedsTwoSegments)
	{
		PackedSampleData = DecompContext.CompressedByteStream + DecompContext.Segment1->ByteStreamOffset + DecompContext.RangeDataSize0;
		PreviousFrameIndex = 0;
		CurrentFrameIndex = DecompContext.SegmentKeyIndex1;
		PreviousSegmentIndex = DecompContext.SegmentIndex1;

		AdvanceCachedKeys(DecompContext, *this);
	}

	PreviousSampleAtTime = SampleAtTime;
}

FAEVariableKeyLerpLinearContext::FAEVariableKeyLerpLinearContext(const FAnimSequenceDecompressionContext& DecompContext)
{
	NumAnimatedTrackStreams.Empty(DecompContext.NumTracks * DecompContext.NumStreamsPerTrack);
	NumAnimatedTrackStreams.AddUninitialized(DecompContext.NumTracks * DecompContext.NumStreamsPerTrack);

	int32 TotalNumAnimatedTrackStreams = 0;
	for (int32 TrackIndex = 0; TrackIndex < DecompContext.NumTracks; ++TrackIndex)
	{
		const FTrivialTrackFlags TrackFlags(DecompContext.TrackFlags[TrackIndex]);

		NumAnimatedTrackStreams[DecompContext.GetTranslationValueOffset(TrackIndex)] = TotalNumAnimatedTrackStreams;
		TotalNumAnimatedTrackStreams += TrackFlags.IsTranslationTrivial() ? 0 : 1;

		NumAnimatedTrackStreams[DecompContext.GetRotationValueOffset(TrackIndex)] = TotalNumAnimatedTrackStreams;
		TotalNumAnimatedTrackStreams += TrackFlags.IsRotationTrivial() ? 0 : 1;

		if (DecompContext.bHasScale)
		{
			NumAnimatedTrackStreams[DecompContext.GetScaleValueOffset(TrackIndex)] = TotalNumAnimatedTrackStreams;
			TotalNumAnimatedTrackStreams += TrackFlags.IsScaleTrivial() ? 0 : 1;
		}
	}
}

void FAEVariableKeyLerpLinearContext::Seek(const FAnimSequenceDecompressionContext& DecompContext, float SampleAtTime)
{
	float FramePos = DecompContext.RelativePos * float(DecompContext.AnimSeq->NumFrames - 1);
	float SegmentFramePos = FramePos - float(DecompContext.Segment0->StartFrame);
	SegmentRelativePos0 = SegmentFramePos / float(DecompContext.Segment0->NumFrames - 1);

	TimeMarkerSize[0] = DecompContext.Segment0->NumFrames < 256 ? sizeof(uint8) : sizeof(uint16);
	TimeMarkerSize[1] = DecompContext.Segment1->NumFrames < 256 ? sizeof(uint8) : sizeof(uint16);

	OffsetNumKeysPairs[0] = DecompContext.CompressedByteStream + DecompContext.Segment0->ByteStreamOffset + DecompContext.RangeDataSize0;
	OffsetNumKeysPairs[1] = DecompContext.CompressedByteStream + DecompContext.Segment1->ByteStreamOffset + DecompContext.RangeDataSize0;
}
#endif