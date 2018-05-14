// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

struct FAssetData;
class UBlueprint;

namespace WatchViewer
{
	// called when we pause execution to update the displayed watch values
	void KISMET_API UpdateDisplayedWatches(const TArray<const FFrame*>& ScriptStack);

	// called when we unpause execution and set watch values back to the blueprint versions
	void KISMET_API ContinueExecution();

	// called when we are adding or changing watches from BlueprintObj
	void KISMET_API UpdateWatchListFromBlueprint(UBlueprint* BlueprintObj);

	// called when we want to remove watches in the watch window from a blueprint
	// does NOT remove watches from the pins in the blueprint object
	void KISMET_API RemoveWatchesForBlueprint(UBlueprint* BlueprintObj);

	// called when we want to remove watches in the watch window from a blueprint
	// does NOT remove watches from the pins in the blueprint object
	void KISMET_API RemoveWatchesForAsset(const FAssetData& AssetData);

	// called when an asset is renamed; updates the watches on the asset
	void KISMET_API OnRenameAsset(const FAssetData& AssetData, const FString& OldAssetName);

	FName GetTabName();
	void RegisterTabSpawner(FTabManager& TabManager);
}
