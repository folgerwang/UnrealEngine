// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ShotgunUIManager.h"
#include "ShotgunEngine.h"
#include "ShotgunStyle.h"

#include "AssetData.h"
#include "ContentBrowserModule.h"
#include "LevelEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#include "IPythonScriptPlugin.h"

#define LOCTEXT_NAMESPACE "Shotgun"
#define LEVELEDITOR_MODULE_NAME TEXT("LevelEditor")
#define CONTENTBROWSER_MODULE_NAME TEXT("ContentBrowser")

TUniquePtr<FShotgunUIManagerImpl> FShotgunUIManager::Instance;

class FShotgunUIManagerImpl
{
public:
	void Initialize();
	void Shutdown();

private:
	FDelegateHandle ContentBrowserAssetExtenderDelegateHandle;
	FDelegateHandle LevelEditorExtenderDelegateHandle;

	bool bIsShotgunEnabled;

	void SetupShotgunMenu();
	void SetupShotgunContextMenus();
	void RemoveShotgunContextMenus();

	void FillToolbar(FToolBarBuilder& ToolbarBuilder);
	TSharedRef<SWidget> GenerateShotgunToolbarMenu();
	void GenerateShotgunMenuContent(FMenuBuilder& MenuBuilder, const TArray<FAssetData>* SelectedAssets, const TArray< AActor*>* SelectedActors);
	void GenerateShotgunAssetContextMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets);
	void GenerateShotgunActorContextMenu(FMenuBuilder& MenuBuilder, TArray< AActor*> SelectedActors);

	// Menu extender callbacks
	TSharedRef<FExtender> OnExtendLevelEditor(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> SelectedActors);
	TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets);
};

void FShotgunUIManagerImpl::Initialize()
{
	bIsShotgunEnabled = false;

	// Check if the bootstrap environment variable is set and that the script exists
	FString ShotgunBootstrap = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_SHOTGUN_BOOTSTRAP"));
	if (!ShotgunBootstrap.IsEmpty() && FPaths::FileExists(ShotgunBootstrap))
	{
		// The following environment variables must be set for the Shotgun apps to be fully functional
		// These variables are automatically set when the editor is launched through Shotgun Desktop
		FString ShotgunEngine = FPlatformMisc::GetEnvironmentVariable(TEXT("SHOTGUN_ENGINE"));
		FString ShotgunEntityType = FPlatformMisc::GetEnvironmentVariable(TEXT("SHOTGUN_ENTITY_TYPE"));
		FString ShotgunEntityId = FPlatformMisc::GetEnvironmentVariable(TEXT("SHOTGUN_ENTITY_ID"));

		if (ShotgunEngine == TEXT("tk-unreal") && !ShotgunEntityType.IsEmpty() && !ShotgunEntityId.IsEmpty())
		{
			bIsShotgunEnabled = true;

			// Set environment variable in the Python interpreter to enable the Shotgun Unreal init script
			IPythonScriptPlugin::Get()->ExecPythonCommand(TEXT("import os\nos.environ['UE_SHOTGUN_ENABLED']='True'"));
		}
	}

	if (bIsShotgunEnabled)
	{
		FShotgunStyle::Initialize();

		SetupShotgunMenu();
		SetupShotgunContextMenus();
	}
}

void FShotgunUIManagerImpl::Shutdown()
{
	if (bIsShotgunEnabled)
	{
		RemoveShotgunContextMenus();

		FShotgunStyle::Shutdown();
	}
}

void FShotgunUIManagerImpl::SetupShotgunMenu()
{
	// Set the Shotgun icons
	FShotgunStyle::SetIcon("Logo", "sg_logo_80px");
	FShotgunStyle::SetIcon("ContextLogo", "sg_context_logo");

	// Add a Shotgun toolbar section after the settings section of the level editor
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(LEVELEDITOR_MODULE_NAME);

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::After, nullptr, FToolBarExtensionDelegate::CreateRaw(this, &FShotgunUIManagerImpl::FillToolbar));

	LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
}

void FShotgunUIManagerImpl::SetupShotgunContextMenus()
{
	// Register Content Browser menu extender
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(CONTENTBROWSER_MODULE_NAME);

	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBAssetMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
	CBAssetMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateRaw(this, &FShotgunUIManagerImpl::OnExtendContentBrowserAssetSelectionMenu));
	ContentBrowserAssetExtenderDelegateHandle = CBAssetMenuExtenderDelegates.Last().GetHandle();

	// Register Level Editor menu extender
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(LEVELEDITOR_MODULE_NAME);

	TArray<FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors>& LevelEditorMenuExtenderDelegates = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();
	LevelEditorMenuExtenderDelegates.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateRaw(this, &FShotgunUIManagerImpl::OnExtendLevelEditor));
	LevelEditorExtenderDelegateHandle = LevelEditorMenuExtenderDelegates.Last().GetHandle();
}

void FShotgunUIManagerImpl::RemoveShotgunContextMenus()
{
	if (FModuleManager::Get().IsModuleLoaded(LEVELEDITOR_MODULE_NAME))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(LEVELEDITOR_MODULE_NAME);
		TArray<FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors>& LevelEditorMenuExtenderDelegates = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();
		LevelEditorMenuExtenderDelegates.RemoveAll([this](const FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors& Delegate) { return Delegate.GetHandle() == LevelEditorExtenderDelegateHandle; });
	}

	if (FModuleManager::Get().IsModuleLoaded(CONTENTBROWSER_MODULE_NAME))
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>(CONTENTBROWSER_MODULE_NAME);
		TArray<FContentBrowserMenuExtender_SelectedAssets>& CBAssetMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
		CBAssetMenuExtenderDelegates.RemoveAll([this](const FContentBrowserMenuExtender_SelectedAssets& Delegate) { return Delegate.GetHandle() == ContentBrowserAssetExtenderDelegateHandle; });
	}
}

