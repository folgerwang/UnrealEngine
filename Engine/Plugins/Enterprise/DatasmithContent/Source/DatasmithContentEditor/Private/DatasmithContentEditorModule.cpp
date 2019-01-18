// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DatasmithContentEditorModule.h"

#include "AssetTypeActions_DatasmithScene.h"
#include "DatasmithContentEditorStyle.h"
#include "DatasmithSceneActorDetailsPanel.h"

#include "Developer/AssetTools/Public/IAssetTools.h"
#include "Developer/AssetTools/Public/AssetToolsModule.h"
#include "Engine/Blueprint.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "DatasmithContentEditorModule"

EAssetTypeCategories::Type IDatasmithContentEditorModule::DatasmithAssetCategoryBit;

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

		// Register Datasmith asset category to group asset type actions related to Datasmith together
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		DatasmithAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("Datasmith")), LOCTEXT("DatasmithContentAssetCategory", "Datasmith"));

		// Register asset type actions for DatasmithScene class
		TSharedPtr<FAssetTypeActions_DatasmithScene> DatasmithSceneAssetTypeAction = MakeShareable(new FAssetTypeActions_DatasmithScene);
		AssetTools.RegisterAssetTypeActions(DatasmithSceneAssetTypeAction.ToSharedRef());
		AssetTypeActionsArray.Add(DatasmithSceneAssetTypeAction);
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

		// Unregister asset type actions
		if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetTools")))
		{
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			for (TSharedPtr<FAssetTypeActions_Base>& AssetTypeActions : AssetTypeActionsArray)
			{
				AssetTools.UnregisterAssetTypeActions(AssetTypeActions.ToSharedRef());
			}
		}
		AssetTypeActionsArray.Empty();

		// Shutdown style set associated with datasmith content
		FDatasmithContentEditorStyle::Shutdown();
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

	void RegisterDatasmithSceneEditorHandler(FOnCreateDatasmithSceneEditor InCreateDatasmithSceneEditorDelegate)
	{
		CreateDatasmithSceneEditorDelegate = InCreateDatasmithSceneEditorDelegate;
	}

	virtual void UnregisterDatasmithSceneEditorHandler(FOnCreateDatasmithSceneEditor InCreateDatasmithSceneEditor)
	{
		if (CreateDatasmithSceneEditorDelegate.IsBound() && InCreateDatasmithSceneEditor.GetHandle() == CreateDatasmithSceneEditorDelegate.GetHandle())
		{
			CreateDatasmithSceneEditorDelegate.Unbind();
		}
	}

	virtual FOnCreateDatasmithSceneEditor GetDatasmithSceneEditorHandler() const
	{
		return CreateDatasmithSceneEditorDelegate;
	}

	virtual void RegisterDatasmithImporter(const void* Registrar, const FImporterDescription& ImporterDescription) override
	{
		DatasmithImporterMap.Add(Registrar) = ImporterDescription;
	}

	virtual void UnregisterDatasmithImporter(const void* Registrar) override
	{
		DatasmithImporterMap.Remove(Registrar);
	}

	TArray<FImporterDescription> GetDatasmithImporters()
	{
		TArray<FImporterDescription> Result;

		for (const auto& ImporterDescriptionEntry : DatasmithImporterMap)
		{
			Result.Add(ImporterDescriptionEntry.Value);
		}

		return Result;
	}

private:
	static TSharedPtr<IDataPrepImporterInterface> CreateEmptyDatasmithImportHandler()
	{
		return TSharedPtr<IDataPrepImporterInterface>();
	}

private:
	FOnSpawnDatasmithSceneActors SpawnActorsDelegate;
	FOnCreateDatasmithSceneEditor CreateDatasmithSceneEditorDelegate;
	TArray<TSharedPtr<FAssetTypeActions_Base>> AssetTypeActionsArray;
	TMap<const void*, FImporterDescription> DatasmithImporterMap;

};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDatasmithContentEditorModule, DatasmithContentEditor);
