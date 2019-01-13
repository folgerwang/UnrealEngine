// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConcertMessageData.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ConcertSyncClientStatics.generated.h"

UCLASS()
class CONCERTSYNCCLIENT_API UConcertSyncClientStatics : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

#if WITH_EDITOR

	/** Set whether presence is currently enabled and should be shown (unless hidden by other settings) */
	UFUNCTION(BlueprintCallable, Category = "Concert Presence")
	static void SetPresenceEnabled(const bool IsEnabled = true);

	/** Set Presence Actor Visibility */
	UFUNCTION(BlueprintCallable, Category = "Concert Presence")
	static void SetPresenceVisibility(FString Name, bool Visibility, bool PropagateToAll = false);

	/** Update Concert Workspace Modified Packages to be in sync for source control submission. */
	UFUNCTION(BlueprintCallable, Category = "Concert Source Control", meta=(DeprecatedFunction, DeprecationMessage = "UpdateWorkspaceModifiedPackages is deprecated. Please use PersistSessionChanges instead."))
	static void UpdateWorkspaceModifiedPackages();

	/** Persist the session changes and prepare the files for source control submission. */
	UFUNCTION(BlueprintCallable, Category = "Concert Source Control")
	static void PersistSessionChanges();

	/** Get the local ClientInfo. Works when not connected to a session. */
	UFUNCTION(BlueprintCallable, Category = "Concert Client")
	static FConcertClientInfo GetLocalConcertClientInfo();

	/** Get the ClientInfo for any Concert participant by name. The local user is found even when not connected to a session. Returns false is no client was found. */
	UFUNCTION(BlueprintCallable, Category = "Concert Client")
	static bool GetConcertClientInfoByName(const FString ClientName, FConcertClientInfo& ClientInfo);

	/** Get ClientInfos of current Concert participants except for the local user. Returns false is no remote clients were found. */
	UFUNCTION(BlueprintCallable, Category = "Concert Client")
	static bool GetRemoteConcertClientInfos(TArray<FConcertClientInfo>& ClientInfos);

	/** Get Concert connection status. */
	UFUNCTION(BlueprintCallable, Category = "Concert Client")
	static bool GetConcertConnectionStatus();

	/** Teleport to another Concert user's presence. */
	UFUNCTION(BlueprintCallable, Category = "Concert Client")
	static void ConcertJumpToPresence(const FString OtherUserName);

#endif
};