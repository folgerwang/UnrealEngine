// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Baseclass for animation compression algorithms.
 *
 */

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/Function.h"
#include "Animation/AnimSequence.h"
#include "AnimationUtils.h"
#include "AnimEnums.h"
#include "AnimationCompression.h"
#include "AnimCompress.generated.h"

//Helper function for ddc key generation
uint8 MakeBitForFlag(uint32 Item, uint32 Position);

// Logic for tracking top N error items for later display
template<typename DataType, typename SortType, int MaxItems>
struct FMaxErrorStatTracker
{
public:
	FMaxErrorStatTracker()
		: CurrentLowestError(0.f)
	{
		Items.Reserve(MaxItems);
	}

	bool CanUseErrorStat(SortType NewError)
	{
		return Items.Num() < MaxItems || NewError > CurrentLowestError;
	}

	template <typename... ArgsType>
	void StoreErrorStat(SortType NewError, ArgsType&&... Args)
	{
		bool bModified = false;

		if (Items.Num() < MaxItems)
		{
			Items.Emplace(Forward<ArgsType>(Args)...);
			bModified = true;
		}
		else if(NewError > CurrentLowestError)
		{
			Items[MaxItems - 1] = DataType(Forward<ArgsType>(Args)...);
			bModified = true;
		}

		if (bModified)
		{
			Algo::Sort(Items, TGreater<>());
			CurrentLowestError = Items.Last().GetErrorValue();
		}
	}

	void LogErrorStat()
	{
		for (int ItemIndex = 0; ItemIndex < Items.Num(); ++ItemIndex)
		{
			UE_LOG(LogAnimationCompression, Display, TEXT("%i) %s"), ItemIndex+1, *Items[ItemIndex].ToText().ToString());
		}
	}

	const DataType& GetMaxErrorItem() const
	{
		return Items[0];
	}

private:
	//Storage of tracked items
	TArray<DataType> Items;

	//For ease cache current lowest error value
	SortType CurrentLowestError;
};

struct FErrorTrackerWorstBone
{
	FErrorTrackerWorstBone()
		: BoneError(0)
		, BoneErrorTime(0)
		, BoneErrorBone(0)
		, BoneErrorBoneName(NAME_None)
		, BoneErrorAnimName(NAME_None)
	{}

	FErrorTrackerWorstBone(float InBoneError, float InBoneErrorTime, int32 InBoneErrorBone, FName InBoneErrorBoneName, FName InBoneErrorAnimName)
		: BoneError(InBoneError)
		, BoneErrorTime(InBoneErrorTime)
		, BoneErrorBone(InBoneErrorBone)
		, BoneErrorBoneName(InBoneErrorBoneName)
		, BoneErrorAnimName(InBoneErrorAnimName)
	{}

	bool operator<(const FErrorTrackerWorstBone& Rhs) const
	{
		return BoneError < Rhs.BoneError;
	}

	float GetErrorValue() const { return BoneError; }

	FText ToText() const
	{
		FNumberFormattingOptions Options;
		Options.MinimumIntegralDigits = 1;
		Options.MinimumFractionalDigits = 3;

		FFormatNamedArguments Args;
		Args.Add(TEXT("BoneError"), FText::AsNumber(BoneError, &Options));
		Args.Add(TEXT("BoneErrorAnimName"), FText::FromName(BoneErrorAnimName));
		Args.Add(TEXT("BoneErrorBoneName"), FText::FromName(BoneErrorBoneName));
		Args.Add(TEXT("BoneErrorBone"), BoneErrorBone);
		Args.Add(TEXT("BoneErrorTime"), FText::AsNumber(BoneErrorTime, &Options));

		return FText::Format(NSLOCTEXT("Engine", "CompressionWorstBoneSummary", "{BoneError} in Animation {BoneErrorAnimName}, Bone : {BoneErrorBoneName}(#{BoneErrorBone}), at Time {BoneErrorTime}"), Args);
	}

	// Error of this bone
	float BoneError;

	// Time in the sequence that the error occurred at
	float BoneErrorTime;

	// Bone index the error occurred on
	int32 BoneErrorBone;

	// Bone name the error occurred on 
	FName BoneErrorBoneName;

	// Animation the error occurred on
	FName BoneErrorAnimName;
};

struct FErrorTrackerWorstAnimation
{
	FErrorTrackerWorstAnimation()
		: AvgError(0)
		, AnimName(NAME_None)
	{}

	FErrorTrackerWorstAnimation(float InAvgError, FName InMaxErrorAnimName)
		: AvgError(InAvgError)
		, AnimName(InMaxErrorAnimName)
	{}

	bool operator<(const FErrorTrackerWorstAnimation& Rhs) const
	{
		return AvgError < Rhs.AvgError;
	}

	float GetErrorValue() const { return AvgError; }

