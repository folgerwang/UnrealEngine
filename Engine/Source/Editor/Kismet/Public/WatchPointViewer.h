// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

namespace WatchViewer
{
	// called when we pause execution to update the displayed watch values
	void KISMET_API UpdateDisplayedWatches(const TArray<const FFrame*>& ScriptStack);

	// called when we unpause execution and set watch values back to the blueprint versions
	void KISMET_API ContinueExecution();

	// called when we are adding or changing watches from BlueprintObj
	void KISMET_API UpdateWatchListFromBlueprint(class UBlueprint* BlueprintObj);

	FName GetTabName();
	void RegisterTabSpawner(FTabManager& TabManager);
}
