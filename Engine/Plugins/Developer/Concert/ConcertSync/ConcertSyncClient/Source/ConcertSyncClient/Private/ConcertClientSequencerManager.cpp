// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertClientSequencerManager.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"
#include "Logging/LogMacros.h"

#include "IConcertSession.h"
#include "ConcertSettings.h"

#include "Engine/GameEngine.h"
#include "MovieSceneSequence.h"
#include "LevelSequencePlayer.h"

#if WITH_EDITOR
	#include "ISequencerModule.h"
	#include "ISequencer.h"
	#include "Toolkits/AssetEditorManager.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogConcertSequencerSync, Warning, Log)

#if WITH_EDITOR

// Enable Sequence Playing on game client
static TAutoConsoleVariable<int32> CVarEnableSequencePlayer(TEXT("concert.EnableSequencePlayer"), 0, TEXT("Enable Concert Sequence Players on `-game` client."));

// Enable opening Sequencer on remote machine whenever a sequencer is opened, if both instance have this option on.
static TAutoConsoleVariable<int32> CVarEnableRemoteSequencerOpen(TEXT("concert.EnableOpenRemoteSequencer"), 0, TEXT("Enable Concert remote Sequencer opening."));


FSequencerEventClient::FSequencerEventClient()
{
	bRespondingToTransportEvent = false;

	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
	OnSequencerCreatedHandle = SequencerModule.RegisterOnSequencerCreated(FOnSequencerCreated::FDelegate::CreateRaw(this, &FSequencerEventClient::OnSequencerCreated));
	OnEndFrameHandle = FCoreDelegates::OnEndFrame.AddRaw(this, &FSequencerEventClient::OnEndFrame);
}

FSequencerEventClient::~FSequencerEventClient()
{
	ISequencerModule* SequencerModulePtr = FModuleManager::Get().GetModulePtr<ISequencerModule>("Sequencer");
	if (SequencerModulePtr)
	{
		SequencerModulePtr->UnregisterOnSequencerCreated(OnSequencerCreatedHandle);
	}

	if (OnEndFrameHandle.IsValid())
	{
		FCoreDelegates::OnEndFrame.Remove(OnEndFrameHandle);
		OnEndFrameHandle.Reset();
	}
	
	for (FOpenSequencerData& OpenSequencer : OpenSequencers)
	{
		TSharedPtr<ISequencer> Sequencer = OpenSequencer.WeakSequencer.Pin();
		if (Sequencer.IsValid())
		{
			Sequencer->OnGlobalTimeChanged().Remove(OpenSequencer.OnGlobalTimeChangedHandle);
			Sequencer->OnCloseEvent().Remove(OpenSequencer.OnCloseEventHandle);
		}
	}
}

void FSequencerEventClient::OnSequencerCreated(TSharedRef<ISequencer> InSequencer)
{
	// Find a Sequencer state for a newly opened sequencer if we have one.
	UMovieSceneSequence* Sequence = InSequencer->GetRootMovieSceneSequence();
	check(Sequence != nullptr);
	FConcertSequencerState& SequencerState = SequencerStates.FindOrAdd(*Sequence->GetPathName());

	// Setup the Sequencer
	FOpenSequencerData OpenSequencer;
	OpenSequencer.WeakSequencer = TWeakPtr<ISequencer>(InSequencer);
	OpenSequencer.PlaybackMode = EPlaybackMode::Undefined;
	OpenSequencer.OnGlobalTimeChangedHandle = InSequencer->OnGlobalTimeChanged().AddRaw(this, &FSequencerEventClient::OnSequencerTimeChanged, OpenSequencer.WeakSequencer);
	OpenSequencer.OnCloseEventHandle = InSequencer->OnCloseEvent().AddRaw(this, &FSequencerEventClient::OnSequencerClosed);
	int OpenIndex = OpenSequencers.Add(OpenSequencer);

	// Setup stored state
	InSequencer->SetPlaybackStatus((EMovieScenePlayerStatus::Type)SequencerState.PlayerStatus);
	InSequencer->SetPlaybackSpeed(SequencerState.PlaybackSpeed);
	// Setting the global time will notify the server of this newly opened state.
	InSequencer->SetGlobalTime(SequencerState.Time.ConvertTo(InSequencer->GetRootTickResolution()));
	// Since setting the global time will potentially have set our playback mode put us back to undefined
	OpenSequencers[OpenIndex].PlaybackMode = EPlaybackMode::Undefined;

	// if we allow for Sequencer remote opening send an event, if we aren't currently responding to one
	if (!bRespondingToTransportEvent && CVarEnableRemoteSequencerOpen.GetValueOnAnyThread() > 0)
	{
		TSharedPtr<IConcertClientSession> Session = WeakSession.Pin();
		if (Session.IsValid())
		{
			FConcertSequencerOpenEvent OpenEvent;
			OpenEvent.SequenceObjectPath = Sequence->GetPathName();

			UE_LOG(LogConcertSequencerSync, Verbose, TEXT("OnsequencerCreated: %s"), *OpenEvent.SequenceObjectPath);
			Session->SendCustomEvent(OpenEvent, Session->GetSessionServerEndpointId(), EConcertMessageFlags::ReliableOrdered);
		}
	}
}

