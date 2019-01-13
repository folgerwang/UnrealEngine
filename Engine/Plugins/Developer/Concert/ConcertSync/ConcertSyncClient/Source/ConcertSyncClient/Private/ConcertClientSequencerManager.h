// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/CoreDefines.h"
#include "UObject/NameTypes.h"
#include "UObject/GCObject.h"
#include "ConcertSequencerMessages.h"

struct FConcertSessionContext;
class IConcertClientSession;
class ULevelSequencePlayer;

#if WITH_EDITOR

class ISequencer;

/**
 * Event manager that is held by the client sync module that keeps track of open sequencer UIs, regardless of whether a session is open or not
 * Events are registered to client sessions that will then operate on any tracked sequencer UIs
 */
struct FSequencerEventClient : public FGCObject
{
	/**
	 * Constructor - registers OnSequencerCreated handler with the sequencer module
	 */
	FSequencerEventClient();


	/**
	 * Destructor - unregisters OnSequencerCreated handler from the sequencer module
	 */
	~FSequencerEventClient();


	/**
	 * Register all custom sequencer events for the specified client session
	 *
	 * @param InSession       The client session to register custom events with
	 */
	void Register(TSharedRef<IConcertClientSession> InSession);


	/**
	 * Unregister previously registered custom sequencer events from the specified client session
	 *
	 * @param InSession       The client session to unregister custom events from
	 */
	void Unregister(TSharedRef<IConcertClientSession> InSession);

private:

	/** Enum signifying how a sequencer UI is currently playing. Necessary to prevent transport event contention. */
	enum class EPlaybackMode
	{
		/** This sequencer's time should be propagated to the collaboration server */
		Master,
		/** This sequencer's time should be updated in response to an event from the collaboration server */
		Slave,
		/** To our knowledge, no sequencer is playing back, and this sequencer will both send and receive transport events */
		Undefined
	};

	/** Struct containing the Open Sequencer data */
	struct FOpenSequencerData
	{
		/** Enum that signifies whether to send/receive transport events. */
		EPlaybackMode PlaybackMode;

		/** Weak pointer to the sequencer itself, if locally opened. */
		TWeakPtr<ISequencer> WeakSequencer;

		/** Delegate handle to the Global Time Changed event for the sequencer, if locally opened. */
		FDelegateHandle OnGlobalTimeChangedHandle;

		/** Delegate handle to the Close event for the sequencer, if locally opened. */
		FDelegateHandle OnCloseEventHandle;
	};

private:

	/**
	 * Called when a sequencer closes.
	 *
	 * @param InSequencer The sequencer that closed.
	 */
	void OnSequencerClosed(TSharedRef<ISequencer> InSequencer);

	/**
	 * Called on receipt of an external transport event from another client
	 *
	 * @param InEventContext              The context for the current client session
	 * @param InEvent                     The sequencer transport event received from the client
	 */
	void OnTransportEvent(const FConcertSessionContext&, const FConcertSequencerStateEvent& InEvent);

	/**
	 * Called on receipt of an external close event from the server
	 *
	 * @param InEventContext              The context for the current client session
	 * @param InEvent                     The sequencer close event received from the server
	 */
	void OnCloseEvent(const FConcertSessionContext&, const FConcertSequencerCloseEvent& InEvent);

	/**
	 * Called on receipt of an external open event from the server
	 *
	 * @param InEventContext              The context for the current client session
	 * @param InEvent                     The sequencer open event received from the server
	 */
	void OnOpenEvent(const FConcertSessionContext&, const FConcertSequencerOpenEvent& InEvent);

	/**
	 * Called on receipt of an external close event from the server
	 *
	 * @param InEventContext              The context for the current client session
	 * @param InEvent                     The sequencer close event received from the server
	 */
	void OnSyncEvent(const FConcertSessionContext& InEventContext, const FConcertSequencerStateSyncEvent& InEvent);

	/**
	 * Called when the global time has been changed for the specified Sequencer
	 *
	 * @param InSequencer                 The sequencer that has just updated its time
	 */
	void OnSequencerTimeChanged(TWeakPtr<ISequencer> InSequencer);

	/**
	 * Handle the creation of a newly opened sequencer instance
	 *
	 * @param InSequencer                 The sequencer that has just been created. Should not hold persistent shared references.
	 */
	void OnSequencerCreated(TSharedRef<ISequencer> InSequencer);

	/**
	 * Handle the end of frame callback to apply pending sequencer events
	 */
	void OnEndFrame();

	/**
	 * Apply a Sequencer open event 
	 *
	 * @param SequenceObjectPath	The sequence to open
	 */
	void ApplyTransportOpenEvent(const FString& SequenceObjectPath);

	/**
	 * Apply a Sequencer event 
	 *
	 * @param PendingState	The pending state to apply
	 */
	void ApplyTransportEvent(const FConcertSequencerState& PendingState);

	/**
	 * Apply a Sequencer event to opened Sequencers
	 *
	 * @param PendingState	The pending state to apply
	 */
	void ApplyEventToSequencers(const FConcertSequencerState& PendingState);

	/**
	 * Apply a Sequencer event to SequencePlayers
	 *
	 * @param PendingState	The pending state to apply
	 */
	void ApplyEventToPlayers(const FConcertSequencerState& PendingState);

	/**
	 * Apply a Sequencer Close Event to SequencePlayers
	 *
	 * @param PendingState	The pending state to apply
	 */
	void ApplyCloseToPlayers(const FConcertSequencerCloseEvent& InEvent);

	/**
	 * Gather all the currently open sequencer UIs that have the specified path as their root sequence
	 *
	 * @param InSequenceObjectPath        The full path to the root asset to gather sequences for
	 * @return An array containing all the entries that apply to the supplied sequence path
	 */
	TArray<FOpenSequencerData*, TInlineAllocator<1>> GatherRootSequencersByAssetPath(const FString& InSequenceObjectPath);


	/** FGCObject interface*/
	virtual void AddReferencedObjects(FReferenceCollector& Collector);
private:

	/** List of pending sequencer events to apply at end of frame. */
	TArray<FConcertSequencerState> PendingSequencerEvents;

	/** List of pending sequencer open events to apply at end of frame. */
	TArray<FString> PendingSequenceOpenEvents;

	/** Map of all currently opened Root Sequence State in a session, locally opened or not. */
	TMap<FName, FConcertSequencerState> SequencerStates;

	/** List of all locally opened sequencer. */
	TArray<FOpenSequencerData> OpenSequencers;

	/** Map of opened sequence players, if not in editor mode. */
	TMap<FName, ULevelSequencePlayer*> SequencePlayers;

	/** Boolean that is set when we are handling any transport event to prevent re-entrancy */
	bool bRespondingToTransportEvent;

	/** Delegate handle for the global sequencer created event registered with the sequencer module */
	FDelegateHandle OnSequencerCreatedHandle;

	/** Delegate handle for the global sequencer created event registered with the sequencer module */
	FDelegateHandle OnEndFrameHandle;

	/** Weak pointer to the client session with which to send events. May be null or stale. */
	TWeakPtr<IConcertClientSession> WeakSession;
};

#endif // WITH_EDITOR
