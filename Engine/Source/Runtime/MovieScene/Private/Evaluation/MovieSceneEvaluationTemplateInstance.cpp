// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Containers/SortedMap.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSequence.h"
#include "Compilation/MovieSceneCompiler.h"
#include "Sections/MovieSceneSubSection.h"
#include "Compilation/MovieSceneEvaluationTemplateGenerator.h"

#include "IMovieSceneModule.h"
#include "Algo/Sort.h"

DECLARE_CYCLE_STAT(TEXT("Entire Evaluation Cost"), MovieSceneEval_EntireEvaluationCost, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Gather Entries For Frame"), MovieSceneEval_GatherEntries, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Call Setup() and TearDown()"), MovieSceneEval_CallSetupTearDown, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Evaluate Group"), MovieSceneEval_EvaluateGroup, STATGROUP_MovieSceneEval);


/**
 * Structure of ptrs that are cached only for the duration of an evaluation frame for a particular sequence.
 * Any of this information may be re-allocated outside of an evaluation so cannot be persistently cached.
 * 24 bytes
 */
struct FMovieSceneEvaluationPtrs
{
	/** The cached sequence ptr - always non-nullptr */
	UMovieSceneSequence*              Sequence;
	/** The cached template ptr - always non-nullptr */
	FMovieSceneEvaluationTemplate*    Template;
	/** The cached sub data ptr from the hierarchy. Only valid for sub sequences. */
	const FMovieSceneSubSequenceData* SubData;
};

/**
 * A cache of ptrs that need to be efficiently referred to during evaluation,
 * but must not persist outside that evaluation.
 */
struct FMovieSceneEvaluationPtrCache
{
	typedef TSortedMap<FMovieSceneSequenceID, FMovieSceneEvaluationPtrs, TInlineAllocator<8>>::TConstIterator FConstIterator;

	/** Construct the cache from a root sequence, template store and a set of sequence IDs that are to be included this frame */
	FMovieSceneEvaluationPtrCache(const FMovieSceneRootOverridePath& RootOverridePath, UMovieSceneSequence* InRootSequence, IMovieSceneSequenceTemplateStore* InTemplateStore, TArrayView<FMovieSceneSequenceID const> InSubSequences);

	/**
	 * Attempt to locate the cached pointers for the specified sequence ID, failing gracefully if they were not found
	 */
	const FMovieSceneEvaluationPtrs* Find(FMovieSceneSequenceID SequenceID) const
	{
		return CachedPtrs.Find(SequenceID);
	}

	/**
	 * Locate the cached pointers for the specified sequence ID assuming they exist, asseting if not
	 */
	const FMovieSceneEvaluationPtrs& GetChecked(FMovieSceneSequenceID SequenceID) const
	{
		return CachedPtrs.FindChecked(SequenceID);
	}

	FConstIterator CreateConstIterator() const
	{
		return CachedPtrs.CreateConstIterator();
	}

private:

	/**
	 * Generally we are dealing with very small numbers of sub sequences (or, just a single master sequence)
	 * For this reason we allocate using a sorted map and an inline allocator to ensure that lookups are as
	 * fast as possible in the common case
	 */
	TSortedMap<FMovieSceneSequenceID, FMovieSceneEvaluationPtrs, TInlineAllocator<8>> CachedPtrs;
};


/** Scoped helper class that facilitates the delayed restoration of preanimated state for specific evaluation keys */
struct FDelayedPreAnimatedStateRestore
{
	FDelayedPreAnimatedStateRestore(IMovieScenePlayer& InPlayer)
		: Player(InPlayer)
	{}

	~FDelayedPreAnimatedStateRestore()
	{
		RestoreNow();
	}

	void Add(FMovieSceneEvaluationKey Key)
	{
		KeysToRestore.Add(Key);
	}

	void RestoreNow()
	{
		for (FMovieSceneEvaluationKey Key : KeysToRestore)
		{
			Player.PreAnimatedState.RestorePreAnimatedState(Player, Key);
		}
		KeysToRestore.Reset();
	}

private:
	/** The movie scene player to restore with */
	IMovieScenePlayer& Player;
	/** The array of keys to restore */
	TArray<FMovieSceneEvaluationKey> KeysToRestore;
};