TArray<FSequencerEventClient::FOpenSequencerData*, TInlineAllocator<1>> FSequencerEventClient::GatherRootSequencersByAssetPath(const FString& InSequenceObjectPath)
{
	TArray<FOpenSequencerData*, TInlineAllocator<1>> OutSequencers;
	for (FOpenSequencerData& Entry : OpenSequencers)
	{
		TSharedPtr<ISequencer> Sequencer = Entry.WeakSequencer.Pin();
		UMovieSceneSequence*   Sequence = Sequencer.IsValid() ? Sequencer->GetRootMovieSceneSequence() : nullptr;

		if (Sequence && Sequence->GetPathName() == InSequenceObjectPath)
		{
			OutSequencers.Add(&Entry);
		}
	}
	return OutSequencers;
}

void FSequencerEventClient::Register(TSharedRef<IConcertClientSession> InSession)
{
	// Hold onto the session so we can trigger events
	WeakSession = InSession;

	// Register our events
	InSession->RegisterCustomEventHandler<FConcertSequencerStateEvent>(this, &FSequencerEventClient::OnTransportEvent);
	InSession->RegisterCustomEventHandler<FConcertSequencerCloseEvent>(this, &FSequencerEventClient::OnCloseEvent);
	InSession->RegisterCustomEventHandler<FConcertSequencerOpenEvent>(this, &FSequencerEventClient::OnOpenEvent);
	InSession->RegisterCustomEventHandler<FConcertSequencerStateSyncEvent>(this, &FSequencerEventClient::OnSyncEvent);
}

void FSequencerEventClient::Unregister(TSharedRef<IConcertClientSession> InSession)
{
	// Unregister our events and explicitly reset the session ptr
	TSharedPtr<IConcertClientSession> Session = WeakSession.Pin();
	if (Session.IsValid())
	{
		Session->UnregisterCustomEventHandler<FConcertSequencerStateEvent>();
		Session->UnregisterCustomEventHandler<FConcertSequencerCloseEvent>();
		Session->UnregisterCustomEventHandler<FConcertSequencerOpenEvent>();
		Session->UnregisterCustomEventHandler<FConcertSequencerStateSyncEvent>();
	}
	WeakSession = nullptr;
}

void FSequencerEventClient::OnSequencerClosed(TSharedRef<ISequencer> InSequencer)
{
	// Find the associated open sequencer index
	int Index = 0;
	for (; Index < OpenSequencers.Num(); ++Index)
	{
		if (OpenSequencers[Index].WeakSequencer.HasSameObject(&InSequencer.Get()))
		{
			break;
		}
	}
	// We didn't find the sequencer
	if (Index >= OpenSequencers.Num())
	{
		return;
	}

	FOpenSequencerData& ClosingSequencer = OpenSequencers[Index];

	// Send close event to server and put back playback mode to undefined
	TSharedPtr<IConcertClientSession> Session = WeakSession.Pin();
	if (Session.IsValid())
	{
		// Find the associated sequence path name
		UMovieSceneSequence* Sequence = InSequencer->GetRootMovieSceneSequence();
		if (Sequence)
		{
			FConcertSequencerCloseEvent CloseEvent;
			CloseEvent.bMasterClose = ClosingSequencer.PlaybackMode == EPlaybackMode::Master; // this sequencer had control over the sequence playback
			CloseEvent.SequenceObjectPath = *Sequence->GetPathName();
			Session->SendCustomEvent(CloseEvent, Session->GetSessionServerEndpointId(), EConcertMessageFlags::ReliableOrdered);
		}
	}
	// Removed the closed Sequencer
	OpenSequencers.RemoveAtSwap(Index);
}

