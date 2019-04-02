// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "Sound/SoundSubmix.h"
#include "AudioDeviceManager.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "UObject/UObjectIterator.h"
#include "AudioDevice.h"

#if WITH_EDITOR
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif

#if WITH_EDITOR
TSharedPtr<ISoundSubmixAudioEditor> USoundSubmix::SoundSubmixAudioEditor = nullptr;
#endif

USoundSubmix::USoundSubmix(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EnvelopeFollowerAttackTime = 10;
	EnvelopeFollowerReleaseTime = 500;
}

void USoundSubmix::StartRecordingOutput(const UObject* WorldContextObject, float ExpectedDuration)
{
	if (!GEngine)
	{
		return;
	}

	// Find device for this specific audio recording thing.
	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	FAudioDevice* DesiredAudioDevice = ThisWorld->GetAudioDevice();

	StartRecordingOutput(DesiredAudioDevice, ExpectedDuration);
}

void USoundSubmix::StartRecordingOutput(FAudioDevice* InDevice, float ExpectedDuration)
{
	if (InDevice)
	{
		InDevice->StartRecording(this, ExpectedDuration);
	}
}

void USoundSubmix::StopRecordingOutput(const UObject* WorldContextObject, EAudioRecordingExportType ExportType, const FString& Name, FString Path, USoundWave* ExistingSoundWaveToOverwrite)
{
	if (!GEngine)
	{
		return;
	}

	// Find device for this specific audio recording thing.
	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	FAudioDevice* DesiredAudioDevice = ThisWorld->GetAudioDevice();

	StopRecordingOutput(DesiredAudioDevice, ExportType, Name, Path, ExistingSoundWaveToOverwrite);
}

void USoundSubmix::StopRecordingOutput(FAudioDevice* InDevice, EAudioRecordingExportType ExportType, const FString& Name, FString Path, USoundWave* ExistingSoundWaveToOverwrite /*= nullptr*/)
{
	if (InDevice)
	{
		float SampleRate;
		float ChannelCount;

		Audio::AlignedFloatBuffer& RecordedBuffer = InDevice->StopRecording(this, ChannelCount, SampleRate);

		// This occurs when Stop Recording Output is called when Start Recording Output was not called.
		if (RecordedBuffer.Num() == 0)
		{
			return;
		}

		// Pack output data into DSPSampleBuffer and record it out!
		RecordingData.Reset(new Audio::FAudioRecordingData());

		RecordingData->InputBuffer = Audio::TSampleBuffer<int16>(RecordedBuffer, ChannelCount, SampleRate);

		switch (ExportType)
		{
			case EAudioRecordingExportType::SoundWave:
			{
				// If we're using the editor, we can write out a USoundWave to the content directory. Otherwise, we just generate a USoundWave without writing it to disk.
				if (GIsEditor)
				{
					RecordingData->Writer.BeginWriteToSoundWave(Name, RecordingData->InputBuffer, Path, [this](const USoundWave* Result)
					{
						if (OnSubmixRecordedFileDone.IsBound())
						{
							OnSubmixRecordedFileDone.Broadcast(Result);
						}
					});
				}
				else
				{
					RecordingData->Writer.BeginGeneratingSoundWaveFromBuffer(RecordingData->InputBuffer, nullptr, [this](const USoundWave* Result)
					{
						if (OnSubmixRecordedFileDone.IsBound())
						{
							OnSubmixRecordedFileDone.Broadcast(Result);
						}
					});
				}
			}
			break;
			
			case EAudioRecordingExportType::WavFile:
			{
				RecordingData->Writer.BeginWriteToWavFile(RecordingData->InputBuffer, Name, Path, [this]()
				{
					if (OnSubmixRecordedFileDone.IsBound())
					{
						OnSubmixRecordedFileDone.Broadcast(nullptr);
					}
				});
			}
			break;

			default:
			break;
		}
	}
}

void USoundSubmix::StartEnvelopeFollowing(const UObject* WorldContextObject)
{
	if (!GEngine)
	{
		return;
	}

	// Find device for this specific audio recording thing.
	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	FAudioDevice* AudioDevice = ThisWorld->GetAudioDevice();

	StartEnvelopeFollowing(AudioDevice);
}

