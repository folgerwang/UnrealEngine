// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimEncoding_VariableKeyLerp.h: Variable key compression.
=============================================================================*/ 

#pragma once

#include "CoreMinimal.h"
#include "AnimEncoding.h"
#include "AnimationCompression.h"
#include "Animation/AnimEncodingDecompressionContext.h"
#include "AnimEncoding_ConstantKeyLerp.h"

class FMemoryWriter;

#if USE_SEGMENTING_CONTEXT
class FAEVariableKeyLerpSortedContext : public FAnimEncodingDecompressionContext
{
public:
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

	TArray<FCachedKey> CachedKeys;			// 1 Entry per track

	int32 SegmentStartFrame0;
	int32 SegmentStartFrame1;
	float FramePos;

	const uint8_t* PackedSampleData;		// The current pointer into our data stream
	int32 PreviousFrameIndex;				// The previously read frame index
	int32 CurrentFrameIndex;				// The current frame index

	float PreviousSampleAtTime;
	uint16 PreviousSegmentIndex;

	FAEVariableKeyLerpSortedContext(const FAnimSequenceDecompressionContext& DecompContext);
	virtual void Seek(const FAnimSequenceDecompressionContext& DecompContext, float SampleAtTime) override;

	template<int32 Format> inline FQuat GetSortedRotation(const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) const;
	template<int32 Format> inline FVector GetSortedTranslation(const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) const;
	template<int32 Format> inline FVector GetSortedScale(const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) const;
};

class FAEVariableKeyLerpLinearContext : public FAnimEncodingDecompressionContext
{
public:
	static constexpr int32 OffsetNumKeysPairSize = sizeof(uint32) + sizeof(uint16);

	float SegmentRelativePos0;

	uint8 TimeMarkerSize[2];	// sizeof(uint8) or sizeof(uint16)

	const uint8* OffsetNumKeysPairs[2];

	TArray<int32> NumAnimatedTrackStreams;

	FAEVariableKeyLerpLinearContext(const FAnimSequenceDecompressionContext& DecompContext);
	virtual void Seek(const FAnimSequenceDecompressionContext& DecompContext, float SampleAtTime) override;

	template<int32 Format> inline FQuat GetLinearRotation(const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) const;
	template<int32 Format> inline FVector GetLinearTranslation(const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) const;
	template<int32 Format> inline FVector GetLinearScale(const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) const;
};
#endif

/**
 * Base class for all Animation Encoding Formats using variably-spaced key interpolation.
 */
class AEFVariableKeyLerpShared : public AEFConstantKeyLerpShared
{
public:
	/**
	 * Handles the ByteSwap of compressed rotation data on import
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryReader	The FMemoryReader to read from.
	 * @param	RotTrackData	The compressed rotation data stream.
	 * @param	NumKeysRot		The number of keys present in the stream.
	 */
	virtual void ByteSwapRotationIn(
		UAnimSequence& Seq, 
		FMemoryReader& MemoryReader,
		uint8*& RotTrackData,
		int32 NumKeysRot) override;

	/**
	 * Handles the ByteSwap of compressed translation data on import
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryReader	The FMemoryReader to read from.
	 * @param	TransTrackData	The compressed translation data stream.
	 * @param	NumKeysTrans	The number of keys present in the stream.
	 */
	virtual void ByteSwapTranslationIn(
		UAnimSequence& Seq, 
		FMemoryReader& MemoryReader,
		uint8*& TransTrackData,
		int32 NumKeysTransn) override;

	/**
	 * Handles the ByteSwap of compressed Scale data on import
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryReader	The FMemoryReader to read from.
	 * @param	ScaleTrackData	The compressed Scale data stream.
	 * @param	NumKeysScale	The number of keys present in the stream.
	 */
	virtual void ByteSwapScaleIn(
		UAnimSequence& Seq, 
		FMemoryReader& MemoryReader,
		uint8*& ScaleTrackData,
		int32 NumKeysScale) override;

	/**
	 * Handles the ByteSwap of compressed rotation data on export
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryWriter	The FMemoryWriter to write to.
	 * @param	RotTrackData	The compressed rotation data stream.
	 * @param	NumKeysRot		The number of keys to write to the stream.
	 */
	virtual void ByteSwapRotationOut(
		UAnimSequence& Seq, 
		FMemoryWriter& MemoryWriter,
		uint8*& RotTrackData,
		int32 NumKeysRot) override;

