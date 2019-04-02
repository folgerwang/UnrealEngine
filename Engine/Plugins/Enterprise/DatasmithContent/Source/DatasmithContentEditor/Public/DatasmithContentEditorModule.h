// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Developer/AssetTools/Public/AssetTypeCategories.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"		// For inline LoadModuleChecked()
#include "Toolkits/IToolkit.h"

#define DATASMITHCONTENTEDITOR_MODULE_NAME TEXT("DatasmithContentEditor")

struct FGuid;
class UDatasmithImportOptions;
class UDatasmithScene;
class UPackage;
class UWorld;

// DATAPREP_TODO: Temporary interface to emulate future workflow. Interface to trigger build world and 'finalize' from data prep editor
class IDataPrepImporterInterface
{
public:
	virtual ~IDataPrepImporterInterface() = default;

	/**
	 * @param GUID				The GUID to use as a seed when generating unique ids.
	 * @param ImportWorld		The destination world that we will spawn the actors in.
	 * @param DatasmithScene	The DatasmithScene that we will apply the data prep pipeline on.
	 */
	virtual bool Initialize(const FGuid& Guid, UWorld* ImportWorld, UDatasmithScene* DatasmithScene) = 0;
	virtual bool BuildWorld(TArray<TWeakObjectPtr<UObject>>& OutAssets) = 0;
	virtual bool SetFinalWorld(UWorld* FinalWorld) = 0;
	virtual bool FinalizeAssets(const TArray<TWeakObjectPtr<UObject>>& Assets) = 0;
	virtual TSubclassOf<class UDatasmithSceneImportData> GetAssetImportDataClass() const = 0;
};

DECLARE_DELEGATE_TwoParams( FOnSpawnDatasmithSceneActors, class ADatasmithSceneActor*, bool );
DECLARE_DELEGATE_ThreeParams( FOnCreateDatasmithSceneEditor, const EToolkitMode::Type, const TSharedPtr< class IToolkitHost >&, class UDatasmithScene*);
DECLARE_DELEGATE_RetVal(TSharedPtr<IDataPrepImporterInterface>, FOnCreateDatasmithImportHandler );

struct FImporterDescription
{
	FText Label;
	FText Description;
	FName StyleName;
	FString IconPath;
	TArray<FString> Formats;
	FString FilterString;
	FOnCreateDatasmithImportHandler Handler;
};


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
	 */
	virtual void RegisterSpawnDatasmithSceneActorsHandler( FOnSpawnDatasmithSceneActors SpawnActorsDelegate ) = 0;
	virtual void UnregisterSpawnDatasmithSceneActorsHandler( FOnSpawnDatasmithSceneActors SpawnActorsDelegate ) = 0;
	virtual FOnSpawnDatasmithSceneActors GetSpawnDatasmithSceneActorsHandler() const = 0;

	/**
	 * Delegate to creation of the datasmith scene editor. The action is in this module while the datasmith scene editor is in its own plugin
	 */
	virtual void RegisterDatasmithSceneEditorHandler(FOnCreateDatasmithSceneEditor InCreateDatasmithSceneEditor) = 0;
	virtual void UnregisterDatasmithSceneEditorHandler(FOnCreateDatasmithSceneEditor InCreateDatasmithSceneEditor) = 0;
	virtual FOnCreateDatasmithSceneEditor GetDatasmithSceneEditorHandler() const = 0;

	/**
	* Delegate to creation of the datasmith scene editor. The action is in this module while the datasmith scene editor is in its own plugin
	*/
	virtual void RegisterDatasmithImporter(const void* Registrar, const FImporterDescription& ImporterDescription) = 0;
	virtual void UnregisterDatasmithImporter(const void* Registrar) = 0;
	virtual TArray<FImporterDescription> GetDatasmithImporters() = 0;

	/** Category bit associated with Datasmith related content */
	static DATASMITHCONTENTEDITOR_API EAssetTypeCategories::Type DatasmithAssetCategoryBit;
};

