// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Containers/Array.h"
#include "Misc/Optional.h"
#include "IMagicLeapPlugin.h"
#include "IMagicLeapScreensPlugin.h"
#include "MagicLeapUtils.h"
#include "MagicLeapScreensMsg.h"
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_image.h>
#include <ml_screens.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

class FMagicLeapScreensPlugin : public IMagicLeapScreensPlugin
{
public:
	void StartupModule() override;
	void ShutdownModule() override;
	bool IsEngineLoopInitComplete() const override;
	void OnEngineLoopInitComplete() override;
	static bool IsSupportedFormat(EPixelFormat InPixelFormat);

#if WITH_MLSDK
	static UTexture2D* MLImageToUTexture2D(const MLImage& Source);
	static void MLWatchHistoryEntryToUnreal(const MLScreensWatchHistoryEntry& InEntry, FScreensWatchHistoryEntry& OutEntry);
	static bool UTexture2DToMLImage(const UTexture2D& Source, MLImage& Target);
#endif // WITH_MLSDK

	static bool RemoveWatchHistoryEntry(const FScreenID& ID);
	static FScreensMessage GetWatchHistoryEntries();
	static bool ClearWatchHistory();
	bool Tick(float DeltaTime);
	static FScreensMessage AddToWatchHistory(const FScreensWatchHistoryEntry& WatchHistoryEntry);

	static void GetWatchHistoryEntriesAsync(const TOptional<FScreensHistoryRequestResultDelegate>& OptionalResultDelegate);
	static void AddToWatchHistoryAsync(const FScreensWatchHistoryEntry& NewEntry, const TOptional<FScreensEntryRequestResultDelegate>& OptionalResultDelegate);
	static void UpdateWatchHistoryEntryAsync(const FScreensWatchHistoryEntry& UpdateEntry, const TOptional<FScreensEntryRequestResultDelegate>& OptionalResultDelegate);

	static FScreensMessage UpdateWatchHistoryEntry(const FScreensWatchHistoryEntry& WatchHistoryEntry);
	static bool GetScreensTransforms(TArray<FScreenTransform>& ScreensTransforms);
	static TArray<uint8> PixelDataMemPool;
	static class FScreensWorker *Impl;

private:
#if WITH_MLSDK
	static bool ShouldUseDefaultThumbnail(const FScreensWatchHistoryEntry& Entry, MLImage& MLImage);
	static MLImage DefaultThumbnail;
	static FCriticalSection CriticalSection;
#endif // WITH_MLSDK
	FMagicLeapAPISetup APISetup;
	bool bEngineLoopInitComplete;
};
