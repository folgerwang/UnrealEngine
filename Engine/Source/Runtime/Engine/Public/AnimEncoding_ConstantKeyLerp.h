// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimEncoding_ConstantKeyLerp.h: Constant key compression.
=============================================================================*/ 

#pragma once

#include "CoreMinimal.h"
#include "AnimEncoding.h"
#include "AnimationCompression.h"
#include "Animation/AnimEncodingDecompressionContext.h"

class FMemoryWriter;

#if USE_SEGMENTING_CONTEXT
class FAEConstantKeyLerpContext : public FAnimEncodingDecompressionContext
{
public:
	FAEConstantKeyLerpContext(const FAnimSequenceDecompressionContext& DecompContext);
	virtual void Seek(const FAnimSequenceDecompressionContext& DecompContext, float SampleAtTime) override;

	template<int32 Format> inline FQuat GetUniformRotation(const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex, uint8 SegmentIndex) const;
	template<int32 Format> inline FVector GetUniformTranslation(const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex, uint8 SegmentIndex) const;
	template<int32 Format> inline FVector GetUniformScale(const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex, uint8 SegmentIndex) const;

	TArray<int32> UniformKeyOffsets;
	int32 KeyFrameSize;

	int32 FrameKeysOffset[2];
};
#endif

/**
 * Base class for all Animation Encoding Formats using consistently-spaced key interpolation.
 */
class AEFConstantKeyLerpShared : public AnimEncodingLegacyBase
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
		int32 NumKeysTrans) override;

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
	 * @param	TransTrackData	The compressed Scale data stream.
	 * @param	NumKeysTrans	The number of keys to write to the stream.
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
class AEFConstantKeyLerp : public AEFConstantKeyLerpShared
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
	void GetPoseRotations(
		FTransformArray& Atoms,
		const BoneTrackArray& DesiredPairs,
		FAnimSequenceDecompressionContext& DecompContext);

	/**
	 * Decompress all requested translation components from an Animation Sequence
	 *
	 * @param	Atoms			The FTransform array to fill in.
	 * @param	DesiredPairs	Array of requested bone information
	 * @param	DecompContext	The decompression context to use.
	 */
	void GetPoseTranslations(
		FTransformArray& Atoms,
		const BoneTrackArray& DesiredPairs,
		FAnimSequenceDecompressionContext& DecompContext);

	/**
	 * Decompress all requested Scale components from an Animation Sequence
	 *
	 * @param	Atoms			The FTransform array to fill in.
	 * @param	DesiredPairs	Array of requested bone information
	 * @param	DecompContext	The decompression context to use.
	 */
	void GetPoseScales(
		FTransformArray& Atoms,
		const BoneTrackArray& DesiredPairs,
		FAnimSequenceDecompressionContext& DecompContext);
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
FORCEINLINE void AEFConstantKeyLerp<FORMAT>::GetBoneAtomRotation(FTransform& OutAtom, const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex)
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
			const FAEConstantKeyLerpContext& EncodingContext = *static_cast<const FAEConstantKeyLerpContext*>(DecompContext.EncodingContext);

			const FQuat R0 = EncodingContext.GetUniformRotation<FORMAT>(DecompContext, TrackIndex, 0);

			if (DecompContext.NeedsInterpolation)
			{
				const FQuat R1 = EncodingContext.GetUniformRotation<FORMAT>(DecompContext, TrackIndex, 1);

				// Fast linear quaternion interpolation.
				FQuat BlendedQuat = FQuat::FastLerp(R0, R1, DecompContext.KeyAlpha);
				BlendedQuat.Normalize();
				OutAtom.SetRotation(BlendedQuat);
			}
			else // (Index0 == Index1)
			{
				OutAtom.SetRotation(R0);
			}
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
		DecompressRotation<ACF_Float96NoW>( R0, RotStream, RotStream );
		OutAtom.SetRotation(R0);
	}
	else
	{
		int32 Index0;
		int32 Index1;
		float Alpha = TimeToIndex(*DecompContext.AnimSeq, DecompContext.RelativePos, NumRotKeys, Index0, Index1);

		const int32 RotationStreamOffset = (FORMAT == ACF_IntervalFixed32NoW) ? (sizeof(float)*6) : 0; // offset past Min and Range data

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
			FQuat BlendedQuat = FQuat::FastLerp(R0, R1, Alpha);
			BlendedQuat.Normalize();
			OutAtom.SetRotation( BlendedQuat );
		}
		else // (Index0 == Index1)
		{
			
			// unpack a single key
			const uint8* RESTRICT KeyData= RotStream + RotationStreamOffset +(Index0*CompressedRotationStrides[FORMAT]*CompressedRotationNum[FORMAT]);
			FQuat R0;
			DecompressRotation<FORMAT>( R0, RotStream, KeyData );
			OutAtom.SetRotation( R0 );
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
FORCEINLINE void AEFConstantKeyLerp<FORMAT>::GetBoneAtomTranslation(FTransform& OutAtom, const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex)
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
			const FAEConstantKeyLerpContext& EncodingContext = *static_cast<const FAEConstantKeyLerpContext*>(DecompContext.EncodingContext);

			const FVector P0 = EncodingContext.GetUniformTranslation<FORMAT>(DecompContext, TrackIndex, 0);

			if (DecompContext.NeedsInterpolation)
			{
				const FVector P1 = EncodingContext.GetUniformTranslation<FORMAT>(DecompContext, TrackIndex, 1);

				OutAtom.SetTranslation(FMath::Lerp(P0, P1, DecompContext.KeyAlpha));
			}
			else // (Index0 == Index1)
			{
				OutAtom.SetTranslation(P0);
			}
		}

		return;
	}
