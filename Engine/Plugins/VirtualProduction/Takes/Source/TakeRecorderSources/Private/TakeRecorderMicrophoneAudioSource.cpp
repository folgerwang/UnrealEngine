// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TakeRecorderMicrophoneAudioSource.h"
#include "TakeRecorderSources.h"
#include "TakeRecorderSettings.h"
#include "TakesUtils.h"
#include "TakeMetaData.h"
#include "Recorder/TakeRecorderParameters.h"
#include "Styling/SlateIconFinder.h"
#include "LevelSequence.h"
#include "Editor.h"
#include "Modules/ModuleManager.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Sound/SoundWave.h"
#include "ISequenceAudioRecorder.h"
#include "ISequenceRecorder.h"
#include "MovieSceneFolder.h"

#include "Misc/PackageName.h"
#include "AssetData.h"
#include "AssetRegistryModule.h"

UTakeRecorderMicrophoneAudioSourceSettings::UTakeRecorderMicrophoneAudioSourceSettings(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
	, AudioTrackName(NSLOCTEXT("UTakeRecorderMicrophoneAudioSource", "DefaultAudioTrackName", "Recorded Audio"))
	, AudioSubDirectory(TEXT("Audio"))
{
	TrackTint = FColor(75, 67, 148);
}

void UTakeRecorderMicrophoneAudioSourceSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		SaveConfig();
	}
}

FString UTakeRecorderMicrophoneAudioSourceSettings::GetSubsceneName(ULevelSequence* InSequence) const
{
	if (UTakeMetaData* TakeMetaData = InSequence->FindMetaData<UTakeMetaData>())
	{
		return TakeMetaData->GetSlate() + TEXT("Audio");
	}
	return TEXT("MicrophoneAudio");
}

UTakeRecorderMicrophoneAudioSource::UTakeRecorderMicrophoneAudioSource(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
	, AudioGain(0.0f)
	, bSplitAudioChannelsIntoSeparateTracks(false)
	, bReplaceRecordedAudio(true)
{
}


static FString MakeNewAssetName(const FString& BaseAssetPath, const FString& BaseAssetName)
{
	const FString Dot(TEXT("."));
	FString AssetPath = BaseAssetPath;
	FString AssetName = BaseAssetName;

	AssetPath /= AssetName;
	AssetPath += Dot + AssetName;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*AssetPath);

	// if object with same name exists, try a different name until we don't find one
	int32 ExtensionIndex = 0;
	while (AssetData.IsValid())
	{
		AssetName = FString::Printf(TEXT("%s_%d"), *BaseAssetName, ExtensionIndex);
		AssetPath = (BaseAssetPath / AssetName) + Dot + AssetName;
		AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*AssetPath);

		ExtensionIndex++;
	}

	return AssetName;
}

TArray<UTakeRecorderSource*> UTakeRecorderMicrophoneAudioSource::PreRecording(ULevelSequence* InSequence, ULevelSequence* InMasterSequence, FManifestSerializer* InManifestSerializer)
{
	UMovieScene* MovieScene = InSequence->GetMovieScene();
	for (auto MasterTrack : MovieScene->GetMasterTracks())
	{
		if (MasterTrack->IsA(UMovieSceneAudioTrack::StaticClass()) && MasterTrack->GetDisplayName().EqualTo(AudioTrackName))
		{
			CachedAudioTrack = Cast<UMovieSceneAudioTrack>(MasterTrack);
		}
	}

	if (!CachedAudioTrack.IsValid())
	{
		CachedAudioTrack = MovieScene->AddMasterTrack<UMovieSceneAudioTrack>();
		CachedAudioTrack->SetDisplayName(AudioTrackName);
	}

	return TArray<UTakeRecorderSource*>();
}

void UTakeRecorderMicrophoneAudioSource::AddContentsToFolder(UMovieSceneFolder* InFolder)
{
	if (CachedAudioTrack.IsValid())
	{
		InFolder->AddChildMasterTrack(CachedAudioTrack.Get());
	}
}


