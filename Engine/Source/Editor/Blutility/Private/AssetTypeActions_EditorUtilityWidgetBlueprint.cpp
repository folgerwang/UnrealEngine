// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
		LOCTEXT("Blutility_Edit", "Edit Blueprint"),
		LOCTEXT("Blutility_EditTooltip", "Opens the selected blueprints in the full blueprint editor."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_EditorUtilityWidgetBlueprint::ExecuteEdit, Blueprints),
			FCanExecuteAction()
		)
	);

}

void FAssetTypeActions_EditorUtilityWidgetBlueprint::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

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
					FName RegistrationName = FName(*CDO->GetPathName());
					FText DisplayName = FText::FromString(Blueprint->GetName());
					FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
					TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
					if (!LevelEditorTabManager->CanSpawnTab(RegistrationName))
					{
						IBlutilityModule* BlutilityModule = FModuleManager::GetModulePtr<IBlutilityModule>("Blutility");
						UEditorUtilityWidgetBlueprint* WidgetBlueprint = Cast<UEditorUtilityWidgetBlueprint>(Blueprint);

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

uint32 FAssetTypeActions_EditorUtilityWidgetBlueprint::GetCategories()
{
	IBlutilityModule* BlutilityModule = FModuleManager::GetModulePtr<IBlutilityModule>("Blutility");
	return BlutilityModule->GetAssetCategory();
}

void FAssetTypeActions_EditorUtilityWidgetBlueprint::ExecuteEdit(FWeakBlueprintPointerArray Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (auto Object = (*ObjIt).Get())
		{
			auto Blueprint = Cast<UBlueprint>(*ObjIt);
			if (Blueprint && Blueprint->SkeletonGeneratedClass && Blueprint->GeneratedClass)
			{
				TSharedRef< FWidgetBlueprintEditor > NewBlueprintEditor(new FWidgetBlueprintEditor());

				TArray<UBlueprint*> Blueprints;
				Blueprints.Add(Blueprint);
				NewBlueprintEditor->InitWidgetBlueprintEditor(EToolkitMode::Standalone, nullptr, Blueprints, true);
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToLoadWidgetBlueprint", "Editor Utility Widget could not be loaded because it derives from an invalid class.\nCheck to make sure the parent class for this blueprint hasn't been removed!"));
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