void FSequencerEventClient::OnSyncEvent(const FConcertSessionContext& InEventContext, const FConcertSequencerStateSyncEvent& InEvent)
{
	for (const auto& State : InEvent.SequencerStates)
	{
		FConcertSequencerState& SequencerState = SequencerStates.FindOrAdd(*State.SequenceObjectPath);
		SequencerState = State;
		for (FOpenSequencerData* OpenSequencer : GatherRootSequencersByAssetPath(State.SequenceObjectPath))
		{
			TSharedPtr<ISequencer> Sequencer = OpenSequencer->WeakSequencer.Pin();
			if (Sequencer.IsValid())
			{
				Sequencer->SetGlobalTime(SequencerState.Time.ConvertTo(Sequencer->GetRootTickResolution()));
				Sequencer->SetPlaybackStatus((EMovieScenePlayerStatus::Type)SequencerState.PlayerStatus);
				Sequencer->SetPlaybackSpeed(SequencerState.PlaybackSpeed);
			}
		}
	}
}

void FSequencerEventClient::OnSequencerTimeChanged(TWeakPtr<ISequencer> InSequencer)
{
	if (bRespondingToTransportEvent)
	{
		return;
	}

	TGuardValue<bool> ReentrancyGuard(bRespondingToTransportEvent, true);

	TSharedPtr<ISequencer> Sequencer = InSequencer.Pin();
	UMovieSceneSequence*   Sequence  = Sequencer.IsValid() ? Sequencer->GetRootMovieSceneSequence() : nullptr;

	TSharedPtr<IConcertClientSession> Session = WeakSession.Pin();
	if (Session.IsValid() && Sequence)
	{
		// Find the entry that has been updated so we can check/assign its playback mode, or add it in case a Sequencer root sequence was just reassigned
		FConcertSequencerState& SequencerState = SequencerStates.FindOrAdd(*Sequence->GetPathName());

		FOpenSequencerData* OpenSequencer = Algo::FindBy(OpenSequencers, InSequencer, &FOpenSequencerData::WeakSequencer);
		check(OpenSequencer);
		// We only send transport events if we're driving playback (Master), or nothing is currently playing back to our knowledge (Undefined)
		// @todo: Do we need to handle race conditions and/or contention between sequencers either initiating playback or scrubbing?
		if (OpenSequencer->PlaybackMode == EPlaybackMode::Master || OpenSequencer->PlaybackMode == EPlaybackMode::Undefined)
		{
			FConcertSequencerStateEvent SequencerStateEvent;
			SequencerStateEvent.State.SequenceObjectPath	= Sequence->GetPathName();
			SequencerStateEvent.State.Time					= Sequencer->GetGlobalTime();
			SequencerStateEvent.State.PlayerStatus			= (EConcertMovieScenePlayerStatus)Sequencer->GetPlaybackStatus();
			SequencerStateEvent.State.PlaybackSpeed			= Sequencer->GetPlaybackSpeed();
			SequencerState = SequencerStateEvent.State;

			// Send to client and server
			UE_LOG(LogConcertSequencerSync, Verbose, TEXT("OnSequencerTimeChanged: %s, at frame: %d"), *SequencerStateEvent.State.SequenceObjectPath, SequencerStateEvent.State.Time.Time.FrameNumber.Value);
			Session->SendCustomEvent(SequencerStateEvent, Session->GetSessionServerEndpointId(), EConcertMessageFlags::ReliableOrdered);

			// If we're playing then ensure we are set to master (driving the playback on all clients)
			if (SequencerStateEvent.State.PlayerStatus == EConcertMovieScenePlayerStatus::Playing)
			{
				OpenSequencer->PlaybackMode = EPlaybackMode::Master;
			}
			else
			{
				OpenSequencer->PlaybackMode = EPlaybackMode::Undefined;
			}
		}
	}
}

