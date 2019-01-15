// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SocialManager.h"
#include "SocialToolkit.h"
#include "SocialSettings.h"

#include "Interactions/CoreInteractions.h"
#include "Interactions/PartyInteractions.h"
#include "Party/PartyTypes.h"
#include "Party/SocialParty.h"
#include "Party/PartyMember.h"
#include "Party/PartyPlatformSessionMonitor.h"
#include "User/SocialUser.h"

#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystemUtils.h"
#include "Engine/LocalPlayer.h"
#include "OnlineSubsystemSessionSettings.h"
#include "Misc/Base64.h"

//////////////////////////////////////////////////////////////////////////
// FRejoinableParty
//////////////////////////////////////////////////////////////////////////

USocialManager::FRejoinableParty::FRejoinableParty(const USocialParty& SourceParty)
	: PartyId(SourceParty.GetPartyId().AsShared())
{
	for (UPartyMember* Member : SourceParty.GetPartyMembers())
	{
		// Rejoin fails if the local user's Id is in there as a member, so only include everyone else
		if (Member != &SourceParty.GetOwningLocalMember())
		{
			FUniqueNetIdRepl MemberId = Member->GetPrimaryNetId();
			if (MemberId.IsValid())
			{
				MemberIds.Add(MemberId->AsShared());
			}
		}		
	}
}

//////////////////////////////////////////////////////////////////////////
// FJoinPartyAttempt
//////////////////////////////////////////////////////////////////////////

USocialManager::FJoinPartyAttempt::FJoinPartyAttempt(const USocialUser* InTargetUser, const FOnlinePartyTypeId& InPartyTypeId, const FOnJoinPartyAttemptComplete& InOnJoinComplete)
	: TargetUser(InTargetUser)
	, PartyTypeId(InPartyTypeId)
	, OnJoinComplete(InOnJoinComplete)
{}

USocialManager::FJoinPartyAttempt::FJoinPartyAttempt(TSharedRef<const FRejoinableParty> InRejoinInfo)
	: PartyTypeId(IOnlinePartySystem::GetPrimaryPartyTypeId())
	, RejoinInfo(InRejoinInfo)
{}

FString USocialManager::FJoinPartyAttempt::ToDebugString() const
{
	return FString::Printf(TEXT("IsRejoin (%s), TargetUser (%s), PartyId (%s), TypeId (%d), TargetUserPlatformId (%s), PlatformSessionId (%s)"),
		RejoinInfo.IsValid() ? TEXT("true") : TEXT("false"),
		TargetUser.IsValid() ? *TargetUser->ToDebugString() : TEXT("invalid"),
		JoinInfo.IsValid() ? *JoinInfo->GetPartyId()->ToDebugString() : RejoinInfo.IsValid() ? *RejoinInfo->PartyId->ToDebugString() : TEXT("unknown"),
		PartyTypeId.GetValue(),
		*TargetUserPlatformId.ToDebugString(),
		*PlatformSessionId);
}

const FName USocialManager::FJoinPartyAttempt::Step_FindPlatformSession = TEXT("FindPlatformSession");
const FName USocialManager::FJoinPartyAttempt::Step_QueryJoinability = TEXT("QueryJoinability");
const FName USocialManager::FJoinPartyAttempt::Step_LeaveCurrentParty = TEXT("LeaveCurrentParty");
const FName USocialManager::FJoinPartyAttempt::Step_JoinParty = TEXT("JoinParty");
const FName USocialManager::FJoinPartyAttempt::Step_DeferredPartyCreation = TEXT("DeferredPartyCreation");

//////////////////////////////////////////////////////////////////////////
// USocialManager
//////////////////////////////////////////////////////////////////////////

TArray<ESocialSubsystem> USocialManager::DefaultSubsystems;
TArray<FSocialInteractionHandle> USocialManager::RegisteredInteractions;
TMap<TWeakObjectPtr<UGameInstance>, TWeakObjectPtr<USocialManager>> USocialManager::AllManagersByGameInstance;

/*static*/bool USocialManager::IsSocialSubsystemEnabled(ESocialSubsystem SubsystemType)
{
	return GetSocialOss(nullptr, SubsystemType) != nullptr;
}

/*static*/FName USocialManager::GetSocialOssName(ESocialSubsystem SubsystemType)
{
	if (IOnlineSubsystem* SocialOSS = GetSocialOss(nullptr, SubsystemType))
	{
		return SocialOSS->GetSubsystemName();
	}
	return NAME_None;
}

/*static*/IOnlineSubsystem* USocialManager::GetSocialOss(UWorld* World, ESocialSubsystem SubsystemType)
{
	if (SubsystemType == ESocialSubsystem::Primary)
	{
		IOnlineSubsystem* PrimaryOss = Online::GetSubsystem(World);
		if (PrimaryOss && !PrimaryOss->GetSubsystemName().IsEqual(NULL_SUBSYSTEM))
		{
			return PrimaryOss;
		}
	}
	else if (SubsystemType == ESocialSubsystem::Platform)
	{
		if (IOnlineSubsystem::IsEnabled(TENCENT_SUBSYSTEM))
		{
			return Online::GetSubsystem(World, TENCENT_SUBSYSTEM);
		}
		/*else if (IOnlineSubsystem::IsEnabled(STEAM_SUBSYSTEM))
		{
			return Online::GetSubsystem(World, STEAM_SUBSYSTEM);
		}*/
		else
		{
			return IOnlineSubsystem::GetByPlatform();
		}
	}
	return nullptr;
}

FUserPlatform USocialManager::GetLocalUserPlatform()
{
	return IOnlineSubsystem::GetLocalPlatformName();
}

USocialManager::USocialManager()
	: ToolkitClass(USocialToolkit::StaticClass())
{
	if (!IsTemplate())
	{
		if (ensureMsgf(!AllManagersByGameInstance.Contains(&GetGameInstance()), TEXT("More than one SocialManager has been created for a game instance! Chaos is sure to ensue. Make sure you only have a single instance living on your GameInstance.")))
		{
			AllManagersByGameInstance.Add(&GetGameInstance(), this);
		}

		if (DefaultSubsystems.Num() == 0)
		{
			//@todo DanH social: This module assumes there is a primary (aka mcp) OSS available that other accounts are linked to. Consider whether we want to support platform-only situations with this module #future
			if (IsSocialSubsystemEnabled(ESocialSubsystem::Primary))
			{
				DefaultSubsystems.Add(ESocialSubsystem::Primary);

				if (IsSocialSubsystemEnabled(ESocialSubsystem::Platform))
				{
					DefaultSubsystems.Add(ESocialSubsystem::Platform);
				}
			}
		}
	}
}

