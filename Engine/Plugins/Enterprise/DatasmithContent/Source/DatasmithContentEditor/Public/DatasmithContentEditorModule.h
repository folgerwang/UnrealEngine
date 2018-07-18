// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"		// For inline LoadModuleChecked()

#define DATASMITHCONTENTEDITOR_MODULE_NAME TEXT("DatasmithContentEditor")

DECLARE_DELEGATE_TwoParams( FOnSpawnDatasmithSceneActors, class ADatasmithSceneActor*, bool );

/**
 * The public interface of the DatasmithContent module
 */
class IDatasmithContentEditorModule : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to IDatasmithContentEditorModule
	 *
	 * @return Returns DatasmithContent singleton instance, loading the module on demand if needed
	 */
	static inline IDatasmithContentEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IDatasmithContentEditorModule>(DATASMITHCONTENTEDITOR_MODULE_NAME);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(DATASMITHCONTENTEDITOR_MODULE_NAME);
	}

	/**
	 * Delegate to spawn the actors related to a Datasmith scene. Called when the user triggers the action in the UI.
	 * The UI is in DatasmithContent both the actual action is in the DatasmithImporter module.
	 */
	virtual void RegisterSpawnDatasmithSceneActorsHandler( FOnSpawnDatasmithSceneActors SpawnActorsDelegate ) = 0;
	virtual void UnregisterSpawnDatasmithSceneActorsHandler( FOnSpawnDatasmithSceneActors SpawnActorsDelegate ) = 0;
	virtual FOnSpawnDatasmithSceneActors GetSpawnDatasmithSceneActorsHandler() const = 0;
};

