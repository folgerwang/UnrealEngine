// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSequenceID.h"
#include "Evaluation/MovieSceneEvaluationKey.h"
#include "Evaluation/MovieSceneSegment.h"
#include "MovieSceneFrameMigration.h"
#include "Evaluation/MovieSceneTrackIdentifier.h"
#include "MovieSceneEvaluationField.generated.h"

struct FFrameNumber;
struct FMovieSceneSequenceHierarchy;
struct IMovieSceneSequenceTemplateStore;
class UMovieSceneSequence;

/** A pointer to a track held within an evaluation template */
USTRUCT()
struct FMovieSceneEvaluationFieldTrackPtr
{
	GENERATED_BODY()

	/**
	 * Default constructor
	 */
	FMovieSceneEvaluationFieldTrackPtr(){}

	/**
	 * Construction from a sequence ID, and the index of the track within that sequence's track list
	 */
	FMovieSceneEvaluationFieldTrackPtr(FMovieSceneSequenceIDRef InSequenceID, FMovieSceneTrackIdentifier InTrackIdentifier)
		: SequenceID(InSequenceID)
		, TrackIdentifier(InTrackIdentifier)
	{}

	/**
	 * Check for equality
	 */
	friend bool operator==(FMovieSceneEvaluationFieldTrackPtr A, FMovieSceneEvaluationFieldTrackPtr B)
	{
		return A.TrackIdentifier == B.TrackIdentifier && A.SequenceID == B.SequenceID;
	}

	/**
	 * Get a hashed representation of this type
	 */
	friend uint32 GetTypeHash(FMovieSceneEvaluationFieldTrackPtr LHS)
	{
		return HashCombine(GetTypeHash(LHS.TrackIdentifier), GetTypeHash(LHS.SequenceID));
	}

	/** The sequence ID that identifies to which sequence the track belongs */
	UPROPERTY()
	FMovieSceneSequenceID SequenceID;

	/** The Identifier of the track inside the track map (see FMovieSceneEvaluationTemplate::Tracks) */
	UPROPERTY()
	FMovieSceneTrackIdentifier TrackIdentifier;
};

/** A pointer to a particular segment of a track held within an evaluation template */
USTRUCT()
struct FMovieSceneEvaluationFieldSegmentPtr : public FMovieSceneEvaluationFieldTrackPtr
{
	GENERATED_BODY()

	/**
	 * Default constructor
	 */
	FMovieSceneEvaluationFieldSegmentPtr(){}

	/**
	 * Construction from a sequence ID, and the index of the track within that sequence's track list
	 */
	FMovieSceneEvaluationFieldSegmentPtr(FMovieSceneSequenceIDRef InSequenceID, FMovieSceneTrackIdentifier InTrackIdentifier, FMovieSceneSegmentIdentifier InSegmentID)
		: FMovieSceneEvaluationFieldTrackPtr(InSequenceID, InTrackIdentifier)
		, SegmentID(InSegmentID)
	{}

	/**
	 * Check for equality
	 */
	friend bool operator==(FMovieSceneEvaluationFieldSegmentPtr A, FMovieSceneEvaluationFieldSegmentPtr B)
	{
		return A.SegmentID == B.SegmentID && A.TrackIdentifier == B.TrackIdentifier && A.SequenceID == B.SequenceID;
	}

	/**
	 * Get a hashed representation of this type
	 */
	friend uint32 GetTypeHash(FMovieSceneEvaluationFieldSegmentPtr LHS)
	{
		return HashCombine(GetTypeHash(LHS.SegmentID), GetTypeHash(static_cast<FMovieSceneEvaluationFieldTrackPtr&>(LHS)));
	}

	/** The identifier of the segment within the track (see FMovieSceneEvaluationTrack::Segments) */
	UPROPERTY()
	FMovieSceneSegmentIdentifier SegmentID;
};

/** Lookup table index for a group of evaluation templates */
USTRUCT()
struct FMovieSceneEvaluationGroupLUTIndex
{
	GENERATED_BODY()

	FMovieSceneEvaluationGroupLUTIndex()
		: LUTOffset(0)
		, NumInitPtrs(0)
		, NumEvalPtrs(0)
	{}

	/** The offset within FMovieSceneEvaluationGroup::SegmentPtrLUT that this index starts */
	UPROPERTY()
	int32 LUTOffset;

	/** The number of initialization pointers are stored after &FMovieSceneEvaluationGroup::SegmentPtrLUT[0] + LUTOffset. */
	UPROPERTY()
	int32 NumInitPtrs;

