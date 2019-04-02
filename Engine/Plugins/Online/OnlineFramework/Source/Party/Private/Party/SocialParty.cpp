// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Party/SocialParty.h"
#include "Party/PartyMember.h"
#include "Party/PartyPlatformSessionMonitor.h"

#include "SocialSettings.h"
#include "SocialManager.h"
#include "SocialToolkit.h"
#include "User/SocialUser.h"

#include "PartyBeaconClient.h"
#include "OnlineSubsystemUtils.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Interfaces/OnlinePartyInterface.h"

//////////////////////////////////////////////////////////////////////////
// FPartyRepData
//////////////////////////////////////////////////////////////////////////

void FPartyRepData::SetOwningParty(const USocialParty& InOwnerParty)
{
	OwnerParty = &InOwnerParty;
}

const FPartyPlatformSessionInfo* FPartyRepData::FindSessionInfo(FName PlatformOssName) const
{
	return PlatformSessions.FindByKey(PlatformOssName);
}

void FPartyRepData::UpdatePlatformSessionInfo(const FPartyPlatformSessionInfo& SessionInfo)
{
	bool bDidModifyRepData = false;
	if (FPartyPlatformSessionInfo* ExistingInfo = PlatformSessions.FindByKey(SessionInfo.OssName))
	{
		if (*ExistingInfo != SessionInfo)
		{
			*ExistingInfo = SessionInfo;
			bDidModifyRepData = true;
		}
	}
	else
	{
		bDidModifyRepData = true;
		PlatformSessions.Add(SessionInfo);
	}

	if (bDidModifyRepData)
	{
		OnDataChanged.ExecuteIfBound();
		OnPlatformSessionsChanged().Broadcast();
	}
}

void FPartyRepData::ClearPlatformSessionInfo(const FName PlatformOssName)
{
	const int32 NumRemoved = PlatformSessions.RemoveAll([&PlatformOssName] (const FPartyPlatformSessionInfo& Info) { return Info.OssName == PlatformOssName; });
	if (NumRemoved > 0)
	{
		OnDataChanged.ExecuteIfBound();
		OnPlatformSessionsChanged().Broadcast();
	}
}

bool FPartyRepData::CanEditData() const
{
	return OwnerParty.IsValid() && OwnerParty->IsLocalPlayerPartyLeader();
}

void FPartyRepData::CompareAgainst(const FOnlinePartyRepDataBase& OldData) const
{
	const FPartyRepData& TypedOldData = static_cast<const FPartyRepData&>(OldData);

	//ComparePartyType(TypedOldData);
	//CompareLeaderFriendsOnly(TypedOldData);
	//CompareLeaderInvitesOnly(TypedOldData);
	//CompareInvitesDisabled(TypedOldData);
	ComparePrivacySettings(TypedOldData);

	if (PlatformSessions != TypedOldData.PlatformSessions)
	{
		OnPlatformSessionsChanged().Broadcast();
	}
}

const USocialParty* FPartyRepData::GetOwnerParty() const
{
	return OwnerParty.Get();
}


//////////////////////////////////////////////////////////////////////////
// USocialParty
//////////////////////////////////////////////////////////////////////////

static int32 EnableAutomaticPartyRejoin = 1;
static FAutoConsoleVariableRef CVarEnableAutomaticPartyRejoin(
	TEXT("Party.EnableAutomaticPartyRejoin"),
	EnableAutomaticPartyRejoin,
	TEXT("Enable automatic rejoining of parties\n")
	TEXT("1 Enables. 0 disables."),
	ECVF_Default);

static int32 AllowPartyJoinsDuringLoad = 1;
static FAutoConsoleVariableRef CVar_AllowPartyJoinsDuringLoad(
	TEXT("Party.AllowJoinsDuringLoad"),
	AllowPartyJoinsDuringLoad,
	TEXT("Enables joins while leader is trying to load into a game\n")
	TEXT("1 Enables. 0 disables."),
	ECVF_Default);

static int32 AutoApproveJoinRequests = 0;
static FAutoConsoleVariableRef CVar_AutoApproveJoinRequests(
	TEXT("Party.AutoApproveJoinRequests"),
	AutoApproveJoinRequests,
	TEXT("Cheat to force all join requests to be immediately approved\n")
	TEXT("1 Enables. 0 disables."),
	ECVF_Cheat);

bool USocialParty::IsJoiningDuringLoadEnabled()
{
	return AllowPartyJoinsDuringLoad != 0;
}

USocialParty::USocialParty()
	: ReservationBeaconClientClass(APartyBeaconClient::StaticClass())
{}

ECrossplayPreference GetCrossplayPreferenceFromJoinData(const FOnlinePartyData& JoinData)
{
	FVariantData CrossplayPreferenceVariant;
	if (JoinData.GetAttribute(TEXT("CrossplayPreference"), CrossplayPreferenceVariant))
	{
		int32 CrossplayPreferenceInt;
		CrossplayPreferenceVariant.GetValue(CrossplayPreferenceInt);
		return (ECrossplayPreference)CrossplayPreferenceInt;
	}
	return ECrossplayPreference::NoSelection;
}

FPartyJoinApproval USocialParty::EvaluateJIPRequest(const FUniqueNetId& PlayerId) const
{
	FPartyJoinApproval JoinApproval;

	JoinApproval.SetApprovalAction(EApprovalAction::Deny);
	JoinApproval.SetDenialReason(EPartyJoinDenialReason::GameModeRestricted);
	for (const UPartyMember* Member : GetPartyMembers())
	{
		// Make sure we are already in the party.
		if (Member->GetPrimaryNetId() == PlayerId)
		{
			JoinApproval.SetApprovalAction(EApprovalAction::EnqueueAndStartBeacon);
			JoinApproval.SetDenialReason(EPartyJoinDenialReason::NoReason);
			break;
		}
	}
	return JoinApproval;
}

FPartyJoinApproval USocialParty::EvaluateJoinRequest(const FUniqueNetId& PlayerId, const FUserPlatform& Platform, const FOnlinePartyData& JoinData, bool bFromJoinRequest) const
{
	FPartyJoinApproval JoinApproval;

	if (IsPartyFull())
	{
		JoinApproval.SetDenialReason(EPartyJoinDenialReason::PartyFull);
	}
	else if (GetOwningLocalMember().GetSocialUser().GetOnlineStatus() == EOnlinePresenceState::Away)
	{
		JoinApproval.SetDenialReason(EPartyJoinDenialReason::TargetUserAway);
	}
	else
	{
		const ECrossplayPreference SenderCrossplayPreference = GetCrossplayPreferenceFromJoinData(JoinData);
		const bool bSenderAllowsCrossplay = !OptedOutOfCrossplay(SenderCrossplayPreference);

		TArray<FString> MemberPlatforms;
		for (const UPartyMember* Member : GetPartyMembers())
		{
			const FUserPlatform& MemberPlatform = Member->GetRepData().GetPlatform();
			if (Platform.IsCrossplayWith(MemberPlatform))
			{
				const ECrossplayPreference MemberCrossplayPreference = Member->GetRepData().GetCrossplayPreference();
				const bool bMemberAllowsCrossplay = !OptedOutOfCrossplay(MemberCrossplayPreference);

				if (!bSenderAllowsCrossplay || !bMemberAllowsCrossplay)
				{
					if (SenderCrossplayPreference == ECrossplayPreference::OptedOutRestricted)
					{
						JoinApproval.SetApprovalAction(EApprovalAction::Deny);
						JoinApproval.SetDenialReason(EPartyJoinDenialReason::JoinerCrossplayRestricted);
						//UFortAnalytics::FireEvent_AutoRejectedFromCrossPlatformParty(FPC, SenderPlatform, true);
					}
					else if (MemberCrossplayPreference == ECrossplayPreference::OptedOutRestricted)
					{
						JoinApproval.SetApprovalAction(EApprovalAction::Deny);
						JoinApproval.SetDenialReason(EPartyJoinDenialReason::MemberCrossplayRestricted);
						//UFortAnalytics::FireEvent_AutoRejectedFromCrossPlatformParty(FPC, SenderPlatform, false);
					}
				}
			}
		}

		//@todo DanH Party: Ask stephan if we still want this and the above events, move to the right spot if so #required
		//UFortAnalytics::FireEvent_JoinedCrossPlatformPartyRequestApproved(FPC, SenderPlatform, MemberPlatforms);
	}

	return JoinApproval;
}

