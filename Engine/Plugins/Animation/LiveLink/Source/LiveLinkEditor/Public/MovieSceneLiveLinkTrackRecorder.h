// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TrackRecorders/MovieSceneTrackRecorder.h"
#include "TrackRecorders/IMovieSceneTrackRecorderFactory.h"
#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "GameFramework/Actor.h"
#include "MovieScene.h"
#include "LiveLinkTypes.h"
#include "Serializers/MovieSceneLiveLinkSerialization.h"
#include "MovieSceneLiveLinkTrackRecorder.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LiveLinkSerialization, Verbose, All);

class UMovieSceneLiveLinkTrack;
class UMovieSceneLiveLinkSection;
class UMotionControllerComponent;
class ULiveLinkComponent;
class ULiveLinkSubjectProperties;


UCLASS(BlueprintType)
class LIVELINKEDITOR_API UMovieSceneLiveLinkTrackRecorder : public UMovieSceneTrackRecorder
{
	GENERATED_BODY()
public:
	virtual ~UMovieSceneLiveLinkTrackRecorder() {}

	// UMovieSceneTrackRecorder Interface
	virtual void RecordSampleImpl(const FQualifiedFrameTime& CurrentTime) override;
	virtual void FinalizeTrackImpl() override;
	virtual void SetSectionStartTimecodeImpl(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame) override;
	virtual void StopRecordingImpl() override;
	virtual void SetSavedRecordingDirectory(const FString& InDirectory)
	{
		Directory = InDirectory;
	}
	virtual bool LoadRecordedFile(const FString& InFileName, UMovieScene *InMovieScene, TMap<FGuid, AActor*>& ActorGuidToActorMap, TFunction<void()> InCompletionCallback) override;


public:
	//we don't call UMovieSceneTrackRecorder::CreateTrack or CreateTrackImpl since that expects an  ObjectToRecord and a GUID which isn't needed.
	void CreateTrack(UMovieScene* InMovieScene, const FName& InSubjectName, UMovieSceneTrackRecorderSettings* InSettingsObject);
	void AddContentsToFolder(UMovieSceneFolder* InFolder);
	void SetReduceKeys(bool bInReduce) { bReduceKeys = bInReduce; }

private:

	UMovieSceneLiveLinkTrack* DoesLiveLinkMasterTrackExist(const FName& MasterTrackName);

	void CreateTracks();

	bool LoadManifestFile(const FString& FileName, UMovieScene *InMovieScene, TFunction<void()> InCompletionCallback);
	bool LoadSubjectFile(const FString& FileName, UMovieScene *InMovieScene, TFunction<void()> InCompletionCallback);
private:

	/** Names of Subjects To Record */
	FName SubjectName;

	/** Cached Arrays of Frames we get from Live Link*/
	TArray<FLiveLinkFrame> CachedFramesArray;

	/** Cached LiveLink Tracks, section per each maps to SubjectNames */
	TWeakObjectPtr<class UMovieSceneLiveLinkTrack> LiveLinkTrack;

	/** Sections to record to on each track*/
	TWeakObjectPtr<class UMovieSceneLiveLinkSection> MovieSceneSection;
	
	/** LiveLink Serializers per track */
	FLiveLinkSerializer LiveLinkSerializer;

	/** Master serializer to point at the individual files for each subject */
	FLiveLinkManifestSerializer Serializer;

	/** Diff between Engine Time from when starting to record and Platform
	Time which is used by Live Link. Still used if no TimeCode present.*/
	double SecondsDiff; 

	/** Guid when registered to get LiveLinkData */
	FGuid HandlerGuid;

	/**Cached directory for serializers to save to*/
	FString Directory;

	/** Cached Key Reduction from Live Link Source Propreties*/
	bool bReduceKeys;

	/** Needed for rewinding, when we set the values we keep track of the last value set to restart the re-winding from that. */
	TOptional<FVector> LastRotationValues;

};
