// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"


namespace WatchViewer
{
	void KISMET_API UpdateDisplayedWatches(class UBlueprint* BlueprintObj);
	void RegisterTabSpawner();
}