bool USocialParty::ShouldCacheForRejoinOnDisconnect() const
{
	return EnableAutomaticPartyRejoin != 0 && GetNumPartyMembers() > 1;
}

bool USocialParty::IsCurrentlyLeaving() const
{
	return bIsLeavingParty;
}

bool USocialParty::IsInitialized() const
{
	return bIsInitialized;
}

bool USocialParty::HasUserBeenInvited(const USocialUser& User) const
{
	const IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());

	const FUniqueNetIdRepl UserId = User.GetUserId(ESocialSubsystem::Primary);
	if (ensure(UserId.IsValid()))
	{
		// No advertised party info, check to see if this user has sent an invite
		TArray<TSharedRef<const FUniqueNetId>> InvitedUserIds;
		if (PartyInterface->GetPendingInvitedUsers(*OwningLocalUserId, GetPartyId(), InvitedUserIds))
		{
			for (const TSharedRef<const FUniqueNetId>& InvitedUserId : InvitedUserIds)
			{
				if (*InvitedUserId == *UserId)
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool USocialParty::CanInviteUser(const USocialUser& User) const
{
	// Only users that are online can be invited
	if (!User.IsOnline())
	{
		return false;
	}

	if (!CurrentConfig.bIsAcceptingMembers && CurrentConfig.NotAcceptingMembersReason != (int32)EPartyJoinDenialReason::PartyPrivate)
	{
		// We aren't accepting members for a reason other than party privacy, so a direct invite won't help
		return false;
	}

	//@todo DanH Party: The problem with CanLocalUserInvite is that it the "friend" restriction is applied to mcp friends only, so a console friend doesn't count (but should) #required
	//		Need to check in with OGS about that...
	if (!OssParty->CanLocalUserInvite(*OwningLocalUserId))
	{
		return false;
	}

	if (GetPartyMember(User.GetUserId(ESocialSubsystem::Primary)))
	{
		// Already in the party
		return false;
	}

	return true;
}

bool USocialParty::TryInviteUser(const USocialUser& UserToInvite)
{
	bool bSentInvite = false;
	ESocialSubsystem InvitationSubsystemType = ESocialSubsystem::MAX;

	if (CanInviteUser(UserToInvite))
	{
		static const bool bPreferPlatformInvite = USocialSettings::ShouldPreferPlatformInvites();

		const FUniqueNetIdRepl UserPrimaryId = UserToInvite.GetUserId(ESocialSubsystem::Primary);
		const FUniqueNetIdRepl UserPlatformId = UserToInvite.GetUserId(ESocialSubsystem::Platform);
		bool bIsOnlineOnPlatform = false;
		if (const FOnlineUserPresence* PlatformPresenceInfo = UserToInvite.GetFriendPresenceInfo(ESocialSubsystem::Platform))
		{
			bIsOnlineOnPlatform = PlatformPresenceInfo->bIsOnline;
		}

		if ((UserPlatformId.IsValid() && bIsOnlineOnPlatform) && (!UserPrimaryId.IsValid() || bPreferPlatformInvite))
		{
			InvitationSubsystemType = ESocialSubsystem::Platform;

			// Platform invites are sent as session invites on platform OSS' - this way we get the OS popups one would expect on XBox, PS4, etc.
			const IOnlineSessionPtr PlatformSessionInterface = Online::GetSessionInterface(GetWorld(), USocialManager::GetSocialOssName(ESocialSubsystem::Platform));
			if (PlatformSessionInterface.IsValid())
			{
				//@todo DanH Party: Any way to know if the session invite was a success? If we don't know we can't show it :/ #future
				bSentInvite = PlatformSessionInterface->SendSessionInviteToFriend(*GetOwningLocalMember().GetRepData().GetPlatformUniqueId(), NAME_PartySession, *UserPlatformId);
			}
		}
		else if (UserPrimaryId.IsValid())
		{
			InvitationSubsystemType = ESocialSubsystem::Primary;

			// Primary subsystem invites can be sent directly to the user via the party interface
			const IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());
			bSentInvite = PartyInterface->SendInvitation(*OwningLocalUserId, GetPartyId(), *UserPrimaryId);
		}
	}

	OnInviteSentInternal(InvitationSubsystemType, UserToInvite, bSentInvite);

	return bSentInvite;
}

bool USocialParty::CanPromoteMember(const UPartyMember& PartyMember) const
{
	check(PartyMembersById.Contains(PartyMember.GetPrimaryNetId()));
	return IsLocalPlayerPartyLeader() && bIsMemberPromotionPossible && !PartyMember.IsPartyLeader();
}

bool USocialParty::TryPromoteMember(const UPartyMember& PartyMember)
{
	if (CanPromoteMember(PartyMember))
	{
		UE_LOG(LogParty, VeryVerbose, TEXT("Party [%s] Attempting to promote member [%s]"), *ToDebugString(), *PartyMember.ToDebugString(false));

		IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());
		return PartyInterface->PromoteMember(*OwningLocalUserId, GetPartyId(), *PartyMember.GetPrimaryNetId());
	}
	return false;
}

bool USocialParty::CanKickMember(const UPartyMember& PartyMember) const
{
	check(PartyMembersById.Contains(PartyMember.GetPrimaryNetId()));
	return IsLocalPlayerPartyLeader() && !PartyMember.IsLocalPlayer();
}

bool USocialParty::TryKickMember(const UPartyMember& PartyMember)
{
	if (CanKickMember(PartyMember))
	{
		UE_LOG(LogParty, VeryVerbose, TEXT("Party [%s] Attempting to kick member [%s]"), *ToDebugString(), *PartyMember.ToDebugString(false));

		IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());
		return PartyInterface->KickMember(*OwningLocalUserId, GetPartyId(), *PartyMember.GetPrimaryNetId());
	}
	return false;
}

const FPartyPrivacySettings& USocialParty::GetPrivacySettings() const
{
	check(PartyDataReplicator.IsValid());
	return PartyDataReplicator->GetPrivacySettings();
}

