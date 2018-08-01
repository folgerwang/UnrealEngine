// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "ScreensComponent.generated.h"

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screens|MagicLeap")
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
		Attempts to instigate a task on the worker thread to retrieve the watch history. 
		Subscribe to the GetWatchHistorySuccess & GetWatchHistoryFailure delegates to recevie the result.

		@return True if the worker thread was idle at the time of the call and the task was successfully queued.
	*/
	UFUNCTION(BlueprintCallable, Category = "Screens|MagicLeap")
	bool GetWatchHistoryAsync();

	/** 
		Adds a watch history entry.
	
		@param[in] WatchHistoryEntry The entry to be added to the history.
		@param[out] FScreenID Stores the id of the newly created entry. The same ID should be used to update or delete this entry.
		@return True if no errors were encountered.
	*/
	UFUNCTION(BlueprintCallable, Category = "Screens|MagicLeap")
	bool AddWatchHistoryEntry(const FScreensWatchHistoryEntry& WatchHistoryEntry, FScreenID& ID);

	/**
		Updates a watch history entry.
	
		@param[in] WatchHistoryEntry The entry to be updated.
		@return True if no errors were encountered.
	*/
	UFUNCTION(BlueprintCallable, Category = "Screens|MagicLeap")
	bool UpdateWatchHistoryEntry(const FScreensWatchHistoryEntry& WatchHistoryEntry);

	/**
		Removes a watch history entry.
		
		@param[in] FScreenID The id of the entry to find and remove.
		@return True if no errors were encountered.
	*/
	UFUNCTION(BlueprintCallable, Category = "Screens|MagicLeap")
	bool RemoveWatchHistoryEntry(const FScreenID& ID);

	/**
		Permanently clears all watch history entries from disk.
	
		@return True if no errors were encountered.
	*/
	UFUNCTION(BlueprintCallable, Category = "Screens|MagicLeap")
	bool ClearWatchHistory();

	/** 
		Retrieves a list of screen transforms.
		
		@param[out] ScreensTransforms A list of transforms to be provided by the underlying api.
		@return True if no errors were encountered.
	*/
	UFUNCTION(BlueprintCallable, Category = "Screens|MagicLeap")
	bool GetScreensTransforms(TArray<FScreenTransform>& ScreensTransforms);

public:
	/** 
		Delegate used to notify the instigating blueprint of a watch history retrieval success.

		@param[out] WatchHistory A list of history entries requested via a call to GetWatchHistoryAsync().
	*/
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FScreensGetWatchHistorySuccess, const TArray<FScreensWatchHistoryEntry>&, WatchHistory);

	/** Delegate used to notify the instigating blueprint of a watch history retrieval failure. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FScreensGetWatchHistoryFailure);

	/** Fired when the watch history is successfully retrieved on the worker thread. */
	FScreensGetWatchHistorySuccess& OnScreensGetWatchHistorySuccess();

	/** Fired when retrieval of the watch history fails on the worker thread. */
	FScreensGetWatchHistoryFailure& OnScreensGetWatchHistoryFailure();

private:
	class FScreensImpl *Impl;
	bool bWorkerBusy;

	UPROPERTY(BlueprintAssignable, Category = "Screens|MagicLeap", meta = (AllowPrivateAccess = true))
	FScreensGetWatchHistorySuccess GetWatchHistorySuccess;

	UPROPERTY(BlueprintAssignable, Category = "Screens|MagicLeap", meta = (AllowPrivateAccess = true))
	FScreensGetWatchHistoryFailure GetWatchHistoryFailure;
};
