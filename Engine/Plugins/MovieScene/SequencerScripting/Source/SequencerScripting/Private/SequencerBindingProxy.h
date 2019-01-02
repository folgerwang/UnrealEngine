// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"

#include "SequencerBindingProxy.generated.h"

class UMovieScene;
class UMovieSceneSequence;


USTRUCT(BlueprintType)
struct FSequencerBindingProxy
{
	GENERATED_BODY()

	FSequencerBindingProxy()
		: Sequence(nullptr)
	{}

	FSequencerBindingProxy(const FGuid& InBindingID, UMovieSceneSequence* InSequence)
		: BindingID(InBindingID)
		, Sequence(InSequence)
	{}

	UMovieScene* GetMovieScene() const;

	UPROPERTY()
	FGuid BindingID;

	UPROPERTY(BlueprintReadOnly, Category=Binding)
	UMovieSceneSequence* Sequence;
};