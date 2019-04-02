// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TimeSynthEditorModule.h"
#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "Factories/Factory.h"
#include "AssetTypeActions_Base.h"
#include "TimeSynthClip.h"
#include "TimeSynthVolumeGroup.h"
#include "AudioEditorModule.h"

IMPLEMENT_MODULE(FTimeSynthEditorModule, TimeSynthEditor)

void FTimeSynthEditorModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_TimeSynthClip));
	AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_TimeSynthVolumeGroup));
}

void FTimeSynthEditorModule::ShutdownModule()
{
}