void UTakeRecorderMicrophoneAudioSource::StartRecording(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame, class ULevelSequence* InSequence)
{
	Super::StartRecording(InSectionStartTimecode, InSectionFirstFrame, InSequence);

	FString PathToRecordTo = FPackageName::GetLongPackagePath(InSequence->GetOutermost()->GetPathName());
	FString BaseName = InSequence->GetName();

	FDirectoryPath AudioDirectory;
	AudioDirectory.Path = PathToRecordTo;
	if (AudioSubDirectory.Len())
	{
		AudioDirectory.Path /= AudioSubDirectory;
	}

	FString AssetName = MakeNewAssetName(AudioDirectory.Path, BaseName);

	ISequenceRecorder& Recorder = FModuleManager::Get().LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");

	FSequenceAudioRecorderSettings AudioSettings;
	AudioSettings.Directory = AudioDirectory;
	AudioSettings.AssetName = AssetName;
	AudioSettings.GainDb = AudioGain;
	AudioSettings.bSplitChannels = bSplitAudioChannelsIntoSeparateTracks;

	AudioRecorder = Recorder.CreateAudioRecorder();
	if (AudioRecorder)
	{
		AudioRecorder->Start(AudioSettings);
	}
}

void UTakeRecorderMicrophoneAudioSource::StopRecording(class ULevelSequence* InSequence)
{
	Super::StopRecording(InSequence);

	TArray<USoundWave*> RecordedSoundWaves;
	if (AudioRecorder)
	{
		AudioRecorder->Stop(RecordedSoundWaves);
	}
	AudioRecorder.Reset();

	if (!RecordedSoundWaves.Num())
	{
		return;
	}

	for (auto RecordedSoundWave : RecordedSoundWaves)
	{
		FAssetRegistryModule::AssetCreated(RecordedSoundWave);
	}

	UMovieScene* MovieScene = InSequence->GetMovieScene();
	check(CachedAudioTrack.IsValid());

	if (bReplaceRecordedAudio)
	{
		CachedAudioTrack->RemoveAllAnimationData();
	}

	FTakeRecorderParameters Parameters;
	Parameters.User = GetDefault<UTakeRecorderUserSettings>()->Settings;
	Parameters.Project = GetDefault<UTakeRecorderProjectSettings>()->Settings;

	for (USoundWave* RecordedAudio : RecordedSoundWaves)
	{
		int32 RowIndex = -1;
		for (UMovieSceneSection* Section : CachedAudioTrack->GetAllSections())
		{
			RowIndex = FMath::Max(RowIndex, Section->GetRowIndex());
		}

		UMovieSceneAudioSection* NewAudioSection = NewObject<UMovieSceneAudioSection>(CachedAudioTrack.Get(), UMovieSceneAudioSection::StaticClass());

		FFrameRate TickResolution = CachedAudioTrack->GetTypedOuter<UMovieScene>()->GetTickResolution();

		NewAudioSection->SetRowIndex(RowIndex + 1);
		NewAudioSection->SetSound(RecordedAudio);
		NewAudioSection->SetRange(TRange<FFrameNumber>(FFrameNumber(0), (RecordedAudio->GetDuration() * TickResolution).CeilToFrame()));

		CachedAudioTrack->AddSection(*NewAudioSection);

		if (Parameters.User.bSaveRecordedAssets || GEditor == nullptr)
		{
			TakesUtils::SaveAsset(RecordedAudio);
		}
	}

	// Reset our audio track pointer
	CachedAudioTrack = nullptr;
}

FText UTakeRecorderMicrophoneAudioSource::GetDisplayTextImpl() const
{
	return NSLOCTEXT("UTakeRecorderMicrophoneAudioSource", "Label", "Microphone Audio");
}

bool UTakeRecorderMicrophoneAudioSource::CanAddSource(UTakeRecorderSources* InSources) const
{
	for (UTakeRecorderSource* Source : InSources->GetSources())
	{
		if (Source->IsA<UTakeRecorderMicrophoneAudioSource>())
		{
			return false;
		}
	}
	return true;
}