#endif

	const int32* RESTRICT TrackData = DecompContext.GetCompressedTrackOffsets() + (TrackIndex * 4);
	int32 TransKeysOffset = TrackData[0];
	int32 NumTransKeys = TrackData[1];
	const uint8* RESTRICT TransStream = DecompContext.GetCompressedByteStream() + TransKeysOffset;

	int32 Index0;
	int32 Index1;
	float Alpha = TimeToIndex(*DecompContext.AnimSeq, DecompContext.RelativePos, NumTransKeys, Index0, Index1);

	const int32 TransStreamOffset = ((FORMAT == ACF_IntervalFixed32NoW) && NumTransKeys > 1) ? (sizeof(float)*6) : 0; // offset past Min and Range data

	if (Index0 != Index1)
	{
		const uint8* RESTRICT KeyData0 = TransStream + TransStreamOffset + Index0*CompressedTranslationStrides[FORMAT]*CompressedTranslationNum[FORMAT];
		const uint8* RESTRICT KeyData1 = TransStream + TransStreamOffset + Index1*CompressedTranslationStrides[FORMAT]*CompressedTranslationNum[FORMAT];
		FVector P0;
		FVector P1;
		DecompressTranslation<FORMAT>( P0, TransStream, KeyData0 );
		DecompressTranslation<FORMAT>( P1, TransStream, KeyData1 );
		OutAtom.SetTranslation( FMath::Lerp( P0, P1, Alpha ) );
	}
	else // (Index0 == Index1)
	{
		// unpack a single key
		const uint8* RESTRICT KeyData = TransStream + TransStreamOffset + Index0*CompressedTranslationStrides[FORMAT]*CompressedTranslationNum[FORMAT];
		FVector P0;
		DecompressTranslation<FORMAT>( P0, TransStream, KeyData);
		OutAtom.SetTranslation(P0);
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
FORCEINLINE void AEFConstantKeyLerp<FORMAT>::GetBoneAtomScale(FTransform& OutAtom, const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex)
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
			const FAEConstantKeyLerpContext& EncodingContext = *static_cast<const FAEConstantKeyLerpContext*>(DecompContext.EncodingContext);

			const FVector S0 = EncodingContext.GetUniformScale<FORMAT>(DecompContext, TrackIndex, 0);

			if (DecompContext.NeedsInterpolation)
			{
				const FVector S1 = EncodingContext.GetUniformScale<FORMAT>(DecompContext, TrackIndex, 1);

				OutAtom.SetScale3D(FMath::Lerp(S0, S1, DecompContext.KeyAlpha));
			}
			else // (Index0 == Index1)
			{
				OutAtom.SetScale3D(S0);
			}
		}

		return;
	}
