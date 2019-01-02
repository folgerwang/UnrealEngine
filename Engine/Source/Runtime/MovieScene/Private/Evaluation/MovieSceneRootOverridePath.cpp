// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneRootOverridePath.h"
#include "Evaluation/MovieSceneSequenceHierarchy.h"

void FMovieSceneRootOverridePath::Reset()
{
	ReverseOverrideRootPath.Reset();
}

void FMovieSceneRootOverridePath::Set(FMovieSceneSequenceID OverrideRootID, const FMovieSceneSequenceHierarchy& RootHierarchy)
{
	ReverseOverrideRootPath.Reset();

	FMovieSceneSequenceID CurrentSequenceID = OverrideRootID;

	while (CurrentSequenceID != MovieSceneSequenceID::Root)
	{
		const FMovieSceneSequenceHierarchyNode* CurrentNode = RootHierarchy.FindNode(CurrentSequenceID);
		const FMovieSceneSubSequenceData* OuterSubData = RootHierarchy.FindSubData(CurrentSequenceID);
		if (!ensureAlwaysMsgf(CurrentNode && OuterSubData, TEXT("Malformed sequence hierarchy")))
		{
			return;
		}

		ReverseOverrideRootPath.Add(OuterSubData->DeterministicSequenceID);
		CurrentSequenceID = CurrentNode->ParentID;
	}
}