void USocialParty::InitializeParty(const TSharedRef<const FOnlineParty>& InOssParty)
{
	checkf(PartyDataReplicator.IsValid(), TEXT("Child classes of UParty MUST call PartyRepData.EstablishRepDataInstance with a valid FPartyRepData struct instance in their constructor."));
	
	if (ensure(!OssParty.IsValid()))
	{
		PartyDataReplicator->SetOwningParty(*this);

		OssParty = InOssParty;
		CurrentConfig = *InOssParty->Config;
		CurrentLeaderId = InOssParty->LeaderId;

		OwningLocalUserId = GetSocialManager().GetFirstLocalUserId(ESocialSubsystem::Primary);
		if (ensure(OwningLocalUserId.IsValid()))
		{
			InitializePartyInternal();
		}

		UE_LOG(LogParty, VeryVerbose, TEXT("New party [%s] created"), *ToDebugString());
	}
}

void USocialParty::InitializePartyInternal()
{
	IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());
	PartyInterface->AddOnPartyConfigChangedDelegate_Handle(FOnPartyConfigChangedDelegate::CreateUObject(this, &USocialParty::HandlePartyConfigChanged));
	PartyInterface->AddOnPartyDataReceivedDelegate_Handle(FOnPartyDataReceivedDelegate::CreateUObject(this, &USocialParty::HandlePartyDataReceived));
	PartyInterface->AddOnPartyJoinRequestReceivedDelegate_Handle(FOnPartyJoinRequestReceivedDelegate::CreateUObject(this, &USocialParty::HandlePartyJoinRequestReceived));
	PartyInterface->AddOnPartyJIPRequestReceivedDelegate_Handle(FOnPartyJIPRequestReceivedDelegate::CreateUObject(this, &USocialParty::HandlePartyJIPRequestReceived));
	PartyInterface->AddOnQueryPartyJoinabilityReceivedDelegate_Handle(FOnQueryPartyJoinabilityReceivedDelegate::CreateUObject(this, &USocialParty::HandleJoinabilityQueryReceived));
	PartyInterface->AddOnPartyExitedDelegate_Handle(FOnPartyExitedDelegate::CreateUObject(this, &USocialParty::HandlePartyLeft));
	PartyInterface->AddOnPartyStateChangedDelegate_Handle(FOnPartyStateChangedDelegate::CreateUObject(this, &USocialParty::HandlePartyStateChanged));

	PartyInterface->AddOnPartyMemberJoinedDelegate_Handle(FOnPartyMemberJoinedDelegate::CreateUObject(this, &USocialParty::HandlePartyMemberJoined));
	PartyInterface->AddOnPartyJIPDelegate_Handle(FOnPartyJIPDelegate::CreateUObject(this, &USocialParty::HandlePartyMemberJIP));
	PartyInterface->AddOnPartyMemberDataReceivedDelegate_Handle(FOnPartyMemberDataReceivedDelegate::CreateUObject(this, &USocialParty::HandlePartyMemberDataReceived));
	PartyInterface->AddOnPartyMemberPromotedDelegate_Handle(FOnPartyMemberPromotedDelegate::CreateUObject(this, &USocialParty::HandlePartyMemberPromoted));
	PartyInterface->AddOnPartyMemberExitedDelegate_Handle(FOnPartyMemberExitedDelegate::CreateUObject(this, &USocialParty::HandlePartyMemberExited));

	// Create a UPartyMember for every existing member on the OSS party
	TArray<TSharedRef<FOnlinePartyMember>> OssPartyMembers;
	PartyInterface->GetPartyMembers(*OwningLocalUserId, GetPartyId(), OssPartyMembers);
	for (TSharedRef<FOnlinePartyMember>& OssMember : OssPartyMembers)
	{
		GetOrCreatePartyMember(*OssMember->GetUserId());
	}
	HandlePartyStateChanged(*OwningLocalUserId, GetPartyId(), OssParty->State);

	if (IsLocalPlayerPartyLeader())
	{
		// Party leader is responsible for the party rep data, so get that all set up now
		InitializePartyRepData();
		OnLocalPlayerIsLeaderChanged(true);
	}

	TryFinishInitialization();
}

void USocialParty::TryFinishInitialization()
{
	if (!bIsInitialized)
	{
		IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld()); 
		uint32 OSSMemberCount = PartyInterface->GetPartyMemberCount(*OwningLocalUserId, GetPartyId());

		if (OSSMemberCount == PartyMembersById.Num())
		{
			bIsInitialized = true;
			GetSocialManager().NotifyPartyInitialized(*this);
		}
	}
}

void USocialParty::RefreshPublicJoinability()
{
	if (IsLocalPlayerPartyLeader())
	{
		FPartyJoinDenialReason DenialReason = DetermineCurrentJoinability();
		if (!DenialReason.HasAnyReason())
		{
			// Party isn't completely unjoinable, but is it private? This only matters for the public joinability of the party
			if (GetRepData().GetPrivacySettings().PartyType == EPartyType::Private)
			{
				DenialReason = EPartyJoinDenialReason::PartyPrivate;
			}
		}

		if (DenialReason != CurrentConfig.NotAcceptingMembersReason)
		{
			CurrentConfig.bIsAcceptingMembers = !DenialReason.HasAnyReason();
			CurrentConfig.NotAcceptingMembersReason = DenialReason;
			UpdatePartyConfig();
		}
	}
}

void USocialParty::InitializePartyRepData()
{
	UE_LOG(LogParty, Verbose, TEXT("Initializing rep data for party [%s]"), *ToDebugString());
}

FPartyPrivacySettings USocialParty::GetDesiredPrivacySettings() const
{
	return FPartyPrivacySettings();
}

void USocialParty::OnLocalPlayerIsLeaderChanged(bool bIsLeader)
{
	if (bIsLeader)
	{
		GetRepData().OnPrivacySettingsChanged().AddUObject(this, &USocialParty::HandlePrivacySettingsChanged);

		// Establish the privacy of the party to match the local player's preference
		GetMutableRepData().SetPrivacySettings(GetDesiredPrivacySettings());
	}
	else
	{
		GetRepData().OnPrivacySettingsChanged().RemoveAll(this);
	}
}

void USocialParty::OnLeftPartyInternal(EMemberExitedReason Reason)
{
	OnPartyLeft().Broadcast(Reason);
}

void USocialParty::OnInviteSentInternal(ESocialSubsystem SubsystemType, const USocialUser& InvitedUser, bool bWasSuccessful)
{
	OnInviteSent().Broadcast(InvitedUser);
}

UPartyMember* USocialParty::GetOrCreatePartyMember(const FUniqueNetId& MemberId)
{
	UPartyMember* PartyMember = nullptr;
	
	if (ensure(MemberId.IsValid()))
	{
		const FUniqueNetIdRepl MemberIdRepl(MemberId.AsShared());
		if (UPartyMember** ExistingMember = PartyMembersById.Find(MemberIdRepl))
		{
			PartyMember = *ExistingMember;
		}
		else
		{
			//@todo DanH Splitscreen: Multiple members in the party can still be local players #future
			TSubclassOf<UPartyMember> PartyMemberClass = GetDesiredMemberClass(MemberId == *OwningLocalUserId);
			if (ensure(PartyMemberClass))
			{
				const FOnlinePartyId& PartyId = GetPartyId();
				const IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());
				const TSharedPtr<FOnlinePartyMember> OssPartyMember = PartyInterface->GetPartyMember(*OwningLocalUserId, PartyId, MemberId);
				if (OssPartyMember.IsValid())
				{
					PartyMember = NewObject<UPartyMember>(this, PartyMemberClass);
					PartyMembersById.Add(MemberIdRepl, PartyMember);
					PartyMember->InitializePartyMember(OssPartyMember.ToSharedRef(), FSimpleDelegate::CreateUObject(this, &USocialParty::HandleMemberInitialized, PartyMember));

					PartyInterface->ApproveUserForRejoin(*OwningLocalUserId, PartyId, MemberId);
					RefreshPublicJoinability();

					OnPartyMemberCreated().Broadcast(*PartyMember);
				}
				else
				{
					UE_LOG(LogParty, Warning, TEXT("Cannot create party member - user [%s] is not in party [%s]"), *MemberId.ToDebugString(), *PartyId.ToDebugString());
				}
			}
		}
	}

	return PartyMember;
}

void USocialParty::HandlePartyJoinRequestReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& SenderId, const FString& Platform, const FOnlinePartyData& JoinData)
{
	if (!IsLocalPlayerPartyLeader() || PartyId != GetPartyId())
	{
		return;
	}

	IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());
#if !UE_BUILD_SHIPPING
	if (AutoApproveJoinRequests != 0)
	{
		PartyInterface->ApproveJoinRequest(LocalUserId, PartyId, SenderId, true);
		return;
	}
#endif

	const FUserPlatform MemberPlatform(Platform);
	FPartyJoinApproval JoinApproval = EvaluateJoinRequest(SenderId, MemberPlatform, JoinData, true);

	if (JoinApproval.GetApprovalAction() == EApprovalAction::Enqueue ||
		JoinApproval.GetApprovalAction() == EApprovalAction::EnqueueAndStartBeacon)
	{
		// Enqueue for a more opportune time
		UE_LOG(LogParty, Verbose, TEXT("[%s] Enqueuing approval request for %s"), *PartyId.ToString(), *SenderId.ToString());
		
		FPendingMemberApproval PendingApproval;
		PendingApproval.RecipientId.SetUniqueNetId(LocalUserId.AsShared());
		PendingApproval.SenderId.SetUniqueNetId(SenderId.AsShared());
		PendingApproval.Platform = MemberPlatform;
		PendingApproval.JoinData = JoinData.AsShared();
		PendingApproval.bIsJIPApproval = false;
		PendingApprovals.Enqueue(PendingApproval);

		if (!ReservationBeaconClient && JoinApproval.GetApprovalAction() == EApprovalAction::EnqueueAndStartBeacon)
		{
			ConnectToReservationBeacon();
		}
	}
	else
	{
		const bool bIsApproved = JoinApproval.CanJoin();
		UE_LOG(LogParty, Verbose, TEXT("[%s] Responding to approval request for %s with %s"), *PartyId.ToString(), *SenderId.ToString(), bIsApproved ? TEXT("approved") : TEXT("denied"));
		
		PartyInterface->ApproveJoinRequest(LocalUserId, PartyId, SenderId, bIsApproved, JoinApproval.GetDenialReason());
	}
}

void USocialParty::HandlePartyJIPRequestReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& SenderId)
{
	if (!IsLocalPlayerPartyLeader() || PartyId != GetPartyId())
	{
		return;
	}

	IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());
	
	FPartyJoinApproval JoinApproval = EvaluateJIPRequest(SenderId);

	if (JoinApproval.GetApprovalAction() == EApprovalAction::Enqueue ||
		JoinApproval.GetApprovalAction() == EApprovalAction::EnqueueAndStartBeacon)
	{

		FUserPlatform MemberPlatform = FUserPlatform();
		for (const UPartyMember* Member : GetPartyMembers())
		{
			if (Member->GetPrimaryNetId() == SenderId)
			{
				MemberPlatform = Member->GetRepData().GetPlatform();
				break;
			}
		}

		// Enqueue for a more opportune time
		UE_LOG(LogParty, Verbose, TEXT("[%s] Enqueuing JIP approval request for %s"), *PartyId.ToString(), *SenderId.ToString());

		FPendingMemberApproval PendingApproval;
		PendingApproval.RecipientId.SetUniqueNetId(LocalUserId.AsShared());
		PendingApproval.SenderId.SetUniqueNetId(SenderId.AsShared());
		PendingApproval.Platform = MemberPlatform;
		PendingApproval.bIsJIPApproval = true;
		PendingApprovals.Enqueue(PendingApproval);

		if (!ReservationBeaconClient && JoinApproval.GetApprovalAction() == EApprovalAction::EnqueueAndStartBeacon)
		{
			ConnectToReservationBeacon();
		}
	}
	else
	{
		const bool bIsApproved = JoinApproval.CanJoin();
		UE_LOG(LogParty, Verbose, TEXT("[%s] Responding to approval request for %s with %s"), *PartyId.ToString(), *SenderId.ToString(), bIsApproved ? TEXT("approved") : TEXT("denied"));

		PartyInterface->ApproveJIPRequest(LocalUserId, PartyId, SenderId, bIsApproved, JoinApproval.GetDenialReason());
	}
}


void USocialParty::HandleJoinabilityQueryReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& SenderId, const FString& Platform, const FOnlinePartyData& JoinData)
{
	if (PartyId == GetPartyId())
	{
		FPartyJoinApproval JoinabilityInfo = EvaluateJoinRequest(SenderId, FUserPlatform(Platform), JoinData, false);
		UE_LOG(LogParty, VeryVerbose, TEXT("[%s] Responding to approval request for %s with %s"), *PartyId.ToString(), *SenderId.ToString(), JoinabilityInfo.CanJoin() ? TEXT("approved") : TEXT("denied"));

		const IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());
		PartyInterface->RespondToQueryJoinability(LocalUserId, PartyId, SenderId, JoinabilityInfo.CanJoin(), JoinabilityInfo.GetDenialReason());
	}
}

void USocialParty::HandlePartyDataReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const TSharedRef<FOnlinePartyData>& PartyData)
{
	if (PartyId == GetPartyId())
	{
		check(PartyDataReplicator.IsValid());
		PartyDataReplicator.ProcessReceivedData(*PartyData);
	}
}

void USocialParty::HandlePartyMemberDataReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& MemberId, const TSharedRef<FOnlinePartyData>& PartyMemberData)
{
	if (PartyId == GetPartyId())
	{
		UPartyMember* UpdatedMember = GetOrCreatePartyMember(MemberId);
		if (ensure(UpdatedMember))
		{
			UpdatedMember->NotifyMemberDataReceived(PartyMemberData);
		}
	}
}

void USocialParty::HandlePartyConfigChanged(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const TSharedRef<FPartyConfiguration>& PartyConfig)
{
	if (PartyId == GetPartyId())
	{
		CurrentConfig = *OssParty->Config;
		OnPartyConfigurationChanged().Broadcast(CurrentConfig);
	}
}

void USocialParty::HandleUpdatePartyConfigComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, EUpdateConfigCompletionResult Result)
{
	if (Result == EUpdateConfigCompletionResult::Succeeded)
	{
		UE_LOG(LogParty, Verbose, TEXT("[%s] Party config updated %s"), *PartyId.ToDebugString(), ToString(Result));

		CurrentConfig = *OssParty->Config;
		OnPartyConfigurationChanged().Broadcast(CurrentConfig);
	}
	else
	{
		UE_LOG(LogParty, Warning, TEXT("Failed to update config for party [%s]"), *PartyId.ToDebugString());
	}
}

void USocialParty::HandlePartyMemberJoined(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& MemberId)
{
	if (PartyId == GetPartyId())
	{
		GetOrCreatePartyMember(MemberId);

		if (!bIsInitialized)
		{
			TryFinishInitialization();
		}
	}
}

