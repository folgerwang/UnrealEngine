// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimCompress_BitwiseCompressionOnly.cpp: Bitwise animation compression only; performs no key reduction.
=============================================================================*/ 

#include "Animation/AnimCompress_BitwiseCompressOnly.h"
#include "AnimationCompression.h"
#include "AnimEncoding.h"

UAnimCompress_BitwiseCompressOnly::UAnimCompress_BitwiseCompressOnly(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Description = TEXT("Bitwise Compress Only");
}

#if WITH_EDITOR
void UAnimCompress_BitwiseCompressOnly::DoReduction(UAnimSequence* AnimSeq, const TArray<FBoneData>& BoneData)
{
#if WITH_EDITORONLY_DATA
	// split the raw data into tracks
	TArray<FTranslationTrack> TranslationData;
	TArray<FRotationTrack> RotationData;
	TArray<FScaleTrack> ScaleData;
	SeparateRawDataIntoTracks( AnimSeq->GetRawAnimationData(), AnimSeq->SequenceLength, TranslationData, RotationData, ScaleData );

	// remove obviously redundant keys from the source data
	FilterTrivialKeys(TranslationData, RotationData, ScaleData, TRANSLATION_ZEROING_THRESHOLD, QUATERNION_ZEROING_THRESHOLD, SCALE_ZEROING_THRESHOLD);

	// record the proper runtime decompressor to use
	AnimSeq->KeyEncodingFormat = AKF_ConstantKeyLerp;
	AnimSeq->RotationCompressionFormat = RotationCompressionFormat;
	AnimSeq->TranslationCompressionFormat = TranslationCompressionFormat;
	AnimSeq->ScaleCompressionFormat = ScaleCompressionFormat;
	AnimationFormat_SetInterfaceLinks(*AnimSeq);

#if USE_SEGMENTING_CONTEXT
	if (bEnableSegmenting)
	{
		TArray<FAnimSegmentContext> RawSegments;
		SeparateRawDataIntoTracks(*AnimSeq, TranslationData, RotationData, ScaleData, IdealNumFramesPerSegment, MaxNumFramesPerSegment, RawSegments);

		BitwiseCompressAnimationTracks(
			*AnimSeq,
			static_cast<AnimationCompressionFormat>(TranslationCompressionFormat),
			static_cast<AnimationCompressionFormat>(RotationCompressionFormat),
			static_cast<AnimationCompressionFormat>(ScaleCompressionFormat),
			RawSegments);

		CoalesceCompressedSegments(*AnimSeq, RawSegments);

		AnimSeq->TranslationCompressionFormat = TranslationCompressionFormat;
		AnimSeq->RotationCompressionFormat = RotationCompressionFormat;
		AnimSeq->ScaleCompressionFormat = ScaleCompressionFormat;
	}
	else
#endif
	{
		// bitwise compress the tracks into the anim sequence buffers
		BitwiseCompressAnimationTracks(
			AnimSeq,
			static_cast<AnimationCompressionFormat>(TranslationCompressionFormat),
			static_cast<AnimationCompressionFormat>(RotationCompressionFormat),
			static_cast<AnimationCompressionFormat>(ScaleCompressionFormat),
			TranslationData,
			RotationData,
			ScaleData);
	}

	// We could be invalid, set the links again
	AnimationFormat_SetInterfaceLinks(*AnimSeq);
#endif // WITH_EDITORONLY_DATA
}

#endif // WITH_EDITOR