void USocialManager::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	USocialManager& This = *CastChecked<USocialManager>(InThis);
	Collector.AddReferencedObjects(This.JoinedPartiesByTypeId);
	Collector.AddReferencedObjects(This.LeavingPartiesByTypeId);
}

void USocialManager::InitSocialManager()
{
	if (RegisteredInteractions.Num() == 0)
	{
		RegisterSocialInteractions();
	}

	if (FPartyPlatformSessionManager::DoesOssNeedPartySession(GetSocialOssName(ESocialSubsystem::Platform)))
	{
		// We're on a platform that requires a platform session backing each party, so spin up the manager to take care of that
		PartySessionManager = FPartyPlatformSessionManager::Create(*this);
	}

	UGameInstance& GameInstance = GetGameInstance();
	GameInstance.OnNotifyPreClientTravel().AddUObject(this, &USocialManager::HandlePreClientTravel);
	if (GameInstance.GetGameViewportClient())
	{
		HandleGameViewportInitialized();
	}
	else
	{
		UGameViewportClient::OnViewportCreated().AddUObject(this, &USocialManager::HandleGameViewportInitialized);
	}

	//@todo DanH Sessions: So it's only at the FortOnlineSessionClient level that the console session interface is used #required
	//		Technically I could have the platform session manager just listen to the platform session interface itself for invites, but then I'm sure we miss out on functionality
	//			Having this happen only at the Fort level though is a travesty, we NEED to know when a platform session invite has been accepted
	/*if (UOnlineSession* OnlineSession = GameInstance.GetOnlineSession())
	{
		OnlineSession->OnSessionInviteAccepted().BindUObject(this, &UFortSocialManager::ProcessConsoleInvite);
	}*/

	// Because multiclient PIE, we need to have a world to be able to access the appropriate OSS suite
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &USocialManager::HandleWorldEstablished);
	if (UWorld* World = GetWorld())
	{
		HandleWorldEstablished(World);
	}
}

void USocialManager::ShutdownSocialManager()
{
	bCanCreatePartyObjects = false;
	JoinAttemptsByTypeId.Reset();

	// Mark all parties and members pending kill to prevent any callbacks from being triggered on them during shutdown
	const auto ShutdownPartiesFunc =
		[this] (TMap<FOnlinePartyTypeId, USocialParty*>& PartiesByTypeId)
		{
			for (auto& TypeIdPartyPair : PartiesByTypeId)
			{
				for (UPartyMember* PartyMember : TypeIdPartyPair.Value->GetPartyMembers())
				{
					PartyMember->MarkPendingKill();
				}
				TypeIdPartyPair.Value->MarkPendingKill();
			}
			PartiesByTypeId.Reset();
		};

	ShutdownPartiesFunc(JoinedPartiesByTypeId);
	ShutdownPartiesFunc(LeavingPartiesByTypeId);

	RejoinableParty.Reset();

	// We could have outstanding OSS queries and requests, and we are no longer interested in getting any callbacks triggered
	MarkPendingKill();
}

USocialToolkit& USocialManager::GetSocialToolkit(ULocalPlayer& LocalPlayer) const
{
	USocialToolkit* FoundToolkit = nullptr;
	for (USocialToolkit* Toolkit : SocialToolkits)
	{
		if (&LocalPlayer == &Toolkit->GetOwningLocalPlayer())
		{
			FoundToolkit = Toolkit;
			break;
		}
	}

	checkf(FoundToolkit, TEXT("No SocialToolkit exists for LocalPlayer [%s]. Should be impossible. Was the LocalPlayer created correctly via UGameInstance::CreateLocalPlayer?"), *LocalPlayer.GetName());
	return *FoundToolkit;
}

USocialToolkit* USocialManager::GetSocialToolkit(int32 LocalPlayerNum) const
{
	for (USocialToolkit* Toolkit : SocialToolkits)
	{
		if (Toolkit->GetLocalUserNum() == LocalPlayerNum)
		{
			return Toolkit;
		}
	}
	return nullptr;
}

void USocialManager::HandlePlatformSessionInviteAccepted(const TSharedRef<const FUniqueNetId>& LocalUserId, const FOnlineSessionSearchResult& InviteResult)
{
	UE_LOG(LogParty, Log, TEXT("LocalUser w/ ID [%s] has accepted platform party session invite. Attempting to join persistent party."), *LocalUserId->ToDebugString());

	// Session invites are always for the persistent party
	const FOnlinePartyTypeId PersistentPartyTypeId = IOnlinePartySystem::GetPrimaryPartyTypeId();

	FJoinPartyAttempt NewAttempt(nullptr, PersistentPartyTypeId, FOnJoinPartyAttemptComplete());
	FJoinPartyResult ValidationResult = ValidateJoinAttempt(PersistentPartyTypeId);
	if (ValidationResult.WasSuccessful())
	{
		FJoinPartyAttempt& JoinAttempt = JoinAttemptsByTypeId.Add(PersistentPartyTypeId, NewAttempt);

		JoinAttempt.JoinInfo = GetJoinInfoFromSession(InviteResult);
		if (JoinAttempt.JoinInfo.IsValid())
		{
			QueryPartyJoinabilityInternal(JoinAttempt);
		}
		else
		{
			FinishJoinPartyAttempt(JoinAttempt, FJoinPartyResult(EPartyJoinDenialReason::PlatformSessionMissingJoinInfo));
		}
	}
	else
	{
		// As in JoinParty, we don't want to call FinishJoinPartyAttempt when the attempt object isn't registered in our map yet
		OnJoinPartyAttemptCompleteInternal(NewAttempt, ValidationResult);
	}
}

USocialToolkit* USocialManager::GetFirstLocalUserToolkit() const
{
	if (SocialToolkits.Num() > 0)
	{
		return SocialToolkits[0];
	}
	return nullptr;
}

