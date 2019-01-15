// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DatasmithSceneNewFactory.h"

#include "DatasmithContentEditorModule.h"
#include "DatasmithScene.h"

#include "AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Toolkits/AssetEditorManager.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/SoftObjectPath.h"

#define LOCTEXT_NAMESPACE "DatasmithSceneNewFactory"

UDatasmithSceneNewFactory::UDatasmithSceneNewFactory()
{
	SupportedClass = UDatasmithScene::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UDatasmithSceneNewFactory::FactoryCreateNew(UClass* Class, UObject* Parent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UDatasmithScene::StaticClass()));

	UDatasmithScene* NewDatasmithScene = NewObject<UDatasmithScene>(Parent, Class, Name, Flags | RF_Transactional);
	check(NewDatasmithScene);

	// Notify the asset registry
	FAssetRegistryModule::AssetCreated(NewDatasmithScene);

	// Mark package dirty
	NewDatasmithScene->MarkPackageDirty();

	return NewDatasmithScene;
}

#undef LOCTEXT_NAMESPACE
