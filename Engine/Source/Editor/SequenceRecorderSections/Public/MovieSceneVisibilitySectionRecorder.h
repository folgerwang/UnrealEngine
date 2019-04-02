// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/LazyObjectPtr.h"
#include "IMovieSceneSectionRecorder.h"
#include "IMovieSceneSectionRecorderFactory.h"
#include "MovieSceneVisibilitySectionRecorderSettings.h"

class UMovieSceneBoolSection;

class FMovieSceneVisibilitySectionRecorderFactory : public IMovieSceneSectionRecorderFactory
{
public:
	virtual ~FMovieSceneVisibilitySectionRecorderFactory() {}

	virtual TSharedPtr<IMovieSceneSectionRecorder> CreateSectionRecorder(const struct FActorRecordingSettings& InActorRecordingSettings) const override;
	virtual bool CanRecordObject(class UObject* InObjectToRecord) const override;
	virtual UObject* CreateSettingsObject(class UObject* InOuter) const override { return  NewObject<UMovieSceneVisibilitySectionRecorderSettings>(InOuter, FName(TEXT("MovieSceneVisibilitySectionRecorder"))); }
};

class FMovieSceneVisibilitySectionRecorder : public IMovieSceneSectionRecorder
{
public:
	virtual ~FMovieSceneVisibilitySectionRecorder() {}

	virtual void CreateSection(UObject* InObjectToRecord, class UMovieScene* MovieScene, const FGuid& Guid, float Time) override;
	virtual void FinalizeSection(float CurrentTime) override;
	virtual void Record(float CurrentTime) override;
	virtual void InvalidateObjectToRecord() override
	{
		ObjectToRecord = nullptr;
	}
	virtual UObject* GetSourceObject() const override
	{
		return ObjectToRecord.Get();
	}

private:
	/** Object to record from */
	TLazyObjectPtr<class UObject> ObjectToRecord;

	/** Section to record to */
	TWeakObjectPtr<class UMovieSceneBoolSection> MovieSceneSection;

	/** Flag used to track visibility state and add keys when this changes */
	bool bWasVisible;
};
