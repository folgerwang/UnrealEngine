// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "User/SocialUserList.h"
#include "User/SocialUser.h"
#include "SocialToolkit.h"
#include "Party/PartyMember.h"

#include "Containers/Ticker.h"
#include "Interfaces/OnlinePresenceInterface.h"

TSharedRef<FSocialUserList> FSocialUserList::CreateUserList(USocialToolkit& InOwnerToolkit, const FSocialUserListConfig& InConfig)
{
	TSharedRef<FSocialUserList> NewList = MakeShareable(new FSocialUserList(InOwnerToolkit, InConfig));
	NewList->InitializeList();
	return NewList;
}

FSocialUserList::FSocialUserList(USocialToolkit& InOwnerToolkit, const FSocialUserListConfig& InConfig)
	: OwnerToolkit(&InOwnerToolkit)
	, ListConfig(InConfig)
{
	if (HasPresenceFilters() &&
		ListConfig.RelationshipType != ESocialRelationship::Friend &&
		ListConfig.RelationshipType != ESocialRelationship::PartyInvite)
	{
		UE_LOG(LogParty, Error, TEXT("A user list with presence filters can only ever track friends. No users will ever appear in this list."));
	}
}

void FSocialUserList::InitializeList()
{
	check(OwnerToolkit.IsValid());

	// Bind appropriate events based on the desired relationship filter
	switch (ListConfig.RelationshipType)
	{
	case ESocialRelationship::FriendInviteReceived:
		OwnerToolkit->OnFriendInviteReceived().AddSP(this, &FSocialUserList::HandleFriendInviteReceived);
	case ESocialRelationship::FriendInviteSent:
		OwnerToolkit->OnFriendshipEstablished().AddSP(this, &FSocialUserList::HandleFriendshipEstablished);
		break;
	case ESocialRelationship::PartyInvite:
		OwnerToolkit->OnPartyInviteReceived().AddSP(this, &FSocialUserList::HandlePartyInviteReceived);
		break;
	case ESocialRelationship::Friend:
		OwnerToolkit->OnFriendshipEstablished().AddSP(this, &FSocialUserList::HandleFriendshipEstablished);
		break;
	case ESocialRelationship::RecentPlayer:
		OwnerToolkit->OnRecentPlayerAdded().AddSP(this, &FSocialUserList::HandleRecentPlayerAdded);
		OwnerToolkit->OnFriendshipEstablished().AddSP(this, &FSocialUserList::HandleFriendshipEstablished);
		break;
	}

	OwnerToolkit->OnToolkitReset().AddSP(this, &FSocialUserList::HandleOwnerToolkitReset);
	OwnerToolkit->OnUserBlocked().AddSP(this, &FSocialUserList::HandleUserBlocked);

	// Run through all the users on the toolkit and add any that qualify for this list
	for (USocialUser* User : OwnerToolkit->GetAllUsers())
	{
		check(User);
		TryAddUserFast(*User);
	}

	SetAutoUpdatePeriod(AutoUpdatePeriod);
}

void FSocialUserList::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(Users);
}

void FSocialUserList::UpdateNow()
{
	HandleAutoUpdateList(0.f);
}

void FSocialUserList::SetAutoUpdatePeriod(float InAutoUpdatePeriod)
{
	AutoUpdatePeriod = InAutoUpdatePeriod;
	if (UpdateTickerHandle.IsValid())
	{
		FTicker::GetCoreTicker().RemoveTicker(UpdateTickerHandle);
	}

	if (InAutoUpdatePeriod >= 0.f)
	{
		UpdateTickerHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &FSocialUserList::HandleAutoUpdateList), InAutoUpdatePeriod);
	}
}

bool FSocialUserList::HasPresenceFilters() const
{
	return ListConfig.RequiredPresenceFlags != ESocialUserStateFlags::None || ListConfig.ForbiddenPresenceFlags != ESocialUserStateFlags::None;
}

void FSocialUserList::HandleOwnerToolkitReset()
{
	const bool bTriggerChangeEvent = Users.Num() > 0;

	for (const USocialUser* User : Users)
	{
		if (ensureMsgf(User, TEXT("Encountered a nullptr entry in FSocialUserList::Users array!")))
		{
			OnUserRemoved().Broadcast(*User);
		}
	}
	Users.Reset();
	PendingAdds.Reset();
	PendingRemovals.Reset();
	UsersWithDirtyPresence.Reset();
	if (bTriggerChangeEvent)
	{
		OnUpdateComplete().Broadcast();
	}
}