void FSequencerEventClient::OnCloseEvent(const FConcertSessionContext&, const FConcertSequencerCloseEvent& InEvent)
{
	FConcertSequencerState* SequencerState = SequencerStates.Find(*InEvent.SequenceObjectPath);
	if (SequencerState)
	{
		// if the event was that a sequencer that was in master playback mode was closed, stop playback
		if (InEvent.bMasterClose)
		{
			SequencerState->PlayerStatus = EConcertMovieScenePlayerStatus::Stopped;
			for (FOpenSequencerData* OpenSequencer : GatherRootSequencersByAssetPath(InEvent.SequenceObjectPath))
			{
				OpenSequencer->PlaybackMode = EPlaybackMode::Undefined;
				OpenSequencer->WeakSequencer.Pin()->SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
			}
		}
		// otherwise, discard the state, it's no longer opened.
		else 
		{
			SequencerStates.Remove(*InEvent.SequenceObjectPath);
		}
	}

	ApplyCloseToPlayers(InEvent);
}

void FSequencerEventClient::OnOpenEvent(const FConcertSessionContext&, const FConcertSequencerOpenEvent& InEvent)
{
	UE_LOG(LogConcertSequencerSync, Verbose, TEXT("OnOpenEvent: %s"), *InEvent.SequenceObjectPath);
	PendingSequenceOpenEvents.Add(InEvent.SequenceObjectPath);
}

void FSequencerEventClient::ApplyTransportOpenEvent(const FString& SequenceObjectPath)
{
	TGuardValue<bool> ReentrancyGuard(bRespondingToTransportEvent, true);
	if (CVarEnableRemoteSequencerOpen.GetValueOnAnyThread() > 0)
	{
		FAssetEditorManager::Get().OpenEditorForAsset(SequenceObjectPath);
	}
}

void FSequencerEventClient::ApplyCloseToPlayers(const FConcertSequencerCloseEvent& InEvent)
{
	ULevelSequencePlayer* Player = SequencePlayers.FindRef(*InEvent.SequenceObjectPath);
	if (Player)
	{
		Player->Stop();
		if (!InEvent.bMasterClose)
		{
			SequencePlayers.Remove(*InEvent.SequenceObjectPath);
		}
	}
}

void FSequencerEventClient::OnTransportEvent(const FConcertSessionContext&, const FConcertSequencerStateEvent& InEvent)
{
	PendingSequencerEvents.Add(InEvent.State);
}

void FSequencerEventClient::ApplyTransportEvent(const FConcertSequencerState& EventState)
{
	if (bRespondingToTransportEvent)
	{
		return;
	}

	TGuardValue<bool> ReentrancyGuard(bRespondingToTransportEvent, true);

	// Update the sequencer pointing to the same sequence
	FConcertSequencerState& SequencerState = SequencerStates.FindOrAdd(*EventState.SequenceObjectPath);

	// Record the Sequencer State
	SequencerState = EventState;

	if (GIsEditor)
	{
		ApplyEventToSequencers(SequencerState);
	}
	else if (CVarEnableSequencePlayer.GetValueOnAnyThread() > 0)
	{
		ApplyEventToPlayers(SequencerState);
	}
}

