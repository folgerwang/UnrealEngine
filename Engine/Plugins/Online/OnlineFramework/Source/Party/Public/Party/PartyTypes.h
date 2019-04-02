// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../SocialTypes.h"

#include "PartyTypes.generated.h"

class USocialParty;
class UPartyMember;

class IOnlinePartySystem;
class FOnlinePartyData;

enum class EJoinPartyCompletionResult;

UENUM()
enum class EPartyType : uint8
{
	/** This party is public (not really supported right now) */
	Public,
	/** This party is joinable by friends */
	FriendsOnly,
	/** This party requires an invite from someone within the party */
	Private
};

UENUM()
enum class EPartyInviteRestriction : uint8
{
	/** Any party member can send invites */
	AnyMember,
	/** Only the leader can send invites */
	LeaderOnly,
	/** Nobody can invite anyone to this party */
	NoInvites
};

UENUM()
enum class EPartyJoinDenialReason : uint8
{
	/** Framework-level denial reasons */

	/** No denial, matches success internally */
	NoReason = 0,
	/** The local player aborted the join attempt */
	JoinAttemptAborted,
	/** Party leader is busy or at inopportune time to allow joins - to be used as a fallback when there isn't a more specific reason (more specific reasons are preferred) */
	Busy,
	/** Either the necessary OSS itself or critical element thereof (PartyInterface, SessionInterface, etc.) is missing. */
	OssUnavailable,
	/** Party is full */
	PartyFull,
	/** Game is full, but not party */
	GameFull,
	/** Asked a non party leader to join game, shouldn't happen */
	NotPartyLeader,
	/** Party has been marked as private and the join request is revoked */
	PartyPrivate,
	/** Player has crossplay restriction that would be violated */
	JoinerCrossplayRestricted,
	/** Party member has crossplay restriction that would be violated */
	MemberCrossplayRestricted,
	/** Player is in a game mode that restricts joining */
	GameModeRestricted,
	/** Player is currently banned */
	Banned,
	/** Player is not yet logged in */
	NotLoggedIn,
	/** Unable to start joining - we are checking for a session to rejoin */
	CheckingForRejoin,
	/** The target user is missing presence info */
	TargetUserMissingPresence,
	/** The target user's presence says the user is unjoinable */
	TargetUserUnjoinable,
	/** The target user is currently Away */
	TargetUserAway,
	/** We found ourself to be the leader of the friend's party according to the console session */
	AlreadyLeaderInPlatformSession,
	/** The target user is not playing the same game as us */
	TargetUserPlayingDifferentGame,
	/** The target user's presence does not have any information about their party session (platform friends only) */
	TargetUserMissingPlatformSession,
	/** There is no party join info available in the target user's platform session */
	PlatformSessionMissingJoinInfo,
	/** We were unable to launch the query to find the platform friend's session (platform friends only) */
	FailedToStartFindConsoleSession,
	/** The party is of a type that the game does not support (it specified nullptr for the USocialParty class) */
	MissingPartyClassForTypeId,
	/** The target user is blocked by the local user on one or more of the active subsystems */
	TargetUserBlocked,

	/**
	* Customizable denial reasons.
	* Expected usage is to assign the entries in the custom enum to the arbitrary custom entry placeholders below.
	* App level users of the system can then cast to/from their custom enum as desired.
	*/
	CustomReason0,
	CustomReason1,
	CustomReason2,
	CustomReason3,
	CustomReason4,
	CustomReason5,
	CustomReason6,
	CustomReason7,
	CustomReason8,
	CustomReason9,
	CustomReason10,
	CustomReason11,
	CustomReason12,
	CustomReason13,
	CustomReason14,
	CustomReason15,
	CustomReason16,
	CustomReason17,
	CustomReason18,
	CustomReason19,
	CustomReason20,
	CustomReason21,
	CustomReason22,
	CustomReason23,
	CustomReason24,
	CustomReason25,
	CustomReason26,
	CustomReason27,
	CustomReason28,
	CustomReason29,
	CustomReason30,
	CustomReason31,
	CustomReason32,
	CustomReason33,
	CustomReason34,
	CustomReason35,
	CustomReason36,
	CustomReason37,
	CustomReason38,
	CustomReason39,

