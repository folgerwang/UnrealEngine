// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneEventTriggerSection.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Engine/Blueprint.h"


UMovieSceneEventTriggerSection::UMovieSceneEventTriggerSection(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	bSupportsInfiniteRange = true;
	SetRange(TRange<FFrameNumber>::All());

#if WITH_EDITOR

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(EventChannel, FMovieSceneChannelMetaData());

#else

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(EventChannel);

#endif
}


#if WITH_EDITORONLY_DATA

void UMovieSceneEventTriggerSection::OnBlueprintRecompiled(UBlueprint* InBlueprint)
{
	bool bHasChanged = false;

	for (FMovieSceneEvent& Event : EventChannel.GetData().GetValues())
	{
		FName OldFunctionName = Event.FunctionName;

		Event.CacheFunctionName();

		if (Event.FunctionName != OldFunctionName)
		{
			bHasChanged = true;
		}
	}

	if (bHasChanged)
	{
		MarkAsChanged();
		MarkPackageDirty();
	}
}

#endif		// WITH_EDITORONLY_DATA