// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemTypes.h"
#include "Engine/EngineBaseTypes.h"

#include "Party/PartyTypes.h"
#include "Interfaces/OnlinePartyInterface.h"
#include "Interactions/SocialInteractionHandle.h"

#include "SocialManager.generated.h"

class ULocalPlayer;
class USocialUser;
class USocialParty;
class USocialToolkit;
class UGameViewportClient;
class UGameInstance;
class FOnlineSessionSearchResult;
class FPartyPlatformSessionManager;

enum ETravelType;

/** Singleton manager at the top of the social framework */
UCLASS(Within = GameInstance, Config = Game)
class PARTY_API USocialManager : public UObject
{
	GENERATED_BODY()

public:
	static bool IsSocialSubsystemEnabled(ESocialSubsystem SubsystemType);
	static FName GetSocialOssName(ESocialSubsystem SubsystemType);
	static IOnlineSubsystem* GetSocialOss(UWorld* World, ESocialSubsystem SubsystemType);
	static FUserPlatform GetLocalUserPlatform();
	static const TArray<ESocialSubsystem>& GetDefaultSubsystems() { return DefaultSubsystems; }
	static const TArray<FSocialInteractionHandle>& GetRegisteredInteractions() { return RegisteredInteractions; }

	USocialManager();
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	/** Initializes the manager - call this right after creating the manager object during GameInstance initialization. */
	virtual void InitSocialManager();
	virtual void ShutdownSocialManager();

	USocialToolkit& GetSocialToolkit(ULocalPlayer& LocalPlayer) const;
	USocialToolkit* GetFirstLocalUserToolkit() const;
	FUniqueNetIdRepl GetFirstLocalUserId(ESocialSubsystem SubsystemType) const;
	int32 GetFirstLocalUserNum() const;

	DECLARE_EVENT_OneParam(USocialManager, FOnSocialToolkitCreated, USocialToolkit&)
	FOnSocialToolkitCreated& OnSocialToolkitCreated() const { return OnSocialToolkitCreatedEvent; }
	
	DECLARE_EVENT_OneParam(USocialManager, FOnPartyMembershipChanged, USocialParty&);
	FOnPartyMembershipChanged& OnPartyJoined() const { return OnPartyJoinedEvent; }

	DECLARE_DELEGATE_OneParam(FOnCreatePartyAttemptComplete, ECreatePartyCompletionResult);
	void CreateParty(const FOnlinePartyTypeId& PartyTypeId, const FPartyConfiguration& PartyConfig, const FOnCreatePartyAttemptComplete& OnCreatePartyComplete);
	void CreatePersistentParty(const FOnCreatePartyAttemptComplete& OnCreatePartyComplete = FOnCreatePartyAttemptComplete());

	bool IsPartyJoinInProgress(const FOnlinePartyTypeId& TypeId) const;
	bool IsPersistentPartyJoinInProgress() const;

	template <typename PartyT = USocialParty>
	PartyT* GetPersistentParty() const
	{
		return Cast<PartyT>(GetPersistentPartyInternal());
	}

	template <typename PartyT = USocialParty>
	PartyT* GetParty(const FOnlinePartyTypeId& PartyTypeId) const
	{
		return Cast<PartyT>(GetPartyInternal(PartyTypeId));
	}

	template <typename PartyT = USocialParty>
	PartyT* GetParty(const FOnlinePartyId& PartyId) const
	{
		return Cast<PartyT>(GetPartyInternal(PartyId));
	}

	bool IsConnectedToPartyService() const { return bIsConnectedToPartyService; }

PARTY_SCOPE:
	/** Validates that the target user has valid join info for us to use and that we can join any party of the given type */
	FJoinPartyResult ValidateJoinTarget(const USocialUser& UserToJoin, const FOnlinePartyTypeId& PartyTypeId) const;
	
	DECLARE_DELEGATE_OneParam(FOnJoinPartyAttemptComplete, const FJoinPartyResult&);
	void JoinParty(const USocialUser& UserToJoin, const FOnlinePartyTypeId& PartyTypeId, const FOnJoinPartyAttemptComplete& OnJoinPartyComplete);

