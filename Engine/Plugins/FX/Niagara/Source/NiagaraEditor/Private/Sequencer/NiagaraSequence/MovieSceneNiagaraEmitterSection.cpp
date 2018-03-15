// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneNiagaraEmitterSection.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandleViewModel.h"
#include "NiagaraEmitterViewModel.h"
#include "Algo/BinarySearch.h"
#include "Channels/MovieSceneChannelProxy.h"

#define LOCTEXT_NAMESPACE "MovieSceneNiagaraEmitterSection"

uint32 FMovieSceneNiagaraEmitterChannel::GetChannelID()
{
	static uint32 ID = FMovieSceneChannelEntry::RegisterNewID();
	return ID;
}

bool FMovieSceneNiagaraEmitterChannel::Evaluate(FFrameTime InTime, FMovieSceneBurstKey& OutValue) const
{
	if (Times.Num())
	{
		const int32 Index = FMath::Max(0, Algo::UpperBound(Times, InTime.FrameNumber)-1);
		OutValue = Values[Index];
		return true;
	}

	return false;
}

UMovieSceneNiagaraEmitterSection::UMovieSceneNiagaraEmitterSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR

	static const FMovieSceneChannelEditorData EditorData;
	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(Channel, EditorData);

#else

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(Channel);

#endif
}

TSharedPtr<FNiagaraEmitterHandleViewModel> UMovieSceneNiagaraEmitterSection::GetEmitterHandle()
{
	return EmitterHandleViewModel.Pin();
}

void UMovieSceneNiagaraEmitterSection::SetEmitterHandle(TSharedRef<FNiagaraEmitterHandleViewModel> InEmitterHandleViewModel)
{
	EmitterHandleViewModel = InEmitterHandleViewModel;
}

#undef LOCTEXT_NAMESPACE // MovieSceneNiagaraEmitterSection