	FText ToText() const
	{
		FNumberFormattingOptions Options;
		Options.MinimumIntegralDigits = 1;
		Options.MinimumFractionalDigits = 3;

		FFormatNamedArguments Args;
		Args.Add(TEXT("AvgError"), FText::AsNumber(AvgError, &Options));
		Args.Add(TEXT("AnimName"), FText::FromName(AnimName));

		return FText::Format(NSLOCTEXT("Engine", "CompressionWorstAnimationSummary", "{AvgError} in Animation {AnimName}"), Args);
	}

private:

	// Average error of this animation
	float AvgError;

	// Animation being tracked
	FName AnimName;
};

class ENGINE_API FCompressionMemorySummary
{
public:
	FCompressionMemorySummary(bool bInEnabled);

	void GatherPreCompressionStats(UAnimSequence* Seq, int32 ProgressNumerator, int32 ProgressDenominator);

	void GatherPostCompressionStats(UAnimSequence* Seq, const TArray<FBoneData>& BoneData, double CompressionTime);

	~FCompressionMemorySummary();

private:
	bool bEnabled;
	bool bUsed;
	int32 TotalRaw;
	int32 TotalBeforeCompressed;
	int32 TotalAfterCompressed;
	int32 NumberOfAnimations;

	// Total time spent compressing animations
	double TotalCompressionExecutionTime;

	// Stats across all animations
	float ErrorTotal;
	float ErrorCount;
	float AverageError;

	// Track the largest errors on a single bone
	FMaxErrorStatTracker<FErrorTrackerWorstBone, float, 10> WorstBoneError;

	// Track the animations with the largest average error
	FMaxErrorStatTracker<FErrorTrackerWorstAnimation, float, 10> WorstAnimationError;
};

//////////////////////////////////////////////////////////////////////////
// FAnimCompressContext - Context information / storage for use during
// animation compression
struct ENGINE_API FAnimCompressContext
{
private:
	FCompressionMemorySummary	CompressionSummary;

	void GatherPreCompressionStats(UAnimSequence* Seq);

	void GatherPostCompressionStats(UAnimSequence* Seq, const TArray<FBoneData>& BoneData, double CompressionTime);


public:
	uint32						AnimIndex;
	uint32						MaxAnimations;
	bool						bAllowAlternateCompressor;
	bool						bOutput;

	FAnimCompressContext(bool bInAllowAlternateCompressor, bool bInOutput, uint32 InMaxAnimations = 1) : CompressionSummary(bInOutput), AnimIndex(0), MaxAnimations(InMaxAnimations), bAllowAlternateCompressor(bInAllowAlternateCompressor), bOutput(bInOutput) {}

	// If we are duping a compression context we don't want the CompressionSummary to output
	FAnimCompressContext(const FAnimCompressContext& Rhs) : CompressionSummary(false), AnimIndex(Rhs.AnimIndex), MaxAnimations(Rhs.MaxAnimations), bAllowAlternateCompressor(Rhs.bAllowAlternateCompressor), bOutput(Rhs.bOutput) {}

	friend class FAnimationUtils;
};

//////////////////////////////////////////////////////////////////////////
// FAnimSegmentContext - This holds the relevant intermediate information
// when compressing animation sequence segments.
struct FAnimSegmentContext
{
	int32 StartFrame;
	int32 NumFrames;

	TArray<struct FTranslationTrack> TranslationData;
	TArray<struct FRotationTrack> RotationData;
	TArray<struct FScaleTrack> ScaleData;

	AnimationCompressionFormat TranslationCompressionFormat;
	AnimationCompressionFormat RotationCompressionFormat;
	AnimationCompressionFormat ScaleCompressionFormat;

	TArray<int32> CompressedTrackOffsets;
	FCompressedOffsetData CompressedScaleOffsets;
	TArray<uint8> CompressedByteStream;
	TArray<uint8> CompressedTrivialTracksByteStream;
};

UCLASS(abstract, hidecategories=Object, MinimalAPI, EditInlineNew)
class UAnimCompress : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Name of Compression Scheme used for this asset*/
	UPROPERTY(Category=Compression, VisibleAnywhere)
	FString Description;

	/** Compression algorithms requiring a skeleton should set this value to true. */
	UPROPERTY()
	uint32 bNeedsSkeleton:1;

	/** Whether to enable segmenting or not. Needed for USE_SEGMENTING_CONTEXT (currently turned off) */
	//UPROPERTY(Category = Compression, EditAnywhere)
	UPROPERTY()
	uint32 bEnableSegmenting:1;

	/** When splitting the sequence into segments, we will try to approach this value as much as possible. */
	UPROPERTY(Category = Compression, EditAnywhere, meta = (ClampMin = "16"))
	uint32 IdealNumFramesPerSegment;

	/** When splitting the sequence into segments, we will never allow a segment with more than the maximum number of frames. */
	UPROPERTY(Category = Compression, EditAnywhere)
	uint32 MaxNumFramesPerSegment;

	/** Format for bitwise compression of translation data. */
	UPROPERTY()
	TEnumAsByte<AnimationCompressionFormat> TranslationCompressionFormat;

	/** Format for bitwise compression of rotation data. */
	UPROPERTY()
	TEnumAsByte<AnimationCompressionFormat> RotationCompressionFormat;

	/** Format for bitwise compression of scale data. */
	UPROPERTY()
	TEnumAsByte<AnimationCompressionFormat> ScaleCompressionFormat;

	/** Max error for compression of curves using remove redundant keys */
	UPROPERTY(Category = Compression, EditAnywhere)
	float MaxCurveError;

