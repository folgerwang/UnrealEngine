// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TakeRecorderSource.h"
#include "TakeRecorderSources.h"
#include "Templates/SubclassOf.h"
#include "TakeRecorderLevelVisibilitySource.generated.h"

/** A recording source that records level visibilitiy */
UCLASS(Abstract, config = EditorSettings, DisplayName = "Level Visibility Recorder Defaults")
class UTakeRecorderLevelVisibilitySourceSettings : public UTakeRecorderSource
{
public:
	GENERATED_BODY()

	UTakeRecorderLevelVisibilitySourceSettings(const FObjectInitializer& ObjInit);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// UTakeRecorderSource Interface
	virtual FString GetSubsceneName(ULevelSequence* InSequence) const override;
	// ~UTakeRecorderSource Interface

	/** Name of the recorded level visibility track name */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Source")
	FText LevelVisibilityTrackName;
};

/** A recording source that records level visibility state */
UCLASS(Category="Other", config = EditorSettings, meta = (TakeRecorderDisplayName = "Level Visibility"))
class UTakeRecorderLevelVisibilitySource : public UTakeRecorderLevelVisibilitySourceSettings
{
public:
	GENERATED_BODY()

	UTakeRecorderLevelVisibilitySource(const FObjectInitializer& ObjInit);

private:

	// UTakeRecorderSource
	virtual TArray<UTakeRecorderSource*> PreRecording(class ULevelSequence* InSequence, class ULevelSequence* InMasterSequence, FManifestSerializer* InManifestSerializer) override;
	virtual void TickRecording(const FQualifiedFrameTime& CurrentTime) override;
	virtual void AddContentsToFolder(class UMovieSceneFolder* InFolder) override;
	virtual FText GetDisplayTextImpl() const override;
	virtual bool CanAddSource(UTakeRecorderSources* InSources) const override;

private:
	TWeakObjectPtr<class UMovieSceneLevelVisibilityTrack> CachedLevelVisibilityTrack;
};
