// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Interactions/CoreInteractions.h"

#include "SocialToolkit.h"
#include "SocialManager.h"
#include "User/SocialUser.h"
#include "Chat/SocialChatManager.h"

#include "OnlineSubsystem.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "Interfaces/OnlinePresenceInterface.h"

#define LOCTEXT_NAMESPACE "UserInteractions"

//////////////////////////////////////////////////////////////////////////
// AddFriend
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_AddFriend::GetDisplayName()
{
	return LOCTEXT("AddFriend", "Add Friend");
}

FString FSocialInteraction_AddFriend::GetSlashCommandToken()
{
	return TEXT("friend");
}

void FSocialInteraction_AddFriend::GetAvailability(const USocialUser& User, TArray<ESocialSubsystem>& OutAvailableSubsystems)
{
	if (!User.IsFriend(ESocialSubsystem::Primary))
	{
		if (User.GetFriendInviteStatus(ESocialSubsystem::Primary) != EInviteStatus::PendingInbound && !User.IsBlocked(ESocialSubsystem::Primary))
		{
			OutAvailableSubsystems.Add(ESocialSubsystem::Primary);
		}
	}
	//@todo DanH: Need to sort out display name differentiation between the same interaction on two subsystems. ViewProfile covers this nicely for now #future
	/*if (!User.IsFriend(ESocialSubsystem::Platform) && User.HasSubsystemInfo(ESocialSubsystem::Platform))
	{
		const FName PlatformSubsystemName = USocialManager::GetSocialOssName(ESocialSubsystem::Platform);
		if (PlatformSubsystemName == LIVE_SUBSYSTEM || PlatformSubsystemName == PS4_SUBSYSTEM)
		{
			OutAvailableSubsystems.Add(ESocialSubsystem::Platform);
		}
	}*/
}

void FSocialInteraction_AddFriend::ExecuteAction(ESocialSubsystem SocialSubsystem, USocialUser& User)
{
	User.SendFriendInvite(SocialSubsystem);
}

//////////////////////////////////////////////////////////////////////////
// RemoveFriend
//////////////////////////////////////////////////////////////////////////


FText FSocialInteraction_RemoveFriend::GetDisplayName()
{
	return LOCTEXT("RemoveFriend", "Remove Friend");
}

FString FSocialInteraction_RemoveFriend::GetSlashCommandToken()
{
	return TEXT("unfriend");
}

void FSocialInteraction_RemoveFriend::GetAvailability(const USocialUser& User, TArray<ESocialSubsystem>& OutAvailableSubsystems)
{
	if (User.IsFriend(ESocialSubsystem::Primary))
	{
		OutAvailableSubsystems.Add(ESocialSubsystem::Primary);
	}
}

void FSocialInteraction_RemoveFriend::ExecuteAction(ESocialSubsystem SocialSubsystem, USocialUser& User)
{
	//@todo DanH SocialInteractions: Same issue as parties - there can be N different named friends lists. Whatever we do for different party types, do here too. #future
	User.EndFriendship(SocialSubsystem);
}

//////////////////////////////////////////////////////////////////////////
// AcceptFriendInvite
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_AcceptFriendInvite::GetDisplayName()
{
	return LOCTEXT("AcceptFriendInvite", "Accept");
}

FString FSocialInteraction_AcceptFriendInvite::GetSlashCommandToken()
{
	return TEXT("accept");
}

void FSocialInteraction_AcceptFriendInvite::GetAvailability(const USocialUser& User, TArray<ESocialSubsystem>& OutAvailableSubsystems)
{
	if (User.GetFriendInviteStatus(ESocialSubsystem::Primary) == EInviteStatus::PendingInbound)
	{
		OutAvailableSubsystems.Add(ESocialSubsystem::Primary);
	}
	if (User.GetFriendInviteStatus(ESocialSubsystem::Platform) == EInviteStatus::PendingInbound)
	{
		OutAvailableSubsystems.Add(ESocialSubsystem::Platform);
	}
}

void FSocialInteraction_AcceptFriendInvite::ExecuteAction(ESocialSubsystem SocialSubsystem, USocialUser& User)
{
	User.AcceptFriendInvite(SocialSubsystem);
}

//////////////////////////////////////////////////////////////////////////
// RejectFriendInvite
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_RejectFriendInvite::GetDisplayName()
{
	return LOCTEXT("RejectFriendInvite", "Reject");
}

FString FSocialInteraction_RejectFriendInvite::GetSlashCommandToken()
{
	return TEXT("reject");
}

void FSocialInteraction_RejectFriendInvite::GetAvailability(const USocialUser& User, TArray<ESocialSubsystem>& OutAvailableSubsystems)
{
	if (User.GetFriendInviteStatus(ESocialSubsystem::Primary) == EInviteStatus::PendingInbound)
	{
		OutAvailableSubsystems.Add(ESocialSubsystem::Primary);
	}
	if (User.GetFriendInviteStatus(ESocialSubsystem::Platform) == EInviteStatus::PendingInbound)
	{
		OutAvailableSubsystems.Add(ESocialSubsystem::Platform);
	}
}

void FSocialInteraction_RejectFriendInvite::ExecuteAction(ESocialSubsystem SocialSubsystem, USocialUser& User)
{
	User.RejectFriendInvite(SocialSubsystem);
}

