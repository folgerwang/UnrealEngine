// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IConcertSessionHandler.h"
#include "ConcertUIExtension.h"
#include "ConcertPresenceEvents.h"
#include "ConcertMessages.h"
#include "ConcertWorkspaceMessages.h"
#include "ConcertClientPresenceActor.h"
#include "ConcertClientPresenceMode.h"
#include "UObject/Class.h"
#include "UObject/GCObject.h"

#if WITH_EDITOR

#include "ViewportWorldInteraction.h"

class IConcertClientSession;

/** Remote client event items */
struct FConcertClientPresenceStateEntry
{
	FConcertClientPresenceStateEntry(TSharedRef<FConcertClientPresenceEventBase> InPresenceEvent)
		: PresenceEvent(MoveTemp(InPresenceEvent))
		, bSyncPending(true) {}

	/** Presence state */
	TSharedRef<FConcertClientPresenceEventBase> PresenceEvent;

	/** Whether state needs to be synchronized to actor */
	bool bSyncPending;
};

/** State for remote clients associated with client id */
struct FConcertClientPresenceState
{
	FConcertClientPresenceState()
		: bIsConnected(true)
		, bVisible(true)
		, bInPIE(false)
		, VRDevice(NAME_None)
	{}

	/** State map */
	TMap<UScriptStruct*, FConcertClientPresenceStateEntry> EventStateMap;

	/** Display name */
	FString DisplayName;

	/** Whether client is connected */
	bool bIsConnected;

	/** Whether client is visible */
	bool bVisible;

	/** Whether client is in PIE */
	bool bInPIE;

	/** Whether client is using a VRDevice */
	FName VRDevice;

	/** Presence actor */
	TWeakObjectPtr<AConcertClientPresenceActor> PresenceActor;
};

/** State that persists beyond remote client sessions, associated with display name */
struct FConcertClientPresencePersistentState
{
	FConcertClientPresencePersistentState()
		: bVisible(true)
		, bPropagateToAll(false) {}

	/** Whether client is visible */
	bool bVisible;

	/** Whether the visibility of this client should be propagated to others? */
	bool bPropagateToAll;
};

class FConcertClientPresenceManager : public TSharedFromThis<FConcertClientPresenceManager>, public FGCObject
{
public:
	FConcertClientPresenceManager(TSharedRef<IConcertClientSession> InSession);
	~FConcertClientPresenceManager();

	//~ FGCObject interfaces
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	/** Gets the container for all the assets of Concert clients. */
	const class UConcertAssetContainer& GetAssetContainer() const;

	/** Returns true if current session is in PIE */
	bool IsInPIE() const;
	
	/** Get the current world */
	UWorld* GetWorld() const;

	/** Get the active perspective viewport */
	FLevelEditorViewportClient * GetPerspectiveViewport() const;

	/** Set whether presence is currently enabled and should be shown (unless hidden by other settings) */
	void SetPresenceEnabled(const bool bIsEnabled = true);

	/** Set presence visibility */
	void SetPresenceVisibility(const FString& InDisplayName, bool bVisibility, bool bPropagateToAll = false);

	/** Get location update frequency */
	static double GetLocationUpdateFrequency();

	/** Jump (teleport) to another presence */
	void InitiateJumpToPresence(FGuid InEndpointId);

	/**
	 * Returns the path to the UWorld object opened in the editor of the specified client endpoint.
	 * The information may be unavailable if the client was disconnected, the information hasn't replicated yet
	 * or the code was not compiled as part of the UE Editor. The path returned can be the path of a play world (PIE/SIE)
	 * if the user is in PIE/SIE. It this case, the path will look like /Game/UEDPIE_10_FooMap.FooMap rather than /Game/FooMap.FooMap.
	 * @param InEndpointId The end point of any clients connected to the session (local or remote).
	 * @return The path to the world being opened in the specified end point editor or an empty string if the information is not available.
	 */
	FString GetClientWorldPath(FGuid InEndpointId) const;

private:

	/** Register event and delegate handlers */
	void Register();

	/** Unregister event and delegate handlers */
	void Unregister();

	/** Is presence visible for the given state? */
	bool IsPresenceVisible(const FConcertClientPresenceState& InPresenceState) const;

	/** Is presence visible for the given endpoint id? */
	bool IsPresenceVisible(const FGuid& InEndpointId) const;

	/** Handles end-of-frame updates */
	void OnEndFrame();

	/** Synchronize presence state */
	void SynchronizePresenceState();

	/** Get cached presence state for given endpoint id */
	const TSharedPtr<FConcertClientPresenceDataUpdateEvent> GetCachedPresenceState(const FConcertClientPresenceState& InPresenceState) const;

	/** Get cached presence state for given endpoint id */
	const TSharedPtr<FConcertClientPresenceDataUpdateEvent> GetCachedPresenceState(const FGuid& InEndpointId) const;

	/** Update existing presence with event data */
	template<class PresenceActorClass, typename PresenceUpdateEventType>
	void UpdatePresence(AConcertClientPresenceActor* InPresenceActor, const PresenceUpdateEventType& InEvent);

	/** Handle a presence data update from another client session */
	template<typename PresenceUpdateEventType>
	void HandleConcertClientPresenceUpdateEvent(const FConcertSessionContext& InSessionContext, const PresenceUpdateEventType& InEvent);
		
	/** Handle a presence visibility update from another client session */
	void HandleConcertClientPresenceVisibilityUpdateEvent(const FConcertSessionContext& InSessionContext, const FConcertClientPresenceVisibilityUpdateEvent& InEvent);

