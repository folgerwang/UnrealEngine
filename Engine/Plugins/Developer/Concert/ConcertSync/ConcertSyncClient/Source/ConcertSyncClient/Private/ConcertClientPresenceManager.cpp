// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertClientPresenceManager.h"
#include "CoreMinimal.h"
#include "IConcertSession.h"
#include "IConcertSessionHandler.h"
#include "ConcertClientPresenceActor.h"
#include "ConcertWorkspaceMessages.h"
#include "ConcertSyncArchives.h"
#include "ConcertSyncSettings.h"
#include "IConcertClient.h"
#include "IConcertModule.h"
#include "IConcertUICoreModule.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Modules/ModuleManager.h"
#include "ConcertLogGlobal.h"
#include "ConcertClientDesktopPresenceActor.h"
#include "ConcertClientVRPresenceActor.h"
#include "ConcertTransactionLedger.h"
#include "Scratchpad/ConcertScratchpad.h"
#include "GameFramework/PlayerController.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorFontGlyphs.h"
#include "EditorWorldExtension.h"
#include "UnrealEdMisc.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "LevelEditor.h"
#include "ILevelViewport.h"
#include "SLevelViewport.h"
#include "ConcertAssetContainer.h"
#include "IVREditorModule.h"
#include "VREditorMode.h"
#include "Framework/Application/SlateApplication.h"
#endif

#define LOCTEXT_NAMESPACE "ConcertPresenceManager"

namespace ConcertClientPresenceManagerUtil
{
	// Update frequency 15 Hz
	const double LocationUpdateFrequencySeconds = 0.0667;
}

#if	WITH_EDITOR

namespace ConcertClientPresenceManagerUtil
{

bool ShowPresenceInPIE(const bool InIsPIE)
{
	return !InIsPIE || GetDefault<UConcertSyncConfig>()->bShowPresenceInPIE;
}

}

static TAutoConsoleVariable<int32> CVarEnablePresence(TEXT("concert.EnablePresence"), 1, TEXT("Enable Concert Presence"));

const TCHAR* FConcertClientPresenceManager::AssetContainerPath = TEXT("/ConcertSyncClient/ConcertAssets");

FConcertClientPresenceManager::FConcertClientPresenceManager(TSharedRef<IConcertClientSession> InSession)
	: OnSessionClientChangedHandle()
	, OnVREditingModeEnterHandle()
	, OnVREditingModeExitHandle()
	, Session(InSession)
	, CurrentAvatarMode(nullptr)
	, AssetContainer(nullptr)
	, bIsPresenceEnabled(true)
	, VRDeviceType(NAME_None)
	, CurrentAvatarActorClass(nullptr)
	, DesktopAvatarActorClass(nullptr)
	, VRAvatarActorClass(nullptr)
{
	// Setup the asset container.
	AssetContainer = LoadObject<UConcertAssetContainer>(nullptr, FConcertClientPresenceManager::AssetContainerPath);
	checkf(AssetContainer, TEXT("Failed to load UConcertAssetContainer (%s). See log for reason."), FConcertClientPresenceManager::AssetContainerPath);

	// @todo - Need to handle the situation where the avatar class might change during a session.
	// This makes the assumption that avatar class will not change during a session 
	// but will cause issues if it does because remote clients will create a 
	// new presence actor but this manager will send updates for the old actor type.
	FSoftClassPath DesktopAvatarActorClassPath = IConcertModule::Get().GetClientInstance()->GetClientInfo().DesktopAvatarActorClass;
	DesktopAvatarActorClass = LoadObject<UClass>(nullptr, *DesktopAvatarActorClassPath.ToString());

	FSoftClassPath VRAvatarActorClassPath = IConcertModule::Get().GetClientInstance()->GetClientInfo().VRAvatarActorClass;
	VRAvatarActorClass = LoadObject<UClass>(nullptr, *VRAvatarActorClassPath.ToString());

	CurrentAvatarActorClass = DesktopAvatarActorClass;

	PreviousEndFrameTime = FPlatformTime::Seconds();
	SecondsSinceLastLocationUpdate = ConcertClientPresenceManagerUtil::LocationUpdateFrequencySeconds;

	Register();
}

FConcertClientPresenceManager::~FConcertClientPresenceManager()
{
	Unregister();
	ClearAllPresenceState();
}

void FConcertClientPresenceManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(AssetContainer);
	Collector.AddReferencedObject(CurrentAvatarActorClass);
	Collector.AddReferencedObject(DesktopAvatarActorClass);
	Collector.AddReferencedObject(VRAvatarActorClass);
}