void FSocialUserList::HandlePartyInviteReceived(USocialUser& InvitingUser)
{
	TryAddUser(InvitingUser);
}

void FSocialUserList::HandlePartyInviteHandled(USocialUser* InvitingUser)
{
	TryRemoveUser(*InvitingUser);
	UpdateNow();
}

void FSocialUserList::HandleFriendInviteReceived(USocialUser& User, ESocialSubsystem SubsystemType)
{
	TryAddUser(User);
}

void FSocialUserList::HandleFriendInviteRemoved(ESocialSubsystem SubsystemType, USocialUser* User)
{
	TryRemoveUser(*User);
	UpdateNow();
}

void FSocialUserList::HandleFriendshipEstablished(USocialUser& NewFriend, ESocialSubsystem SubsystemType, bool bIsNewRelationship)
{
	if (ListConfig.RelationshipType == ESocialRelationship::Friend ||
		ListConfig.RelationshipType == ESocialRelationship::FriendInviteReceived)
	{
		TryAddUser(NewFriend);
	}

	if (ListConfig.RelationshipType != ESocialRelationship::Friend ||
		ListConfig.RelationshipType == ESocialRelationship::FriendInviteReceived)
	{
		// Any non-friends list that cares about friendship does so to remove entries (i.e. invites & recent players)
		TryRemoveUser(NewFriend);
		UpdateNow();
	}
}

void FSocialUserList::HandleFriendRemoved(ESocialSubsystem SubsystemType, USocialUser* User)
{
	TryRemoveUser(*User);
	UpdateNow();
}

void FSocialUserList::HandleUserBlocked(USocialUser& BlockedUser, ESocialSubsystem SubsystemType, bool bIsNewRelationship)
{
	if (ListConfig.RelationshipType == ESocialRelationship::BlockedPlayer)
	{
		TryAddUser(BlockedUser);
	}
	else
	{
		// When a player is blocked, any other existing relationship is implicitly nixed
		TryRemoveUser(BlockedUser);
	}
	
	UpdateNow();
}

void FSocialUserList::HandleUserBlockStatusChanged(ESocialSubsystem SubsystemType, bool bIsBlocked, USocialUser* User)
{
	if (!bIsBlocked)
	{
		TryRemoveUser(*User);
		UpdateNow();
	}
}

void FSocialUserList::HandleRecentPlayerAdded(USocialUser& AddedUser, ESocialSubsystem SubsystemType, bool bIsNewRelationship)
{
	TryAddUser(AddedUser);
}

void FSocialUserList::HandleRecentPlayerRemoved(USocialUser& RemovedUser, ESocialSubsystem SubsystemType)
{
	TryRemoveUser(RemovedUser);
}

void FSocialUserList::HandleUserPresenceChanged(ESocialSubsystem SubsystemType, USocialUser* User)
{
	// Save this dirtied user for re-evaluation during the next update
	UsersWithDirtyPresence.Add(User);
	bNeedsSort = true;
}

void FSocialUserList::TryAddUser(USocialUser& User)
{
	if (!PendingAdds.Contains(&User) && (!Users.Contains(&User) || PendingRemovals.Contains(&User)))
	{
		TryAddUserFast(User);
	}
	else
	{
		// Something changed about a user already in the list, so we'll need to re-sort
		bNeedsSort = true;
	}
}

