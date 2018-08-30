// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Engine/BlueprintGeneratedClass.h"

#include "LevelSequenceDirectorGeneratedClass.generated.h"

class ULevelSequencePlayer;

UCLASS()
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
class LEVELSEQUENCE_API ULevelSequenceDirectorGeneratedClass : public UBlueprintGeneratedClass
{
public:
	GENERATED_BODY()
};