const UConcertAssetContainer& FConcertClientPresenceManager::GetAssetContainer() const
{
	return *AssetContainer;
}

bool FConcertClientPresenceManager::IsPresenceVisible(const FConcertClientPresenceState& InPresenceState) const
{
	return InPresenceState.bVisible && ConcertClientPresenceManagerUtil::ShowPresenceInPIE(InPresenceState.bInPIE);
}

bool FConcertClientPresenceManager::IsPresenceVisible(const FGuid& InEndpointId) const
{
	const FConcertClientPresenceState* PresenceState = PresenceStateMap.Find(InEndpointId);
	return PresenceState && IsPresenceVisible(*PresenceState);
}

template<class PresenceActorClass, typename PresenceUpdateEventType>
void FConcertClientPresenceManager::UpdatePresence(AConcertClientPresenceActor* InPresenceActor, const PresenceUpdateEventType& InEvent)
{
	if (InPresenceActor)
	{
		if (PresenceActorClass* PresenceActor = Cast<PresenceActorClass>(InPresenceActor))
		{
			PresenceActor->HandleEvent(InEvent);
		}
	}
}

template<typename PresenceUpdateEventType>
void FConcertClientPresenceManager::HandleConcertClientPresenceUpdateEvent(const FConcertSessionContext& InSessionContext, const PresenceUpdateEventType& InEvent)
{
	// FName EventTypeName = PresenceUpdateEventType::StaticStruct()->GetFName();
	//const FConcertClientPresenceEventBase& Event = InEvent;

	if (!ShouldProcessPresenceEvent(InSessionContext, PresenceUpdateEventType::StaticStruct(), InEvent))
	{
		UE_LOG(LogConcert, VeryVerbose, TEXT("Dropping presence update event for '%s' (index %d) as it arrived out-of-order"), *InSessionContext.SourceEndpointId.ToString(), InEvent.TransactionUpdateIndex);
		return;
	}

	FConcertClientPresenceState& PresenceState = EnsurePresenceState(InSessionContext.SourceEndpointId);

	TSharedRef<PresenceUpdateEventType> EventRef = MakeShared<PresenceUpdateEventType>(InEvent);
	FConcertClientPresenceStateEntry StateEntry(EventRef);
	PresenceState.EventStateMap.Emplace(PresenceUpdateEventType::StaticStruct(), MoveTemp(StateEntry));
}

void FConcertClientPresenceManager::Register()
{
	Session->RegisterCustomEventHandler<FConcertClientPresenceVisibilityUpdateEvent>(this, &FConcertClientPresenceManager::HandleConcertClientPresenceVisibilityUpdateEvent);
	Session->RegisterCustomEventHandler<FConcertClientPresenceInVREvent>(this, &FConcertClientPresenceManager::HandleConcertClientPresenceInVREvent);
	Session->RegisterCustomEventHandler<FConcertClientPresenceDataUpdateEvent>(this, &FConcertClientPresenceManager::HandleConcertClientPresenceUpdateEvent<FConcertClientPresenceDataUpdateEvent>);
	Session->RegisterCustomEventHandler<FConcertClientDesktopPresenceUpdateEvent>(this, &FConcertClientPresenceManager::HandleConcertClientPresenceUpdateEvent<FConcertClientDesktopPresenceUpdateEvent>);
	Session->RegisterCustomEventHandler<FConcertClientVRPresenceUpdateEvent>(this, &FConcertClientPresenceManager::HandleConcertClientPresenceUpdateEvent<FConcertClientVRPresenceUpdateEvent>);
	Session->RegisterCustomEventHandler<FConcertPlaySessionEvent>(this, &FConcertClientPresenceManager::HandleConcertPlaySessionEvent);

	// Add handler for session client changing
	OnSessionClientChangedHandle = Session->OnSessionClientChanged().AddRaw(this, &FConcertClientPresenceManager::OnSessionClientChanged);

	// Add handler for VR mode
	OnVREditingModeEnterHandle = IVREditorModule::Get().OnVREditingModeEnter().AddRaw(this, &FConcertClientPresenceManager::OnVREditingModeEnter);
	OnVREditingModeExitHandle = IVREditorModule::Get().OnVREditingModeExit().AddRaw(this, &FConcertClientPresenceManager::OnVREditingModeExit);	

	// Register OnEndFrame events
	OnEndFrameHandle = FCoreDelegates::OnEndFrame.AddRaw(this, &FConcertClientPresenceManager::OnEndFrame);

	ClientButtonExtensionHandle = IConcertUICoreModule::Get().GetConcertBrowserClientButtonExtension().AddRaw(this, &FConcertClientPresenceManager::FConcertClientPresenceManager::BuildPresenceClientUI);
}