void USocialParty::HandlePartyMemberJIP(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, bool Success)
{
	if (PartyId == GetPartyId())
	{
		// We are allowed to join the party.. start the JIP flow. 
		OnPartyJIPApprovedEvent.Broadcast(PartyId, Success);
	}
}

void USocialParty::HandlePartyMemberPromoted(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& NewLeaderId)
{
	if (PartyId == GetPartyId())
	{
		UE_LOG(LogParty, VeryVerbose, TEXT("Party member [%s] in party [%s] promoted"), *NewLeaderId.ToDebugString(), *PartyId.ToDebugString());

		if (CurrentLeaderId.IsValid() && NewLeaderId != *CurrentLeaderId)
		{
			if (UPartyMember* PreviousLeader = GetPartyMember(CurrentLeaderId))
			{
				PreviousLeader->NotifyMemberDemoted();
				if (PreviousLeader->IsLocalPlayer())
				{
					OnLocalPlayerIsLeaderChanged(false);
				}
			}
		}

		CurrentLeaderId = NewLeaderId.AsShared();

		UPartyMember* NewLeader = GetPartyMember(CurrentLeaderId);
		if (ensure(NewLeader))
		{
			NewLeader->NotifyMemberPromoted();
			if (NewLeader->IsLocalPlayer())
			{
				OnLocalPlayerIsLeaderChanged(true);
			}
		}

		// Now that the leader is gone and a new leader established, make sure the accepting state is correct
		RefreshPublicJoinability();
	}
}

void USocialParty::HandlePartyPromotionLockoutChanged(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, bool bArePromotionsLocked)
{
	if (PartyId == GetPartyId())
	{
		bIsMemberPromotionPossible = !bArePromotionsLocked;
	}
}

void USocialParty::HandleMemberInitialized(UPartyMember* Member)
{
	if (IsLocalPlayerPartyLeader())
	{
		Member->GetRepData().OnPlatformSessionIdChanged().AddUObject(this, &USocialParty::HandleMemberSessionIdChanged, Member);

		const FName MemberPlatformOssName = Member->GetPlatformOssName();
		if (FPartyPlatformSessionManager::DoesOssNeedPartySession(MemberPlatformOssName) && !GetRepData().FindSessionInfo(MemberPlatformOssName))
		{
			// We don't have session info yet for this platform, so make it now and establish this member as the owner
			FPartyPlatformSessionInfo NewSessionInfo;
			NewSessionInfo.OssName = MemberPlatformOssName;
			NewSessionInfo.OwnerPrimaryId = Member->GetPrimaryNetId();

			GetMutableRepData().UpdatePlatformSessionInfo(NewSessionInfo);
		}
	}
}

void USocialParty::HandleMemberSessionIdChanged(const FSessionId& NewSessionId, UPartyMember* Member)
{
	check(IsLocalPlayerPartyLeader());

	const FName PlatformOssName = Member->GetPlatformOssName();
	const FPartyPlatformSessionInfo* PlatformSessionInfo = GetRepData().FindSessionInfo(PlatformOssName);
	if (ensure(PlatformSessionInfo))
	{
		if (PlatformSessionInfo->IsSessionOwner(*Member))
		{
			if (NewSessionId.IsEmpty() && !PlatformSessionInfo->SessionId.IsEmpty())
			{
				//@todo DanH Sessions: I don't think this is possible - we leave the party before leaving the session. Can a player get booted from a session without DC-ing completely? #required
				ensure(false);
				UpdatePlatformSessionLeader(PlatformOssName);
			}
			else if (PlatformSessionInfo->SessionId.IsEmpty() || PlatformSessionInfo->SessionId != NewSessionId)
			{
				// The expectation here is that this was previously empty and the owner established the session
				// But if the owner created a different session for whatever reason in an edge case, update accordingly to stay accurate
				FPartyPlatformSessionInfo ModifiedSessionInfo = *PlatformSessionInfo;
				ModifiedSessionInfo.SessionId = NewSessionId;
				GetMutableRepData().UpdatePlatformSessionInfo(ModifiedSessionInfo);
			}
		}
	}
	else if (!ensure(NewSessionId.IsEmpty()))
	{
		// This member has just joined a session on a platform we have no entry for, which really shouldn't be possible
		UE_LOG(LogParty, Error, TEXT("[%s]: Member [%s] claims to be in session [%s], but we have no record of it."), *OwningLocalUserId.ToDebugString(), *Member->GetDisplayName(), *NewSessionId);
	}
}

void USocialParty::HandleLeavePartyComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, ELeavePartyCompletionResult LeaveResult, FOnLeavePartyAttemptComplete OnAttemptComplete)
{
	OnAttemptComplete.ExecuteIfBound(LeaveResult);

	FinalizePartyLeave(EMemberExitedReason::Left);
}

void USocialParty::HandlePrivacySettingsChanged(const FPartyPrivacySettings& NewPrivacySettings)
{
	check(IsLocalPlayerPartyLeader());

	const bool bIsPrivate = NewPrivacySettings.PartyType == EPartyType::Private;

	if (bIsPrivate)
	{
		CurrentConfig.PresencePermissions = PartySystemPermissions::EPermissionType::Noone;
	}
	else if (NewPrivacySettings.bOnlyLeaderFriendsCanJoin)
	{
		CurrentConfig.PresencePermissions = PartySystemPermissions::EPermissionType::Leader;
	}
	else
	{
		CurrentConfig.PresencePermissions = PartySystemPermissions::EPermissionType::Anyone;
	}

	switch (NewPrivacySettings.PartyInviteRestriction)
	{
	case EPartyInviteRestriction::AnyMember:
		CurrentConfig.InvitePermissions = PartySystemPermissions::EPermissionType::Anyone;
		break;
	case EPartyInviteRestriction::LeaderOnly:
		CurrentConfig.InvitePermissions = PartySystemPermissions::EPermissionType::Leader;
		break;
	case EPartyInviteRestriction::NoInvites:
		CurrentConfig.InvitePermissions = PartySystemPermissions::EPermissionType::Noone;
		break;
	}

	UpdatePartyConfig(bIsPrivate);
	RefreshPublicJoinability();
}

void USocialParty::HandlePartyLeft(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId)
{
	// this function is called when a party is left due to unintentional leave (e.g. disconnect)
	if (PartyId == GetPartyId())
	{
		// process an full "leave" for the party which will clean it up here and in OnlinePartyMcp
		// this will also trigger a new persistent party to be created
		LeaveParty();
	}
}

void USocialParty::HandlePartyMemberExited(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& MemberId, EMemberExitedReason ExitReason)
{
	if (PartyId == GetPartyId())
	{
		if (UPartyMember** FoundPartyMember = PartyMembersById.Find(MemberId))
		{
			if (LocalUserId == MemberId)
			{
				//@todo DanH Party: Do I get this for a self-initiated party leave as well? #required
				if (!bIsLeavingParty)
				{
					FinalizePartyLeave(ExitReason);
				}
			}
			else
			{
				// Make a direct ref before removing the entry from the map
				UPartyMember& LeftMember = **FoundPartyMember;
				PartyMembersById.Remove(FUniqueNetIdRepl(MemberId.AsShared()));

				UpdatePlatformSessionLeader(LeftMember.GetPlatformOssName());
				LeftMember.NotifyRemovedFromParty(ExitReason);
				LeftMember.MarkPendingKill();

				// Update party join state, will cause a failure on leader promotion currently
				// because we can't tell the difference between "expected leader" and "actually the new leader"
				RefreshPublicJoinability();

				if (ExitReason != EMemberExitedReason::Removed)
				{
					Online::GetPartyInterfaceChecked(GetWorld())->RemoveUserForRejoin(*OwningLocalUserId, PartyId, MemberId);
				}
				else
				{
					// TODO:  Add a timer to remove players eventually
				}
			}
		}
		else
		{
			UE_LOG(LogParty, Error, TEXT("Party [%s] received notification that member ID [%s] has exited, but cannot find them in the party"), *ToDebugString(), *MemberId.ToDebugString());
		}
	}
}

