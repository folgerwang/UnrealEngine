// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimEncoding_ConstantKeyLerp.cpp: Skeletal mesh animation functions.
=============================================================================*/ 

#include "AnimEncoding_ConstantKeyLerp.h"
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
void AEFConstantKeyLerpShared::ByteSwapRotationIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader,
	uint8*& TrackData,
	int32 NumKeys)
{
	// Calculate the effective compression (in a track with only one key, it's always stored lossless)
	const int32 EffectiveFormat = (NumKeys == 1) ? ACF_Float96NoW : (int32) Seq.RotationCompressionFormat;
	const int32 KeyComponentSize = CompressedRotationStrides[EffectiveFormat];
	const int32 KeyNumComponents = CompressedRotationNum[EffectiveFormat];

	// Load the bounds if present
	if (EffectiveFormat == ACF_IntervalFixed32NoW)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, sizeof(float));
		}
	}

	// Load the keys
	for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		for (int32 i = 0; i < KeyNumComponents; ++i)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, KeyComponentSize);
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
void AEFConstantKeyLerpShared::ByteSwapTranslationIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader,
	uint8*& TrackData,
	int32 NumKeys)
{
	// Calculate the effective compression (in a track with only one key, it's always stored lossless)
	const int32 EffectiveFormat = (NumKeys == 1) ? ACF_None : (int32) Seq.TranslationCompressionFormat;
	const int32 KeyComponentSize = CompressedTranslationStrides[EffectiveFormat];
	const int32 KeyNumComponents = CompressedTranslationNum[EffectiveFormat];

	// Load the bounds if present
	if (EffectiveFormat == ACF_IntervalFixed32NoW)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, sizeof(float));
		}
	}

	// Load the keys
	for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		for (int32 i = 0; i < KeyNumComponents; ++i)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, KeyComponentSize);
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
void AEFConstantKeyLerpShared::ByteSwapScaleIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader,
	uint8*& TrackData,
	int32 NumKeys)
{
	// Calculate the effective compression (in a track with only one key, it's always stored lossless)
	const int32 EffectiveFormat = (NumKeys == 1) ? ACF_None : (int32) Seq.ScaleCompressionFormat;
	const int32 KeyComponentSize = CompressedScaleStrides[EffectiveFormat];
	const int32 KeyNumComponents = CompressedScaleNum[EffectiveFormat];

	// Load the bounds if present
	if (EffectiveFormat == ACF_IntervalFixed32NoW)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, sizeof(float));
		}
	}

	// Load the keys
	for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		for (int32 i = 0; i < KeyNumComponents; ++i)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, KeyComponentSize);
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
void AEFConstantKeyLerpShared::ByteSwapRotationOut(
	UAnimSequence& Seq, 
	FMemoryWriter& MemoryWriter,
	uint8*& TrackData,
	int32 NumKeys)
{
	// Calculate the effective compression (in a track with only one key, it's always stored lossless)
	const int32 EffectiveFormat = (NumKeys == 1) ? ACF_Float96NoW : (int32) Seq.RotationCompressionFormat;
	const int32 KeyComponentSize = CompressedRotationStrides[EffectiveFormat];
	const int32 KeyNumComponents = CompressedRotationNum[EffectiveFormat];

	// Store the bounds if needed
	if (EffectiveFormat == ACF_IntervalFixed32NoW)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, sizeof(float));
		}
	}

	// Store the keys
	for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		for (int32 i = 0; i < KeyNumComponents; ++i)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, KeyComponentSize);
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
void AEFConstantKeyLerpShared::ByteSwapTranslationOut(
	UAnimSequence& Seq, 
	FMemoryWriter& MemoryWriter,
	uint8*& TrackData,
	int32 NumKeys)
{
	// Calculate the effective compression (in a track with only one key, it's always stored lossless)
	const int32 EffectiveFormat = (NumKeys == 1) ? ACF_None : (int32) Seq.TranslationCompressionFormat;
	const int32 KeyComponentSize = CompressedTranslationStrides[EffectiveFormat];
	const int32 KeyNumComponents = CompressedTranslationNum[EffectiveFormat];

	// Store the bounds if needed
	if (EffectiveFormat == ACF_IntervalFixed32NoW)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, sizeof(float));
		}
	}

	// Store the keys
	for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		for (int32 i = 0; i < KeyNumComponents; ++i)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, KeyComponentSize);
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
void AEFConstantKeyLerpShared::ByteSwapScaleOut(
	UAnimSequence& Seq, 
	FMemoryWriter& MemoryWriter,
	uint8*& TrackData,
	int32 NumKeys)
{
	// Calculate the effective compression (in a track with only one key, it's always stored lossless)
	const int32 EffectiveFormat = (NumKeys == 1) ? ACF_None : (int32) Seq.ScaleCompressionFormat;
	const int32 KeyComponentSize = CompressedScaleStrides[EffectiveFormat];
	const int32 KeyNumComponents = CompressedScaleNum[EffectiveFormat];

	// Store the bounds if needed
	if (EffectiveFormat == ACF_IntervalFixed32NoW)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, sizeof(float));
		}
	}

	// Store the keys
	for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		for (int32 i = 0; i < KeyNumComponents; ++i)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, KeyComponentSize);
		}
	}
}