FUniqueNetIdRepl USocialManager::GetFirstLocalUserId(ESocialSubsystem SubsystemType) const
{
	if (SocialToolkits.Num() > 0)
	{
		return SocialToolkits[0]->GetLocalUserNetId(SubsystemType);
	}
	return FUniqueNetIdRepl();
}

int32 USocialManager::GetFirstLocalUserNum() const
{
	if (SocialToolkits.Num() > 0)
	{
		return SocialToolkits[0]->GetLocalUserNum();
	}
	return 0;
}

void USocialManager::CreateParty(const FOnlinePartyTypeId& PartyTypeId, const FPartyConfiguration& PartyConfig, const FOnCreatePartyAttemptComplete& OnCreatePartyComplete)
{
	if (const USocialParty* ExistingParty = GetPartyInternal(PartyTypeId, true))
	{
		UE_LOG(LogParty, Warning, TEXT("Existing party [%s] of type [%d] found when trying to create a new one. Cannot create new one until the existing one has been left."), *ExistingParty->GetPartyId().ToDebugString());
		OnCreatePartyComplete.ExecuteIfBound(ECreatePartyCompletionResult::AlreadyInPartyOfSpecifiedType);
	}
	else
	{
		// Only the primary local player can create parties (which secondary players will auto-join)
		FUniqueNetIdRepl PrimaryLocalUserId = GetFirstLocalUserId(ESocialSubsystem::Primary);
		IOnlinePartyPtr PartyInterface = Online::GetPartyInterface(GetWorld());
		if (PartyInterface.IsValid() && PrimaryLocalUserId.IsValid())
		{
			PartyInterface->CreateParty(*PrimaryLocalUserId, PartyTypeId, PartyConfig, FOnCreatePartyComplete::CreateUObject(this, &USocialManager::HandleCreatePartyComplete, PartyTypeId, OnCreatePartyComplete));
		}
		else
		{
			UE_LOG(LogParty, Warning, TEXT("Cannot create party of type [%d] - no PartyInterface available on the primary OSS [%s]"), PartyTypeId.GetValue(), *GetSocialOssName(ESocialSubsystem::Primary).ToString());
			OnCreatePartyComplete.ExecuteIfBound(ECreatePartyCompletionResult::UnknownClientFailure);
		}
	}
}

void USocialManager::CreatePersistentParty(const FOnCreatePartyAttemptComplete& OnCreatePartyComplete)
{
	UE_LOG(LogParty, Log, TEXT("Attempting to create new persistent party"));

	// The persistent party starts off closed by default, and will update its config as desired after initializing
	FPartyConfiguration InitialPersistentPartyConfig;
	InitialPersistentPartyConfig.JoinRequestAction = EJoinRequestAction::Manual;
	InitialPersistentPartyConfig.bIsAcceptingMembers = false;
	InitialPersistentPartyConfig.bShouldRemoveOnDisconnection = true;
	InitialPersistentPartyConfig.PresencePermissions = PartySystemPermissions::EPermissionType::Noone;
	InitialPersistentPartyConfig.InvitePermissions = PartySystemPermissions::EPermissionType::Noone;
	InitialPersistentPartyConfig.MaxMembers = USocialSettings::GetDefaultMaxPartySize();

	CreateParty(IOnlinePartySystem::GetPrimaryPartyTypeId(), InitialPersistentPartyConfig, OnCreatePartyComplete);
}

void USocialManager::RegisterSocialInteractions()
{
	// Register Party Interactions
	RegisterInteraction<FSocialInteraction_JoinParty>();
	RegisterInteraction<FSocialInteraction_InviteToParty>();
	RegisterInteraction<FSocialInteraction_AcceptPartyInvite>();
	RegisterInteraction<FSocialInteraction_RejectPartyInvite>();
	RegisterInteraction<FSocialInteraction_PromoteToPartyLeader>();
	RegisterInteraction<FSocialInteraction_KickPartyMember>();
	RegisterInteraction<FSocialInteraction_LeaveParty>();

	// Register Core interactions
	RegisterInteraction<FSocialInteraction_AddFriend>();
	RegisterInteraction<FSocialInteraction_AddPlatformFriend>();
	RegisterInteraction<FSocialInteraction_AcceptFriendInvite>();
	RegisterInteraction<FSocialInteraction_RejectFriendInvite>();
	RegisterInteraction<FSocialInteraction_PrivateMessage>();
	RegisterInteraction<FSocialInteraction_RemoveFriend>();
	RegisterInteraction<FSocialInteraction_Block>();
	RegisterInteraction<FSocialInteraction_Unblock>();
	RegisterInteraction<FSocialInteraction_ShowPlatformProfile>();
}

FJoinPartyResult USocialManager::ValidateJoinAttempt(const FOnlinePartyTypeId& PartyTypeId) const
{
	UE_LOG(LogParty, VeryVerbose, TEXT("Validating join attempt of party of type [%d]"), PartyTypeId.GetValue());

	FJoinPartyResult ValidationResult;

	IOnlinePartyPtr PartyInterface = Online::GetPartyInterface(GetWorld());
	if (!PartyInterface.IsValid())
	{
		return FPartyJoinDenialReason(EPartyJoinDenialReason::OssUnavailable);
	}
	else if (JoinAttemptsByTypeId.Contains(PartyTypeId))
	{
		//@todo DanH Party: Is this ok? Or should we mark the existing attempt as something we should bail asap and restart the process with the new target? #suggested
		//		We'll need to track join attempts by party ID if that's the case and just be diligent about making sure that only 1 of the same party type is actually live at a time.
		return EJoinPartyCompletionResult::AlreadyJoiningParty;
	}
	else if (!GetPartyClassForType(PartyTypeId))
	{
		return FPartyJoinDenialReason(EPartyJoinDenialReason::MissingPartyClassForTypeId);
	}

	return ValidationResult;
}

