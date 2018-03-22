// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "SequenceRecorder.h"
#include "SequenceRecorderActorGroup.generated.h"

UCLASS()
class SEQUENCERECORDER_API USequenceRecorderActorGroup : public UObject
{
	GENERATED_BODY()
public:
	USequenceRecorderActorGroup()
	{
		SequenceName = TEXT("RecordedSequence");
		SequenceRecordingBasePath.Path = TEXT("/Game/Cinematics/Sequences");
	}

	UPROPERTY(EditAnywhere, Category = "Recording Groups")
	FName GroupName;

	/** The base name of the sequence to record to. This name will also be used to auto-generate any assets created by this recording. */
	UPROPERTY(EditAnywhere, Category = "Recording Groups")
	FString SequenceName;

	/** Base path for this recording. Sub-assets will be created in subdirectories as specified */
	UPROPERTY(EditAnywhere, Category = "Recording Groups", meta = (ContentDir))
	FDirectoryPath SequenceRecordingBasePath;

	/** A list of actor recordings in this group which contains both the actors to record as well as settings for each one. */
	UPROPERTY(VisibleAnywhere, Category = "Recording Groups")
	TArray<UActorRecording*> RecordedActors;

	void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override
	{
		Super::PostEditChangeChainProperty(PropertyChangedEvent);

		if (PropertyChangedEvent.Property)
		{
			if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(USequenceRecorderActorGroup, SequenceName) ||
				PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(USequenceRecorderActorGroup, SequenceRecordingBasePath))
			{
				FSequenceRecorder::Get().RefreshNextSequence();
			}
		}
	}

};

/**
 * 
 */
UCLASS(hidedropdown, NotBlueprintable)
class SEQUENCERECORDER_API ASequenceRecorderGroup : public AActor
{
	GENERATED_BODY()
public:
	ASequenceRecorderGroup()
		: AActor()
	{
		bListedInSceneOutliner = false;
	}

	/** AActor Interface */
	virtual bool IsEditorOnly() const final
	{
		return true;
	}
	/** End AActor Interface */

	TWeakObjectPtr<USequenceRecorderActorGroup> FindActorGroup(const FName& Name)
	{
		for (USequenceRecorderActorGroup* Group : ActorGroups)
		{
			if (Group->GroupName == Name)
			{
				return Group;
			}
		}

		return nullptr;
	}

public:

	UPROPERTY()
	TArray<USequenceRecorderActorGroup*> ActorGroups;
};
