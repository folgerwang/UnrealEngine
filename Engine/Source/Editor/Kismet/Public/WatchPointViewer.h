// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

struct FAssetData;
class UBlueprint;

namespace WatchViewer
{
	// updates the instanced watch values, these are only valid while execution is paused
	void KISMET_API UpdateInstancedWatchDisplay();

	// called when we unpause execution and set watch values back to the blueprint versions
	void KISMET_API ContinueExecution();

	// called when we are adding or changing watches from BlueprintObj
	void KISMET_API UpdateWatchListFromBlueprint(TWeakObjectPtr<UBlueprint> BlueprintObj);

	// called when we want to remove watches in the watch window from a blueprint
	// does NOT remove watches from the pins in the blueprint object
	void KISMET_API RemoveWatchesForBlueprint(TWeakObjectPtr<UBlueprint> BlueprintObj);

	// called when we want to remove watches in the watch window from a blueprint
	// does NOT remove watches from the pins in the blueprint object
	void KISMET_API RemoveWatchesForAsset(const FAssetData& AssetData);

	// called when an asset is renamed; updates the watches on the asset
	void KISMET_API OnRenameAsset(const FAssetData& AssetData, const FString& OldAssetName);

	// called when a BlueprintObj should no longer be watched
	void KISMET_API ClearWatchListFromBlueprint(TWeakObjectPtr<UBlueprint> BlueprintObj);

	FName GetTabName();
	void RegisterTabSpawner(FTabManager& TabManager);
}