	MAX
};

inline const TCHAR* ToString(EPartyJoinDenialReason Type)
{
	switch (Type)
	{
	case EPartyJoinDenialReason::NoReason:
		return TEXT("NoReason");
		break;
	case EPartyJoinDenialReason::JoinAttemptAborted:
		return TEXT("JoinAttemptAborted");
		break;
	case EPartyJoinDenialReason::Busy:
		return TEXT("Busy");
		break;
	case EPartyJoinDenialReason::OssUnavailable:
		return TEXT("OssUnavailable");
		break;
	case EPartyJoinDenialReason::PartyFull:
		return TEXT("PartyFull");
		break;
	case EPartyJoinDenialReason::GameFull:
		return TEXT("GameFull");
		break;
	case EPartyJoinDenialReason::NotPartyLeader:
		return TEXT("NotPartyLeader");
		break;
	case EPartyJoinDenialReason::PartyPrivate:
		return TEXT("PartyPrivate");
		break;
	case EPartyJoinDenialReason::JoinerCrossplayRestricted:
		return TEXT("JoinerCrossplayRestricted");
		break;
	case EPartyJoinDenialReason::MemberCrossplayRestricted:
		return TEXT("MemberCrossplayRestricted");
		break;
	case EPartyJoinDenialReason::GameModeRestricted:
		return TEXT("GameModeRestricted");
		break;
	case EPartyJoinDenialReason::Banned:
		return TEXT("Banned");
		break;
	case EPartyJoinDenialReason::NotLoggedIn:
		return TEXT("NotLoggedIn");
		break;
	case EPartyJoinDenialReason::CheckingForRejoin:
		return TEXT("CheckingForRejoin");
		break;
	case EPartyJoinDenialReason::TargetUserMissingPresence:
		return TEXT("TargetUserMissingPresence");
		break;
	case EPartyJoinDenialReason::TargetUserUnjoinable:
		return TEXT("TargetUserUnjoinable");
		break;
	case EPartyJoinDenialReason::AlreadyLeaderInPlatformSession:
		return TEXT("AlreadyLeaderInPlatformSession");
		break;
	case EPartyJoinDenialReason::TargetUserPlayingDifferentGame:
		return TEXT("TargetUserPlayingDifferentGame");
		break;
	case EPartyJoinDenialReason::TargetUserMissingPlatformSession:
		return TEXT("TargetUserMissingPlatformSession");
		break;
	case EPartyJoinDenialReason::PlatformSessionMissingJoinInfo:
		return TEXT("PlatformSessionMissingJoinInfo");
		break;
	case EPartyJoinDenialReason::FailedToStartFindConsoleSession:
		return TEXT("FailedToStartFindConsoleSession");
		break;
	case EPartyJoinDenialReason::MissingPartyClassForTypeId:
		return TEXT("MissingPartyClassForTypeId");
		break;
	default:
		return TEXT("CustomReason");
		break;
	}
}

UENUM()
enum class EApprovalAction : uint8
{
	Approve,
	Enqueue,
	EnqueueAndStartBeacon,
	Deny
};

// Gives a smidge more meaning to the intended use for the string. These should just be UniqueId's (and are), but not reliably allocated as shared ptrs, so they cannot be replicated via FUniqueNetIdRepl.
using FSessionId = FString;

USTRUCT()
struct FPartyPlatformSessionInfo
{
	GENERATED_BODY();

public:
	/** The name of the OSS the platform session is for (because in a crossplay party, members can be part of different platform OSS') */
	UPROPERTY()
	FName OssName;

	/** The platform session id. Will be unset if it is not yet available to be joined. */
	UPROPERTY()
	FString SessionId;