	/**
	 * Handles the ByteSwap of compressed translation data on export
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryWriter	The FMemoryWriter to write to.
	 * @param	TransTrackData	The compressed translation data stream.
	 * @param	NumKeysTrans	The number of keys to write to the stream.
	 */
	virtual void ByteSwapTranslationOut(
		UAnimSequence& Seq, 
		FMemoryWriter& MemoryWriter,
		uint8*& TransTrackData,
		int32 NumKeysTrans) override;

	/**
	 * Handles the ByteSwap of compressed Scale data on export
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryWriter	The FMemoryWriter to write to.
	 * @param	ScaleTrackData	The compressed Scale data stream.
	 * @param	NumKeysScale	The number of keys to write to the stream.
	 */
	virtual void ByteSwapScaleOut(
		UAnimSequence& Seq, 
		FMemoryWriter& MemoryWriter,
		uint8*& ScaleTrackData,
		int32 NumKeysScale) override;

#if USE_SEGMENTING_CONTEXT
	virtual void CreateEncodingContext(FAnimSequenceDecompressionContext& DecompContext) override;
	virtual void ReleaseEncodingContext(FAnimSequenceDecompressionContext& DecompContext) override;
#endif
};

template<int32 FORMAT>
class AEFVariableKeyLerp : public AEFVariableKeyLerpShared
{
public:
	/**
	 * Decompress the Rotation component of a BoneAtom
	 *
	 * @param	OutAtom			The FTransform to fill in.
	 * @param	DecompContext	The decompression context to use.
	 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
	 */
	virtual void GetBoneAtomRotation(FTransform& OutAtom, const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) override;

	/**
	 * Decompress the Translation component of a BoneAtom
	 *
	 * @param	OutAtom			The FTransform to fill in.
	 * @param	DecompContext	The decompression context to use.
	 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
	 */
	virtual void GetBoneAtomTranslation(FTransform& OutAtom, const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) override;

	/**
	 * Decompress the Scale component of a BoneAtom
	 *
	 * @param	OutAtom			The FTransform to fill in.
	 * @param	DecompContext	The decompression context to use.
	 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
	 */
	virtual void GetBoneAtomScale(FTransform& OutAtom, const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) override;

#if USE_ANIMATION_CODEC_BATCH_SOLVER

	/**
	 * Decompress all requested rotation components from an Animation Sequence
	 *
	 * @param	Atoms			The FTransform array to fill in.
	 * @param	DesiredPairs	Array of requested bone information
	 * @param	DecompContext	The decompression context to use.
	 */
	virtual void GetPoseRotations(
		FTransformArray& Atoms,
		const BoneTrackArray& DesiredPairs,
		FAnimSequenceDecompressionContext& DecompContext) override;

	/**
	 * Decompress all requested translation components from an Animation Sequence
	 *
	 * @param	Atoms			The FTransform array to fill in.
	 * @param	DesiredPairs	Array of requested bone information
	 * @param	DecompContext	The decompression context to use.
	 */
	virtual void GetPoseTranslations(
		FTransformArray& Atoms,
		const BoneTrackArray& DesiredPairs,
		FAnimSequenceDecompressionContext& DecompContext) override;

	/**
	 * Decompress all requested Scale components from an Animation Sequence
	 *
	 * @param	Atoms			The FTransform array to fill in.
	 * @param	DesiredPairs	Array of requested bone information
	 * @param	DecompContext	The decompression context to use.
	 */
	virtual void GetPoseScales(
		FTransformArray& Atoms,
		const BoneTrackArray& DesiredPairs,
		FAnimSequenceDecompressionContext& DecompContext) override;
#endif

};


/**
 * Decompress the Rotation component of a BoneAtom
 *
 * @param	OutAtom			The FTransform to fill in.
 * @param	DecompContext	The decompression context to use.
 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
 */
template<int32 FORMAT>
FORCEINLINE_DEBUGGABLE void AEFVariableKeyLerp<FORMAT>::GetBoneAtomRotation(FTransform& OutAtom, const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex)
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
			FQuat Rotation;
			if (DecompContext.bIsSorted)
			{
				const FAEVariableKeyLerpSortedContext& EncodingContext = *static_cast<const FAEVariableKeyLerpSortedContext*>(DecompContext.EncodingContext);
				Rotation = EncodingContext.GetSortedRotation<FORMAT>(DecompContext, TrackIndex);
			}
			else
			{
				const FAEVariableKeyLerpLinearContext& EncodingContext = *static_cast<const FAEVariableKeyLerpLinearContext*>(DecompContext.EncodingContext);
				Rotation = EncodingContext.GetLinearRotation<FORMAT>(DecompContext, TrackIndex);
			}

			OutAtom.SetRotation(Rotation);
		}

		return;
	}

