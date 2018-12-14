// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneEnumSection.h"
#include "UObject/SequencerObjectVersion.h"
#include "UObject/SequencerObjectVersion.h"
#include "UObject/SequencerObjectVersion.h"
#include "Channels/MovieSceneChannelProxy.h"

UMovieSceneEnumSection::UMovieSceneEnumSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	EvalOptions.EnableAndSetCompletionMode
		(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToRestoreState ? 
			EMovieSceneCompletionMode::KeepState : 
			GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault ? 
			EMovieSceneCompletionMode::RestoreState : 
			EMovieSceneCompletionMode::ProjectDefault);
	bSupportsInfiniteRange = true;
#if WITH_EDITOR

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(EnumCurve, FMovieSceneChannelMetaData(), TMovieSceneExternalValue<uint8>::Make());

#else

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(EnumCurve);

#endif
}