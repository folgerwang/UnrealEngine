// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "LevelSequenceDirector.generated.h"

class ULevelSequencePlayer;

UCLASS(Blueprintable)
class LEVELSEQUENCE_API ULevelSequenceDirector : public UObject
{
public:
	GENERATED_BODY()

	/** Called when this director is created */
	UFUNCTION(BlueprintImplementableEvent, Category="Sequencer")
	void OnCreated();

	virtual UWorld* GetWorld() const override;

	/** Pointer to the player that's playing back this director's sequence */
	UPROPERTY(BlueprintReadOnly, Category="Cinematics")
	ULevelSequencePlayer* Player;
};


UCLASS()
class ULegacyLevelSequenceDirectorBlueprint : public UBlueprint
{
	GENERATED_BODY()

	ULegacyLevelSequenceDirectorBlueprint(const FObjectInitializer& ObjInit)
		: Super(ObjInit)
	{
		ParentClass = ULevelSequenceDirector::StaticClass();
	}
};