#if WITH_EDITOR
public:
	/**
	 * Reduce the number of keyframes and bitwise compress the specified sequence.
	 *
	 * @param	AnimSeq		The animation sequence to compress.
	 * @param	bOutput		If false don't generate output or compute memory savings.
	 * @return				false if a skeleton was needed by the algorithm but not provided.
	 */
	ENGINE_API bool Reduce(class UAnimSequence* AnimSeq, bool bOutput, const TArray<FBoneData>& BoneData);

	/**
	 * Reduce the number of keyframes and bitwise compress all sequences in the specified array.
	 *
	 * @param	AnimSequences	The animations to compress.
	 * @param	bOutput			If false don't generate output or compute memory savings.
	 * @return					false if a skeleton was needed by the algorithm but not provided.
	 */
	ENGINE_API bool Reduce(class UAnimSequence* AnimSeq, FAnimCompressContext& Context, const TArray<FBoneData>& BoneData);
#endif // WITH_EDITOR
protected:
#if WITH_EDITOR
	/**
	 * Implemented by child classes, this function reduces the number of keyframes in
	 * the specified sequence, given the specified skeleton (if needed).
	 *
	 * @return		true if the keyframe reduction was successful.
	 */
	virtual void DoReduction(class UAnimSequence* AnimSeq, const TArray<class FBoneData>& BoneData) PURE_VIRTUAL(UAnimCompress::DoReduction,);
