// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimSequenceDecompressionContext.h"
#include "Animation/AnimEncodingDecompressionContext.h"
#include "Animation/AnimSequence.h"
#include "AnimEncoding.h"

#include <algorithm>

#if USE_SEGMENTING_CONTEXT 
FAnimSequenceDecompressionContext::FAnimSequenceDecompressionContext()
{
	memset(this, 0, sizeof(FAnimSequenceDecompressionContext));
}

FAnimSequenceDecompressionContext::FAnimSequenceDecompressionContext(const UAnimSequence* AnimSeq_)
	: FAnimSequenceDecompressionContext()
{
	Bind(AnimSeq_);
}

FAnimSequenceDecompressionContext::FAnimSequenceDecompressionContext(const FAnimSequenceDecompressionContext& Other)
{
	// We don't disable copying but we also don't retain any state
	memset(this, 0, sizeof(FAnimSequenceDecompressionContext));
}

FAnimSequenceDecompressionContext& FAnimSequenceDecompressionContext::operator=(const FAnimSequenceDecompressionContext& Other)
{
	// We don't disable copying but we also don't retain any state
	if (EncodingContext != nullptr)
	{
		checkSlow(RotationCodec != nullptr);
		RotationCodec->ReleaseEncodingContext(*this);
		checkSlow(EncodingContext == nullptr);
	}

	memset(this, 0, sizeof(FAnimSequenceDecompressionContext));
	return *this;
}

FAnimSequenceDecompressionContext::~FAnimSequenceDecompressionContext()
{
	if (EncodingContext != nullptr)
	{
		checkSlow(RotationCodec != nullptr);
		RotationCodec->ReleaseEncodingContext(*this);
		checkSlow(EncodingContext == nullptr);
	}
}

