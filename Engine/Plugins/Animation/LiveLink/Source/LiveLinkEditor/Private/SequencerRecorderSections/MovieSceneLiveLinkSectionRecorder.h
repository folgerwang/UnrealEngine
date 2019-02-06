// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "GameFramework/Actor.h"
#include "MovieScene.h"
#include "IMovieSceneSectionRecorder.h"
#include "IMovieSceneSectionRecorderFactory.h"
#include "LiveLinkTypes.h"



class UMovieSceneLiveLinkSection;
class UMovieSceneLiveLinkTrack;
class UMotionControllerComponent;
class ULiveLinkComponent;

class FMovieSceneLiveLinkSectionRecorderFactory : public IMovieSceneSectionRecorderFactory
{
public:
	virtual ~FMovieSceneLiveLinkSectionRecorderFactory() {}

	virtual TSharedPtr<IMovieSceneSectionRecorder> CreateSectionRecorder(const struct FActorRecordingSettings& InActorRecordingSettings) const override;
	virtual bool CanRecordObject(class UObject* InObjectToRecord) const override;
};

class FMovieSceneLiveLinkSectionRecorder : public IMovieSceneSectionRecorder
{
public:
	virtual ~FMovieSceneLiveLinkSectionRecorder() {}

	virtual void CreateSection(UObject* InObjectToRecord, class UMovieScene* InMovieScene, const FGuid& Guid, float Time) override;
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
	/*With a motion controller we set one Subject Name*/
	void SetLiveLinkSubject(UMotionControllerComponent* MotionControllerComp);
	/*With Component we get all Subject Names */
	void SetLiveLinkSubjects(ULiveLinkComponent* MovieSceneLiveComp);
	/*Use SubjectNames from previous two functions to create the tracks and sections */
	void CreateTracks(UMovieScene* InMovieScene, const FGuid& Guid, float Time);

private:
	/** Object to record from */
	TLazyObjectPtr<UObject> ObjectToRecord;

	/** Names of Subjects To Record */
	TArray<FName> SubjectNames;

	/** Sections to record to, maps to SubjectNames */
	TArray<TWeakObjectPtr<class UMovieSceneLiveLinkSection>> MovieSceneSections;

	/** Frames to capture, we cache it to keep data*/
	TArray<TArray<FLiveLinkFrame>> CachedFramesArray;

	/** Movie scene we are recording to */
	TWeakObjectPtr<class UMovieScene> MovieScene;

	/** Identifier of the object we are recording */
	FGuid ObjectGuid;

	/** The timecode source at the beginning of recording */
	FMovieSceneTimecodeSource TimecodeSource;  //mz todo use

	/** Diff between Engine Time from when starting to record and Platform
	Time which is used by Live Link. */
	double SecondsDiff; 

	/** Guid for getting data from Live Link*/
	FGuid HandlerGuid;

	/** Needed for rewinding, when we set the values we keep track of the last value set to restart the re-winding from that. */
	TOptional<FVector> LastRotationValues;

};
