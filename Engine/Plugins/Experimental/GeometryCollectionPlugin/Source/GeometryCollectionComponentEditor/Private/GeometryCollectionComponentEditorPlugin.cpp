// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#include "GeometryCollectionComponentEditorPlugin.h"

#include "AssetToolsModule.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "GeometryCollectionComponent.h"
#include "AssetTypeActions_GeometryCollection.h"
#include "GeometryCollectionEditorStyle.h"
#include "GeometryCollectionConversion.h"
#include "HAL/ConsoleManager.h"
#include "GeometryCollectionCommands.h"



IMPLEMENT_MODULE( IGeometryCollectionComponentEditorPlugin, GeometryCollectionComponentEditor )

void IGeometryCollectionComponentEditorPlugin::StartupModule()
{
	FGeometryCollectionEditorStyle::Get();

	FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
	IAssetTools& AssetTools = AssetToolsModule.Get();
	AssetTypeActions_GeometryCollection = new FAssetTypeActions_GeometryCollection();
	AssetTools.RegisterAssetTypeActions(MakeShareable(AssetTypeActions_GeometryCollection));

	if (GIsEditor && !IsRunningCommandlet())
	{
		EditorCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("GeometryCollection.CreateFromSelectedActors"),
			TEXT("Creates a GeometryCollection from the selected Actors that contain Skeletal and Statict Mesh Components"),
			FConsoleCommandWithWorldDelegate::CreateStatic(&FGeometryCollectionConversion::CreateFromSelectedActorsCommand),
			ECVF_Default
		));
		EditorCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("GeometryCollection.CreateFromSelectedAssets"),
			TEXT("Creates a GeometryCollection from the selected Skeletal Mesh and Static Mesh Assets"),
			FConsoleCommandWithWorldDelegate::CreateStatic(&FGeometryCollectionConversion::CreateFromSelectedAssetsCommand),
			ECVF_Default
		));
		EditorCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("GeometryCollection.ToString"),
			TEXT("Dump the contents of the collection to the log file. WARNING: The collection can be very large."),
			FConsoleCommandWithWorldDelegate::CreateStatic(&FGeometryCollectionCommands::ToString),
			ECVF_Default
		));
		EditorCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("GeometryCollection.ClusterAlongYZPlane"),
			TEXT("Debuigging command to split the unclustered geometry collection along the YZPlane."),
			FConsoleCommandWithWorldDelegate::CreateStatic(&FGeometryCollectionCommands::SplitAcrossYZPlane),
			ECVF_Default
		));
	}

}


void IGeometryCollectionComponentEditorPlugin::ShutdownModule()
{
	if (UObjectInitialized())
	{
		FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
		IAssetTools& AssetTools = AssetToolsModule.Get();
		AssetTools.UnregisterAssetTypeActions(AssetTypeActions_GeometryCollection->AsShared());
	}
}