FChatRoomId USocialParty::GetChatRoomId() const
{
	return ensure(OssParty.IsValid()) ? OssParty->RoomId : FChatRoomId();
}

bool USocialParty::IsPersistentParty() const
{
	return GetPartyTypeId() == IOnlinePartySystem::GetPrimaryPartyTypeId();
}

const FOnlinePartyTypeId& USocialParty::GetPartyTypeId() const
{
	check(OssParty.IsValid());
	return OssParty->PartyTypeId;
}

const FOnlinePartyId& USocialParty::GetPartyId() const
{
	check(OssParty.IsValid());
	return *OssParty->PartyId;
}

EPartyState USocialParty::GetOssPartyState() const
{
	check(OssParty.IsValid());
	return OssParty->State;
}

bool USocialParty::IsCurrentlyCrossplaying() const
{
	TArray<FUserPlatform> AllPlatformsPresent;
	for (const UPartyMember* Member : GetPartyMembers())
	{
		const FUserPlatform& MemberPlatform = Member->GetRepData().GetPlatform();
		if (!AllPlatformsPresent.Contains(MemberPlatform))
		{
			for (const FUserPlatform& Platform : AllPlatformsPresent)
			{
				if (MemberPlatform.IsCrossplayWith(Platform))
				{
					return true;
				}
			}
			AllPlatformsPresent.Add(MemberPlatform);
		}
	}
	return false;
}

void USocialParty::StayWithPartyOnExit(bool bInStayWithParty)
{
	bStayWithPartyOnDisconnect = bInStayWithParty;
}

bool USocialParty::ShouldStayWithPartyOnExit() const
{
	return bStayWithPartyOnDisconnect;
}

bool USocialParty::IsPartyFunctionalityDegraded() const
{
	return bIsMissingXmppConnection || bIsMissingPlatformSession;
}

int32 USocialParty::GetNumPartyMembers() const
{
	return PartyMembersById.Num();
}

void USocialParty::SetPartyMaxSize(int32 NewSize)
{
	if (IsLocalPlayerPartyLeader())
	{
		if (CurrentConfig.MaxMembers != NewSize)
		{
			CurrentConfig.MaxMembers = FMath::Clamp(NewSize, 1, USocialSettings::GetDefaultMaxPartySize());
			UpdatePartyConfig();
		}
	}
}

int32 USocialParty::GetPartyMaxSize() const
{
	check(OssParty.IsValid());
	return OssParty->Config->MaxMembers;
}

FPartyJoinDenialReason USocialParty::GetPublicJoinability() const
{
	return FPartyJoinDenialReason(CurrentConfig.NotAcceptingMembersReason);
}

bool USocialParty::IsPartyFull() const
{
	return GetNumPartyMembers() >= GetPartyMaxSize();
}

bool USocialParty::IsInRestrictedGameSession() const
{
	bool bInGame = false;
	bool bGameJoinable = false;

	UWorld* World = GetWorld();
	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(World);
	if (ensure(SessionInt.IsValid()))
	{
		bool bGamePublicJoinable = false;
		bool bGameFriendJoinable = false;
		bool bGameInviteOnly = false;
		bool bGameAllowInvites = false;

		FNamedOnlineSession* GameSession = SessionInt->GetNamedSession(GetGameSessionName());
		if (GameSession && GameSession->GetJoinability(bGamePublicJoinable, bGameFriendJoinable, bGameInviteOnly, bGameAllowInvites))
		{
			bInGame = true;
			if (GameSession->SessionInfo.IsValid())
			{
				// User's game is joinable in some way if any of this is true
				bGameJoinable = bGamePublicJoinable || bGameFriendJoinable || bGameInviteOnly;
			}
		}
	}

	return bInGame && !bGameJoinable;
}

void USocialParty::HandlePreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
	if (!IsJoiningDuringLoadEnabled())
	{
		// Possibly deal with pending approvals?
		RejectAllPendingJoinRequests();
	}
	CleanupReservationBeacon();
}

void USocialParty::UpdatePartyConfig(bool bResetAccessKey)
{
	check(IsLocalPlayerPartyLeader());

	UE_LOG(LogParty, Verbose, TEXT("Party [%s] attempting to update party config"), *ToDebugString());

	IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());
	PartyInterface->UpdateParty(*OwningLocalUserId, GetPartyId(), CurrentConfig, bResetAccessKey, FOnUpdatePartyComplete::CreateUObject(this, &USocialParty::HandleUpdatePartyConfigComplete));
}

UPartyMember* USocialParty::GetMemberInternal(const FUniqueNetIdRepl& MemberId) const
{
	UPartyMember* const* Member = PartyMembersById.Find(MemberId);
	return Member ? *Member : nullptr;
}

void USocialParty::LeaveParty(const FOnLeavePartyAttemptComplete& OnLeaveAttemptComplete)
{
	if (bIsLeavingParty)
	{
		// Already working on it!
		OnLeaveAttemptComplete.ExecuteIfBound(ELeavePartyCompletionResult::LeavePending);
	}
	else
	{
		UE_LOG(LogParty, Verbose, TEXT("Attempting to leave party [%s]"), *ToDebugString());

		BeginLeavingParty(EMemberExitedReason::Left);

		const IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());
		FOnLeavePartyComplete OnLeaveComplete = FOnLeavePartyComplete::CreateUObject(this, &USocialParty::HandleLeavePartyComplete, OnLeaveAttemptComplete);
		PartyInterface->LeaveParty(*OwningLocalUserId, GetPartyId(), OnLeaveComplete);
	}
}

ULocalPlayer& USocialParty::GetOwningLocalPlayer() const
{
	//@todo DanH Party: This is a wee bit heavy - should be able to do this in fewer steps
	return GetOwningLocalMember().GetSocialUser().GetOwningToolkit().GetOwningLocalPlayer();
}

bool USocialParty::IsLocalPlayerPartyLeader() const
{
	return OwningLocalUserId == CurrentLeaderId;
}

bool USocialParty::IsNetDriverFromReservationBeacon(const UNetDriver* const InNetDriver) const
{
	const FName NetDriverName = InNetDriver->NetDriverName;
	return (ReservationBeaconClient && NetDriverName == ReservationBeaconClient->GetNetDriverName()) || (NetDriverName == LastReservationBeaconClientNetDriverName);
}

FString USocialParty::ToDebugString() const
{
	const FString LeaderStr = GetPartyLeader() ? GetPartyLeader()->ToDebugString(false) : CurrentLeaderId.ToDebugString();
	const FString LocalOwnerStr = IsCurrentlyLeaving() ? OwningLocalUserId.ToDebugString() : GetOwningLocalMember().ToDebugString(false);
	return *FString::Printf(TEXT("%s, LocalOwner (%s), Leader (%s)"), *GetPartyId().ToDebugString(), *LocalOwnerStr, *LeaderStr);
}

