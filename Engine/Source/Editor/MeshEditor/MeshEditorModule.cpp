// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MeshEditorModule.h"
#include "IMeshEditorModule.h"
#include "EditorModeRegistry.h"
#include "MeshEditorMode.h"
#include "MeshEditorStyle.h"
#include "EditorModeManager.h"
#include "ISettingsModule.h"
#include "MeshEditorSettings.h"

#define LOCTEXT_NAMESPACE "MeshEditor"

class FMeshEditorModule : public IMeshEditorModule
{
public:
	FMeshEditorModule()
	{
	}

	// FModuleInterface overrides
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual void PostLoadCallback() override;
	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}

	static void ToggleMeshEditorMode();

	virtual IMeshEditorMode* GetLevelEditorMeshEditorMode() override
	{
		return static_cast<IMeshEditorMode*>(GLevelEditorModeTools().GetActiveModeTyped<FMeshEditorMode>("MeshEditor"));
	}
};


namespace MeshEd
{
	// @todo mesheditor: get rid of this some day
	static FAutoConsoleCommand ToggleMeshEditorMode( TEXT( "MeshEd.MeshEditorMode" ), TEXT( "Toggles Mesh Editor Mode" ), FConsoleCommandDelegate::CreateStatic( &FMeshEditorModule::ToggleMeshEditorMode ) );
}



void FMeshEditorModule::StartupModule()
{
	FMeshEditorStyle::Initialize();

	FEditorModeRegistry::Get().RegisterMode<FMeshEditorMode>(
		IMeshEditorModule::GetEditorModeID(),
		LOCTEXT( "ModeName", "Mesh Editor" ),
		FSlateIcon( FMeshEditorStyle::GetStyleSetName(), "LevelEditor.MeshEditorMode", "LevelEditor.MeshEditorMode.Small" ),
		true,
		600
		);
//	GLevelEditorModeTools().AddDefaultMode( MeshEditorModeID );

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>( "Settings" );
	if( SettingsModule )
	{
		// Designer settings
		SettingsModule->RegisterSettings( "Editor", "ContentEditors", "MeshEditor",
										  LOCTEXT("MeshEditorSettingsName", "Mesh Editor"),
										  LOCTEXT("MeshEditorSettingsDescription", "Configure options for the Mesh Editor."),
										  GetMutableDefault<UMeshEditorSettings>()
		);
	}
}


void FMeshEditorModule::ShutdownModule()
{
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>( "Settings" );
	if( SettingsModule )
	{
		SettingsModule->UnregisterSettings( "Editor", "ContentEditors", "MeshEditor" );
	}

	GLevelEditorModeTools().RemoveDefaultMode(IMeshEditorModule::GetEditorModeID());
	FEditorModeRegistry::Get().UnregisterMode(IMeshEditorModule::GetEditorModeID());

	FMeshEditorStyle::Shutdown();
}


void FMeshEditorModule::PostLoadCallback()
{
	GLevelEditorModeTools().ActivateDefaultMode();
}


void FMeshEditorModule::ToggleMeshEditorMode()
{
	if( GLevelEditorModeTools().IsModeActive(IMeshEditorModule::GetEditorModeID()) )
	{
		// Shut off Mesh Editor Mode
		GLevelEditorModeTools().RemoveDefaultMode(IMeshEditorModule::GetEditorModeID());
		GLevelEditorModeTools().DeactivateMode(IMeshEditorModule::GetEditorModeID());
	}
	else
	{
		// Activate the mode right away.  We expect it to stay active forever!
		GLevelEditorModeTools().AddDefaultMode(IMeshEditorModule::GetEditorModeID());
		GLevelEditorModeTools().ActivateMode(IMeshEditorModule::GetEditorModeID());
	}
}


IMPLEMENT_MODULE( FMeshEditorModule, MeshEditor )

#undef LOCTEXT_NAMESPACE
