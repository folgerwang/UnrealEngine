// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class FWorkspaceItem;

/**
 * Interface for the Concert Frontend module.
 */
class IConcertFrontendModule : public IModuleInterface
{
public:
	/** Registers a tab spawner for the the Concert Frontend */
	virtual void RegisterTabSpawner(const TSharedPtr<FWorkspaceItem>& WorkspaceGroup) = 0;

	/** Unregisters the tab spawner for the Concert Frontend */
	virtual void UnregisterTabSpawner() = 0;

	/** Get the Concert Frontend module */
	static IConcertFrontendModule& Get()
	{
		static const FName ModuleName = TEXT("ConcertFrontend");
		return FModuleManager::Get().GetModuleChecked<IConcertFrontendModule>(ModuleName);
	}
};
