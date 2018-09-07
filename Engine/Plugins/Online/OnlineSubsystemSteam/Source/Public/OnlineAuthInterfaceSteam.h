// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemSteam.h"
#include "OnlineSubsystemSteamTypes.h"
#include "OnlineSubsystemSteamPackage.h"

/** Steam Authentication Interface.
 *
 *  For the most part, this is fully automated. You simply just need to add the packet handler and your server will now
 *  require Steam Authentication for any incoming users. If a player fails to respond correctly, they will be kicked.
 */

/** When authentication has failed and we are about to take action on the user, this delegate is fired. 
 *	For the auth interface, overriding the delegate exposed in the class allows a game to override the default
 *	behavior, which is to kick anyone who fails authentication.
 *	
 *	If you would like to receive analytics as to the success/failure for users we can identify
 *	(have their unique net id), use the result delegate instead.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSteamAuthFailure, const class FUniqueNetId& /*FailedUserId*/);
typedef FOnSteamAuthFailure::FDelegate FOnSteamAuthFailureDelegate;

/** This delegate dictates the success or failure of an authentication result. 
 *  This means we got a result, but we won't be taking action yet.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSteamAuthResult, const class FUniqueNetId& /*UserId*/, bool /*bWasSuccess*/);
typedef FOnSteamAuthResult::FDelegate FOnSteamAuthResultDelegate;

enum class ESteamAuthStatus : uint8
{
	None = 0,
	AuthSuccess = 1 << 0,
	AuthFail = 1 << 1,
	ValidationStarted = 1 << 2,
	KickUser = 1 << 3,
	FailKick = AuthFail | KickUser,
	HasOrIsPendingAuth = AuthSuccess | ValidationStarted
};

ENUM_CLASS_FLAGS(ESteamAuthStatus);

class FOnlineAuthSteam
{
PACKAGE_SCOPE:
	FOnlineAuthSteam(FOnlineSubsystemSteam* InSubsystem);

	/** Data pertaining the current authentication state of the users in the game */
	struct FSteamAuthUser
	{
		FSteamAuthUser() : Status(ESteamAuthStatus::None) { }
		void SetKey(const FString& NewKey);
		// String representation of another user's ticket. Stored only temporarily
		FString RecvTicket;
		ESteamAuthStatus Status;
	};

	typedef TSharedPtr<FSteamAuthUser, ESPMode::NotThreadSafe> SharedAuthUserSteamPtr;

	SharedAuthUserSteamPtr GetUser(const FUniqueNetId& InUserId);
	SharedAuthUserSteamPtr GetOrCreateUser(const FUniqueNetId& InUserId);

	bool AuthenticateUser(const FUniqueNetId& InUserId);
	void EndAuthentication(const FUniqueNetId& InUserId);
	void MarkPlayerForKick(const FUniqueNetId& InUserId);
	void RevokeTicket(const uint32& Handle);
	void RevokeAllTickets();
	void RemoveUser(const FUniqueNetId& TargetUser);

	/** Generates Steam auth tickets */
	FString GetAuthTicket(uint32& AuthTokenHandle);

	bool Tick(float DeltaTime);
	bool Exec(const TCHAR* Cmd);

	/** Callback from Steam messaging */
	void OnAuthResult(const FUniqueNetId& TargetId, int32 Response);

	void ExecuteResultDelegate(const FUniqueNetId& TargetId, bool bWasSuccessful);

private:
	typedef TMap<FUniqueNetIdSteam, SharedAuthUserSteamPtr> SteamAuthentications;
	SteamAuthentications AuthUsers;
	TArray<uint32> SteamTicketHandles;

	/** Utility functions */
	FORCEINLINE bool IsServer() const
	{
		return SteamSubsystem != nullptr && SteamSubsystem->IsServer();
	}
	bool KickPlayer(const FUniqueNetId& InUserId, bool bSuppressFailure);

	FOnlineAuthSteam();

	/** Steam Interfaces */
	class ISteamUser* SteamUserPtr;
	class ISteamGameServer* SteamServerPtr;

	/** Cached pointer to owning subsystem */
	class FOnlineSubsystemSteam* SteamSubsystem;

	/** Settings */
	bool bEnabled;

	/** Testing flags */
PACKAGE_SCOPE:
	bool bBadKey;		// Send out invalid keys
	bool bReuseKey;		// Always send out the same key
	bool bBadWrite;		// Always make the bit writers have errors
	bool bDropAll;		// Drop all packets
	bool bRandomDrop;	// Randomly drop packets.
	bool bNeverSendKey;	// Client never sends their key.
	bool bSendBadId;	// Always send invalid steam ids.

public:
	virtual ~FOnlineAuthSteam();

	/** Setting Getters */
	bool IsSessionAuthEnabled() const { return bEnabled; }

	static uint32 GetMaxTicketSizeInBytes();

	/** Attach to this delegate to control the behavior of authentication failure. 
	 *  This overrides the default behavior (kick). 
	 */
	FOnSteamAuthFailureDelegate OverrideFailureDelegate;
	FOnSteamAuthResultDelegate OnAuthenticationResultDelegate;
};
