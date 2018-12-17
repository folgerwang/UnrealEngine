// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneBinding.h"
#include "MovieSceneTrack.h"


/* FMovieSceneBinding interface
 *****************************************************************************/

void FMovieSceneBinding::AddTrack(UMovieSceneTrack& NewTrack)
{
	Tracks.Add(&NewTrack);
}

bool FMovieSceneBinding::RemoveTrack(UMovieSceneTrack& Track)
{
	return (Tracks.RemoveSingle(&Track) != 0);
}

#if WITH_EDITOR
void FMovieSceneBinding::PerformCookOptimization(bool& bShouldRemoveObject)
{
	for (int32 Index = Tracks.Num() - 1; Index >= 0; --Index)
	{
		ECookOptimizationFlags Flags = Tracks[Index]->GetCookOptimizationFlags();

		if ((Flags & ECookOptimizationFlags::RemoveObject) != ECookOptimizationFlags::None)
		{
			bShouldRemoveObject = true;
			return;
		}
		else if ((Flags & ECookOptimizationFlags::RemoveTrack) != ECookOptimizationFlags::None)
		{
			Tracks.RemoveAtSwap(Index, 1, false);
		}
	}
}
#endif