FPartyJoinDenialReason USocialParty::DetermineCurrentJoinability() const
{
	if (IsInRestrictedGameSession())
	{
		return EPartyJoinDenialReason::GameFull;
	}
	else if (IsPartyFull())
	{
		return EPartyJoinDenialReason::PartyFull;
	}

	return EPartyJoinDenialReason::NoReason;
}

TSubclassOf<UPartyMember> USocialParty::GetDesiredMemberClass(bool bLocalPlayer) const
{
	return UPartyMember::StaticClass();
}

void USocialParty::HandlePartyStateChanged(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, EPartyState PartyState)
{
	if (PartyState == EPartyState::Disconnected)
	{
		// If we transition to the disconnected state, then we are lacking an XMPP connection (or logged out of MCP?)
		SetIsMissingXmppConnection(true);
	}
	else if (PartyState == EPartyState::Active)
	{
		// If we transition to the active state, then we have an XMPP connection
		SetIsMissingXmppConnection(false);
	}
	OnPartyStateChanged().Broadcast(PartyState);
}

void USocialParty::ConnectToReservationBeacon()
{
	if (IsLocalPlayerPartyLeader() && !ReservationBeaconClient)
	{
		FPendingMemberApproval NextApproval;
		if (PendingApprovals.Peek(NextApproval))
		{
			bool bStartedConnection = false;

			// Clear out our cached net driver name, we're going to create a new one here
			LastReservationBeaconClientNetDriverName = NAME_None;

			UWorld* World = GetWorld();
			check(World);
			IOnlineSessionPtr SessionInterface = Online::GetSessionInterface(World);
			if (SessionInterface.IsValid())
			{
				const FName GameSessionName = GetGameSessionName();
				if (FNamedOnlineSession* Session = SessionInterface->GetNamedSession(GameSessionName))
				{
					FString URL;
					if (ensure(SessionInterface->GetResolvedConnectString(GameSessionName, URL, NAME_BeaconPort)))
					{
						// Reconnect to the reservation beacon to maintain our place in the game (just until actual joined, holds place for all party members)
						ReservationBeaconClient = World->SpawnActor<APartyBeaconClient>(ReservationBeaconClientClass);
						if (ReservationBeaconClient)
						{
							UE_LOG(LogParty, Verbose, TEXT("Party [%s] created reservation beacon [%s]."), *ToDebugString(), *ReservationBeaconClient->GetName());

							ReservationBeaconClient->OnHostConnectionFailure().BindUObject(this, &USocialParty::HandleBeaconHostConnectionFailed);
							ReservationBeaconClient->OnReservationRequestComplete().BindUObject(this, &USocialParty::HandleReservationRequestComplete);

							FPlayerReservation Reservation;
							Reservation.UniqueId = NextApproval.SenderId;
							Reservation.Platform = NextApproval.Platform;

							if (!NextApproval.bIsJIPApproval)
							{
								const ECrossplayPreference CrossplayPreference = GetCrossplayPreferenceFromJoinData(*NextApproval.JoinData);
								Reservation.bAllowCrossplay = (CrossplayPreference == ECrossplayPreference::OptedIn);
							}
							else
							{
								Reservation.bAllowCrossplay = true; // THis will not matter since we are JIP, and the session already has crossplay set.
							}

							TArray<FPlayerReservation> ReservationAsArray;
							ReservationAsArray.Add(Reservation);
							bStartedConnection = ReservationBeaconClient->RequestReservationUpdate(URL, Session->GetSessionIdStr(), GetPartyLeader()->GetPrimaryNetId(), ReservationAsArray);
						}
					}
				}
			}

			if (!bStartedConnection)
			{
				HandleBeaconHostConnectionFailed();
			}
		}
	}
}

void USocialParty::RejectAllPendingJoinRequests()
{
	IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());

	const FOnlinePartyId& PartyId = GetPartyId();
	FPendingMemberApproval PendingApproval;
	while (!PendingApprovals.IsEmpty())
	{
		PendingApprovals.Dequeue(PendingApproval);
		UE_LOG(LogParty, Verbose, TEXT("[%s] Responding to approval request for %s with denied"), *PartyId.ToString(), *PendingApproval.SenderId.ToString());
		if (PendingApproval.bIsJIPApproval)
		{
			PartyInterface->ApproveJIPRequest(*PendingApproval.RecipientId, PartyId, *PendingApproval.SenderId, false, (int32)EPartyJoinDenialReason::Busy);
		}
		else
		{
			PartyInterface->ApproveJoinRequest(*PendingApproval.RecipientId, PartyId, *PendingApproval.SenderId, false, (int32)EPartyJoinDenialReason::Busy);
		}
	}
}

void USocialParty::HandleBeaconHostConnectionFailed()
{
	UE_LOG(LogParty, Verbose, TEXT("Host connection failed for reservation beacon [%s]"), ReservationBeaconClient ? *ReservationBeaconClient->GetName() : TEXT(""));

	// empty the queue, denying all requests
	RejectAllPendingJoinRequests();
	CleanupReservationBeacon();
}

APartyBeaconClient* USocialParty::CreateReservationBeaconClient()
{
	UWorld* World = GetWorld();
	check(World);

	// Clear out our cached net driver name, we're going to create a new one here
	LastReservationBeaconClientNetDriverName = NAME_None;
	ReservationBeaconClient = World->SpawnActor<APartyBeaconClient>(ReservationBeaconClientClass);
	
	return ReservationBeaconClient;
}

void USocialParty::PumpApprovalQueue()
{
	// Check if there are any more while we are connected
	FPendingMemberApproval NextApproval;
	if (PendingApprovals.Peek(NextApproval))
	{
		if (ensure(ReservationBeaconClient))
		{
			if (NextApproval.bIsJIPApproval == false)
			{
				// This is a request to join our party
				ECrossplayPreference CrossplayPreference = GetCrossplayPreferenceFromJoinData(*NextApproval.JoinData);

				FPlayerReservation NewPlayerRes;
				NewPlayerRes.UniqueId = NextApproval.SenderId;
				NewPlayerRes.Platform = NextApproval.Platform;
				NewPlayerRes.bAllowCrossplay = (CrossplayPreference == ECrossplayPreference::OptedIn);

				TArray<FPlayerReservation> PlayersToAdd;
				PlayersToAdd.Add(NewPlayerRes);

				ReservationBeaconClient->RequestReservationUpdate(GetPartyLeader()->GetPrimaryNetId(), PlayersToAdd);
			}
			else
			{
				// This is a request from a party member to join a JIP game.
				FPlayerReservation NewPlayerRes;
				NewPlayerRes.UniqueId = NextApproval.SenderId;
				NewPlayerRes.Platform = NextApproval.Platform;

				// This doesn't matter, since the crossplay state of the match has already been set.
				NewPlayerRes.bAllowCrossplay = true;

				TArray<FPlayerReservation> PlayersToAdd;
				PlayersToAdd.Add(NewPlayerRes);

				ReservationBeaconClient->RequestReservationUpdate(GetPartyLeader()->GetPrimaryNetId(), PlayersToAdd);
			}
		}
		else
		{
			UE_LOG(LogParty, Warning, TEXT("ReservationBeaconClient is null while trying to process more requests"));
			RejectAllPendingJoinRequests();
		}
	}
	else
	{
		CleanupReservationBeacon();
	}
}

