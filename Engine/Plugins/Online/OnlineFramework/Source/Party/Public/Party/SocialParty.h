// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Party/PartyTypes.h"
#include "Party/PartyDataReplicator.h"

#include "PartyBeaconState.h"
#include "Interfaces/OnlinePartyInterface.h"
#include "Interfaces/OnlineChatInterface.h"
#include "Containers/Queue.h"
#include "Engine/EngineBaseTypes.h"

#include "SocialParty.generated.h"

class APartyBeaconClient;
class UNetDriver;

class ULocalPlayer;
class USocialManager;
class USocialUser;

class FOnlineSessionSettings;
class FOnlineSessionSearchResult;
enum class EMemberExitedReason;

/** Base struct used to replicate data about the state of the party to all members. */
USTRUCT()
struct PARTY_API FPartyRepData : public FOnlinePartyRepDataBase
{
	GENERATED_BODY();

public:
	FPartyRepData() {}
	void SetOwningParty(const class USocialParty& InOwnerParty);

	const FPartyPlatformSessionInfo* FindSessionInfo(FName PlatformOssName) const;
	const TArray<FPartyPlatformSessionInfo>& GetPlatformSessions() const { return PlatformSessions; }
	FSimpleMulticastDelegate& OnPlatformSessionsChanged() const { return OnPlatformSessionsChangedEvent; } 

	void UpdatePlatformSessionInfo(const FPartyPlatformSessionInfo& SessionInfo);
	void ClearPlatformSessionInfo(const FName PlatformOssName);

protected:
	virtual bool CanEditData() const override;
	virtual void CompareAgainst(const FOnlinePartyRepDataBase& OldData) const override;
	virtual const USocialParty* GetOwnerParty() const override;

	TWeakObjectPtr<const USocialParty> OwnerParty;

	//@todo DanH Party: Isn't this redundant with the party config itself? Why bother putting it here too when the config replicates to everyone already? #suggested
	/** The privacy settings for the party */
	UPROPERTY()
	FPartyPrivacySettings PrivacySettings;
	EXPOSE_REP_DATA_PROPERTY(FPartyRepData, FPartyPrivacySettings, PrivacySettings);

	/** List of platform sessions for the party. Includes one entry per platform that needs a session and has a member of that session. */
	UPROPERTY()
	TArray<FPartyPlatformSessionInfo> PlatformSessions;

private:
	mutable FSimpleMulticastDelegate OnPlatformSessionsChangedEvent;
};

using FPartyDataReplicator = TPartyDataReplicator<FPartyRepData>;

/**
 * Party game state that contains all information relevant to the communication within a party
 * Keeps all players in sync with the state of the party and its individual members
 */
UCLASS(Abstract, Within=SocialManager, config=Game, Transient)
class PARTY_API USocialParty : public UObject
{
	GENERATED_BODY()

public:
	static bool IsJoiningDuringLoadEnabled();

	USocialParty();

	/** Re-evaluates whether this party is joinable by anyone and, if not, establishes the reason why */
	void RefreshPublicJoinability();

	DECLARE_DELEGATE_OneParam(FOnLeavePartyAttemptComplete, ELeavePartyCompletionResult)
	void LeaveParty(const FOnLeavePartyAttemptComplete& OnLeaveAttemptComplete = FOnLeavePartyAttemptComplete());

	const FPartyRepData& GetRepData() const { return *PartyDataReplicator; }

	template <typename SocialManagerT = USocialManager>
	SocialManagerT& GetSocialManager() const
	{
		SocialManagerT* ManagerOuter = GetTypedOuter<SocialManagerT>();
		check(ManagerOuter);
		return *ManagerOuter;
	}
	
	template <typename MemberT = UPartyMember>
	MemberT& GetOwningLocalMember() const
	{
		MemberT* LocalMember = GetPartyMember<MemberT>(OwningLocalUserId);
		check(LocalMember);
		return *LocalMember;
	}

	template <typename MemberT = UPartyMember>
	MemberT* GetPartyLeader() const
	{
		return GetPartyMember<MemberT>(CurrentLeaderId);
	}

