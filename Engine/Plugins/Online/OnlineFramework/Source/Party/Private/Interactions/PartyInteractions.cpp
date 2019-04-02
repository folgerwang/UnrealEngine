// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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

FText FSocialInteraction_InviteToParty::GetDisplayName(const USocialUser& User)
{
	return LOCTEXT("InviteToParty", "Invite to Party");
}

FString FSocialInteraction_InviteToParty::GetSlashCommandToken()
{
	return (LOCTEXT("SlashCommand_InviteToParty", "invite")).ToString();
}

bool FSocialInteraction_InviteToParty::CanExecute(const USocialUser& User)
{
	return User.CanInviteToParty(IOnlinePartySystem::GetPrimaryPartyTypeId());
}

void FSocialInteraction_InviteToParty::ExecuteInteraction(USocialUser& User)
{
	User.InviteToParty(IOnlinePartySystem::GetPrimaryPartyTypeId());
}

//////////////////////////////////////////////////////////////////////////
// JoinParty
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_JoinParty::GetDisplayName(const USocialUser& User)
{
	return LOCTEXT("JoinParty", "Join Party");
}

FString FSocialInteraction_JoinParty::GetSlashCommandToken()
{
	return LOCTEXT("SlashCommand_JoinParty", "join").ToString();
}

bool FSocialInteraction_JoinParty::CanExecute(const USocialUser& User)
{
	FJoinPartyResult MockJoinResult = User.CheckPartyJoinability(IOnlinePartySystem::GetPrimaryPartyTypeId());
	return MockJoinResult.WasSuccessful();
}

void FSocialInteraction_JoinParty::ExecuteInteraction(USocialUser& User)
{
	User.JoinParty(IOnlinePartySystem::GetPrimaryPartyTypeId());
}

//////////////////////////////////////////////////////////////////////////
// AcceptPartyInvite
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_AcceptPartyInvite::GetDisplayName(const USocialUser& User)
{
	return LOCTEXT("AcceptPartyInvite", "Accept");
}

FString FSocialInteraction_AcceptPartyInvite::GetSlashCommandToken()
{
	//join should be the preferred method of accepting a party invite
	return FString();
}

bool FSocialInteraction_AcceptPartyInvite::CanExecute(const USocialUser& User)
{
	return User.HasSentPartyInvite(IOnlinePartySystem::GetPrimaryPartyTypeId());
}

void FSocialInteraction_AcceptPartyInvite::ExecuteInteraction(USocialUser& User)
{
	User.JoinParty(IOnlinePartySystem::GetPrimaryPartyTypeId());
}

//////////////////////////////////////////////////////////////////////////
// RejectPartyInvite
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_RejectPartyInvite::GetDisplayName(const USocialUser& User)
{
	return LOCTEXT("RejectPartyInvite", "Reject");
}

FString FSocialInteraction_RejectPartyInvite::GetSlashCommandToken()
{
	return FString();
}

bool FSocialInteraction_RejectPartyInvite::CanExecute(const USocialUser& User)
{
	return User.HasSentPartyInvite(IOnlinePartySystem::GetPrimaryPartyTypeId());
}

void FSocialInteraction_RejectPartyInvite::ExecuteInteraction(USocialUser& User)
{
	User.RejectPartyInvite(IOnlinePartySystem::GetPrimaryPartyTypeId());
}

//////////////////////////////////////////////////////////////////////////
// LeaveParty
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_LeaveParty::GetDisplayName(const USocialUser& User)
{
	return LOCTEXT("LeaveParty", "Leave Party");
}

FString FSocialInteraction_LeaveParty::GetSlashCommandToken()
{
	return LOCTEXT("SlashCommand_LeaveParty", "leave").ToString();
}

bool FSocialInteraction_LeaveParty::CanExecute(const USocialUser& User)
{
	if (User.IsLocalUser())
	{
		const UPartyMember* LocalMember = User.GetPartyMember(IOnlinePartySystem::GetPrimaryPartyTypeId());
		return LocalMember && LocalMember->GetParty().GetNumPartyMembers() > 1;
	}
	return false;
}

void FSocialInteraction_LeaveParty::ExecuteInteraction(USocialUser& User)
{
	if (const UPartyMember* LocalMember = User.GetPartyMember(IOnlinePartySystem::GetPrimaryPartyTypeId()))
	{
		LocalMember->GetParty().LeaveParty();
	}
}

//////////////////////////////////////////////////////////////////////////
// KickPartyMember
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_KickPartyMember::GetDisplayName(const USocialUser& User)
{
	return LOCTEXT("KickPartyMember", "Kick");
}

FString FSocialInteraction_KickPartyMember::GetSlashCommandToken()
{
	return LOCTEXT("SlashCommand_KickMember", "kick").ToString();
}

bool FSocialInteraction_KickPartyMember::CanExecute(const USocialUser& User)
{
	const UPartyMember* PartyMember = User.GetPartyMember(IOnlinePartySystem::GetPrimaryPartyTypeId());
	return PartyMember && PartyMember->CanKickFromParty();
}

void FSocialInteraction_KickPartyMember::ExecuteInteraction(USocialUser& User)
{
	if (UPartyMember* PartyMember = User.GetPartyMember(IOnlinePartySystem::GetPrimaryPartyTypeId()))
	{
		PartyMember->KickFromParty();
	}
}

//////////////////////////////////////////////////////////////////////////
// PromoteToPartyLeader
//////////////////////////////////////////////////////////////////////////

FText FSocialInteraction_PromoteToPartyLeader::GetDisplayName(const USocialUser& User)
{
	return LOCTEXT("PromoteToPartyLeader", "Promote");
}

FString FSocialInteraction_PromoteToPartyLeader::GetSlashCommandToken()
{
	return LOCTEXT("SlashCommand_PromoteToLeader", "promote").ToString();
}

bool FSocialInteraction_PromoteToPartyLeader::CanExecute(const USocialUser& User)
{
	const UPartyMember* PartyMember = User.GetPartyMember(IOnlinePartySystem::GetPrimaryPartyTypeId());
	return PartyMember && PartyMember->CanPromoteToLeader();
}

void FSocialInteraction_PromoteToPartyLeader::ExecuteInteraction(USocialUser& User)
{
	if (UPartyMember* PartyMember = User.GetPartyMember(IOnlinePartySystem::GetPrimaryPartyTypeId()))
	{
		PartyMember->PromoteToPartyLeader();
	}
}

#undef LOCTEXT_NAMESPACE