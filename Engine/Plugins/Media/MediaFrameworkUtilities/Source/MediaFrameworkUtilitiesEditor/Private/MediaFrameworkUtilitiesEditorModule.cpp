// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "UObject/UObjectBase.h"

#include "AssetTypeActions/AssetTypeActions_MediaBundle.h"
#include "MediaBundleActorDetails.h"
#include "MediaBundleFactoryNew.h"
#include "MediaFrameworkUtilitiesPlacement.h"
#include "CaptureTab/SMediaFrameworkCapture.h"
#include "UI/MediaFrameworkUtilitiesEditorStyle.h"
#include "UI/MediaProfileMenuEntry.h"

#define LOCTEXT_NAMESPACE "MediaFrameworkEditor"

/**
 * Implements the MediaPlayerEditor module.
 */
class FMediaFrameworkUtilitiesEditorModule : public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override
	{
		if (GEditor)
		{
			FMediaFrameworkUtilitiesEditorStyle::Register();

			GEditor->ActorFactories.Add(NewObject<UActorFactoryMediaBundle>());

			FMediaFrameworkUtilitiesPlacement::RegisterPlacement();

			// Register AssetTypeActions
			AssetTypeAction = MakeShareable(new FAssetTypeActions_MediaBundle());
			FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().RegisterAssetTypeActions(AssetTypeAction.ToSharedRef());

			FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.RegisterCustomClassLayout("MediaBundleActorBase", FOnGetDetailCustomizationInstance::CreateStatic(&FMediaBundleActorDetails::MakeInstance));

			SMediaFrameworkCapture::RegisterNomadTabSpawner();
			FMediaProfileMenuEntry::Register();
		}
	}

	virtual void ShutdownModule() override
	{
		if (!GIsRequestingExit && GEditor && UObjectInitialized())
		{
			FMediaProfileMenuEntry::Register();
			SMediaFrameworkCapture::UnregisterNomadTabSpawner();

			FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomClassLayout("MediaBundleActorBase");

			// Unregister AssetTypeActions
			FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().UnregisterAssetTypeActions(AssetTypeAction.ToSharedRef());

			FMediaFrameworkUtilitiesPlacement::UnregisterPlacement();

			GEditor->ActorFactories.RemoveAll([](const UActorFactory* ActorFactory) { return ActorFactory->IsA<UActorFactoryMediaBundle>(); });

			FMediaFrameworkUtilitiesEditorStyle::Unregister();
		}
	}

	TSharedPtr<FAssetTypeActions_MediaBundle> AssetTypeAction;
};


IMPLEMENT_MODULE(FMediaFrameworkUtilitiesEditorModule, MediaFrameworkUtilitiesEditor);


#undef LOCTEXT_NAMESPACE
