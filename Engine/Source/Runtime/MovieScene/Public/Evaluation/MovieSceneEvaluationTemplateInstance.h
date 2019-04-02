// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Map.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "MovieSceneSequenceID.h"
#include "Evaluation/MovieSceneSequenceTransform.h"
#include "Evaluation/MovieScenePlayback.h"
#include "Evaluation/MovieSceneSequenceTemplateStore.h"
#include "Evaluation/MovieSceneEvaluationTemplate.h"
#include "Evaluation/MovieSceneExecutionTokens.h"
#include "Evaluation/MovieSceneRootOverridePath.h"
#include "MovieSceneEvaluationTemplateInstance.generated.h"

class UMovieSceneSequence;
struct FDelayedPreAnimatedStateRestore;
struct FMovieSceneEvaluationPtrCache;


/**
 * Root evaluation template instance used to play back any sequence
 */
USTRUCT()
struct FMovieSceneRootEvaluationTemplateInstance
{
public:
	GENERATED_BODY()

	MOVIESCENE_API FMovieSceneRootEvaluationTemplateInstance();
	MOVIESCENE_API ~FMovieSceneRootEvaluationTemplateInstance();

	FMovieSceneRootEvaluationTemplateInstance(const FMovieSceneRootEvaluationTemplateInstance&) = delete;
	FMovieSceneRootEvaluationTemplateInstance& operator=(const FMovieSceneRootEvaluationTemplateInstance&) = delete;

	FMovieSceneRootEvaluationTemplateInstance(FMovieSceneRootEvaluationTemplateInstance&&) = delete;
	FMovieSceneRootEvaluationTemplateInstance& operator=(FMovieSceneRootEvaluationTemplateInstance&&) = delete;

	/**
	 * Check if this instance has been initialized correctly
	 */
	bool IsValid() const
	{
		return RootSequence.Get() && RootTemplate;
	}

	/**
	 * Initialize this template instance with the specified sequence
	 *
	 * @param RootSequence				The sequence play back
	 * @param Player					The player responsible for playback
	 */
	MOVIESCENE_API void Initialize(UMovieSceneSequence& RootSequence, IMovieScenePlayer& Player);

	/**
	 * Initialize this template instance with the specified sequence
	 *
	 * @param RootSequence				The sequence play back
	 * @param Player					The player responsible for playback
	 * @param TemplateStore				Template store responsible for supplying templates for a given sequence
	 */
	MOVIESCENE_API void Initialize(UMovieSceneSequence& RootSequence, IMovieScenePlayer& Player, TSharedRef<IMovieSceneSequenceTemplateStore> TemplateStore);

	/**
	 * Evaluate this sequence
	 *
	 * @param Context				Evaluation context containing the time (or range) to evaluate
	 * @param Player				The player responsible for playback
	 * @param OverrideRootID		The ID of the sequence from which to evaluate.
	 */
	MOVIESCENE_API void Evaluate(FMovieSceneContext Context, IMovieScenePlayer& Player, FMovieSceneSequenceID OverrideRootID = MovieSceneSequenceID::Root);

	/**
	 * Indicate that we're not going to evaluate this instance again, and that we should tear down any current state
	 *
	 * @param Player				The player responsible for playback
	 */
	MOVIESCENE_API void Finish(IMovieScenePlayer& Player);

	/**
	 * Check whether the evaluation template is dirty based on the last evaluated frame's meta-data
	 *
	 * @param OutDirtySequences		(Optional) A set to populate with dirty sequences
	 */
	MOVIESCENE_API bool IsDirty(TSet<UMovieSceneSequence*>* OutDirtySequences = nullptr) const;

public:

	/**
	 * Attempt to locate the underlying sequence given a sequence ID
	 *
	 * @param SequenceID 			ID of the sequence to locate
	 * @return The sequence, or nullptr if the ID was not found
	 */
	MOVIESCENE_API UMovieSceneSequence* GetSequence(FMovieSceneSequenceIDRef SequenceID) const;

	/**
	 * Attempt to locate a template corresponding to the specified Sequence ID
	 *
	 * @param SequenceID 			ID of the sequence template to locate
	 * @return The template, or nullptr if the ID is invalid, or the template has not been compiled
	 */
	MOVIESCENE_API FMovieSceneEvaluationTemplate* FindTemplate(FMovieSceneSequenceIDRef SequenceID);