FJoinPartyResult USocialManager::ValidateJoinTarget(const USocialUser& UserToJoin, const FOnlinePartyTypeId& PartyTypeId) const
{
	UE_LOG(LogParty, VeryVerbose, TEXT("Validating user [%s] as join target of party type [%d]"), *UserToJoin.ToDebugString(), PartyTypeId.GetValue());

	FJoinPartyResult PartyTypeValidation = ValidateJoinAttempt(PartyTypeId);
	if (!PartyTypeValidation.WasSuccessful())
	{
		// Don't bother checking the user for info if we can't even join anyway
		return PartyTypeValidation;
	}
	else if (!UserToJoin.GetOwningToolkit().IsOwnerLoggedIn())
	{
		return FPartyJoinDenialReason(EPartyJoinDenialReason::NotLoggedIn);
	}
	else if (UserToJoin.GetOnlineStatus() == EOnlinePresenceState::Away)
	{
		return FPartyJoinDenialReason(EPartyJoinDenialReason::TargetUserAway);
	}
	else if (UserToJoin.GetPartyMember(PartyTypeId))
	{
		return EJoinPartyCompletionResult::AlreadyInParty;
	}
	else if (UserToJoin.IsBlocked())
	{
		return FPartyJoinDenialReason(EPartyJoinDenialReason::TargetUserBlocked);
	}
	else
	{
		TSharedPtr<const IOnlinePartyJoinInfo> JoinInfo = UserToJoin.GetPartyJoinInfo(PartyTypeId);
		if (JoinInfo.IsValid())
		{
			if (!JoinInfo->IsValid())
			{
				return EJoinPartyCompletionResult::JoinInfoInvalid;
			}
			else if (!JoinInfo->IsAcceptingMembers())
			{
				FPartyJoinDenialReason DenialReason = JoinInfo->GetNotAcceptingReason();
				if (DenialReason.GetReason() != EPartyJoinDenialReason::PartyPrivate || !UserToJoin.HasSentPartyInvite(PartyTypeId))
				{
					return DenialReason;
				}
			}
		}
		else if (UserToJoin.IsFriend(ESocialSubsystem::Platform))
		{
			ECrossplayPreference Preference = GetCrossplayPreference();
			if (UserToJoin.GetCurrentPlatform().IsCrossplayWithLocalPlatform() && OptedOutOfCrossplay(Preference))
			{
				return FPartyJoinDenialReason(EPartyJoinDenialReason::JoinerCrossplayRestricted);
			}
			if (const FOnlineUserPresence* PlatformPresence = UserToJoin.GetFriendPresenceInfo(ESocialSubsystem::Platform))
			{
				if (!PlatformPresence->bIsPlayingThisGame)
				{
					return FPartyJoinDenialReason(EPartyJoinDenialReason::TargetUserPlayingDifferentGame);
				}
				else if (!UserToJoin.HasSentPartyInvite(PartyTypeId))
				{
					if (!PlatformPresence->bIsJoinable)
					{
						return FPartyJoinDenialReason(EPartyJoinDenialReason::TargetUserUnjoinable);
					}
					else if (!PlatformPresence->SessionId.IsValid() || !PlatformPresence->SessionId->IsValid())
					{
						return FPartyJoinDenialReason(EPartyJoinDenialReason::TargetUserMissingPlatformSession);
					}
				}
			}
			else
			{
				return FPartyJoinDenialReason(EPartyJoinDenialReason::TargetUserMissingPresence);
			}
		}
		else
		{
			// We've got no info on this party for the given user, so it's gotta be private (or doesn't even exist)
			return FPartyJoinDenialReason(EPartyJoinDenialReason::PartyPrivate);
		}
	}

	return FJoinPartyResult();
}

void USocialManager::JoinParty(const USocialUser& UserToJoin, const FOnlinePartyTypeId& PartyTypeId, const FOnJoinPartyAttemptComplete& OnJoinPartyComplete)
{
	UE_LOG(LogParty, Verbose, TEXT("Attempting to join user [%s]'s party of type [%d]"), *UserToJoin.ToDebugString(), PartyTypeId.GetValue());

	FJoinPartyAttempt NewAttempt(&UserToJoin, PartyTypeId, OnJoinPartyComplete);
	const FJoinPartyResult ValidationResult = ValidateJoinTarget(UserToJoin, PartyTypeId);
	if (ValidationResult.WasSuccessful())
	{
		FJoinPartyAttempt& JoinAttempt = JoinAttemptsByTypeId.Add(PartyTypeId, NewAttempt);

		JoinAttempt.JoinInfo = UserToJoin.GetPartyJoinInfo(PartyTypeId);
		if (JoinAttempt.JoinInfo.IsValid())
		{
			QueryPartyJoinabilityInternal(JoinAttempt);
		}
		else
		{
			JoinAttempt.ActionTimeTracker.BeginStep(FJoinPartyAttempt::Step_FindPlatformSession);
			PartySessionManager->FindSession(UserToJoin, FPartyPlatformSessionManager::FOnFindSessionAttemptComplete::CreateUObject(this, &USocialManager::HandleFindSessionForJoinComplete, PartyTypeId));
		}
	}
	else
	{
		// We don't do the standard FinishJoinAttempt here because this entry isn't actually in our map of join attempts yet
		// It's possible that this attempt failed immediately because a join is already in progress, in which case we don't want to nuke the legitimate attempt with the same ID
		OnJoinPartyAttemptCompleteInternal(NewAttempt, ValidationResult);
		NewAttempt.OnJoinComplete.ExecuteIfBound(ValidationResult);
	}
}

void USocialManager::NotifyPartyInitialized(USocialParty& Party)
{
	//only make the outside modules aware of party after initialization is complete
	OnPartyJoined().Broadcast(Party);
}

bool USocialManager::IsPartyJoinInProgress(const FOnlinePartyTypeId& TypeId) const
{
	return JoinAttemptsByTypeId.Contains(TypeId);
}

bool USocialManager::IsPersistentPartyJoinInProgress() const
{
	return IsPartyJoinInProgress(IOnlinePartySystem::GetPrimaryPartyTypeId());
}

void USocialManager::FillOutJoinRequestData(const FOnlinePartyId& TargetParty, FOnlinePartyData& OutJoinRequestData) const
{
	ECrossplayPreference Preference = GetCrossplayPreference();
	if (Preference != ECrossplayPreference::NoSelection)
	{
		FVariantData CrossplayPreferenceVal;
		CrossplayPreferenceVal.SetValue((int32)Preference);
		OutJoinRequestData.SetAttribute(TEXT("CrossplayPreference"), CrossplayPreferenceVal);
	}
}

