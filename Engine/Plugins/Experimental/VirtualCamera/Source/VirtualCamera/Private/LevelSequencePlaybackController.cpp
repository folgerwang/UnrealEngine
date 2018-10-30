// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LevelSequencePlaybackController.h"
#include "AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "TimerManager.h"
#include "UObject/UObjectBase.h"
#include "UObject/UObjectIterator.h"

#define TAKE_MINIMUM_DIGITS	3

ULevelSequencePlaybackController::ULevelSequencePlaybackController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsRecording = false;
	TargetCamera = nullptr;
	CameraToFollow = nullptr;
	AssetRegistry = nullptr;
	bIsReversed = false;	
	NextTakeNumber = 0;
	Sequence = nullptr;
	CachedSequenceName = "";

#if WITH_EDITOR
	RecorderSettings = GetMutableDefault<USequenceRecorderSettings>();
	OnFinished.AddDynamic(this, &ULevelSequencePlaybackController::StopRecording);
#endif //WITH_EDITOR
}

void ULevelSequencePlaybackController::StartRecording()
{
#if WITH_EDITOR
	if (!Recorder)
	{
		return;
	}

	// If there's a level sequence to be played, associate the recorded sequence with it; otherwise, use defaults for Sequence Recorder
	if (Sequence)
	{
		// Reset player to start and begin playing
		Pause();
		JumpToFrame(StartTime);

		// Find the camera that is bound in the sequence, if any
		// ToDo: Need to find a better way to do this for shots with multiple camera cut sequences or master sequences
		for (TObjectIterator<ACineCameraActor> Itr; Itr; ++Itr)
		{
			ACineCameraActor& CineCameraActor = **Itr;

			if (Sequence->FindPossessableObjectId(CineCameraActor, GetWorld()).IsValid())
			{
				TargetCamera = &CineCameraActor;
				break;
			}
		}

		// Start the sequence after the countdown of 5 seconds
		FTimerHandle SequenceStart;
		GetWorld()->GetTimerManager().SetTimer(SequenceStart, this, &ULevelSequencePlaybackController::PlayToEnd, RecorderSettings->RecordingDelay, false);
	}

	SetupTargetCamera();

	// Pass in an empty array to avoid crushing the existing actors in sequence recorder
	bIsRecording = Recorder->StartRecording(TArray<AActor*>());
#endif //WITH_EDITOR
}

void ULevelSequencePlaybackController::StopRecording()
{
#if WITH_EDITOR
	if (Recorder && bIsRecording)
	{
		Recorder->StopRecording();
		UpdateNextTakeNumber();
		bIsRecording = false;
	}
#endif //WITH_EDITOR
}

void ULevelSequencePlaybackController::ResumeLevelSequencePlay()
{
	if (Sequence)
	{
		PlayLooping();
	}
}

void ULevelSequencePlaybackController::GetLevelSequences(TArray<FLevelSequenceData>& OutLevelSequenceNames)
{
	OutLevelSequenceNames.Empty();

	TArray<FAssetData> LevelSequences;

	IFileManager& FileManager = IFileManager::Get();
	LevelSequences.Empty();

	if (!AssetRegistry || !AssetRegistry->GetAssetsByClass("LevelSequence", LevelSequences, false))
	{
		return;
	}

	for (FAssetData LevelSequence : LevelSequences)
	{
		// Get the file system name of the package so we can get other data on it (i.e. timestamp)
		FString LevelSequenceFile = FPackageName::LongPackageNameToFilename(
			LevelSequence.PackageName.ToString(), 
			FPackageName::GetAssetPackageExtension()
		);

		FLevelSequenceData NewSequenceData = FLevelSequenceData(
			LevelSequence.ObjectPath.ToString(),
			LevelSequence.AssetName.ToString(),
			FileManager.GetTimeStamp(*LevelSequenceFile)
		);

		OutLevelSequenceNames.Add(NewSequenceData);
	}

	// Sort the level sequence names alphabetically by Display Name
	OutLevelSequenceNames.Sort([](const FLevelSequenceData& LeftItem, const FLevelSequenceData& RightItem)
	{
		return LeftItem.DisplayName < RightItem.DisplayName;
	});
}

FString ULevelSequencePlaybackController::GetActiveLevelSequenceName() const 
{
	if (Sequence)
	{
		return Sequence->GetName();
	}

	return FString();
}

bool ULevelSequencePlaybackController::SetActiveLevelSequence(const FString& LevelSequencePath)
{
	if (!AssetRegistry)
	{
		return false;
	}

	FAssetData Asset = AssetRegistry->GetAssetByObjectPath(FName(*LevelSequencePath));
	ULevelSequence* NewSequence = Cast<ULevelSequence>(Asset.GetAsset());
	
	if (!NewSequence)
	{
		UE_LOG(LogActor, Warning, TEXT("VirtualCamera: Level Sequence could not be loaded"))
		return false;
	}
	
	if (NewSequence->GetMovieScene()->GetCameraCutTrack())
	{
		PlaybackSettings.bDisableLookAtInput = true;
		OnRecordEnabledStateChanged.ExecuteIfBound(false);
	}
	else
	{
		PlaybackSettings.bDisableMovementInput = false;
		OnRecordEnabledStateChanged.ExecuteIfBound(true);
	}

	Initialize(NewSequence, GetWorld(), PlaybackSettings);
	return true;
}

void ULevelSequencePlaybackController::ClearActiveLevelSequence()
{
	if (Sequence)
	{
		Stop();
		Sequence = nullptr;
	}
}

