// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "SocialTypes.h"
#include "User/ISocialUserList.h"
#include "OnlineSubsystem.h"

#include "SocialToolkit.generated.h"

class ULocalPlayer;
class USocialUser;
class FSocialFriendInvitation;
class FSocialPartyInvitation;
class USocialChatManager;

class FOnlinePartyId;

enum class EMemberExitedReason;

namespace EOnlinePresenceState { enum Type : uint8; }

/** Represents the full suite of social functionality available to a given LocalPlayer */
UCLASS(Within = SocialManager)
class PARTY_API USocialToolkit : public UObject
{
	GENERATED_BODY()

public:
	static USocialToolkit* GetToolkitForPlayer(ULocalPlayer* LocalPlayer);

	USocialToolkit();

	virtual void InitializeToolkit(ULocalPlayer& InOwningLocalPlayer);
	virtual bool IsOwnerLoggedIn() const;
	
	USocialChatManager& GetChatManager() const;

	IOnlineSubsystem* GetSocialOss(ESocialSubsystem SubsystemType) const;

	TSharedRef<ISocialUserList> CreateUserList(const FSocialUserListConfig& ListConfig);

	USocialUser& GetLocalUser() const;
	FUniqueNetIdRepl GetLocalUserNetId(ESocialSubsystem SubsystemType) const;
	int32 GetLocalUserNum() const;

	const FOnlineUserPresence* GetPresenceInfo(ESocialSubsystem SubsystemType) const;
	void SetLocalUserOnlineState(EOnlinePresenceState::Type OnlineState);

	USocialManager& GetSocialManager() const;
	ULocalPlayer& GetOwningLocalPlayer() const;
	const TArray<USocialUser*>& GetAllUsers() const { return AllUsers; }

	/** Finds a SocialUser given a unique net ID from any OSS */
	USocialUser* FindUser(const FUniqueNetIdRepl& UserId) const;

	/**
	 * Queues an action to be executed when a USocialUser corresponding to the given UniqueId has been established.
	 * The process of creating and initializing a user varies in complexity depending on the local user's platform and the OSS the given Id corresponds to.
	 *
	 * Up to the caller whether they want the action executed immediately upon creation of the USocialUser instance,
	 *	or after the instance is fully initialized (i.e. has finished discovering all available backend information)
	 *  Default is to execute after initialization and is generally more appropriate.
	 */
	void QueueUserDependentAction(const FUniqueNetIdRepl& UserId, TFunction<void(USocialUser&)>&& UserActionFunc, bool bExecutePostInit = true);

	/**
	 * Attempts to send a friend invite to another user based on display name or email.
	 * Only necessary to use this path when you do not have a known USocialUser for this user already.
	 */
	void TrySendFriendInvite(const FString& DisplayNameOrEmail) const;

	void RequestDisplayPlatformSocialUI() const;

	const FString& GetRecentPlayerNamespaceToQuery() const { return RecentPlayerNamespaceToQuery; }
	
	DECLARE_EVENT_OneParam(USocialToolkit, FPartyInviteEvent, USocialUser&);
	FPartyInviteEvent& OnPartyInviteReceived() const { return OnPartyInviteReceivedEvent; }

	DECLARE_EVENT_TwoParams(USocialToolkit, FFriendInviteEvent, USocialUser&, ESocialSubsystem);
	FFriendInviteEvent& OnFriendInviteSent() const { return OnFriendInviteSentEvent; }
	FFriendInviteEvent& OnFriendInviteReceived() const { return OnFriendInviteReceivedEvent; }
	
	DECLARE_EVENT_ThreeParams(USocialToolkit, FOnRelationshipEstablished, USocialUser&, ESocialSubsystem, bool /*bIsNewlyEstablished*/);
	FOnRelationshipEstablished& OnFriendshipEstablished() const { return OnFriendshipEstablishedEvent; }
	FOnRelationshipEstablished& OnUserBlocked() const { return OnUserBlockedEvent; }
	FOnRelationshipEstablished& OnRecentPlayerAdded() const { return OnRecentPlayerAddedEvent; }

	DECLARE_EVENT_OneParam(USocialToolkit, FOnKnownUserInitialized, USocialUser&);
	FOnKnownUserInitialized& OnKnownUserInitialized() { return OnKnownUserInitializedEvent; }