void FSequencerEventClient::ApplyEventToSequencers(const FConcertSequencerState& EventState)
{
	UE_LOG(LogConcertSequencerSync, Verbose, TEXT("ApplyEvent: %s, at frame: %d"), *EventState.SequenceObjectPath, EventState.Time.Time.FrameNumber.Value);
	FConcertSequencerState& SequencerState = SequencerStates.FindOrAdd(*EventState.SequenceObjectPath);

	// Record the Sequencer State
	SequencerState = EventState;

	float LatencyCompensationMs = GetDefault<UConcertClientConfig>()->ClientSettings.LatencyCompensationMs;

	// Update all opened sequencer with this root sequence
	for (FOpenSequencerData* OpenSequencer : GatherRootSequencersByAssetPath(EventState.SequenceObjectPath))
	{	
		ISequencer* Sequencer = OpenSequencer->WeakSequencer.Pin().Get();
		// If the entry is driving playback (PlaybackMode == Master) then we never respond to external transport events
		if (!Sequencer || OpenSequencer->PlaybackMode == EPlaybackMode::Master)
		{
			return;
		}

		FFrameRate SequenceRate = Sequencer->GetRootTickResolution();
		FFrameTime IncomingTime = EventState.Time.ConvertTo(SequenceRate);

		// If the event is coming from a sequencer that is playing back, we are a slave to its updates until it stops
		// We also apply any latency compensation when playing back
		if (EventState.PlayerStatus == EConcertMovieScenePlayerStatus::Playing ||
			EventState.PlayerStatus == EConcertMovieScenePlayerStatus::Recording)
		{
			OpenSequencer->PlaybackMode = EPlaybackMode::Slave;

			FFrameTime CurrentTime = Sequencer->GetGlobalTime().Time;

			// We should be playing back, but are not currently - we compensate the event time for network latency and commence playback
			if (Sequencer->GetPlaybackStatus() != EMovieScenePlayerStatus::Playing)
			{
				// @todo: latency compensation could be more accurate (and automatic) if we're genlocked, and events are timecoded.
				// @todo: latency compensation does not take into account slomo tracks on the sequence - should it? (that would be intricate to support)
				FFrameTime CompensatedTime = IncomingTime + (LatencyCompensationMs / 1000.0) * SequenceRate;

				// Log time metrics
				UE_LOG(LogConcertSequencerSync, Display, TEXT(
					"Starting multi-user playback for sequence '%s':\n"
					"    Current Time           = %d+%fs (%f seconds)\n"
					"    Incoming Time          = %d+%fs (%f seconds)\n"
					"    Compensated Time       = %d+%fs (%f seconds)"),
					*EventState.SequenceObjectPath,
					CurrentTime.FrameNumber.Value, CurrentTime.GetSubFrame(), CurrentTime / SequenceRate,
					IncomingTime.FrameNumber.Value, IncomingTime.GetSubFrame(), IncomingTime / SequenceRate,
					CompensatedTime.FrameNumber.Value, CompensatedTime.GetSubFrame(), CompensatedTime / SequenceRate
				);

				Sequencer->SetGlobalTime(CompensatedTime);
				Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Playing);
				Sequencer->SetPlaybackSpeed(EventState.PlaybackSpeed);
			}
			else
			{
				// We're already playing so just report the time metrics, but adjust playback speed
				FFrameTime Error = FMath::Abs(IncomingTime - CurrentTime);
				Sequencer->SetPlaybackSpeed(EventState.PlaybackSpeed);

				UE_LOG(LogConcertSequencerSync, Display, TEXT(
					"Incoming update to sequence '%s':\n"
					"    Current Time       = %d+%fs (%f seconds)\n"
					"    Incoming Time      = %d+%fs (%f seconds)\n"
					"    Error              = %d+%fs (%f seconds)"),
					*EventState.SequenceObjectPath,
					CurrentTime.FrameNumber.Value, CurrentTime.GetSubFrame(), CurrentTime / SequenceRate,
					IncomingTime.FrameNumber.Value, IncomingTime.GetSubFrame(), IncomingTime / SequenceRate,
					Error.FrameNumber.Value, Error.GetSubFrame(), Error / SequenceRate
				);
			}
		}
		else
		{
			OpenSequencer->PlaybackMode = EPlaybackMode::Undefined;

			// If the incoming event is not playing back, set the player status to that of the event, and set the time
			if (Sequencer->GetPlaybackStatus() != (EMovieScenePlayerStatus::Type)EventState.PlayerStatus)
			{
				Sequencer->SetPlaybackStatus((EMovieScenePlayerStatus::Type)EventState.PlayerStatus);
			}

			// Set time after the status so that audio correctly stops playing after the sequence stops
			Sequencer->SetGlobalTime(IncomingTime);
			Sequencer->SetPlaybackSpeed(EventState.PlaybackSpeed);
		}
	}
}

