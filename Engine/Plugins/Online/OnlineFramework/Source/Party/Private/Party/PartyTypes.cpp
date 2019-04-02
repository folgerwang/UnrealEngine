// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Party/PartyTypes.h"
#include "Party/SocialParty.h"
#include "Party/PartyMember.h"
#include "OnlineSubsystemUtils.h"

//////////////////////////////////////////////////////////////////////////
// FPartyPlatformSessionInfo
//////////////////////////////////////////////////////////////////////////

bool FPartyPlatformSessionInfo::operator==(const FPartyPlatformSessionInfo& Other) const
{
	return OssName == Other.OssName
		&& SessionId == Other.SessionId
		&& OwnerPrimaryId == Other.OwnerPrimaryId;
}

bool FPartyPlatformSessionInfo::operator==(FName PlatformOssName) const
{
	return OssName == PlatformOssName;
}

void FPartyPlatformSessionInfo::operator=(const FPartyPlatformSessionInfo& Other)
{
	OssName = Other.OssName;
	SessionId = Other.SessionId;
	OwnerPrimaryId = Other.OwnerPrimaryId;
}

FString FPartyPlatformSessionInfo::ToDebugString() const
{
	return FString::Printf(TEXT("OssName=[%s], SessionId=[%s], OwnerPrimaryId=[%s]"), *OssName.ToString(), *SessionId, *OwnerPrimaryId.ToDebugString());
}

bool FPartyPlatformSessionInfo::IsSessionOwner(const UPartyMember& PartyMember) const
{
	return PartyMember.GetPrimaryNetId() == OwnerPrimaryId;
}

bool FPartyPlatformSessionInfo::IsInSession(const UPartyMember& PartyMember) const
{
	return PartyMember.GetRepData().GetPlatformSessionId() == SessionId;
}

//////////////////////////////////////////////////////////////////////////
// FPartyPrivacySettings
//////////////////////////////////////////////////////////////////////////

bool FPartyPrivacySettings::operator==(const FPartyPrivacySettings& Other) const
{
	return PartyType == Other.PartyType
		&& PartyInviteRestriction == Other.PartyInviteRestriction
		&& bOnlyLeaderFriendsCanJoin == Other.bOnlyLeaderFriendsCanJoin;
}

//////////////////////////////////////////////////////////////////////////
// FJoinPartyResult
//////////////////////////////////////////////////////////////////////////

FJoinPartyResult::FJoinPartyResult()
	: Result(EJoinPartyCompletionResult::Succeeded)
{
}

FJoinPartyResult::FJoinPartyResult(FPartyJoinDenialReason InDenialReason)
{
	SetDenialReason(InDenialReason);
}

FJoinPartyResult::FJoinPartyResult(EJoinPartyCompletionResult InResult)
	: Result(InResult)
{
}

FJoinPartyResult::FJoinPartyResult(EJoinPartyCompletionResult InResult, FPartyJoinDenialReason InDenialReason)
{
	SetResult(InResult);
	if (InResult == EJoinPartyCompletionResult::NotApproved)
	{
		SetDenialReason(InDenialReason);
	}
}

void FJoinPartyResult::SetDenialReason(FPartyJoinDenialReason InDenialReason)
{
	DenialReason = InDenialReason;
	if (InDenialReason.HasAnyReason())
	{
		Result = EJoinPartyCompletionResult::NotApproved;
	}
}

void FJoinPartyResult::SetResult(EJoinPartyCompletionResult InResult)
{
	Result = InResult;
	if (InResult != EJoinPartyCompletionResult::NotApproved)
	{
		DenialReason = EPartyJoinDenialReason::NoReason;
	}
}

bool FJoinPartyResult::WasSuccessful() const
{
	return Result == EJoinPartyCompletionResult::Succeeded;
}

//////////////////////////////////////////////////////////////////////////
// FOnlinePartyRepDataBase
//////////////////////////////////////////////////////////////////////////

void FOnlinePartyRepDataBase::LogPropertyChanged(const TCHAR* OwningStructTypeName, const TCHAR* ProperyName, bool bFromReplication) const
{
	const USocialParty* OwningParty = GetOwnerParty();
	
	// Only thing this lacks is the id of the party member for member rep data changes
	UE_LOG(LogParty, VeryVerbose, TEXT("RepData property [%s::%s] changed %s in party [%s]"),
		OwningStructTypeName,
		ProperyName,
		bFromReplication ? TEXT("remotely") : TEXT("locally"),
		ensure(OwningParty) ? *OwningParty->ToDebugString() : TEXT("unknown"));
}