	DECLARE_EVENT(USocialToolkit, FBasicToolkitEvent);
	FBasicToolkitEvent& OnToolkitReset() const { return OnToolkitResetEvent; }

PARTY_SCOPE:
	void NotifySubsystemIdEstablished(USocialUser& SocialUser, ESocialSubsystem SubsystemType, const FUniqueNetIdRepl& SubsystemId);
	TSubclassOf<USocialChatManager> GetChatManagerClass() { return ChatManagerClass; }

	bool TrySendFriendInvite(USocialUser& SocialUser, ESocialSubsystem SubsystemType) const;

#if PLATFORM_PS4
	void NotifyPSNFriendsListRebuilt();
#endif

protected:
	virtual bool IsFriendshipRestricted(const USocialUser& SocialUser, ESocialSubsystem SubsystemType) const;

	virtual void OnOwnerLoggedIn();
	virtual void OnOwnerLoggedOut();
	
	virtual void NotifyFriendInviteFailed(const FUniqueNetId& InvitedUserId, const FString& InvitedUserName, ESendFriendInviteFailureReason FailureReason, bool bCanShow = true) {}

	void QueryFriendsLists();
	void QueryBlockedPlayers();
	void QueryRecentPlayers();

	virtual void OnQueryFriendsListSuccess(ESocialSubsystem SubsystemType, const TArray<TSharedRef<FOnlineFriend>>& FriendsList) {}
	virtual void OnQueryBlockedPlayersSuccess(ESocialSubsystem SubsystemType, const TArray<TSharedRef<FOnlineBlockedPlayer>>& BlockedPlayers) {}
	virtual void OnQueryRecentPlayersSuccess(ESocialSubsystem SubsystemType, const TArray<TSharedRef<FOnlineRecentPlayer>>& FriendsList) {}
	
	/** The type of SocialUser to create to represent known users */
	TSubclassOf<USocialUser> SocialUserClass;

	/** Class to use when creating the ChatManager */
	TSubclassOf<USocialChatManager> ChatManagerClass;

	/** The name of the friends list to query */
	FString FriendListToQuery = TEXT("default");

	/** The name of the recent player namespace to query */
	FString RecentPlayerNamespaceToQuery;

	//@todo DanH: Basic idea here is that the toolkit may not want to query all of these things up-front for scalability reasons, some it may want to wait on until some requirement is met
	//		Something nicer is definitely possible and worth revisiting
	bool bQueryFriendsOnStartup = true;
	bool bQueryBlockedPlayersOnStartup = true;
	bool bQueryRecentPlayersOnStartup = true;

private:
	void QueueUserDependentActionInternal(const FUniqueNetIdRepl& SubsystemId, ESocialSubsystem SubsystemType, TFunction<void(USocialUser&)>&& UserActionFunc, bool bExecutePostInit = true);

	template <typename OnlineUserT>
	void ProcessUserList(const TArray<TSharedRef<OnlineUserT>>& OssUserList, ESocialSubsystem SubsystemType, FOnRelationshipEstablished& RelationshipEvent)
	{
		UE_LOG(LogParty, Verbose, TEXT("SocialToolkit [%d] processing queried list of [%d] users on subsystem [%s]"), GetLocalUserNum(), OssUserList.Num(), ToString(SubsystemType));
		for (const TSharedRef<OnlineUserT>& OssUserInfo : OssUserList)
		{
			QueueUserDependentActionInternal(OssUserInfo->GetUserId(), SubsystemType,
				[WeakUserInfo = TWeakPtr<OnlineUserT>(OssUserInfo), SubsystemType, &RelationshipEvent](USocialUser& SocialUser)
				{
					// Make sure the OSS info is still around - given the async nature of all this it's possible that it's been replaced or removed
					if (WeakUserInfo.IsValid())
					{
						SocialUser.EstablishOssInfo(WeakUserInfo.Pin().ToSharedRef(), SubsystemType);
						RelationshipEvent.Broadcast(SocialUser, SubsystemType, false);
					}
				});
		}
	}

private:	// Handlers
	void HandleControllerIdChanged(int32 NewId, int32 OldId);
	void HandlePlayerLoginStatusChanged(int32 LocalUserNum, ELoginStatus::Type OldStatus, ELoginStatus::Type NewStatus, const FUniqueNetId& NewId);
	
