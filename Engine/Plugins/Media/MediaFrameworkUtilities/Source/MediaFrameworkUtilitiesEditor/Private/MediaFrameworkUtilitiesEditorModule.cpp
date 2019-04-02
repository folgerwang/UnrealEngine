// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "MediaFrameworkUtilitiesEditorModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "UObject/UObjectBase.h"

#include "AssetEditor/MediaProfileCommands.h"
#include "AssetTypeActions/AssetTypeActions_MediaBundle.h"
#include "AssetTypeActions/AssetTypeActions_MediaProfile.h"
#include "CaptureTab/SMediaFrameworkCapture.h"
#include "MediaBundleActorDetails.h"
#include "MediaBundleFactoryNew.h"
#include "MediaFrameworkUtilitiesPlacement.h"
#include "VideoInputTab/SMediaFrameworkVideoInput.h"
#include "UI/MediaFrameworkUtilitiesEditorStyle.h"
#include "UI/MediaProfileMenuEntry.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"


#define LOCTEXT_NAMESPACE "MediaFrameworkEditor"

DEFINE_LOG_CATEGORY(LogMediaFrameworkUtilitiesEditor);

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
			FMediaProfileCommands::Register();
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

			{
				const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
				TSharedRef<FWorkspaceItem> MediaBrowserGroup = MenuStructure.GetDeveloperToolsMiscCategory()->GetParent()->AddGroup(
					LOCTEXT("WorkspaceMenu_MediaCategory", "Media"),
					FSlateIcon(),
					true);

				SMediaFrameworkCapture::RegisterNomadTabSpawner(MediaBrowserGroup);
				SMediaFrameworkVideoInput::RegisterNomadTabSpawner(MediaBrowserGroup);
			}
			FMediaProfileMenuEntry::Register();
		}
	}

	virtual void ShutdownModule() override
	{
		if (!GIsRequestingExit && GEditor && UObjectInitialized())
		{
			FMediaProfileMenuEntry::Unregister();
			SMediaFrameworkVideoInput::UnregisterNomadTabSpawner();
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
			FMediaProfileCommands::Unregister();
		}
	}

private:

	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;
};


IMPLEMENT_MODULE(FMediaFrameworkUtilitiesEditorModule, MediaFrameworkUtilitiesEditor);


#undef LOCTEXT_NAMESPACE