void USoundSubmix::StartEnvelopeFollowing(FAudioDevice* InAudioDevice)
{
	if (InAudioDevice)
	{
		InAudioDevice->StartEnvelopeFollowing(this);
	}
}

void USoundSubmix::StopEnvelopeFollowing(const UObject* WorldContextObject)
{
	if (!GEngine)
	{
		return;
	}

	// Find device for this specific audio recording thing.
	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	FAudioDevice* AudioDevice = ThisWorld->GetAudioDevice();

	StopEnvelopeFollowing(AudioDevice);
}

void USoundSubmix::StopEnvelopeFollowing(FAudioDevice* InAudioDevice)
{
	if (InAudioDevice)
	{
		InAudioDevice->StopEnvelopeFollowing(this);
	}
}

void USoundSubmix::AddEnvelopeFollowerDelegate(const UObject* WorldContextObject, const FOnSubmixEnvelopeBP& OnSubmixEnvelopeBP)
{
	if (!GEngine)
	{
		return;
	}

	// Find device for this specific audio recording thing.
	UWorld* ThisWorld = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	FAudioDevice* AudioDevice = ThisWorld->GetAudioDevice();
	if (AudioDevice)
	{
		AudioDevice->AddEnvelopeFollowerDelegate(this, OnSubmixEnvelopeBP);
	}
}

FString USoundSubmix::GetDesc()
{
	return FString(TEXT("Sound submix"));
}

void USoundSubmix::BeginDestroy()
{
	Super::BeginDestroy();

	// Use the main/default audio device for storing and retrieving sound class properties
	FAudioDeviceManager* AudioDeviceManager = (GEngine ? GEngine->GetAudioDeviceManager() : nullptr);

	// Force the properties to be initialized for this SoundClass on all active audio devices
	if (AudioDeviceManager)
	{
		AudioDeviceManager->UnregisterSoundSubmix(this);
	}
}

void USoundSubmix::PostLoad()
{
	Super::PostLoad();

	// Use the main/default audio device for storing and retrieving sound class properties
	FAudioDeviceManager* AudioDeviceManager = (GEngine ? GEngine->GetAudioDeviceManager() : nullptr);

	// Force the properties to be initialized for this SoundClass on all active audio devices
	if (AudioDeviceManager)
	{
		AudioDeviceManager->RegisterSoundSubmix(this);
	}
}

#if WITH_EDITOR

TArray<USoundSubmix*> BackupChildSubmixes;

void USoundSubmix::PreEditChange(UProperty* PropertyAboutToChange)
{
	static FName NAME_ChildSubmixes(TEXT("ChildSubmixes"));

	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == NAME_ChildSubmixes)
	{
		// Take a copy of the current state of child classes
		BackupChildSubmixes = ChildSubmixes;
	}
}

