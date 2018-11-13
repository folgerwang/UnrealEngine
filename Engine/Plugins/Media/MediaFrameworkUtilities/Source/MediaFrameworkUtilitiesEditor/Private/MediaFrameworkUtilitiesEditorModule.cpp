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
#include "AssetTypeActions/AssetTypeActions_MediaProfile.h"
#include "MediaBundleActorDetails.h"
#include "MediaBundleFactoryNew.h"
#include "MediaFrameworkUtilitiesPlacement.h"
#include "CaptureTab/SMediaFrameworkCapture.h"
#include "UI/MediaFrameworkUtilitiesEditorStyle.h"
#include "UI/MediaProfileMenuEntry.h"
#include "AssetEditor/MediaProfileCommands.h"

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
			auto RegisterAssetTypeAction = [this](const TSharedRef<IAssetTypeActions>& InAssetTypeAction)
			{
				IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
				RegisteredAssetTypeActions.Add(InAssetTypeAction);
				AssetTools.RegisterAssetTypeActions(InAssetTypeAction);
			};

			RegisterAssetTypeAction(MakeShared<FAssetTypeActions_MediaBundle>());
			RegisterAssetTypeAction(MakeShared<FAssetTypeActions_MediaProfile>());

			FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.RegisterCustomClassLayout("MediaBundleActorBase", FOnGetDetailCustomizationInstance::CreateStatic(&FMediaBundleActorDetails::MakeInstance));

			SMediaFrameworkCapture::RegisterNomadTabSpawner();
			FMediaProfileMenuEntry::Register();
			FMediaProfileCommands::Register();
		}
	}

	virtual void ShutdownModule() override
	{
		if (!GIsRequestingExit && GEditor && UObjectInitialized())
		{
			FMediaProfileCommands::Unregister();
			FMediaProfileMenuEntry::Unregister();
			SMediaFrameworkCapture::UnregisterNomadTabSpawner();

			FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomClassLayout("MediaBundleActorBase");

			// Unregister AssetTypeActions
			FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");

			if (AssetToolsModule != nullptr)
			{
				IAssetTools& AssetTools = AssetToolsModule->Get();

				for (auto Action : RegisteredAssetTypeActions)
				{
					AssetTools.UnregisterAssetTypeActions(Action);
				}
			}

			FMediaFrameworkUtilitiesPlacement::UnregisterPlacement();

			GEditor->ActorFactories.RemoveAll([](const UActorFactory* ActorFactory) { return ActorFactory->IsA<UActorFactoryMediaBundle>(); });

			FMediaFrameworkUtilitiesEditorStyle::Unregister();
		}
	}

private:

	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;
};


IMPLEMENT_MODULE(FMediaFrameworkUtilitiesEditorModule, MediaFrameworkUtilitiesEditor);


#undef LOCTEXT_NAMESPACE
