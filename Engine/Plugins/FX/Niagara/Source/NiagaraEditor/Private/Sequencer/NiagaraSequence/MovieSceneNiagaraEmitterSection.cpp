// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneNiagaraEmitterSection.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandleViewModel.h"
#include "NiagaraEmitterViewModel.h"
#include "Algo/BinarySearch.h"
#include "Channels/MovieSceneChannelProxy.h"

#define LOCTEXT_NAMESPACE "MovieSceneNiagaraEmitterSection"

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

void FMovieSceneNiagaraEmitterChannel::GetKeys(const TRange<FFrameNumber>& WithinRange, TArray<FFrameNumber>* OutKeyTimes, TArray<FKeyHandle>* OutKeyHandles)
{
	GetData().GetKeys(WithinRange, OutKeyTimes, OutKeyHandles);
}

void FMovieSceneNiagaraEmitterChannel::GetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<FFrameNumber> OutKeyTimes)
{
	GetData().GetKeyTimes(InHandles, OutKeyTimes);
}

void FMovieSceneNiagaraEmitterChannel::SetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<const FFrameNumber> InKeyTimes)
{
	GetData().SetKeyTimes(InHandles, InKeyTimes);
}

void FMovieSceneNiagaraEmitterChannel::DuplicateKeys(TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyHandle> OutNewHandles)
{
	GetData().DuplicateKeys(InHandles, OutNewHandles);
}

void FMovieSceneNiagaraEmitterChannel::DeleteKeys(TArrayView<const FKeyHandle> InHandles)
{
	GetData().DeleteKeys(InHandles);
}

void FMovieSceneNiagaraEmitterChannel::ChangeFrameResolution(FFrameRate SourceRate, FFrameRate DestinationRate)
{
	GetData().ChangeFrameResolution(SourceRate, DestinationRate);
}

TRange<FFrameNumber> FMovieSceneNiagaraEmitterChannel::ComputeEffectiveRange() const
{
	return GetData().GetTotalRange();
}

int32 FMovieSceneNiagaraEmitterChannel::GetNumKeys() const
{
	return Times.Num();
}

void FMovieSceneNiagaraEmitterChannel::Reset()
{
	Times.Reset();
	Values.Reset();
	KeyHandles.Reset();
}

void FMovieSceneNiagaraEmitterChannel::Offset(FFrameNumber DeltaPosition)
{
	GetData().Offset(DeltaPosition);
}

UMovieSceneNiagaraEmitterSection::UMovieSceneNiagaraEmitterSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR

	static const FMovieSceneChannelMetaData MetaData;
	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(Channel, MetaData);

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