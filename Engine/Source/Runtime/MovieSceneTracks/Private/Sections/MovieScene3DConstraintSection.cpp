// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieScene3DConstraintSection.h"
#include "MovieSceneObjectBindingID.h"


UMovieScene3DConstraintSection::UMovieScene3DConstraintSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ 
	bSupportsInfiniteRange = true;
}

void UMovieScene3DConstraintSection::SetConstraintId(const FGuid& InConstraintId, const FMovieSceneSequenceID& SequenceID)
{
	if (TryModify())
	{
		SetConstraintBindingID(FMovieSceneObjectBindingID(InConstraintId, SequenceID, EMovieSceneObjectBindingSpace::Root)); //use root if specifying binding
	}
}

void UMovieScene3DConstraintSection::SetConstraintId(const FGuid& InConstraintId)
{
	if (TryModify())
	{
		SetConstraintBindingID(FMovieSceneObjectBindingID(InConstraintId, MovieSceneSequenceID::Root, EMovieSceneObjectBindingSpace::Local));
	}
}

void UMovieScene3DConstraintSection::OnBindingsUpdated(const TMap<FGuid, FGuid>& OldGuidToNewGuidMap)
{
	if (OldGuidToNewGuidMap.Contains(ConstraintBindingID.GetGuid()))
	{
		ConstraintBindingID.SetGuid(OldGuidToNewGuidMap[ConstraintBindingID.GetGuid()]);
	}
}

void UMovieScene3DConstraintSection::GetReferencedBindings(TArray<FGuid>& OutBindings)
{
	OutBindings.Add(ConstraintBindingID.GetGuid());
}

void UMovieScene3DConstraintSection::PostLoad()
{
	Super::PostLoad();

	if (ConstraintId_DEPRECATED.IsValid())
	{
		if (!ConstraintBindingID.IsValid())
		{
			ConstraintBindingID = FMovieSceneObjectBindingID(ConstraintId_DEPRECATED, MovieSceneSequenceID::Root, EMovieSceneObjectBindingSpace::Local);
		}
		ConstraintId_DEPRECATED.Invalidate();
	}
}