void FSocialUserList::TryAddUserFast(USocialUser& User)
{
	bool bCanAdd = false;

	TArray<ESocialSubsystem> ActiveRelationshipSubsystems = User.GetRelationshipSubsystems(ListConfig.RelationshipType);
	for (ESocialSubsystem RelationshipSubsystem : ActiveRelationshipSubsystems)
	{
		// Is the relationship on this subsystem relevant to us?
		if (ListConfig.ForbiddenSubsystems.Contains(RelationshipSubsystem))
		{
			// Immediately bail entirely if the relationship exists on any forbidden subsystems
			return;
		}
		else if (!bCanAdd && ListConfig.RelevantSubsystems.Contains(RelationshipSubsystem))
		{
			// Even if the user does not qualify for the list now due to presence filters, we still want to know about any changes to their presence to reevaluate
			if (HasPresenceFilters() && !User.OnUserPresenceChanged().IsBoundToObject(this))
			{
				User.OnUserPresenceChanged().AddSP(this, &FSocialUserList::HandleUserPresenceChanged, &User);
			}

			// Check that the user's current presence is acceptable
			if (EvaluateUserPresence(User, RelationshipSubsystem))
			{
				// Last step is to check the custom filter, if provided
				bCanAdd = ListConfig.OnCustomFilterUser.IsBound() ? ListConfig.OnCustomFilterUser.Execute(User) : true;
			}
		}
	}

	if (bCanAdd)
	{
		// Bind directly to the user we're adding to find out when we should remove them
		// ** Be sure to unbind within TryRemoveUserFast **
		switch (ListConfig.RelationshipType)
		{
		case ESocialRelationship::FriendInviteReceived:
		case ESocialRelationship::FriendInviteSent:
			User.OnFriendInviteRemoved().AddSP(this, &FSocialUserList::HandleFriendInviteRemoved, &User);
			break;
		case ESocialRelationship::PartyInvite:
			// We don't care whether the invite was accepted or rejected, just that it was handled in some way
			User.OnPartyInviteAccepted().AddSP(this, &FSocialUserList::HandlePartyInviteHandled, &User);
			User.OnPartyInviteRejected().AddSP(this, &FSocialUserList::HandlePartyInviteHandled, &User);
		case ESocialRelationship::Friend:
			User.OnFriendRemoved().AddSP(this, &FSocialUserList::HandleFriendRemoved, &User);
			break;
		case ESocialRelationship::BlockedPlayer:
			User.OnBlockedStatusChanged().AddSP(this, &FSocialUserList::HandleUserBlockStatusChanged, &User);
			break;
		}

		PendingRemovals.Remove(&User);
		PendingAdds.Add(&User);
	}
}

void FSocialUserList::TryRemoveUser(USocialUser& User)
{
	if (!PendingRemovals.Contains(&User) && (Users.Contains(&User) || PendingAdds.Contains(&User)))
	{
		TryRemoveUserFast(User);
	}
}

void FSocialUserList::TryRemoveUserFast(USocialUser& User)
{
	bool bUnbindFromPresenceUpdates = true;
	bool bRemoveUser = true;
	TArray<ESocialSubsystem> ActiveRelationshipSubsystems = User.GetRelationshipSubsystems(ListConfig.RelationshipType);
	for (ESocialSubsystem RelationshipSubsystem : ActiveRelationshipSubsystems)
	{
		if (ListConfig.ForbiddenSubsystems.Contains(RelationshipSubsystem))
		{
			bRemoveUser = true;
			break;
		}
		else if (bRemoveUser && ListConfig.RelevantSubsystems.Contains(RelationshipSubsystem))
		{
			bUnbindFromPresenceUpdates = false;
			if (EvaluateUserPresence(User, RelationshipSubsystem))
			{
				// We're going to keep the user based on the stock filters, but the custom filter can still veto
				bRemoveUser = ListConfig.OnCustomFilterUser.IsBound() ? !ListConfig.OnCustomFilterUser.Execute(User) : false;
			}
		}
	}

	if (bRemoveUser)
	{
		PendingAdds.Remove(&User);
		PendingRemovals.Add(&User);

		// Clear out all direct user bindings
		User.OnFriendInviteRemoved().RemoveAll(this);
		User.OnPartyInviteAccepted().RemoveAll(this);
		User.OnPartyInviteRejected().RemoveAll(this);
		User.OnBlockedStatusChanged().RemoveAll(this);
		User.OnPartyInviteAccepted().RemoveAll(this);
		User.OnPartyInviteRejected().RemoveAll(this);

		if (bUnbindFromPresenceUpdates)
		{
			// Not only does this user not qualify for the list, they don't even have the appropriate relationship anymore (so we no longer care about presence changes)
			User.OnUserPresenceChanged().RemoveAll(this);
		}
	}
}

