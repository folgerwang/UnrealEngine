// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KeyframeTrackEditor.h"
#include "Tracks/MovieScenePrimitiveMaterialTrack.h"
#include "Materials/MaterialInterface.h"

class FPrimitiveMaterialTrackEditor : public FKeyframeTrackEditor<UMovieScenePrimitiveMaterialTrack>
{
public:

	/** Constructor. */
	FPrimitiveMaterialTrackEditor(TSharedRef<ISequencer> InSequencer);

	/** Factory function */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

public:

	//~ ISequencerTrackEditor interface
	virtual void ExtendObjectBindingTrackMenu(TSharedRef<FExtender> Extender, const FGuid& ObjectBinding, const UClass* ObjectClass) override;

	void ConstructObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding);
	void CreateTrackForElement(FGuid ObjectBindingID, int32 MaterialIndex);
};
