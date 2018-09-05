// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/Docking/TabManager.h"
#include "IBlutilityModule.h"
#include "EditorUtilityBlueprint.h"


#include "AssetToolsModule.h"
#include "PropertyEditorModule.h"
#include "AssetTypeActions_EditorUtilityBlueprint.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#include "BlutilityDetailsPanel.h"
#include "BlutilityShelf.h"
#include "Widgets/Docking/SDockTab.h"
#include "BlutilityContentBrowserExtensions.h"
#include "BlutilityLevelEditorExtensions.h"
#include "AssetTypeActions_EditorUtilityWidgetBlueprint.h"
#include "KismetCompiler.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "ComponentReregisterContext.h"
#include "KismetCompilerModule.h"
#include "WidgetBlueprintCompiler.h"
#include "UMGEditorModule.h"
#include "EditorUtilityContext.h"
#include "LevelEditor.h"
#include "Editor.h"
#include "UnrealEdMisc.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"

/////////////////////////////////////////////////////

namespace BlutilityModule
{
	static const FName BlutilityShelfApp = FName(TEXT("BlutilityShelfApp"));
}

/////////////////////////////////////////////////////
// FBlutilityModule 

// Blutility module implementation (private)
class FBlutilityModule : public IBlutilityModule, public FGCObject
{
public:
	/** Asset type actions for blutility assets.  Cached here so that we can unregister it during shutdown. */
	TSharedPtr<FAssetTypeActions_EditorUtilityBlueprint> EditorBlueprintAssetTypeActions;
	TSharedPtr<FAssetTypeActions_EditorUtilityWidgetBlueprint> EditorWidgetBlueprintAssetTypeActions;

public:
	virtual void StartupModule() override
	{
		// Register the asset type
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		EditorUtilityAssetCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("EditorUtilities")), LOCTEXT("EditorUtilitiesAssetCategory", "Editor Utilities"));
		EditorBlueprintAssetTypeActions = MakeShareable(new FAssetTypeActions_EditorUtilityBlueprint);
		AssetTools.RegisterAssetTypeActions(EditorBlueprintAssetTypeActions.ToSharedRef());
		EditorWidgetBlueprintAssetTypeActions = MakeShareable(new FAssetTypeActions_EditorUtilityWidgetBlueprint);
		AssetTools.RegisterAssetTypeActions(EditorWidgetBlueprintAssetTypeActions.ToSharedRef());
		
		// Register the details customizer
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout("PlacedEditorUtilityBase", FOnGetDetailCustomizationInstance::CreateStatic(&FEditorUtilityInstanceDetails::MakeInstance));
		PropertyModule.RegisterCustomClassLayout("GlobalEditorUtilityBase", FOnGetDetailCustomizationInstance::CreateStatic(&FEditorUtilityInstanceDetails::MakeInstance));
		PropertyModule.NotifyCustomizationModuleChanged();

		FGlobalTabmanager::Get()->RegisterTabSpawner(BlutilityModule::BlutilityShelfApp, FOnSpawnTab::CreateStatic(&SpawnBlutilityShelfTab))
			.SetDisplayName(NSLOCTEXT("BlutilityShelf", "TabTitle", "Blutility Shelf"))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());
		FKismetCompilerContext::RegisterCompilerForBP(UEditorUtilityWidgetBlueprint::StaticClass(), &UWidgetBlueprint::GetCompilerForWidgetBP);

		// Register widget blueprint compiler we do this no matter what.
		IUMGEditorModule& UMGEditorModule = FModuleManager::LoadModuleChecked<IUMGEditorModule>("UMGEditor");
		IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
		KismetCompilerModule.GetCompilers().Add(UMGEditorModule.GetRegisteredCompiler());

		FBlutilityContentBrowserExtensions::InstallHooks();
		FBlutilityLevelEditorExtensions::InstallHooks();

		ScriptedEditorWidgetsGroup = WorkspaceMenu::GetMenuStructure().GetToolsCategory()->AddGroup(
			LOCTEXT("WorkspaceMenu_EditorUtilityWidgetsGroup", "Editor Utility Widgets"),
			LOCTEXT("ScriptedEditorWidgetsGroupTooltipText", "Custom editor UI created with Blueprints or Python."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "WorkspaceMenu.AdditionalUI"),
			true);

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		LevelEditorModule.OnTabManagerChanged().AddRaw(this, &FBlutilityModule::ReinitializeUIs);
		LevelEditorModule.OnMapChanged().AddRaw(this, &FBlutilityModule::OnMapChanged);
	}

	void ReinitializeUIs()
	{
		EditorUtilityContext = NewObject<UEditorUtilityContext>();
		if (EditorUtilityContext)
		{
			for (FSoftObjectPath BlueprintPath : EditorUtilityContext->LoadedUIs)
			{ 
				UObject* BlueprintObject = BlueprintPath.TryLoad();
				if (BlueprintObject)
				{
					UEditorUtilityWidgetBlueprint* Blueprint = Cast<UEditorUtilityWidgetBlueprint>(BlueprintObject);
					const UEditorUtilityWidget* CDO = Blueprint->GeneratedClass->GetDefaultObject<UEditorUtilityWidget>();
					FName RegistrationName = FName(*CDO->GetPathName());
					FText DisplayName = FText::FromString(Blueprint->GetName());
					FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
					TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
					if (LevelEditorTabManager && !LevelEditorTabManager->CanSpawnTab(RegistrationName))
					{
						LevelEditorTabManager->RegisterTabSpawner(RegistrationName, FOnSpawnTab::CreateUObject(Blueprint, &UEditorUtilityWidgetBlueprint::SpawnEditorUITab))
							.SetDisplayName(DisplayName)
							.SetGroup(GetMenuGroup().ToSharedRef());
					}
				}
			}
		}
	}

	void OnMapChanged(UWorld* World, EMapChangeType MapChangeType)
	{
		if (EditorUtilityContext)
		{
			for (FSoftObjectPath LoadedUI : EditorUtilityContext->LoadedUIs)
			{
				UEditorUtilityWidgetBlueprint* LoadedEditorUtilityBlueprint = Cast<UEditorUtilityWidgetBlueprint>(LoadedUI.ResolveObject());
				if (LoadedEditorUtilityBlueprint)
				{
					UEditorUtilityWidget* CreatedWidget = LoadedEditorUtilityBlueprint->GetCreatedWidget();
					if (CreatedWidget)
					{
						if (MapChangeType == EMapChangeType::TearDownWorld)
						{
							CreatedWidget->Rename(*CreatedWidget->GetName(), GetTransientPackage());
						}
						else if (MapChangeType == EMapChangeType::LoadMap || MapChangeType == EMapChangeType::NewMap)
						{
							UWorld* World = GEditor->GetEditorWorldContext().World();
							check(World);
							CreatedWidget->Rename(*CreatedWidget->GetName(), World);
						}
					}
				}
			}
		}
	}

	virtual void ShutdownModule() override
	{
		if (!UObjectInitialized())
		{
			return;
		}

		// Unregister widget blueprint compiler we do this no matter what.
		IUMGEditorModule& UMGEditorModule = FModuleManager::LoadModuleChecked<IUMGEditorModule>("UMGEditor");
		IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
		KismetCompilerModule.GetCompilers().Remove(UMGEditorModule.GetRegisteredCompiler());

		FBlutilityLevelEditorExtensions::RemoveHooks();
		FBlutilityContentBrowserExtensions::RemoveHooks();

		FGlobalTabmanager::Get()->UnregisterTabSpawner(BlutilityModule::BlutilityShelfApp);

		// Only unregister if the asset tools module is loaded.  We don't want to forcibly load it during shutdown phase.
		check( EditorBlueprintAssetTypeActions.IsValid() );
		if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			AssetTools.UnregisterAssetTypeActions(EditorBlueprintAssetTypeActions.ToSharedRef());
			AssetTools.UnregisterAssetTypeActions(EditorWidgetBlueprintAssetTypeActions.ToSharedRef());
		}
		EditorBlueprintAssetTypeActions.Reset();
		EditorWidgetBlueprintAssetTypeActions.Reset();

		// Unregister the details customization
		if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomClassLayout("PlacedEditorUtilityBase");
			PropertyModule.UnregisterCustomClassLayout("GlobalEditorUtilityBase");
			PropertyModule.NotifyCustomizationModuleChanged();
		}
	}

	virtual bool IsBlutility( const UBlueprint* Blueprint ) const override
	{
		const UClass* BPClass = Blueprint ? Blueprint->GetClass() : nullptr;

		if( BPClass && BPClass->IsChildOf( UEditorUtilityBlueprint::StaticClass() ))
		{
			return true;
		}
		return false;
	}

	virtual TSharedPtr<class FWorkspaceItem> GetMenuGroup() const override
	{
		return ScriptedEditorWidgetsGroup;
	}

	virtual EAssetTypeCategories::Type GetAssetCategory() const override
	{
		return EditorUtilityAssetCategory;
	}

	virtual void AddLoadedScriptUI(class UEditorUtilityWidgetBlueprint* InBlueprint) override
	{
		if (EditorUtilityContext)
		{
			EditorUtilityContext->LoadedUIs.Add(InBlueprint);
			EditorUtilityContext->SaveConfig();
		}
	}


	virtual void RemoveLoadedScriptUI(class UEditorUtilityWidgetBlueprint* InBlueprint) override
	{
		if (EditorUtilityContext)
		{
			EditorUtilityContext->LoadedUIs.Remove(InBlueprint);
			EditorUtilityContext->SaveConfig();
		}
	}

protected:
	static TSharedRef<SDockTab> SpawnBlutilityShelfTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SBlutilityShelf)
			];
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		if (EditorUtilityContext)
		{
			Collector.AddReferencedObject(EditorUtilityContext);
		}
	}

	/** Scripted Editor Widgets workspace menu item */
	TSharedPtr<class FWorkspaceItem> ScriptedEditorWidgetsGroup;

	EAssetTypeCategories::Type EditorUtilityAssetCategory;

	UEditorUtilityContext* EditorUtilityContext;
};



IMPLEMENT_MODULE( FBlutilityModule, Blutility );
#undef LOCTEXT_NAMESPACE 