void FConcertClientPresenceManager::Unregister()
{
	Session->OnSessionClientChanged().Remove(OnSessionClientChangedHandle);

	Session->UnregisterCustomEventHandler<FConcertClientPresenceVisibilityUpdateEvent>();
	Session->UnregisterCustomEventHandler<FConcertClientPresenceInVREvent>();
	Session->UnregisterCustomEventHandler<FConcertClientPresenceDataUpdateEvent>();
	Session->UnregisterCustomEventHandler<FConcertClientDesktopPresenceUpdateEvent>();
	Session->UnregisterCustomEventHandler<FConcertClientVRPresenceUpdateEvent>();
	Session->UnregisterCustomEventHandler<FConcertPlaySessionEvent>();

	if (ClientButtonExtensionHandle.IsValid())
	{
		IConcertUICoreModule::Get().GetConcertBrowserClientButtonExtension().Remove(ClientButtonExtensionHandle);
		ClientButtonExtensionHandle.Reset();
	}

	if (OnVREditingModeEnterHandle.IsValid())
	{
		IVREditorModule::Get().OnVREditingModeEnter().Remove(OnVREditingModeEnterHandle);
		OnVREditingModeEnterHandle.Reset();
	}

	if (OnVREditingModeExitHandle.IsValid())
	{
		IVREditorModule::Get().OnVREditingModeExit().Remove(OnVREditingModeExitHandle);
		OnVREditingModeExitHandle.Reset();
	}

	if (OnEndFrameHandle.IsValid())
	{
		FCoreDelegates::OnEndFrame.Remove(OnEndFrameHandle);
		OnEndFrameHandle.Reset();
	}
}

UWorld* FConcertClientPresenceManager::GetWorld() const
{
	check(GEditor);

	if (FWorldContext* WorldContext = GEditor->GetPIEWorldContext())
	{
		return WorldContext->World();
	}

	return GEditor->GetEditorWorldContext().World();
}

FLevelEditorViewportClient* FConcertClientPresenceManager::GetPerspectiveViewport() const
{
	return (GCurrentLevelEditingViewportClient && GCurrentLevelEditingViewportClient->IsPerspective())
		? GCurrentLevelEditingViewportClient
		: nullptr;
}

void FConcertClientPresenceManager::OnEndFrame()
{
	const double CurrentTime = FPlatformTime::Seconds();

	double DeltaTime = CurrentTime - PreviousEndFrameTime;
	SecondsSinceLastLocationUpdate += DeltaTime;

	if (SecondsSinceLastLocationUpdate >= ConcertClientPresenceManagerUtil::LocationUpdateFrequencySeconds)
	{
		if (!CurrentAvatarMode)
		{
			CurrentAvatarMode = FConcertClientBasePresenceMode::CreatePresenceMode(CurrentAvatarActorClass, this);
		}

		// Send our current presence data to remote clients
		if (CurrentAvatarMode)
		{
			CurrentAvatarMode->SendEvents(*Session);
		}

		SecondsSinceLastLocationUpdate = 0.0;
	}
	
	PreviousEndFrameTime = CurrentTime;

	// Synchronize our local state for each remote client
	SynchronizePresenceState();
}

const TSharedPtr<FConcertClientPresenceDataUpdateEvent> FConcertClientPresenceManager::GetCachedPresenceState(const FConcertClientPresenceState& InPresenceState) const
{
	const FConcertClientPresenceStateEntry* StateItem = InPresenceState.EventStateMap.Find(FConcertClientPresenceDataUpdateEvent::StaticStruct());
	if (StateItem)
	{
		return StaticCastSharedRef<FConcertClientPresenceDataUpdateEvent>(StateItem->PresenceEvent);
	}

	return nullptr;
}

const TSharedPtr<FConcertClientPresenceDataUpdateEvent> FConcertClientPresenceManager::GetCachedPresenceState(const FGuid& InEndpointId) const
{
	const FConcertClientPresenceState* PresenceState = PresenceStateMap.Find(InEndpointId);
	return PresenceState ? GetCachedPresenceState(*PresenceState) : nullptr;
}