FMovieSceneEvaluationPtrCache::FMovieSceneEvaluationPtrCache(const FMovieSceneRootOverridePath& RootOverridePath, UMovieSceneSequence* InRootSequence, IMovieSceneSequenceTemplateStore* InTemplateStore, TArrayView<FMovieSceneSequenceID const> InSubSequences)
{
	// No root sequence == empty container
	if (!InRootSequence)
	{
		return;
	}

	check(InTemplateStore);

	// Find the root template from the template store
	FMovieSceneEvaluationTemplate* RootTemplate = &InTemplateStore->AccessTemplate(*InRootSequence);

	// We always remap sequence IDs to their root space to ensure that spawnables presist properly when jumping into/out of shots with Eval in Isolation turned on
	FMovieSceneSequenceID RemappedSequenceID = RootOverridePath.Remap(MovieSceneSequenceID::Root);

	// Cache all the ptrs for the root sequence
	CachedPtrs.Add(RemappedSequenceID, FMovieSceneEvaluationPtrs{ InRootSequence, RootTemplate, nullptr });

	// Cache all sub-sequence ptrs
	const FMovieSceneSequenceHierarchy& RootHierarchy = RootTemplate->Hierarchy;
	for (FMovieSceneSequenceID SubSequenceID : InSubSequences)
	{
		if (SubSequenceID == MovieSceneSequenceID::Root)
		{
			continue;
		}

		const FMovieSceneSubSequenceData* SubData     = RootHierarchy.FindSubData(SubSequenceID);
		UMovieSceneSequence*              SubSequence = SubData ? SubData->GetSequence() : nullptr;

		// We gracefully handle nullptr here because in some rare cases a previous frame's meta-data may
		// be referencing stale data that no longer exists
		if (SubSequence)
		{
			RemappedSequenceID = RootOverridePath.Remap(SubSequenceID);
			CachedPtrs.Add(RemappedSequenceID, FMovieSceneEvaluationPtrs{ SubSequence, &InTemplateStore->AccessTemplate(*SubSequence), SubData });
		}
	}
}

FMovieSceneRootEvaluationTemplateInstance::FMovieSceneRootEvaluationTemplateInstance()
	: RootSequence(nullptr)
	, RootID(MovieSceneSequenceID::Root)
	, TemplateStore(MakeShared<FMovieSceneSequencePrecompiledTemplateStore>())
{
}

FMovieSceneRootEvaluationTemplateInstance::~FMovieSceneRootEvaluationTemplateInstance()
{
}

void FMovieSceneRootEvaluationTemplateInstance::Initialize(UMovieSceneSequence& InRootSequence, IMovieScenePlayer& Player, TSharedRef<IMovieSceneSequenceTemplateStore> InTemplateStore)
{
	if (RootSequence != &InRootSequence)
	{
		Finish(Player);
	}

	TemplateStore = MoveTemp(InTemplateStore);

	Initialize(InRootSequence, Player);
}

void FMovieSceneRootEvaluationTemplateInstance::Initialize(UMovieSceneSequence& InRootSequence, IMovieScenePlayer& Player)
{
	if (RootSequence.Get() != &InRootSequence)
	{
		Finish(Player);

		// Always ensure that there is no persistent data when initializing a new sequence
		// to ensure we don't collide with the previous sequence's entity keys
		Player.State.PersistentEntityData.Reset();
		Player.State.PersistentSharedData.Reset();

		LastFrameMetaData.Reset();
		ThisFrameMetaData.Reset();
		ExecutionTokens = FMovieSceneExecutionTokens();
	}

	RootSequence = &InRootSequence;
	RootTemplate = &TemplateStore->AccessTemplate(InRootSequence);

	RootID = MovieSceneSequenceID::Root;
}

void FMovieSceneRootEvaluationTemplateInstance::Finish(IMovieScenePlayer& Player)
{
	Swap(ThisFrameMetaData, LastFrameMetaData);
	ThisFrameMetaData.Reset();

	CallSetupTearDown(Player);

	ResetDirectorInstances();
}