bool FSocialUserList::EvaluateUserPresence(const USocialUser& User, ESocialSubsystem SubsystemType)
{
	if (HasPresenceFilters())
	{
		if (const FOnlineUserPresence* UserPresence = User.GetFriendPresenceInfo(SubsystemType))
		{
			return EvaluatePresenceFlag(UserPresence->bIsOnline, ESocialUserStateFlags::Online)
				&& EvaluatePresenceFlag(UserPresence->bIsPlayingThisGame, ESocialUserStateFlags::SameApp);
			// && EvaluatePresenceFlag(UserPresence->bIsJoinable, ESocialUserStateFlags::Joinable) <-- //@todo DanH: This property exists on presence, but is ALWAYS false... 
			// && EvaluateFlag(UserPresence->?, ESocialUserStateFlag::SamePlatform)
			// && EvaluateFlag(UserPresence->?, ESocialUserStateFlag::SameParty)
			// && EvaluateFlag(UserPresence->?, ESocialUserStateFlag::LookingForGroup)
		}
		return false;
	}
	return true;
}

bool FSocialUserList::EvaluatePresenceFlag(bool bPresenceValue, ESocialUserStateFlags Flag) const
{
	if (EnumHasAnyFlags(ListConfig.RequiredPresenceFlags, Flag))
	{
		// It's required, so value must be true to be eligible
		return bPresenceValue;
	}
	else if (EnumHasAnyFlags(ListConfig.ForbiddenPresenceFlags, Flag))
	{
		// It's forbidden, so value must be false to be eligible
		return !bPresenceValue;
	}
	// Irrelevant
	return true;
}

bool FSocialUserList::HandleAutoUpdateList(float)
{
	// Re-evaluate whether each user with dirtied presence is still fit for the list
	for (TWeakObjectPtr<USocialUser> DirtyUser : UsersWithDirtyPresence)
	{
		const bool bContainsUser = Users.Contains(DirtyUser);
		const bool bPendingAdd = PendingAdds.Contains(DirtyUser);
		const bool bPendingRemove = PendingRemovals.Contains(DirtyUser);

		if (bPendingRemove || (!bContainsUser && !bPendingAdd))
		{
			TryAddUserFast(*DirtyUser);
		}
		else if (bPendingAdd || (bContainsUser && !bPendingRemove))
		{
			TryRemoveUser(*DirtyUser);
		}
	}
	UsersWithDirtyPresence.Reset();

	// Update the users in the list
	bool bListChanged = false;
	if (PendingRemovals.Num() > 0)
	{
		bListChanged = true;

		Users.RemoveAllSwap(
			[this] (USocialUser* User)
			{
				return PendingRemovals.Contains(User);
			});

		for (TWeakObjectPtr<const USocialUser> User : PendingRemovals)
		{
			if (User.IsValid())
			{
				OnUserRemoved().Broadcast(*User.Get());
			}
		}
		PendingRemovals.Reset();
	}
	
	if (PendingAdds.Num() > 0)
	{
		bListChanged = true;
		Users.Append(PendingAdds);
		
		for (USocialUser* User : PendingAdds)
		{
			OnUserAdded().Broadcast(*User);
		}
		PendingAdds.Reset();
	}

	if (bListChanged || bNeedsSort)
	{
		bNeedsSort = false;

		Users.Sort([](USocialUser& UserA, USocialUser& UserB)
			{
				const UPartyMember* PartyMemberA = UserA.GetPartyMember(IOnlinePartySystem::GetPrimaryPartyTypeId());
				const UPartyMember* PartyMemberB = UserB.GetPartyMember(IOnlinePartySystem::GetPrimaryPartyTypeId());

				// Put party members at the top
				if (PartyMemberA && !PartyMemberB)
				{
					return true;
				}
				else if (PartyMemberB && !PartyMemberA)
				{
					return false;
				}

				// Goes from if online, then alphabetical
				if (UserA.GetOnlineStatus() == UserB.GetOnlineStatus())
				{
					if (UserA.IsPlayingThisGame() == UserB.IsPlayingThisGame())
					{
						return UserA.GetDisplayName() < UserB.GetDisplayName();
					}
					else
					{
						return UserA.IsPlayingThisGame() > UserB.IsPlayingThisGame();
					}
				}
				else
				{
					// @todo StephanJ: note Online < Offline < Away, but it's okay for now since we show offline in a separate list #future
					return UserA.GetOnlineStatus() < UserB.GetOnlineStatus();
				}
				
			});

		OnUpdateComplete().Broadcast();
	}

	return true;
}