#endif // WITH_EDITOR
	/**
	 * Common compression utility to remove 'redundant' position keys based on the provided delta threshold
	 *
	 * @param	Track			Position tracks to reduce
	 * @param	MaxPosDelta		Maximum local-space threshold for stationary motion
	 */
	static void FilterTrivialPositionKeys(
		TArray<struct FTranslationTrack>& Track,
		float MaxPosDelta);

	/**
	 * Common compression utility to remove 'redundant' position keys in a single track based on the provided delta threshold
	 *
	 * @param	Track			Track to reduce
	 * @param	MaxPosDelta		Maximum local-space threshold for stationary motion
	 */
	static void FilterTrivialPositionKeys(
		struct FTranslationTrack& Track,
		float MaxPosDelta);

	/**
	 * Common compression utility to remove 'redundant' rotation keys in a set of tracks based on the provided delta threshold
	 *
	 * @param	InputTracks		Array of rotation track elements to reduce
	 * @param	MaxRotDelta		Maximum angle threshold to consider stationary motion
	 */
	static void FilterTrivialRotationKeys(
		TArray<struct FRotationTrack>& InputTracks,
		float MaxRotDelta);

	/**
	 * Common compression utility to remove 'redundant' rotation keys in a set of tracks based on the provided delta threshold
	 *
	 * @param	Track			Track to reduce
	 * @param	MaxRotDelta		Maximum angle threshold to consider stationary motion
	 */
	static void FilterTrivialRotationKeys(
		struct FRotationTrack& Track,
		float MaxRotDelta);

	/**
	 * Common compression utility to remove 'redundant' Scale keys based on the provided delta threshold
	 *
	 * @param	Track			Scale tracks to reduce
	 * @param	MaxPosDelta		Maximum local-space threshold for stationary motion
	 */
	static void FilterTrivialScaleKeys(
		TArray<struct FScaleTrack>& Track,
		float MaxScaleDelta);

	/**
	 * Common compression utility to remove 'redundant' Scale keys in a single track based on the provided delta threshold
	 *
	 * @param	Track			Track to reduce
	 * @param	MaxPosDelta		Maximum local-space threshold for stationary motion
	 */
	static void FilterTrivialScaleKeys(
		struct FScaleTrack& Track,
		float MaxScaleDelta);

	
	/**
	 * Common compression utility to remove 'redundant' keys based on the provided delta thresholds
	 *
	 * @param	PositionTracks	Array of position track elements to reduce
	 * @param	RotationTracks	Array of rotation track elements to reduce
	 * @param	ScaleTracks		Array of scale track elements to reduce
	 * @param	MaxPosDelta		Maximum local-space threshold for stationary motion
	 * @param	MaxRotDelta		Maximum angle threshold to consider stationary motion
	 * @param	MaxScaleDelta	Maximum scale threshold to consider stationary motion
	 */
	static void FilterTrivialKeys(
		TArray<struct FTranslationTrack>& PositionTracks,
		TArray<struct FRotationTrack>& RotationTracks,
		TArray<struct FScaleTrack>& ScaleTracks,
		float MaxPosDelta,
		float MaxRotDelta, 
		float MaxScaleDelta);

	/**
	 * Same as above function but it will execute for every segment.
	 *
	 * @param	RawSegments		Array of segments being compressed
	 * @param	MaxPosDelta		Maximum local-space threshold for stationary motion
	 * @param	MaxRotDelta		Maximum angle threshold to consider stationary motion
	 * @param	MaxScaleDelta	Maximum scale threshold to consider stationary motion
	 */
	static void FilterTrivialKeys(
		TArray<FAnimSegmentContext>& RawSegments,
		float MaxPosDelta,
		float MaxRotDelta,
		float MaxScaleDelta);

	/**
	 * Common compression utility to retain only intermittent position keys. For example,
	 * calling with an Interval of 3 would keep every third key in the set and discard the rest
	 *
	 * @param	PositionTracks	Array of position track elements to reduce
	 * @param	StartIndex		Index at which to begin reduction
	 * @param	Interval		Interval of keys to retain
	 */
	static void FilterIntermittentPositionKeys(
		TArray<struct FTranslationTrack>& PositionTracks,
		int32 StartIndex,
		int32 Interval);

	/**
	 * Common compression utility to retain only intermittent position keys. For example,
	 * calling with an Interval of 3 would keep every third key in the set and discard the rest
	 *
	 * @param	Track			Track to reduce
	 * @param	StartIndex		Index at which to begin reduction
	 * @param	Interval		Interval of keys to retain
	 */
	static void FilterIntermittentPositionKeys(
		struct FTranslationTrack& Track,
		int32 StartIndex,
		int32 Interval);

	/**
	 * Common compression utility to retain only intermittent rotation keys. For example,
	 * calling with an Interval of 3 would keep every third key in the set and discard the rest
	 *
	 * @param	RotationTracks	Array of rotation track elements to reduce
	 * @param	StartIndex		Index at which to begin reduction
	 * @param	Interval		Interval of keys to retain
	 */
	static void FilterIntermittentRotationKeys(
		TArray<struct FRotationTrack>& RotationTracks,
		int32 StartIndex,
		int32 Interval);

	/**
	 * Common compression utility to retain only intermittent rotation keys. For example,
	 * calling with an Interval of 3 would keep every third key in the set and discard the rest
	 *
	 * @param	Track			Track to reduce
	 * @param	StartIndex		Index at which to begin reduction
	 * @param	Interval		Interval of keys to retain
	 */
	static void FilterIntermittentRotationKeys(
		struct FRotationTrack& Track,
		int32 StartIndex,
		int32 Interval);

	/**
	 * Common compression utility to retain only intermittent animation keys. For example,
	 * calling with an Interval of 3 would keep every third key in the set and discard the rest
	 *
	 * @param	PositionTracks	Array of position track elements to reduce
	 * @param	RotationTracks	Array of rotation track elements to reduce
	 * @param	StartIndex		Index at which to begin reduction
	 * @param	Interval		Interval of keys to retain
	 */
	static void FilterIntermittentKeys(
		TArray<struct FTranslationTrack>& PositionTracks,
		TArray<struct FRotationTrack>& RotationTracks,
		int32 StartIndex,
		int32 Interval);

	/**
	 * Common compression utility to populate individual rotation and translation track
	 * arrays from a set of raw animation tracks. Used as a precurser to animation compression.
	 *
	 * @param	RawAnimData			Array of raw animation tracks
	 * @param	SequenceLength		The duration of the animation in seconds
	 * @param	OutTranslationData	Translation tracks to fill
	 * @param	OutRotationData		Rotation tracks to fill
	 * @param	OutScaleData		Scale tracks to fill
	 */
	static void SeparateRawDataIntoTracks(
		const TArray<struct FRawAnimSequenceTrack>& RawAnimData,
		float SequenceLength,
		TArray<struct FTranslationTrack>& OutTranslationData,
		TArray<struct FRotationTrack>& OutRotationData, 
		TArray<struct FScaleTrack>& OutScaleData);

	/**
	 * This will populate segments from the raw animation data.
	 *
	 * @param	AnimSeq						AnimSequence being compressed
	 * @param	TranslationData				Translation tracks
	 * @param	RotationData				Rotation tracks
	 * @param	ScaleData					Scale tracks
	 * @param	IdealNumFramesPerSegment	The ideal number of frames within a segment
	 * @param	MaxNumFramesPerSegment		The maximum number of frames within a segment
	 * @param	OutRawSegments				The array of segments to populate
	 */
	static void SeparateRawDataIntoTracks(
		const UAnimSequence& AnimSeq,
		const TArray<struct FTranslationTrack>& TranslationData,
		const TArray<struct FRotationTrack>& RotationData,
		const TArray<struct FScaleTrack>& ScaleData,
		int32 IdealNumFramesPerSegment,
		int32 MaxNumFramesPerSegment,
		TArray<FAnimSegmentContext>& OutRawSegments);

	/**
	 * Common compression utility to walk an array of rotation tracks and enforce
	 * that all adjacent rotation keys are represented by shortest-arc quaternion pairs.
	 *
	 * @param	RotationData	Array of rotation track elements to reduce.
	 */
	static void PrecalculateShortestQuaternionRoutes(TArray<struct FRotationTrack>& RotationData);