void USoundSubmix::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != nullptr)
	{
		static const FName NAME_ChildSubmixes(TEXT("ChildSubmixes"));
		static const FName NAME_ParentSubmix(TEXT("ParentSubmix"));

		if (PropertyChangedEvent.Property->GetFName() == NAME_ChildSubmixes)
		{
			// Find child that was changed/added
			for (int32 ChildIndex = 0; ChildIndex < ChildSubmixes.Num(); ChildIndex++)
			{
				if (ChildSubmixes[ChildIndex] != nullptr && !BackupChildSubmixes.Contains(ChildSubmixes[ChildIndex]))
				{
					if (ChildSubmixes[ChildIndex]->RecurseCheckChild(this))
					{
						// Contains cycle so revert to old layout - launch notification to inform user
						FNotificationInfo Info(NSLOCTEXT("Engine", "UnableToChangeSoundSubmixChildDueToInfiniteLoopNotification", "Could not change SoundSubmix child as it would create a loop"));
						Info.ExpireDuration = 5.0f;
						Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Error"));
						FSlateNotificationManager::Get().AddNotification(Info);

						// Revert to the child submixes
						ChildSubmixes = BackupChildSubmixes;
					}
					else
					{
						// Update parentage
						ChildSubmixes[ChildIndex]->SetParentSubmix(this);
					}
					break;
				}
			}

			// Update old child's parent if it has been removed
			for (int32 ChildIndex = 0; ChildIndex < BackupChildSubmixes.Num(); ChildIndex++)
			{
				if (BackupChildSubmixes[ChildIndex] != nullptr && !ChildSubmixes.Contains(BackupChildSubmixes[ChildIndex]))
				{
					BackupChildSubmixes[ChildIndex]->Modify();
					BackupChildSubmixes[ChildIndex]->ParentSubmix = nullptr;
				}
			}

			RefreshAllGraphs(false);
		}
		else if (PropertyChangedEvent.Property->GetFName() == NAME_ParentSubmix)
		{
			// Add this sound class to the parent class if it's not already added
			if (ParentSubmix)
			{
				bool bIsChildSubmix = false;
				for (int32 i = 0; i < ParentSubmix->ChildSubmixes.Num(); ++i)
				{
					USoundSubmix* ChildSubmix = ParentSubmix->ChildSubmixes[i];
					if (ChildSubmix && ChildSubmix == this)
					{
						bIsChildSubmix = true;
						break;
					}
				}

				if (!bIsChildSubmix)
				{
					ParentSubmix->Modify();
					ParentSubmix->ChildSubmixes.Add(this);
				}
			}

			Modify();
			RefreshAllGraphs(false);
		}
	}

	// Use the main/default audio device for storing and retrieving sound class properties
	FAudioDeviceManager* AudioDeviceManager = (GEngine ? GEngine->GetAudioDeviceManager() : nullptr);

	// Force the properties to be initialized for this SoundSubmix on all active audio devices
	if (AudioDeviceManager)
	{
		AudioDeviceManager->RegisterSoundSubmix(this);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool USoundSubmix::RecurseCheckChild(USoundSubmix* ChildSoundSubmix)
{
	for (int32 Index = 0; Index < ChildSubmixes.Num(); Index++)
	{
		if (ChildSubmixes[Index])
		{
			if (ChildSubmixes[Index] == ChildSoundSubmix)
			{
				return true;
			}

			if (ChildSubmixes[Index]->RecurseCheckChild(ChildSoundSubmix))
			{
				return true;
			}
		}
	}

	return false;
}

void USoundSubmix::SetParentSubmix(USoundSubmix* InParentSubmix)
{
	if (ParentSubmix != InParentSubmix)
	{
		if (ParentSubmix != nullptr)
		{
			ParentSubmix->Modify();
			ParentSubmix->ChildSubmixes.Remove(this);
		}

		Modify();
		ParentSubmix = InParentSubmix;
	}
}

void USoundSubmix::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	USoundSubmix* This = CastChecked<USoundSubmix>(InThis);

	Collector.AddReferencedObject(This->SoundSubmixGraph, This);

	Super::AddReferencedObjects(InThis, Collector);
}

void USoundSubmix::RefreshAllGraphs(bool bIgnoreThis)
{
	if (SoundSubmixAudioEditor.IsValid())
	{
		// Update the graph representation of every SoundClass
		for (TObjectIterator<USoundSubmix> It; It; ++It)
		{
			USoundSubmix* SoundSubmix = *It;
			if (!bIgnoreThis || SoundSubmix != this)
			{
				if (SoundSubmix->SoundSubmixGraph)
				{
					SoundSubmixAudioEditor->RefreshGraphLinks(SoundSubmix->SoundSubmixGraph);
				}
			}
		}
	}
}

void USoundSubmix::SetSoundSubmixAudioEditor(TSharedPtr<ISoundSubmixAudioEditor> InSoundSubmixAudioEditor)
{
	check(!SoundSubmixAudioEditor.IsValid());
	SoundSubmixAudioEditor = InSoundSubmixAudioEditor;
}

TSharedPtr<ISoundSubmixAudioEditor> USoundSubmix::GetSoundSubmixAudioEditor()
{
	return SoundSubmixAudioEditor;
}

#endif