	/** Primary OSS ID of the player that owns this console session */
	UPROPERTY()
	FUniqueNetIdRepl OwnerPrimaryId;

	void operator=(const FPartyPlatformSessionInfo& Other);
	bool operator==(FName PlatformOssName) const;
	bool operator==(const FPartyPlatformSessionInfo& Other) const;
	bool operator!=(const FPartyPlatformSessionInfo& Other) const { return !(*this == Other); }

	FString ToDebugString() const;
	bool IsSessionOwner(const UPartyMember& PartyMember) const;
	bool IsInSession(const UPartyMember& PartyMember) const;
};

USTRUCT()
struct FPartyPrivacySettings
{
	GENERATED_BODY()

public:
	/** The type of party in terms of advertised joinability restrictions */
	UPROPERTY()
	EPartyType PartyType = EPartyType::FriendsOnly;

	/** Who is allowed to send invitataions to the party? */
	UPROPERTY()
	EPartyInviteRestriction PartyInviteRestriction = EPartyInviteRestriction::AnyMember;

	/** True to restrict the party exclusively to friends of the party leader */
	UPROPERTY()
	bool bOnlyLeaderFriendsCanJoin = false;

	bool operator==(const FPartyPrivacySettings& Other) const;
	bool operator!=(const FPartyPrivacySettings& Other) const { return !operator==(Other); }

	FPartyPrivacySettings() {}
};


/** Companion to EPartyJoinDisapproval to lessen the hassle of working with a "customized" enum */
struct PARTY_API FPartyJoinDenialReason
{
public:
	FPartyJoinDenialReason() {}
	FPartyJoinDenialReason(int32 DenialReasonCode)
	{
		if (ensure(DenialReasonCode >= 0 && DenialReasonCode < (uint8)EPartyJoinDenialReason::MAX))
		{
			DenialReason = (EPartyJoinDenialReason)DenialReasonCode;
		}
	}

	template <typename CustomReasonEnumT>
	FPartyJoinDenialReason(CustomReasonEnumT CustomReason)
		: FPartyJoinDenialReason((int32)CustomReason)
	{
	}

	bool operator==(const FPartyJoinDenialReason& Other) const { return DenialReason == Other.DenialReason; }
	bool operator==(int32 ReasonCode) const { return (int32)DenialReason == ReasonCode; }

	template <typename CustomReasonEnumT>
	bool operator==(CustomReasonEnumT CustomReason) const { return (uint8)DenialReason == (uint8)CustomReason; }

	operator int32() const { return (int32)DenialReason; }

	bool HasAnyReason() const { return DenialReason != EPartyJoinDenialReason::NoReason; }
	bool HasCustomReason() const { return DenialReason >= EPartyJoinDenialReason::CustomReason0; }
	EPartyJoinDenialReason GetReason() const { return DenialReason; }

	template <typename CustomReasonEnumT>
	CustomReasonEnumT GetCustomReason() const
	{
		checkf(HasCustomReason(), TEXT("Cannot convert a non-custom disapproval reason to a custom one. Always check HasCustomReason() first."));
		return static_cast<CustomReasonEnumT>((uint8)DenialReason);
	}

	template <typename CustomReasonEnumT>
	const TCHAR* AsString() const
	{
		if (HasCustomReason())
		{
			return ToString(GetCustomReason<CustomReasonEnumT>());
		}
		return ToString(DenialReason);
	}

private:
	EPartyJoinDenialReason DenialReason = EPartyJoinDenialReason::NoReason;
};

struct PARTY_API FPartyJoinApproval
{
	FPartyJoinApproval() {}
	
	void SetDenialReason(FPartyJoinDenialReason InDenialReason)
	{
		DenialReason = InDenialReason;
		if (DenialReason.HasAnyReason())
		{
			ApprovalAction = EApprovalAction::Deny;
		}
	}

	void SetApprovalAction(EApprovalAction InApprovalAction)
	{
		ApprovalAction = InApprovalAction;
		if (ApprovalAction != EApprovalAction::Deny)
		{
			DenialReason = EPartyJoinDenialReason::NoReason;
		}
	}

