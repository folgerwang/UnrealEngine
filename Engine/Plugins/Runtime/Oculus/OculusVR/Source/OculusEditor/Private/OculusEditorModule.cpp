#include "OculusEditorModule.h"
#include "OculusToolStyle.h"
#include "OculusToolCommands.h"
#include "OculusToolWidget.h"
#include "OculusAssetDirectory.h"
#include "OculusHMDRuntimeSettings.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ISettingsModule.h"

#define LOCTEXT_NAMESPACE "OculusEditor"

const FName FOculusEditorModule::OculusPerfTabName = FName("OculusPerfCheck");

void FOculusEditorModule::StartupModule()
{
	RegisterSettings();
	FOculusAssetDirectory::LoadForCook();

	if(!IsRunningCommandlet())
	{
		FOculusToolStyle::Initialize();
		FOculusToolStyle::ReloadTextures();

		FOculusToolCommands::Register();

		PluginCommands = MakeShareable(new FUICommandList);

		PluginCommands->MapAction(
			FOculusToolCommands::Get().OpenPluginWindow,
			FExecuteAction::CreateRaw(this, &FOculusEditorModule::PluginButtonClicked),
			FCanExecuteAction());

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		{
			TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
			MenuExtender->AddMenuExtension("Miscellaneous", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FOculusEditorModule::AddMenuExtension));
			LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
		}

		// If we want a toolbar icon we can uncomment this. Leaving it nice and low key right now.
		/*
		{
			TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
			ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::After, PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FOculusEditorModule::AddToolbarExtension));
			LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
		}*/


		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(OculusPerfTabName, FOnSpawnTab::CreateRaw(this, &FOculusEditorModule::OnSpawnPluginTab))
			.SetDisplayName(LOCTEXT("FOculusEditorTabTitle", "Oculus Performance Check"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);
	 }
}

void FOculusEditorModule::ShutdownModule()
{
	if(!IsRunningCommandlet())
	{
		FOculusToolStyle::Shutdown();
		FOculusToolCommands::Unregister();
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(OculusPerfTabName);
	}

	FOculusAssetDirectory::ReleaseAll();
	if (UObjectInitialized())
	{
		UnregisterSettings();  
	}		
}

TSharedRef<SDockTab> FOculusEditorModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	auto myTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SOculusToolWidget)
		];


	return myTab;
}

void FOculusEditorModule::RegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "OculusVR",
			LOCTEXT("RuntimeSettingsName", "OculusVR"),
			LOCTEXT("RuntimeSettingsDescription", "Configure the OculusVR plugin"),
			GetMutableDefault<UOculusHMDRuntimeSettings>()
		);
		SettingsModule->RegisterSettings("Project", "Plugins", "OculusVR",
			LOCTEXT("RuntimeSettingsName", "OculusVR"),
			LOCTEXT("RuntimeSettingsDescription", "Configure the OculusVR plugin"),
			GetMutableDefault<UOculusHMDRuntimeSettings>()
		);
	}
}

void FOculusEditorModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "OculusVR");
	}
}


void FOculusEditorModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->InvokeTab(OculusPerfTabName);
}

void FOculusEditorModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FOculusToolCommands::Get().OpenPluginWindow);
}

void FOculusEditorModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FOculusToolCommands::Get().OpenPluginWindow);
}
	
//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FOculusEditorModule, OculusEditor);

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE