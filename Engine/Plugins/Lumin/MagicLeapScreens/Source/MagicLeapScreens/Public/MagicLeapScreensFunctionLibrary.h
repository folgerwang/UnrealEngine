// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Engine.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MagicLeapScreensTypes.h"
#include "MagicLeapScreensFunctionLibrary.generated.h"

UCLASS(ClassGroup = MagicLeap)
class MAGICLEAPSCREENS_API UMagicLeapScreensFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
		Asynchronously requests all watch history entries.

		@param[in] ResultDelegate Delegate that is called once the request has been resolved.
	*/
	UFUNCTION(BlueprintCallable, Category = "Screens Function Library|MagicLeap")
	static void GetWatchHistoryAsync(const FScreensHistoryRequestResultDelegate& ResultDelegate);

	/**
		Asynchronously requests to adds a new entry into the watch history.

		@param[in] NewEntry Watch history entry to add.
		@param[in] ResultDelegate Delegate that is called once the add request has been resolved. The resulting FScreensWatchHistoryEntry returned.
		on success contains the new ID of the added entry.
	*/
	UFUNCTION(BlueprintCallable, Category = "Screens Function Library|MagicLeap")
	static void AddToWatchHistoryAsync(const FScreensWatchHistoryEntry& NewEntry, const FScreensEntryRequestResultDelegate& ResultDelegate);

	/**
		Asynchronously requests to update an entry in the watch history

		@param[in] UpdateEntry Watch history entry to update. The ID of this entry must be valid and in the watch history in order for the update to resolve successfully
		@param[in] ResultDelegate Delegate that is called once the update request has been resolved.
	*/
	UFUNCTION(BlueprintCallable, Category = "Screens Function Library|MagicLeap")
	static void UpdateWatchHistoryEntryAsync(const FScreensWatchHistoryEntry& UpdateEntry, const FScreensEntryRequestResultDelegate& ResultDelegate);

	/**
		Removes an entry from the watch history that corresponse with the ID passed.

		@param[in] ID ID of the watch history entry to remove.
		@return True when the entry is sucessfully removed.
	*/
	UFUNCTION(BlueprintCallable, Category = "Screens Function Library|MagicLeap")
	static bool RemoveWatchHistoryEntry(const FScreenID& ID);

	/**
		Removes all watch history entries.

		@return True when all watch history entries have been successfully removed.
	*/
	UFUNCTION(BlueprintCallable, Category = "Screens Function Library|MagicLeap")
	static bool ClearWatchHistory();

	/**
		Gets the transform for all watch history entries.

		@param[out] ScreenTransforms Array of transforms that corresponse to all current watch history entries.
		@return True when the request for all transforms succeeds 
	*/
	UFUNCTION(BlueprintCallable, Category = "Screens Function Library|MagicLeap")
	static bool GetScreensTransforms(TArray<FScreenTransform>& ScreenTransforms);

	/**
		Delegate used to relay the results of GetWatchHistory.
	*/
	UPROPERTY()
	FScreensHistoryRequestResultDelegate HistoryResultDelegate;

	/**
		Delegate used to relay the results of both AddToWatchHistory and UpdateWatchHistoryEntry.
	*/
	UPROPERTY()
	FScreensEntryRequestResultDelegate EntryResultDelegate;
};