	void NotifyPartyInitialized(USocialParty& Party);
	USocialToolkit* GetSocialToolkit(int32 LocalPlayerNum) const;
	
protected:
	struct PARTY_API FRejoinableParty : public TSharedFromThis<FRejoinableParty>
	{
		FRejoinableParty(const USocialParty& SourceParty);

		TSharedRef<const FOnlinePartyId> PartyId;
		TArray<TSharedRef<const FUniqueNetId>> MemberIds;
	};

	struct PARTY_API FJoinPartyAttempt
	{
		FJoinPartyAttempt(TSharedRef<const FRejoinableParty> InRejoinInfo);
		FJoinPartyAttempt(const USocialUser* InTargetUser, const FOnlinePartyTypeId& InPartyTypeId, const FOnJoinPartyAttemptComplete& InOnJoinComplete);

		FString ToDebugString() const;

		TWeakObjectPtr<const USocialUser> TargetUser;
		FOnlinePartyTypeId PartyTypeId;
		FUniqueNetIdRepl TargetUserPlatformId;
		FSessionId PlatformSessionId;

		TSharedPtr<const FRejoinableParty> RejoinInfo;
		TSharedPtr<const IOnlinePartyJoinInfo> JoinInfo;

		FOnJoinPartyAttemptComplete OnJoinComplete;

		static const FName Step_FindPlatformSession;
		static const FName Step_QueryJoinability;
		static const FName Step_LeaveCurrentParty;
		static const FName Step_JoinParty;
		static const FName Step_DeferredPartyCreation;

		FSocialActionTimeTracker ActionTimeTracker;
	};

	virtual void RegisterSocialInteractions();

	/** Validate that we are clear to try joining a party of the given type. If not, gives the reason why. */
	virtual FJoinPartyResult ValidateJoinAttempt(const FOnlinePartyTypeId& PartyTypeId) const;

	/**
	 * Gives child classes a chance to append any additional data to a join request that's about to be sent to another party.
	 * This is where you'll add game-specific information that can affect whether you are eligible for the target party.
	 */
	virtual void FillOutJoinRequestData(const FOnlinePartyId& TargetParty, FOnlinePartyData& OutJoinRequestData) const;

	virtual TSubclassOf<USocialParty> GetPartyClassForType(const FOnlinePartyTypeId& PartyTypeId) const;

	//virtual void OnCreatePartyComplete(const TSharedPtr<const FOnlinePartyId>& PartyId, ECreatePartyCompletionResult Result, FOnlinePartyTypeId PartyTypeId) {}
	//virtual void OnQueryJoinabilityComplete(const FOnlinePartyId& PartyId, EJoinPartyCompletionResult Result, int32 DeniedResultCode, FOnlinePartyTypeId PartyTypeId) {}

	virtual void OnJoinPartyAttemptCompleteInternal(const FJoinPartyAttempt& JoinAttemptInfo, const FJoinPartyResult& Result);
	virtual void OnPartyLeftInternal(USocialParty& LeftParty, EMemberExitedReason Reason) {}
	virtual void OnToolkitCreatedInternal(USocialToolkit& NewToolkit);

	virtual bool CanCreateNewPartyObjects() const;

	/** Up to the game to decide whether it wants to allow crossplay (generally based on a user setting of some kind) */
	virtual ECrossplayPreference GetCrossplayPreference() const;

	virtual bool ShouldTryRejoiningPersistentParty(const FRejoinableParty& InRejoinableParty) const;

	template <typename InteractionT>
	void RegisterInteraction()
	{
		RegisteredInteractions.Add(InteractionT::GetHandle());
	}

	void RefreshCanCreatePartyObjects();

	USocialParty* GetPersistentPartyInternal(bool bEvenIfLeaving = false) const;
	const FJoinPartyAttempt* GetJoinAttemptInProgress(const FOnlinePartyTypeId& PartyTypeId) const;

	//@todo DanH: TEMP - for now relying on FN to bind to its game-level UFortOnlineSessionClient instance #required
	void HandlePlatformSessionInviteAccepted(const TSharedRef<const FUniqueNetId>& LocalUserId, const FOnlineSessionSearchResult& InviteResult);

	/** Info on the persistent party we were in when losing connection to the party service and want to rejoin when it returns */
	TSharedPtr<FRejoinableParty> RejoinableParty;

