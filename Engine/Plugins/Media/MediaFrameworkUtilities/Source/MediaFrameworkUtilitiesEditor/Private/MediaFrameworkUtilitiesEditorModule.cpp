// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "UObject/UObjectBase.h"

#include "MediaBundleActorDetails.h"
#include "MediaBundleFactoryNew.h"
#include "MediaFrameworkUtilitiesPlacement.h"

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
			GEditor->ActorFactories.Add(NewObject<UActorFactoryMediaBundle>());
		}
		FMediaFrameworkUtilitiesPlacement::RegisterPlacement();

		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout("MediaBundleActorBase", FOnGetDetailCustomizationInstance::CreateStatic(&FMediaBundleActorDetails::MakeInstance));
	}

	virtual void ShutdownModule() override
	{
		if (!GIsRequestingExit && GEditor && UObjectInitialized())
		{
			GEditor->ActorFactories.RemoveAll([](const UActorFactory* ActorFactory) { return ActorFactory->IsA<UActorFactoryMediaBundle>(); });
			FMediaFrameworkUtilitiesPlacement::UnregisterPlacement();

			FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomClassLayout("MediaBundleActorBase");
		}
	}
};


IMPLEMENT_MODULE(FMediaFrameworkUtilitiesEditorModule, MediaFrameworkUtilitiesEditorModule);


#undef LOCTEXT_NAMESPACE