	EApprovalAction GetApprovalAction() const { return ApprovalAction; }
	FPartyJoinDenialReason GetDenialReason() const { return DenialReason; }

	bool CanJoin() const { return ApprovalAction != EApprovalAction::Deny && ensure(!DenialReason.HasAnyReason()); }

private:
	FPartyJoinDenialReason DenialReason;
	EApprovalAction ApprovalAction = EApprovalAction::Approve;
};

struct PARTY_API FJoinPartyResult
{
public:
	FJoinPartyResult(); 
	FJoinPartyResult(FPartyJoinDenialReason InDenialReason);
	FJoinPartyResult(EJoinPartyCompletionResult InResult);
	FJoinPartyResult(EJoinPartyCompletionResult InResult, FPartyJoinDenialReason InDenialReason);
	
	void SetDenialReason(FPartyJoinDenialReason InDenialReason);
	void SetResult(EJoinPartyCompletionResult InResult);
	
	bool WasSuccessful() const;

	EJoinPartyCompletionResult GetResult() const { return Result; }
	FPartyJoinDenialReason GetDenialReason() const { return DenialReason; }

private:
	EJoinPartyCompletionResult Result;
	FPartyJoinDenialReason DenialReason;
};

/** Base for all rep data structs */
USTRUCT()
struct PARTY_API FOnlinePartyRepDataBase
{
	GENERATED_USTRUCT_BODY()
	virtual ~FOnlinePartyRepDataBase() {}

protected:
	/**
	 * Compare this data against the given old data, triggering delegates as appropriate.
	 * Intended to be used in concert with EXPOSE_REP_DATA_PROPERTY so that all you need to do is run through each property's CompareX function.
	 * If you need to do more complex comparison logic, you're welcome to do that as well/instead.
	 * @see FPartyRepData::CompareAgainst for an example
	 */
	virtual void CompareAgainst(const FOnlinePartyRepDataBase& OldData) const {}

	/**
	 * Called directly after an updated member state is received and copied into the local state
	 */
	virtual void PostReplication() {}
	virtual bool CanEditData() const { return false; }
	virtual const USocialParty* GetOwnerParty() const { return nullptr; }

	void LogPropertyChanged(const TCHAR* OwningStructTypeName, const TCHAR* ProperyName, bool bFromReplication) const;

	friend class FPartyDataReplicatorHelper;
	template <typename> friend class TPartyDataReplicator;
	FSimpleDelegate OnDataChanged;
};

//////////////////////////////////////////////////////////////////////////
// Boilerplate for exposing RepData properties
//////////////////////////////////////////////////////////////////////////

/** Simplest option - exposes getter and events, but no default setter */
#define EXPOSE_REP_DATA_PROPERTY_NO_SETTER(Owner, PropertyType, Property)	\
public:	\
	/** If the property is a POD or ptr type, we'll work with it by copy. Otherwise, by const ref */	\
	using Mutable##Property##Type = typename TRemoveConst<PropertyType>::Type;	\
	using Property##ArgType = typename TChooseClass<TOr<TIsPODType<PropertyType>, TIsPointer<PropertyType>>::Value, PropertyType, const Mutable##Property##Type&>::Result;	\
	\
