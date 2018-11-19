// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "USDImporterPrivatePCH.h"
#include "Misc/Paths.h"
#include "UObject/ObjectMacros.h"
#include "UObject/GCObject.h"
#include "USDImporter.h"
#include "ISettingsModule.h"
#include "PropertyEditorModule.h"
#include "USDLevelInfoDetails.h"
#include "Modules/ModuleManager.h"
#include "USDImporterProjectSettings.h"

#define LOCTEXT_NAMESPACE "USDImportPlugin"

class FUSDImporterModule : public IUSDImporterModule, public FGCObject
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{

		// Ensure base usd plugins are found and loaded
		FString BasePluginPath = FPaths::ConvertRelativePathToFull(FPaths::EnginePluginsDir() + FString(TEXT("Editor/USDImporter")));

#if PLATFORM_WINDOWS
		BasePluginPath /= TEXT("Resources/UsdResources/Windows/plugins");
#elif PLATFORM_LINUX
		BasePluginPath /= ("Resources/UsdResources/Linux/plugins");
#endif

		std::vector<std::string> PluginPaths;
		PluginPaths.push_back(TCHAR_TO_ANSI(*BasePluginPath));

		// Load any custom plugins the user may have
		const TArray<FDirectoryPath>& AdditionalPluginDirectories = GetDefault<UUSDImporterProjectSettings>()->AdditionalPluginDirectories;

		for (const FDirectoryPath& Directory : AdditionalPluginDirectories)
		{
			if (!Directory.Path.IsEmpty())
			{
				PluginPaths.push_back(TCHAR_TO_ANSI(*Directory.Path));
			}
		}

		UnrealUSDWrapper::Initialize(PluginPaths);

		USDImporter = NewObject<UUSDImporter>();

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(TEXT("USDLevelInfo"), FOnGetDetailCustomizationInstance::CreateStatic(&FUSDLevelInfoDetails::MakeInstance));
		PropertyModule.NotifyCustomizationModuleChanged();
	}

	virtual void ShutdownModule() override
	{
		FPropertyEditorModule* PropertyEditorModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
		if (PropertyEditorModule)
		{
			PropertyEditorModule->UnregisterCustomClassLayout(TEXT("USDLevelInfo"));
			PropertyEditorModule->NotifyCustomizationModuleChanged();
		}

		USDImporter = nullptr;
	}

	class UUSDImporter* GetImporter() override
	{
		return USDImporter;
	}

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(USDImporter);
	}
private:
	UUSDImporter* USDImporter;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUSDImporterModule, USDImporter)