void FSequencerEventClient::ApplyEventToPlayers(const FConcertSequencerState& EventState)
{
	ULevelSequencePlayer* Player = nullptr;
	// we do not have a player for this state yet
	if (!SequencePlayers.Contains(*EventState.SequenceObjectPath))
	{
		UWorld* CurrentWorld = nullptr;
		if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
		{
			CurrentWorld = GameEngine->GetGameWorld();
		}

		// Get the actual sequence
		ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *EventState.SequenceObjectPath);
		if (Sequence && CurrentWorld)
		{
			Player = NewObject<ULevelSequencePlayer>((UObject*)GetTransientPackage(), FName("ConcertSequencePlayer"));
			Player->Initialize(Sequence, CurrentWorld->PersistentLevel, FMovieSceneSequencePlaybackSettings());
		}
		SequencePlayers.Add(*EventState.SequenceObjectPath, Player);
	}

	Player = SequencePlayers.FindChecked(*EventState.SequenceObjectPath);
	if (Player)
	{
		float LatencyCompensationMs = GetDefault<UConcertClientConfig>()->ClientSettings.LatencyCompensationMs;

		FFrameRate SequenceRate = Player->GetFrameRate();
		FFrameTime IncomingTime = EventState.Time.ConvertTo(SequenceRate);

		// If the event is coming from a sequencer that is playing back, we are a slave to its updates until it stops
		// We also apply any latency compensation when playing back
		if (EventState.PlayerStatus == EConcertMovieScenePlayerStatus::Playing ||
			EventState.PlayerStatus == EConcertMovieScenePlayerStatus::Recording)
		{
			FFrameTime CurrentTime = Player->GetCurrentTime().Time;

			// We should be playing back, but are not currently - we compensate the event time for network latency and commence playback
			if (!Player->IsPlaying())
			{
				// @todo: latency compensation could be more accurate (and automatic) if we're genlocked, and events are timecoded.
				// @todo: latency compensation does not take into account slomo tracks on the sequence - should it? (that would be intricate to support)
				FFrameTime CompensatedTime = IncomingTime + (LatencyCompensationMs / 1000.0) * SequenceRate;

				// Log time metrics
				UE_LOG(LogConcertSequencerSync, Display, TEXT(
					"Starting multi-user playback for sequence '%s':\n"
					"    Current Time           = %d+%fs (%f seconds)\n"
					"    Incoming Time          = %d+%fs (%f seconds)\n"
					"    Compensated Time       = %d+%fs (%f seconds)"),
					*EventState.SequenceObjectPath,
					CurrentTime.FrameNumber.Value, CurrentTime.GetSubFrame(), CurrentTime / SequenceRate,
					IncomingTime.FrameNumber.Value, IncomingTime.GetSubFrame(), IncomingTime / SequenceRate,
					CompensatedTime.FrameNumber.Value, CompensatedTime.GetSubFrame(), CompensatedTime / SequenceRate
				);

				Player->PlayToFrame(CompensatedTime);
				Player->SetPlayRate(EventState.PlaybackSpeed);
			}
			else
			{
				// We're already playing so just report the time metrics, but adjust playback speed
				FFrameTime Error = FMath::Abs(IncomingTime - CurrentTime);
				Player->SetPlayRate(EventState.PlaybackSpeed);

				UE_LOG(LogConcertSequencerSync, Display, TEXT(
					"Incoming update to sequence '%s':\n"
					"    Current Time       = %d+%fs (%f seconds)\n"
					"    Incoming Time      = %d+%fs (%f seconds)\n"
					"    Error              = %d+%fs (%f seconds)"),
					*EventState.SequenceObjectPath,
					CurrentTime.FrameNumber.Value, CurrentTime.GetSubFrame(), CurrentTime / SequenceRate,
					IncomingTime.FrameNumber.Value, IncomingTime.GetSubFrame(), IncomingTime / SequenceRate,
					Error.FrameNumber.Value, Error.GetSubFrame(), Error / SequenceRate
				);
			}
		}
		else
		{
			switch (EventState.PlayerStatus)
			{
			case EConcertMovieScenePlayerStatus::Stepping:
				// fallthrough, handles as scrub
			case EConcertMovieScenePlayerStatus::Scrubbing:
				Player->ScrubToFrame(IncomingTime);
				break;
			case EConcertMovieScenePlayerStatus::Paused:
				Player->JumpToFrame(IncomingTime);
				Player->Pause();
				break;
			case EConcertMovieScenePlayerStatus::Stopped:
				Player->JumpToFrame(IncomingTime);
				Player->Stop();
				break;
			case EConcertMovieScenePlayerStatus::Jumping:
				// fallthrough, handles as stop
			default:
				Player->JumpToFrame(IncomingTime);
			}

			Player->SetPlayRate(EventState.PlaybackSpeed);
		}
	}
}

void FSequencerEventClient::OnEndFrame()
{
	for (const FString& SequenceObjectPath : PendingSequenceOpenEvents)
	{
		ApplyTransportOpenEvent(SequenceObjectPath);
	}
	PendingSequenceOpenEvents.Reset();

	for (const FConcertSequencerState& State : PendingSequencerEvents)
	{
		ApplyTransportEvent(State);
	}
	PendingSequencerEvents.Reset();
}

void FSequencerEventClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(SequencePlayers);
}

#endif

