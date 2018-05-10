// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DatasmithContentEditorModule.h"
#include "DatasmithSceneActorDetailsPanel.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

/**
 * DatasmithContent module implementation (private)
 */
class FDatasmithContentEditorModule : public IDatasmithContentEditorModule
{
public:
	virtual void StartupModule() override
	{
		// Register the details customizer
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked< FPropertyEditorModule >( TEXT("PropertyEditor") );
		PropertyModule.RegisterCustomClassLayout( TEXT("DatasmithSceneActor"), FOnGetDetailCustomizationInstance::CreateStatic( &FDatasmithSceneActorDetailsPanel::MakeInstance ) );
	}

	virtual void ShutdownModule() override
	{
		// Unregister the details customization
		if ( FModuleManager::Get().IsModuleLoaded( TEXT("PropertyEditor") ) )
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked< FPropertyEditorModule >( TEXT("PropertyEditor") );
			PropertyModule.UnregisterCustomClassLayout( TEXT("DatasmithSceneActor") );
			PropertyModule.NotifyCustomizationModuleChanged();
		}
	}

	virtual void RegisterSpawnDatasmithSceneActorsHandler( FOnSpawnDatasmithSceneActors InSpawnActorsDelegate ) override
	{
		SpawnActorsDelegate = InSpawnActorsDelegate;
	}

	virtual void UnregisterSpawnDatasmithSceneActorsHandler( FOnSpawnDatasmithSceneActors InSpawnActorsDelegate ) override
	{
		SpawnActorsDelegate.Unbind();
	}

	virtual FOnSpawnDatasmithSceneActors GetSpawnDatasmithSceneActorsHandler() const override
	{
		return SpawnActorsDelegate;
	}

private:
	FOnSpawnDatasmithSceneActors SpawnActorsDelegate;
};

IMPLEMENT_MODULE(FDatasmithContentEditorModule, DatasmithContentEditor);
