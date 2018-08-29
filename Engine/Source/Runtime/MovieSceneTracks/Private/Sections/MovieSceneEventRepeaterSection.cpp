// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneEventRepeaterSection.h"

#if WITH_EDITORONLY_DATA

void UMovieSceneEventRepeaterSection::OnBlueprintRecompiled(UBlueprint*)
{
	FName OldFunctionName = Event.FunctionName;

	Event.CacheFunctionName();

	if (Event.FunctionName != OldFunctionName)
	{
		MarkAsChanged();
		MarkPackageDirty();
	}
}

#endif		// WITH_EDITORONLY_DATA