	/** The number of evaluation pointers are stored after &FMovieSceneEvaluationGroup::SegmentPtrLUT[0] + LUTOffset + NumInitPtrs. */
	UPROPERTY()
	int32 NumEvalPtrs;
};

/** Holds segment pointers for all segments that are active for a given range of the sequence */
USTRUCT()
struct FMovieSceneEvaluationGroup
{
	GENERATED_BODY()

	/** Array of indices that define all the flush groups in the range. */
	UPROPERTY()
	TArray<FMovieSceneEvaluationGroupLUTIndex> LUTIndices;

	/** A grouping of evaluation pointers that occur in this range of the sequence */
	UPROPERTY()
	TArray<FMovieSceneEvaluationFieldSegmentPtr> SegmentPtrLUT;
};

/** Struct that stores the key for an evaluated entity, and the index at which it was (or is to be) evaluated */
USTRUCT()
struct FMovieSceneOrderedEvaluationKey
{
	GENERATED_BODY()

	UPROPERTY()
	FMovieSceneEvaluationKey Key;

	UPROPERTY()
	uint32 EvaluationIndex;
};

/** Informational meta-data that applies to a given time range */
USTRUCT()
struct FMovieSceneEvaluationMetaData
{
	GENERATED_BODY()

	/**
	 * Reset this meta-data
	 */
	void Reset()
	{
		ActiveSequences.Reset();
		ActiveEntities.Reset();
	}

	/**
	 * Diff the active sequences this frame, with the specified previous frame's meta-data
	 *
	 * @param LastFrame				Meta-data pertaining to the last frame
	 * @param NewSequences			(Optional) Ptr to an array that will be populated with sequences that are new this frame
	 * @param ExpiredSequences		(Optional) Ptr to an array that will be populated with sequences that are no longer being evaluated
	 */
	void DiffSequences(const FMovieSceneEvaluationMetaData& LastFrame, TArray<FMovieSceneSequenceID>* NewSequences, TArray<FMovieSceneSequenceID>* ExpiredSequences) const;

	/**
	 * Diff the active entities (tracks and sections) this frame, with the specified previous frame's meta-data
	 *
	 * @param LastFrame				Meta-data pertaining to the last frame
	 * @param NewKeys				(Optional) Ptr to an array that will be populated with entities that are new this frame
	 * @param ExpiredKeys			(Optional) Ptr to an array that will be populated with entities that are no longer being evaluated
	 */
	void DiffEntities(const FMovieSceneEvaluationMetaData& LastFrame, TArray<FMovieSceneOrderedEvaluationKey>* NewKeys, TArray<FMovieSceneOrderedEvaluationKey>* ExpiredKeys) const;

	/**
	 * Check whether this meta-data entry is still up-to-date
	 *
	 * @param RootHierarchy				The hierarchy that corresponds to this meta-data's root sequence
	 * @param TemplateStore				The template store used to retrieve templates for sub sequences
	 * @param OutSubRangeToInvalidate	(Optional) A range to fill with a range to invalidate in the sequence's evaluation field (in root space)
	 * @param OutDirtySequences			(Optional) A set to populate with dirty sequences
	 *
	 * @return true if the meta-data needs re-generating, false otherwise
	 */
	bool IsDirty(const FMovieSceneSequenceHierarchy& RootHierarchy, IMovieSceneSequenceTemplateStore& TemplateStore, TRange<FFrameNumber>* OutSubRangeToInvalidate = nullptr, TSet<UMovieSceneSequence*>* OutDirtySequences = nullptr) const;

	/** Array of sequences that are active in this time range. */
	UPROPERTY()
	TArray<FMovieSceneSequenceID> ActiveSequences;

	/** Array of entities (tracks and/or sections) that are active in this time range. */
	UPROPERTY()
	TArray<FMovieSceneOrderedEvaluationKey> ActiveEntities;

	/** Map of sub sequence IDs to FMovieSceneEvaluationTemplate::TemplateSerialNumber that this meta data was generated with (not including root). */
	UPROPERTY()
	TMap<FMovieSceneSequenceID, uint32> SubTemplateSerialNumbers;
};

/**
 * Memory layout optimized primarily for speed of searching the applicable ranges
 */
USTRUCT()
struct FMovieSceneEvaluationField
{
	GENERATED_BODY()