void FConcertClientPresenceManager::SynchronizePresenceState()
{
	const FName ActiveWorldPathName = *GetWorld()->GetPathName();

	// Process all pending state updates
	for (auto It = PresenceStateMap.CreateIterator(); It; ++It)
	{
		const FGuid& RemoteEndpointId = It.Key();
		FConcertClientPresenceState& PresenceState = It.Value();

		// Find presence world
		FName EventWorldPathName;
		const TSharedPtr<FConcertClientPresenceDataUpdateEvent> PresenceUpdateEvent = GetCachedPresenceState(PresenceState);
		EventWorldPathName = PresenceUpdateEvent.IsValid() ? PresenceUpdateEvent->WorldPath : TEXT("");

		const bool bIsValidViewport = GetPerspectiveViewport() != nullptr;
		const bool bInCurrentWorld = !ActiveWorldPathName.IsNone() && ActiveWorldPathName == EventWorldPathName;
		
		const bool bShowPresence = bIsPresenceEnabled && bIsValidViewport && bInCurrentWorld && PresenceState.bIsConnected && IsPresenceVisible(PresenceState) && (CVarEnablePresence.GetValueOnAnyThread() > 0);
		if (bShowPresence)
		{
			FConcertSessionClientInfo ClientSessionInfo;
			Session->FindSessionClient(RemoteEndpointId, ClientSessionInfo);
			if (!PresenceState.PresenceActor.IsValid())
			{
				PresenceState.PresenceActor = CreatePresenceActor(ClientSessionInfo.ClientInfo, PresenceState.VRDevice);
			}

			if (PresenceState.PresenceActor.IsValid())
			{
				AConcertClientPresenceActor* PresenceActor = PresenceState.PresenceActor.Get();

				for (auto StateIt = PresenceState.EventStateMap.CreateIterator(); StateIt; ++StateIt)
				{
					const UScriptStruct* EventKey = StateIt.Key();
					FConcertClientPresenceStateEntry& EventItem = StateIt.Value();

					if (EventItem.bSyncPending)
					{
						FStructOnScope Event(EventKey, (uint8*)&EventItem.PresenceEvent.Get());
						PresenceActor->HandleEvent(Event);
						EventItem.bSyncPending = false;
					}
				}
			}
		}
		else
		{
			ClearPresenceActor(RemoteEndpointId);
		}

		if (!PresenceState.bIsConnected)
		{
			It.RemoveCurrent();
		}
	}
}

bool FConcertClientPresenceManager::ShouldProcessPresenceEvent(const FConcertSessionContext& InSessionContext, const UStruct* InEventType, const FConcertClientPresenceEventBase& InEvent) const
{
	check(InEventType);

	const FName EventId = *FString::Printf(TEXT("PresenceManager.%s.EndpointId:%s"), *InEventType->GetFName().ToString(), *InSessionContext.SourceEndpointId.ToString());

	FConcertScratchpadPtr SenderScratchpad = Session->GetClientScratchpad(InSessionContext.SourceEndpointId);
	if (SenderScratchpad.IsValid())
	{
		// If the event isn't required, then we can drop it if its update index is older than the last update we processed
		if (uint32* EventUpdateIndexPtr = Session->GetScratchpad()->GetValue<uint32>(EventId))
		{
			uint32& EventUpdateIndex = *EventUpdateIndexPtr;
			const bool bShouldProcess = InEvent.TransactionUpdateIndex >= EventUpdateIndex + 1; // Note: We +1 before doing the check to handle overflow
			EventUpdateIndex = InEvent.TransactionUpdateIndex;
			return bShouldProcess;
		}

		// First update for this transaction, just process it
		SenderScratchpad->SetValue<uint32>(EventId, InEvent.TransactionUpdateIndex);
		return true;
	}

	return true;
}

AConcertClientPresenceActor* FConcertClientPresenceManager::CreatePresenceActor(const FConcertClientInfo& InClientInfo, FName VRDevice)
{
	AConcertClientPresenceActor* PresenceActor = SpawnPresenceActor(InClientInfo, VRDevice);

	if (PresenceActor)
	{
		PresenceActor->SetPresenceName(InClientInfo.DisplayName);
		PresenceActor->SetPresenceColor(InClientInfo.AvatarColor);
	}

	return PresenceActor;
}