void FMovieSceneRootEvaluationTemplateInstance::Evaluate(FMovieSceneContext Context, IMovieScenePlayer& Player, FMovieSceneSequenceID InOverrideRootID)
{
	SCOPE_CYCLE_COUNTER(MovieSceneEval_EntireEvaluationCost);

	Swap(ThisFrameMetaData, LastFrameMetaData);
	ThisFrameMetaData.Reset();

	if (RootID != InOverrideRootID)
	{
		// Tear everything down if we're evaluating a different root sequence
		CallSetupTearDown(Player);
		LastFrameMetaData.Reset();
	}

	UMovieSceneSequence* OverrideRootSequence = GetSequence(InOverrideRootID);
	if (!OverrideRootSequence)
	{
		CallSetupTearDown(Player);
		return;
	}

	const FMovieSceneEvaluationGroup* GroupToEvaluate = SetupFrame(OverrideRootSequence, InOverrideRootID, &Context);
	if (!GroupToEvaluate)
	{
		CallSetupTearDown(Player);
		return;
	}

	// Cache all the pointers needed for this frame
	FMovieSceneEvaluationPtrCache EvaluationPtrCache = ConstructEvaluationPtrCacheForFrame(OverrideRootSequence);

	// Ensure the correct sequences are assigned for each sequence ID
	for (auto Iter = EvaluationPtrCache.CreateConstIterator(); Iter; ++Iter)
	{
		Player.State.AssignSequence(Iter.Key(), *Iter.Value().Sequence, Player);
	}

	// Cause stale tracks to not restore until after evaluation. This fixes issues when tracks that are set to 'Restore State' are regenerated, causing the state to be restored then re-animated by the new track
	FDelayedPreAnimatedStateRestore DelayedRestore(Player);

	// Run the post root evaluate steps which invoke tear downs for anything no longer evaluated.
	// Do this now to ensure they don't undo any of the current frame's execution tokens 
	CallSetupTearDown(EvaluationPtrCache, Player, &DelayedRestore);

	// Ensure any null objects are not cached
	Player.State.InvalidateExpiredObjects();

	// Accumulate execution tokens into this structure
	EvaluateGroup(EvaluationPtrCache, *GroupToEvaluate, Context, Player);

	// Process execution tokens
	ExecutionTokens.Apply(Context, Player);
}

FMovieSceneEvaluationPtrCache FMovieSceneRootEvaluationTemplateInstance::ConstructEvaluationPtrCacheForFrame(UMovieSceneSequence* OverrideRootSequence)
{
	// We recreate all necessary sequence data for the current and previous frames by diffing the sequences active last frame, with this frame
	TArray<FMovieSceneSequenceID> PreviousAndCurrentFrameSequenceIDs = LastFrameMetaData.ActiveSequences;
	ThisFrameMetaData.DiffSequences(LastFrameMetaData, &PreviousAndCurrentFrameSequenceIDs, nullptr);

	return FMovieSceneEvaluationPtrCache(RootOverridePath, OverrideRootSequence, TemplateStore.Get(), PreviousAndCurrentFrameSequenceIDs);
}

