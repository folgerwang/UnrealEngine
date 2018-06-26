// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SequencerChannelTraits.h"
#include "EditorStyleSet.h"
#include "CurveModel.h"

namespace Sequencer
{


void DrawKeys(FMovieSceneChannel* Channel, TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyDrawParams> OutKeyDrawParams)
{
	// By default just render diamonds for keys
	FKeyDrawParams DefaultParams;
	DefaultParams.BorderBrush = DefaultParams.FillBrush = FEditorStyle::Get().GetBrush("Sequencer.KeyDiamond");

	for (FKeyDrawParams& Param : OutKeyDrawParams)
	{
		Param = DefaultParams;
	}
}

TUniquePtr<FCurveModel> CreateCurveEditorModel(const FMovieSceneChannelHandle& ChannelHandle, UMovieSceneSection* OwningSection, TSharedRef<ISequencer> InSequencer)
{
	return nullptr;
}

}	// namespace Sequencer