public:

	/**
	 * Encodes individual key arrays into an AnimSequence using the desired bit packing formats.
	 *
	 * @param	Seq							Pointer to an Animation Sequence which will contain the bit-packed data .
	 * @param	TargetTranslationFormat		The format to use when encoding translation keys.
	 * @param	TargetRotationFormat		The format to use when encoding rotation keys.
	 * @param	TargetScaleFormat			The format to use when encoding scale keys.	 
	 * @param	TranslationData				Translation Tracks to bit-pack into the Animation Sequence.
	 * @param	RotationData				Rotation Tracks to bit-pack into the Animation Sequence.
	 * @param	ScaleData					Scale Tracks to bit-pack into the Animation Sequence.	 
	 * @param	IncludeKeyTable				true if the compressed data should also contain a table of frame indices for each key. (required by some codecs)
	 */
	static void BitwiseCompressAnimationTracks(
		UAnimSequence* Seq, 
		AnimationCompressionFormat TargetTranslationFormat, 
		AnimationCompressionFormat TargetRotationFormat,
		AnimationCompressionFormat TargetScaleFormat,
		const TArray<FTranslationTrack>& TranslationData,
		const TArray<FRotationTrack>& RotationData,
		const TArray<FScaleTrack>& ScaleData,
		bool IncludeKeyTable = false);

	/**
	 * Encodes individual key arrays into an AnimSequence using the desired bit packing formats.
	 * This action is performed for every segment supplied.
	 *
	 * @param	AnimSeq						Animation Sequence which will contain the bit-packed data.
	 * @param	TargetTranslationFormat		The format to use when encoding translation keys.
	 * @param	TargetRotationFormat		The format to use when encoding rotation keys.
	 * @param	TargetScaleFormat			The format to use when encoding scale keys.
	 * @param	RawSegments					The array of segments to compress
	 * @param	bIsSorted					For variable interpolation, is the compressed data sorted or not?
	 */
	static void BitwiseCompressAnimationTracks(
		UAnimSequence& AnimSeq,
		AnimationCompressionFormat TargetTranslationFormat,
		AnimationCompressionFormat TargetRotationFormat,
		AnimationCompressionFormat TargetScaleFormat,
		TArray<FAnimSegmentContext>& RawSegments,
		bool bIsSorted = false);

	/**
	 * Encodes individual key arrays into an AnimSequence using the desired bit packing formats.
	 * This action is performed for a single segment.
	 *
	 * @param	AnimSeq						Animation Sequence which will contain the bit-packed data.
	 * @param	TargetTranslationFormat		The format to use when encoding translation keys.
	 * @param	TargetRotationFormat		The format to use when encoding rotation keys.
	 * @param	TargetScaleFormat			The format to use when encoding scale keys.
	 * @param	RawSegment					The segment to compress
	 * @param	bIsSorted					For variable interpolation, is the compressed data sorted or not?
	 */
	static void BitwiseCompressAnimationTracks(
		const UAnimSequence& AnimSeq,
		AnimationCompressionFormat TargetTranslationFormat,
		AnimationCompressionFormat TargetRotationFormat,
		AnimationCompressionFormat TargetScaleFormat,
		FAnimSegmentContext& RawSegment,
		bool bIsSorted = false);

	/**
	 * Encodes the trivial tracks within a segment.
	 *
	 * @param	AnimSeq						Animation Sequence which will contain the bit-packed data.
	 * @param	RawSegment					The segment to compress
	 */
	static void BitwiseCompressTrivialAnimationTracks(const UAnimSequence& AnimSeq, FAnimSegmentContext& RawSegment);

	/**
	 * Coalesces the compressed data from every segment into a single contiguous array stored within the sequence.
	 *
	 * @param	AnimSeq						Animation Sequence which will contain the bit-packed data.
	 * @param	RawSegments					The array of segments to coalesce
	 * @param	bIsSorted					For variable interpolation, is the compressed data sorted or not?
	 */
	static void CoalesceCompressedSegments(UAnimSequence& AnimSeq, const TArray<FAnimSegmentContext>& RawSegments, bool bIsSorted = false);