const FMovieSceneEvaluationGroup* FMovieSceneRootEvaluationTemplateInstance::SetupFrame(UMovieSceneSequence* OverrideRootSequence, FMovieSceneSequenceID InOverrideRootID, FMovieSceneContext* InOutContext)
{
	check(OverrideRootSequence);

	RootID = InOverrideRootID;
	RootOverridePath.Set(InOverrideRootID, GetHierarchy());

	FMovieSceneEvaluationTemplate* OverrideRootTemplate = nullptr;

	if (InOverrideRootID == MovieSceneSequenceID::Root)
	{
		OverrideRootTemplate = RootTemplate;
	}
	else
	{
		// Evaluate Sub Sequences in Isolation is turned on
		OverrideRootTemplate = &TemplateStore->AccessTemplate(*OverrideRootSequence);
		if (const FMovieSceneSubSequenceData* OverrideSubData = GetHierarchy().FindSubData(InOverrideRootID))
		{
			*InOutContext = InOutContext->Transform(OverrideSubData->RootToSequenceTransform, OverrideSubData->TickResolution);
		}
	}

	if (!ensureMsgf(OverrideRootTemplate, TEXT("Could not find valid template for supplied sequence ID.")))
	{
		return nullptr;
	}

	// Ensure the root is up to date
	if (OverrideRootTemplate->SequenceSignature != OverrideRootSequence->GetSignature())
	{
		FMovieSceneEvaluationTemplateGenerator(*OverrideRootSequence, *OverrideRootTemplate).Generate();
	}

	FMovieSceneEvaluationField& EvaluationField = OverrideRootTemplate->EvaluationField;

	// Get the range that we are evaluating in the root's space
	TRange<FFrameNumber> ContextRange = InOutContext->GetTraversedFrameNumberRange();

	// Verify and update the evaluation field for this range, returning the bounds of the overlapping field entries
	TRange<int32> FieldRange = EvaluationField.ConditionallyCompileRange(ContextRange, OverrideRootSequence, *TemplateStore);
	if (FieldRange.IsEmpty())
	{
		return nullptr;
	}

	// The one that we want to evaluate is either the first or last index in the range.
	// FieldRange is always of the form [First, Last+1)
	int32 TemplateFieldIndex = InOutContext->GetDirection() == EPlayDirection::Forwards ? FieldRange.GetUpperBoundValue() - 1 : FieldRange.GetLowerBoundValue();
	if (TemplateFieldIndex != INDEX_NONE)
	{
		// Set meta-data
		ThisFrameMetaData = EvaluationField.GetMetaData(TemplateFieldIndex);
		return &EvaluationField.GetGroup(TemplateFieldIndex);
	}

	return nullptr;
}

