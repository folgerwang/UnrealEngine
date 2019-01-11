// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DatasmithScene.h"

#include "DatasmithAssetImportData.h"
#include "DatasmithContentEditorModule.h"
#include "DatasmithScene.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_DatasmithScene"

uint32 FAssetTypeActions_DatasmithScene::GetCategories()
{
	return IDatasmithContentEditorModule::DatasmithAssetCategoryBit;
}

FText FAssetTypeActions_DatasmithScene::GetName() const
{
	return NSLOCTEXT("AssetTypeActions_DatasmithScene", "AssetTypeActions_DatasmithScene_Name", "Datasmith Scene");
}

UClass* FAssetTypeActions_DatasmithScene::GetSupportedClass() const
{
	return UDatasmithScene::StaticClass();
}

void FAssetTypeActions_DatasmithScene::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for ( const UObject* Asset : TypeAssets )
	{
		const UDatasmithScene* DatasmithScene = CastChecked< UDatasmithScene >( Asset );

		if ( DatasmithScene && DatasmithScene->AssetImportData )
		{
			DatasmithScene->AssetImportData->ExtractFilenames( OutSourceFilePaths );
		}
	}
}

void FAssetTypeActions_DatasmithScene::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	if (InObjects.Num() == 0)
	{
		return;
	}

	FOnCreateDatasmithSceneEditor DatasmithSceneEditorHandler = IDatasmithContentEditorModule::Get().GetDatasmithSceneEditorHandler();

	if (DatasmithSceneEditorHandler.IsBound() == false)
	{
		FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
		return;
	}

	for (UObject* Object : InObjects)
	{
		UDatasmithScene* DatasmithScene = Cast<UDatasmithScene>(Object);
		if (DatasmithScene != nullptr)
		{
			DatasmithSceneEditorHandler.ExecuteIfBound(EToolkitMode::Standalone, EditWithinLevelEditor, DatasmithScene);
		}
	}
}

#undef LOCTEXT_NAMESPACE