#endif

	const int32* RESTRICT TrackData = DecompContext.GetCompressedTrackOffsets() + (TrackIndex * 4);
	int32 RotKeysOffset = TrackData[2];
	int32 NumRotKeys = TrackData[3];
	const uint8* RESTRICT RotStream = DecompContext.GetCompressedByteStream() + RotKeysOffset;

	if (NumRotKeys == 1)
	{
		// For a rotation track of n=1 keys, the single key is packed as an FQuatFloat96NoW.
		FQuat R0;
		DecompressRotation<ACF_Float96NoW>( R0 , RotStream, RotStream );
		OutAtom.SetRotation(R0);
	}
	else
	{
		const int32 RotationStreamOffset = (FORMAT == ACF_IntervalFixed32NoW) ? (sizeof(float)*6) : 0; // offset past Min and Range data
		const uint8* RESTRICT FrameTable= RotStream + RotationStreamOffset +(NumRotKeys*CompressedRotationStrides[FORMAT]*CompressedRotationNum[FORMAT]);
		FrameTable = Align(FrameTable, 4);

		int32 Index0;
		int32 Index1;
		float Alpha = TimeToIndex(*DecompContext.AnimSeq, FrameTable, DecompContext.RelativePos, NumRotKeys, Index0, Index1);


		if (Index0 != Index1)
		{
			// unpack and lerp between the two nearest keys
			const uint8* RESTRICT KeyData0= RotStream + RotationStreamOffset +(Index0*CompressedRotationStrides[FORMAT]*CompressedRotationNum[FORMAT]);
			const uint8* RESTRICT KeyData1= RotStream + RotationStreamOffset +(Index1*CompressedRotationStrides[FORMAT]*CompressedRotationNum[FORMAT]);
			FQuat R0;
			FQuat R1;
			DecompressRotation<FORMAT>( R0, RotStream, KeyData0 );
			DecompressRotation<FORMAT>( R1, RotStream, KeyData1 );

			// Fast linear quaternion interpolation.
			FQuat RLerped = FQuat::FastLerp(R0, R1, Alpha);
			RLerped.Normalize();
			OutAtom.SetRotation(RLerped);
		}
		else // (Index0 == Index1)
		{
			// unpack a single key
			const uint8* RESTRICT KeyData= RotStream + RotationStreamOffset +(Index0*CompressedRotationStrides[FORMAT]*CompressedRotationNum[FORMAT]);

			FQuat R0;
			DecompressRotation<FORMAT>( R0, RotStream, KeyData );

			OutAtom.SetRotation(R0);
		}
	}

}

/**
 * Decompress the Translation component of a BoneAtom
 *
 * @param	OutAtom			The FTransform to fill in.
 * @param	DecompContext	The decompression context to use.
 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
 */
template<int32 FORMAT>
FORCEINLINE_DEBUGGABLE void AEFVariableKeyLerp<FORMAT>::GetBoneAtomTranslation(FTransform& OutAtom, const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex)
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
			FVector Translation;
			if (DecompContext.bIsSorted)
			{
				const FAEVariableKeyLerpSortedContext& EncodingContext = *static_cast<const FAEVariableKeyLerpSortedContext*>(DecompContext.EncodingContext);
				Translation = EncodingContext.GetSortedTranslation<FORMAT>(DecompContext, TrackIndex);
			}
			else
			{
				const FAEVariableKeyLerpLinearContext& EncodingContext = *static_cast<const FAEVariableKeyLerpLinearContext*>(DecompContext.EncodingContext);
				Translation = EncodingContext.GetLinearTranslation<FORMAT>(DecompContext, TrackIndex);
			}

			OutAtom.SetTranslation(Translation);
		}

		return;
	}