AConcertClientPresenceActor* FConcertClientPresenceManager::SpawnPresenceActor(const FConcertClientInfo& InClientInfo, FName VRDevice)
{
	check(AssetContainer);

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogConcert, Warning, TEXT("No world active. Presence will not be displayed"));
		return nullptr;
	}

	// @todo this is potentially slow and hitchy as clients connect.  It might be better to preload all the presence actor types
	UClass* PresenceActorClass = nullptr;	
	if (!VRDevice.IsNone())
	{
		PresenceActorClass = LoadObject<UClass>(nullptr, *InClientInfo.VRAvatarActorClass);
	}
	else
	{
		PresenceActorClass = LoadObject<UClass>(nullptr, *InClientInfo.DesktopAvatarActorClass);
	}

	if (!PresenceActorClass)
	{
		UE_LOG(LogConcert, Warning, TEXT("Failed to load presence actor class '%s'. Presence will not be displayed"), !VRDevice.IsNone() ? *InClientInfo.VRAvatarActorClass : *InClientInfo.DesktopAvatarActorClass);
		return nullptr;
	}

	AConcertClientPresenceActor* PresenceActor = nullptr;
	{
		const bool bWasWorldPackageDirty = World->GetOutermost()->IsDirty();

		FActorSpawnParameters ActorSpawnParameters;
		ActorSpawnParameters.Name = MakeUniqueObjectName(World, PresenceActorClass, PresenceActorClass->GetFName()); // @todo how should spawned actors be named?
		ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ActorSpawnParameters.ObjectFlags = EObjectFlags::RF_DuplicateTransient;
		ActorSpawnParameters.bDeferConstruction = true;

		PresenceActor = World->SpawnActor<AConcertClientPresenceActor>(PresenceActorClass, ActorSpawnParameters);

		// Don't dirty the level file after spawning a transient actor
		if (!bWasWorldPackageDirty)
		{
			World->GetOutermost()->SetDirtyFlag(false);
		}
	}

	if (!PresenceActor)
	{
		UE_LOG(LogConcert, Warning, TEXT("Failed to spawn presence actor of class '%s'. Presence will not be displayed"), !VRDevice.IsNone() ? *InClientInfo.VRAvatarActorClass : *InClientInfo.DesktopAvatarActorClass);
		return nullptr;
	}

	// Setup the asset container.
	PresenceActor->InitPresence(*AssetContainer, VRDevice);
	{
		FEditorScriptExecutionGuard UCSGuard;
		PresenceActor->FinishSpawning(FTransform(), true);
	}

	return PresenceActor;
}

void FConcertClientPresenceManager::ClearPresenceActor(const FGuid& InEndpointId)
{
	FConcertClientPresenceState* PresenceState = PresenceStateMap.Find(InEndpointId);
	if (PresenceState)
	{
		DestroyPresenceActor(PresenceState->PresenceActor);
		PresenceState->PresenceActor.Reset();
	}
}

void FConcertClientPresenceManager::DestroyPresenceActor(TWeakObjectPtr<AConcertClientPresenceActor> InPresenceActor)
{
	if (AConcertClientPresenceActor* PresenceActor = InPresenceActor.Get())
	{
		UWorld* World = PresenceActor->GetWorld();
		const bool bWasWorldPackageDirty = World->GetOutermost()->IsDirty();

		const bool bNetForce = false;
		const bool bShouldModifyLevel = false;	// Don't modify level for transient actor destruction
		World->DestroyActor(PresenceActor, bNetForce, bShouldModifyLevel);

		// Don't dirty the level file after destroying a transient actor
		if (!bWasWorldPackageDirty)
		{
			World->GetOutermost()->SetDirtyFlag(false);
		}
	}
}

void FConcertClientPresenceManager::ClearAllPresenceState()
{
	for (auto& Elem : PresenceStateMap)
	{
		DestroyPresenceActor(Elem.Value.PresenceActor);
	}
	PresenceStateMap.Empty();
}

void FConcertClientPresenceManager::HandleConcertClientPresenceVisibilityUpdateEvent(const FConcertSessionContext& InSessionContext, const FConcertClientPresenceVisibilityUpdateEvent& InEvent)
{
	SetPresenceVisibility(InEvent.ModifiedEndpointId, InEvent.bVisibility);
}

void FConcertClientPresenceManager::HandleConcertPlaySessionEvent(const FConcertSessionContext& InSessionContext, const FConcertPlaySessionEvent& InEvent)
{
	bool bPIE =  (!InEvent.bIsSimulating && 
					(InEvent.EventType == EConcertPlaySessionEventType::BeginPlay ||
					 InEvent.EventType == EConcertPlaySessionEventType::SwitchPlay));

	// This event is sent by the server so the InSession.SourceEndpointId 
	// will be the server's guid not the client's.
	SetPresenceInPIE(InEvent.PlayEndpointId, bPIE);
}

void FConcertClientPresenceManager::OnSessionClientChanged(IConcertClientSession& InSession, EConcertClientStatus InClientStatus, const FConcertSessionClientInfo& InClientInfo)
{
	if (InClientStatus == EConcertClientStatus::Connected || InClientStatus == EConcertClientStatus::Updated)
	{
		// Sync persistent presence when a client connects or is updated
		if (FConcertClientPresencePersistentState* PresencePersistentState = PresencePersistentStateMap.Find(InClientInfo.ClientInfo.DisplayName))
		{
			SetPresenceVisibility(InClientInfo.ClientEndpointId, PresencePersistentState->bVisible, PresencePersistentState->bPropagateToAll);
		}

		// Send avatar-related info for this client when a remote client connects or is updated
		SendPresenceInVREvent(&InClientInfo.ClientEndpointId);
	}
	else if (InClientStatus == EConcertClientStatus::Disconnected)
	{
		// Disconnect presence when a client disconnects
		if (FConcertClientPresenceState* PresenceState = PresenceStateMap.Find(InClientInfo.ClientEndpointId))
		{
			PresenceState->bIsConnected = false;
		}
	}
}

