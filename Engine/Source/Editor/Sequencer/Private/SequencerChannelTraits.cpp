// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SequencerChannelTraits.h"
#include "EditorStyleSet.h"

namespace Sequencer
{


void DrawKeys(void* Channel, TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyDrawParams> OutKeyDrawParams)
{
	// By default just render diamonds for keys
	FKeyDrawParams DefaultParams;
	DefaultParams.BorderBrush = DefaultParams.FillBrush = FEditorStyle::Get().GetBrush("Sequencer.KeyDiamond");

	for (FKeyDrawParams& Param : OutKeyDrawParams)
	{
		Param = DefaultParams;
	}
}



}	// namespace Sequencer