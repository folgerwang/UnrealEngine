// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TakeRecorderSource.h"
#include "UObject/SoftObjectPtr.h"
#include "TakeRecorderLevelSequenceSource.generated.h"

class UTexture;
class ALevelSequenceActor;
class ULevelSequence;

/** Plays level sequence actors when recording starts */
UCLASS(Category="Other", meta = (TakeRecorderDisplayName = "Level Sequence"))
class UTakeRecorderLevelSequenceSource : public UTakeRecorderSource
{
public:
	GENERATED_BODY()

	UTakeRecorderLevelSequenceSource(const FObjectInitializer& ObjInit);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source")
	TArray<ULevelSequence*> LevelSequencesToTrigger;

private:

	// UTakeRecorderSource
	virtual TArray<UTakeRecorderSource*> PreRecording(class ULevelSequence* InSequence, class ULevelSequence* InMasterSequence, FManifestSerializer* InManifestSerializer) override;
	virtual void StartRecording(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame, class ULevelSequence* InSequence) override;
	virtual void StopRecording(class ULevelSequence* InSequence) override;
	virtual FText GetDisplayTextImpl() const override;
	virtual FText GetDescriptionTextImpl() const override;

	/** Transient level sequence actors to trigger, to be stopped and reset at the end of recording */
	TArray<TWeakObjectPtr<ALevelSequenceActor>> ActorsToTrigger;
};