#endif

	const int32 ScaleKeysOffset = DecompContext.GetCompressedScaleOffsets()->GetOffsetData(TrackIndex, 0);
	const int32 NumScaleKeys = DecompContext.GetCompressedScaleOffsets()->GetOffsetData(TrackIndex, 1);
	const uint8* RESTRICT ScaleStream = DecompContext.GetCompressedByteStream() + ScaleKeysOffset;

	int32 Index0;
	int32 Index1;
	float Alpha = TimeToIndex(*DecompContext.AnimSeq, DecompContext.RelativePos, NumScaleKeys, Index0, Index1);

	const int32 ScaleStreamOffset = ((FORMAT == ACF_IntervalFixed32NoW) && NumScaleKeys > 1) ? (sizeof(float)*6) : 0; // offset past Min and Range data

	if (Index0 != Index1)
	{
		const uint8* RESTRICT KeyData0 = ScaleStream + ScaleStreamOffset + Index0*CompressedScaleStrides[FORMAT]*CompressedScaleNum[FORMAT];
		const uint8* RESTRICT KeyData1 = ScaleStream + ScaleStreamOffset + Index1*CompressedScaleStrides[FORMAT]*CompressedScaleNum[FORMAT];
		FVector P0;
		FVector P1;
		DecompressScale<FORMAT>( P0, ScaleStream, KeyData0 );
		DecompressScale<FORMAT>( P1, ScaleStream, KeyData1 );
		OutAtom.SetScale3D( FMath::Lerp( P0, P1, Alpha ) );
	}
	else // (Index0 == Index1)
	{
		// unpack a single key
		const uint8* RESTRICT KeyData = ScaleStream + ScaleStreamOffset + Index0*CompressedScaleStrides[FORMAT]*CompressedScaleNum[FORMAT];
		FVector P0;
		DecompressScale<FORMAT>( P0, ScaleStream, KeyData);
		OutAtom.SetScale3D(P0);
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
inline void AEFConstantKeyLerp<FORMAT>::GetPoseRotations(
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
		AEFConstantKeyLerp<FORMAT>::GetBoneAtomRotation(BoneAtom, DecompContext, TrackIndex);
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
inline void AEFConstantKeyLerp<FORMAT>::GetPoseTranslations(
	FTransformArray& Atoms,
	const BoneTrackArray& DesiredPairs,
	FAnimSequenceDecompressionContext& DecompContext)
{
	const int32 PairCount = DesiredPairs.Num();

	//@TODO: Verify that this prefetch is helping
	// Prefetch the desired pairs array and 2 destination spots; the loop will prefetch one 2 out each iteration
	FPlatformMisc::Prefetch(&(DesiredPairs[0]));
	const int32 PrefetchCount = FMath::Min(PairCount, 1);
	for (int32 PairIndex=0; PairIndex<PairCount; ++PairIndex)
	{
		const BoneTrackPair& Pair = DesiredPairs[PairIndex];
		FPlatformMisc::Prefetch(Atoms.GetData() + Pair.AtomIndex);
	}

	for (int32 PairIndex=0; PairIndex<PairCount; ++PairIndex)
	{
		int32 PrefetchIndex = PairIndex + PrefetchCount;
		if (PrefetchIndex < PairCount)
		{
			FPlatformMisc::Prefetch(Atoms.GetData() + DesiredPairs[PrefetchIndex].AtomIndex);
		}

		const BoneTrackPair& Pair = DesiredPairs[PairIndex];
		const int32 TrackIndex = Pair.TrackIndex;
		const int32 AtomIndex = Pair.AtomIndex;
		FTransform& BoneAtom = Atoms[AtomIndex];

		// call the decoder directly (not through the vtable)
		AEFConstantKeyLerp<FORMAT>::GetBoneAtomTranslation(BoneAtom, DecompContext, TrackIndex);
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
inline void AEFConstantKeyLerp<FORMAT>::GetPoseScales(
	FTransformArray& Atoms,
	const BoneTrackArray& DesiredPairs,
	FAnimSequenceDecompressionContext& DecompContext)
{
	checkSlow(DecompContext.bHasScale);

	const int32 PairCount= DesiredPairs.Num();

	//@TODO: Verify that this prefetch is helping
	// Prefetch the desired pairs array and 2 destination spots; the loop will prefetch one 2 out each iteration
	FPlatformMisc::Prefetch(&(DesiredPairs[0]));
	const int32 PrefetchCount = FMath::Min(PairCount, 1);
	for (int32 PairIndex=0; PairIndex<PairCount; ++PairIndex)
	{
		const BoneTrackPair& Pair = DesiredPairs[PairIndex];
		FPlatformMisc::Prefetch(Atoms.GetData() + Pair.AtomIndex);
	}

	for (int32 PairIndex=0; PairIndex<PairCount; ++PairIndex)
	{
		int32 PrefetchIndex = PairIndex + PrefetchCount;
		if (PrefetchIndex < PairCount)
		{
			FPlatformMisc::Prefetch(Atoms.GetData() + DesiredPairs[PrefetchIndex].AtomIndex);
		}

		const BoneTrackPair& Pair = DesiredPairs[PairIndex];
		const int32 TrackIndex = Pair.TrackIndex;
		const int32 AtomIndex = Pair.AtomIndex;
		FTransform& BoneAtom = Atoms[AtomIndex];

		// call the decoder directly (not through the vtable)
		AEFConstantKeyLerp<FORMAT>::GetBoneAtomScale(BoneAtom, DecompContext, TrackIndex);
	}
}
#endif

#if USE_SEGMENTING_CONTEXT
template<int32 Format>
FQuat FAEConstantKeyLerpContext::GetUniformRotation(const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex, uint8 SegmentIndex) const
{
	const int32 FrameKeyOffset = UniformKeyOffsets[DecompContext.GetRotationValueOffset(TrackIndex)];
	const int32 SegmentKeyOffset = FrameKeysOffset[SegmentIndex] + FrameKeyOffset;
	const uint8* KeyData = DecompContext.CompressedByteStream + SegmentKeyOffset;

	FQuat Rotation;
	DecompressRotation<Format>(Rotation, DecompContext.TrackRangeData[SegmentIndex], KeyData);

	return Rotation;
}

template<int32 Format>
FVector FAEConstantKeyLerpContext::GetUniformTranslation(const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex, uint8 SegmentIndex) const
{
	const int32 FrameKeyOffset = UniformKeyOffsets[DecompContext.GetTranslationValueOffset(TrackIndex)];
	const int32 SegmentKeyOffset = FrameKeysOffset[SegmentIndex] + FrameKeyOffset;
	const uint8* KeyData = DecompContext.CompressedByteStream + SegmentKeyOffset;

	FVector Translation;
	DecompressTranslation<Format>(Translation, DecompContext.TrackRangeData[SegmentIndex], KeyData);

	return Translation;
}

template<int32 Format>
FVector FAEConstantKeyLerpContext::GetUniformScale(const FAnimSequenceDecompressionContext& DecompContext, int32 TrackIndex, uint8 SegmentIndex) const
{
	const int32 FrameKeyOffset = UniformKeyOffsets[DecompContext.GetScaleValueOffset(TrackIndex)];
	const int32 SegmentKeyOffset = FrameKeysOffset[SegmentIndex] + FrameKeyOffset;
	const uint8* KeyData = DecompContext.CompressedByteStream + SegmentKeyOffset;

	FVector Scale;
	DecompressScale<Format>(Scale, DecompContext.TrackRangeData[SegmentIndex], KeyData);

	return Scale;
}
#endif