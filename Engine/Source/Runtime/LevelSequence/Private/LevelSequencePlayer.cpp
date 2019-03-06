// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LevelSequencePlayer.h"
#include "GameFramework/Actor.h"
#include "MovieScene.h"
#include "Misc/CoreDelegates.h"
#include "EngineGlobals.h"
#include "Camera/PlayerCameraManager.h"
#include "UObject/Package.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Tickable.h"
#include "Engine/LevelScriptActor.h"
#include "MovieSceneCommonHelpers.h"
#include "Sections/MovieSceneSubSection.h"
#include "LevelSequenceSpawnRegister.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LocalPlayer.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "LevelSequenceActor.h"
#include "Modules/ModuleManager.h"
#include "LevelUtils.h"
#include "Core/Public/ProfilingDebugging/CsvProfiler.h"

/* ULevelSequencePlayer structors
 *****************************************************************************/

ULevelSequencePlayer::ULevelSequencePlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}


/* ULevelSequencePlayer interface
 *****************************************************************************/

ULevelSequencePlayer* ULevelSequencePlayer::CreateLevelSequencePlayer(UObject* WorldContextObject, ULevelSequence* InLevelSequence, FMovieSceneSequencePlaybackSettings Settings, ALevelSequenceActor*& OutActor)
{
	if (InLevelSequence == nullptr)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.bAllowDuringConstructionScript = true;

	// Defer construction for autoplay so that BeginPlay() is called
	SpawnParams.bDeferConstruction = true;

	ALevelSequenceActor* Actor = World->SpawnActor<ALevelSequenceActor>(SpawnParams);

	Actor->PlaybackSettings = Settings;
	Actor->LevelSequence = InLevelSequence;

	Actor->InitializePlayer();
	OutActor = Actor;

	FTransform DefaultTransform;
	Actor->FinishSpawning(DefaultTransform);

	return Actor->SequencePlayer;
}

/* ULevelSequencePlayer implementation
 *****************************************************************************/

void ULevelSequencePlayer::Initialize(ULevelSequence* InLevelSequence, ULevel* InLevel, const FMovieSceneSequencePlaybackSettings& Settings)
{
	// Never use the level to resolve bindings unless we're playing back within a streamed or instanced level
	StreamedLevelAssetPath = NAME_None;

	World = InLevel->OwningWorld;
	Level = InLevel;

	// Construct the path to the level asset that the streamed level relates to
	ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel(InLevel);
	if (LevelStreaming)
	{
		// StreamedLevelPackage is a package name of the form /Game/Folder/MapName, not a full asset path
		FString StreamedLevelPackage = (LevelStreaming->PackageNameToLoad == NAME_None ? LevelStreaming->GetWorldAssetPackageFName() : LevelStreaming->PackageNameToLoad).ToString();

		int32 SlashPos = 0;
		if (StreamedLevelPackage.FindLastChar('/', SlashPos) && SlashPos < StreamedLevelPackage.Len()-1)
		{
			// Construct the asset path by appending .MapName to the end for efficient comparison with FSoftObjectPath::GetAssetPathName
			const TCHAR* Pair[] = { *StreamedLevelPackage, &StreamedLevelPackage[SlashPos+1] };
			StreamedLevelAssetPath = *FString::Join(Pair, TEXT("."));
		}
	}

	SpawnRegister = MakeShareable(new FLevelSequenceSpawnRegister);
	UMovieSceneSequencePlayer::Initialize(InLevelSequence, Settings);
}

void ULevelSequencePlayer::ResolveBoundObjects(const FGuid& InBindingId, FMovieSceneSequenceID SequenceID, UMovieSceneSequence& InSequence, UObject* ResolutionContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	bool bAllowDefault = PlaybackClient ? PlaybackClient->RetrieveBindingOverrides(InBindingId, SequenceID, OutObjects) : true;

	if (bAllowDefault)
	{
		if (StreamedLevelAssetPath != NAME_None && ResolutionContext && ResolutionContext->IsA<UWorld>())
		{
			ResolutionContext = Level.Get();
		}

		// Passing through the streamed level asset path ensures that bindings within instance sub levels resolve correctly
		CastChecked<ULevelSequence>(&InSequence)->LocateBoundObjects(InBindingId, ResolutionContext, StreamedLevelAssetPath, OutObjects);
	}
}

bool ULevelSequencePlayer::CanPlay() const
{
	return World.IsValid();
}

