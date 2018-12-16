// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sequencer/MovieSceneControlRigSection.h"
#include "Animation/AnimSequence.h"
#include "Logging/MessageLog.h"
#include "MovieScene.h"
#include "Sequencer/ControlRigSequence.h"
#include "Sequencer/ControlRigBindingTemplate.h"
#include "Sequencer/MovieSceneControlRigInstanceData.h"
#include "Channels/MovieSceneChannelProxy.h"

#define LOCTEXT_NAMESPACE "MovieSceneControlRigSection"

UMovieSceneControlRigSection::UMovieSceneControlRigSection()
{
	// Section template relies on always restoring state for objects when they are no longer animating. This is how it releases animation control.
	EvalOptions.CompletionMode = EMovieSceneCompletionMode::RestoreState;

	Weight.SetDefault(1.0f);

#if WITH_EDITOR

	static const FMovieSceneChannelMetaData MetaData("Weight", LOCTEXT("WeightChannelText", "Weight"));
	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(Weight, MetaData, TMovieSceneExternalValue<float>());

#else

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(Weight);

#endif
}

void UMovieSceneControlRigSection::OnDilated(float DilationFactor, FFrameNumber Origin)
{
	Parameters.TimeScale /= DilationFactor;
}

FMovieSceneSubSequenceData UMovieSceneControlRigSection::GenerateSubSequenceData(const FSubSequenceInstanceDataParams& Params) const
{
	FMovieSceneSubSequenceData SubData(*this);

	FMovieSceneControlRigInstanceData InstanceData;
	InstanceData.bAdditive = bAdditive;
	InstanceData.bApplyBoneFilter = bApplyBoneFilter;
	if (InstanceData.bApplyBoneFilter)
	{
		InstanceData.BoneFilter = BoneFilter;
	}
	else
	{
		InstanceData.BoneFilter.BranchFilters.Empty();
	}
	InstanceData.Operand = Params.Operand;

	InstanceData.Weight = Weight;

	// Apply timescale and start offset so the weight curve is in the inner sequence's space
	FMovieSceneSequenceTransform OuterToInner = OuterToInnerTransform();

	TArrayView<FFrameNumber> Times = InstanceData.Weight.GetData().GetTimes();
	for (FFrameNumber& Time : Times)
	{
		Time = (Time * OuterToInner).FloorToFrame();
	}

	SubData.InstanceData = FMovieSceneSequenceInstanceDataPtr(InstanceData);

	return SubData;
}

#undef LOCTEXT_NAMESPACE 