void FConcertClientPresenceManager::OnVREditingModeEnter()
{
	UVREditorMode* VRMode = IVREditorModule::Get().GetVRMode();
	VRDeviceType = VRMode ? VRMode->GetHMDDeviceType() : FName();
	UpdatePresenceMode();
}

void FConcertClientPresenceManager::OnVREditingModeExit()
{
	VRDeviceType = FName();
	UpdatePresenceMode();
}

void FConcertClientPresenceManager::UpdatePresenceMode()
{
	if ((!VRDeviceType.IsNone() && CurrentAvatarActorClass != VRAvatarActorClass) ||
		(VRDeviceType.IsNone() && CurrentAvatarActorClass != DesktopAvatarActorClass))
	{
		// Mode will get recreated on next call to OnEndFrame
		CurrentAvatarMode.Reset();
		CurrentAvatarActorClass = !VRDeviceType.IsNone() ? VRAvatarActorClass : DesktopAvatarActorClass;
		SendPresenceInVREvent();
	}
}

void FConcertClientPresenceManager::SendPresenceInVREvent(const FGuid* InEndpointId)
{
	FConcertClientPresenceInVREvent Event;
	Event.VRDevice = VRDeviceType;

	if (InEndpointId)
	{
		Session->SendCustomEvent(Event, *InEndpointId, EConcertMessageFlags::ReliableOrdered);
	}
	else
	{
		Session->SendCustomEvent(Event, Session->GetSessionClientEndpointIds(), EConcertMessageFlags::ReliableOrdered);
	}
}

void FConcertClientPresenceManager::HandleConcertClientPresenceInVREvent(const FConcertSessionContext& InSessionContext, const FConcertClientPresenceInVREvent& InEvent)
{
	UpdatePresenceAvatar(InSessionContext.SourceEndpointId, InEvent.VRDevice);
}

void FConcertClientPresenceManager::UpdatePresenceAvatar(const FGuid& InEndpointId, FName VRDevice)
{
	FConcertClientPresenceState& PresenceState = EnsurePresenceState(InEndpointId);
	PresenceState.VRDevice = VRDevice;

	if (PresenceState.PresenceActor.IsValid())
	{
		// Presence actor will be recreated on next call to OnEndFrame
		ClearPresenceActor(InEndpointId);
	}
}

void FConcertClientPresenceManager::SetPresenceInPIE(const FGuid& InEndpointId, bool bPIE)
{
	FConcertClientPresenceState& PresenceState = EnsurePresenceState(InEndpointId);
	PresenceState.bInPIE = bPIE;
}

void FConcertClientPresenceManager::SetPresenceVisibility(const FGuid& InEndpointId, bool bVisibility, bool bPropagateToAll)
{
	FConcertClientPresenceState& PresenceState = EnsurePresenceState(InEndpointId);
	PresenceState.bVisible = bVisibility;

	if (bPropagateToAll)
	{
		FConcertClientPresenceVisibilityUpdateEvent VisibilityUpdateEvent;
		VisibilityUpdateEvent.ModifiedEndpointId = InEndpointId;
		VisibilityUpdateEvent.bVisibility = bVisibility;

		Session->SendCustomEvent(VisibilityUpdateEvent, Session->GetSessionClientEndpointIds(), EConcertMessageFlags::ReliableOrdered);
	}
}

bool FConcertClientPresenceManager::IsInPIE() const
{
	check(GEditor);
	return GEditor->PlayWorld && !GEditor->bIsSimulatingInEditor;
}

void FConcertClientPresenceManager::SetPresenceEnabled(const bool bIsEnabled)
{
	bIsPresenceEnabled = bIsEnabled;
}