	/** The desired type of SocialToolkit to create for each local player */
	TSubclassOf<USocialToolkit> ToolkitClass;

private:
	UGameInstance& GetGameInstance() const;
	USocialToolkit& CreateSocialToolkit(ULocalPlayer& OwningLocalPlayer);

	void QueryPartyJoinabilityInternal(FJoinPartyAttempt& JoinAttempt);
	void JoinPartyInternal(FJoinPartyAttempt& JoinAttempt);
	void FinishJoinPartyAttempt(FJoinPartyAttempt& JoinAttemptToDestroy, const FJoinPartyResult& JoinResult);
	
	USocialParty* EstablishNewParty(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FOnlinePartyTypeId& PartyTypeId);

	USocialParty* GetPartyInternal(const FOnlinePartyTypeId& PartyTypeId, bool bIncludeLeavingParties = false) const;
	USocialParty* GetPartyInternal(const FOnlinePartyId& PartyId, bool bIncludeLeavingParties = false) const;

	TSharedPtr<IOnlinePartyJoinInfo> GetJoinInfoFromSession(const FOnlineSessionSearchResult& PlatformSession);

private:	// Handlers
	void HandleGameViewportInitialized();
	void HandlePreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel);
	void HandleWorldEstablished(UWorld* World);
	void HandleLocalPlayerAdded(int32 LocalUserNum);
	void HandleLocalPlayerRemoved(int32 LocalUserNum);

	void HandleQueryJoinabilityComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, EJoinPartyCompletionResult Result, int32 NotApprovedReasonCode, FOnlinePartyTypeId PartyTypeId);
	void HandleCreatePartyComplete(const FUniqueNetId& LocalUserId, const TSharedPtr<const FOnlinePartyId>& PartyId, ECreatePartyCompletionResult Result, FOnlinePartyTypeId PartyTypeId, FOnCreatePartyAttemptComplete CompletionDelegate);
	void HandleJoinPartyComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, EJoinPartyCompletionResult Result, int32 NotApprovedReasonCode, FOnlinePartyTypeId PartyTypeId);
	
	void HandlePersistentPartyStateChanged(EPartyState NewState, USocialParty* PersistentParty);
	void HandleLeavePartyForJoinComplete(ELeavePartyCompletionResult LeaveResult, USocialParty* LeftParty);
	void HandlePartyLeaveBegin(EMemberExitedReason Reason, USocialParty* LeavingParty);
	void HandlePartyLeft(EMemberExitedReason Reason, USocialParty* LeftParty);

	void HandleLeavePartyForMissingJoinAttempt(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, ELeavePartyCompletionResult LeaveResult, FOnlinePartyTypeId PartyTypeId);

	void HandleFillPartyJoinRequestData(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, FOnlinePartyData& PartyData);
	void HandleFindSessionForJoinComplete(bool bWasSuccessful, const FOnlineSessionSearchResult& FoundSession, FOnlinePartyTypeId PartyTypeId);

private:
	static TArray<ESocialSubsystem> DefaultSubsystems;
	static TArray<FSocialInteractionHandle> RegisteredInteractions;
	static TMap<TWeakObjectPtr<UGameInstance>, TWeakObjectPtr<USocialManager>> AllManagersByGameInstance;

	UPROPERTY()
	TArray<USocialToolkit*> SocialToolkits;

	bool bIsConnectedToPartyService = false;
	
	/**
	 * False during brief windows where the game isn't in a state conducive to creating a new party object and after the manager is completely shut down (prior to being GC'd)
	 * Tracked to allow OSS level party activity to execute immediately, but hold off on establishing our local (and replicated) awareness of the party until this client is ready.
	 */
	bool bCanCreatePartyObjects = false;

	TSharedPtr<FPartyPlatformSessionManager> PartySessionManager;

	TMap<FOnlinePartyTypeId, USocialParty*> JoinedPartiesByTypeId;
	TMap<FOnlinePartyTypeId, USocialParty*> LeavingPartiesByTypeId;
	TMap<FOnlinePartyTypeId, FJoinPartyAttempt> JoinAttemptsByTypeId;

	FDelegateHandle OnFillJoinRequestInfoHandle;

	mutable FOnSocialToolkitCreated OnSocialToolkitCreatedEvent;
	mutable FOnPartyMembershipChanged OnPartyJoinedEvent;
};