//////////////////////////////////////////////////////////////////////////
// Block
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_Block::GetDisplayName()
{
	return LOCTEXT("BlockUser", "Block");
}

FString FSocialInteraction_Block::GetSlashCommandToken()
{
	return TEXT("block");
}

void FSocialInteraction_Block::GetAvailability(const USocialUser& User, TArray<ESocialSubsystem>& OutAvailableSubsystems)
{
	if (User.HasSubsystemInfo(ESocialSubsystem::Primary))
	{
		// If there is a primary subsystem, only bother with blocking there
		if (!User.IsBlocked(ESocialSubsystem::Primary))
		{
			OutAvailableSubsystems.Add(ESocialSubsystem::Primary);
		}
	}
	else if (User.HasSubsystemInfo(ESocialSubsystem::Platform) && !User.IsBlocked(ESocialSubsystem::Platform))
	{
		// If the platform subsystem is the only one available, allow that instead (so long as it's xbox or ps4)
		const FName PlatformSubsystemName = USocialManager::GetSocialOssName(ESocialSubsystem::Platform);
		if (PlatformSubsystemName == LIVE_SUBSYSTEM || PlatformSubsystemName == PS4_SUBSYSTEM)
		{
			OutAvailableSubsystems.Add(ESocialSubsystem::Platform);
		}
	}
}

void FSocialInteraction_Block::ExecuteAction(ESocialSubsystem SocialSubsystem, USocialUser& User)
{
	User.BlockUser(SocialSubsystem);
}

//////////////////////////////////////////////////////////////////////////
// Unblock
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_Unblock::GetDisplayName()
{
	return LOCTEXT("UnblockUser", "Unblock");
}

FString FSocialInteraction_Unblock::GetSlashCommandToken()
{
	return TEXT("unblock");
}

void FSocialInteraction_Unblock::GetAvailability(const USocialUser& User, TArray<ESocialSubsystem>& OutAvailableSubsystems)
{
	if (User.HasSubsystemInfo(ESocialSubsystem::Primary))
	{
		// If there is a primary subsystem, only bother with blocking there
		if (User.IsBlocked(ESocialSubsystem::Primary))
		{
			OutAvailableSubsystems.Add(ESocialSubsystem::Primary);
		}
	}
	else if (User.HasSubsystemInfo(ESocialSubsystem::Platform) && User.IsBlocked(ESocialSubsystem::Platform))
	{
		// If the platform subsystem is the only one available, allow that instead (so long as it's xbox or ps4)
		const FName PlatformSubsystemName = USocialManager::GetSocialOssName(ESocialSubsystem::Platform);
		if (PlatformSubsystemName == LIVE_SUBSYSTEM || PlatformSubsystemName == PS4_SUBSYSTEM)
		{
			OutAvailableSubsystems.Add(ESocialSubsystem::Platform);
		}
	}
}

void FSocialInteraction_Unblock::ExecuteAction(ESocialSubsystem SocialSubsystem, USocialUser& User)
{
	User.UnblockUser(SocialSubsystem);
}

//////////////////////////////////////////////////////////////////////////
// PrivateMessage
//////////////////////////////////////////////////////////////////////////


FText FSocialInteraction_PrivateMessage::GetDisplayName()
{
	return LOCTEXT("SendPrivateMessage", "Whisper");
}

FString FSocialInteraction_PrivateMessage::GetSlashCommandToken()
{
	return TEXT("whisper");
}

void FSocialInteraction_PrivateMessage::GetAvailability(const USocialUser& User, TArray<ESocialSubsystem>& OutAvailableSubsystems)
{
	if (User.GetOwningToolkit().GetChatManager().IsChatRestricted())
	{
		return;
	}

	// Whispering only takes place on the primary subsystem, but is enabled for friends on both primary and platform subsystems
	if (User.GetOnlineStatus() != EOnlinePresenceState::Offline)
	{
		if (User.IsFriend(ESocialSubsystem::Primary) || User.IsFriend(ESocialSubsystem::Platform))
		{
			OutAvailableSubsystems.Add(ESocialSubsystem::Primary);
		}
	}
}

void FSocialInteraction_PrivateMessage::ExecuteAction(ESocialSubsystem SocialSubsystem, USocialUser& User)
{
	USocialChatManager& ChatManager = User.GetOwningToolkit().GetChatManager();
	ChatManager.CreateChatChannel(User);
	ChatManager.FocusChatChannel(User);
}

//////////////////////////////////////////////////////////////////////////
// ShowPlatformProfile
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_ShowPlatformProfile::GetDisplayName()
{
	return LOCTEXT("ShowPlatformProfile", "View Profile");
}

FString FSocialInteraction_ShowPlatformProfile::GetSlashCommandToken()
{
	return TEXT("profile");
}

void FSocialInteraction_ShowPlatformProfile::GetAvailability(const USocialUser& User, TArray<ESocialSubsystem>& OutAvailableSubsystems)
{
	if (USocialManager::GetLocalUserPlatform().IsConsole() && User.GetUserId(ESocialSubsystem::Platform).IsValid())
	{
		OutAvailableSubsystems.Add(ESocialSubsystem::Platform);
	}
}

void FSocialInteraction_ShowPlatformProfile::ExecuteAction(ESocialSubsystem SocialSubsystem, USocialUser& User)
{
	User.ShowPlatformProfile();
}

#undef LOCTEXT_NAMESPACE