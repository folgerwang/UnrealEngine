// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../SocialTypes.h"
#include "UObject/Object.h"
#include "OnlineSessionSettings.h"
#include "Party/PartyTypes.h"
#include "Interactions/SocialInteractionHandle.h"

#include "SocialUser.generated.h"

class IOnlinePartyJoinInfo;
class FOnlineUserPresence;
class UPartyMember;

namespace EOnlinePresenceState { enum Type : uint8; }

DECLARE_MULTICAST_DELEGATE_OneParam(FOnNewSocialUserInitialized, USocialUser&);

UCLASS(Within = SocialToolkit)
class PARTY_API USocialUser : public UObject
{
	GENERATED_BODY()

public:
	USocialUser();

	void RegisterInitCompleteHandler(const FOnNewSocialUserInitialized::FDelegate& OnInitializationComplete);
	bool IsInitialized() const { return bIsInitialized; }

	void ValidateFriendInfo(ESocialSubsystem SubsystemType);
	TArray<ESocialSubsystem> GetRelationshipSubsystems(ESocialRelationship Relationship) const;
	TArray<ESocialSubsystem> GetRelevantSubsystems() const;
	bool HasSubsystemInfo(ESocialSubsystem Subsystem) const;
	bool HasSubsystemInfo(const TSet<ESocialSubsystem>& SubsystemTypes, bool bRequireAll = false);

	bool IsLocalUser() const;
	bool HasNetId(const FUniqueNetIdRepl& UniqueId) const;
	USocialToolkit& GetOwningToolkit() const;
	EOnlinePresenceState::Type GetOnlineStatus() const;

	FUniqueNetIdRepl GetUserId(ESocialSubsystem SubsystemType) const;
	FString GetDisplayName() const;
	FString GetDisplayName(ESocialSubsystem SubsystemType) const;

	EInviteStatus::Type GetFriendInviteStatus(ESocialSubsystem SubsystemType) const;
	bool IsFriend() const;
	bool IsFriend(ESocialSubsystem SubsystemType) const;
	bool IsFriendshipPending(ESocialSubsystem SubsystemType) const;
	const FOnlineUserPresence* GetFriendPresenceInfo(ESocialSubsystem SubsystemType) const;
	FDateTime GetFriendshipCreationDate() const;
	FText GetSocialName() const;
	FUserPlatform GetCurrentPlatform() const;

	virtual void GetRichPresenceText(FText& OutRichPresence) const;

	bool IsBlocked() const;
	bool IsBlocked(ESocialSubsystem SubsystemType) const;

	bool IsOnline() const;
	bool IsPlayingThisGame() const;
	
	bool SetUserLocalAttribute(ESocialSubsystem SubsystemType, const FString& AttrName, const FString& AttrValue);
	bool GetUserAttribute(ESocialSubsystem SubsystemType, const FString& AttrName, FString& OutAttrValue) const;

	bool HasAnyInteractionsAvailable() const;
	TArray<FSocialInteractionHandle> GetAllAvailableInteractions() const;

	bool CanSendFriendInvite(ESocialSubsystem SubsystemType) const;
	bool SendFriendInvite(ESocialSubsystem SubsystemType);
	bool AcceptFriendInvite(ESocialSubsystem SocialSubsystem) const;
	bool RejectFriendInvite(ESocialSubsystem SocialSubsystem) const;
	bool EndFriendship(ESocialSubsystem SocialSubsystem) const;

	bool ShowPlatformProfile();

	TSharedPtr<const IOnlinePartyJoinInfo> GetPartyJoinInfo(const FOnlinePartyTypeId& PartyTypeId) const;

	bool HasSentPartyInvite(const FOnlinePartyTypeId& PartyTypeId) const;
	FJoinPartyResult CheckPartyJoinability(const FOnlinePartyTypeId& PartyTypeId) const;
	void JoinParty(const FOnlinePartyTypeId& PartyTypeId) const;
	void RejectPartyInvite(const FOnlinePartyTypeId& PartyTypeId);

	bool HasBeenInvitedToParty(const FOnlinePartyTypeId& PartyTypeId) const;
	bool CanInviteToParty(const FOnlinePartyTypeId& PartyTypeId) const;
	bool InviteToParty(const FOnlinePartyTypeId& PartyTypeId) const;

	bool BlockUser(ESocialSubsystem Subsystem);
	bool UnblockUser(ESocialSubsystem Subsystem);

	UPartyMember* GetPartyMember(const FOnlinePartyTypeId& PartyTypeId) const;

	DECLARE_EVENT(USocialUser, FPartyInviteResponseEvent);
	FPartyInviteResponseEvent& OnPartyInviteAccepted() const { return OnPartyInviteAcceptedEvent; }
	FPartyInviteResponseEvent& OnPartyInviteRejected() const { return OnPartyInviteRejectedEvent; }

	DECLARE_EVENT_OneParam(USocialUser, FOnUserPresenceChanged, ESocialSubsystem)
	FOnUserPresenceChanged& OnUserPresenceChanged() const { return OnUserPresenceChangedEvent; }