	template <typename MemberT = UPartyMember>
	MemberT* GetPartyMember(const FUniqueNetIdRepl& MemberId) const
	{
		return Cast<MemberT>(GetMemberInternal(MemberId));
	}
	
	ULocalPlayer& GetOwningLocalPlayer() const;
	const FUniqueNetIdRepl& GetOwningLocalUserId() const { return OwningLocalUserId; }
	const FUniqueNetIdRepl& GetPartyLeaderId() const { return CurrentLeaderId; }
	bool IsLocalPlayerPartyLeader() const;

	FChatRoomId GetChatRoomId() const;
	bool IsPersistentParty() const;
	const FOnlinePartyTypeId& GetPartyTypeId() const;
	const FOnlinePartyId& GetPartyId() const;

	EPartyState GetOssPartyState() const;

	bool IsCurrentlyCrossplaying() const;
	void StayWithPartyOnExit(bool bInStayWithParty);
	bool ShouldStayWithPartyOnExit() const;

	bool IsPartyFunctionalityDegraded() const;

	bool IsPartyFull() const;
	int32 GetNumPartyMembers() const;
	void SetPartyMaxSize(int32 NewSize);
	int32 GetPartyMaxSize() const;
	FPartyJoinDenialReason GetPublicJoinability() const;
	bool IsLeavingParty() const { return bIsLeavingParty; }

	/** Is the specified net driver for our reservation beacon? */
	bool IsNetDriverFromReservationBeacon(const UNetDriver* InNetDriver) const;

	template <typename MemberT = UPartyMember>
	TArray<MemberT*> GetPartyMembers() const
	{
		TArray<MemberT*> PartyMembers;
		PartyMembers.Reserve(PartyMembersById.Num());
		for (const auto& IdMemberPair : PartyMembersById)
		{
			PartyMembers.Add(Cast<MemberT>(IdMemberPair.Value));
		}
		return PartyMembers;
	}

	FString ToDebugString() const;

	DECLARE_EVENT_OneParam(USocialParty, FLeavePartyEvent, EMemberExitedReason);
	FLeavePartyEvent& OnPartyLeaveBegin() const { return OnPartyLeaveBeginEvent; }
	FLeavePartyEvent& OnPartyLeft() const { return OnPartyLeftEvent; }

	DECLARE_EVENT_OneParam(USocialParty, FOnPartyMemberCreated, UPartyMember&);
	FOnPartyMemberCreated& OnPartyMemberCreated() const { return OnPartyMemberCreatedEvent; }

	DECLARE_EVENT_OneParam(USocialParty, FOnPartyConfigurationChanged, const FPartyConfiguration&);
	FOnPartyConfigurationChanged& OnPartyConfigurationChanged() const { return OnPartyConfigurationChangedEvent; }

	DECLARE_EVENT_OneParam(USocialParty, FOnPartyStateChanged, EPartyState);
	FOnPartyStateChanged& OnPartyStateChanged() const { return OnPartyStateChangedEvent; }

	DECLARE_EVENT_OneParam(USocialParty, FOnPartyFunctionalityDegradedChanged, bool /*bFunctionalityDegraded*/);
	FOnPartyFunctionalityDegradedChanged& OnPartyFunctionalityDegradedChanged() const { return OnPartyFunctionalityDegradedChangedEvent; }

	DECLARE_EVENT_OneParam(USocialParty, FOnInviteSent, const USocialUser&);
	FOnInviteSent& OnInviteSent() const { return OnInviteSentEvent; }

	DECLARE_EVENT_TwoParams(USocialParty, FOnPartyJIPApproved, const FOnlinePartyId&, bool /* Success*/);
	FOnPartyJIPApproved& OnPartyJIPApproved() const { return OnPartyJIPApprovedEvent; }

	const FPartyPrivacySettings& GetPrivacySettings() const;

PARTY_SCOPE:
	void InitializeParty(const TSharedRef<const FOnlineParty>& InOssParty);
	bool IsInitialized() const;
	void TryFinishInitialization();

