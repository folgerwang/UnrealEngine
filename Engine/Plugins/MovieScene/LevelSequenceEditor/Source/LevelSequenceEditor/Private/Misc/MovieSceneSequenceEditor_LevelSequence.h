// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSequenceEditor.h"
#include "LevelSequenceDirector.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "K2Node_FunctionEntry.h"

struct FMovieSceneSequenceEditor_LevelSequence : FMovieSceneSequenceEditor
{
	virtual bool CanCreateEvents(UMovieSceneSequence* InSequence) const
	{
		return true;
	}

	virtual UBlueprint* GetBlueprintForSequence(UMovieSceneSequence* InSequence) const override
	{
		ULevelSequence* LevelSequence = CastChecked<ULevelSequence>(InSequence);
		return LevelSequence->GetDirectorBlueprint();
	}

	virtual UBlueprint* CreateBlueprintForSequence(UMovieSceneSequence* InSequence) const override
	{
		UBlueprint* Blueprint = GetBlueprintForSequence(InSequence);
		if (!ensureMsgf(!Blueprint, TEXT("Should not call CreateBlueprintForSequence when one already exists")))
		{
			return Blueprint;
		}

		ULevelSequence* LevelSequence = CastChecked<ULevelSequence>(InSequence);

		FName BlueprintName = "SequenceDirector";
		Blueprint = FKismetEditorUtilities::CreateBlueprint(ULevelSequenceDirector::StaticClass(), InSequence, BlueprintName, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());

		LevelSequence->SetDirectorBlueprint(Blueprint);
		return Blueprint;
	}

	virtual void SetupDefaultPinForEndpoint(UMovieSceneEventTrack* EventTrack, UK2Node_FunctionEntry* Endpoint) const override
	{
		// By default for master tracks we create a pin (that receives the level blueprint for master event tracks, or the event receivers)
		UClass* PinClass = nullptr;

		// When event receivers are not set, we set the pin type to the type of the object binding
		if (EventTrack->EventReceivers.Num() == 0)
		{
			PinClass = FindTrackObjectBindingClass(EventTrack);
		}

		FEdGraphPinType PinType;
		PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
		PinType.PinSubCategoryObject = PinClass ? PinClass : UObject::StaticClass();

		Endpoint->CreateUserDefinedPin(TargetPinName, PinType, EGPD_Output, true);
	}
};