void FConcertClientPresenceManager::SetPresenceVisibility(const FString& InDisplayName, bool bVisibility, bool bPropagateToAll)
{
	FConcertClientPresencePersistentState& PresencePersistentState = PresencePersistentStateMap.FindOrAdd(InDisplayName);
	PresencePersistentState.bVisible = bVisibility;
	PresencePersistentState.bPropagateToAll = bPropagateToAll;

	TArray<FGuid, TInlineAllocator<2>> MatchingEndpointIds;
	for (const auto& PresenceStatePair : PresenceStateMap)
	{
		if (PresenceStatePair.Value.DisplayName == InDisplayName)
		{
			MatchingEndpointIds.Add(PresenceStatePair.Key);
		}
	}

	for (const FGuid& MatchingEndpointId : MatchingEndpointIds)
	{
		SetPresenceVisibility(MatchingEndpointId, bVisibility, bPropagateToAll);
	}

	// We also need to propagate a fake visibility change if the display name matches our local 
	// presence data, as that isn't handled by the loop above since we have no local presence
	if (bPropagateToAll && IConcertModule::Get().GetClientInstance()->GetClientInfo().DisplayName == InDisplayName)
	{
		FConcertClientPresenceVisibilityUpdateEvent VisibilityUpdateEvent;
		VisibilityUpdateEvent.ModifiedEndpointId = Session->GetSessionClientEndpointId();
		VisibilityUpdateEvent.bVisibility = bVisibility;

		Session->SendCustomEvent(VisibilityUpdateEvent, Session->GetSessionClientEndpointIds(), EConcertMessageFlags::ReliableOrdered);
	}
}

void FConcertClientPresenceManager::TogglePresenceVisibility(const FGuid& InEndpointId, bool bPropagateToAll)
{
	if (const FConcertClientPresenceState *PresenceState = PresenceStateMap.Find(InEndpointId))
	{
		SetPresenceVisibility(InEndpointId, !PresenceState->bVisible, bPropagateToAll);
	}
}

FConcertClientPresenceState& FConcertClientPresenceManager::EnsurePresenceState(const FGuid& InEndpointId)
{
	FConcertClientPresenceState* PresenceState = PresenceStateMap.Find(InEndpointId);
	if (!PresenceState)
	{
		PresenceState = &PresenceStateMap.Add(InEndpointId);
		{
			FConcertSessionClientInfo ClientSessionInfo;
			if (Session->FindSessionClient(InEndpointId, ClientSessionInfo))
			{
				PresenceState->DisplayName = ClientSessionInfo.ClientInfo.DisplayName;
			}
		}
		PresencePersistentStateMap.FindOrAdd(PresenceState->DisplayName);
	}
	return *PresenceState;
}

void FConcertClientPresenceManager::BuildPresenceClientUI(const FConcertSessionClientInfo& InClientInfo, TArray<FConcertUIButtonDefinition>& OutButtonDefs)
{
	// Only add buttons for the clients in our session
	if (InClientInfo.ClientEndpointId != Session->GetSessionClientEndpointId())
	{
		FConcertSessionClientInfo Unused;
		if (!Session->FindSessionClient(InClientInfo.ClientEndpointId, Unused))
		{
			return;
		}
	}

	FConcertUIButtonDefinition& JumpToPresenceDef = OutButtonDefs.AddDefaulted_GetRef();
	JumpToPresenceDef.IsEnabled = MakeAttributeSP(this, &FConcertClientPresenceManager::IsJumpToPresenceEnabled, InClientInfo.ClientEndpointId);
	JumpToPresenceDef.Text = FEditorFontGlyphs::Map_Marker;
	JumpToPresenceDef.ToolTipText = LOCTEXT("JumpToPresenceToolTip", "Jump to the presence location of this client");
	JumpToPresenceDef.OnClicked.BindSP(this, &FConcertClientPresenceManager::OnJumpToPresenceClicked, InClientInfo.ClientEndpointId);

	FConcertUIButtonDefinition& ShowHidePresenceDef = OutButtonDefs.AddDefaulted_GetRef();
	ShowHidePresenceDef.IsEnabled = MakeAttributeSP(this, &FConcertClientPresenceManager::IsShowHidePresenceEnabled, InClientInfo.ClientEndpointId);
	ShowHidePresenceDef.Text = MakeAttributeSP(this, &FConcertClientPresenceManager::GetShowHidePresenceText, InClientInfo.ClientEndpointId);
	ShowHidePresenceDef.ToolTipText = MakeAttributeSP(this, &FConcertClientPresenceManager::GetShowHidePresenceToolTip, InClientInfo.ClientEndpointId);
	ShowHidePresenceDef.OnClicked.BindSP(this, &FConcertClientPresenceManager::OnShowHidePresenceClicked, InClientInfo.ClientEndpointId);
}