#if WITH_EDITOR
	FString MakeDDCKey();

protected:
	virtual void PopulateDDCKey(FArchive& Ar);
#endif // WITH_EDITOR

	/**
	 * Structure that holds the range min/extent for a track.
	 */
	struct FAnimTrackRange
	{
		FVector RotMin;
		FVector RotExtent;
		FVector TransMin;
		FVector TransExtent;
		FVector ScaleMin;
		FVector ScaleExtent;
	};

	/**
	 * Utility function to append data to a byte stream.
	 *
	 * @param	ByteStream					Byte stream to append to
	 * @param	Src							Pointer to the source data to append
	 * @param	Len							Length in bytes of the source data to append
	 */
	static void UnalignedWriteToStream(TArray<uint8>& ByteStream, const void* Src, SIZE_T Len);

	/**
	* Utility function to write data to a byte stream.
	*
	* @param	ByteStream					Byte stream to write to
	* @param	StreamOffset				Offset in stream to start writing to
	* @param	Src							Pointer to the source data to write
	* @param	Len							Length in bytes of the source data to write
	*/
	static void UnalignedWriteToStream(TArray<uint8>& ByteStream, int32& StreamOffset, const void* Src, SIZE_T Len);

	/**
	 * Utility function to append a packed FVector to a byte stream.
	 *
	 * @param	ByteStream					Byte stream to append to
	 * @param	Format						Compression format to pack with
	 * @param	Vec							The FVector to pack
	 * @param	Mins						The range minimum of the input value to pack for range normalization
	 * @param	Ranges						The range extent of the input value to pack for range normalization
	 */
	static void PackVectorToStream(
		TArray<uint8>& ByteStream,
		AnimationCompressionFormat Format,
		const FVector& Vec,
		const float* Mins,
		const float* Ranges);

	/**
	* Utility function to append a packed FQuat to a byte stream.
	*
	* @param	ByteStream					Byte stream to append to
	* @param	Format						Compression format to pack with
	* @param	Vec							The FQuat to pack
	* @param	Mins						The range minimum of the input value to pack for range normalization
	* @param	Ranges						The range extent of the input value to pack for range normalization
	*/
	static void PackQuaternionToStream(
		TArray<uint8>& ByteStream,
		AnimationCompressionFormat Format,
		const FQuat& Quat,
		const float* Mins,
		const float* Ranges);

	/**
	 * Utility function that performs minimal sanity checks.
	 *
	 * @param	AnimSeq						The anim sequence to check
	 * @param	Segment						The segment to check
	 */
	static void SanityCheckTrackData(const UAnimSequence& AnimSeq, const FAnimSegmentContext& Segment);

	/**
	 * Calculates the translation track range.
	 *
	 * @param	TranslationData				The translation track data
	 * @param	Format						Compression format
	 * @param	OutMin						The output range minimum
	 * @param	OutExtent					The output range extent
	 */
	static void CalculateTrackRange(const FTranslationTrack& TranslationData, AnimationCompressionFormat Format, FVector& OutMin, FVector& OutExtent);

	/**
	 * Calculates the rotation track range.
	 *
	 * @param	TranslationData				The rotation track data
	 * @param	Format						Compression format
	 * @param	OutMin						The output range minimum
	 * @param	OutExtent					The output range extent
	 */
	static void CalculateTrackRange(const FRotationTrack& RotationData, AnimationCompressionFormat Format, FVector& OutMin, FVector& OutExtent);

	/**
	 * Calculates the sacle track range.
	 *
	 * @param	TranslationData				The scale track data
	 * @param	Format						Compression format
	 * @param	OutMin						The output range minimum
	 * @param	OutExtent					The output range extent
	 */
	static void CalculateTrackRange(const FScaleTrack& ScaleData, AnimationCompressionFormat Format, FVector& OutMin, FVector& OutExtent);

	/**
	 * Calculates the track ranges within a segment.
	 *
	 * @param	TargetTranslationFormat		Compression format for translations
	 * @param	TargetRotationFormat		Compression format for rotations
	 * @param	TargetScaleFormat			Compression format for scales
	 * @param	Segment						The segment that contains our tracks to process
	 * @param	TrackRanges					The calculated track ranges
	 */
	static void CalculateTrackRanges(
		AnimationCompressionFormat TargetTranslationFormat,
		AnimationCompressionFormat TargetRotationFormat,
		AnimationCompressionFormat TargetScaleFormat,
		const FAnimSegmentContext& Segment,
		TArray<FAnimTrackRange>& TrackRanges);

	/**
	 * Structure to wrap and represent track key flags.
	 */
	struct FTrackKeyFlags
	{
		constexpr FTrackKeyFlags() : Flags(0) {}
		constexpr explicit FTrackKeyFlags(uint8 InFlags) : Flags(InFlags) {}

		constexpr bool IsComponentNeededX() const { return (Flags & 0x1) != 0; }
		constexpr bool IsComponentNeededY() const { return (Flags & 0x2) != 0; }
		constexpr bool IsComponentNeededZ() const { return (Flags & 0x4) != 0; }

		constexpr bool IsValid() const { return (Flags & ~0x7) == 0; }

		uint8 Flags;
	};

	/**
	 * Writes the necessary track ranges to a byte stream.
	 *
	 * @param	ByteStream					Byte stream to append to
	 * @param	GetTranslationFormatFun		Function that returns the translation format for a track index
	 * @param	GetRotationFormatFun		Function that returns the rotation format for a track index
	 * @param	GetScaleFormatFun			Function that returns the scale format for a track index
	 * @param	GetTranslationFlagsFun		Function that returns the translation flags for a track index
	 * @param	GetRotationFlagsFun			Function that returns the rotation flags for a track index
	 * @param	GetScaleFlagsFun			Function that returns the scale flags for a track index
	 * @param	Segment						Segment to process
	 * @param	TrackRanges					Track ranges to pack
	 * @param	bInterleaveValues			Whether to interleave range values or not
	 */
	static void WriteTrackRanges(
		TArray<uint8>& ByteStream,
		TFunction<AnimationCompressionFormat(int32 TrackIndex)> GetTranslationFormatFun,
		TFunction<AnimationCompressionFormat(int32 TrackIndex)> GetRotationFormatFun,
		TFunction<AnimationCompressionFormat(int32 TrackIndex)> GetScaleFormatFun,
		TFunction<FTrackKeyFlags(int32 TrackIndex)> GetTranslationFlagsFun,
		TFunction<FTrackKeyFlags(int32 TrackIndex)> GetRotationFlagsFun,
		TFunction<FTrackKeyFlags(int32 TrackIndex)> GetScaleFlagsFun,
		const FAnimSegmentContext& Segment,
		const TArray<FAnimTrackRange>& TrackRanges,
		bool bInterleaveValues);

	/**
	 * Writes a segment's uniform track data to a byte stream.
	 * A track's data is uniform if no keys are removed.
	 *
	 * @param	ByteStream					Byte stream to append to
	 * @param	GetTranslationFormatFun		Function that returns the translation format for a track index
	 * @param	GetRotationFormatFun		Function that returns the rotation format for a track index
	 * @param	GetScaleFormatFun			Function that returns the scale format for a track index
	 * @param	IsTranslationUniformFun		Function that returns if a track translation's data is uniform or not for a track index
	 * @param	IsRotationUniformFun		Function that returns if a track rotation's data is uniform or not for a track index
	 * @param	IsScaleUniformFun			Function that returns if a track scale's data is uniform or not for a track index
	 * @param	PackTranslationKeyFun		Function that packs a translation key for a track index
	 * @param	PackRotationKeyFun			Function that packs a rotation key for a track index
	 * @param	PackScaleKeyFun				Function that packs a scale key for a track index
	 * @param	Segment						Segment to process
	 * @param	TrackRanges					Track ranges for the segment to process
	 */
	static void WriteUniformTrackData(
		TArray<uint8>& ByteStream,
		TFunction<AnimationCompressionFormat(int32 TrackIndex)> GetTranslationFormatFun,
		TFunction<AnimationCompressionFormat(int32 TrackIndex)> GetRotationFormatFun,
		TFunction<AnimationCompressionFormat(int32 TrackIndex)> GetScaleFormatFun,
		TFunction<bool(int32 TrackIndex)> IsTranslationUniformFun,
		TFunction<bool(int32 TrackIndex)> IsRotationUniformFun,
		TFunction<bool(int32 TrackIndex)> IsScaleUniformFun,
		TFunction<void(TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FVector& Key, const float* Mins, const float* Ranges, int32 TrackIndex)> PackTranslationKeyFun,
		TFunction<void(TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FQuat& Key, const float* Mins, const float* Ranges, int32 TrackIndex)> PackRotationKeyFun,
		TFunction<void(TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FVector& Key, const float* Mins, const float* Ranges, int32 TrackIndex)> PackScaleKeyFun,
		const FAnimSegmentContext& Segment,
		const TArray<FAnimTrackRange>& TrackRanges);

	/**
	 * Writes a segment's variable track data to a byte stream with a sorted ordering.
	 * A track's data is variable if some keys are removed.
	 *
	 * @param	ByteStream					Byte stream to append to
	 * @param	AnimSeq						The anim sequence to process
	 * @param	GetTranslationFormatFun		Function that returns the translation format for a track index
	 * @param	GetRotationFormatFun		Function that returns the rotation format for a track index
	 * @param	GetScaleFormatFun			Function that returns the scale format for a track index
	 * @param	IsTranslationUniformFun		Function that returns if a track translation's data is uniform or not for a track index
	 * @param	IsRotationUniformFun		Function that returns if a track rotation's data is uniform or not for a track index
	 * @param	IsScaleUniformFun			Function that returns if a track scale's data is uniform or not for a track index
	 * @param	PackTranslationKeyFun		Function that packs a translation key for a track index
	 * @param	PackRotationKeyFun			Function that packs a rotation key for a track index
	 * @param	PackScaleKeyFun				Function that packs a scale key for a track index
	 * @param	Segment						Segment to process
	 * @param	TrackRanges					Track ranges for the segment to process
	 */
	static void WriteSortedVariableTrackData(
		TArray<uint8>& ByteStream,
		const UAnimSequence& AnimSeq,
		TFunction<AnimationCompressionFormat(int32 TrackIndex)> GetTranslationFormatFun,
		TFunction<AnimationCompressionFormat(int32 TrackIndex)> GetRotationFormatFun,
		TFunction<AnimationCompressionFormat(int32 TrackIndex)> GetScaleFormatFun,
		TFunction<bool(int32 TrackIndex)> IsTranslationVariableFun,
		TFunction<bool(int32 TrackIndex)> IsRotationVariableFun,
		TFunction<bool(int32 TrackIndex)> IsScaleVariableFun,
		TFunction<void(TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FVector& Key, const float* Mins, const float* Ranges, int32 TrackIndex)> PackTranslationKeyFun,
		TFunction<void(TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FQuat& Key, const float* Mins, const float* Ranges, int32 TrackIndex)> PackRotationKeyFun,
		TFunction<void(TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FVector& Key, const float* Mins, const float* Ranges, int32 TrackIndex)> PackScaleKeyFun,
		FAnimSegmentContext& Segment,
		const TArray<FAnimTrackRange>& TrackRanges);

	/**
	 * Writes a segment's variable track data to a byte stream with a linear ordering.
	 * A track's data is variable if some keys are removed.
	 *
	 * @param	ByteStream					Byte stream to append to
	 * @param	AnimSeq						The anim sequence to process
	 * @param	GetTranslationFormatFun		Function that returns the translation format for a track index
	 * @param	GetRotationFormatFun		Function that returns the rotation format for a track index
	 * @param	GetScaleFormatFun			Function that returns the scale format for a track index
	 * @param	IsTranslationUniformFun		Function that returns if a track translation's data is uniform or not for a track index
	 * @param	IsRotationUniformFun		Function that returns if a track rotation's data is uniform or not for a track index
	 * @param	IsScaleUniformFun			Function that returns if a track scale's data is uniform or not for a track index
	 * @param	PackTranslationKeyFun		Function that packs a translation key for a track index
	 * @param	PackRotationKeyFun			Function that packs a rotation key for a track index
	 * @param	PackScaleKeyFun				Function that packs a scale key for a track index
	 * @param	Segment						Segment to process
	 * @param	TrackRanges					Track ranges for the segment to process
	 */
	static void WriteLinearVariableTrackData(
		TArray<uint8>& ByteStream,
		const UAnimSequence& AnimSeq,
		TFunction<AnimationCompressionFormat(int32 TrackIndex)> GetTranslationFormatFun,
		TFunction<AnimationCompressionFormat(int32 TrackIndex)> GetRotationFormatFun,
		TFunction<AnimationCompressionFormat(int32 TrackIndex)> GetScaleFormatFun,
		TFunction<bool(int32 TrackIndex)> IsTranslationVariableFun,
		TFunction<bool(int32 TrackIndex)> IsRotationVariableFun,
		TFunction<bool(int32 TrackIndex)> IsScaleVariableFun,
		TFunction<void(TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FVector& Key, const float* Mins, const float* Ranges, int32 TrackIndex)> PackTranslationKeyFun,
		TFunction<void(TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FQuat& Key, const float* Mins, const float* Ranges, int32 TrackIndex)> PackRotationKeyFun,
		TFunction<void(TArray<uint8>& ByteStream, AnimationCompressionFormat Format, const FVector& Key, const float* Mins, const float* Ranges, int32 TrackIndex)> PackScaleKeyFun,
		const FAnimSegmentContext& Segment,
		const TArray<FAnimTrackRange>& TrackRanges);

	/**
	 * Pads a byte stream to force a particular alignment for the data to follow.
	 *
	 * @param	ByteStream					Byte stream to append to
	 * @param	Alignment					Required alignment
	 * @param	Sentinel					If we need to add padding to meet the requested alignment, this is the padding value used
	 */
	static void PadByteStream(TArray<uint8>& ByteStream, const int32 Alignment, uint8 Sentinel);

	/**
	 * Default animation padding value.
	 */
	static constexpr uint8 AnimationPadSentinel = 85; //(1<<1)+(1<<3)+(1<<5)+(1<<7)
};



