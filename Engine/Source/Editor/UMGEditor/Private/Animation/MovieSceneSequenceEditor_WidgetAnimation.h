// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSequenceEditor.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Engine/Blueprint.h"
#include "K2Node_FunctionEntry.h"

struct FMovieSceneSequenceEditor_WidgetAnimation : FMovieSceneSequenceEditor
{
	virtual bool CanCreateEvents(UMovieSceneSequence* InSequence) const
	{
		return true;
	}

	virtual UBlueprint* GetBlueprintForSequence(UMovieSceneSequence* InSequence) const override
	{
		return InSequence->GetTypedOuter<UBlueprint>();
	}

	virtual void SetupDefaultPinForEndpoint(UMovieSceneEventTrack* EventTrack, UK2Node_FunctionEntry* Endpoint) const override
	{
		UClass* PinClass = nullptr;

		// When event receivers are set, we just add a generic object pin as it's impossible to know what type the event receiver is here
		if (EventTrack->EventReceivers.Num() > 0)
		{
			PinClass = UObject::StaticClass();
		}
		// Otherwise we use the object binding's class, or nullptr
		else
		{
			PinClass = FindTrackObjectBindingClass(EventTrack);
		}

		// Do not create a pin for nullptr since we're probably a master track that is just triggering on self
		if (PinClass)
		{
			FEdGraphPinType PinType;
			PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			PinType.PinSubCategoryObject = PinClass;

			Endpoint->CreateUserDefinedPin(TargetPinName, PinType, EGPD_Output, true);
		}
	}
};