	/**
	 * Ensure that the evaluation field is up-to-date for the range encompassing at least the range specified, and return its entries
	 *
	 * @param OverlapRange	A range specifying the smallest set of times to ensure are compiled
	 * @param InSequence	The sequence that this evaluation field relates to
	 * @param TemplateStore	A template store that defines how to retrieve templates for the specified sequence
	 * @return A range of indices for which GetRange() overlaps the specified OverlapRange, of the form [First, First+Num)
	 */
	MOVIESCENE_API TRange<int32> ConditionallyCompileRange(const TRange<FFrameNumber>& OverlapRange, UMovieSceneSequence* InSequence, IMovieSceneSequenceTemplateStore& TemplateStore);

	/**
	 * Efficiently find the entry that exists at the specified time, if any
	 *
	 * @param Time			The time at which to seach
	 * @return The index within Ranges, Groups and MetaData that the current time resides, or INDEX_NONE if there is nothing to do at the requested time
	 */
	MOVIESCENE_API int32 GetSegmentFromTime(FFrameNumber Time) const;

	/**
	 * Deduce the indices into Ranges and Groups that overlap with the specified time range
	 *
	 * @param Range			The range to overlap with our field
	 * @return A range of indices for which GetRange() overlaps the specified Range, of the form [First, First+Num)
	 */
	MOVIESCENE_API TRange<int32> OverlapRange(const TRange<FFrameNumber>& Range) const;

	/**
	 * Invalidate a range in this field
	 *
	 * @param Range			The range to overlap with our field
	 * @return A range of indices into Ranges and Groups that overlap with the requested range
	 */
	MOVIESCENE_API void Invalidate(const TRange<FFrameNumber>& Range);

	/**
	 * Insert a new range into this field
	 *
	 * @param InRange		The total range to insert to the field. Will potentially be intersected with preexisting adjacent ranges
	 * @param InGroup		The group defining what should happen at this time
	 * @param InMetaData	The meta-data defining efficient access to what happens in this frame
	 * @return The index the entries were inserted at
	 */
	MOVIESCENE_API int32 Insert(const TRange<FFrameNumber>& InRange, FMovieSceneEvaluationGroup&& InGroup, FMovieSceneEvaluationMetaData&& InMetaData);

	/**
	 * Add the specified data to this field, assuming the specified range lies after any other entries
	 *
	 * @param InRange		The range to add
	 * @param InGroup		The group defining what should happen at this time
	 * @param InMetaData	The meta-data defining efficient access to what happens in this frame
	 */
	MOVIESCENE_API void Add(const TRange<FFrameNumber>& InRange, FMovieSceneEvaluationGroup&& InGroup, FMovieSceneEvaluationMetaData&& InMetaData);

	/**
	 * Access this field's signature
	 */
#if WITH_EDITORONLY_DATA
	const FGuid& GetSignature() const
	{
		return Signature;
	}
#endif

	/**
	 * Access this field's size
	 */
	int32 Size() const
	{
		return Ranges.Num();
	}

	/**
	 * Access this entire field's set of ranges
	 */
	TArrayView<const FMovieSceneFrameRange> GetRanges() const
	{
		return Ranges;
	}

	/**
	 * Lookup a valid range by index
	 * @param Index 	The valid index within the ranges to lookup
	 * @return The range
	 */
	const TRange<FFrameNumber>& GetRange(int32 Index) const
	{
		return Ranges[Index].Value;
	}

	/**
	 * Lookup a valid evaluation group by entry index
	 * @param Index 	The valid index within the evaluation group array to lookup
	 * @return The group
	 */
	const FMovieSceneEvaluationGroup& GetGroup(int32 Index) const
	{
		return Groups[Index];
	}

	/**
	 * Lookup valid meta-data by entry index
	 * @param Index 	The valid index within the meta-data array to lookup
	 * @return The meta-data
	 */
	const FMovieSceneEvaluationMetaData& GetMetaData(int32 Index) const
	{
		return MetaData[Index];
	}

private:
#if WITH_EDITORONLY_DATA
	/** Signature that uniquely identifies any state this field can be in - regenerated on mutation */
	UPROPERTY()
	FGuid Signature;
#endif

	/** Ranges stored separately for fast (cache efficient) lookup. Each index has a corresponding entry in FMovieSceneEvaluationField::Groups. */
	UPROPERTY()
	TArray<FMovieSceneFrameRange> Ranges;

	/** Groups that store segment pointers for each of the above ranges. Each index has a corresponding entry in FMovieSceneEvaluationField::Ranges. */
	UPROPERTY()
	TArray<FMovieSceneEvaluationGroup> Groups;

	/** Meta data that maps to entries in the 'Ranges' array. */
	UPROPERTY()
	TArray<FMovieSceneEvaluationMetaData> MetaData;
};
