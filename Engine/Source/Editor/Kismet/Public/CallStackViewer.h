// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class FTabManager;
struct FFrame;

namespace CallStackViewer
{
	void KISMET_API UpdateDisplayedCallstack(const TArray<const FFrame*>& ScriptStack);
	FName GetTabName();
	void RegisterTabSpawner(FTabManager& TabManager);
}
