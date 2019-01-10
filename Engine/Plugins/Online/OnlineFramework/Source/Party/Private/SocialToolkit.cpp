// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SocialToolkit.h"
#include "SocialManager.h"
#include "SocialQuery.h"
#include "SocialSettings.h"
#include "User/SocialUser.h"
#include "User/SocialUserList.h"
#include "Chat/SocialChatManager.h"

#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Engine/LocalPlayer.h"

bool NameToSocialSubsystem(FName SubsystemName, ESocialSubsystem& OutSocialSubsystem)
{
	for (uint8 SocialSubsystemIdx = 0; SocialSubsystemIdx < (uint8)ESocialSubsystem::MAX; ++SocialSubsystemIdx)
	{
		if (SubsystemName == USocialManager::GetSocialOssName((ESocialSubsystem)SocialSubsystemIdx))
		{
			OutSocialSubsystem = (ESocialSubsystem)SocialSubsystemIdx;
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// FSocialQuery_MapExternalIds
//////////////////////////////////////////////////////////////////////////

class FSocialQuery_MapExternalIds : public TSocialQuery<FString, const FUniqueNetIdRepl&>
{
public:
	static FName GetQueryId() { return TEXT("MapExternalIds"); }

	virtual void AddUserId(const FString& UserIdStr, const FOnQueryComplete& QueryCompleteHandler) override
	{
		// Prepend the environment prefix (if there is one) to the true ID we're after before actually adding the ID
		const FString MappableIdStr = USocialSettings::GetUniqueIdEnvironmentPrefix(SubsystemType) + UserIdStr;

		TSocialQuery<FString, const FUniqueNetIdRepl&>::AddUserId(MappableIdStr, QueryCompleteHandler);
	}

	virtual void ExecuteQuery() override
	{
		FUniqueNetIdRepl LocalUserPrimaryId = Toolkit.IsValid() ? Toolkit->GetLocalUserNetId(ESocialSubsystem::Primary) : FUniqueNetIdRepl();
		if (LocalUserPrimaryId.IsValid())
		{
			// The external mappings will always be checked on the primary OSS, so we use the passed-in OSS as the target we want to map to
			IOnlineSubsystem* OSS = GetOSS();
			IOnlineIdentityPtr IdentityInterface = OSS ? OSS->GetIdentityInterface() : nullptr;
			IOnlineUserPtr PrimaryUserInterface = Toolkit->GetSocialOss(ESocialSubsystem::Primary)->GetUserInterface();
			if (ensure(IdentityInterface && PrimaryUserInterface))
			{
				bHasExecuted = true;
				
				TArray<FString> ExternalUserIds;
				CompletionCallbacksByUserId.GenerateKeyArray(ExternalUserIds);
				UE_LOG(LogParty, Log, TEXT("FSocialQuery_MapExternalIds executing for [%d] users on subsystem [%s]"), ExternalUserIds.Num(), ToString(SubsystemType));

				const FString AuthType = IdentityInterface->GetAuthType().ToLower();
				FExternalIdQueryOptions QueryOptions(AuthType, false);
				PrimaryUserInterface->QueryExternalIdMappings(*LocalUserPrimaryId, QueryOptions, ExternalUserIds, IOnlineUser::FOnQueryExternalIdMappingsComplete::CreateSP(this, &FSocialQuery_MapExternalIds::HandleQueryExternalIdMappingsComplete));
			}
		}
		else
		{
			UE_LOG(LogParty, Warning, TEXT("FSocialQuery_MapExternalIds cannot execute query - unable to get a valid primary net ID for the local player."));
		}
	}

	void HandleQueryExternalIdMappingsComplete(bool bWasSuccessful, const FUniqueNetId&, const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, const FString& ErrorStr)
	{
		UE_LOG(LogParty, Log, TEXT("FSocialQuery_MapExternalIds completed query for [%d] users on subsystem [%s] with error [%s]"), ExternalIds.Num(), ToString(SubsystemType), *ErrorStr);

		if (bWasSuccessful)
		{
			IOnlineUserPtr PrimaryUserInterface = Toolkit->GetSocialOss(ESocialSubsystem::Primary)->GetUserInterface();
			if (PrimaryUserInterface.IsValid() && bWasSuccessful)
			{
				for (const FString& ExternalId : ExternalIds)
				{
					TSharedPtr<const FUniqueNetId> PrimaryId = PrimaryUserInterface->GetExternalIdMapping(QueryOptions, ExternalId);
					if (!PrimaryId.IsValid())
					{
#if !UE_BUILD_SHIPPING
						UE_LOG(LogParty, Verbose, TEXT("No primary Id exists that corresponds to external Id [%s]"), *ExternalId);
#endif	
					}
					else if (CompletionCallbacksByUserId[ExternalId].IsBound())
					{
						CompletionCallbacksByUserId[ExternalId].Execute(SubsystemType, bWasSuccessful, PrimaryId);
					}
				}
			}

			OnQueryCompleted.ExecuteIfBound(GetQueryId(), AsShared());
		}
		else
		{
			bHasExecuted = false;
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// USocialToolkit
//////////////////////////////////////////////////////////////////////////

//@todo DanH Social: Need a non-backdoor way to get toolkits from the manager (an issue when we don't know where the manager is) - new game subsystems should be a nice solve
TMap<TWeakObjectPtr<ULocalPlayer>, TWeakObjectPtr<USocialToolkit>> USocialToolkit::AllToolkitsByOwningPlayer;
USocialToolkit* USocialToolkit::GetToolkitForPlayer(ULocalPlayer* LocalPlayer)
{
	TWeakObjectPtr<USocialToolkit>* FoundToolkit = AllToolkitsByOwningPlayer.Find(LocalPlayer);
	return FoundToolkit ? FoundToolkit->Get() : nullptr;
}

USocialToolkit::USocialToolkit()
	: SocialUserClass(USocialUser::StaticClass())
	, ChatManagerClass(USocialChatManager::StaticClass())
{

}

void USocialToolkit::InitializeToolkit(ULocalPlayer& InOwningLocalPlayer)
{
	LocalPlayerOwner = &InOwningLocalPlayer;

	SocialChatManager = USocialChatManager::CreateChatManager(*this);

	// We want to allow reliable access to the SocialUser for the local player, but we can't initialize it until we actually log in
	LocalUser = NewObject<USocialUser>(this, SocialUserClass);

	check(!AllToolkitsByOwningPlayer.Contains(LocalPlayerOwner));
	AllToolkitsByOwningPlayer.Add(LocalPlayerOwner, this);

	InOwningLocalPlayer.OnControllerIdChanged().AddUObject(this, &USocialToolkit::HandleControllerIdChanged);
	HandleControllerIdChanged(InOwningLocalPlayer.GetControllerId(), INVALID_CONTROLLERID);
}

bool USocialToolkit::IsOwnerLoggedIn() const
{
	const IOnlineIdentityPtr IdentityInterface = Online::GetIdentityInterface(GetWorld());
	if (ensure(IdentityInterface.IsValid()))
	{
		const ELoginStatus::Type CurrentLoginStatus = IdentityInterface->GetLoginStatus(GetLocalUserNum());
		return CurrentLoginStatus == ELoginStatus::LoggedIn;
	}
	return false;
}

USocialChatManager& USocialToolkit::GetChatManager() const
{
	check(SocialChatManager);
	return *SocialChatManager;
}

IOnlineSubsystem* USocialToolkit::GetSocialOss(ESocialSubsystem SubsystemType) const
{
	return Online::GetSubsystem(GetWorld(), USocialManager::GetSocialOssName(SubsystemType));
}

TSharedRef<ISocialUserList> USocialToolkit::CreateUserList(const FSocialUserListConfig& ListConfig)
{
	return FSocialUserList::CreateUserList(*this, ListConfig);
}

USocialUser& USocialToolkit::GetLocalUser() const
{
	return *LocalUser;
}

FUniqueNetIdRepl USocialToolkit::GetLocalUserNetId(ESocialSubsystem SubsystemType) const
{
	return LocalUser->GetUserId(SubsystemType);
}

int32 USocialToolkit::GetLocalUserNum() const
{
	return GetOwningLocalPlayer().GetControllerId();
}

const FOnlineUserPresence* USocialToolkit::GetPresenceInfo(ESocialSubsystem SubsystemType) const
{
	if (IOnlineSubsystem* Oss = GetSocialOss(SubsystemType))
	{
		IOnlinePresencePtr PresenceInterface = Oss->GetPresenceInterface();
		FUniqueNetIdRepl LocalUserId = GetLocalUserNetId(SubsystemType);
		if (PresenceInterface.IsValid() && LocalUserId.IsValid())
		{
			TSharedPtr<FOnlineUserPresence> CurrentPresence;
			PresenceInterface->GetCachedPresence(*LocalUserId, CurrentPresence);
			if (CurrentPresence.IsValid())
			{
				return CurrentPresence.Get();
			}
		}
	}
	return nullptr;
}

void USocialToolkit::SetLocalUserOnlineState(EOnlinePresenceState::Type OnlineState)
{
	if (IOnlineSubsystem* PrimaryOss = GetSocialOss(ESocialSubsystem::Primary))
	{
		 IOnlinePresencePtr PresenceInterface = PrimaryOss->GetPresenceInterface();
		 FUniqueNetIdRepl LocalUserId = GetLocalUserNetId(ESocialSubsystem::Primary);
		 if (PresenceInterface.IsValid() && LocalUserId.IsValid())
		 {
			 TSharedPtr<FOnlineUserPresence> CurrentPresence;
			 PresenceInterface->GetCachedPresence(*LocalUserId, CurrentPresence);

			 FOnlineUserPresenceStatus NewStatus;
			 if (CurrentPresence.IsValid())
			 {
				 NewStatus = CurrentPresence->Status;
			 }
			 NewStatus.State = OnlineState;
			 PresenceInterface->SetPresence(*LocalUserId, NewStatus);
		 }
	}
}

USocialManager& USocialToolkit::GetSocialManager() const
{
	USocialManager* OuterSocialManager = GetTypedOuter<USocialManager>();
	check(OuterSocialManager);
	return *OuterSocialManager;
}

ULocalPlayer& USocialToolkit::GetOwningLocalPlayer() const
{
	check(LocalPlayerOwner);
	return *LocalPlayerOwner;
}

USocialUser* USocialToolkit::FindUser(const FUniqueNetIdRepl& UserId) const
{
	const TWeakObjectPtr<USocialUser>* FoundUser = UsersBySubsystemIds.Find(UserId);
	return FoundUser ? FoundUser->Get() : nullptr;
}

void USocialToolkit::TrySendFriendInvite(const FString& DisplayNameOrEmail) const
{
	IOnlineSubsystem* PrimaryOSS = GetSocialOss(ESocialSubsystem::Primary);
	IOnlineUserPtr UserInterface = PrimaryOSS ? PrimaryOSS->GetUserInterface() : nullptr;
	if (UserInterface.IsValid())
	{
		IOnlineUser::FOnQueryUserMappingComplete QueryCompleteDelegate = IOnlineUser::FOnQueryUserMappingComplete::CreateUObject(this, &USocialToolkit::HandleQueryPrimaryUserIdMappingComplete);
		UserInterface->QueryUserIdMapping(*GetLocalUserNetId(ESocialSubsystem::Primary), DisplayNameOrEmail, QueryCompleteDelegate);
	}
}

#if PLATFORM_PS4
void USocialToolkit::NotifyPSNFriendsListRebuilt()
{
	UE_LOG(LogParty, Log, TEXT("SocialToolkit [%d] quietly refreshing PSN FriendInfo on existing users due to an external requery of the friends list."), GetLocalUserNum());

	TArray<TSharedRef<FOnlineFriend>> PSNFriendsList;
	IOnlineFriendsPtr FriendsInterfacePSN = Online::GetFriendsInterfaceChecked(GetWorld(), PS4_SUBSYSTEM);
	FriendsInterfacePSN->GetFriendsList(GetLocalUserNum(), FriendListToQuery, PSNFriendsList);

	// This is a stealth update just to prevent the WeakPtr references to friend info on a given user disappearing out from under the user, so we don't actually want it to fire a real event
	static FOnRelationshipEstablished GarbageRelationshipEstablishedFiller;
	ProcessUserList(PSNFriendsList, ESocialSubsystem::Platform, GarbageRelationshipEstablishedFiller);
}
#endif

void USocialToolkit::QueueUserDependentAction(const FUniqueNetIdRepl& UserId, TFunction<void(USocialUser&)>&& UserActionFunc, bool bExecutePostInit)
{
	ESocialSubsystem CompatibleSubsystem = ESocialSubsystem::MAX;
	if (UserId.IsValid() && NameToSocialSubsystem(UserId.GetType(), CompatibleSubsystem))
	{
		QueueUserDependentActionInternal(UserId, CompatibleSubsystem, MoveTemp(UserActionFunc), bExecutePostInit);
	}
}

void USocialToolkit::QueueUserDependentActionInternal(const FUniqueNetIdRepl& SubsystemId, ESocialSubsystem SubsystemType, TFunction<void(USocialUser&)>&& UserActionFunc, bool bExecutePostInit)
{
	if (!ensure(SubsystemId.IsValid()))
	{
		return;
	}
	
	USocialUser* User = FindUser(SubsystemId);
	if (!User)
	{
		if (SubsystemType == ESocialSubsystem::Primary)
		{
			User = NewObject<USocialUser>(this, SocialUserClass);
			AllUsers.Add(User);
			User->Initialize(SubsystemId);
		}
		else
		{
			// Check to see if this external Id has already been mapped
			IOnlineUserPtr UserInterface = Online::GetUserInterfaceChecked(GetWorld(), USocialManager::GetSocialOssName(ESocialSubsystem::Primary));

			FExternalIdQueryOptions QueryOptions;
			QueryOptions.AuthType = GetSocialOss(SubsystemType)->GetIdentityInterface()->GetAuthType();
			FUniqueNetIdRepl MappedPrimaryId = UserInterface->GetExternalIdMapping(QueryOptions, SubsystemId.ToString());
			if (MappedPrimaryId.IsValid())
			{
				HandleMapExternalIdComplete(SubsystemType, true, MappedPrimaryId, SubsystemId, UserActionFunc, bExecutePostInit);
				return;
			}
			else
			{
				// Gotta map this non-primary Id to the corresponding primary one (if there is one) before we can make a user
				UE_LOG(LogParty, VeryVerbose, TEXT("Mapping primary Id for unknown, unmapped external Id [%s] for user action"), *SubsystemId.ToDebugString());

				FUniqueNetIdRepl LocalUserPrimaryNetId = GetLocalUserNetId(ESocialSubsystem::Primary);
				auto QueryCompleteHandler = FSocialQuery_MapExternalIds::FOnQueryComplete::CreateUObject(this, &USocialToolkit::HandleMapExternalIdComplete, SubsystemId, UserActionFunc, bExecutePostInit);
				FSocialQueryManager::AddUserId<FSocialQuery_MapExternalIds>(*this, SubsystemType, SubsystemId.ToString(), QueryCompleteHandler);
			}
		}
	}

	if (User && UserActionFunc)
	{
		if (User->IsInitialized() || !bExecutePostInit)
		{
			UserActionFunc(*User);
		}
		else
		{
			User->RegisterInitCompleteHandler(FOnNewSocialUserInitialized::FDelegate::CreateLambda(UserActionFunc));
		}
	}
}

void USocialToolkit::HandleControllerIdChanged(int32 NewId, int32 OldId)
{
	IOnlineSubsystem* PrimaryOss = GetSocialOss(ESocialSubsystem::Primary);
	if (const IOnlineIdentityPtr IdentityInterface = PrimaryOss ? PrimaryOss->GetIdentityInterface() : nullptr)
	{
		IdentityInterface->ClearOnLoginCompleteDelegates(OldId, this);
		IdentityInterface->ClearOnLoginStatusChangedDelegates(OldId, this);
		IdentityInterface->ClearOnLogoutCompleteDelegates(OldId, this);

		IdentityInterface->AddOnLoginStatusChangedDelegate_Handle(NewId, FOnLoginStatusChangedDelegate::CreateUObject(this, &USocialToolkit::HandlePlayerLoginStatusChanged));

		if (IdentityInterface->GetLoginStatus(NewId) == ELoginStatus::LoggedIn)
		{
			UE_CLOG(OldId != INVALID_CONTROLLERID, LogParty, Error, TEXT("SocialToolkit updating controller IDs for local player while logged in. That makes no sense! OldId = [%d], NewId = [%d]"), OldId, NewId);

			TSharedPtr<const FUniqueNetId> LocalUserId = IdentityInterface->GetUniquePlayerId(NewId);
			if (ensure(LocalUserId))
			{
				HandlePlayerLoginStatusChanged(NewId, ELoginStatus::NotLoggedIn, ELoginStatus::LoggedIn, *LocalUserId);
			}
		}
	}
}

void USocialToolkit::RequestDisplayPlatformSocialUI() const
{
	//@todo DanH Social: If the local player is on a platform with its own Social overlay, show it #required

	/*if (ShouldShowExternalFriendsUI())
	{
		if (IOnlineSubsystem* PlatformOSS = UFortGlobals::GetPlatformOSS(GetWorld()))
		{
			IOnlineExternalUIPtr ExternalUI = PlatformOSS->GetExternalUIInterface();
			if (ExternalUI.IsValid())
			{
				const UFortLocalPlayer& LocalPlayer = GetFortLocalPlayer();
				if (ExternalUI->ShowFriendsUI(LocalPlayer.GetControllerId()))
				{
					return;
				}
			}
		}
	}*/
}

void USocialToolkit::NotifySubsystemIdEstablished(USocialUser& SocialUser, ESocialSubsystem SubsystemType, const FUniqueNetIdRepl& SubsystemId)
{
	if (ensure(!UsersBySubsystemIds.Contains(SubsystemId)))
	{
		UE_LOG(LogParty, VeryVerbose, TEXT("Toolkit [%d] establishing subsystem Id [%s] for user [%s]"), GetLocalUserNum(), *SubsystemId.ToDebugString(), *SocialUser.ToDebugString());
		UsersBySubsystemIds.Add(SubsystemId, &SocialUser);
	}
	else
	{
		FString LogString = FString::Printf(TEXT("SubsystemId [%s] for user [%s] is already in the UsersBySubsystemId map.\n"), *SubsystemId.ToDebugString(), *SocialUser.GetName());

		LogString += TEXT("Currently in the map:\n");
		for (const auto& IdUserPair : UsersBySubsystemIds)
		{
			LogString += FString::Printf(TEXT("ID: [%s], User: [%s]\n"), *IdUserPair.Key.ToDebugString(), IdUserPair.Value.IsValid() ? *IdUserPair.Value->GetName() : TEXT("ERROR - INVALID USER!"));
		}

		UE_LOG(LogParty, Error, TEXT("%s"), *LogString);
	}
}

bool USocialToolkit::TrySendFriendInvite(USocialUser& SocialUser, ESocialSubsystem SubsystemType) const
{
	if (SocialUser.GetFriendInviteStatus(SubsystemType) == EInviteStatus::PendingOutbound)
	{
		OnFriendInviteSent().Broadcast(SocialUser, SubsystemType);
		return true;
	}
	else if (!SocialUser.IsFriend(SubsystemType))
	{
		IOnlineFriendsPtr FriendsInterface = Online::GetFriendsInterface(GetWorld(), USocialManager::GetSocialOssName(SubsystemType));
		const FUniqueNetIdRepl SubsystemId = SocialUser.GetUserId(SubsystemType);

		const bool bIsFriendshipRestricted = IsFriendshipRestricted(SocialUser, SubsystemType);

		if (FriendsInterface && SubsystemId.IsValid() && !bIsFriendshipRestricted)
		{
			return FriendsInterface->SendInvite(GetLocalUserNum(), *SubsystemId, FriendListToQuery, FOnSendInviteComplete::CreateUObject(this, &USocialToolkit::HandleFriendInviteSent, SubsystemType, SocialUser.GetDisplayName()));
		}
	}
	return false;
}

bool USocialToolkit::IsFriendshipRestricted(const USocialUser& SocialUser, ESocialSubsystem SubsystemType) const
{
	return false;
}

//@todo DanH: Rename this in a way that keeps the intent but relates that more than just the primary login has completed (i.e. the game has also completed whatever specific stuff it wants to for login as well)
void USocialToolkit::OnOwnerLoggedIn()
{
	UE_LOG(LogParty, Log, TEXT("LocalPlayer [%d] has logged in - starting up SocialToolkit."), GetLocalUserNum());

	// Establish the owning player's ID on each subsystem and bind to events for general social goings-on
	const int32 LocalUserNum = GetLocalUserNum();
	for (ESocialSubsystem SubsystemType : USocialManager::GetDefaultSubsystems())
	{
		FUniqueNetIdRepl LocalUserNetId = LocalUser->GetUserId(SubsystemType);
		if (LocalUserNetId.IsValid())
		{
			IOnlineSubsystem* OSS = GetSocialOss(SubsystemType);
			check(OSS);
			if (IOnlineFriendsPtr FriendsInterface = OSS->GetFriendsInterface())
			{
				FriendsInterface->AddOnFriendRemovedDelegate_Handle(FOnFriendRemovedDelegate::CreateUObject(this, &USocialToolkit::HandleFriendRemoved, SubsystemType));
				FriendsInterface->AddOnDeleteFriendCompleteDelegate_Handle(LocalUserNum, FOnDeleteFriendCompleteDelegate::CreateUObject(this, &USocialToolkit::HandleDeleteFriendComplete, SubsystemType));

				FriendsInterface->AddOnInviteReceivedDelegate_Handle(FOnInviteReceivedDelegate::CreateUObject(this, &USocialToolkit::HandleFriendInviteReceived, SubsystemType));
				FriendsInterface->AddOnInviteAcceptedDelegate_Handle(FOnInviteAcceptedDelegate::CreateUObject(this, &USocialToolkit::HandleFriendInviteAccepted, SubsystemType));
				FriendsInterface->AddOnInviteRejectedDelegate_Handle(FOnInviteRejectedDelegate::CreateUObject(this, &USocialToolkit::HandleFriendInviteRejected, SubsystemType));

				FriendsInterface->AddOnBlockedPlayerCompleteDelegate_Handle(LocalUserNum, FOnBlockedPlayerCompleteDelegate::CreateUObject(this, &USocialToolkit::HandleBlockPlayerComplete, SubsystemType));
				FriendsInterface->AddOnUnblockedPlayerCompleteDelegate_Handle(LocalUserNum, FOnUnblockedPlayerCompleteDelegate::CreateUObject(this, &USocialToolkit::HandleUnblockPlayerComplete, SubsystemType));

				FriendsInterface->AddOnRecentPlayersAddedDelegate_Handle(FOnRecentPlayersAddedDelegate::CreateUObject(this, &USocialToolkit::HandleRecentPlayersAdded, SubsystemType));
			}

			if (IOnlinePartyPtr PartyInterface = OSS->GetPartyInterface())
			{
				PartyInterface->AddOnPartyInviteReceivedDelegate_Handle(FOnPartyInviteReceivedDelegate::CreateUObject(this, &USocialToolkit::HandlePartyInviteReceived));
			}

			if (IOnlinePresencePtr PresenceInterface = OSS->GetPresenceInterface())
			{
				PresenceInterface->AddOnPresenceReceivedDelegate_Handle(FOnPresenceReceivedDelegate::CreateUObject(this, &USocialToolkit::HandlePresenceReceived, SubsystemType));
			}
		}
	}

	// Now that everything is set up, immediately query whatever we can
	if (bQueryFriendsOnStartup)
	{
		QueryFriendsLists();
	}
	if (bQueryBlockedPlayersOnStartup)
	{
		QueryBlockedPlayers();
	}
	if (bQueryRecentPlayersOnStartup)
	{
		QueryRecentPlayers();
	}
}

void USocialToolkit::OnOwnerLoggedOut()
{
	UE_LOG(LogParty, Log, TEXT("LocalPlayer [%d] has logged out - wiping user roster from SocialToolkit."), GetLocalUserNum());

	const int32 LocalUserNum = GetLocalUserNum();
	for (ESocialSubsystem SubsystemType : USocialManager::GetDefaultSubsystems())
	{
		if (IOnlineSubsystem* OSS = GetSocialOss(SubsystemType))
		{
			IOnlineFriendsPtr FriendsInterface = OSS->GetFriendsInterface();
			if (FriendsInterface.IsValid())
			{
				FriendsInterface->ClearOnFriendRemovedDelegates(this);
				FriendsInterface->ClearOnDeleteFriendCompleteDelegates(LocalUserNum, this);

				FriendsInterface->ClearOnInviteReceivedDelegates(this);
				FriendsInterface->ClearOnInviteAcceptedDelegates(this);
				FriendsInterface->ClearOnInviteRejectedDelegates(this);

				FriendsInterface->ClearOnBlockedPlayerCompleteDelegates(LocalUserNum, this);
				FriendsInterface->ClearOnUnblockedPlayerCompleteDelegates(LocalUserNum, this);

				FriendsInterface->ClearOnQueryBlockedPlayersCompleteDelegates(this);
				FriendsInterface->ClearOnQueryRecentPlayersCompleteDelegates(this);
			}

			IOnlinePartyPtr PartyInterface = OSS->GetPartyInterface();
			if (PartyInterface.IsValid())
			{
				PartyInterface->ClearOnPartyInviteReceivedDelegates(this);
			}

			IOnlineUserPtr UserInterface = OSS->GetUserInterface();
			if (UserInterface.IsValid())
			{
				UserInterface->ClearOnQueryUserInfoCompleteDelegates(LocalUserNum, this);
			}

			IOnlinePresencePtr PresenceInterface = OSS->GetPresenceInterface();
			if (PresenceInterface.IsValid())
			{
				PresenceInterface->ClearOnPresenceArrayUpdatedDelegates(this);
			}
		}
	}

	UsersBySubsystemIds.Reset();
	AllUsers.Reset();

	// Remake a fresh uninitialized local user
	LocalUser = NewObject<USocialUser>(this, SocialUserClass);

	OnToolkitReset().Broadcast();
}

void USocialToolkit::QueryFriendsLists()
{
	for (ESocialSubsystem SubsystemType : USocialManager::GetDefaultSubsystems())
	{
		const FUniqueNetIdRepl LocalUserNetId = LocalUser->GetUserId(SubsystemType);
		if (LocalUserNetId.IsValid())
		{
			IOnlineSubsystem* OSS = GetSocialOss(SubsystemType);
			check(OSS);

			if (IOnlineFriendsPtr FriendsInterface = OSS->GetFriendsInterface())
			{
				FriendsInterface->ReadFriendsList(GetLocalUserNum(), FriendListToQuery, FOnReadFriendsListComplete::CreateUObject(this, &USocialToolkit::HandleReadFriendsListComplete, SubsystemType));
			}
		}
	}
}

void USocialToolkit::QueryBlockedPlayers()
{
	for (ESocialSubsystem SubsystemType : USocialManager::GetDefaultSubsystems())
	{
		const FUniqueNetIdRepl LocalUserSubsystemId = LocalUser->GetUserId(SubsystemType);
		if (LocalUserSubsystemId.IsValid())
		{
			IOnlineSubsystem* OSS = GetSocialOss(SubsystemType);
			check(OSS);

			if (IOnlineFriendsPtr FriendsInterface = OSS->GetFriendsInterface())
			{
				//@todo DanH Social: There is an inconsistency in OSS interfaces - some just return false for unimplemented features while others return false and trigger the callback
				//		Seems like they should return false if the feature isn't implemented and trigger the callback for failure if it is implemented and couldn't start
				//		As it is now, there are two ways to know if the call didn't succeed and zero ways to know if it ever could
				FriendsInterface->AddOnQueryBlockedPlayersCompleteDelegate_Handle(FOnQueryBlockedPlayersCompleteDelegate::CreateUObject(this, &USocialToolkit::HandleQueryBlockedPlayersComplete, SubsystemType));
				if (!FriendsInterface->QueryBlockedPlayers(*LocalUserSubsystemId))
				{
					FriendsInterface->ClearOnQueryBlockedPlayersCompleteDelegates(this);
				}
			}
		}
	}
}

void USocialToolkit::QueryRecentPlayers()
{
	for (ESocialSubsystem SubsystemType : USocialManager::GetDefaultSubsystems())
	{
		const FUniqueNetIdRepl LocalUserSubsystemId = LocalUser->GetUserId(SubsystemType);
		if (LocalUserSubsystemId.IsValid())
		{
			IOnlineSubsystem* OSS = GetSocialOss(SubsystemType);
			check(OSS);

			if (IOnlineFriendsPtr FriendsInterface = OSS->GetFriendsInterface())
			{
				FriendsInterface->AddOnQueryRecentPlayersCompleteDelegate_Handle(FOnQueryRecentPlayersCompleteDelegate::CreateUObject(this, &USocialToolkit::HandleQueryRecentPlayersComplete, SubsystemType));
				if (RecentPlayerNamespaceToQuery.IsEmpty() || !FriendsInterface->QueryRecentPlayers(*LocalUserSubsystemId, RecentPlayerNamespaceToQuery))
				{
					FriendsInterface->ClearOnQueryRecentPlayersCompleteDelegates(this);
				}
			}
		}
	}
}

void USocialToolkit::HandlePlayerLoginStatusChanged(int32 LocalUserNum, ELoginStatus::Type OldStatus, ELoginStatus::Type NewStatus, const FUniqueNetId& NewId)
{
	if (LocalUserNum == GetLocalUserNum())
	{
		if (NewStatus == ELoginStatus::LoggedIn)
		{
			if (!ensure(AllUsers.Num() == 0))
			{
				// Nobody told us we logged out! Handle it now just so we're fresh, but not good!
				OnOwnerLoggedOut();
			}

			AllUsers.Add(LocalUser);
			LocalUser->InitLocalUser();

			if (IsOwnerLoggedIn())
			{
				OnOwnerLoggedIn();
			}
		}
		else if (NewStatus == ELoginStatus::NotLoggedIn)
		{
			OnOwnerLoggedOut();
		}
	}
}

void USocialToolkit::HandleReadFriendsListComplete(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr, ESocialSubsystem SubsystemType)
{
	UE_LOG(LogParty, Log, TEXT("SocialToolkit [%d] finished querying friends list [%s] on subsystem [%s] with error [%s]."), GetLocalUserNum(), *ListName, ToString(SubsystemType), *ErrorStr);
	if (bWasSuccessful)
	{
		TArray<TSharedRef<FOnlineFriend>> FriendsList;
		IOnlineFriendsPtr FriendsInterface = Online::GetFriendsInterfaceChecked(GetWorld(), USocialManager::GetSocialOssName(SubsystemType));
		FriendsInterface->GetFriendsList(LocalUserNum, ListName, FriendsList);

		//@todo DanH: This isn't actually quite correct - some of these could actually just be friend info for pending invites, not fully accepted friends. Should piece out the list into respective categories and process each separately (or make the associated event determination more complex)
		ProcessUserList(FriendsList, SubsystemType, OnFriendshipEstablished());
		OnQueryFriendsListSuccess(SubsystemType, FriendsList);
	}
	else
	{
		//@todo DanH: This is a really big deal on primary and a frustrating deal on platform
		// In both cases I think we should give it another shot, but I dunno how long to wait and if we should behave differently between the two
	}
}

void USocialToolkit::HandleQueryBlockedPlayersComplete(const FUniqueNetId& UserId, bool bWasSuccessful, const FString& ErrorStr, ESocialSubsystem SubsystemType)
{
	if (UserId == *GetLocalUserNetId(SubsystemType))
	{
		UE_LOG(LogParty, Log, TEXT("SocialToolkit [%d] finished querying blocked players on subsystem [%s] with error [%s]."), GetLocalUserNum(), ToString(SubsystemType), *ErrorStr);

		if (bWasSuccessful)
		{
			IOnlineFriendsPtr FriendsInterface = Online::GetFriendsInterfaceChecked(GetWorld(), USocialManager::GetSocialOssName(SubsystemType));
			FriendsInterface->ClearOnQueryBlockedPlayersCompleteDelegates(this);

			TArray<TSharedRef<FOnlineBlockedPlayer>> BlockedPlayers;
			FriendsInterface->GetBlockedPlayers(UserId, BlockedPlayers);
			ProcessUserList(BlockedPlayers, SubsystemType, OnUserBlocked());
			OnQueryBlockedPlayersSuccess(SubsystemType, BlockedPlayers);
		}
		else
		{
			//@todo DanH: Only bother retrying on primary
		}
	}
}

void USocialToolkit::HandleQueryRecentPlayersComplete(const FUniqueNetId& UserId, const FString& Namespace, bool bWasSuccessful, const FString& ErrorStr, ESocialSubsystem SubsystemType)
{
	if (UserId == *GetLocalUserNetId(SubsystemType))
	{
		UE_LOG(LogParty, Log, TEXT("SocialToolkit [%d] finished querying recent player list [%s] on subsystem [%s] with error [%s]."), GetLocalUserNum(), *Namespace, ToString(SubsystemType), *ErrorStr);

		if (bWasSuccessful)
		{
			IOnlineFriendsPtr FriendsInterface = Online::GetFriendsInterfaceChecked(GetWorld(), USocialManager::GetSocialOssName(SubsystemType));
			FriendsInterface->ClearOnQueryRecentPlayersCompleteDelegates(this);

			TArray<TSharedRef<FOnlineRecentPlayer>> RecentPlayers;
			FriendsInterface->GetRecentPlayers(UserId, Namespace, RecentPlayers);
			ProcessUserList(RecentPlayers, SubsystemType, OnRecentPlayerAdded());
			OnQueryRecentPlayersSuccess(SubsystemType, RecentPlayers);
		}
		else
		{
			//@todo DanH: Only bother retrying on primary
		}
	}
}

void USocialToolkit::HandleRecentPlayersAdded(const FUniqueNetId& LocalUserId, const TArray<TSharedRef<FOnlineRecentPlayer>>& NewRecentPlayers, ESocialSubsystem SubsystemType)
{
	if (LocalUserId == *GetLocalUserNetId(SubsystemType))
	{
		for (const TSharedRef<FOnlineRecentPlayer>& RecentPlayerInfo : NewRecentPlayers)
		{
			QueueUserDependentActionInternal(RecentPlayerInfo->GetUserId(), SubsystemType,
				[this, RecentPlayerInfo, SubsystemType] (USocialUser& User)
				{
					User.EstablishOssInfo(RecentPlayerInfo, SubsystemType);
					OnRecentPlayerAdded().Broadcast(User, SubsystemType, true);
				});
		}
	}
}

void USocialToolkit::HandleMapExternalIdComplete(ESocialSubsystem SubsystemType, bool bWasSuccessful, const FUniqueNetIdRepl& MappedPrimaryId, FUniqueNetIdRepl ExternalId, TFunction<void(USocialUser&)> UserActionFunc, bool bExecutePostInit)
{
	if (bWasSuccessful && MappedPrimaryId.IsValid())
	{
		QueueUserDependentActionInternal(MappedPrimaryId, ESocialSubsystem::Primary,
			[this, SubsystemType, ExternalId, UserActionFunc] (USocialUser& User)
			{
				// Make sure the primary user info agreed about the external Id
				if (ensure(User.GetUserId(SubsystemType) == ExternalId) && UserActionFunc)
				{
					UserActionFunc(User);
				}
			}
			//@todo DanH: Since we're relying on the primary UserInfo as the authority here, platform ID-based queued actions always execute post-init. Revisit this #future
			/*, bExecutePostInit*/);
	}
}

void USocialToolkit::HandlePresenceReceived(const FUniqueNetId& UserId, const TSharedRef<FOnlineUserPresence>& NewPresence, ESocialSubsystem SubsystemType)
{
	if (USocialUser* UpdatedUser = FindUser(UserId))
	{
		UpdatedUser->NotifyPresenceChanged(SubsystemType);
	}
	else if (SubsystemType == ESocialSubsystem::Platform)
	{
		FString ErrorString = TEXT("Platform presence received, but existing SocialUser could not be found.\n");
		ErrorString += TEXT("Incoming UserId is ") + UserId.ToString() + TEXT(", as a UniqueIdRepl it's ") + FUniqueNetIdRepl(UserId).ToString();

		ErrorString += TEXT("Outputting all cached platform IDs and the corresponding user: \n") + UserId.ToString();
		for (auto IdUserPair : UsersBySubsystemIds)
		{
			if (IdUserPair.Key.GetType() != MCP_SUBSYSTEM)
			{
				ErrorString += FString::Printf(TEXT("\tUserId [%s]: SocialUser [%s]\n"), *IdUserPair.Key.ToString(), *IdUserPair.Value->ToDebugString());
				if (IdUserPair.Key == FUniqueNetIdRepl(UserId) || !ensure(*IdUserPair.Key != UserId))
				{
					ErrorString += TEXT("\t\tAnd look at that, this one DOES actually match. The map has lied to us!!\n");
				}
			}
		}

		UE_LOG(LogParty, Error, TEXT("%s"), *ErrorString);
	}
}

void USocialToolkit::HandleQueryPrimaryUserIdMappingComplete(bool bWasSuccessful, const FUniqueNetId& RequestingUserId, const FString& DisplayName, const FUniqueNetId& IdentifiedUserId, const FString& Error)
{
	if (!IdentifiedUserId.IsValid())
	{
		NotifyFriendInviteFailed(IdentifiedUserId, DisplayName, ESendFriendInviteFailureReason::NotFound);
	}
	else if (RequestingUserId == IdentifiedUserId)
	{
		NotifyFriendInviteFailed(IdentifiedUserId, DisplayName, ESendFriendInviteFailureReason::AddingSelfFail);
	}
	else
	{
		QueueUserDependentActionInternal(IdentifiedUserId, ESocialSubsystem::Primary,
			[this, DisplayName] (USocialUser& SocialUser)
			{
				if (SocialUser.IsBlocked())
				{
					NotifyFriendInviteFailed(*SocialUser.GetUserId(ESocialSubsystem::Primary), DisplayName, ESendFriendInviteFailureReason::AddingBlockedFail);
				}
				else if (SocialUser.IsFriend(ESocialSubsystem::Primary))
				{
					NotifyFriendInviteFailed(*SocialUser.GetUserId(ESocialSubsystem::Primary), DisplayName, ESendFriendInviteFailureReason::AlreadyFriends);
				}
				else
				{
					TrySendFriendInvite(SocialUser, ESocialSubsystem::Primary);
				}
			});
	}
}

void USocialToolkit::HandleFriendInviteReceived(const FUniqueNetId& LocalUserId, const FUniqueNetId& SenderId, ESocialSubsystem SubsystemType)
{
	if (LocalUserId == *GetLocalUserNetId(SubsystemType))
	{
		QueueUserDependentActionInternal(SenderId, SubsystemType,
			[this, SubsystemType] (USocialUser& SocialUser)
			{
				//@todo DanH: This event should send the name of the list the accepting friend is on, shouldn't it?
				IOnlineFriendsPtr FriendsInterface = Online::GetFriendsInterfaceChecked(GetWorld(), USocialManager::GetSocialOssName(SubsystemType));
				if (TSharedPtr<FOnlineFriend> OssFriend = FriendsInterface->GetFriend(GetLocalUserNum(), *SocialUser.GetUserId(SubsystemType), FriendListToQuery))
				{
					SocialUser.EstablishOssInfo(OssFriend.ToSharedRef(), SubsystemType);
					if (ensure(SocialUser.GetFriendInviteStatus(SubsystemType) == EInviteStatus::PendingInbound))
					{
						OnFriendInviteReceived().Broadcast(SocialUser, SubsystemType);
					}
				}
			});
	}
}

void USocialToolkit::HandleFriendInviteAccepted(const FUniqueNetId& LocalUserId, const FUniqueNetId& FriendId, ESocialSubsystem SubsystemType)
{
	if (LocalUserId == *GetLocalUserNetId(SubsystemType))
	{
		QueueUserDependentActionInternal(FriendId, SubsystemType,
			[this, SubsystemType] (USocialUser& SocialUser)
			{
				//@todo DanH: This event should send the name of the list the accepting friend is on, shouldn't it?
				IOnlineFriendsPtr FriendsInterface = Online::GetFriendsInterfaceChecked(GetWorld(), USocialManager::GetSocialOssName(SubsystemType));
				if (TSharedPtr<FOnlineFriend> OssFriend = FriendsInterface->GetFriend(GetLocalUserNum(), *SocialUser.GetUserId(SubsystemType), FriendListToQuery))
				{
					SocialUser.EstablishOssInfo(OssFriend.ToSharedRef(), SubsystemType);
					if (SocialUser.IsFriend(SubsystemType))
					{
						OnFriendshipEstablished().Broadcast(SocialUser, SubsystemType, true);
					}
				}
			});
	}
}

void USocialToolkit::HandleFriendInviteRejected(const FUniqueNetId& LocalUserId, const FUniqueNetId& FriendId, ESocialSubsystem SubsystemType)
{
	if (LocalUserId == *GetLocalUserNetId(SubsystemType))
	{
		if (USocialUser* InvitedUser = FindUser(FriendId))
		{
			InvitedUser->NotifyFriendInviteRemoved(SubsystemType);
		}
	}
}

void USocialToolkit::HandleFriendInviteSent(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& InvitedUserId, const FString& ListName, const FString& ErrorStr, ESocialSubsystem SubsystemType, FString DisplayName)
{
	if (bWasSuccessful)
	{
		QueueUserDependentActionInternal(InvitedUserId, SubsystemType,
			[this, SubsystemType, ListName] (USocialUser& SocialUser)
			{
				IOnlineFriendsPtr FriendsInterface = Online::GetFriendsInterfaceChecked(GetWorld(), USocialManager::GetSocialOssName(SubsystemType));
				if (TSharedPtr<FOnlineFriend> OssFriend = FriendsInterface->GetFriend(GetLocalUserNum(), *SocialUser.GetUserId(SubsystemType), ListName))
				{
					SocialUser.EstablishOssInfo(OssFriend.ToSharedRef(), SubsystemType);
					if (SocialUser.GetFriendInviteStatus(SubsystemType) == EInviteStatus::PendingOutbound)
					{
						OnFriendInviteSent().Broadcast(SocialUser, SubsystemType);
					}
				}
			});
	}
	else
	{
		NotifyFriendInviteFailed(InvitedUserId, ErrorStr, ESendFriendInviteFailureReason::UnknownError, false);
	}
}

void USocialToolkit::HandleFriendRemoved(const FUniqueNetId& LocalUserId, const FUniqueNetId& FriendId, ESocialSubsystem SubsystemType)
{
	if (LocalUserId == *GetLocalUserNetId(SubsystemType))
	{
		USocialUser* FormerFriend = FindUser(FriendId);
		if (ensure(FormerFriend))
		{
			FormerFriend->NotifyUserUnfriended(SubsystemType);
		}
	}
}

void USocialToolkit::HandleDeleteFriendComplete(int32 InLocalUserNum, bool bWasSuccessful, const FUniqueNetId& DeletedFriendId, const FString& ListName, const FString& ErrorStr, ESocialSubsystem SubsystemType)
{
	if (bWasSuccessful && InLocalUserNum == GetLocalUserNum())
	{
		USocialUser* FormerFriend = FindUser(DeletedFriendId);
		if (ensure(FormerFriend))
		{
			FormerFriend->NotifyUserUnfriended(SubsystemType);
		}
	}
}

void USocialToolkit::HandlePartyInviteReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& SenderId)
{
	if (LocalUserId == *GetLocalUserNetId(ESocialSubsystem::Primary))
	{
		// We really should know about the sender of the invite already, but queue it up in case we receive it during initial setup
		QueueUserDependentActionInternal(SenderId, ESocialSubsystem::Primary,
			[this] (USocialUser& User)
			{
				if (User.IsFriend(ESocialSubsystem::Primary))
				{
					OnPartyInviteReceived().Broadcast(User);
				}
			});
	}
}

void USocialToolkit::HandleBlockPlayerComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& BlockedPlayerId, const FString& ListName, const FString& ErrorStr, ESocialSubsystem SubsystemType)
{
	if (bWasSuccessful && LocalUserNum == GetLocalUserNum())
	{
		QueueUserDependentActionInternal(BlockedPlayerId, SubsystemType, 
			[this, SubsystemType] (USocialUser& User)
			{
				// Quite frustrating that the event doesn't sent the FOnlineBlockedPlayer in the first place or provide a direct getter on the interface...
				TArray<TSharedRef<FOnlineBlockedPlayer>> AllBlockedPlayers;
				IOnlineFriendsPtr FriendsInterface = Online::GetFriendsInterfaceChecked(GetWorld(), USocialManager::GetSocialOssName(SubsystemType));
				if (FriendsInterface->GetBlockedPlayers(*GetLocalUserNetId(SubsystemType), AllBlockedPlayers))
				{
					FUniqueNetIdRepl BlockedUserId = User.GetUserId(SubsystemType);
					const TSharedRef<FOnlineBlockedPlayer>* BlockedPlayerInfoPtr = AllBlockedPlayers.FindByPredicate(
						[&BlockedUserId] (TSharedRef<FOnlineBlockedPlayer> BlockedPlayerInfo)
						{
							return *BlockedPlayerInfo->GetUserId() == *BlockedUserId;
						});

					if (BlockedPlayerInfoPtr)
					{
						User.EstablishOssInfo(*BlockedPlayerInfoPtr, SubsystemType);
						OnUserBlocked().Broadcast(User, SubsystemType, true);
					}
				}
			});
	}
}

void USocialToolkit::HandleUnblockPlayerComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UnblockedPlayerId, const FString& ListName, const FString& ErrorStr, ESocialSubsystem SubsystemType)
{
	if (bWasSuccessful && LocalUserNum == GetLocalUserNum())
	{
		USocialUser* UnblockedUser = FindUser(UnblockedPlayerId);
		if (ensure(UnblockedUser))
		{
			UnblockedUser->NotifyUserUnblocked(SubsystemType);
		}
	}
}

//@todo DanH recent players: Where is the line for this between backend and game to update this stuff? #required
//		Seems like I should just be able to get an event for OnRecentPlayersAdded or even a full OnRecentPlayersListRefreshed from IOnlineFriends
void USocialToolkit::HandlePartyMemberExited(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& MemberId, const EMemberExitedReason Reason)
{
	// If the party member wasn't a friend, they're now a recent player
}

void USocialToolkit::HandleGameDestroyed(const FName SessionName, bool bWasSuccessful)
{
	// Update the recent player list whenever a game session ends
}