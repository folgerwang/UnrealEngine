// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TakeRecorderLiveLinkSource.h"
#include "MovieSceneLiveLinkTrackRecorder.h"
#include "LevelSequence.h"
#include "Editor.h"
#include "Modules/ModuleManager.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "SequenceRecorderUtils.h"
#include "Sound/SoundWave.h"
#include "MovieSceneFolder.h"
#include "MovieScene/MovieSceneLiveLinkTrack.h"
#include "Misc/PackageName.h"
#include "AssetData.h"
#include "AssetRegistryModule.h"
#include "TakeMetaData.h"
#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"

UTakeRecorderLiveLinkSource::UTakeRecorderLiveLinkSource(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	TrackTint = FColor(74, 108, 164);
}

TArray<UTakeRecorderSource*> UTakeRecorderLiveLinkSource::PreRecording(class ULevelSequence* InSequence, class ULevelSequence* InMasterSequence, FManifestSerializer* InManifestSerializer) 
{
	UMovieScene* MovieScene = InSequence->GetMovieScene();
	TrackRecorder = NewObject<UMovieSceneLiveLinkTrackRecorder>();
	TrackRecorder->CreateTrack(MovieScene, SubjectName, nullptr);

	return TArray<UTakeRecorderSource*>();
}

void UTakeRecorderLiveLinkSource::StartRecording(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame, class ULevelSequence* InSequence)
{
	if (TrackRecorder)
	{
		TrackRecorder->SetReduceKeys(bReduceKeys);
		TrackRecorder->SetSectionStartTimecode(InSectionStartTimecode, InSectionFirstFrame);
	}
}

void UTakeRecorderLiveLinkSource::TickRecording(const FQualifiedFrameTime& CurrentSequenceTime)
{
	if(TrackRecorder)
	{
		TrackRecorder->RecordSample(CurrentSequenceTime);
	}
}

void UTakeRecorderLiveLinkSource::StopRecording(class ULevelSequence* InSequence)
{
	if (TrackRecorder)
	{
		TrackRecorder->StopRecording();
	}
}

TArray<UTakeRecorderSource*> UTakeRecorderLiveLinkSource::PostRecording(class ULevelSequence* InSequence, class ULevelSequence* InMasterSequence)
{
	if (TrackRecorder)
	{
		TrackRecorder->FinalizeTrack();
	}
	
	TrackRecorder = nullptr;
	return TArray<UTakeRecorderSource*>();
}

FText UTakeRecorderLiveLinkSource::GetDisplayTextImpl() const
{
	return FText::FromName(SubjectName);
}

void UTakeRecorderLiveLinkSource::AddContentsToFolder(UMovieSceneFolder* InFolder)
{
	TrackRecorder->AddContentsToFolder(InFolder);
}

bool UTakeRecorderLiveLinkSource::CanAddSource(UTakeRecorderSources* InSources) const
{
	for (UTakeRecorderSource* Source : InSources->GetSources())
	{
		if (Source->IsA<UTakeRecorderLiveLinkSource>() )
		{
			if (UTakeRecorderLiveLinkSource* OtherSource = Cast<UTakeRecorderLiveLinkSource>(Source) )
			{
				if (OtherSource->SubjectName == SubjectName)
				{
					return false;
				}
			}
		}
	}
	return true;
}

FString UTakeRecorderLiveLinkSource::GetSubsceneName(ULevelSequence* InSequence) const
{
	if (UTakeMetaData* TakeMetaData = InSequence->FindMetaData<UTakeMetaData>())
	{
		return TakeMetaData->GetSlate() + SubjectName.ToString();
	}
	else if (SubjectName != NAME_None)
	{
		return SubjectName.ToString();
	}

	return TEXT("LiveLink");
}