void FAnimSequenceDecompressionContext::Seek(float SampleAtTime)
{
	if (AnimSeq == nullptr)
	{
		return;	// Context is not bound and valid
	}

	// Make sure to clamp
	SampleAtTime = FMath::Clamp(SampleAtTime, 0.0f, AnimSeq->SequenceLength);

	Time = SampleAtTime;
	RelativePos = SampleAtTime / AnimSeq->SequenceLength;

	if (!AnimSeq->IsCompressedDataValid())
	{
		return;	// No compressed data
	}

	if (HasSegments)
	{
		const FAnimSequenceCompressionHeader* Header = reinterpret_cast<const FAnimSequenceCompressionHeader*>(CompressedByteStream);
		const int32 NumFrames = Header->NumFrames;

		KeyAlpha = AnimEncoding::TimeToIndex(*AnimSeq, RelativePos, NumFrames, KeyIndex0, KeyIndex1);

		SegmentIndex0 = INDEX_NONE;
		SegmentIndex1 = INDEX_NONE;
		const uint16 NumSegments = static_cast<uint16>(AnimSeq->CompressedSegments.Num());
		for (uint16 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
		{
			const FCompressedSegment& Segment = AnimSeq->CompressedSegments[SegmentIndex];
			if (KeyIndex0 >= Segment.StartFrame && KeyIndex0 < Segment.StartFrame + Segment.NumFrames)
			{
				SegmentIndex0 = SegmentIndex;

				if (KeyIndex1 >= Segment.StartFrame && KeyIndex1 < Segment.StartFrame + Segment.NumFrames)
				{
					SegmentIndex1 = SegmentIndex;
				}
				else
				{
					check(SegmentIndex + 1 < NumSegments);
					const FCompressedSegment& NextSegment = AnimSeq->CompressedSegments[SegmentIndex + 1];
					check(KeyIndex1 >= NextSegment.StartFrame && KeyIndex1 < NextSegment.StartFrame + NextSegment.NumFrames);
					SegmentIndex1 = SegmentIndex + 1;
				}
				break;
			}
		}

		check(SegmentIndex0 != INDEX_NONE && SegmentIndex1 != INDEX_NONE);

		Segment0 = &AnimSeq->CompressedSegments[SegmentIndex0];
		Segment1 = &AnimSeq->CompressedSegments[SegmentIndex1];
		NeedsTwoSegments = SegmentIndex0 != SegmentIndex1;
		NeedsInterpolation = KeyIndex0 != KeyIndex1;

		SegmentKeyIndex0 = KeyIndex0 - Segment0->StartFrame;
		SegmentKeyIndex1 = KeyIndex1 - (SegmentIndex0 == SegmentIndex1 ? Segment0->StartFrame : Segment1->StartFrame);

		// When we need two segments, we always need the last key from segment 0 and the first key from segment 1
		checkSlow(!NeedsTwoSegments || SegmentKeyIndex0 == Segment0->NumFrames - 1);
		checkSlow(!NeedsTwoSegments || SegmentKeyIndex1 == 0);

		PackedTranslationSize0 = CompressedTranslationStrides[Segment0->TranslationCompressionFormat] * CompressedTranslationNum[Segment0->TranslationCompressionFormat];
		PackedRotationSize0 = CompressedRotationStrides[Segment0->RotationCompressionFormat] * CompressedRotationNum[Segment0->RotationCompressionFormat];
		PackedScaleSize0 = bHasScale ? (CompressedScaleStrides[Segment0->ScaleCompressionFormat] * CompressedScaleNum[Segment0->ScaleCompressionFormat]) : 0;

		// TODO: Use bitset popcnt to calculate this
		RangeDataSize0 = 0;
		for (int32 CompressedTrackIndex = 0; CompressedTrackIndex < NumTracks; ++CompressedTrackIndex)
		{
			const FTrivialTrackFlags TrivialTrackFlags(TrackFlags[CompressedTrackIndex]);

			RangeDataSize0 += !TrivialTrackFlags.IsTranslationTrivial() && Segment0->TranslationCompressionFormat == ACF_IntervalFixed32NoW ? (sizeof(float) * 6) : 0;
			RangeDataSize0 += !TrivialTrackFlags.IsRotationTrivial() && Segment0->RotationCompressionFormat == ACF_IntervalFixed32NoW ? (sizeof(float) * 6) : 0;
			RangeDataSize0 += !TrivialTrackFlags.IsScaleTrivial() && Segment0->ScaleCompressionFormat == ACF_IntervalFixed32NoW ? (sizeof(float) * 6) : 0;
		}

		TrackRangeData[0] = CompressedByteStream + Segment0->ByteStreamOffset;
		TrackRangeData[1] = CompressedByteStream + Segment1->ByteStreamOffset;
	}
	else
	{
		// Legacy sequences need to refresh every update since we can't detect a stale context without a CRC
		RotationCodec = AnimSeq->RotationCodec;
		TranslationCodec = AnimSeq->TranslationCodec;
		ScaleCodec = AnimSeq->ScaleCodec;
		CompressedTrackOffsets = AnimSeq->CompressedTrackOffsets.GetData();
		CompressedByteStream = AnimSeq->CompressedByteStream.GetData();
		CompressedScaleOffsets = AnimSeq->CompressedScaleOffsets.IsValid() ? &AnimSeq->CompressedScaleOffsets : nullptr;
	}

	if (EncodingContext != nullptr)
	{
		EncodingContext->Seek(*this, SampleAtTime);
	}
}

bool FAnimSequenceDecompressionContext::IsStale(const UAnimSequence* AnimSeq_) const
{
	if (AnimSeq_ != AnimSeq)
	{
		return true;
	}

	if (AnimSeq_ == nullptr)
	{
		return false;
	}

	uint32 NewCRC = 0;
	if (AnimSeq_->CompressedSegments.Num() != 0)
	{
		const FAnimSequenceCompressionHeader* Header = reinterpret_cast<const FAnimSequenceCompressionHeader*>(AnimSeq_->CompressedByteStream.GetData());
		NewCRC = Header->SequenceCRC;
	}

	if (NewCRC != SequenceCRC || AnimSeq_->CompressedByteStream.GetData() != CompressedByteStream)
	{
		return true;
	}

	return false;
}

void FAnimSequenceDecompressionContext::Bind(const UAnimSequence* AnimSeq_)
{
	if (!IsStale(AnimSeq_))
	{
		return;	// Nothing to do
	}

#if WITH_EDITOR
	// Check if our context is bound too often, if probably means that it is being shared between multiple anim sequences which is bad for performance.
	const double CurrentTime = FPlatformTime::Seconds();
	const double ElapsedTime = CurrentTime - PreviousBindTimeStamps[0];

	const double BIND_FREQUENCY_CHECK_THRESHOLD = 1.0;		// In seconds
	if (ElapsedTime < BIND_FREQUENCY_CHECK_THRESHOLD)
	{
		// PreviousBindTimeStamps.Num() bind calls were executed on a stale context within BIND_FREQUENCY_CHECK_THRESHOLD seconds
		UE_LOG(LogAnimation, Warning, TEXT("Decompression context is bound too often. Consider reusing the same context for an anim sequence every frame or performance will be degraded."));
	}

	std::copy_n(&PreviousBindTimeStamps[1], PreviousBindTimeStamps.Num() - 1, &PreviousBindTimeStamps[0]);
	PreviousBindTimeStamps[PreviousBindTimeStamps.Num() - 1] = CurrentTime;
#endif

	// Was previously bound to something else, reset everything since we need to refresh
	if (EncodingContext != nullptr)
	{
		checkSlow(RotationCodec != nullptr);
		RotationCodec->ReleaseEncodingContext(*this);
		checkSlow(EncodingContext == nullptr);
	}

	memset(this, 0, sizeof(FAnimSequenceDecompressionContext));

	AnimSeq = AnimSeq_;

	if (AnimSeq_ == nullptr || !AnimSeq_->IsCompressedDataValid())
	{
		return;	// No compressed data
	}

	RotationCodec = AnimSeq_->RotationCodec;
	TranslationCodec = AnimSeq_->TranslationCodec;
	ScaleCodec = AnimSeq_->ScaleCodec;
	CompressedTrackOffsets = AnimSeq_->CompressedTrackOffsets.GetData();
	CompressedByteStream = AnimSeq_->CompressedByteStream.GetData();
	CompressedScaleOffsets = AnimSeq_->CompressedScaleOffsets.IsValid() ? &AnimSeq_->CompressedScaleOffsets : nullptr;
	EncodingContext = nullptr;

	HasSegments = AnimSeq_->CompressedSegments.Num() != 0;
	if (HasSegments)
	{
		const FAnimSequenceCompressionHeader* Header = reinterpret_cast<const FAnimSequenceCompressionHeader*>(CompressedByteStream);
		NumTracks = Header->NumTracks;
		SequenceCRC = Header->SequenceCRC;
		bHasScale = Header->bHasScale != 0;
		bIsSorted = Header->bIsSorted != 0;
		NumStreamsPerTrack = Header->bHasScale ? 3 : 2;

		const SIZE_T TrackFlagsOffset = sizeof(FAnimSequenceCompressionHeader);
		TrackFlags = CompressedByteStream + TrackFlagsOffset;

		const SIZE_T TrivialTrackKeysOffset = TrackFlagsOffset + Align(NumTracks * sizeof(uint8), 4);
		TrivialTrackKeys = CompressedByteStream + TrivialTrackKeysOffset;

		TrivialTrackStreamOffsets.Empty(NumTracks * NumStreamsPerTrack);
		TrivialTrackStreamOffsets.AddUninitialized(NumTracks * NumStreamsPerTrack);

		int32 TrivialTrackStreamKeyOffset = 0;
		for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
		{
			const FTrivialTrackFlags TrivialTrackFlags(TrackFlags[TrackIndex]);
			const bool IsTranslationTrivial = TrivialTrackFlags.IsTranslationTrivial();
			const bool IsRotationTrivial = TrivialTrackFlags.IsRotationTrivial();

			TrivialTrackStreamOffsets[GetTranslationValueOffset(TrackIndex)] = IsTranslationTrivial ? TrivialTrackStreamKeyOffset : -1;
			TrivialTrackStreamKeyOffset += IsTranslationTrivial ? (CompressedTranslationStrides[ACF_None] * CompressedTranslationNum[ACF_None]) : 0;

			TrivialTrackStreamOffsets[GetRotationValueOffset(TrackIndex)] = IsRotationTrivial ? TrivialTrackStreamKeyOffset : -1;
			TrivialTrackStreamKeyOffset += IsRotationTrivial ? (CompressedRotationStrides[ACF_Float96NoW] * CompressedRotationNum[ACF_Float96NoW]) : 0;

			if (Header->bHasScale)
			{
				const bool IsScaleTrivial = TrivialTrackFlags.IsScaleTrivial();

				TrivialTrackStreamOffsets[GetScaleValueOffset(TrackIndex)] = IsScaleTrivial ? TrivialTrackStreamKeyOffset : -1;
				TrivialTrackStreamKeyOffset += IsScaleTrivial ? (CompressedScaleStrides[ACF_None] * CompressedScaleNum[ACF_None]) : 0;
			}
		}

		RotationCodec->CreateEncodingContext(*this);
	}
	else
	{
		bHasScale = CompressedScaleOffsets != nullptr;
		SequenceCRC = 0;
	}
}
#endif