// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Party/PartyTypes.h"
#include "Party/PartyDataReplicator.h"

#include "PartyMember.generated.h"

class USocialUser;
class FOnlinePartyMember;
class FOnlinePartyData;
enum class EMemberExitedReason;

/** Base struct used to replicate data about the state of a single party member to all members. */
USTRUCT()
struct PARTY_API FPartyMemberRepData : public FOnlinePartyRepDataBase
{
	GENERATED_BODY()

public:
	FPartyMemberRepData() {}
	void SetOwningMember(const class UPartyMember& InOwnerMember);

protected:
	virtual bool CanEditData() const override;
	virtual void CompareAgainst(const FOnlinePartyRepDataBase& OldData) const override;
	virtual const USocialParty* GetOwnerParty() const override;

private:
	TWeakObjectPtr<const UPartyMember> OwnerMember;

	/** Native platform on which this party member is playing. */
	UPROPERTY()
	FUserPlatform Platform;
	EXPOSE_REP_DATA_PROPERTY(FPartyMemberRepData, FUserPlatform, Platform);

	/** Net ID for this party member on their native platform. Blank if this member has no Platform SocialSubsystem. */
	UPROPERTY()
	FUniqueNetIdRepl PlatformUniqueId;
	EXPOSE_REP_DATA_PROPERTY(FPartyMemberRepData, FUniqueNetIdRepl, PlatformUniqueId);

	/**
	 * The platform session this member is in. Can be blank for a bit while creating/joining.
	 * Only relevant when this member is on a platform that requires a session backing the party.
	 */
	UPROPERTY()
	FString PlatformSessionId;
	EXPOSE_REP_DATA_PROPERTY(FPartyMemberRepData, FSessionId, PlatformSessionId);

	/** The crossplay preference of this user. Only relevant to crossplay party scenarios. */
	UPROPERTY()
	ECrossplayPreference CrossplayPreference = ECrossplayPreference::NoSelection;
	EXPOSE_REP_DATA_PROPERTY(FPartyMemberRepData, ECrossplayPreference, CrossplayPreference);
};

using FPartyMemberDataReplicator = TPartyDataReplicator<FPartyMemberRepData>;

UCLASS(Abstract, config = Game, Within = SocialParty, Transient)
class PARTY_API UPartyMember : public UObject
{
	GENERATED_BODY()

public:
	UPartyMember();

	virtual void BeginDestroy() override;

	bool CanPromoteToLeader() const;
	bool PromoteToPartyLeader();

	bool CanKickFromParty() const;
	bool KickFromParty();

	bool IsInitialized() const;
	bool IsPartyLeader() const;
	bool IsLocalPlayer() const;
	
	USocialParty& GetParty() const;
	FUniqueNetIdRepl GetPrimaryNetId() const;
	const FPartyMemberRepData& GetRepData() const { return *MemberDataReplicator; }
	USocialUser& GetSocialUser() const;

	FString GetDisplayName() const;
	FName GetPlatformOssName() const;

	DECLARE_EVENT(UPartyMember, FOnPartyMemberStateChanged);
	FOnPartyMemberStateChanged& OnInitializationComplete() const { return OnMemberInitializedEvent; }
	FOnPartyMemberStateChanged& OnPromotedToLeader() const { return OnPromotedToLeaderEvent; }
	FOnPartyMemberStateChanged& OnDemoted() const { return OnDemotedEvent; }

	DECLARE_EVENT_OneParam(UPartyMember, FOnPartyMemberLeft, EMemberExitedReason)
	FOnPartyMemberLeft& OnLeftParty() const { return OnLeftPartyEvent; }

	FString ToDebugString(bool bIncludePartyId = true) const;

PARTY_SCOPE:
	void InitializePartyMember(const TSharedRef<FOnlinePartyMember>& OssMember, const FSimpleDelegate& OnInitComplete);

	FPartyMemberRepData& GetMutableRepData() { return *MemberDataReplicator; }
	void NotifyMemberDataReceived(const TSharedRef<FOnlinePartyData>& MemberData);
	void NotifyMemberPromoted();
	void NotifyMemberDemoted();
	void NotifyRemovedFromParty(EMemberExitedReason ExitReason);

protected:
	virtual void FinishInitializing();
	virtual void InitializeLocalMemberRepData();

	virtual void OnMemberPromotedInternal();
	virtual void OnMemberDemotedInternal();
	virtual void OnRemovedFromPartyInternal(EMemberExitedReason ExitReason);
	virtual void Shutdown();

	FPartyMemberDataReplicator MemberDataReplicator;

private:
	void HandleSocialUserInitialized(USocialUser& InitializedUser);

	TSharedPtr<FOnlinePartyMember> OssPartyMember;

	UPROPERTY()
	USocialUser* SocialUser = nullptr;

	bool bHasReceivedInitialData = false;
	mutable FOnPartyMemberStateChanged OnMemberInitializedEvent;
	mutable FOnPartyMemberStateChanged OnPromotedToLeaderEvent;
	mutable FOnPartyMemberStateChanged OnDemotedEvent;
	mutable FOnPartyMemberLeft OnLeftPartyEvent;
};