// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "User/ISocialUserList.h"
#include "UObject/GCObject.h"

class FSocialUserList : public ISocialUserList, public FGCObject, public TSharedFromThis<FSocialUserList>
{
public:
	static TSharedRef<FSocialUserList> CreateUserList(USocialToolkit& InOwnerToolkit, const FSocialUserListConfig& Config);

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	FOnUserAdded& OnUserAdded() const override { return OnUserAddedEvent; }
	FOnUserRemoved& OnUserRemoved() const override { return OnUserRemovedEvent; }
	FOnUpdateComplete& OnUpdateComplete() const override { return OnUpdateCompleteEvent; }

	void UpdateNow();
	void SetAutoUpdatePeriod(float InAutoUpdatePeriod);
	const TArray<USocialUser*>& GetUsers() const { return Users; }

	bool HasPresenceFilters() const;

private:
	void HandleOwnerToolkitReset();

	void HandlePartyInviteReceived(USocialUser& InvitingUser);
	void HandlePartyInviteHandled(USocialUser* InvitingUser);

	void HandleFriendInviteReceived(USocialUser& User, ESocialSubsystem SubsystemType);
	void HandleFriendInviteRemoved(ESocialSubsystem SubsystemType, USocialUser* User);

	void HandleFriendshipEstablished(USocialUser& NewFriend, ESocialSubsystem SubsystemType, bool bIsNewRelationship);
	void HandleFriendRemoved(ESocialSubsystem SubsystemType, USocialUser* User);
	
	void HandleUserBlocked(USocialUser& BlockedUser, ESocialSubsystem SubsystemType, bool bIsNewRelationship);
	void HandleUserBlockStatusChanged(ESocialSubsystem SubsystemType, bool bIsBlocked, USocialUser* User);

	void HandleRecentPlayerAdded(USocialUser& AddedUser, ESocialSubsystem SubsystemType, bool bIsNewRelationship);
	void HandleRecentPlayerRemoved(USocialUser& RemovedUser, ESocialSubsystem SubsystemType);
	
	void HandleUserPresenceChanged(ESocialSubsystem SubsystemType, USocialUser* User);

	void TryAddUser(USocialUser& User);
	void TryAddUserFast(USocialUser& User);

	void TryRemoveUser(USocialUser& User);
	void TryRemoveUserFast(USocialUser& User);
	
	bool EvaluateUserPresence(const USocialUser& User, ESocialSubsystem SubsystemType);
	bool EvaluatePresenceFlag(bool bPresenceValue, ESocialUserStateFlags Flag) const;

	bool HandleAutoUpdateList(float);

private:
	FSocialUserList(USocialToolkit& InOwnerToolkit, const FSocialUserListConfig& Config);
	void InitializeList();

	TWeakObjectPtr<USocialToolkit> OwnerToolkit;

	UPROPERTY()
	TArray<USocialUser*> Users;

	UPROPERTY()
	TArray<USocialUser*> PendingAdds;

	TSet<TWeakObjectPtr<USocialUser>> UsersWithDirtyPresence;
	TArray<TWeakObjectPtr<const USocialUser>> PendingRemovals;

	FSocialUserListConfig ListConfig;

	bool bNeedsSort = false;
	float AutoUpdatePeriod = 5.f;
	FDelegateHandle UpdateTickerHandle;

	mutable FOnUserAdded OnUserAddedEvent;
	mutable FOnUserRemoved OnUserRemovedEvent;
	mutable FOnUpdateComplete OnUpdateCompleteEvent;
};