void FShotgunUIManagerImpl::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection(TEXT("Shotgun"));
	{
		// Add a drop-down menu (with a label and an icon for the drop-down button) to list the Shotgun actions available
		ToolbarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateRaw(this, &FShotgunUIManagerImpl::GenerateShotgunToolbarMenu),
			LOCTEXT("ShotgunCombo_Label", "Shotgun"),
			LOCTEXT("ShotgunCombo_Tooltip", "Available Shotgun commands"),
			FSlateIcon(FShotgunStyle::GetStyleSetName(), "Shotgun.Logo")
		);
	}
	ToolbarBuilder.EndSection();
}

TSharedRef<SWidget> FShotgunUIManagerImpl::GenerateShotgunToolbarMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	GenerateShotgunMenuContent(MenuBuilder, nullptr, nullptr);

	return MenuBuilder.MakeWidget();
}

void FShotgunUIManagerImpl::GenerateShotgunMenuContent(FMenuBuilder& MenuBuilder, const TArray<FAssetData>* SelectedAssets, const TArray< AActor*>* SelectedActors)
{
	if (UShotgunEngine* Engine = UShotgunEngine::GetInstance())
	{
		Engine->SetSelection(SelectedAssets, SelectedActors);

		// Query the available Shotgun commands from the Shotgun engine
		TArray<FShotgunMenuItem> MenuItems = Engine->GetShotgunMenuItems();
		for (const FShotgunMenuItem& MenuItem : MenuItems)
		{
			if (MenuItem.Type == TEXT("context_begin"))
			{
				MenuBuilder.BeginSection(NAME_None, FText::FromString(MenuItem.Title));
			}
			else if (MenuItem.Type == TEXT("context_end"))
			{
				MenuBuilder.EndSection();
			}
			else if (MenuItem.Type == TEXT("separator"))
			{
				MenuBuilder.AddMenuSeparator();
			}
			else
			{
				// The other menu types correspond to actual Shotgun commands with an associated action
				FString CommandName = MenuItem.Title;
				MenuBuilder.AddMenuEntry(
					FText::FromString(CommandName),
					FText::FromString(MenuItem.Description),
					FSlateIcon(),
					FExecuteAction::CreateLambda([CommandName]()
					{
						if (UShotgunEngine* Engine = UShotgunEngine::GetInstance())
						{
							Engine->ExecuteCommand(CommandName);
						}
					})
				);
			}
		}
	}
}

void FShotgunUIManagerImpl::GenerateShotgunAssetContextMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
{
	GenerateShotgunMenuContent(MenuBuilder, &SelectedAssets, nullptr);
}

void FShotgunUIManagerImpl::GenerateShotgunActorContextMenu(FMenuBuilder& MenuBuilder, TArray<AActor*> SelectedActors)
{
	GenerateShotgunMenuContent(MenuBuilder, nullptr, &SelectedActors);
}

TSharedRef<FExtender> FShotgunUIManagerImpl::OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
	// Menu extender for Content Browser context menu when an asset is selected
	TSharedRef<FExtender> Extender(new FExtender());

	if (SelectedAssets.Num() > 0)
	{
		Extender->AddMenuExtension("AssetContextReferences", EExtensionHook::After, nullptr, FMenuExtensionDelegate::CreateLambda(
			[this, SelectedAssets](FMenuBuilder& MenuBuilder)
			{
				MenuBuilder.AddMenuSeparator();
				MenuBuilder.AddSubMenu(
					LOCTEXT("Shotgun_ContextMenu", "Shotgun"),
					FText(),
					FNewMenuDelegate::CreateRaw(this, &FShotgunUIManagerImpl::GenerateShotgunAssetContextMenu, SelectedAssets),
					false,
					FSlateIcon(FShotgunStyle::GetStyleSetName(), "Shotgun.ContextLogo")
				);
			}));
	}

	return Extender;
}

TSharedRef<FExtender> FShotgunUIManagerImpl::OnExtendLevelEditor(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> SelectedActors)
{
	// Menu extender for Level Editor and World Outliner context menus when an actor is selected
	TSharedRef<FExtender> Extender(new FExtender());

	if (SelectedActors.Num() > 0)
	{
		Extender->AddMenuExtension("LevelViewportAttach", EExtensionHook::After, nullptr, FMenuExtensionDelegate::CreateLambda(
			[this, SelectedActors](FMenuBuilder& MenuBuilder)
			{
				MenuBuilder.AddMenuSeparator();
				MenuBuilder.AddSubMenu(
					LOCTEXT("Shotgun_ContextMenu", "Shotgun"),
					FText(),
					FNewMenuDelegate::CreateRaw(this, &FShotgunUIManagerImpl::GenerateShotgunActorContextMenu, SelectedActors),
					false,
					FSlateIcon(FShotgunStyle::GetStyleSetName(), "Shotgun.ContextLogo")
				);
			}));
	}

	return Extender;
}

void FShotgunUIManager::Initialize()
{
	if (!Instance.IsValid())
	{
		Instance = MakeUnique<FShotgunUIManagerImpl>();
		Instance->Initialize();
	}
}

void FShotgunUIManager::Shutdown()
{
	if (Instance.IsValid())
	{
		Instance->Shutdown();
		Instance.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