	DECLARE_EVENT_OneParam(USocialUser, FOnFriendRemoved, ESocialSubsystem)
	FOnFriendRemoved& OnFriendRemoved() const { return OnFriendRemovedEvent; }
	FOnFriendRemoved& OnFriendInviteRemoved() const { return OnFriendInviteRemovedEvent; }

	DECLARE_EVENT_TwoParams(USocialUser, FOnBlockedStatusChanged, ESocialSubsystem, bool)
	FOnBlockedStatusChanged& OnBlockedStatusChanged() const { return OnBlockedStatusChangedEvent; }

	DECLARE_EVENT_ThreeParams(USocialUser, FOnSubsystemIdEstablished, USocialUser&, ESocialSubsystem, const FUniqueNetIdRepl&)
	FOnSubsystemIdEstablished& OnSubsystemIdEstablished() const { return OnSubsystemIdEstablishedEvent; }

	//void ClearPopulateInfoDelegateForSubsystem(ESocialSubsystem SubsystemType);

	FString ToDebugString() const;

PARTY_SCOPE:
	void InitLocalUser();
	void Initialize(const FUniqueNetIdRepl& PrimaryId);

	void NotifyPresenceChanged(ESocialSubsystem SubsystemType);
	void NotifyUserUnblocked(ESocialSubsystem SubsystemType);
	void NotifyFriendInviteRemoved(ESocialSubsystem SubsystemType);
	void NotifyUserUnfriended(ESocialSubsystem SubsystemType);

	void EstablishOssInfo(const TSharedRef<FOnlineFriend>& FriendInfo, ESocialSubsystem SubsystemType);
	void EstablishOssInfo(const TSharedRef<FOnlineBlockedPlayer>& BlockedPlayerInfo, ESocialSubsystem SubsystemType);
	void EstablishOssInfo(const TSharedRef<FOnlineRecentPlayer>& RecentPlayerInfo, ESocialSubsystem SubsystemType);

protected:
	virtual void OnPresenceChangedInternal(ESocialSubsystem SubsystemType);

private:
	void SetSubsystemId(ESocialSubsystem SubsystemType, const FUniqueNetIdRepl& SubsystemId);
	
	void SetUserInfo(ESocialSubsystem SubsystemType, const TSharedRef<FOnlineUser>& UserInfo);
	void HandleQueryUserInfoComplete(ESocialSubsystem SubsystemType, bool bWasSuccessful, const TSharedPtr<FOnlineUser>& UserInfo);
	
private:
	void TryBroadcastInitializationComplete();

	struct FSubsystemUserInfo
	{
		FSubsystemUserInfo(const FUniqueNetIdRepl& InUserId)
			: UserId(InUserId)
		{}

		bool IsValid() const;
		const FUniqueNetIdRepl& GetUserId() const { return UserId; }
		FString GetDisplayName() const { return UserInfo.IsValid() ? UserInfo.Pin()->GetDisplayName() : TEXT(""); }
		bool IsFriend() const { return GetFriendInviteStatus() == EInviteStatus::Accepted; }
		bool IsBlocked() const { return BlockedPlayerInfo.IsValid() || GetFriendInviteStatus() == EInviteStatus::Blocked; }
		EInviteStatus::Type GetFriendInviteStatus() const { return FriendInfo.IsValid() ? FriendInfo.Pin()->GetInviteStatus() : EInviteStatus::Unknown; }
		bool HasValidPresenceInfo() const { return IsFriend(); }
		const FOnlineUserPresence* GetPresenceInfo() const;

		// On the fence about caching this locally. We don't care about where it came from if we do, and we can cache it independent from any of the info structs (which will play nice with external mapping queries before querying the user info itself)
		FUniqueNetIdRepl UserId;

		TWeakPtr<FOnlineUser> UserInfo;
		TWeakPtr<FOnlineFriend> FriendInfo;
		TWeakPtr<FOnlineRecentPlayer> RecentPlayerInfo;
		TWeakPtr<FOnlineBlockedPlayer> BlockedPlayerInfo;
	};
	FSubsystemUserInfo& FindOrCreateSubsystemInfo(const FUniqueNetIdRepl& SubsystemId, ESocialSubsystem SubsystemType);

	int32 NumPendingQueries = 0;
	bool bIsInitialized = false;

	TSharedPtr<IOnlinePartyJoinInfo> PersistentPartyInfo;
	TMap<ESocialSubsystem, FSubsystemUserInfo> SubsystemInfoByType;

	// Initialization delegates that fire only when a specific user has finishing initializing
	static TMap<TWeakObjectPtr<USocialUser>, FOnNewSocialUserInitialized> InitEventsByUser;

	mutable FPartyInviteResponseEvent OnPartyInviteAcceptedEvent;
	mutable FPartyInviteResponseEvent OnPartyInviteRejectedEvent;
	mutable FOnUserPresenceChanged OnUserPresenceChangedEvent;
	mutable FOnFriendRemoved OnFriendRemovedEvent;
	mutable FOnFriendRemoved OnFriendInviteRemovedEvent;
	mutable FOnBlockedStatusChanged OnBlockedStatusChangedEvent;
	mutable FOnSubsystemIdEstablished OnSubsystemIdEstablishedEvent;
};