void ULevelSequencePlayer::OnStartedPlaying()
{
	EnableCinematicMode(true);
}

void ULevelSequencePlayer::OnStopped()
{
	EnableCinematicMode(false);

	AActor* LevelSequenceActor = Cast<AActor>(GetOuter());
	if (LevelSequenceActor == nullptr)
	{
		return;
	}

	for (FObjectKey WeakActor : PrerequisiteActors)
	{
		AActor* Actor = Cast<AActor>(WeakActor.ResolveObjectPtr());
		if (Actor)
		{
			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (Component)
				{
					Component->PrimaryComponentTick.RemovePrerequisite(LevelSequenceActor, LevelSequenceActor->PrimaryActorTick);
				}
			}

			Actor->PrimaryActorTick.RemovePrerequisite(LevelSequenceActor, LevelSequenceActor->PrimaryActorTick);
		}
	}

	if (World != nullptr && World->GetGameInstance() != nullptr)
	{
		APlayerController* PC = World->GetGameInstance()->GetFirstLocalPlayerController();

		if (PC != nullptr)
		{
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->bClientSimulatingViewTarget = false;
			}
		}
	}

	PrerequisiteActors.Reset();
	LastViewTarget.Reset();
}

void ULevelSequencePlayer::UpdateMovieSceneInstance(FMovieSceneEvaluationRange InRange, EMovieScenePlayerStatus::Type PlayerStatus, bool bHasJumped)
{
	UMovieSceneSequencePlayer::UpdateMovieSceneInstance(InRange, PlayerStatus, bHasJumped);

	FLevelSequencePlayerSnapshot NewSnapshot;
	TakeFrameSnapshot(NewSnapshot);

	if (!PreviousSnapshot.IsSet() || PreviousSnapshot.GetValue().CurrentShotName != NewSnapshot.CurrentShotName)
	{
		CSV_EVENT_GLOBAL(TEXT("%s"), *NewSnapshot.CurrentShotName);
		//UE_LOG(LogMovieScene, Log, TEXT("Shot evaluated: '%s'"), *NewSnapshot.CurrentShotName);
	}

	PreviousSnapshot = NewSnapshot;
}

/* IMovieScenePlayer interface
 *****************************************************************************/

void ULevelSequencePlayer::UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut)
{
	if (World == nullptr || World->GetGameInstance() == nullptr)
	{
		return;
	}

	// skip missing player controller
	APlayerController* PC = World->GetGameInstance()->GetFirstLocalPlayerController();

	if (PC == nullptr)
	{
		return;
	}

	// skip same view target
	AActor* ViewTarget = PC->GetViewTarget();

	// save the last view target so that it can be restored when the camera object is null
	if (!LastViewTarget.IsValid())
	{
		LastViewTarget = ViewTarget;
		if (PC->GetLocalPlayer())
		{
			LastAspectRatioAxisConstraint = PC->GetLocalPlayer()->AspectRatioAxisConstraint;
		}
	}

	UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromRuntimeObject(CameraObject);
	if (CameraComponent && CameraComponent->GetOwner() != CameraObject)
	{
		CameraObject = CameraComponent->GetOwner();
	}

	CachedCameraComponent = CameraComponent;

	if (!CanUpdateCameraCut())
	{
		return;
	}

	if (CameraObject == ViewTarget)
	{
		if ( bJumpCut )
		{
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->bGameCameraCutThisFrame = true;
			}

			if (CameraComponent)
			{
				CameraComponent->NotifyCameraCut();
			}
		}
		return;
	}

	// skip unlocking if the current view target differs
	AActor* UnlockIfCameraActor = Cast<AActor>(UnlockIfCameraObject);

	// if unlockIfCameraActor is valid, release lock if currently locked to object
	if (CameraObject == nullptr && UnlockIfCameraActor != nullptr && UnlockIfCameraActor != ViewTarget)
	{
		return;
	}

	// override the player controller's view target
	AActor* CameraActor = Cast<AActor>(CameraObject);

	// if the camera object is null, use the last view target so that it is restored to the state before the sequence takes control
	if (CameraActor == nullptr)
	{
		CameraActor = LastViewTarget.Get();

		// Skip if the last view target is the same as the current view target so that there's no additional camera cut
		if (CameraActor == ViewTarget)
		{
			return;
		}
	}

	FViewTargetTransitionParams TransitionParams;
	PC->SetViewTarget(CameraActor, TransitionParams);

	if (PC->GetLocalPlayer())
	{
		PC->GetLocalPlayer()->AspectRatioAxisConstraint = EAspectRatioAxisConstraint::AspectRatio_MaintainXFOV;
	}

	if (CameraComponent)
	{
		CameraComponent->NotifyCameraCut();
	}

	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->bClientSimulatingViewTarget = (CameraActor != nullptr);
		PC->PlayerCameraManager->bGameCameraCutThisFrame = true;
	}

	if (OnCameraCut.IsBound())
	{
		OnCameraCut.Broadcast(CameraComponent);
	}
}

