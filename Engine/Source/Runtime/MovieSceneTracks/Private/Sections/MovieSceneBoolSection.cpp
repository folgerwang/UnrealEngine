// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneBoolSection.h"
#include "UObject/SequencerObjectVersion.h"
#include "Channels/MovieSceneChannelProxy.h"

UMovieSceneBoolSection::UMovieSceneBoolSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
	, DefaultValue_DEPRECATED(false)
{
	bSupportsInfiniteRange = true;
#if WITH_EDITORONLY_DATA
	bIsInfinite_DEPRECATED = true;
#endif

	SetRange(TRange<FFrameNumber>::All());

	EvalOptions.EnableAndSetCompletionMode
		(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToRestoreState ? 
			EMovieSceneCompletionMode::KeepState : 
			GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault ? 
			EMovieSceneCompletionMode::RestoreState : 
			EMovieSceneCompletionMode::ProjectDefault);

#if WITH_EDITOR

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(BoolCurve, FMovieSceneChannelEditorData(), TMovieSceneExternalValue<bool>::Make());

#else

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(BoolCurve);

#endif
}


void UMovieSceneBoolSection::PostLoad()
{
	if (!BoolCurve.GetDefault().IsSet() && DefaultValue_DEPRECATED)
	{
		BoolCurve.SetDefault(DefaultValue_DEPRECATED);
	}
	Super::PostLoad();
}