// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Compilation/MovieSceneCompiler.h"

#include "IMovieSceneModule.h"
#include "Evaluation/MovieSceneEvaluationTemplate.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Compilation/MovieSceneEvaluationTemplateGenerator.h"

#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "Sections/MovieSceneSubSection.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneTimeHelpers.h"

DECLARE_CYCLE_STAT(TEXT("Full Compile"),  MovieSceneEval_CompileFull,  STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Compile Range"), MovieSceneEval_CompileRange, STATGROUP_MovieSceneEval);

/** Parameter structure used for gathering entities for a given time or range */
struct FGatherParameters
{
	FGatherParameters(FMovieSceneRootOverridePath& InRootPath, FMovieSceneSequenceHierarchy& InRootHierarchy, IMovieSceneSequenceTemplateStore& InTemplateStore, TRange<FFrameNumber> InCompileRange)
		: RootPath(InRootPath)
		, RootHierarchy(InRootHierarchy)
		, TemplateStore(InTemplateStore)
		, RootCompileRange(InCompileRange)
		, RootClampRange(TRange<FFrameNumber>::All())
		, LocalCompileRange(RootCompileRange)
		, LocalClampRange(RootClampRange)
		, Flags(ESectionEvaluationFlags::None)
		, HierarchicalBias(0)
	{}

	FGatherParameters CreateForSubData(const FMovieSceneSubSequenceData& SubData) const
	{
		FGatherParameters SubParams = *this;

		SubParams.RootToSequenceTransform   = SubData.RootToSequenceTransform;
		SubParams.HierarchicalBias          = SubData.HierarchicalBias;

		SubParams.LocalCompileRange         = SubParams.RootCompileRange * SubData.RootToSequenceTransform;
		SubParams.LocalClampRange           = SubParams.RootClampRange   * SubData.RootToSequenceTransform;

		return SubParams;
	}

	void SetClampRange(TRange<FFrameNumber> InNewRootClampRange)
	{
		RootClampRange  = InNewRootClampRange;
		LocalClampRange = InNewRootClampRange * RootToSequenceTransform;
	}

	/** Clamp the specified range to the current clamp range (in root space) */
	TRange<FFrameNumber> ClampRoot(const TRange<FFrameNumber>& InRootRange) const
	{
		return TRange<FFrameNumber>::Intersection(RootClampRange, InRootRange);
	}

	/** Path from root to current sequence */
	FMovieSceneRootOverridePath& RootPath;
	/** Hierarchy for the root sequence template */
	FMovieSceneSequenceHierarchy& RootHierarchy;
	/** Store from which to retrieve templates */
	IMovieSceneSequenceTemplateStore& TemplateStore;

	/** The range that is being compiled in the root's time-space */
	TRange<FFrameNumber> RootCompileRange;
	/** A range to clamp compilation to in the root's time-space */
	TRange<FFrameNumber> RootClampRange;

	/** The range that is being compiled in the current sequence's time-space */
	TRange<FFrameNumber> LocalCompileRange;
	/** A range to clamp compilation to in the current sequence's time-space */
	TRange<FFrameNumber> LocalClampRange;

	/** Evaluation flags for the current sequence */
	ESectionEvaluationFlags Flags;

	/** Transform from the root time-space to the current sequence's time-space */
	FMovieSceneSequenceTransform RootToSequenceTransform;

	/** Current accumulated hierarchical bias */
	int32 HierarchicalBias;
};

struct FCompileOnTheFlyData
{
	/** Primary sort - group */
	uint16 GroupEvaluationPriority;
	/** Secondary sort - Hierarchical bias */
	int32 HierarchicalBias;
	/** Tertiary sort - Eval priority */
	int32 EvaluationPriority;

	/** Whether the track requires initialization or not */
	bool bRequiresInit;
	/** Cached ptr to the evaluation track */
	const FMovieSceneEvaluationTrack* Track;
	/** Cached segment ptr within the above track */
	FMovieSceneEvaluationFieldSegmentPtr Segment;
};

/** Gathered data for a given time or range */
struct FMovieSceneGatheredCompilerData
{
	/** Intersection of any empty space that overlaps the currently evaluating time range */
	FMovieSceneEvaluationTree EmptySpace;
	/** Tree of tracks to evaluate */
	TMovieSceneEvaluationTree<FCompileOnTheFlyData> Tracks;
	/** Tree of active sequences */
	TMovieSceneEvaluationTree<FMovieSceneSequenceID> Sequences;
};

/**
 * Populate the specified tree with all the ranges from the specified array that fully encompass the specified range
 * This is specifically used when compiling a specific range of an evaluation field in FMovieSceneCompiler::CompileRange()
 * The desire is to have the first range-entry that exists before TestRange, the last entry-range that exists
 * after TestRange, and all those in between. With this information we can quickly iterate the relevant gaps in
 * the field along with the compiled data.
 */
void PopulateIterableTreeWithEncompassingRanges(const TRange<FFrameNumber>& TestRange, TArrayView<const FMovieSceneFrameRange> Ranges, TMovieSceneEvaluationTree<int32>& OutFieldTree)
{
	// Add the first range that's before the input range
	int32 FirstIndex = Algo::LowerBoundBy(Ranges, TestRange.GetLowerBound(), &FMovieSceneFrameRange::GetLowerBound, MovieSceneHelpers::SortLowerBounds);
	if (FirstIndex - 1 >= 0)
	{
		--FirstIndex;
	}

	TRangeBound<FFrameNumber> StopAfterBound = TRangeBound<FFrameNumber>::FlipInclusion(TestRange.GetUpperBound());

	// Add all ranges that overlap the input range, and the first subsequent range
	for (int32 Index = FirstIndex; Index < Ranges.Num(); ++Index)
	{
		OutFieldTree.Add(Ranges[Index].Value, Index);

		// If this range's lower bound is >= the end of TestRange, we have enough information now to perform the compile
		TRangeBound<FFrameNumber> ThisLowerBound = Ranges[Index].Value.GetLowerBound();
		if (StopAfterBound.IsClosed() && ThisLowerBound.IsClosed() && TRangeBound<FFrameNumber>::MaxLower(ThisLowerBound, StopAfterBound) == ThisLowerBound)
		{
			break;
		}
	}
}

IMovieSceneModule& GetMovieSceneModule()
{
	static TWeakPtr<IMovieSceneModule> WeakMovieSceneModule;

	TSharedPtr<IMovieSceneModule> Shared = WeakMovieSceneModule.Pin();
	if (!Shared.IsValid())
	{
		WeakMovieSceneModule = IMovieSceneModule::Get().GetWeakPtr();
		Shared = WeakMovieSceneModule.Pin();
	}
	check(Shared.IsValid());

	return *Shared;
}

void FMovieSceneCompiler::Compile(UMovieSceneSequence& InCompileSequence, IMovieSceneSequenceTemplateStore& InTemplateStore)
{
	SCOPE_CYCLE_COUNTER(MovieSceneEval_CompileFull)

	FMovieSceneEvaluationTemplate& CompileTemplate = InTemplateStore.AccessTemplate(InCompileSequence);

	// Pass down a mutable path to the gather functions
	FMovieSceneRootOverridePath RootPath;

	// Gather everything that happens, recursively
	FMovieSceneGatheredCompilerData GatherData;
	FGatherParameters GatherParams(RootPath, CompileTemplate.Hierarchy, InTemplateStore, TRange<FFrameNumber>::All());
	GatherCompileOnTheFlyData(InCompileSequence, GatherParams, GatherData);

	// Wipe the current evaluation field for the template
	CompileTemplate.EvaluationField = FMovieSceneEvaluationField();

	TArray<FCompileOnTheFlyData> CompileData;

	for (FMovieSceneEvaluationTreeRangeIterator It(GatherData.Tracks); It; ++It)
	{
		CompileData.Reset();

		for (const FCompileOnTheFlyData& TrackData : GatherData.Tracks.GetAllData(It.Node()))
		{
			CompileData.Add(TrackData);
		}

		// Sort the compilation data based on (in order):
		//  1. Group
		//  2. Hierarchical bias
		//  3. Evaluation priority
		CompileData.Sort(SortPredicate);

		// Compose the final result for the compiled range
		FCompiledGroupResult Result(It.Range());

		// Generate the evaluation group by gathering initialization and evaluation ptrs for each unique group
		PopulateEvaluationGroup(Result, CompileData);

		// Compute meta data for this segment
		TMovieSceneEvaluationTreeDataIterator<FMovieSceneSequenceID> SubSequences = GatherData.Sequences.GetAllData(GatherData.Sequences.IterateFromLowerBound(It.Range().GetLowerBound()).Node());
		PopulateMetaData(Result, CompileTemplate.Hierarchy, InTemplateStore, CompileData, SubSequences);

		CompileTemplate.EvaluationField.Add(Result.Range, MoveTemp(Result.Group), MoveTemp(Result.MetaData));
	}
}

void FMovieSceneCompiler::CompileRange(TRange<FFrameNumber> InGlobalRange, UMovieSceneSequence& InCompileSequence, IMovieSceneSequenceTemplateStore& InTemplateStore)
{
	SCOPE_CYCLE_COUNTER(MovieSceneEval_CompileRange)

	FMovieSceneEvaluationTemplate& CompileTemplate = InTemplateStore.AccessTemplate(InCompileSequence);

	// Pass down a mutable path to the gather functions
	FMovieSceneRootOverridePath RootPath;

	// Gather everything that happens over this range, recursively throughout the entire sequence
	FMovieSceneGatheredCompilerData GatherData;
	FGatherParameters GatherParams(RootPath, CompileTemplate.Hierarchy, InTemplateStore, InGlobalRange);
	GatherCompileOnTheFlyData(InCompileSequence, GatherParams, GatherData);

	// --------------------------------------------------------------------------------------------------------------------
	// When compiling a range we want to compile *at least* the range specified by InGlobalRange
	// We may compile outside of this range if a gap in the evaluation field overlaps either bound, and the actual
	// unique sequence state defines sections outside of the range.
	// The general idea here is to iterate over any empty gaps in the evaluation field, populating it with the compiled
	// result for each lower bound. Note that there will be one or more new field entries added for each gap, depending on
	// whether any tracks or sections begin or end during the range of the gap.
	// --------------------------------------------------------------------------------------------------------------------

	// Populate an iterable tree with the ranges that at least encompass the range we want to comile,
	// plus one either side of InGlobalRange if they exist. This allows us to fully understand which gaps we want to fill in
	TMovieSceneEvaluationTree<int32> EvaluationFieldAsTree;
	PopulateIterableTreeWithEncompassingRanges(InGlobalRange, CompileTemplate.EvaluationField.GetRanges(), EvaluationFieldAsTree);

	// Start adding new field entries from the lower bound of the desired global range.
	// IterFromBound should be <= InGlobalRange.GetLowerBound at this point
	TRangeBound<FFrameNumber> IterFromBound = InGlobalRange.GetLowerBound();
	FMovieSceneEvaluationTreeRangeIterator ExistingEvaluationFieldIter = EvaluationFieldAsTree.IterateFromLowerBound(IterFromBound);

	// Now keep iterating the empty spaces in the field until we have nothing left to do.
	// We only increment ExistingEvaluationFieldIter when it is at an already populated range,
	// or if we've just compiled a range that has the same upper bound as the current gap (empty space)
	TArray<FCompileOnTheFlyData> SortedCompileData;
	for ( ; ExistingEvaluationFieldIter && !IterFromBound.IsOpen(); )
	{
		// If EvaluationFieldAsTree has any data at the current iterator position for it,
		// the evaluation field is already populated for that node
		if (EvaluationFieldAsTree.GetAllData(ExistingEvaluationFieldIter.Node()))
		{
			IterFromBound = TRangeBound<FFrameNumber>::FlipInclusion(ExistingEvaluationFieldIter.Range().GetUpperBound());
			++ExistingEvaluationFieldIter;
			continue;
		}

		TRange<FFrameNumber> EmptySpaceRange = ExistingEvaluationFieldIter.Range();

		// Find the intersection of all the current ranges (the gap in the evaluation field, the track field, sub sequence field, and empty space)
		FMovieSceneEvaluationTreeRangeIterator TrackIteratorFromHere       = GatherData.Tracks.IterateFromLowerBound(IterFromBound);
		FMovieSceneEvaluationTreeRangeIterator SubSequenceIteratorFromHere = GatherData.Sequences.IterateFromLowerBound(IterFromBound);
		FMovieSceneEvaluationTreeRangeIterator EmptySpaceIteratorFromHere  = GatherData.EmptySpace.IterateFromLowerBound(IterFromBound);

		// Find the intersection of all the compiled data
		TRange<FFrameNumber> CompiledRange = TRange<FFrameNumber>::Intersection(
			EmptySpaceRange,
			TRange<FFrameNumber>::Intersection(
				TrackIteratorFromHere.Range(),
				TRange<FFrameNumber>::Intersection(
					EmptySpaceIteratorFromHere.Range(),
					SubSequenceIteratorFromHere.Range()
				)
			)
		);

		// If the range we just compiled no longer overlaps the range we were asked to compile,
		// Break out of the loop as all of our work is done. This will happen if there is a gap
		// in the evaluation field that overlaps with the upper bound of InGlobalRange.
		if (!CompiledRange.Overlaps(InGlobalRange))
		{
			break;
		}

		SortedCompileData.Reset();
		for (const FCompileOnTheFlyData& TrackData : GatherData.Tracks.GetAllData(TrackIteratorFromHere.Node()))
		{
			SortedCompileData.Add(TrackData);
		}

		// Sort the compilation data based on (in order):
		//  1. Group
		//  2. Hierarchical bias
		//  3. Evaluation priority
		SortedCompileData.Sort(SortPredicate);

		// Compose the final result for the compiled range
		FCompiledGroupResult Result(CompiledRange);

		// Generate the evaluation group by gathering initialization and evaluation ptrs for each unique group
		PopulateEvaluationGroup(Result, SortedCompileData);

		// Compute meta data for this segment
		TMovieSceneEvaluationTreeDataIterator<FMovieSceneSequenceID> SubSequences = GatherData.Sequences.GetAllData(SubSequenceIteratorFromHere.Node());
		PopulateMetaData(Result, CompileTemplate.Hierarchy, InTemplateStore, SortedCompileData, SubSequences);

		// Add the results to the evaluation field and continue iterating starting from the end of the compiled range
		CompileTemplate.EvaluationField.Insert(Result.Range, MoveTemp(Result.Group), MoveTemp(Result.MetaData));

		// We may still have some to compile
		IterFromBound = TRangeBound<FFrameNumber>::FlipInclusion(CompiledRange.GetUpperBound());

		// If the range that we just compiled goes right up to the end of the gap, increment onto the
		// next entry in the evaluation field iterator (which should be a populated range)
		if (CompiledRange.GetUpperBound() == EmptySpaceRange.GetUpperBound())
		{
			++ExistingEvaluationFieldIter;
		}
	}
}

void FMovieSceneCompiler::CompileHierarchy(const UMovieSceneSequence& InRootSequence, FMovieSceneSequenceHierarchy& OutHierarchy, FMovieSceneSequenceID RootSequenceID, int32 MaxDepth)
{
	FMovieSceneRootOverridePath Path;
	Path.Set(RootSequenceID, OutHierarchy);

	CompileHierarchy(InRootSequence, OutHierarchy, Path, MaxDepth);
}

void FMovieSceneCompiler::CompileHierarchy(const UMovieSceneSequence& InSequence, FMovieSceneSequenceHierarchy& OutHierarchy, FMovieSceneRootOverridePath& Path, int32 MaxDepth)
{
	UMovieScene* MovieScene = InSequence.GetMovieScene();
	if (!MovieScene)
	{
		return;
	}

	FMovieSceneSequenceID ParentID = Path.Remap(MovieSceneSequenceID::Root);

	// Remove all existing children
	if (FMovieSceneSequenceHierarchyNode* ExistingNode = OutHierarchy.FindNode(ParentID))
	{
		OutHierarchy.Remove(ExistingNode->Children);
	}

	auto ProcessSection = [&](const UMovieSceneSection* Section, const FGuid& InObjectBindingId)
	{
		const UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
		const UMovieSceneSequence* SubSequence = SubSection ? SubSection->GetSequence() : nullptr;

		if (SubSequence)
		{
			FMovieSceneSequenceID DeterministicID = SubSection->GetSequenceID();

			GetOrCreateSubSequenceData(Path.Remap(DeterministicID), ParentID, *SubSection, InObjectBindingId, OutHierarchy);

			int32 NewMaxDepth = MaxDepth == -1 ? -1 : MaxDepth - 1;
			if (NewMaxDepth == -1 || NewMaxDepth > 1)
			{
				Path.Push(DeterministicID);

				CompileHierarchy(*SubSequence, OutHierarchy, Path, NewMaxDepth);

				Path.Pop();
			}
		}
	};

	for (const UMovieSceneTrack* Track : MovieScene->GetMasterTracks())
	{
		for (const UMovieSceneSection* Section : Track->GetAllSections())
		{
			ProcessSection(Section, FGuid());
		}
	}

	for (const FMovieSceneBinding& ObjectBinding : MovieScene->GetBindings())
	{
		for (const UMovieSceneTrack* Track : ObjectBinding.GetTracks())
		{
			for (const UMovieSceneSection* Section : Track->GetAllSections())
			{
				ProcessSection(Section, ObjectBinding.GetObjectGuid());
			}
		}
	}
}

void FMovieSceneCompiler::GatherCompileOnTheFlyData(UMovieSceneSequence& InSequence, const FGatherParameters& Params, FMovieSceneGatheredCompilerData& OutData)
{
	// Regenerate the track structure if it's out of date
	FMovieSceneEvaluationTemplate& Template = Params.TemplateStore.AccessTemplate(InSequence);
	if (Template.SequenceSignature != InSequence.GetSignature())
	{
		FMovieSceneEvaluationTemplateGenerator(InSequence, Template).Generate();
	}

	// Iterate tracks within this template
	for (TTuple<FMovieSceneTrackIdentifier, FMovieSceneEvaluationTrack>& Pair : Template.GetTracks())
	{
		const bool bTrackMatchesFlags = ( Params.Flags == ESectionEvaluationFlags::None )
			|| ( EnumHasAnyFlags(Params.Flags, ESectionEvaluationFlags::PreRoll)  && Pair.Value.ShouldEvaluateInPreroll()  )
			|| ( EnumHasAnyFlags(Params.Flags, ESectionEvaluationFlags::PostRoll) && Pair.Value.ShouldEvaluateInPostroll() );

		if (bTrackMatchesFlags)
		{
			GatherCompileDataForTrack(Pair.Value, Pair.Key, Params, OutData);
		}
	}

	TRange<FFrameNumber> CompileClampIntersection = TRange<FFrameNumber>::Intersection(Params.LocalCompileRange, Params.LocalClampRange);

	// Iterate sub section ranges that overlap with the compile range
	FGatherParameters SubSectionGatherParams = Params;

	const TMovieSceneEvaluationTree<FMovieSceneSubSectionData>& SubSectionField = Template.GetSubSectionField();

	// Start iterating the field from the lower bound of the compile range
	FMovieSceneEvaluationTreeRangeIterator SubSectionIt(SubSectionField.IterateFromLowerBound(CompileClampIntersection.GetLowerBound()));

	for ( ; SubSectionIt && SubSectionIt.Range().Overlaps(CompileClampIntersection); ++SubSectionIt)
	{
		TRange<FFrameNumber> ThisSegmentRangeRoot = Params.ClampRoot(SubSectionIt.Range() * Params.RootToSequenceTransform.Inverse());
		if (ThisSegmentRangeRoot.IsEmpty())
		{
			continue;
		}

		SubSectionGatherParams.SetClampRange(ThisSegmentRangeRoot);

		bool bAnySubSections = false;

		// Iterate all sub sections in the current range
		for (const FMovieSceneSubSectionData& SubSectionData : SubSectionField.GetAllData(SubSectionIt.Node()))
		{
			UMovieSceneSubSection* SubSection = SubSectionData.Section.Get();
			if (!SubSection)
			{
				continue;
			}

			UMovieSceneSubTrack* SubTrack = SubSection->GetTypedOuter<UMovieSceneSubTrack>();

			const bool bTrackMatchesFlags = ( Params.Flags == ESectionEvaluationFlags::None )
				|| ( EnumHasAnyFlags(Params.Flags, ESectionEvaluationFlags::PreRoll)  && SubTrack && SubTrack->EvalOptions.bEvaluateInPreroll  )
				|| ( EnumHasAnyFlags(Params.Flags, ESectionEvaluationFlags::PostRoll) && SubTrack && SubTrack->EvalOptions.bEvaluateInPostroll );

			if (bTrackMatchesFlags && SubSection)
			{
				bAnySubSections = true;

				SubSectionGatherParams.Flags = SubSectionData.Flags;

				GatherCompileDataForSubSection(*SubSection, SubSectionData.ObjectBindingId, SubSectionGatherParams, OutData);
			}
		}

		if (!bAnySubSections)
		{
			// Intersect the unique range in the tree with the current overlapping empty range to constrict the resulting compile range in the case where this is a gap between sub sections
			OutData.EmptySpace.AddTimeRange(Params.ClampRoot(SubSectionIt.Range() * Params.RootToSequenceTransform.Inverse()));
		}
	}
}

void FMovieSceneCompiler::GatherCompileDataForSubSection(const UMovieSceneSubSection& SubSection, const FGuid& InObjectBindingId, const FGatherParameters& Params, FMovieSceneGatheredCompilerData& OutData)
{
	UMovieSceneSequence* SubSequence = SubSection.GetSequence();
	if (!SubSequence)
	{
		return;
	}

	FMovieSceneSequenceID UnAccumulatedSequenceID = SubSection.GetSequenceID();

	// Hash this source ID with the outer sequence ID to make it unique
	FMovieSceneSequenceID ParentSequenceID = Params.RootPath.Remap(MovieSceneSequenceID::Root);
	FMovieSceneSequenceID InnerSequenceID  = Params.RootPath.Remap(UnAccumulatedSequenceID);

	// Add the active sequence ID to each range. We add each range individually since this range may inform the final compiled range
	OutData.Sequences.Add(Params.RootClampRange, InnerSequenceID);

	// Add this sub sequence ID to the root path
	Params.RootPath.Push(UnAccumulatedSequenceID);

	// Find/add sub data in the root template
	TOptional<FGatherParameters> SubParams;

	{
		const FMovieSceneSubSequenceData* CompilationSubData = GetOrCreateSubSequenceData(InnerSequenceID, ParentSequenceID, SubSection, InObjectBindingId, Params.RootHierarchy);
		check(CompilationSubData);

		SubParams = Params.CreateForSubData(*CompilationSubData);
		// Any code after this point may reallocate the root hierarchy, so CompilationSubData cannot be used
	}

	GatherCompileOnTheFlyData(*SubSequence, SubParams.GetValue(), OutData);

	// Pop the path off the root path
	Params.RootPath.Pop();
}

const FMovieSceneSubSequenceData* FMovieSceneCompiler::GetOrCreateSubSequenceData(FMovieSceneSequenceID InnerSequenceID, FMovieSceneSequenceID ParentSequenceID, const UMovieSceneSubSection& SubSection, const FGuid& InObjectBindingId, FMovieSceneSequenceHierarchy& InOutHierarchy)
{
	// Find/add sub data in the root template
	const FMovieSceneSubSequenceData* SubData = InOutHierarchy.FindSubData(InnerSequenceID);
	if (SubData && !SubData->IsDirty(SubSection))
	{
		return SubData;
	}

	// Ensure that any ((great)grand)child sub sequences have their sub data regenerated
	// by removing this whole sequence branch from the hierarchy (if it exists). This is
	// necessary as all children will depend on this sequences's transform
	InOutHierarchy.Remove(MakeArrayView(&InnerSequenceID, 1));

	FSubSequenceInstanceDataParams InstanceParams{ InnerSequenceID, FMovieSceneEvaluationOperand(ParentSequenceID, InObjectBindingId) };
	FMovieSceneSubSequenceData NewSubData = SubSection.GenerateSubSequenceData(InstanceParams);

	// Intersect this inner sequence's valid play range with the parent's if possible
	const FMovieSceneSubSequenceData* ParentSubData = ParentSequenceID != MovieSceneSequenceID::Root ? InOutHierarchy.FindSubData(ParentSequenceID) : nullptr;
	if (ParentSubData)
	{
		TRange<FFrameNumber> ParentPlayRangeChildSpace = ParentSubData->PlayRange.Value * NewSubData.RootToSequenceTransform;
		NewSubData.PlayRange = TRange<FFrameNumber>::Intersection(ParentPlayRangeChildSpace, NewSubData.PlayRange.Value);

		// Accumulate parent transform
		NewSubData.RootToSequenceTransform = NewSubData.RootToSequenceTransform * ParentSubData->RootToSequenceTransform;

		// Accumulate parent hierarchical bias
		NewSubData.HierarchicalBias += ParentSubData->HierarchicalBias;
	}

	// Add the sub data to the root hierarchy
	InOutHierarchy.Add(NewSubData, InnerSequenceID, ParentSequenceID);

	return InOutHierarchy.FindSubData(InnerSequenceID);
}

void FMovieSceneCompiler::GatherCompileDataForTrack(FMovieSceneEvaluationTrack& Track, FMovieSceneTrackIdentifier TrackID, const FGatherParameters& Params, FMovieSceneGatheredCompilerData& OutData)
{
	auto RequiresInit = [&Track](FSectionEvaluationData EvalData)
	{
		return Track.HasChildTemplate(EvalData.ImplIndex) && Track.GetChildTemplate(EvalData.ImplIndex).RequiresInitialization();
	};

	FMovieSceneSequenceTransform SequenceToRootTransform  = Params.RootToSequenceTransform.Inverse();
	FMovieSceneSequenceID        CurrentSequenceID        = Params.RootPath.Remap(MovieSceneSequenceID::Root);
	TRange<FFrameNumber>         CompileClampIntersection = TRange<FFrameNumber>::Intersection(Params.LocalCompileRange, Params.LocalClampRange);

	FMovieSceneEvaluationTreeRangeIterator TrackIter = Track.IterateFrom(CompileClampIntersection.GetLowerBound());
	for ( ; TrackIter && TrackIter.Range().Overlaps(CompileClampIntersection); ++TrackIter)
	{
		FMovieSceneSegmentIdentifier SegmentID = Track.GetSegmentFromIterator(TrackIter);
		if (!SegmentID.IsValid())
		{
			// No segment at this time, so just report the time range of the empty space.
			TRange<FFrameNumber> ClampedEmptyTrackSpaceRoot = Params.ClampRoot(TrackIter.Range() * SequenceToRootTransform);
			OutData.EmptySpace.AddTimeRange(ClampedEmptyTrackSpaceRoot);
		}
		else
		{
			const FMovieSceneSegment& ThisSegment = Track.GetSegment(SegmentID);

			FCompileOnTheFlyData Data;
			Data.Segment = FMovieSceneEvaluationFieldSegmentPtr(CurrentSequenceID, TrackID, SegmentID);
			Data.GroupEvaluationPriority = GetMovieSceneModule().GetEvaluationGroupParameters(Track.GetEvaluationGroup()).EvaluationPriority;
			Data.HierarchicalBias = Params.HierarchicalBias;
			Data.EvaluationPriority = Track.GetEvaluationPriority();
			Data.Track = &Track;
			Data.bRequiresInit = ThisSegment.Impls.ContainsByPredicate(RequiresInit);

			TRange<FFrameNumber> SegmentTrackIntersection = TRange<FFrameNumber>::Intersection(ThisSegment.Range, TrackIter.Range());
			TRange<FFrameNumber> IntersectionRange        = Params.ClampRoot(SegmentTrackIntersection * SequenceToRootTransform);
			if (!IntersectionRange.IsEmpty())
			{
				OutData.Tracks.Add(IntersectionRange, Data);
			}
		}
	}
}

void FMovieSceneCompiler::PopulateMetaData(FCompiledGroupResult& OutResult, const FMovieSceneSequenceHierarchy& RootHierarchy, IMovieSceneSequenceTemplateStore& TemplateStore, const TArray<FCompileOnTheFlyData>& SortedCompileData, TMovieSceneEvaluationTreeDataIterator<FMovieSceneSequenceID> SubSequences)
{
	OutResult.MetaData.Reset();

	// Add all the init tracks first
	uint32 SortOrder = 0;
	for (const FCompileOnTheFlyData& CompileData : SortedCompileData)
	{
		if (!CompileData.bRequiresInit)
		{
			continue;
		}

		FMovieSceneEvaluationFieldSegmentPtr SegmentPtr = CompileData.Segment;

		// Add the track key
		FMovieSceneEvaluationKey TrackKey(SegmentPtr.SequenceID, SegmentPtr.TrackIdentifier);
		OutResult.MetaData.ActiveEntities.Add(FMovieSceneOrderedEvaluationKey{ TrackKey, SortOrder++ });

		for (FSectionEvaluationData EvalData : CompileData.Track->GetSegment(SegmentPtr.SegmentID).Impls)
		{
			FMovieSceneEvaluationKey SectionKey = TrackKey.AsSection(EvalData.ImplIndex);
			OutResult.MetaData.ActiveEntities.Add(FMovieSceneOrderedEvaluationKey{ SectionKey, SortOrder++ });
		}
	}

	// Then all the eval tracks
	for (const FCompileOnTheFlyData& CompileData : SortedCompileData)
	{
		if (CompileData.bRequiresInit)
		{
			continue;
		}

		FMovieSceneEvaluationFieldSegmentPtr SegmentPtr = CompileData.Segment;

		// Add the track key
		FMovieSceneEvaluationKey TrackKey(SegmentPtr.SequenceID, SegmentPtr.TrackIdentifier);
		OutResult.MetaData.ActiveEntities.Add(FMovieSceneOrderedEvaluationKey{ TrackKey, SortOrder++ });

		for (FSectionEvaluationData EvalData : CompileData.Track->GetSegment(SegmentPtr.SegmentID).Impls)
		{
			FMovieSceneEvaluationKey SectionKey = TrackKey.AsSection(EvalData.ImplIndex);
			OutResult.MetaData.ActiveEntities.Add(FMovieSceneOrderedEvaluationKey{ SectionKey, SortOrder++ });
		}
	}

	Algo::SortBy(OutResult.MetaData.ActiveEntities, &FMovieSceneOrderedEvaluationKey::Key);

	{
		OutResult.MetaData.ActiveSequences.Reset();
		OutResult.MetaData.ActiveSequences.Add(MovieSceneSequenceID::Root);

		for (FMovieSceneSequenceID SequenceID : SubSequences)
		{
			const FMovieSceneSubSequenceData* SubData = RootHierarchy.FindSubData(SequenceID);
			check(SubData);

			UMovieSceneSequence* Sequence = SubData->GetSequence();

			uint32 TemplateSerialNumber = Sequence ? TemplateStore.AccessTemplate(*Sequence).TemplateSerialNumber.GetValue() : 0;

			OutResult.MetaData.ActiveSequences.Add(SequenceID);
			OutResult.MetaData.SubTemplateSerialNumbers.Add(SequenceID, TemplateSerialNumber);
		}

		OutResult.MetaData.ActiveSequences.Sort();
	}
}

bool FMovieSceneCompiler::SortPredicate(const FCompileOnTheFlyData& A, const FCompileOnTheFlyData& B)
{
	if (A.GroupEvaluationPriority != B.GroupEvaluationPriority)
	{
		return A.GroupEvaluationPriority > B.GroupEvaluationPriority;
	}
	else if (A.HierarchicalBias != B.HierarchicalBias)
	{
		return A.HierarchicalBias < B.HierarchicalBias;
	}
	else
	{
		return A.EvaluationPriority > B.EvaluationPriority;
	}
}

void FMovieSceneCompiler::AddPtrsToGroup(FMovieSceneEvaluationGroup& Group, TArray<FMovieSceneEvaluationFieldSegmentPtr>& InitPtrs, TArray<FMovieSceneEvaluationFieldSegmentPtr>& EvalPtrs)
{
	if (!InitPtrs.Num() && !EvalPtrs.Num())
	{
		return;
	}

	FMovieSceneEvaluationGroupLUTIndex Index;

	Index.LUTOffset = Group.SegmentPtrLUT.Num();
	Index.NumInitPtrs = InitPtrs.Num();
	Index.NumEvalPtrs = EvalPtrs.Num();

	Group.LUTIndices.Add(Index);
	Group.SegmentPtrLUT.Append(InitPtrs);
	Group.SegmentPtrLUT.Append(EvalPtrs);

	InitPtrs.Reset();
	EvalPtrs.Reset();
}

void FMovieSceneCompiler::PopulateEvaluationGroup(FCompiledGroupResult& OutResult, const TArray<FCompileOnTheFlyData>& SortedCompileData)
{
	TArray<FMovieSceneEvaluationFieldSegmentPtr> EvalPtrs;
	TArray<FMovieSceneEvaluationFieldSegmentPtr> InitPtrs;

	// Now iterate the tracks and insert indices for initialization and evaluation
	FName CurrentEvaluationGroup, LastEvaluationGroup;

	for (const FCompileOnTheFlyData& Data : SortedCompileData)
	{
		// If we're now in a different flush group, add the ptrs to the group
		{
			CurrentEvaluationGroup = Data.Track->GetEvaluationGroup();
			if (CurrentEvaluationGroup != LastEvaluationGroup)
			{
				AddPtrsToGroup(OutResult.Group, InitPtrs, EvalPtrs);
			}
			LastEvaluationGroup = CurrentEvaluationGroup;
		}

		// If this track requires initialization, add it to the init array
		if (Data.bRequiresInit)
		{
			InitPtrs.Add(Data.Segment);
		}

		// All tracks require evaluation implicitly
		EvalPtrs.Add(Data.Segment);
	}
	AddPtrsToGroup(OutResult.Group, InitPtrs, EvalPtrs);
}