	void HandleReadFriendsListComplete(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr, ESocialSubsystem SubsystemType);
	void HandleQueryBlockedPlayersComplete(const FUniqueNetId& UserId, bool bWasSuccessful, const FString& ErrorStr, ESocialSubsystem SubsystemType);
	void HandleQueryRecentPlayersComplete(const FUniqueNetId& UserId, const FString& Namespace, bool bWasSuccessful, const FString& ErrorStr, ESocialSubsystem SubsystemType);

	void HandleRecentPlayersAdded(const FUniqueNetId& LocalUserId, const TArray<TSharedRef<FOnlineRecentPlayer>>& NewRecentPlayers, ESocialSubsystem SubsystemType);

	void HandleMapExternalIdComplete(ESocialSubsystem SubsystemType, bool bWasSuccessful, const FUniqueNetIdRepl& MappedPrimaryId, FUniqueNetIdRepl ExternalId, TFunction<void(USocialUser&)> UserActionFunc, bool bExecutePostInit);
	void HandlePresenceReceived(const FUniqueNetId& UserId, const TSharedRef<FOnlineUserPresence>& NewPresence, ESocialSubsystem SubsystemType);

	void HandleFriendInviteReceived(const FUniqueNetId& LocalUserId, const FUniqueNetId& SenderId, ESocialSubsystem SubsystemType);
	void HandleFriendInviteAccepted(const FUniqueNetId& LocalUserId, const FUniqueNetId& NewFriendId, ESocialSubsystem SubsystemType);
	void HandleFriendInviteRejected(const FUniqueNetId& LocalUserId, const FUniqueNetId& RejecterId, ESocialSubsystem SubsystemType);
	void HandleFriendInviteSent(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& InvitedUserId, const FString& ListName, const FString& ErrorStr, ESocialSubsystem SubsystemType, FString DisplayName);
	void HandleFriendRemoved(const FUniqueNetId& LocalUserId, const FUniqueNetId& FormerFriendId, ESocialSubsystem SubsystemType);

	void HandleDeleteFriendComplete(int32 LocalPlayer, bool bWasSuccessful, const FUniqueNetId& FormerFriendId, const FString& ListName, const FString& ErrorStr, ESocialSubsystem SubsystemType);

	void HandlePartyInviteReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& SenderId);

	void HandleBlockPlayerComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& BlockedPlayerID, const FString& ListName, const FString& ErrorStr, ESocialSubsystem SubsystemType);
	void HandleUnblockPlayerComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UnblockedPlayerID, const FString& ListName, const FString& ErrorStr, ESocialSubsystem SubsystemType);
	
	void HandleQueryPrimaryUserIdMappingComplete(bool bWasSuccessful, const FUniqueNetId& RequestingUserId, const FString& DisplayName, const FUniqueNetId& IdentifiedUserId, const FString& Error);

	void HandlePartyMemberExited(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& MemberId, const EMemberExitedReason Reason);
	void HandleGameDestroyed(const FName SessionName, bool bWasSuccessful);

private:
	static TMap<TWeakObjectPtr<ULocalPlayer>, TWeakObjectPtr<USocialToolkit>> AllToolkitsByOwningPlayer;

	UPROPERTY()
	USocialUser* LocalUser;

	UPROPERTY()
	TArray<USocialUser*> AllUsers;
	TMap<FUniqueNetIdRepl, TWeakObjectPtr<USocialUser>> UsersBySubsystemIds;

	UPROPERTY()
	ULocalPlayer* LocalPlayerOwner = nullptr;

	UPROPERTY()
	USocialChatManager* SocialChatManager;

	mutable FPartyInviteEvent OnPartyInviteReceivedEvent;
	mutable FPartyInviteEvent OnPartyInviteAcceptedEvent;
	mutable FPartyInviteEvent OnPartyInviteRejectedEvent;

	mutable FFriendInviteEvent OnFriendInviteReceivedEvent;
	mutable FFriendInviteEvent OnFriendInviteSentEvent;
	
	mutable FOnRelationshipEstablished OnFriendshipEstablishedEvent;
	mutable FOnRelationshipEstablished OnUserBlockedEvent;
	mutable FOnRelationshipEstablished OnRecentPlayerAddedEvent;
	
	mutable FOnKnownUserInitialized OnKnownUserInitializedEvent;
	mutable FBasicToolkitEvent OnToolkitResetEvent;
};
