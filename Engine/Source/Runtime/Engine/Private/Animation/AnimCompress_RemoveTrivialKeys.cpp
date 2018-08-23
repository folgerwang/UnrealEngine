// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimCompress_RemoveTrivialKeys.cpp: Removes trivial frames from the raw animation data.
=============================================================================*/ 

#include "Animation/AnimCompress_RemoveTrivialKeys.h"
#include "AnimationCompression.h"
#include "AnimEncoding.h"

UAnimCompress_RemoveTrivialKeys::UAnimCompress_RemoveTrivialKeys(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Description = TEXT("Remove Trivial Keys");
	MaxPosDiff = 0.0001f;
	MaxAngleDiff = 0.0003f;
	MaxScaleDiff = 0.00001f;
}

#if WITH_EDITOR
void UAnimCompress_RemoveTrivialKeys::DoReduction(UAnimSequence* AnimSeq, const TArray<FBoneData>& BoneData)
{
#if WITH_EDITORONLY_DATA
	// split the filtered data into tracks
	TArray<FTranslationTrack> TranslationData;
	TArray<FRotationTrack> RotationData;
	TArray<FScaleTrack> ScaleData;
	SeparateRawDataIntoTracks( AnimSeq->GetRawAnimationData(), AnimSeq->SequenceLength, TranslationData, RotationData, ScaleData );
	
	// remove obviously redundant keys from the source data
	FilterTrivialKeys(TranslationData, RotationData, ScaleData, TRANSLATION_ZEROING_THRESHOLD, QUATERNION_ZEROING_THRESHOLD, SCALE_ZEROING_THRESHOLD);

	// record the proper runtime decompressor to use
	AnimSeq->KeyEncodingFormat = AKF_ConstantKeyLerp;
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

void UAnimCompress_RemoveTrivialKeys::PopulateDDCKey(FArchive& Ar)
{
	Super::PopulateDDCKey(Ar);
	Ar << MaxPosDiff;
	Ar << MaxAngleDiff;
	Ar << MaxScaleDiff;
}

#endif // WITH_EDITOR