TSubclassOf<USocialParty> USocialManager::GetPartyClassForType(const FOnlinePartyTypeId& PartyTypeId) const
{
	return USocialParty::StaticClass();
}

void USocialManager::OnJoinPartyAttemptCompleteInternal(const FJoinPartyAttempt& JoinAttemptInfo, const FJoinPartyResult& Result)
{
	UE_LOG(LogParty, Verbose, TEXT("JoinPartyAttempt [%s] completed with result [%s] and reason [%s]"), *JoinAttemptInfo.ToDebugString(), ToString(Result.GetResult()), ToString(Result.GetDenialReason().GetReason()));
}

void USocialManager::OnToolkitCreatedInternal(USocialToolkit& NewToolkit)
{
	OnSocialToolkitCreated().Broadcast(NewToolkit);
}

bool USocialManager::CanCreateNewPartyObjects() const
{
	// At the root level, we just want to be sure that we have a world before spinning up party UObjects
	return GetWorld() != nullptr;
}

ECrossplayPreference USocialManager::GetCrossplayPreference() const
{
	return ECrossplayPreference::NoSelection;
}

bool USocialManager::ShouldTryRejoiningPersistentParty(const FRejoinableParty& InRejoinableParty) const
{
	// If we're alone in our persistent party or we don't have one at the moment, go for it (games will likely have more opinions on the matter)
	USocialParty* PersistentParty = GetPersistentParty();
 	return (!PersistentParty || PersistentParty->GetNumPartyMembers() == 1) && !JoinAttemptsByTypeId.Contains(IOnlinePartySystem::GetPrimaryPartyTypeId());
}

void USocialManager::RefreshCanCreatePartyObjects()
{
	const bool bCanNowCreate = CanCreateNewPartyObjects();
	if (bCanNowCreate != bCanCreatePartyObjects)
	{
		bCanCreatePartyObjects = bCanNowCreate;
		if (bCanNowCreate && JoinAttemptsByTypeId.Num() > 0)
		{
			// We'll potentially be removing map entries mid-loop, just work with a copy
			auto JoinAttemptsCopy = JoinAttemptsByTypeId;
			for (auto& TypeIdJoinAttemptPair : JoinAttemptsCopy)
			{
				FJoinPartyAttempt& JoinAttempt = TypeIdJoinAttemptPair.Value;
				if (JoinAttempt.ActionTimeTracker.GetCurrentStepName() == FJoinPartyAttempt::Step_DeferredPartyCreation && ensure(JoinAttempt.JoinInfo.IsValid()))
				{
					JoinAttempt.ActionTimeTracker.CompleteStep(FJoinPartyAttempt::Step_DeferredPartyCreation);
					FJoinPartyResult JoinResult = EJoinPartyCompletionResult::Succeeded;
					if (!EstablishNewParty(*GetFirstLocalUserId(ESocialSubsystem::Primary), *JoinAttempt.JoinInfo->GetPartyId(), JoinAttempt.JoinInfo->GetPartyTypeId()))
					{
						JoinResult = EJoinPartyCompletionResult::UnknownClientFailure;
					}
					FinishJoinPartyAttempt(JoinAttempt, JoinResult);
				}
			}
		}
	}
}

UGameInstance& USocialManager::GetGameInstance() const
{
	return *GetTypedOuter<UGameInstance>();
}

USocialToolkit& USocialManager::CreateSocialToolkit(ULocalPlayer& OwningLocalPlayer)
{
	for (USocialToolkit* ExistingToolkit : SocialToolkits)
	{
		check(&OwningLocalPlayer != &ExistingToolkit->GetOwningLocalPlayer());
	}
	check(ToolkitClass);

	USocialToolkit* NewToolkit = NewObject<USocialToolkit>(this, ToolkitClass);
	SocialToolkits.Add(NewToolkit);
	NewToolkit->InitializeToolkit(OwningLocalPlayer);
	OnToolkitCreatedInternal(*NewToolkit);
	return *NewToolkit;
}

void USocialManager::QueryPartyJoinabilityInternal(FJoinPartyAttempt& JoinAttempt)
{
	FUniqueNetIdRepl LocalUserId = GetFirstLocalUserId(ESocialSubsystem::Primary);
	if (ensure(LocalUserId.IsValid()) && ensure(JoinAttempt.JoinInfo.IsValid()))
	{
		JoinAttempt.ActionTimeTracker.BeginStep(FJoinPartyAttempt::Step_QueryJoinability);

		IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());
		PartyInterface->QueryPartyJoinability(*LocalUserId, *JoinAttempt.JoinInfo, FOnQueryPartyJoinabilityComplete::CreateUObject(this, &USocialManager::HandleQueryJoinabilityComplete, JoinAttempt.JoinInfo->GetPartyTypeId()));
	}
	else
	{
		FinishJoinPartyAttempt(JoinAttempt, FJoinPartyResult(EJoinPartyCompletionResult::UnknownClientFailure));
	}
}

void USocialManager::JoinPartyInternal(FJoinPartyAttempt& JoinAttempt)
{
	IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());
	FUniqueNetIdRepl LocalUserId = GetFirstLocalUserId(ESocialSubsystem::Primary);
	check(LocalUserId.IsValid());

	JoinAttempt.ActionTimeTracker.BeginStep(FJoinPartyAttempt::Step_JoinParty);

	if (JoinAttempt.RejoinInfo.IsValid())
	{
		UE_LOG(LogParty, Verbose, TEXT("Attempting to rejoin party [%s] now."), *JoinAttempt.RejoinInfo->PartyId->ToDebugString());

		// Rejoin attempts are initiated differently, but the handler/follow-up is identical to a normal join
		PartyInterface->RejoinParty(*GetFirstLocalUserId(ESocialSubsystem::Primary), *JoinAttempt.RejoinInfo->PartyId, IOnlinePartySystem::GetPrimaryPartyTypeId(), JoinAttempt.RejoinInfo->MemberIds, FOnJoinPartyComplete::CreateUObject(this, &USocialManager::HandleJoinPartyComplete, IOnlinePartySystem::GetPrimaryPartyTypeId()));
	}
	else
	{
		PartyInterface->JoinParty(*LocalUserId, *JoinAttempt.JoinInfo, FOnJoinPartyComplete::CreateUObject(this, &USocialManager::HandleJoinPartyComplete, JoinAttempt.JoinInfo->GetPartyTypeId()));
	}
}