	/** Returns true if presence event was not received out-of-order */
	bool ShouldProcessPresenceEvent(const FConcertSessionContext& InSessionContext, const UStruct* InEventType, const FConcertClientPresenceEventBase& InEvent) const;

	/** Create a new presence actor */
	AConcertClientPresenceActor* CreatePresenceActor(const FConcertClientInfo& InClientInfo, FName VRDevice);

	/** Spawn a presence actor */
	AConcertClientPresenceActor* SpawnPresenceActor(const FConcertClientInfo& InClientInfo, FName VRDevice);

	/** Clear presence */
	void ClearPresenceActor(const FGuid& InEndpointId);

	/** Destroy a presence */
	void DestroyPresenceActor(TWeakObjectPtr<AConcertClientPresenceActor> PresenceActor);

	/** Clear all presence */
	void ClearAllPresenceState();

	/** Handle a play session update from another client session */
	void HandleConcertPlaySessionEvent(const FConcertSessionContext& InSessionContext, const FConcertPlaySessionEvent &InEvent);

	/** Handle a client session disconnect */
	void OnSessionClientChanged(IConcertClientSession& InSession, EConcertClientStatus InClientStatus, const FConcertSessionClientInfo& InClientInfo);

	/** Handle entering VR */
	void OnVREditingModeEnter();

	/** Handle exiting VR */
	void OnVREditingModeExit();

	/** Updates current presence mode based on VR status and avatar classes */
	void UpdatePresenceMode();

	/** Notifies other clients to update avatar for this client */
	void SendPresenceInVREvent(const FGuid* InEndpointId = nullptr);

	/** Handle a presence in VR update from another client session */
	void HandleConcertClientPresenceInVREvent(const FConcertSessionContext& InSessionContext, const FConcertClientPresenceInVREvent& InEvent);

	/** Updates presence avatar for remote client by invalidating current presence actor */
	void UpdatePresenceAvatar(const FGuid& InEndpointId, FName VRDevice);

	/** Set presence PIE state */
	void SetPresenceInPIE(const FGuid& InEndpointId, bool bInPIE);

	/** Set presence visibility */
	void SetPresenceVisibility(const FGuid& InEndpointId, bool bVisibility, bool bPropagateToAll = false);

	/** Toggle presence visibility */
	void TogglePresenceVisibility(const FGuid& InEndpointId, bool bPropagateToAll = false);

	/** Ensure presence state info for client session */
	FConcertClientPresenceState& EnsurePresenceState(const FGuid& InEndpointId);

	/** Build presence specific UI in the Concert Browser */
	void BuildPresenceClientUI(const FConcertSessionClientInfo& InClientInfo, TArray<FConcertUIButtonDefinition>& OutButtonDefs);

	/** Is the jump-to button enabled for the given endpoint? */
	bool IsJumpToPresenceEnabled(FGuid InEndpointId) const;

	/** Handle the show/hide button being clicked for the given endpoint */
	FReply OnJumpToPresenceClicked(FGuid InEndpointId);

	/** Is the show/hide button enabled for the given endpoint? */
	bool IsShowHidePresenceEnabled(FGuid InEndpointId) const;

	/** Get the correct show/hide text for the given endpoint */
	FText GetShowHidePresenceText(FGuid InEndpointId) const;

	/** Get the correct show/hide tooltip for the given endpoint */
	FText GetShowHidePresenceToolTip(FGuid InEndpointId) const;

	/** Handle the show/hide button being clicked for the given endpoint */
	FReply OnShowHidePresenceClicked(FGuid InEndpointId);

	/** Delegate handle for the end-of-frame notification */
	FDelegateHandle OnEndFrameHandle;

	/** Delegate handle for adding extra buttons to clients in the Concert Browser */
	FDelegateHandle ClientButtonExtensionHandle;

	/** Delegate handle invoked when a client session connects or disconnects */
	FDelegateHandle OnSessionClientChangedHandle;

	/** Delegate handle invoked when entering VR */
	FDelegateHandle OnVREditingModeEnterHandle;

	/** Delegate handle invoked when exiting VR */
	FDelegateHandle OnVREditingModeExitHandle;

	/** Session Pointer */
	TSharedRef<IConcertClientSession> Session; 

	/** Presence avatar mode for this client */
	TUniquePtr<FConcertClientBasePresenceMode> CurrentAvatarMode;

	/** The asset container path */
	static const TCHAR* AssetContainerPath;

	/** Container of assets */
	class UConcertAssetContainer* AssetContainer;

	/** True if presence is currently enabled and should be shown (unless hidden by other settings) */
	bool bIsPresenceEnabled;

	/** NAME_None if not in VR */
	FName VRDeviceType;

	/** Avatar actor class */
	UClass* CurrentAvatarActorClass;

	/** Desktop avatar actor class */
	UClass* DesktopAvatarActorClass;

	/** VR avatar actor class */
	UClass* VRAvatarActorClass;

	/** Presence state associated with remote client id */
	TMap<FGuid, FConcertClientPresenceState> PresenceStateMap;

	/** Presence state associated with client display name */
	TMap<FString, FConcertClientPresencePersistentState> PresencePersistentStateMap;

	/** Time of previous call to OnEndFrame */
	double PreviousEndFrameTime;

	/** Time since last location update for this client */
	double SecondsSinceLastLocationUpdate;
};

#else

namespace FConcertClientPresenceManager
{
	/** Get location update frequency */
	double GetLocationUpdateFrequency();
}

#endif

