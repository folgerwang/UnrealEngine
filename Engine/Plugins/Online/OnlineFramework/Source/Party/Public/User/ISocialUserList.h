// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SocialTypes.h"
#include "Misc/EnumClassFlags.h"
#include "SocialUser.h"

DECLARE_DELEGATE_RetVal_OneParam(bool, FOnCustomFilterUser, const USocialUser&);

/** OSS-agnostic user state filters (presence info generally required). Some of these do imply others and some conflict. Up to consumers to responsibly choose appropriate combinations. */
enum class ESocialUserStateFlags
{
	None = 0,
	Online = 1, 
	Joinable = 1 << 1,
	LookingForGroup = 1 << 2,
	SamePlatform = 1 << 3,
	InGame = 1 << 4,
	SameApp = 1 << 5,
	SameParty = 1 << 6,
};

ENUM_CLASS_FLAGS(ESocialUserStateFlags);

/** Configuration of ISocialUserList properties that are immutable once the list is created. */
class FSocialUserListConfig
{
public:
	FSocialUserListConfig() {}

	ESocialRelationship RelationshipType = ESocialRelationship::Friend;
	TArray<ESocialSubsystem> RelevantSubsystems;
	TArray<ESocialSubsystem> ForbiddenSubsystems;
	ESocialUserStateFlags RequiredPresenceFlags = ESocialUserStateFlags::None;
	ESocialUserStateFlags ForbiddenPresenceFlags = ESocialUserStateFlags::None;
	FOnCustomFilterUser OnCustomFilterUser;
};

class ISocialUserList
{
public:
	virtual ~ISocialUserList() {}
	
	DECLARE_EVENT_OneParam(ISocialUserList, FOnUserAdded, USocialUser&)
	virtual FOnUserAdded& OnUserAdded() const = 0;

	DECLARE_EVENT_OneParam(ISocialUserList, FOnUserRemoved, const USocialUser&)
	virtual FOnUserRemoved& OnUserRemoved() const = 0;

	/** Fires one time whenever an update results in some kind of change */
	DECLARE_EVENT(ISocialUserList, FOnUpdateComplete)
	virtual FOnUpdateComplete& OnUpdateComplete() const = 0;

	virtual const TArray<USocialUser*>& GetUsers() const = 0;

	/** Trigger an update of the list immediately, regardless of auto update period */
	virtual void UpdateNow() = 0;

	/** Sets the period at which to update the list with all users that  */
	virtual void SetAutoUpdatePeriod(float InAutoUpdatePeriod) = 0;
};