void USocialManager::FinishJoinPartyAttempt(FJoinPartyAttempt& JoinAttemptToDestroy, const FJoinPartyResult& JoinResult)
{
	OnJoinPartyAttemptCompleteInternal(JoinAttemptToDestroy, JoinResult);
	JoinAttemptToDestroy.OnJoinComplete.ExecuteIfBound(JoinResult);

	const bool bWasPersistentPartyJoinAttempt = JoinAttemptToDestroy.PartyTypeId == IOnlinePartySystem::GetPrimaryPartyTypeId();

	// JoinAttemptToDestroy is garbage after this. Be careful!
	JoinAttemptsByTypeId.Remove(JoinAttemptToDestroy.PartyTypeId);

	if (bWasPersistentPartyJoinAttempt && !JoinResult.WasSuccessful() && !GetPersistentParty())
	{
		// Something goofed when trying to join a new persistent party, so create a replacement immediately
		CreatePersistentParty();
	}
}

USocialParty* USocialManager::GetPersistentPartyInternal(bool bEvenIfLeaving /*= false*/) const
{
	USocialParty* const* PersistentParty = JoinedPartiesByTypeId.Find(IOnlinePartySystem::GetPrimaryPartyTypeId());
	if (PersistentParty && ensure(*PersistentParty) && (bEvenIfLeaving || !(*PersistentParty)->IsLeavingParty()))
	{
		return *PersistentParty;
	}
	return nullptr;
}

const USocialManager::FJoinPartyAttempt* USocialManager::GetJoinAttemptInProgress(const FOnlinePartyTypeId& PartyTypeId) const
{
	return JoinAttemptsByTypeId.Find(PartyTypeId);
}

USocialParty* USocialManager::EstablishNewParty(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FOnlinePartyTypeId& PartyTypeId)
{
	IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());
	
	TSubclassOf<USocialParty> PartyClass = GetPartyClassForType(PartyTypeId);
	TSharedPtr<const FOnlineParty> OssParty = PartyInterface->GetParty(LocalUserId, PartyTypeId);
	if (ensure(OssParty.IsValid()) && ensure(PartyClass) && ensure(*OssParty->PartyId == PartyId))
	{
		USocialParty* NewParty = NewObject<USocialParty>(this, *PartyClass);
		NewParty->OnPartyLeaveBegin().AddUObject(this, &USocialManager::HandlePartyLeaveBegin, NewParty);
		NewParty->OnPartyLeft().AddUObject(this, &USocialManager::HandlePartyLeft, NewParty);

		// This must be done before InitializeParty(), as initialization can complete synchronously.
		JoinedPartiesByTypeId.Add(PartyTypeId, NewParty);

		NewParty->InitializeParty(OssParty.ToSharedRef());

		if (NewParty->IsPersistentParty())
		{
			NewParty->OnPartyStateChanged().AddUObject(this, &USocialManager::HandlePersistentPartyStateChanged, NewParty);
			HandlePersistentPartyStateChanged(NewParty->GetOssPartyState(), NewParty);
		}

		return NewParty;
	}

	return nullptr;
}

USocialParty* USocialManager::GetPartyInternal(const FOnlinePartyTypeId& PartyTypeId, bool bIncludeLeavingParties) const
{
	USocialParty* const* Party = JoinedPartiesByTypeId.Find(PartyTypeId);
	if (!Party && bIncludeLeavingParties)
	{
		Party = LeavingPartiesByTypeId.Find(PartyTypeId);
	}
	return Party ? *Party : nullptr;
}

USocialParty* USocialManager::GetPartyInternal(const FOnlinePartyId& PartyId, bool bIncludeLeavingParties) const
{
	for (auto& TypeIdPartyPair : JoinedPartiesByTypeId)
	{
		if (TypeIdPartyPair.Value->GetPartyId() == PartyId)
		{
			return TypeIdPartyPair.Value;
		}
	}
	if (bIncludeLeavingParties)
	{
		for (auto& TypeIdPartyPair : LeavingPartiesByTypeId)
		{
			if (TypeIdPartyPair.Value->GetPartyId() == PartyId)
			{
				return TypeIdPartyPair.Value;
			}
		}
	}
	
	return nullptr;
}

TSharedPtr<IOnlinePartyJoinInfo> USocialManager::GetJoinInfoFromSession(const FOnlineSessionSearchResult& PlatformSession)
{
	static const FName JoinInfoSettingName = PLATFORM_XBOXONE ? SETTING_CUSTOM_JOIN_INFO : SETTING_CUSTOM;

	FString JoinInfoJson;
	if (PlatformSession.Session.SessionSettings.Get(JoinInfoSettingName, JoinInfoJson))
	{
#if PLATFORM_XBOXONE
		// On Xbox we encode our party data in base64 to avoid XboxLive trying to parse our json, so now we need to decode that
		FBase64::Decode(JoinInfoJson, JoinInfoJson);
#endif 
		IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());
		return PartyInterface->MakeJoinInfoFromJson(JoinInfoJson);
	}
	return nullptr;
}

void USocialManager::HandleGameViewportInitialized()
{
	UGameViewportClient::OnViewportCreated().RemoveAll(this);

	UGameInstance& GameInstance = GetGameInstance();
	UGameViewportClient* GameViewport = GameInstance.GetGameViewportClient();
	check(GameViewport);

	GameViewport->OnPlayerAdded().AddUObject(this, &USocialManager::HandleLocalPlayerAdded);
	GameViewport->OnPlayerRemoved().AddUObject(this, &USocialManager::HandleLocalPlayerRemoved);

	// Immediately spin up toolkits for local players that already exist
	for (ULocalPlayer* ExistingLocalPlayer : GameInstance.GetLocalPlayers())
	{
		CreateSocialToolkit(*ExistingLocalPlayer);
	}
}

void USocialManager::HandlePreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
	RefreshCanCreatePartyObjects();
}

void USocialManager::HandleWorldEstablished(UWorld* World)
{
	RefreshCanCreatePartyObjects();

	if (!OnFillJoinRequestInfoHandle.IsValid())
	{
		IOnlinePartyPtr PartyInterface = Online::GetPartyInterface(World);
		if (PartyInterface.IsValid())
		{
			OnFillJoinRequestInfoHandle = PartyInterface->AddOnFillPartyJoinRequestDataDelegate_Handle(FOnFillPartyJoinRequestDataDelegate::CreateUObject(this, &USocialManager::HandleFillPartyJoinRequestData));
		}
	}
}