void USocialParty::HandleReservationRequestComplete(EPartyReservationResult::Type ReservationResponse)
{
	UE_LOG(LogParty, Verbose, TEXT("Reservation request complete with response: %s"), EPartyReservationResult::ToString(ReservationResponse));

	const bool bReservationApproved = ReservationResponse == EPartyReservationResult::ReservationAccepted || ReservationResponse == EPartyReservationResult::ReservationDuplicate;
	const FPartyJoinDenialReason DenialReason = ReservationResponse == EPartyReservationResult::ReservationDenied_CrossPlayRestriction ? EPartyJoinDenialReason::JoinerCrossplayRestricted : EPartyJoinDenialReason::NoReason;

	if (bReservationApproved || DenialReason.HasAnyReason())
	{
		// There should be at least the one
		FPendingMemberApproval PendingApproval;
		if (ensure(PendingApprovals.Dequeue(PendingApproval)))
		{
			IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());
			if (PendingApproval.bIsJIPApproval)
			{
				// This player is already in our party. ApproveJIPRequest
				PartyInterface->ApproveJIPRequest(*PendingApproval.RecipientId, GetPartyId(), *PendingApproval.SenderId, bReservationApproved, DenialReason);
			}
			else
			{
				PartyInterface->ApproveJoinRequest(*PendingApproval.RecipientId, GetPartyId(), *PendingApproval.SenderId, bReservationApproved, DenialReason);
			}
		}
		PumpApprovalQueue();
	}
	else
	{
		//@todo DanH Party: I don't quite follow this - why would one reservation rejection mean we want to fully reject everything queued? #required
		// empty the queue, denying all requests
		RejectAllPendingJoinRequests();
		CleanupReservationBeacon();
	}
}

void USocialParty::CleanupReservationBeacon()
{
	if (ReservationBeaconClient)
	{
		UE_LOG(LogParty, Verbose, TEXT("Party reservation beacon cleanup while in state %s, pending approvals: %s"), ToString(ReservationBeaconClient->GetConnectionState()), !PendingApprovals.IsEmpty() ? TEXT("true") : TEXT("false"));

		LastReservationBeaconClientNetDriverName = ReservationBeaconClient->GetNetDriverName();
		ReservationBeaconClient->OnHostConnectionFailure().Unbind();
		ReservationBeaconClient->OnReservationRequestComplete().Unbind();
		ReservationBeaconClient->DestroyBeacon();
		ReservationBeaconClient = nullptr;
	}
}

FName USocialParty::GetGameSessionName() const
{
	const APlayerController* OwnerPC = GetOwningLocalPlayer().GetPlayerController(GetWorld());
	if (OwnerPC && OwnerPC->PlayerState)
	{
		return OwnerPC->PlayerState->SessionName;
	}
	return NAME_GameSession;
}

void USocialParty::SetIsMissingPlatformSession(bool bInIsMissingPlatformSession)
{
	if (bInIsMissingPlatformSession != bIsMissingPlatformSession)
	{
		UE_LOG(LogParty, VeryVerbose, TEXT("Party [%s] is %s missing platform session"), *ToDebugString(), bInIsMissingPlatformSession ? TEXT("now") : TEXT("no longer"));

		const bool bWasPartyFunctionalityDegraded = IsPartyFunctionalityDegraded();
		bIsMissingPlatformSession = bInIsMissingPlatformSession;
		const bool bIsPartyFunctionalityDegraded = IsPartyFunctionalityDegraded();
		if (bWasPartyFunctionalityDegraded != bIsPartyFunctionalityDegraded)
		{
			OnPartyFunctionalityDegradedChanged().Broadcast(bIsPartyFunctionalityDegraded);
		}
	}
}

void USocialParty::SetIsMissingXmppConnection(bool bInMissingXmppConnection)
{
	if (bInMissingXmppConnection != bIsMissingXmppConnection)
	{
		UE_LOG(LogParty, VeryVerbose, TEXT("Party [%s] is %s missing XMPP connection"), *ToDebugString(), bInMissingXmppConnection ? TEXT("now") : TEXT("no longer"));

		const bool bWasPartyFunctionalityDegraded = IsPartyFunctionalityDegraded();
		bIsMissingXmppConnection = bInMissingXmppConnection;
		const bool bIsPartyFunctionalityDegraded = IsPartyFunctionalityDegraded();
		if (bWasPartyFunctionalityDegraded != bIsPartyFunctionalityDegraded)
		{
			OnPartyFunctionalityDegradedChanged().Broadcast(bIsPartyFunctionalityDegraded);
		}
	}
}

void USocialParty::BeginLeavingParty(EMemberExitedReason Reason)
{
	if (!bIsLeavingParty)
	{
		bIsLeavingParty = true;
		CleanupReservationBeacon();
		OnPartyLeaveBegin().Broadcast(Reason);
	}
}

void USocialParty::FinalizePartyLeave(EMemberExitedReason Reason)
{
	UE_LOG(LogParty, Verbose, TEXT("Local player [%s] is no longer in party [%s]. Reason [%s]."), *GetOwningLocalMember().ToDebugString(false), *ToDebugString(), ToString(Reason));

	if (!bIsLeavingParty)
	{
		// If we haven't already announced the leave begin, do so before shutting down completely
		BeginLeavingParty(Reason);
	}

	for (UPartyMember* PartyMember : GetPartyMembers())
	{
		PartyMember->NotifyRemovedFromParty(EMemberExitedReason::Unknown);
		PartyMember->MarkPendingKill();
	}

	OnLeftPartyInternal(Reason);

	// Wait until the very end to actually clear out the members array, since otherwise the exact order of event broadcasting matters and becomes a hassle
	PartyMembersById.Reset();
}

void USocialParty::UpdatePlatformSessionLeader(FName PlatformOssName)
{
	if (const FPartyPlatformSessionInfo* PlatformSessionInfo = GetRepData().FindSessionInfo(PlatformOssName))
	{
		UPartyMember* NewSessionOwner = nullptr;
		for (UPartyMember* PartyMember : GetPartyMembers())
		{
			if (PlatformSessionInfo->IsInSession(*PartyMember))
			{
				NewSessionOwner = PartyMember;
				if (PlatformSessionInfo->IsSessionOwner(*PartyMember))
				{
					// The current owner is still valid - bail and do nothing
					return;
				}
				else if (PartyMember->IsLocalPlayer())
				{
					// Prefer the local player when possible
					break;
				}
			}
		}

		if (NewSessionOwner)
		{
			UE_LOG(LogParty, Verbose, TEXT("Party [%s] updating session owner on platform [%s] to [%s]"), *ToDebugString(), *PlatformOssName.ToString(), *NewSessionOwner->ToDebugString(false));

			FPartyPlatformSessionInfo ModifiedSessionInfo = *PlatformSessionInfo;
			ModifiedSessionInfo.OwnerPrimaryId = NewSessionOwner->GetPrimaryNetId();
			GetMutableRepData().UpdatePlatformSessionInfo(ModifiedSessionInfo);
		}
		else
		{
			UE_LOG(LogParty, Verbose, TEXT("Party [%s] no longer has any members on platform [%s], clearing session info entry."), *ToDebugString(), *PlatformOssName.ToString());

			PlatformSessionInfo = nullptr;
			GetMutableRepData().ClearPlatformSessionInfo(PlatformOssName);
		}
	}
}