	bool ShouldCacheForRejoinOnDisconnect() const;
	bool IsCurrentlyLeaving() const;

	void SetIsMissingPlatformSession(bool bInIsMissingPlatformSession);

	FPartyRepData& GetMutableRepData() { return *PartyDataReplicator; }

	//--------------------------
	// User/member-specific actions that are best exposed on the individuals themselves, but best handled by the actual party (thus the package scoping)
	bool HasUserBeenInvited(const USocialUser& User) const;
	
	bool CanInviteUser(const USocialUser& User) const;
	bool CanPromoteMember(const UPartyMember& PartyMember) const;
	bool CanKickMember(const UPartyMember& PartyMember) const;
	
	bool TryInviteUser(const USocialUser& UserToInvite);
	bool TryPromoteMember(const UPartyMember& PartyMember);
	bool TryKickMember(const UPartyMember& PartyMember);
	//--------------------------

protected:
	virtual void InitializePartyInternal();
	
	/** Only called when a new party is being created by the local player and they are responsible for the rep data. Otherwise we just wait to receive it from the leader. */
	virtual void InitializePartyRepData();
	virtual FPartyPrivacySettings GetDesiredPrivacySettings() const;
	virtual void OnLocalPlayerIsLeaderChanged(bool bIsLeader);
	virtual void HandlePrivacySettingsChanged(const FPartyPrivacySettings& NewPrivacySettings);
	virtual void OnLeftPartyInternal(EMemberExitedReason Reason);

	virtual void OnInviteSentInternal(ESocialSubsystem SubsystemType, const USocialUser& InvitedUser, bool bWasSuccessful);
	
	/** Determines the joinability of this party for a specific user requesting to join */
	virtual FPartyJoinApproval EvaluateJoinRequest(const FUniqueNetId& PlayerId, const FUserPlatform& Platform, const FOnlinePartyData& JoinData, bool bFromJoinRequest) const;

	/** Determines the joinability of the game a party is in for JoinInProgress */
	virtual FPartyJoinApproval EvaluateJIPRequest(const FUniqueNetId& PlayerId) const;

	/** Determines the reason why, if at all, this party is currently flat-out unjoinable  */
	virtual FPartyJoinDenialReason DetermineCurrentJoinability() const;

	/** Override in child classes to specify the type of UPartyMember to create */
	virtual TSubclassOf<UPartyMember> GetDesiredMemberClass(bool bLocalPlayer) const;

	FName GetGameSessionName() const;
	bool IsInRestrictedGameSession() const;

	/**
	 * Create a reservation beacon and connect to the server to get approval for new party members
	 * Only relevant while in an active game, not required while pre lobby / game
	 */
	void ConnectToReservationBeacon();
	void CleanupReservationBeacon();
	APartyBeaconClient* CreateReservationBeaconClient();

	APartyBeaconClient* GetReservationBeaconClient() const { return ReservationBeaconClient; }

	/** Child classes MUST call EstablishRepDataInstance() on this using their member rep data struct instance */
	FPartyDataReplicator PartyDataReplicator;

	/** Reservation beacon class for getting server approval for new party members while in a game */
	UPROPERTY()
	TSubclassOf<APartyBeaconClient> ReservationBeaconClientClass;

private:
	UPartyMember* GetOrCreatePartyMember(const FUniqueNetId& MemberId);
	void PumpApprovalQueue();
	void RejectAllPendingJoinRequests();
	void SetIsMissingXmppConnection(bool bInMissingXmppConnection);
	void BeginLeavingParty(EMemberExitedReason Reason);
	void FinalizePartyLeave(EMemberExitedReason Reason);

	void UpdatePlatformSessionLeader(FName PlatformOssName);

	void HandlePreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel);

	/** Apply local party configuration to the OSS party, optionally resetting the access key to the party in the process */
	void UpdatePartyConfig(bool bResetAccessKey = false);

	UPartyMember* GetMemberInternal(const FUniqueNetIdRepl& MemberId) const;