void USocialManager::HandleLocalPlayerAdded(int32 LocalUserNum)
{
	ULocalPlayer* NewLocalPlayer = GetGameInstance().FindLocalPlayerFromControllerId(LocalUserNum);
	check(NewLocalPlayer);

	CreateSocialToolkit(*NewLocalPlayer);
}

void USocialManager::HandleLocalPlayerRemoved(int32 LocalUserNum)
{
	if (USocialToolkit* Toolkit = GetSocialToolkit(LocalUserNum))
	{
		SocialToolkits.Remove(Toolkit);
		Toolkit->MarkPendingKill();
	}
}

void USocialManager::HandleQueryJoinabilityComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, EJoinPartyCompletionResult Result, int32 NotApprovedReasonCode, FOnlinePartyTypeId PartyTypeId)
{
	if (FJoinPartyAttempt* JoinAttempt = JoinAttemptsByTypeId.Find(PartyTypeId))
	{
		if (Result == EJoinPartyCompletionResult::Succeeded)
		{
			if (USocialParty* ExistingParty = GetPartyInternal(PartyTypeId, true))
			{
				// We're currently in another party of the same type, so we have to leave that one first
				JoinAttempt->ActionTimeTracker.BeginStep(FJoinPartyAttempt::Step_LeaveCurrentParty);

				if (!ExistingParty->IsCurrentlyLeaving())
				{
					ExistingParty->LeaveParty(USocialParty::FOnLeavePartyAttemptComplete::CreateUObject(this, &USocialManager::HandleLeavePartyForJoinComplete, ExistingParty));
				}
			}
			else
			{
				JoinPartyInternal(*JoinAttempt);
			}
		}
		else
		{
			FinishJoinPartyAttempt(*JoinAttempt, FJoinPartyResult(Result, NotApprovedReasonCode));
		}
	}
}

void USocialManager::HandleCreatePartyComplete(const FUniqueNetId& LocalUserId, const TSharedPtr<const FOnlinePartyId>& PartyId, ECreatePartyCompletionResult Result, FOnlinePartyTypeId PartyTypeId, FOnCreatePartyAttemptComplete CompletionDelegate)
{
	ECreatePartyCompletionResult LocalCreationResult = Result;
	if (Result == ECreatePartyCompletionResult::Succeeded)
	{
		USocialParty* NewParty = EstablishNewParty(LocalUserId, *PartyId, PartyTypeId);
		if (!NewParty)
		{
			LocalCreationResult = ECreatePartyCompletionResult::UnknownClientFailure;
		}
	}

	UE_LOG(LogParty, Verbose, TEXT("Finished trying to create party [%s] with result [%s]"), PartyId.IsValid() ? *PartyId->ToDebugString() : TEXT("Invalid"), ToString(LocalCreationResult));
	CompletionDelegate.ExecuteIfBound(LocalCreationResult);
}

void USocialManager::HandleJoinPartyComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, EJoinPartyCompletionResult Result, int32 NotApprovedReasonCode, FOnlinePartyTypeId PartyTypeId)
{
	UE_LOG(LogParty, Log, TEXT("Attempt to join party of type [%d] completed with result [%s] and reason code [%d]"), PartyTypeId.GetValue(), ToString(Result), NotApprovedReasonCode);

	FJoinPartyResult JoinResult(Result, NotApprovedReasonCode);
	FJoinPartyAttempt* JoinAttempt = JoinAttemptsByTypeId.Find(PartyTypeId);
	if (ensure(JoinAttempt))
	{
		JoinAttempt->ActionTimeTracker.CompleteStep(FJoinPartyAttempt::Step_JoinParty);
		
		if (JoinResult.WasSuccessful())
		{
			if (bCanCreatePartyObjects)
			{
				USocialParty* NewParty = EstablishNewParty(LocalUserId, PartyId, PartyTypeId);
				if (!NewParty)
				{
					JoinResult.SetResult(EJoinPartyCompletionResult::UnknownClientFailure);
				}
				FinishJoinPartyAttempt(*JoinAttempt, JoinResult);
			}
			else
			{
				// Not currently in an ok state to be creating new party objects (between maps or something) - update the join attempt and revisit when we're cleared to create party objects again
				JoinAttempt->ActionTimeTracker.BeginStep(FJoinPartyAttempt::Step_DeferredPartyCreation);
			}
		}
		else
		{
			FinishJoinPartyAttempt(*JoinAttempt, JoinResult);
		}
	}
	else
	{
		//@note DanH: Should be quite impossible, but happening in the wild without repro steps (FORT-123031) - putting in lots of mines here to make sure we see it if it happens in-house
		UE_LOG(LogParty, Error, TEXT("Attempt to join party of type [%d] completed with result [%s], but there is no existing FJoinPartyAttempt object."), PartyTypeId.GetValue(), ToString(Result));
		if (!ensure(!JoinResult.WasSuccessful()))
		{
			UE_LOG(LogParty, Error, TEXT("Auto-bailing on party of type [%d] - cannot finish establishing it without a valid FJoinPartyAttempt."), PartyTypeId.GetValue(), ToString(Result));
			IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());
			PartyInterface->LeaveParty(LocalUserId, PartyId, FOnLeavePartyComplete::CreateUObject(this, &USocialManager::HandleLeavePartyForMissingJoinAttempt, PartyTypeId));
		}
		else
		{
			// Failed to join this party in the first place - skip to the leave complete handler and to do any necessary fixup (since we were still missing the join attempt, and that's far less than ideal)
			HandleLeavePartyForMissingJoinAttempt(LocalUserId, PartyId, ELeavePartyCompletionResult::Succeeded, PartyTypeId);
		}
	}
}