private:	\
	/** Bummer to have two signatures, but cases that want both the old and new values are much rarer, so most don't want to bother with a handler that takes an extra unused param */	\
	DECLARE_MULTICAST_DELEGATE_OneParam(FOn##Property##Changed, Property##ArgType /*NewValue*/);	\
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOn##Property##ChangedDif, Property##ArgType /*NewValue*/, Property##ArgType /*OldValue*/);	\
public:	\
	/** Bind to receive the new property value only on changes */	\
	FOn##Property##Changed& On##Property##Changed() const { return On##Property##ChangedEvent; }	\
	/** Bind to receive both the new and old property value on changes */	\
	FOn##Property##ChangedDif& On##Property##ChangedDif() const { return On##Property##ChangedDifEvent; }	\
	\
	Property##ArgType Get##Property() const { return Property; }	\
	\
	bool Has##Property##InitiallyReplicated() const { return b##Property##InitiallyReplicated; }	\
private:	\
	void Compare##Property(const Owner& OldData) const	\
	{	\
		if(!b##Property##InitiallyReplicated) \
		{ \
			b##Property##InitiallyReplicated = true; \
		} \
		if (Property != OldData.Property)	\
		{	\
			LogPropertyChanged(TEXT(#Owner), TEXT(#Property), true);	\
			On##Property##ChangedDif().Broadcast(Property, OldData.Property);	\
			On##Property##Changed().Broadcast(Property);	\
		}	\
	}	\
	mutable FOn##Property##Changed On##Property##ChangedEvent;	\
	mutable FOn##Property##ChangedDif On##Property##ChangedDifEvent; \
	mutable bool b##Property##InitiallyReplicated

/**
 * Exposes a rep data property and provides a default property setter.
 * Awkwardly named util - don't bother using directly. Opt for EXPOSE_PRIVATE_REP_DATA_PROPERTY or EXPOSE_REP_DATA_PROPERTY
 */
#define EXPOSE_REP_DATA_PROPERTY_SETTER_ACCESS(Owner, PropertyType, Property, SetterPrivacy)	\
EXPOSE_REP_DATA_PROPERTY_NO_SETTER(Owner, PropertyType, Property);	\
SetterPrivacy:	\
	void Set##Property(Property##ArgType New##Property)	\
	{	\
		if (CanEditData() && Property != New##Property)	\
		{	\
			LogPropertyChanged(TEXT(#Owner), TEXT(#Property), false);	\
			if (On##Property##ChangedDif().IsBound())	\
			{	\
				PropertyType OldValue = Property;	\
				Property = New##Property;	\
				On##Property##ChangedDif().Broadcast(Property, OldValue);	\
			}	\
			else	\
			{	\
				Property = New##Property;	\
			}	\
			On##Property##Changed().Broadcast(Property);	\
			OnDataChanged.ExecuteIfBound();	\
		}	\
	}	\
private: //

/**
 * Exposes a rep data property with a private setter.
 * The setter only allows modification if the local user has authority to alter the property and will automatically trigger delegates appropriately.
 */
#define EXPOSE_PRIVATE_REP_DATA_PROPERTY(Owner, PropertyType, Property)	\
EXPOSE_REP_DATA_PROPERTY_SETTER_ACCESS(Owner, PropertyType, Property, private);

/**
 * Fully exposes the rep data property with a public setter.
 * The setter only allows modification if the local user has authority to alter the property and will automatically trigger delegates appropriately
 */
#define EXPOSE_REP_DATA_PROPERTY(Owner, PropertyType, Property)	\
EXPOSE_REP_DATA_PROPERTY_SETTER_ACCESS(Owner, PropertyType, Property, public);

inline const TCHAR* ToString(EPartyType Type)
{
	switch (Type)
	{
	case EPartyType::Public:
	{
		return TEXT("Public");
	}
	case EPartyType::FriendsOnly:
	{
		return TEXT("FriendsOnly");
	}
	case EPartyType::Private:
	{
		return TEXT("Private");
	}
	default:
	{
		return TEXT("Unknown");
	}
	}
}

inline const TCHAR* ToString(EApprovalAction Type)
{
	switch (Type)
	{
	case EApprovalAction::Approve:
	{
		return TEXT("Approve");
	}
	case EApprovalAction::Enqueue:
	{
		return TEXT("Enqueue");
	}
	case EApprovalAction::EnqueueAndStartBeacon:
	{
		return TEXT("EnqueueAndStartBeacon");
	}
	case EApprovalAction::Deny:
	{
		return TEXT("Deny");
	}
	default:
	{
		return TEXT("Unknown");
	}
	}
}
