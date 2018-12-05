// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSequenceEditor.h"
#include "ActorSequence.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "K2Node_FunctionEntry.h"
#include "EdGraphSchema_K2.h"

struct FMovieSceneSequenceEditor_ActorSequence : FMovieSceneSequenceEditor
{
	virtual bool CanCreateEvents(UMovieSceneSequence* InSequence) const
	{
		return true;
	}

	virtual UBlueprint* GetBlueprintForSequence(UMovieSceneSequence* InSequence) const override
	{
		UActorSequence* ActorSequence = CastChecked<UActorSequence>(InSequence);
		if (UBlueprint* Blueprint = ActorSequence->GetParentBlueprint())
		{
			return Blueprint;
		}

		UActorSequenceComponent* Component = ActorSequence->GetTypedOuter<UActorSequenceComponent>();
		ULevel* Level = Component ? Component->GetOwner()->GetLevel() : nullptr;

		bool bDontCreateNewBlueprint = true;
		return Level ? Level->GetLevelScriptBlueprint(bDontCreateNewBlueprint) : nullptr;
	}

	virtual UBlueprint* CreateBlueprintForSequence(UMovieSceneSequence* InSequence) const override
	{
		UActorSequence* ActorSequence = CastChecked<UActorSequence>(InSequence);
		check(!ActorSequence->GetParentBlueprint());

		UActorSequenceComponent* Component = ActorSequence->GetTypedOuter<UActorSequenceComponent>();
		ULevel* Level = Component ? Component->GetOwner()->GetLevel() : nullptr;

		bool bDontCreateNewBlueprint = false;
		return Level ? Level->GetLevelScriptBlueprint(bDontCreateNewBlueprint) : nullptr;
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