// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapScreensFunctionLibrary.h"
#include "MagicLeapHMD.h"
#include "MagicLeapUtils.h"
#include "MagicLeapScreensWorker.h"
#include "MagicLeapScreensMsg.h"
#include "MagicLeapScreensPlugin.h"

void UMagicLeapScreensFunctionLibrary::GetWatchHistoryAsync(const FScreensHistoryRequestResultDelegate& ResultDelegate)
{
#if WITH_MLSDK
	FMagicLeapScreensPlugin::GetWatchHistoryEntriesAsync(TOptional<FScreensHistoryRequestResultDelegate>(ResultDelegate));
#else
	TArray<FScreensWatchHistoryEntry> ResultEntries;
	ResultDelegate.ExecuteIfBound(false, ResultEntries);
#endif // WITH_MLSDK
}

void UMagicLeapScreensFunctionLibrary::AddToWatchHistoryAsync(const FScreensWatchHistoryEntry& NewEntry, const FScreensEntryRequestResultDelegate& ResultDelegate)
{
#if WITH_MLSDK
	FMagicLeapScreensPlugin::AddToWatchHistoryAsync(NewEntry, TOptional<FScreensEntryRequestResultDelegate>(ResultDelegate));
#else
	ResultDelegate.ExecuteIfBound(false, NewEntry);
#endif // WITH_MLSDK
}

void UMagicLeapScreensFunctionLibrary::UpdateWatchHistoryEntryAsync(const FScreensWatchHistoryEntry& UpdateEntry, const FScreensEntryRequestResultDelegate& ResultDelegate)
{
#if WITH_MLSDK
	FMagicLeapScreensPlugin::UpdateWatchHistoryEntryAsync(UpdateEntry, TOptional<FScreensEntryRequestResultDelegate>(ResultDelegate));
#else
	ResultDelegate.ExecuteIfBound(false, UpdateEntry);
#endif // WITH_MLSDK
}

bool UMagicLeapScreensFunctionLibrary::RemoveWatchHistoryEntry(const FScreenID& ID)
{
	return FMagicLeapScreensPlugin::RemoveWatchHistoryEntry(ID);
}

bool UMagicLeapScreensFunctionLibrary::ClearWatchHistory()
{
	return FMagicLeapScreensPlugin::ClearWatchHistory();
}

bool UMagicLeapScreensFunctionLibrary::GetScreensTransforms(TArray<FScreenTransform>& ScreensTransforms)
{
	return FMagicLeapScreensPlugin::GetScreensTransforms(ScreensTransforms);
}