void FMovieSceneRootEvaluationTemplateInstance::EvaluateGroup(const FMovieSceneEvaluationPtrCache& EvaluationPtrCache, const FMovieSceneEvaluationGroup& Group, const FMovieSceneContext& RootContext, IMovieScenePlayer& Player)
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_EvaluateGroup);

	FPersistentEvaluationData PersistentDataProxy(Player);

	FMovieSceneEvaluationOperand Operand;

	FMovieSceneContext Context = RootContext;
	FMovieSceneContext SubContext = Context;

	for (const FMovieSceneEvaluationGroupLUTIndex& Index : Group.LUTIndices)
	{
		int32 TrackIndex = Index.LUTOffset;
		
		// Initialize anything that wants to be initialized first
		for ( ; TrackIndex < Index.LUTOffset + Index.NumInitPtrs; ++TrackIndex)
		{
			FMovieSceneEvaluationFieldSegmentPtr SegmentPtr = Group.SegmentPtrLUT[TrackIndex];

			// Ensure we're able to find the sequence instance in our root if we've overridden
			SegmentPtr.SequenceID = RootOverridePath.Remap(SegmentPtr.SequenceID);

			const FMovieSceneEvaluationPtrs&  EvalPtrs = EvaluationPtrCache.GetChecked(SegmentPtr.SequenceID);
			const FMovieSceneEvaluationTrack* Track    = EvalPtrs.Template->FindTrack(SegmentPtr.TrackIdentifier);

			if (Track)
			{
				Operand.ObjectBindingID = Track->GetObjectBindingID();
				Operand.SequenceID = SegmentPtr.SequenceID;
				
				FMovieSceneEvaluationKey TrackKey(SegmentPtr.SequenceID, SegmentPtr.TrackIdentifier);

				PersistentDataProxy.SetTrackKey(TrackKey);
				Player.PreAnimatedState.SetCaptureEntity(TrackKey, EMovieSceneCompletionMode::KeepState);

				SubContext = Context;
				if (EvalPtrs.SubData)
				{
					SubContext = Context.Transform(EvalPtrs.SubData->RootToSequenceTransform, EvalPtrs.SubData->TickResolution);

					// Hittest against the sequence's pre and postroll ranges
					SubContext.ReportOuterSectionRanges(EvalPtrs.SubData->PreRollRange.Value, EvalPtrs.SubData->PostRollRange.Value);
					SubContext.SetHierarchicalBias(EvalPtrs.SubData->HierarchicalBias);
				}

				Track->Initialize(SegmentPtr.SegmentID, Operand, SubContext, PersistentDataProxy, Player);
			}
		}

		// Then evaluate

		// *Threading candidate*
		// @todo: if we want to make this threaded, we need to:
		//  - Make the execution tokens threadsafe, and sortable (one container per thread + append?)
		//  - Do the above in a lockless manner
		for (; TrackIndex < Index.LUTOffset + Index.NumInitPtrs + Index.NumEvalPtrs; ++TrackIndex)
		{
			FMovieSceneEvaluationFieldSegmentPtr SegmentPtr = Group.SegmentPtrLUT[TrackIndex];

			// Ensure we're able to find the sequence instance in our root if we've overridden
			SegmentPtr.SequenceID = RootOverridePath.Remap(SegmentPtr.SequenceID);

			const FMovieSceneEvaluationPtrs&  EvalPtrs = EvaluationPtrCache.GetChecked(SegmentPtr.SequenceID);
			const FMovieSceneEvaluationTrack* Track    = EvalPtrs.Template->FindTrack(SegmentPtr.TrackIdentifier);

			if (Track)
			{
				Operand.ObjectBindingID = Track->GetObjectBindingID();
				Operand.SequenceID = SegmentPtr.SequenceID;

				FMovieSceneEvaluationKey TrackKey(SegmentPtr.SequenceID, SegmentPtr.TrackIdentifier);

				PersistentDataProxy.SetTrackKey(TrackKey);

				ExecutionTokens.SetOperand(Operand);
				ExecutionTokens.SetCurrentScope(FMovieSceneEvaluationScope(TrackKey, EMovieSceneCompletionMode::KeepState));

				SubContext = Context;
				if (EvalPtrs.SubData)
				{
					SubContext = Context.Transform(EvalPtrs.SubData->RootToSequenceTransform, EvalPtrs.SubData->TickResolution);

					// Hittest against the sequence's pre and postroll ranges
					SubContext.ReportOuterSectionRanges(EvalPtrs.SubData->PreRollRange.Value, EvalPtrs.SubData->PostRollRange.Value);
					SubContext.SetHierarchicalBias(EvalPtrs.SubData->HierarchicalBias);
				}

				Track->Evaluate(
					SegmentPtr.SegmentID,
					Operand,
					SubContext,
					PersistentDataProxy,
					ExecutionTokens);
			}
		}

		ExecutionTokens.Apply(Context, Player);
	}
}

void FMovieSceneRootEvaluationTemplateInstance::CallSetupTearDown(IMovieScenePlayer& Player)
{
	UMovieSceneSequence* RootSequencePtr      = RootSequence.Get();
	UMovieSceneSequence* OverrideRootSequence = nullptr;

	if (RootID == MovieSceneSequenceID::Root)
	{
		OverrideRootSequence = RootSequencePtr;
	}
	else if (RootSequencePtr && RootTemplate)
	{
		// Evaluate Sub Sequences in Isolation is turned on
		const FMovieSceneSubSequenceData* OverrideSubData = GetHierarchy().FindSubData(RootID);
		if (OverrideSubData)
		{
			OverrideRootSequence = OverrideSubData->GetSequence();
		}
	}

	if (OverrideRootSequence)
	{
		// Cache all the pointers needed for the teardown
		FMovieSceneEvaluationPtrCache PtrCache = ConstructEvaluationPtrCacheForFrame(OverrideRootSequence);

		// Ensure the correct sequences are assigned for each sequence ID
		for (auto Iter = PtrCache.CreateConstIterator(); Iter; ++Iter)
		{
			Player.State.AssignSequence(Iter.Key(), *Iter.Value().Sequence, Player);
		}

		CallSetupTearDown(PtrCache, Player);
	}
}