bool FConcertClientPresenceManager::IsJumpToPresenceEnabled(FGuid InEndpointId) const
{
	// Disable this button for ourselves since we don't have presence
	if (InEndpointId == Session->GetSessionClientEndpointId())
	{
		return false;
	}

	// Only enable the button if we have a valid perspective viewport to move and we're not in VR
	if (!GetPerspectiveViewport() || IVREditorModule::Get().IsVREditorModeActive())
	{
		return false;
	}

	// Can only jump to clients that exist, have cached state and both clients are in the same level.
	const TSharedPtr<FConcertClientPresenceDataUpdateEvent> CachedPresenceState = GetCachedPresenceState(InEndpointId);
	if (CachedPresenceState.IsValid())
	{
		// The client should be in the same world to enable teleporting.
		return GetWorld()->GetPathName() == CachedPresenceState->WorldPath.ToString();
	}
	return false;
}

double FConcertClientPresenceManager::GetLocationUpdateFrequency()
{
	return ConcertClientPresenceManagerUtil::LocationUpdateFrequencySeconds;
}

void FConcertClientPresenceManager::InitiateJumpToPresence(FGuid InEndpointId)
{
	OnJumpToPresenceClicked(InEndpointId);
}

FReply FConcertClientPresenceManager::OnJumpToPresenceClicked(FGuid InEndpointId)
{
	const TSharedPtr<FConcertClientPresenceDataUpdateEvent> OtherClientState = GetCachedPresenceState(InEndpointId);
	if (OtherClientState.IsValid())
	{
		FRotator OtherClientRotation(OtherClientState->Orientation.Rotator());

		// Disregard pitch and roll when teleporting to a VR presence.
		if (!VRDeviceType.IsNone())
		{
			OtherClientRotation.Pitch = 0.0f;
			OtherClientRotation.Roll = 0.0f;
		}

		if (IsInPIE())
		{
			check(GEditor->PlayWorld);

			// In 'play in editor', we need to change the 'player' location/orientation.
			if (APlayerController* PC = GEditor->PlayWorld->GetFirstPlayerController())
			{
				PC->ClientSetLocation(OtherClientState->Position, OtherClientRotation);
			}
		}
		else
		{
			FLevelEditorViewportClient* PerspectiveViewport = GetPerspectiveViewport();
			check(PerspectiveViewport);

			PerspectiveViewport->SetViewLocation(OtherClientState->Position);
			PerspectiveViewport->SetViewRotation(OtherClientRotation);
		}
	}

	return FReply::Handled();
}

bool FConcertClientPresenceManager::IsShowHidePresenceEnabled(FGuid InEndpointId) const
{
	// Disable this button for ourselves since we don't have presence
	if (InEndpointId == Session->GetSessionClientEndpointId())
	{
		return false;
	}

	const FConcertClientPresenceState* PresenceState = PresenceStateMap.Find(InEndpointId);
	return PresenceState && ConcertClientPresenceManagerUtil::ShowPresenceInPIE(PresenceState->bInPIE);
}

FText FConcertClientPresenceManager::GetShowHidePresenceText(FGuid InEndpointId) const
{
	return IsPresenceVisible(InEndpointId)
		? FEditorFontGlyphs::Eye
		: FEditorFontGlyphs::Eye_Slash;
}

FText FConcertClientPresenceManager::GetShowHidePresenceToolTip(FGuid InEndpointId) const
{
	return IsPresenceVisible(InEndpointId)
		? LOCTEXT("HidePresenceToolTip", "Hide the presence for this client\nHold Ctrl to propagate this visibility change to all connected clients.")
		: LOCTEXT("ShowPresenceToolTip", "Show the presence for this client\nHold Ctrl to propagate this visibility change to all connected clients.");
}

FReply FConcertClientPresenceManager::OnShowHidePresenceClicked(FGuid InEndpointId)
{
	const bool bPropagateToAll = FSlateApplication::Get().GetModifierKeys().IsControlDown();
	TogglePresenceVisibility(InEndpointId, bPropagateToAll);

	return FReply::Handled();
}

FString FConcertClientPresenceManager::GetClientWorldPath(FGuid InEndpointId) const
{
	// Is it the local client endpoint?
	if (InEndpointId == Session->GetSessionClientEndpointId())
	{
		return GetWorld()->GetPathName();
	}

	// Is it the endpoint of another remote client?
	const TSharedPtr<FConcertClientPresenceDataUpdateEvent> CachedPresenceState = GetCachedPresenceState(InEndpointId);
	if (CachedPresenceState.IsValid())
	{
		return CachedPresenceState->WorldPath.ToString();
	}

	return FString();
}

#else

namespace FConcertClientPresenceManager
{
	double GetLocationUpdateFrequency()
	{
		return ConcertClientPresenceManagerUtil::LocationUpdateFrequencySeconds;
	}
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
