// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "EditorUtilityWidgetBlueprint.h"
#include "WidgetBlueprint.h"
#include "Editor.h"
#include "EditorUtilityWidget.h"
#include "IBlutilityModule.h"
#include "Modules/ModuleManager.h"
#include "LevelEditor.h"




/////////////////////////////////////////////////////
// UEditorUtilityWidgetBlueprint

UEditorUtilityWidgetBlueprint::UEditorUtilityWidgetBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UEditorUtilityWidgetBlueprint::BeginDestroy()
{
	// prevent the cleanup script from running on editor shutdown
	if (!GIsRequestingExit)
	{
		IBlutilityModule* BlutilityModule = FModuleManager::GetModulePtr<IBlutilityModule>("Blutility");
		if (BlutilityModule)
		{
			BlutilityModule->RemoveLoadedScriptUI(this);
		}

		FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
		if (LevelEditorModule)
		{
			TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule->GetLevelEditorTabManager();
			if (LevelEditorTabManager.IsValid())
			{
				LevelEditorTabManager->UnregisterTabSpawner(RegistrationName);
			}
		}
	}

	Super::BeginDestroy();
}


TSharedRef<SDockTab> UEditorUtilityWidgetBlueprint::SpawnEditorUITab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab);

	TSharedRef<SWidget> TabWidget = CreateUtilityWidget();
	SpawnedTab->SetContent(TabWidget);
	
	SpawnedTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateUObject(this, &UEditorUtilityWidgetBlueprint::UpdateRespawnListIfNeeded));
	CreatedTab = SpawnedTab;
	
	OnCompiled().AddUObject(this, &UEditorUtilityWidgetBlueprint::RegenerateCreatedTab);

	return SpawnedTab;
}

TSharedRef<SWidget> UEditorUtilityWidgetBlueprint::CreateUtilityWidget()
{
	UClass* BlueprintClass = GeneratedClass;
	TSubclassOf<UEditorUtilityWidget> WidgetClass = BlueprintClass;
	UWorld* World = GEditor->GetEditorWorldContext().World();
	check(World);
	CreatedUMGWidget = CreateWidget<UEditorUtilityWidget>(World, WidgetClass);
	TSharedRef<SWidget> TabWidget = SNullWidget::NullWidget;
	if (CreatedUMGWidget)
	{
		TSharedRef<SWidget> CreatedSlateWidget = CreatedUMGWidget->TakeWidget();
		TabWidget = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			[
				CreatedSlateWidget
			];
	}
	return TabWidget;
}

void UEditorUtilityWidgetBlueprint::RegenerateCreatedTab(UBlueprint* RecompiledBlueprint)
{
	if (CreatedTab.IsValid())
	{
		TSharedRef<SWidget> TabWidget = CreateUtilityWidget();
		CreatedTab.Pin()->SetContent(TabWidget);
	}
}

void UEditorUtilityWidgetBlueprint::UpdateRespawnListIfNeeded(TSharedRef<SDockTab> TabBeingClosed)
{
	UEditorUtilityWidget* EditorUtilityWidget = Cast<UEditorUtilityWidget>(GeneratedClass);
	if (EditorUtilityWidget && EditorUtilityWidget->ShouldAlwaysReregisterWithWindowsMenu() == false)
	{
		IBlutilityModule* BlutilityModule = FModuleManager::GetModulePtr<IBlutilityModule>("Blutility");
		BlutilityModule->RemoveLoadedScriptUI(this);
	}
}

void UEditorUtilityWidgetBlueprint::GetReparentingRules(TSet< const UClass* >& AllowedChildrenOfClasses, TSet< const UClass* >& DisallowedChildrenOfClasses) const
{
	AllowedChildrenOfClasses.Empty();
	AllowedChildrenOfClasses.Add(UEditorUtilityWidget::StaticClass());
}