void FMovieSceneRootEvaluationTemplateInstance::CallSetupTearDown(const FMovieSceneEvaluationPtrCache& EvaluationPtrCache, IMovieScenePlayer& Player, FDelayedPreAnimatedStateRestore* DelayedRestore)
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_CallSetupTearDown);

	FPersistentEvaluationData PersistentDataProxy(Player);

	TArray<FMovieSceneOrderedEvaluationKey> ExpiredEntities;
	TArray<FMovieSceneOrderedEvaluationKey> NewEntities;
	ThisFrameMetaData.DiffEntities(LastFrameMetaData, &NewEntities, &ExpiredEntities);

	for (const FMovieSceneOrderedEvaluationKey& OrderedKey : ExpiredEntities)
	{
		FMovieSceneEvaluationKey Key = OrderedKey.Key;

		// Ensure we're able to find the sequence instance in our root if we've overridden
		Key.SequenceID = RootOverridePath.Remap(Key.SequenceID);

		const FMovieSceneEvaluationPtrs* EvalPtrs = EvaluationPtrCache.Find(Key.SequenceID);
		if (EvalPtrs)
		{
			const FMovieSceneEvaluationTrack* Track = EvalPtrs->Template->FindTrack(Key.TrackIdentifier);
			const bool bStaleTrack = EvalPtrs->Template->IsTrackStale(Key.TrackIdentifier);

			// Track data key may be required by both tracks and sections
			PersistentDataProxy.SetTrackKey(Key.AsTrack());

			if (Key.SectionIndex == uint32(-1))
			{
				if (Track)
				{
					Track->OnEndEvaluation(PersistentDataProxy, Player);
				}
				
				PersistentDataProxy.ResetTrackData();
			}
			else
			{
				PersistentDataProxy.SetSectionKey(Key);
				if (Track && Track->HasChildTemplate(Key.SectionIndex))
				{
					Track->GetChildTemplate(Key.SectionIndex).OnEndEvaluation(PersistentDataProxy, Player);
				}

				PersistentDataProxy.ResetSectionData();
			}

			if (bStaleTrack && DelayedRestore)
			{
				DelayedRestore->Add(Key);
			}
			else
			{
				Player.PreAnimatedState.RestorePreAnimatedState(Player, Key);
			}
		}
	}

	for (const FMovieSceneOrderedEvaluationKey& OrderedKey : NewEntities)
	{
		FMovieSceneEvaluationKey Key = OrderedKey.Key;

		// Ensure we're able to find the sequence instance in our root if we've overridden
		Key.SequenceID = RootOverridePath.Remap(Key.SequenceID);

		const FMovieSceneEvaluationPtrs&  EvalPtrs = EvaluationPtrCache.GetChecked(Key.SequenceID);
		const FMovieSceneEvaluationTrack* Track    = EvalPtrs.Template->FindTrack(Key.TrackIdentifier);

		if (Track)
		{
			PersistentDataProxy.SetTrackKey(Key.AsTrack());

			if (Key.SectionIndex == uint32(-1))
			{
				Track->OnBeginEvaluation(PersistentDataProxy, Player);
			}
			else if (Track->HasChildTemplate(Key.SectionIndex))
			{
				PersistentDataProxy.SetSectionKey(Key);
				Track->GetChildTemplate(Key.SectionIndex).OnBeginEvaluation(PersistentDataProxy, Player);
			}
		}
	}

	// Tear down spawned objects
	FMovieSceneSpawnRegister& Register = Player.GetSpawnRegister();

	TArray<FMovieSceneSequenceID> ExpiredSequenceIDs;
	ThisFrameMetaData.DiffSequences(LastFrameMetaData, nullptr, &ExpiredSequenceIDs);
	for (FMovieSceneSequenceID ExpiredID : ExpiredSequenceIDs)
	{
		Register.OnSequenceExpired(RootOverridePath.Remap(ExpiredID), Player);
	}
}