#if USE_SEGMENTING_CONTEXT
void AEFConstantKeyLerpShared::CreateEncodingContext(FAnimSequenceDecompressionContext& DecompContext)
{
	checkSlow(DecompContext.EncodingContext == nullptr);
	DecompContext.EncodingContext = new FAEConstantKeyLerpContext(DecompContext);
}

void AEFConstantKeyLerpShared::ReleaseEncodingContext(FAnimSequenceDecompressionContext& DecompContext)
{
	checkSlow(DecompContext.EncodingContext != nullptr);
	delete DecompContext.EncodingContext;
	DecompContext.EncodingContext = nullptr;
}

FAEConstantKeyLerpContext::FAEConstantKeyLerpContext(const FAnimSequenceDecompressionContext& DecompContext)
	: KeyFrameSize(-1)
{
}

void FAEConstantKeyLerpContext::Seek(const FAnimSequenceDecompressionContext& DecompContext, float SampleAtTime)
{
	if (KeyFrameSize < 0)
	{
		// First update, cache some stuff
		UniformKeyOffsets.Empty(DecompContext.NumTracks * DecompContext.NumStreamsPerTrack);
		UniformKeyOffsets.AddUninitialized(DecompContext.NumTracks * DecompContext.NumStreamsPerTrack);

		const int32 PackedTranslationSize0 = CompressedTranslationStrides[DecompContext.Segment0->TranslationCompressionFormat] * CompressedTranslationNum[DecompContext.Segment0->TranslationCompressionFormat];
		const int32 PackedRotationSize0 = CompressedRotationStrides[DecompContext.Segment0->RotationCompressionFormat] * CompressedRotationNum[DecompContext.Segment0->RotationCompressionFormat];
		const int32 PackedScaleSize0 = DecompContext.bHasScale ? (CompressedScaleStrides[DecompContext.Segment0->ScaleCompressionFormat] * CompressedScaleNum[DecompContext.Segment0->ScaleCompressionFormat]) : 0;

		int32 KeyOffset = 0;
		for (int32 TrackIndex = 0; TrackIndex < DecompContext.NumTracks; ++TrackIndex)
		{
			const FTrivialTrackFlags TrackFlags(DecompContext.TrackFlags[TrackIndex]);

			UniformKeyOffsets[DecompContext.GetTranslationValueOffset(TrackIndex)] = KeyOffset;
			KeyOffset += TrackFlags.IsTranslationTrivial() ? 0 : PackedTranslationSize0;

			UniformKeyOffsets[DecompContext.GetRotationValueOffset(TrackIndex)] = KeyOffset;
			KeyOffset += TrackFlags.IsRotationTrivial() ? 0 : PackedRotationSize0;

			if (DecompContext.bHasScale)
			{
				UniformKeyOffsets[DecompContext.GetScaleValueOffset(TrackIndex)] = KeyOffset;
				KeyOffset += TrackFlags.IsScaleTrivial() ? 0 : PackedScaleSize0;
			}
		}

		KeyFrameSize = KeyOffset;
	}

	FrameKeysOffset[0] = DecompContext.Segment0->ByteStreamOffset + DecompContext.RangeDataSize0 + (KeyFrameSize * DecompContext.SegmentKeyIndex0);
	FrameKeysOffset[1] = DecompContext.Segment1->ByteStreamOffset + DecompContext.RangeDataSize0 + (KeyFrameSize * DecompContext.SegmentKeyIndex1);
}
#endif