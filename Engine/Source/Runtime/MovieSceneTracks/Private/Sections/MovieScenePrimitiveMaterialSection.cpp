// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieScenePrimitiveMaterialSection.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Materials/MaterialInterface.h"


UMovieScenePrimitiveMaterialSection::UMovieScenePrimitiveMaterialSection(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	bSupportsInfiniteRange = true;
	EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::ProjectDefault);
	SetRange(TRange<FFrameNumber>::All());

	MaterialChannel.SetPropertyClass(UMaterialInterface::StaticClass());

#if WITH_EDITOR
	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MaterialChannel, FMovieSceneChannelMetaData(), TMovieSceneExternalValue<UObject*>::Make());
#else
	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MaterialChannel);
#endif
}