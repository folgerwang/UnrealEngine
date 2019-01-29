// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Engine.h"
#include "MagicLeapScreensTypes.generated.h"

/** 
  ID for a Screens Watch History Entry.

  Save this off when you add a new watch history and use the same to update or delete that same entry.
 */
USTRUCT(BlueprintType)
struct FScreenID
{
	GENERATED_BODY()

public:
	int64 ID;
};

/** Channel watch history, may be displayed in the Screens Launcher application. */
USTRUCT(BlueprintType)
struct FScreensWatchHistoryEntry
{
	GENERATED_BODY()

public:
	/** Entry Identifier. Must be used to update and delete a given entry. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Screens|MagicLeap")
	FScreenID ID;

	/** Title of the media for which this entry is created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screens|MagicLeap")
	FString Title;

	/** Subtitle of the media for which this entry is created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screens|MagicLeap")
	FString Subtitle;

	/** Current media playback position. Can be fed from UMediaPlayer::GetTime(). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screens|MagicLeap")
	FTimespan PlaybackPosition;

	/** Total duration of the media. Can be fed from UMediaPlayer::GetDuration() */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screens|MagicLeap")
	FTimespan PlaybackDuration;

	/** Any data the application might want to save off in the watch history and then receive back from the Screens Launher. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screens|MagicLeap")
	FString CustomData;

	/** Thumbnail to be shown in the Screens Launcher application for this watch history entry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screens|MagicLeap")
	UTexture2D* Thumbnail;
};

/**
  Information required to place a screen in the world.

  This will be received from the Screens Launcher api, based on the previous screens spawned by user.
 */
USTRUCT(BlueprintType)
struct FScreenTransform
{
	GENERATED_BODY()

public:
	/** Position of the screen in Unreal's world space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screens|MagicLeap")
	FVector ScreenPosition;

	/** Orientation of the screen in Unreal's world space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screens|MagicLeap")
	FRotator ScreenOrientation;

	/** Dimensions of the screen in Unreal Units. The dimensions are axis-aligned with the orientation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screens|MagicLeap")
	FVector ScreenDimensions;
};

/**
	Delegate used to relay the result of a Screens operation that involves a single watch history entry.
	For example updating or adding a history entry.

	@param[out] bSuccess True when the request is successful
	@param[out] WatchHistoryEntry Resulting watch history entry for which the operation was performed on.
*/
DECLARE_DYNAMIC_DELEGATE_TwoParams(FScreensEntryRequestResultDelegate, const bool, bSuccess, const FScreensWatchHistoryEntry&, WatchHistoryEntry);

/**
	Delegate used to relay the result of getting the entire watch history.

	@param[out] bSuccess True when the request is successful.
	@param[out] WatchHistoryEntry Resulting array of watch histories returned by the operation.
*/
DECLARE_DYNAMIC_DELEGATE_TwoParams(FScreensHistoryRequestResultDelegate, const bool, bSuccess, const TArray<FScreensWatchHistoryEntry>&, WatchHistoryEntries);