#endif

	const int32* RESTRICT TrackData = DecompContext.GetCompressedTrackOffsets() + (TrackIndex * 4);
	int32 TransKeysOffset = TrackData[0];
	int32 NumTransKeys = TrackData[1];
	const uint8* RESTRICT TransStream = DecompContext.GetCompressedByteStream() + TransKeysOffset;

	const uint8* RESTRICT FrameTable= TransStream +(NumTransKeys*CompressedTranslationStrides[FORMAT]*CompressedTranslationNum[FORMAT]);
	FrameTable= Align(FrameTable, 4);

	int32 Index0;
	int32 Index1;
	float Alpha = TimeToIndex(*DecompContext.AnimSeq, FrameTable, DecompContext.RelativePos, NumTransKeys, Index0, Index1);
	const int32 TransStreamOffset = ((FORMAT == ACF_IntervalFixed32NoW) && NumTransKeys > 1) ? (sizeof(float)*6) : 0; // offset past Min and Range data

	if (Index0 != Index1)
	{
		FVector P0;
		FVector P1;
		const uint8* RESTRICT KeyData0 = TransStream + TransStreamOffset + Index0*CompressedTranslationStrides[FORMAT]*CompressedTranslationNum[FORMAT];
		const uint8* RESTRICT KeyData1 = TransStream + TransStreamOffset + Index1*CompressedTranslationStrides[FORMAT]*CompressedTranslationNum[FORMAT];
		DecompressTranslation<FORMAT>( P0, TransStream, KeyData0 );
		DecompressTranslation<FORMAT>( P1, TransStream, KeyData1 );
		OutAtom.SetTranslation( FMath::Lerp( P0, P1, Alpha ) );
	}
	else // (Index0 == Index1)
	{
		// unpack a single key
		FVector P0;
		const uint8* RESTRICT KeyData = TransStream + TransStreamOffset + Index0*CompressedTranslationStrides[FORMAT]*CompressedTranslationNum[FORMAT];
		DecompressTranslation<FORMAT>( P0, TransStream, KeyData);
		OutAtom.SetTranslation( P0 );
	}
}

/**
 * Decompress the Scale component of a BoneAtom
 *
 * @param	OutAtom			The FTransform to fill in.
 * @param	DecompContext	The decompression context to use.
 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
 */
template<int32 FORMAT>
FORCEINLINE_DEBUGGABLE void AEFVariableKeyLerp<FORMAT>::GetBoneAtomScale(FTransform& OutAtom, const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex)
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
			FVector Scale;
			if (DecompContext.bIsSorted)
			{
				const FAEVariableKeyLerpSortedContext& EncodingContext = *static_cast<const FAEVariableKeyLerpSortedContext*>(DecompContext.EncodingContext);
				Scale = EncodingContext.GetSortedScale<FORMAT>(DecompContext, TrackIndex);
			}
			else
			{
				const FAEVariableKeyLerpLinearContext& EncodingContext = *static_cast<const FAEVariableKeyLerpLinearContext*>(DecompContext.EncodingContext);
				Scale = EncodingContext.GetLinearScale<FORMAT>(DecompContext, TrackIndex);
			}

			OutAtom.SetScale3D(Scale);
		}

		return;
	}