void ULevelSequencePlayer::NotifyBindingUpdate(const FGuid& InGuid, FMovieSceneSequenceIDRef InSequenceID, TArrayView<TWeakObjectPtr<>> Objects)
{
	AActor* LevelSequenceActor = Cast<AActor>(GetOuter());
	if (LevelSequenceActor == nullptr)
	{
		return;
	}

	for (TWeakObjectPtr<> WeakObject : Objects)
	{
		if (AActor* Actor = Cast<AActor>(WeakObject.Get()))
		{
			
			if (Actor == LevelSequenceActor)
			{
				continue;
			}

			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (Component)
				{
					Component->PrimaryComponentTick.AddPrerequisite(LevelSequenceActor, LevelSequenceActor->PrimaryActorTick);
				}
			}

			Actor->PrimaryActorTick.AddPrerequisite(LevelSequenceActor, LevelSequenceActor->PrimaryActorTick);
			PrerequisiteActors.Add(Actor);
		}
	}
}

UObject* ULevelSequencePlayer::GetPlaybackContext() const
{
	return World.Get();
}

TArray<UObject*> ULevelSequencePlayer::GetEventContexts() const
{
	TArray<UObject*> EventContexts;
	if (World.IsValid())
	{
		GetEventContexts(*World, EventContexts);
	}

	ALevelSequenceActor* OwningActor = GetTypedOuter<ALevelSequenceActor>();
	if (OwningActor)
	{
		for (AActor* Actor : OwningActor->AdditionalEventReceivers)
		{
			if (Actor)
			{
				EventContexts.Add(Actor);
			}
		}
	}

	return EventContexts;
}

void ULevelSequencePlayer::GetEventContexts(UWorld& InWorld, TArray<UObject*>& OutContexts)
{
	if (InWorld.GetLevelScriptActor())
	{
		OutContexts.Add(InWorld.GetLevelScriptActor());
	}

	for (ULevelStreaming* StreamingLevel : InWorld.GetStreamingLevels())
	{
		if (StreamingLevel && StreamingLevel->GetLevelScriptActor())
		{
			OutContexts.Add(StreamingLevel->GetLevelScriptActor());
		}
	}
}

