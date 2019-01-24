// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ConcertSyncClientStatics.generated.h"


/**
 * BP copy of FConcertClientInfo
 * Holds info on a client connected through concert
 */
USTRUCT(BlueprintType)
struct FConcertSyncClientInfo
{
	GENERATED_BODY()

	/** Holds the display name of the user that owns this instance. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Client Info")
	FString DisplayName;

	/** Holds the color of the user avatar in a session. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Client Info")
	FLinearColor AvatarColor;
};


UCLASS()
class CONCERTSYNCCLIENTLIBRARY_API UConcertSyncClientStatics : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	/** Set whether presence is currently enabled and should be shown (unless hidden by other settings) */
	UFUNCTION(BlueprintCallable, Category = "Concert Presence", meta=(DevelopmentOnly))
	static void SetPresenceEnabled(const bool IsEnabled = true);

	/** Set Presence Actor Visibility */
	UFUNCTION(BlueprintCallable, Category = "Concert Presence", meta=(DevelopmentOnly))
	static void SetPresenceVisibility(FString Name, bool Visibility, bool PropagateToAll = false);

	/** Update Concert Workspace Modified Packages to be in sync for source control submission. */
	UFUNCTION(BlueprintCallable, Category = "Concert Source Control", meta=(DevelopmentOnly, DeprecatedFunction, DeprecationMessage = "UpdateWorkspaceModifiedPackages is deprecated. Please use PersistSessionChanges instead."))
	static void UpdateWorkspaceModifiedPackages();

	/** Persist the session changes and prepare the files for source control submission. */
	UFUNCTION(BlueprintCallable, Category = "Concert Source Control", meta=(DevelopmentOnly))
	static void PersistSessionChanges();

	/** Get the local ClientInfo. Works when not connected to a session. */
	UFUNCTION(BlueprintCallable, Category = "Concert Client", meta=(DevelopmentOnly))
	static FConcertSyncClientInfo GetLocalConcertClientInfo();

	/** Get the ClientInfo for any Concert participant by name. The local user is found even when not connected to a session. Returns false is no client was found. */
	UFUNCTION(BlueprintCallable, Category = "Concert Client", meta=(DevelopmentOnly))
	static bool GetConcertClientInfoByName(const FString ClientName, FConcertSyncClientInfo& ClientInfo);

	/** Get ClientInfos of current Concert participants except for the local user. Returns false is no remote clients were found. */
	UFUNCTION(BlueprintCallable, Category = "Concert Client", meta=(DevelopmentOnly))
	static bool GetRemoteConcertClientInfos(TArray<FConcertSyncClientInfo>& ClientInfos);

	/** Get Concert connection status. */
	UFUNCTION(BlueprintCallable, Category = "Concert Client", meta=(DevelopmentOnly))
	static bool GetConcertConnectionStatus();

	/** Teleport to another Concert user's presence. */
	UFUNCTION(BlueprintCallable, Category = "Concert Client", meta=(DevelopmentOnly))
	static void ConcertJumpToPresence(const FString OtherUserName);
};