bool FMovieSceneRootEvaluationTemplateInstance::IsDirty(TSet<UMovieSceneSequence*>* OutDirtySequences) const
{
	UMovieSceneSequence* RootSequencePtr = RootSequence.Get();

	// Dirty if our master sequence is no longer valid
	if (!RootSequencePtr || !RootTemplate)
	{
		return true;
	}

	bool bIsDirty = false;

	// Dirty if our master sequence signature doesn't match the template
	if (RootTemplate->SequenceSignature != RootSequencePtr->GetSignature())
	{
		bIsDirty = true;

		if (OutDirtySequences)
		{
			OutDirtySequences->Add(RootSequencePtr);
		}
	}

	UMovieSceneSequence*           OverrideRootSequence = RootSequencePtr;
	FMovieSceneEvaluationTemplate* OverrideRootTemplate = RootTemplate;

	// Find the sequence we're actually evaluating (only != MovieSceneSequenceID::Root when "Evaluate Sequences in Isolation" is on)
	if (RootID != MovieSceneSequenceID::Root)
	{
		OverrideRootSequence = GetSequence(RootID);
		OverrideRootTemplate = OverrideRootSequence ? &TemplateStore->AccessTemplate(*OverrideRootSequence) : nullptr;

		// Dirty if the root sequence is not valid
		if (!OverrideRootSequence || !OverrideRootTemplate)
		{
			bIsDirty = true;
		}
		else
		{
			// Dirty if our root override template signature doesn't match the sequence
			if (OverrideRootTemplate->SequenceSignature != OverrideRootSequence->GetSignature())
			{
				bIsDirty = true;

				if (OutDirtySequences)
				{
					OutDirtySequences->Add(OverrideRootSequence);
				}
			}
		}
	}

	// Dirty if anything we evaluated last frame is dirty
	if (OverrideRootTemplate && LastFrameMetaData.IsDirty(OverrideRootTemplate->Hierarchy, *TemplateStore, nullptr, OutDirtySequences))
	{
		bIsDirty = true;
	}

	return bIsDirty;
}

void FMovieSceneRootEvaluationTemplateInstance::CopyActuators(FMovieSceneBlendingAccumulator& Accumulator) const
{
	Accumulator.Actuators = ExecutionTokens.GetBlendingAccumulator().Actuators;
}

UMovieSceneSequence* FMovieSceneRootEvaluationTemplateInstance::GetSequence(FMovieSceneSequenceIDRef SequenceID) const
{
	UMovieSceneSequence* RootSequencePtr = RootSequence.Get();
	if (!RootSequencePtr)
	{
		return nullptr;
	}
	else if (SequenceID == MovieSceneSequenceID::Root)
	{
		return RootSequencePtr;
	}
	
	const FMovieSceneSubSequenceData* SubData = GetHierarchy().FindSubData(SequenceID);
	return SubData ? SubData->GetSequence() : nullptr;
}

FMovieSceneEvaluationTemplate* FMovieSceneRootEvaluationTemplateInstance::FindTemplate(FMovieSceneSequenceIDRef SequenceID)
{
	if (SequenceID == MovieSceneSequenceID::Root)
	{
		return RootTemplate;
	}

	const FMovieSceneSubSequenceData* SubData = GetHierarchy().FindSubData(SequenceID);
	UMovieSceneSequence* Sequence = SubData ? SubData->GetSequence() : nullptr;

	return Sequence ? &TemplateStore->AccessTemplate(*Sequence) : nullptr;
}

UObject* FMovieSceneRootEvaluationTemplateInstance::GetOrCreateDirectorInstance(FMovieSceneSequenceIDRef SequenceID, IMovieScenePlayer& Player)
{
	UObject* ExistingDirectorInstance = DirectorInstances.FindRef(SequenceID);
	if (ExistingDirectorInstance)
	{
		return ExistingDirectorInstance;
	}

	UMovieSceneSequence* Sequence            = GetSequence(SequenceID);
	UObject*             NewDirectorInstance = Sequence->CreateDirectorInstance(Player);

	if (NewDirectorInstance)
	{
		DirectorInstances.Add(SequenceID, NewDirectorInstance);
	}

	return NewDirectorInstance;
}

void FMovieSceneRootEvaluationTemplateInstance::ResetDirectorInstances()
{
	DirectorInstances.Reset();
}
