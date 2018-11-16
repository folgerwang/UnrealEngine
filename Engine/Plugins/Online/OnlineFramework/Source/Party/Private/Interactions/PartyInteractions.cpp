// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Interactions/PartyInteractions.h"
#include "User/SocialUser.h"
#include "SocialToolkit.h"

#include "Party/SocialParty.h"
#include "Party/PartyMember.h" 
#include "Interfaces/OnlinePartyInterface.h"

#define LOCTEXT_NAMESPACE "PartyInteractions"

//////////////////////////////////////////////////////////////////////////
// InviteToParty
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_InviteToParty::GetDisplayName()
{
	return LOCTEXT("InviteToParty", "Invite to Party");
}

FString FSocialInteraction_InviteToParty::GetSlashCommandToken()
{
	return TEXT("invite");
}

void FSocialInteraction_InviteToParty::GetAvailability(const USocialUser& User, TArray<ESocialSubsystem>& OutAvailableSubsystems)
{
	if (User.CanInviteToParty(IOnlinePartySystem::GetPrimaryPartyTypeId()) && !User.IsBlocked())
	{
		OutAvailableSubsystems.Add(ESocialSubsystem::Primary);
	}
}

void FSocialInteraction_InviteToParty::ExecuteAction(ESocialSubsystem SocialSubsystem, USocialUser& User)
{
	User.InviteToParty(IOnlinePartySystem::GetPrimaryPartyTypeId());
}

//////////////////////////////////////////////////////////////////////////
// JoinParty
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_JoinParty::GetDisplayName()
{
	return LOCTEXT("JoinParty", "Join Party");
}

FString FSocialInteraction_JoinParty::GetSlashCommandToken()
{
	return TEXT("join");
}

void FSocialInteraction_JoinParty::GetAvailability(const USocialUser& User, TArray<ESocialSubsystem>& OutAvailableSubsystems)
{
	FJoinPartyResult Result;
	if (User.CanJoinParty(IOnlinePartySystem::GetPrimaryPartyTypeId(), Result) && !User.HasSentPartyInvite())
	{
		OutAvailableSubsystems.Add(ESocialSubsystem::Primary);
	}
}

void FSocialInteraction_JoinParty::ExecuteAction(ESocialSubsystem SocialSubsystem, USocialUser& User)
{
	User.JoinParty(IOnlinePartySystem::GetPrimaryPartyTypeId());
}

//////////////////////////////////////////////////////////////////////////
// AcceptPartyInvite
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_AcceptPartyInvite::GetDisplayName()
{
	return LOCTEXT("AcceptPartyInvite", "Accept");
}

FString FSocialInteraction_AcceptPartyInvite::GetSlashCommandToken()
{
	return TEXT("acceptpartyinvite");
}

void FSocialInteraction_AcceptPartyInvite::GetAvailability(const USocialUser& User, TArray<ESocialSubsystem>& OutAvailableSubsystems)
{
	if (User.HasSentPartyInvite())
	{
		OutAvailableSubsystems.Add(ESocialSubsystem::Primary);
	}
}

void FSocialInteraction_AcceptPartyInvite::ExecuteAction(ESocialSubsystem SocialSubsystem, USocialUser& User)
{
	User.JoinParty(IOnlinePartySystem::GetPrimaryPartyTypeId());
}

//////////////////////////////////////////////////////////////////////////
// RejectPartyInvite
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_RejectPartyInvite::GetDisplayName()
{
	return LOCTEXT("RejectPartyInvite", "Reject");
}

FString FSocialInteraction_RejectPartyInvite::GetSlashCommandToken()
{
	return TEXT("rejectpartyinvite");
}

void FSocialInteraction_RejectPartyInvite::GetAvailability(const USocialUser& User, TArray<ESocialSubsystem>& OutAvailableSubsystems)
{
	if (User.HasSentPartyInvite())
	{
		OutAvailableSubsystems.Add(ESocialSubsystem::Primary);
	}
}

void FSocialInteraction_RejectPartyInvite::ExecuteAction(ESocialSubsystem SocialSubsystem, USocialUser& User)
{
	User.RejectPartyInvite();
}

//////////////////////////////////////////////////////////////////////////
// LeaveParty
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_LeaveParty::GetDisplayName()
{
	return LOCTEXT("LeaveParty", "Leave Party");
}

FString FSocialInteraction_LeaveParty::GetSlashCommandToken()
{
	return TEXT("leave");
}

void FSocialInteraction_LeaveParty::GetAvailability(const USocialUser& User, TArray<ESocialSubsystem>& OutAvailableSubsystems)
{
	if (User.IsLocalUser())
	{
		const UPartyMember* LocalMember = User.GetPartyMember(IOnlinePartySystem::GetPrimaryPartyTypeId());
		if (LocalMember && LocalMember->GetParty().GetNumPartyMembers() > 1)
		{
			OutAvailableSubsystems.Add(ESocialSubsystem::Primary);
		}
	}
}

void FSocialInteraction_LeaveParty::ExecuteAction(ESocialSubsystem SocialSubsystem, USocialUser& User)
{
	if (const UPartyMember* LocalMember = User.GetPartyMember(IOnlinePartySystem::GetPrimaryPartyTypeId()))
	{
		LocalMember->GetParty().LeaveParty();
	}
}

//////////////////////////////////////////////////////////////////////////
// KickPartyMember
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_KickPartyMember::GetDisplayName()
{
	return LOCTEXT("KickPartyMember", "Kick");
}

FString FSocialInteraction_KickPartyMember::GetSlashCommandToken()
{
	return TEXT("kick");
}

void FSocialInteraction_KickPartyMember::GetAvailability(const USocialUser& User, TArray<ESocialSubsystem>& OutAvailableSubsystems)
{
	const UPartyMember* PartyMember = User.GetPartyMember(IOnlinePartySystem::GetPrimaryPartyTypeId());
	if (PartyMember && PartyMember->CanKickFromParty())
	{
		OutAvailableSubsystems.Add(ESocialSubsystem::Primary);
	}
}

void FSocialInteraction_KickPartyMember::ExecuteAction(ESocialSubsystem SocialSubsystem, USocialUser& User)
{
	if (UPartyMember* PartyMember = User.GetPartyMember(IOnlinePartySystem::GetPrimaryPartyTypeId()))
	{
		PartyMember->KickFromParty();
	}
}

//////////////////////////////////////////////////////////////////////////
// PromoteToPartyLeader
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_PromoteToPartyLeader::GetDisplayName()
{
	return LOCTEXT("PromoteToPartyLeader", "Promote");
}

FString FSocialInteraction_PromoteToPartyLeader::GetSlashCommandToken()
{
	return TEXT("promote");
}

void FSocialInteraction_PromoteToPartyLeader::GetAvailability(const USocialUser& User, TArray<ESocialSubsystem>& OutAvailableSubsystems)
{
	const UPartyMember* PartyMember = User.GetPartyMember(IOnlinePartySystem::GetPrimaryPartyTypeId());
	if (PartyMember && PartyMember->CanPromoteToLeader())
	{
		OutAvailableSubsystems.Add(ESocialSubsystem::Primary);
	}
}

void FSocialInteraction_PromoteToPartyLeader::ExecuteAction(ESocialSubsystem SocialSubsystem, USocialUser& User)
{
	if (UPartyMember* PartyMember = User.GetPartyMember(IOnlinePartySystem::GetPrimaryPartyTypeId()))
	{
		PartyMember->PromoteToPartyLeader();
	}
}

#undef LOCTEXT_NAMESPACE