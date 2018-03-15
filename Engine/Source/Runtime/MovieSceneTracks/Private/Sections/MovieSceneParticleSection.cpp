// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneParticleSection.h"
#include "Evaluation/MovieSceneParticleTemplate.h"
#include "UObject/SequencerObjectVersion.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "UObject/Package.h"

FMovieSceneParticleChannel::FMovieSceneParticleChannel()
{
	SetEnum(FindObject<UEnum>(ANY_PACKAGE, TEXT("EParticleKey")));
}

uint32 FMovieSceneParticleChannel::GetChannelID()
{
	static const uint32 ID = FMovieSceneChannelEntry::RegisterNewID();
	return ID;
}

UMovieSceneParticleSection::UMovieSceneParticleSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
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

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(ParticleKeys, FMovieSceneChannelEditorData(), TMovieSceneExternalValue<uint8>());

#else

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(ParticleKeys);

#endif
}

FMovieSceneEvalTemplatePtr UMovieSceneParticleSection::GenerateTemplate() const
{
	return FMovieSceneParticleSectionTemplate(*this);
}