#endif

	const int32 ScaleKeysOffset = DecompContext.GetCompressedScaleOffsets()->GetOffsetData(TrackIndex, 0);
	const int32 NumScaleKeys = DecompContext.GetCompressedScaleOffsets()->GetOffsetData(TrackIndex, 1);
	const uint8* RESTRICT ScaleStream = DecompContext.GetCompressedByteStream() + ScaleKeysOffset;

	const uint8* RESTRICT FrameTable= ScaleStream +(NumScaleKeys*CompressedScaleStrides[FORMAT]*CompressedScaleNum[FORMAT]);
	FrameTable= Align(FrameTable, 4);

	int32 Index0;
	int32 Index1;
	float Alpha = TimeToIndex(*DecompContext.AnimSeq, FrameTable, DecompContext.RelativePos, NumScaleKeys, Index0, Index1);
	const int32 ScaleStreamOffset = ((FORMAT == ACF_IntervalFixed32NoW) && NumScaleKeys > 1) ? (sizeof(float)*6) : 0; // offset past Min and Range data

	if (Index0 != Index1)
	{
		FVector P0;
		FVector P1;
		const uint8* RESTRICT KeyData0 = ScaleStream + ScaleStreamOffset + Index0*CompressedScaleStrides[FORMAT]*CompressedScaleNum[FORMAT];
		const uint8* RESTRICT KeyData1 = ScaleStream + ScaleStreamOffset + Index1*CompressedScaleStrides[FORMAT]*CompressedScaleNum[FORMAT];
		DecompressScale<FORMAT>( P0, ScaleStream, KeyData0 );
		DecompressScale<FORMAT>( P1, ScaleStream, KeyData1 );
		OutAtom.SetScale3D( FMath::Lerp( P0, P1, Alpha ) );
	}
	else // (Index0 == Index1)
	{
		// unpack a single key
		FVector P0;
		const uint8* RESTRICT KeyData = ScaleStream + ScaleStreamOffset + Index0*CompressedScaleStrides[FORMAT]*CompressedScaleNum[FORMAT];
		DecompressScale<FORMAT>( P0, ScaleStream, KeyData);
		OutAtom.SetScale3D( P0 );
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
template<int32 FORMAT>
void AEFVariableKeyLerp<FORMAT>::GetPoseRotations(
	FTransformArray& Atoms,
	const BoneTrackArray& DesiredPairs,
	FAnimSequenceDecompressionContext& DecompContext)
{
	const int32 PairCount = DesiredPairs.Num();

	for (int32 PairIndex=0; PairIndex<PairCount; ++PairIndex)
	{
		const BoneTrackPair& Pair = DesiredPairs[PairIndex];
		const int32 TrackIndex = Pair.TrackIndex;
		const int32 AtomIndex = Pair.AtomIndex;
		FTransform& BoneAtom = Atoms[AtomIndex];

		// call the decoder directly (not through the vtable)
		AEFVariableKeyLerp<FORMAT>::GetBoneAtomRotation(BoneAtom, DecompContext, TrackIndex);
	}
}

/**
 * Decompress all requested translation components from an Animation Sequence
 *
 * @param	Atoms			The FTransform array to fill in.
 * @param	DesiredPairs	Array of requested bone information
 * @param	DecompContext	The decompression context to use.
 */
template<int32 FORMAT>
void AEFVariableKeyLerp<FORMAT>::GetPoseTranslations(
	FTransformArray& Atoms,
	const BoneTrackArray& DesiredPairs,
	FAnimSequenceDecompressionContext& DecompContext)
{
	const int32 PairCount= DesiredPairs.Num();

	for (int32 PairIndex=0; PairIndex<PairCount; ++PairIndex)
	{
		const BoneTrackPair& Pair = DesiredPairs[PairIndex];
		const int32 TrackIndex = Pair.TrackIndex;
		const int32 AtomIndex = Pair.AtomIndex;
		FTransform& BoneAtom = Atoms[AtomIndex];

		// call the decoder directly (not through the vtable)
		AEFVariableKeyLerp<FORMAT>::GetBoneAtomTranslation(BoneAtom, DecompContext, TrackIndex);
	}
}

/**
 * Decompress all requested Scale components from an Animation Sequence
 *
 * @param	Atoms			The FTransform array to fill in.
 * @param	DesiredPairs	Array of requested bone information
 * @param	DecompContext	The decompression context to use.
 */
template<int32 FORMAT>
void AEFVariableKeyLerp<FORMAT>::GetPoseScales(
	FTransformArray& Atoms,
	const BoneTrackArray& DesiredPairs,
	FAnimSequenceDecompressionContext& DecompContext)
{
	checkSlow(DecompContext.bHasScale);

	const int32 PairCount= DesiredPairs.Num();

	for (int32 PairIndex=0; PairIndex<PairCount; ++PairIndex)
	{
		const BoneTrackPair& Pair = DesiredPairs[PairIndex];
		const int32 TrackIndex = Pair.TrackIndex;
		const int32 AtomIndex = Pair.AtomIndex;
		FTransform& BoneAtom = Atoms[AtomIndex];

		// call the decoder directly (not through the vtable)
		AEFVariableKeyLerp<FORMAT>::GetBoneAtomScale(BoneAtom, DecompContext, TrackIndex);
	}
}
#endif

#if USE_SEGMENTING_CONTEXT
template<int32 Format>
FQuat FAEVariableKeyLerpSortedContext::GetSortedRotation(const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) const
{
	const FCachedKey& CachedKey = CachedKeys[TrackIndex];

	float Alpha;
	if (DecompContext.NeedsTwoSegments)
	{
		Alpha = DecompContext.KeyAlpha;
	}
	else
	{
		// compute the blend parameters for the keys we have found
		const int32 FrameIndex0 = SegmentStartFrame0 + CachedKey.RotFrameIndices[0];
		const int32 FrameIndex1 = SegmentStartFrame1 + CachedKey.RotFrameIndices[1];

		const int32 Delta = FMath::Max(FrameIndex1 - FrameIndex0, 1);
		const float Remainder = FramePos - float(FrameIndex0);
		Alpha = DecompContext.AnimSeq->Interpolation == EAnimInterpolationType::Step ? 0.0f : (Remainder / float(Delta));
	}

	const uint8* KeyData0 = DecompContext.CompressedByteStream + CachedKey.RotOffsets[0];

	FQuat Rotation0;
	DecompressRotation<Format, false>(Rotation0, DecompContext.TrackRangeData[0], KeyData0);

	const uint8* KeyData1 = DecompContext.CompressedByteStream + CachedKey.RotOffsets[1];

	FQuat Rotation1;
	DecompressRotation<Format, false>(Rotation1, DecompContext.TrackRangeData[1], KeyData1);

	// Fast linear quaternion interpolation.
	FQuat BlendedQuat = FQuat::FastLerp(Rotation0, Rotation1, Alpha);
	BlendedQuat.Normalize();

	return BlendedQuat;
}

template<int32 Format>
FVector FAEVariableKeyLerpSortedContext::GetSortedTranslation(const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) const
{
	const FCachedKey& CachedKey = CachedKeys[TrackIndex];

	float Alpha;
	if (DecompContext.NeedsTwoSegments)
	{
		Alpha = DecompContext.KeyAlpha;
	}
	else
	{
		// compute the blend parameters for the keys we have found
		const int32 FrameIndex0 = SegmentStartFrame0 + CachedKey.TransFrameIndices[0];
		const int32 FrameIndex1 = SegmentStartFrame1 + CachedKey.TransFrameIndices[1];

		const int32 Delta = FMath::Max(FrameIndex1 - FrameIndex0, 1);
		const float Remainder = FramePos - float(FrameIndex0);
		Alpha = DecompContext.AnimSeq->Interpolation == EAnimInterpolationType::Step ? 0.0f : (Remainder / float(Delta));
	}

	const uint8* KeyData0 = DecompContext.CompressedByteStream + CachedKey.TransOffsets[0];

	FVector Translation0;
	DecompressTranslation<Format, false>(Translation0, DecompContext.TrackRangeData[0], KeyData0);

	const uint8* KeyData1 = DecompContext.CompressedByteStream + CachedKey.TransOffsets[1];

	FVector Translation1;
	DecompressTranslation<Format, false>(Translation1, DecompContext.TrackRangeData[1], KeyData1);

	return FMath::Lerp(Translation0, Translation1, Alpha);
}

template<int32 Format>
FVector FAEVariableKeyLerpSortedContext::GetSortedScale(const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) const
{
	const FCachedKey& CachedKey = CachedKeys[TrackIndex];

	float Alpha;
	if (DecompContext.NeedsTwoSegments)
	{
		Alpha = DecompContext.KeyAlpha;
	}
	else
	{
		// compute the blend parameters for the keys we have found
		const int32 FrameIndex0 = SegmentStartFrame0 + CachedKey.ScaleFrameIndices[0];
		const int32 FrameIndex1 = SegmentStartFrame1 + CachedKey.ScaleFrameIndices[1];

		const int32 Delta = FMath::Max(FrameIndex1 - FrameIndex0, 1);
		const float Remainder = FramePos - float(FrameIndex0);
		Alpha = DecompContext.AnimSeq->Interpolation == EAnimInterpolationType::Step ? 0.0f : (Remainder / float(Delta));
	}

	const uint8* KeyData0 = DecompContext.CompressedByteStream + CachedKey.ScaleOffsets[0];

	FVector Scale0;
	DecompressScale<Format, false>(Scale0, DecompContext.TrackRangeData[0], KeyData0);

	const uint8* KeyData1 = DecompContext.CompressedByteStream + CachedKey.ScaleOffsets[1];

	FVector Scale1;
	DecompressScale<Format, false>(Scale1, DecompContext.TrackRangeData[1], KeyData1);

	return FMath::Lerp(Scale0, Scale1, Alpha);
}

template<int32 Format>
FQuat FAEVariableKeyLerpLinearContext::GetLinearRotation(const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) const
{
	const int32 NumTrackStreams = NumAnimatedTrackStreams[DecompContext.GetRotationValueOffset(TrackIndex)];

	const uint8* OffsetNumKeysPair0 = OffsetNumKeysPairs[0] + (OffsetNumKeysPairSize * NumTrackStreams);
	const uint16 NumKeys0 = *reinterpret_cast<const uint16*>(OffsetNumKeysPair0 + sizeof(uint32));

	const uint32 KeysOffset0 = AnimationCompressionUtils::UnalignedRead<uint32>(OffsetNumKeysPair0);
	const int32 TimeMarkersOffset0 = DecompContext.Segment0->ByteStreamOffset + KeysOffset0;

	int32 FrameIndex0;
	int32 FrameIndex1;
	float Alpha;
	if (DecompContext.NeedsTwoSegments)
	{
		FrameIndex0 = NumKeys0 - 1;
		FrameIndex1 = 0;
		Alpha = DecompContext.KeyAlpha;
	}
	else
	{
		const uint8* TimeMarkers0 = DecompContext.CompressedByteStream + TimeMarkersOffset0;

		Alpha = AnimEncoding::TimeToIndex(DecompContext, TimeMarkers0, NumKeys0, DecompContext.Segment0->NumFrames, TimeMarkerSize[0], SegmentRelativePos0, FrameIndex0, FrameIndex1);
	}

	const int32 TrackDataOffset0 = Align(TimeMarkersOffset0 + (NumKeys0 * TimeMarkerSize[0]), 4);
	const int32 KeyDataOffset0 = TrackDataOffset0 + (FrameIndex0 * CompressedRotationStrides[Format] * CompressedRotationNum[Format]);
	const uint8* KeyData0 = DecompContext.CompressedByteStream + KeyDataOffset0;

	FQuat Rotation;
	DecompressRotation<Format>(Rotation, DecompContext.TrackRangeData[0], KeyData0);

	if (DecompContext.NeedsInterpolation)
	{
		const uint8* OffsetNumKeysPair1 = OffsetNumKeysPairs[1] + (OffsetNumKeysPairSize * NumTrackStreams);
		const uint32 KeysOffset1 = AnimationCompressionUtils::UnalignedRead<uint32>(OffsetNumKeysPair1);
		const uint16 NumKeys1 = *reinterpret_cast<const uint16*>(OffsetNumKeysPair1 + sizeof(uint32));

		const int32 TimeMarkersOffset1 = DecompContext.Segment1->ByteStreamOffset + KeysOffset1;
		const int32 TrackDataOffset1 = Align(TimeMarkersOffset1 + (NumKeys1 * TimeMarkerSize[1]), 4);
		const int32 KeyDataOffset1 = TrackDataOffset1 + (FrameIndex1 * CompressedRotationStrides[Format] * CompressedRotationNum[Format]);
		const uint8* KeyData1 = DecompContext.CompressedByteStream + KeyDataOffset1;

		FQuat Rotation1;
		DecompressRotation<Format>(Rotation1, DecompContext.TrackRangeData[1], KeyData1);

		// Fast linear quaternion interpolation.
		FQuat BlendedQuat = FQuat::FastLerp(Rotation, Rotation1, Alpha);
		BlendedQuat.Normalize();
		Rotation = BlendedQuat;
	}

	return Rotation;
}

template<int32 Format>
FVector FAEVariableKeyLerpLinearContext::GetLinearTranslation(const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) const
{
	const int32 NumTrackStreams = NumAnimatedTrackStreams[DecompContext.GetTranslationValueOffset(TrackIndex)];

	const uint8* OffsetNumKeysPair0 = OffsetNumKeysPairs[0] + (OffsetNumKeysPairSize * NumTrackStreams);
	const uint16 NumKeys0 = *reinterpret_cast<const uint16*>(OffsetNumKeysPair0 + sizeof(uint32));

	const uint32 KeysOffset0 = AnimationCompressionUtils::UnalignedRead<uint32>(OffsetNumKeysPair0);
	const int32 TimeMarkersOffset0 = DecompContext.Segment0->ByteStreamOffset + KeysOffset0;

	int32 FrameIndex0;
	int32 FrameIndex1;
	float Alpha;
	if (DecompContext.NeedsTwoSegments)
	{
		FrameIndex0 = NumKeys0 - 1;
		FrameIndex1 = 0;
		Alpha = DecompContext.KeyAlpha;
	}
	else
	{
		const uint8* TimeMarkers0 = DecompContext.CompressedByteStream + TimeMarkersOffset0;

		Alpha = AnimEncoding::TimeToIndex(DecompContext, TimeMarkers0, NumKeys0, DecompContext.Segment0->NumFrames, TimeMarkerSize[0], SegmentRelativePos0, FrameIndex0, FrameIndex1);
	}

	const int32 TrackDataOffset0 = Align(TimeMarkersOffset0 + (NumKeys0 * TimeMarkerSize[0]), 4);
	const int32 KeyDataOffset0 = TrackDataOffset0 + (FrameIndex0 * CompressedTranslationStrides[Format] * CompressedTranslationNum[Format]);
	const uint8* KeyData0 = DecompContext.CompressedByteStream + KeyDataOffset0;

	FVector Translation;
	DecompressTranslation<Format>(Translation, DecompContext.TrackRangeData[0], KeyData0);

	if (DecompContext.NeedsInterpolation)
	{
		const uint8* OffsetNumKeysPair1 = OffsetNumKeysPairs[1] + (OffsetNumKeysPairSize * NumTrackStreams);
		const uint32 KeysOffset1 = AnimationCompressionUtils::UnalignedRead<uint32>(OffsetNumKeysPair1);
		const uint16 NumKeys1 = *reinterpret_cast<const uint16*>(OffsetNumKeysPair1 + sizeof(uint32));

		const int32 TimeMarkersOffset1 = DecompContext.Segment1->ByteStreamOffset + KeysOffset1;
		const int32 TrackDataOffset1 = Align(TimeMarkersOffset1 + (NumKeys1 * TimeMarkerSize[1]), 4);
		const int32 KeyDataOffset1 = TrackDataOffset1 + (FrameIndex1 * CompressedTranslationStrides[Format] * CompressedTranslationNum[Format]);
		const uint8* KeyData1 = DecompContext.CompressedByteStream + KeyDataOffset1;

		FVector Translation1;
		DecompressTranslation<Format>(Translation1, DecompContext.TrackRangeData[1], KeyData1);

		Translation = FMath::Lerp(Translation, Translation1, Alpha);
	}

	return Translation;
}

template<int32 Format>
FVector FAEVariableKeyLerpLinearContext::GetLinearScale(const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex) const
{
	const int32 NumTrackStreams = NumAnimatedTrackStreams[DecompContext.GetScaleValueOffset(TrackIndex)];

	const uint8* OffsetNumKeysPair0 = OffsetNumKeysPairs[0] + (OffsetNumKeysPairSize * NumTrackStreams);
	const uint16 NumKeys0 = *reinterpret_cast<const uint16*>(OffsetNumKeysPair0 + sizeof(uint32));

	const uint32 KeysOffset0 = AnimationCompressionUtils::UnalignedRead<uint32>(OffsetNumKeysPair0);
	const int32 TimeMarkersOffset0 = DecompContext.Segment0->ByteStreamOffset + KeysOffset0;

	int32 FrameIndex0;
	int32 FrameIndex1;
	float Alpha;
	if (DecompContext.NeedsTwoSegments)
	{
		FrameIndex0 = NumKeys0 - 1;
		FrameIndex1 = 0;
		Alpha = DecompContext.KeyAlpha;
	}
	else
	{
		const uint8* TimeMarkers0 = DecompContext.CompressedByteStream + TimeMarkersOffset0;

		Alpha = AnimEncoding::TimeToIndex(DecompContext, TimeMarkers0, NumKeys0, DecompContext.Segment0->NumFrames, TimeMarkerSize[0], SegmentRelativePos0, FrameIndex0, FrameIndex1);
	}

	const int32 TrackDataOffset0 = Align(TimeMarkersOffset0 + (NumKeys0 * TimeMarkerSize[0]), 4);
	const int32 KeyDataOffset0 = TrackDataOffset0 + (FrameIndex0 * CompressedScaleStrides[Format] * CompressedScaleNum[Format]);
	const uint8* KeyData0 = DecompContext.CompressedByteStream + KeyDataOffset0;

	FVector Scale;
	DecompressScale<Format>(Scale, DecompContext.TrackRangeData[0], KeyData0);

	if (DecompContext.NeedsInterpolation)
	{
		const uint8* OffsetNumKeysPair1 = OffsetNumKeysPairs[1] + (OffsetNumKeysPairSize * NumTrackStreams);
		const uint32 KeysOffset1 = AnimationCompressionUtils::UnalignedRead<uint32>(OffsetNumKeysPair1);
		const uint16 NumKeys1 = *reinterpret_cast<const uint16*>(OffsetNumKeysPair1 + sizeof(uint32));

		const int32 TimeMarkersOffset1 = DecompContext.Segment1->ByteStreamOffset + KeysOffset1;
		const int32 TrackDataOffset1 = Align(TimeMarkersOffset1 + (NumKeys1 * TimeMarkerSize[1]), 4);
		const int32 KeyDataOffset1 = TrackDataOffset1 + (FrameIndex1 * CompressedScaleStrides[Format] * CompressedScaleNum[Format]);
		const uint8* KeyData1 = DecompContext.CompressedByteStream + KeyDataOffset1;

		FVector Scale1;
		DecompressScale<Format>(Scale1, DecompContext.TrackRangeData[1], KeyData1);

		Scale = FMath::Lerp(Scale, Scale1, Alpha);
	}

	return Scale;
}
#endif