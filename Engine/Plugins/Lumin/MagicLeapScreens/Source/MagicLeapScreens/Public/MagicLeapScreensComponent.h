// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MagicLeapScreensTypes.h"
#include "MagicLeapScreensComponent.generated.h"

/**
	Component that provides access to screens functionality.
*/
UCLASS(ClassGroup = MagicLeap, BlueprintType, Blueprintable, EditInlineNew, meta = (BlueprintSpawnableComponent))
class MAGICLEAPSCREENS_API UScreensComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UScreensComponent();
	virtual ~UScreensComponent();

	/** Polls for incoming messages from the worker thread */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/** 
		Queues a task on the worker thread to retrieve the watch history. 
		Subscribe to the GetWatchHistorySuccess & GetWatchHistoryFailure delegates to receive the result.
	*/
	UFUNCTION(BlueprintCallable, Category = "Screens|MagicLeap", meta = (DeprecatedFunction, DeprecationMessage = "Please use the Magic Leap Screens Function Library instead."))
	void GetWatchHistoryAsync();

	/** 
		Queues a task on the worker thread to add a new entry into the watch history.
		Subscribe to the AddToWatchHistoryResult delegate to receive the result.
	
		@param[in] WatchHistoryEntry The entry to be added to the history.
	*/
	UFUNCTION(BlueprintCallable, Category = "Screens|MagicLeap", meta = (DeprecatedFunction, DeprecationMessage = "Please use the Magic Leap Screens Function Library instead."))
	void AddWatchHistoryEntryAsync(const FScreensWatchHistoryEntry& WatchHistoryEntry);

	/**
		Queues a task on the worker thread to update an entry in the watch history.
		The ID of the entry to be updated must be valid and already present in the watch history.
		Subscribe to the UpdateWatchHistoryEntryResult delegate to receive the result.
	
		@param[in] WatchHistoryEntry The entry to be updated.
	*/
	UFUNCTION(BlueprintCallable, Category = "Screens|MagicLeap", meta = (DeprecatedFunction, DeprecationMessage = "Please use the Magic Leap Screens Function Library instead."))
	void UpdateWatchHistoryEntryAsync(const FScreensWatchHistoryEntry& WatchHistoryEntry);

	/**
		Removes a watch history entry.
		
		@param[in] FScreenID The id of the entry to find and remove.
		@return True if no errors were encountered.
	*/
	UFUNCTION(BlueprintCallable, Category = "Screens|MagicLeap", meta = (DeprecatedFunction, DeprecationMessage = "Please use the Magic Leap Screens Function Library instead."))
	bool RemoveWatchHistoryEntry(const FScreenID& ID);

	/**
		Permanently clears all watch history entries from disk.
	
		@return True if no errors were encountered.
	*/
	UFUNCTION(BlueprintCallable, Category = "Screens|MagicLeap", meta = (DeprecatedFunction, DeprecationMessage = "Please use the Magic Leap Screens Function Library instead."))
	bool ClearWatchHistory();

	/** 
		Retrieves a list of screen transforms.
		
		@param[out] ScreensTransforms A list of transforms to be provided by the underlying api.
		@return True if no errors were encountered.
	*/
	UFUNCTION(BlueprintCallable, Category = "Screens|MagicLeap", meta = (DeprecatedFunction, DeprecationMessage = "Please use the Magic Leap Screens Function Library instead."))
	bool GetScreensTransforms(TArray<FScreenTransform>& ScreensTransforms);

public:
	/** 
		Delegate used to notify the instigating blueprint of a watch history retrieval success.

		@param[out] WatchHistory A list of history entries requested via a call to GetWatchHistoryAsync().
	*/
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FScreensGetWatchHistorySuccess, const TArray<FScreensWatchHistoryEntry>&, WatchHistory);

	/** Delegate used to notify the instigating blueprint of a watch history retrieval failure. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FScreensGetWatchHistoryFailure);

	/** 
		Delegate used to notify the instigating blueprint of the result from a request to add a new watch history entry.

		@param[out] Entry The new entry created if a request via a call to AddToWatchHistoryAsync() succeeds.
		@param[out] Success True when the call to AddToWatchHistoryAsync() succeeds.
	*/
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FScreensAddToWatchHistoryResult, const FScreensWatchHistoryEntry&, Entry, const bool, Success);

	/**
		Delegate used to notify the instigating blueprint of the result from a request to update an entry in the watch history

		@param[out] Entry The updated entry resulting from a successful request via a call to UpdateWatchHistoryEntry().
		@param[out] Success True when the call to UpdateWatchHistoryEntryAsync() succeeds.
	*/
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FScreensUpdateWatchHistoryEntryResult, const FScreensWatchHistoryEntry&, Entry, const bool, Success);

private:

	UPROPERTY(BlueprintAssignable, Category = "Screens|MagicLeap", meta = (AllowPrivateAccess = true))
	FScreensGetWatchHistorySuccess GetWatchHistorySuccess;

	UPROPERTY(BlueprintAssignable, Category = "Screens|MagicLeap", meta = (AllowPrivateAccess = true))
	FScreensGetWatchHistoryFailure GetWatchHistoryFailure;

	UPROPERTY(BlueprintAssignable, Category = "Screens|MagicLeap", meta = (AllowPrivateAccess = true))
	FScreensAddToWatchHistoryResult AddToWatchHistoryResult;

	UPROPERTY(BlueprintAssignable, Category = "Screens|MagicLeap", meta = (AllowPrivateAccess = true))
	FScreensUpdateWatchHistoryEntryResult UpdateWatchHistoryEntryResult;
};

