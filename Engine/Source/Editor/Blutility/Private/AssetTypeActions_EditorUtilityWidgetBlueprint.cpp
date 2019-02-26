// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_EditorUtilityWidgetBlueprint.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/PackageName.h"
#include "Misc/MessageDialog.h"
#include "EditorUtilityBlueprintFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BlueprintEditorModule.h"
#include "EditorUtilityDialog.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "EditorUtilityWidget.h"
#include "WidgetBlueprintEditor.h"
#include "LevelEditor.h"
#include "Editor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "IBlutilityModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

/////////////////////////////////////////////////////
// FAssetTypeActions_EditorUtilityWidget

FText FAssetTypeActions_EditorUtilityWidgetBlueprint::GetName() const
{
	return LOCTEXT("AssetTypeActions_EditorUtilityWidget", "Editor Widget");
}

FColor FAssetTypeActions_EditorUtilityWidgetBlueprint::GetTypeColor() const
{
	return FColor(0, 169, 255);
}

UClass* FAssetTypeActions_EditorUtilityWidgetBlueprint::GetSupportedClass() const
{
	return UEditorUtilityWidgetBlueprint::StaticClass();
}

bool FAssetTypeActions_EditorUtilityWidgetBlueprint::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}

void FAssetTypeActions_EditorUtilityWidgetBlueprint::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	auto Blueprints = GetTypedWeakObjectPtrs<UWidgetBlueprint>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("EditorUtilityWidget_Edit", "Run Editor Utility Widget"),
		LOCTEXT("EditorUtilityWidget_EditTooltip", "Runs the single action or opens the tab built by this Editor Utility Widget Blueprint."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_EditorUtilityWidgetBlueprint::ExecuteRun, Blueprints),
			FCanExecuteAction()
		)
	);

}

void FAssetTypeActions_EditorUtilityWidgetBlueprint::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (UObject* Object : InObjects)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(Object);
		if (Blueprint && Blueprint->SkeletonGeneratedClass && Blueprint->GeneratedClass)
		{
			TSharedRef< FWidgetBlueprintEditor > NewBlueprintEditor(new FWidgetBlueprintEditor());

			TArray<UBlueprint*> Blueprints;
			Blueprints.Add(Blueprint);
			NewBlueprintEditor->InitWidgetBlueprintEditor(EToolkitMode::Standalone, nullptr, Blueprints, true);
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToLoadEditorUtilityWidgetBlueprint", "Editor Utility Widget could not be loaded because it derives from an invalid class.\nCheck to make sure the parent class for this blueprint hasn't been removed!"));
		}
	}
}

uint32 FAssetTypeActions_EditorUtilityWidgetBlueprint::GetCategories()
{
	IBlutilityModule* BlutilityModule = FModuleManager::GetModulePtr<IBlutilityModule>("Blutility");
	return BlutilityModule->GetAssetCategory();
}

void FAssetTypeActions_EditorUtilityWidgetBlueprint::ExecuteRun(FWeakBlueprintPointerArray InObjects)
{
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(*ObjIt))
		{
			if (Blueprint->GeneratedClass->IsChildOf(UEditorUtilityWidget::StaticClass()))
			{
				const UEditorUtilityWidget* CDO = Blueprint->GeneratedClass->GetDefaultObject<UEditorUtilityWidget>();
				if (CDO->ShouldAutoRunDefaultAction())
				{
					// This is an instant-run blueprint, just execute it
					UEditorUtilityWidget* Instance = NewObject<UEditorUtilityWidget>(GetTransientPackage(), Blueprint->GeneratedClass);
					Instance->ExecuteDefaultAction();
				}
				else
				{
					FName RegistrationName = FName(*(Blueprint->GetPathName() + LOCTEXT("ActiveTabSuffix", "_ActiveTab").ToString()));
					FText DisplayName = FText::FromString(Blueprint->GetName());
					FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
					TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
					if (!LevelEditorTabManager->CanSpawnTab(RegistrationName))
					{
						IBlutilityModule* BlutilityModule = FModuleManager::GetModulePtr<IBlutilityModule>("Blutility");
						UEditorUtilityWidgetBlueprint* WidgetBlueprint = Cast<UEditorUtilityWidgetBlueprint>(Blueprint);
						WidgetBlueprint->SetRegistrationName(RegistrationName);
						LevelEditorTabManager->RegisterTabSpawner(RegistrationName, FOnSpawnTab::CreateUObject(WidgetBlueprint, &UEditorUtilityWidgetBlueprint::SpawnEditorUITab))
							.SetDisplayName(DisplayName)
							.SetGroup(BlutilityModule->GetMenuGroup().ToSharedRef());
						BlutilityModule->AddLoadedScriptUI(WidgetBlueprint);
					}
					TSharedRef<SDockTab> NewDockTab = LevelEditorTabManager->InvokeTab(RegistrationName);

				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