	/**
	 * Locate a director instance object for the specified sequence ID, creating one if necessary
	 *
	 * @param SequenceID 			ID of the sequence template to locate a director instance for
	 * @return A director instance as defined by the specific sequence, or nullptr if one could not be found or created
	 */
	MOVIESCENE_API UObject* GetOrCreateDirectorInstance(FMovieSceneSequenceIDRef SequenceID, IMovieScenePlayer& Player);

	/**
	 * Resets all the director instances currently stored by this template instance
	 */
	MOVIESCENE_API void ResetDirectorInstances();

	/**
	 * Access the master sequence's hierarchy data
	 */
	const FMovieSceneSequenceHierarchy& GetHierarchy() const
	{
		check(RootTemplate);
		return RootTemplate->Hierarchy;
	}

	/**
	 * Access the master sequence's hierarchy data
	 */
	FMovieSceneSequenceHierarchy& GetHierarchy()
	{
		check(RootTemplate);
		return RootTemplate->Hierarchy;
	}

	/**
 	 * Cache of everything that is evaluated this frame 
	 */
	const FMovieSceneEvaluationMetaData& GetThisFrameMetaData() const
	{
		return ThisFrameMetaData;
	}

	/**
	 * Copy any actuators from this template instance into the specified accumulator
	 *
	 * @param Accumulator 			The accumulator to copy actuators into
	 */
	MOVIESCENE_API void CopyActuators(FMovieSceneBlendingAccumulator& Accumulator) const;

private:

	/**
	 * Setup the current frame by finding or generating the necessary evaluation group and meta-data
	 *
	 * @param OverrideRootSequence	Pointer to the sequence that is considered the root for this evaluation (that maps to InOverrideRootID)
	 * @param OverrideRootID		The sequence ID of the currently considered root (normally MovieSceneSequenceID::Root unless Evaluate Sub-Sequences in Isolation is active)
	 * @param InOutContext			The evaluation context for this frame, will be transformed to the correct space if InOverrideRootID is not Root
	 * @return The evaluation group within the root template's evaluation field to evaluate, or nullptr if none could be found or compiled
	 */
	const FMovieSceneEvaluationGroup* SetupFrame(UMovieSceneSequence* OverrideRootSequence, FMovieSceneSequenceID InOverrideRootID, FMovieSceneContext* InOutContext);

	/**
	 * Process entities that are newly evaluated, and those that are no longer being evaluated
	 */
	void CallSetupTearDown(IMovieScenePlayer& Player);

	/**
	 * Process entities that are newly evaluated, and those that are no longer being evaluated
	 */
	void CallSetupTearDown(const FMovieSceneEvaluationPtrCache& EvaluationCache, IMovieScenePlayer& Player, FDelayedPreAnimatedStateRestore* DelayedRestore = nullptr);

	/**
	 * Evaluate a particular group of a segment
	 */
	void EvaluateGroup(const FMovieSceneEvaluationPtrCache& EvaluationCache, const FMovieSceneEvaluationGroup& Group, const FMovieSceneContext& Context, IMovieScenePlayer& Player);

	/**
	 * Construct all the template and sub-data ptrs required for this frame by combining all those needed last frame, with those needed this frame
	 */
	FMovieSceneEvaluationPtrCache ConstructEvaluationPtrCacheForFrame(UMovieSceneSequence* OverrideRootSequence);

private:

	TWeakObjectPtr<UMovieSceneSequence> RootSequence;

	FMovieSceneEvaluationTemplate* RootTemplate;

	/** Sequence ID that was last used to evaluate from */
	FMovieSceneSequenceID RootID;

	/** Map of director instances by sequence ID. Kept alive by this map assuming this struct is reference collected */
	UPROPERTY()
	TMap<FMovieSceneSequenceID, UObject*> DirectorInstances;

	/** Cache of everything that was evaluated last frame */
	FMovieSceneEvaluationMetaData LastFrameMetaData;
	/** Cache of everything that is evaluated this frame */
	FMovieSceneEvaluationMetaData ThisFrameMetaData;

	/** Template store responsible for supplying templates for a given sequence */
	TSharedPtr<IMovieSceneSequenceTemplateStore> TemplateStore;

	/** Override path that is used to remap inner sequence IDs to the root space when evaluating with a root override */
	FMovieSceneRootOverridePath RootOverridePath;

	/** Execution tokens that are used to apply animated state */
	FMovieSceneExecutionTokens ExecutionTokens;
};

template<>
struct TStructOpsTypeTraits<FMovieSceneRootEvaluationTemplateInstance> : public TStructOpsTypeTraitsBase2<FMovieSceneRootEvaluationTemplateInstance>
{
	enum
	{
		WithCopy = false
	};
};