// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealEdMisc.h"
#include "SubComponentTreeItem.h"

namespace SceneOutliner
{

	class FSceneOutlinerDelegates
	{
	public:
		/** Return a single FSceneOutlinerDelegates object */
		SCENEOUTLINER_API static FSceneOutlinerDelegates& Get()
		{
			// return the singleton object
			static FSceneOutlinerDelegates Singleton;
			return Singleton;
		}

		/**	Broadcasts whenever the current selection changes */
		FSimpleMulticastDelegate SelectionChanged;

		/** Broadcasts whenever a SubComponentTreeItem selection changes */
		DECLARE_MULTICAST_DELEGATE_OneParam(FOnSubComponentSelectionChanged, TArray<FSubComponentTreeItem*>&);
		FOnSubComponentSelectionChanged OnSubComponentSelectionChanged;

	};

} // namespace SceneOutliner