void ULevelSequencePlaybackController::PilotTargetedCamera(FCameraFilmbackSettings* FilmbackSettingsOverride)
{
	if (!TargetCamera)
	{
		return;
	}

	TargetCamera->SetActorLocationAndRotation(CameraToFollow->GetComponentLocation(), CameraToFollow->GetComponentRotation().Quaternion());
	UCineCameraComponent* TargetComponent = TargetCamera->GetCineCameraComponent();

	TargetComponent->FocusSettings = CameraToFollow->FocusSettings;
	TargetComponent->CurrentFocalLength = CameraToFollow->CurrentFocalLength;
	TargetComponent->LensSettings = CameraToFollow->LensSettings;
	TargetComponent->FilmbackSettings = FilmbackSettingsOverride ? *FilmbackSettingsOverride : CameraToFollow->FilmbackSettings;
}

void ULevelSequencePlaybackController::PlayFromBeginning()
{
	if (Sequence)
	{
		JumpToFrame(StartTime);
		PlayLooping();
	}
}

void ULevelSequencePlaybackController::SetupSequenceRecorderSettings(const TArray<FName>& RequiredSettings)
{
#if WITH_EDITOR
	Recorder = FModuleManager::Get().GetModulePtr<ISequenceRecorder>("SequenceRecorder");
	if (!Recorder)
	{
		UE_LOG(LogActor, Error, TEXT("VirtualCamera - No Sequence Recorder module found!"))
	}

	FAssetRegistryModule* AssetRegistryModule = FModuleManager::Get().GetModulePtr<FAssetRegistryModule>("AssetRegistry");
	if (AssetRegistryModule)
	{
		AssetRegistry = &AssetRegistryModule->Get();
	}
	
	if (!AssetRegistry)
	{
		UE_LOG(LogActor, Error, TEXT("VirtualCamera - No Asset Registry module found!"))
	}

	UpdateNextTakeNumber();

	// If the sequencer settings are still engine default, update them to add important camera settings
	FPropertiesToRecordForClass* CineCameraSettings = RecorderSettings->ClassesAndPropertiesToRecord.FindByPredicate([](const FPropertiesToRecordForClass& InItem)
	{
		return InItem.Class == UCineCameraComponent::StaticClass();
	});

	if (!CineCameraSettings)
	{
		int32 Index = RecorderSettings->ClassesAndPropertiesToRecord.Add(FPropertiesToRecordForClass(UCineCameraComponent::StaticClass()));
		CineCameraSettings = &RecorderSettings->ClassesAndPropertiesToRecord[Index];
	} 

	for (FName RequiredSetting : RequiredSettings)
	{
		CineCameraSettings->Properties.AddUnique(RequiredSetting);
	}
#endif //WITH_EDITOR
}

float ULevelSequencePlaybackController::GetCurrentRecordingFrameRate() const
{
#if WITH_EDITOR
	if (RecorderSettings)
	{
		return RecorderSettings->DefaultAnimationSettings.SampleRate;
	}
#endif //WITH_EDITOR

	return 0.0f;
}

float ULevelSequencePlaybackController::GetCurrentRecordingLength() const
{
#if WITH_EDITOR
	if (Recorder)
	{
		return Recorder->GetCurrentRecordingLength().AsSeconds();
	}
#endif //WITH_EDITOR

	return 0.0f;
}

FString ULevelSequencePlaybackController::GetCurrentRecordingSceneName()
{
#if WITH_EDITOR
	if (Recorder)
	{
		// Check if we need to update the take number
		FString ReturnString = Recorder->GetSequenceRecordingName();
		if (ReturnString != CachedSequenceName)
		{
			CachedSequenceName = ReturnString;
			SetupTargetCamera();
		}

		return ReturnString;
	}
#endif //WITH_EDITOR

	return FString();
}

FString ULevelSequencePlaybackController::GetCurrentRecordingTakeName() const
{
	// If the take number is 0, don't give a number back
	if (NextTakeNumber == 0)
	{
		return FString();
	}

	FNumberFormattingOptions LeadingZeroesFormatter = FNumberFormattingOptions();
	LeadingZeroesFormatter.MinimumIntegralDigits = TAKE_MINIMUM_DIGITS;
	return FText::AsNumber(NextTakeNumber, &LeadingZeroesFormatter).ToString();
}

void ULevelSequencePlaybackController::OnObjectSpawned(UObject * InObject, const FMovieSceneEvaluationOperand & Operand)
{
	Super::OnObjectSpawned(InObject, Operand);
	
	// Camera actors spawn with lock to hmd set to true by default. Unlock them here to prevent unwanted movement.
	ACameraActor* CameraActor = Cast<ACameraActor>(InObject);
	if (CameraActor && CameraActor->GetCameraComponent())
	{
		CameraActor->GetCameraComponent()->bLockToHmd = false;
	}
}

void ULevelSequencePlaybackController::PlayToEnd()
{
	if (Sequence)
	{
		PlayLooping(0);
	}
}

void ULevelSequencePlaybackController::UpdateNextTakeNumber()
{	
#if	WITH_EDITOR
	if (Recorder)
	{
		NextTakeNumber = Recorder->GetTakeNumberForActor(TargetCamera); 
	}
#endif //WITH_EDITOR
}

void ULevelSequencePlaybackController::SetupTargetCamera()
{
#if WITH_EDITOR
	if (!TargetCamera)
	{
		TargetCamera = GetWorld()->SpawnActor<ACineCameraActor>();

		// If spawn failed, exit early
		if (!TargetCamera)
		{
			return;
		}
	}
	Recorder->QueueActorToRecord(TargetCamera);
	UpdateNextTakeNumber();
#endif //WITH_EDITOR
}