void ULevelSequencePlayer::TakeFrameSnapshot(FLevelSequencePlayerSnapshot& OutSnapshot) const
{
	if (!ensure(Sequence))
	{
		return;
	}

	// In Play Rate Resolution
	const FFrameTime StartTimeWithoutWarmupFrames = SnapshotOffsetTime.IsSet() ? StartTime + SnapshotOffsetTime.GetValue() : StartTime;
	const FFrameTime CurrentPlayTime = PlayPosition.GetCurrentPosition();
	// In Playback Resolution
	const FFrameTime CurrentSequenceTime		  = ConvertFrameTime(CurrentPlayTime, PlayPosition.GetInputRate(), PlayPosition.GetOutputRate());

	OutSnapshot.Settings = SnapshotSettings;

	OutSnapshot.MasterTime = FQualifiedFrameTime(CurrentPlayTime, PlayPosition.GetInputRate());
	OutSnapshot.MasterName = Sequence->GetName();

	OutSnapshot.CurrentShotName = OutSnapshot.MasterName;
	OutSnapshot.CurrentShotLocalTime = FQualifiedFrameTime(CurrentPlayTime, PlayPosition.GetInputRate());
	OutSnapshot.CameraComponent = CachedCameraComponent.IsValid() ? CachedCameraComponent.Get() : nullptr;
	OutSnapshot.ShotID = MovieSceneSequenceID::Invalid;
	OutSnapshot.ActiveShot = nullptr;

	UMovieSceneCinematicShotTrack* ShotTrack = Sequence->GetMovieScene()->FindMasterTrack<UMovieSceneCinematicShotTrack>();
	if (ShotTrack)
	{
		UMovieSceneCinematicShotSection* ActiveShot = nullptr;
		for (UMovieSceneSection* Section : ShotTrack->GetAllSections())
		{
			if (!ensure(Section))
			{
				continue;
			}

			// It's unfortunate that we have to copy the logic of UMovieSceneCinematicShotTrack::GetRowCompilerRules() to some degree here, but there's no better way atm
			bool bThisShotIsActive = Section->IsActive();

			TRange<FFrameNumber> SectionRange = Section->GetRange();
			bThisShotIsActive = bThisShotIsActive && SectionRange.Contains(CurrentSequenceTime.FrameNumber);

			if (bThisShotIsActive && ActiveShot)
			{
				if (Section->GetRowIndex() < ActiveShot->GetRowIndex())
				{
					bThisShotIsActive = true;
				}
				else if (Section->GetRowIndex() == ActiveShot->GetRowIndex())
				{
					// On the same row - latest start wins
					bThisShotIsActive = TRangeBound<FFrameNumber>::MaxLower(SectionRange.GetLowerBound(), ActiveShot->GetRange().GetLowerBound()) == SectionRange.GetLowerBound();
				}
				else
				{
					bThisShotIsActive = false;
				}
			}

			if (bThisShotIsActive)
			{
				ActiveShot = Cast<UMovieSceneCinematicShotSection>(Section);
			}
		}

		if (ActiveShot)
		{
			// Assume that shots with no sequence start at 0.
			FMovieSceneSequenceTransform OuterToInnerTransform = ActiveShot->OuterToInnerTransform();
			UMovieSceneSequence*         InnerSequence = ActiveShot->GetSequence();
			FFrameRate                   InnerTickResoloution = InnerSequence ? InnerSequence->GetMovieScene()->GetTickResolution() : PlayPosition.GetOutputRate();
			FFrameRate                   InnerFrameRate = InnerSequence ? InnerSequence->GetMovieScene()->GetDisplayRate() : PlayPosition.GetInputRate();
			FFrameTime                   InnerDisplayTime = ConvertFrameTime(CurrentSequenceTime * OuterToInnerTransform, InnerTickResoloution, InnerFrameRate);

			OutSnapshot.CurrentShotName = ActiveShot->GetShotDisplayName();
			OutSnapshot.CurrentShotLocalTime = FQualifiedFrameTime(InnerDisplayTime, InnerFrameRate);
			OutSnapshot.ShotID = ActiveShot->GetSequenceID();
			OutSnapshot.ActiveShot = Cast<ULevelSequence>(ActiveShot->GetSequence());

#if WITH_EDITORONLY_DATA
			FFrameNumber  InnerFrameNumber = InnerFrameRate.AsFrameNumber(InnerFrameRate.AsSeconds(InnerDisplayTime));
			FFrameNumber  InnerStartFrameNumber = ActiveShot->TimecodeSource.Timecode.ToFrameNumber(InnerFrameRate);
			FFrameNumber  InnerCurrentFrameNumber = InnerStartFrameNumber + InnerFrameNumber;
			FTimecode     InnerCurrentTimecode = ActiveShot->TimecodeSource.Timecode.FromFrameNumber(InnerCurrentFrameNumber, InnerFrameRate, false);

			OutSnapshot.SourceTimecode = InnerCurrentTimecode.ToString();
#else
			OutSnapshot.SourceTimecode = FTimecode().ToString();
#endif
		}
	}
}

void ULevelSequencePlayer::EnableCinematicMode(bool bEnable)
{
	// iterate through the controller list and set cinematic mode if necessary
	bool bNeedsCinematicMode = PlaybackSettings.bDisableMovementInput || PlaybackSettings.bDisableLookAtInput || PlaybackSettings.bHidePlayer || PlaybackSettings.bHideHud;

	if (bNeedsCinematicMode)
	{
		if (World.IsValid())
		{
			for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				APlayerController* PC = Iterator->Get();
				if (PC && PC->IsLocalController())
				{
					PC->SetCinematicMode(bEnable, PlaybackSettings.bHidePlayer, PlaybackSettings.bHideHud, PlaybackSettings.bDisableMovementInput, PlaybackSettings.bDisableLookAtInput);
				}
			}
		}
	}
}

