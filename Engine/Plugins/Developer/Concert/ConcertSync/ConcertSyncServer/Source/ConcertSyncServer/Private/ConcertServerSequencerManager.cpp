// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertServerSequencerManager.h"
#include "IConcertSession.h"


FConcertServerSequencerManager::FConcertServerSequencerManager(TSharedRef<IConcertServerSession> InSession)
{
	BindSession(InSession);
}

FConcertServerSequencerManager::~FConcertServerSequencerManager()
{
	UnbindSession();
}

void FConcertServerSequencerManager::BindSession(const TSharedRef<IConcertServerSession>& InSession)
{
	UnbindSession();
	Session = InSession;

	SessionClientChangedHandle = Session->OnSessionClientChanged().AddRaw(this, &FConcertServerSequencerManager::HandleSessionClientChanged);
	Session->RegisterCustomEventHandler<FConcertSequencerCloseEvent>(this, &FConcertServerSequencerManager::HandleSequencerCloseEvent);
	Session->RegisterCustomEventHandler<FConcertSequencerStateEvent>(this, &FConcertServerSequencerManager::HandleSequencerStateEvent);
	Session->RegisterCustomEventHandler<FConcertSequencerOpenEvent>(this, &FConcertServerSequencerManager::HandleSequencerOpenEvent);
}

void FConcertServerSequencerManager::UnbindSession()
{
	if (Session.IsValid())
	{
		Session->OnSessionClientChanged().Remove(SessionClientChangedHandle);
		Session->UnregisterCustomEventHandler<FConcertSequencerOpenEvent>();
		Session->UnregisterCustomEventHandler<FConcertSequencerCloseEvent>();
		Session->UnregisterCustomEventHandler<FConcertSequencerStateEvent>();
		Session.Reset();
	}
}

void FConcertServerSequencerManager::HandleSequencerStateEvent(const FConcertSessionContext& InEventContext, const FConcertSequencerStateEvent& InEvent)
{
	// Create or update the Sequencer state 
	FConcertOpenSequencerState& SequencerState = SequencerStates.FindOrAdd(*InEvent.State.SequenceObjectPath);
	SequencerState.ClientEndpointIds.AddUnique(InEventContext.SourceEndpointId);
	SequencerState.State = InEvent.State;

	// Forward the message to the other clients
	TArray<FGuid> ClientIds = Session->GetSessionClientEndpointIds();
	ClientIds.Remove(InEventContext.SourceEndpointId);
	Session->SendCustomEvent(InEvent, ClientIds, EConcertMessageFlags::ReliableOrdered);
}

void FConcertServerSequencerManager::HandleSequencerOpenEvent(const FConcertSessionContext& InEventContext, const FConcertSequencerOpenEvent& InEvent)
{
	// Create or update the Sequencer state 
	FConcertOpenSequencerState& SequencerState = SequencerStates.FindOrAdd(*InEvent.SequenceObjectPath);
	SequencerState.ClientEndpointIds.AddUnique(InEventContext.SourceEndpointId);
	SequencerState.State.SequenceObjectPath = InEvent.SequenceObjectPath;

	// Forward the message to the other clients
	TArray<FGuid> ClientIds = Session->GetSessionClientEndpointIds();
	ClientIds.Remove(InEventContext.SourceEndpointId);
	Session->SendCustomEvent(InEvent, ClientIds, EConcertMessageFlags::ReliableOrdered);
}

void FConcertServerSequencerManager::HandleSequencerCloseEvent(const FConcertSessionContext& InEventContext, const FConcertSequencerCloseEvent& InEvent)
{
	FConcertOpenSequencerState* SequencerState = SequencerStates.Find(*InEvent.SequenceObjectPath);
	if (SequencerState)
	{
		SequencerState->ClientEndpointIds.Remove(InEventContext.SourceEndpointId);
		if (SequencerState->ClientEndpointIds.Num() == 0)
		{
			// Forward a normal close event to clients
			FConcertSequencerCloseEvent CloseEvent;
			CloseEvent.bMasterClose = false;
			CloseEvent.SequenceObjectPath = InEvent.SequenceObjectPath;
			Session->SendCustomEvent(CloseEvent, Session->GetSessionClientEndpointIds(), EConcertMessageFlags::ReliableOrdered);
			SequencerStates.Remove(*InEvent.SequenceObjectPath);
		}
		// if a sequence was close while it was the master, forward it to client
		else if (InEvent.bMasterClose)
		{
			Session->SendCustomEvent(InEvent, Session->GetSessionClientEndpointIds(), EConcertMessageFlags::ReliableOrdered);
		}
	}
}

void FConcertServerSequencerManager::HandleSessionClientChanged(IConcertServerSession& InSession, EConcertClientStatus InClientStatus, const FConcertSessionClientInfo& InClientInfo)
{
	check(&InSession == Session.Get());
	// Remove the client from all open sequences
	if (InClientStatus == EConcertClientStatus::Disconnected)
	{
		for (auto It = SequencerStates.CreateIterator(); It; ++It)
		{
			It->Value.ClientEndpointIds.Remove(InClientInfo.ClientEndpointId);
			if (It->Value.ClientEndpointIds.Num() == 0)
			{
				// Forward the close event to clients
				FConcertSequencerCloseEvent CloseEvent;
				CloseEvent.SequenceObjectPath = It->Key.ToString();
				Session->SendCustomEvent(CloseEvent, Session->GetSessionClientEndpointIds(), EConcertMessageFlags::ReliableOrdered);

				It.RemoveCurrent();
			}
		}
	}
	// Send the current Sequencers states to the newly connected client
	else
	{
		FConcertSequencerStateSyncEvent SyncEvent;
		for (const auto& Pair : SequencerStates)
		{
			SyncEvent.SequencerStates.Add(Pair.Value.State);
		}
		Session->SendCustomEvent(SyncEvent, InClientInfo.ClientEndpointId, EConcertMessageFlags::ReliableOrdered);
	}
}