void USocialManager::HandlePersistentPartyStateChanged(EPartyState NewState, USocialParty* PersistentParty)
{
	UE_LOG(LogParty, Verbose, TEXT("Persistent party state changed to %s"), ToString(NewState));

	if (NewState == EPartyState::Disconnected)
	{
		bIsConnectedToPartyService = false;

		// If we have other members in our party, then we will try to rejoin this when we come back online
		if (!RejoinableParty.IsValid() && PersistentParty->ShouldCacheForRejoinOnDisconnect())
		{
			UE_LOG(LogParty, Log, TEXT("Caching persistent party [%s] for rejoin"), *PersistentParty->GetPartyId().ToDebugString());
			RejoinableParty = MakeShared<FRejoinableParty>(*PersistentParty);
		}
	}
	else if (NewState == EPartyState::Active)
	{
		bIsConnectedToPartyService = true;

		if (RejoinableParty.IsValid())
		{
			if (ShouldTryRejoiningPersistentParty(*RejoinableParty))
			{
				// Bail on the current party, we'll try to rejoin once we've left
				UE_LOG(LogParty, Log, TEXT("Leaving current persistent party [%s] to attempt to rejoin previous party [%s]"), *PersistentParty->ToDebugString(), *RejoinableParty->PartyId->ToDebugString());

				FJoinPartyAttempt& RejoinAttempt = JoinAttemptsByTypeId.Add(IOnlinePartySystem::GetPrimaryPartyTypeId(), FJoinPartyAttempt(RejoinableParty.ToSharedRef()));
				RejoinAttempt.ActionTimeTracker.BeginStep(FJoinPartyAttempt::Step_LeaveCurrentParty);

				PersistentParty->LeaveParty();
			}

			// This is the only time we would try to rejoin, and it's saved on the join attempt if initiated
			RejoinableParty.Reset();
		}
	}
}

void USocialManager::HandleLeavePartyForJoinComplete(ELeavePartyCompletionResult LeaveResult, USocialParty* LeftParty)
{
	UE_LOG(LogParty, Verbose, TEXT("Attempt to leave party [%s] for pending join completed with result [%s]"), *LeftParty->ToDebugString(), ToString(LeaveResult));
}

void USocialManager::HandlePartyLeaveBegin(EMemberExitedReason Reason, USocialParty* LeavingParty)
{
	const FOnlinePartyTypeId& PartyTypeId = LeavingParty->GetPartyTypeId();
	JoinedPartiesByTypeId.Remove(PartyTypeId);
	LeavingPartiesByTypeId.Add(PartyTypeId, LeavingParty);
}

void USocialManager::HandlePartyLeft(EMemberExitedReason Reason, USocialParty* LeftParty)
{
	const FOnlinePartyTypeId& PartyTypeId = LeftParty->GetPartyTypeId();
	LeavingPartiesByTypeId.Remove(PartyTypeId);

	if (!ensure(!JoinedPartiesByTypeId.Contains(PartyTypeId)))
	{
		// Really shouldn't be any scenario wherein we receive a PartyLeft event without a prior PartyLeaveBegin
		JoinedPartiesByTypeId.Remove(PartyTypeId);
	}

	OnPartyLeftInternal(*LeftParty, Reason);
	LeftParty->MarkPendingKill();

	if (FJoinPartyAttempt* JoinAttempt = JoinAttemptsByTypeId.Find(PartyTypeId))
	{
		JoinAttempt->ActionTimeTracker.CompleteStep(FJoinPartyAttempt::Step_LeaveCurrentParty);

		// We're in the process of joining another party of the same type - do we know where we're heading yet?
		if (JoinAttempt->JoinInfo.IsValid() || JoinAttempt->RejoinInfo.IsValid())
		{
			// Join the new party immediately and early out
			JoinPartyInternal(*JoinAttempt);
			return;
		}
		else
		{
			// An attempt to join a party of this type has been initiated, but something/someone decided to leave the party before the attempt was ready to do so
			// It's not worth accounting for the potential limbo that this could put us into, so just abort the join attempt and let the explicit leave action win
			UE_LOG(LogParty, Verbose, TEXT("Finished leaving party [%s] before the current join attempt established join info. Cancelling join attempt."), *LeftParty->ToDebugString());
			FinishJoinPartyAttempt(*JoinAttempt, FJoinPartyResult(EPartyJoinDenialReason::JoinAttemptAborted));
		}
	}

	if (LeftParty->IsPersistentParty() && GetFirstLocalUserToolkit()->IsOwnerLoggedIn())
	{
		UE_LOG(LogParty, Verbose, TEXT("Finished leaving persistent party without a join/rejoin target. Creating a new persistent party now."));

		// This wasn't part of a join process, so immediately create a new persistent party
		CreatePersistentParty();
	}
}

void USocialManager::HandleLeavePartyForMissingJoinAttempt(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, ELeavePartyCompletionResult LeaveResult, FOnlinePartyTypeId PartyTypeId)
{
	if (PartyTypeId == IOnlinePartySystem::GetPrimaryPartyTypeId() && GetFirstLocalUserToolkit()->IsOwnerLoggedIn() && !GetPersistentPartyInternal(true))
	{
		// We just had to bail on the persistent party due to unforeseen shenanigans, so try to correct things and set another one back up
		CreatePersistentParty();
	}
}

void USocialManager::HandleFillPartyJoinRequestData(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, FOnlinePartyData& PartyData)
{
	FillOutJoinRequestData(PartyId, PartyData);
}

void USocialManager::HandleFindSessionForJoinComplete(bool bWasSuccessful, const FOnlineSessionSearchResult& FoundSession, FOnlinePartyTypeId PartyTypeId)
{
	if (FJoinPartyAttempt* JoinAttempt = JoinAttemptsByTypeId.Find(PartyTypeId))
	{
		JoinAttempt->ActionTimeTracker.CompleteStep(FJoinPartyAttempt::Step_FindPlatformSession);

		if (bWasSuccessful)
		{
			JoinAttempt->JoinInfo = GetJoinInfoFromSession(FoundSession);
			if (JoinAttempt->JoinInfo.IsValid())
			{
				QueryPartyJoinabilityInternal(*JoinAttempt);
			}
			else
			{
				FinishJoinPartyAttempt(*JoinAttempt, FJoinPartyResult(EPartyJoinDenialReason::PlatformSessionMissingJoinInfo));
			}
		}
		else
		{
			FinishJoinPartyAttempt(*JoinAttempt, FJoinPartyResult(EPartyJoinDenialReason::TargetUserMissingPlatformSession));
		}
	}
}