private:	// Handlers
	void HandlePartyStateChanged(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, EPartyState PartyState);
	void HandlePartyConfigChanged(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const TSharedRef<FPartyConfiguration>& PartyConfig);
	void HandleUpdatePartyConfigComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, EUpdateConfigCompletionResult Result);
	void HandlePartyDataReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const TSharedRef<FOnlinePartyData>& PartyData);
	void HandleJoinabilityQueryReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& SenderId, const FString& Platform, const FOnlinePartyData& JoinData);
	void HandlePartyJoinRequestReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& SenderId, const FString& Platform, const FOnlinePartyData& JoinData);
	void HandlePartyJIPRequestReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& SenderId);
	void HandlePartyLeft(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId);
	void HandlePartyMemberExited(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& MemberId, EMemberExitedReason ExitReason);
	void HandlePartyMemberDataReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& MemberId, const TSharedRef<FOnlinePartyData>& PartyMemberData);
	void HandlePartyMemberJoined(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& MemberId);
	void HandlePartyMemberJIP(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, bool Success);
	void HandlePartyMemberPromoted(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& NewLeaderId);
	void HandlePartyPromotionLockoutChanged(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, bool bArePromotionsLocked);

	void HandleMemberInitialized(UPartyMember* Member);
	void HandleMemberSessionIdChanged(const FSessionId& NewSessionId, UPartyMember* Member);

	void HandleBeaconHostConnectionFailed();
	void HandleReservationRequestComplete(EPartyReservationResult::Type ReservationResponse);

	void HandleLeavePartyComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, ELeavePartyCompletionResult LeaveResult, FOnLeavePartyAttemptComplete OnAttemptComplete);
	
private:
	TSharedPtr<const FOnlineParty> OssParty;

	UPROPERTY()
	FUniqueNetIdRepl OwningLocalUserId;

	/** Tracked explicitly so we know which player was demoted whenever the leader changes */
	UPROPERTY()
	FUniqueNetIdRepl CurrentLeaderId;

	UPROPERTY()
	TMap<FUniqueNetIdRepl, UPartyMember*> PartyMembersById;

	FPartyConfiguration CurrentConfig;

	//@todo DanH Party: Rename/reorg this to more clearly call out that this is specific to lobby beacon stuff #suggested
	struct FPendingMemberApproval
	{
		FUniqueNetIdRepl RecipientId;
		FUniqueNetIdRepl SenderId;
		FUserPlatform Platform;
		bool bIsJIPApproval;
		TSharedPtr<const FOnlinePartyData> JoinData;
	};
	TQueue<FPendingMemberApproval> PendingApprovals;

	bool bStayWithPartyOnDisconnect = false;
	bool bIsMemberPromotionPossible = true;
	
	/**
	 * Last known reservation beacon client net driver name
	 * Intended to be used to detect network errors related to our current or last reservation beacon client's net driver.
	 * Some network error handlers may be called after we cleanup our beacon connection.
	 */
	FName LastReservationBeaconClientNetDriverName;

	/** Reservation beacon client instance while getting approval for new party members*/
	UPROPERTY()
	APartyBeaconClient* ReservationBeaconClient = nullptr;

	/**
	 * True when we have limited functionality due to lacking an xmpp connection.
	 * Don't set directly, use the private setter to trigger events appropriately.
	 */
	bool bIsMissingXmppConnection = false;
	bool bIsMissingPlatformSession = false;

	bool bIsLeavingParty = false;
	bool bIsInitialized = false;

	mutable FLeavePartyEvent OnPartyLeaveBeginEvent;
	mutable FLeavePartyEvent OnPartyLeftEvent;
	mutable FOnPartyMemberCreated OnPartyMemberCreatedEvent;
	mutable FOnPartyConfigurationChanged OnPartyConfigurationChangedEvent;
	mutable FOnPartyStateChanged OnPartyStateChangedEvent;
	mutable FOnPartyFunctionalityDegradedChanged OnPartyFunctionalityDegradedChangedEvent;
	mutable FOnInviteSent OnInviteSentEvent;
	mutable FOnPartyJIPApproved OnPartyJIPApprovedEvent;
};
