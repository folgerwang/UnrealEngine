// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "EditorUtilityWidgetBlueprint.h"
#include "WidgetBlueprint.h"
#include "Editor.h"
#include "EditorUtilityWidget.h"
#include "IBlutilityModule.h"
#include "Modules/ModuleManager.h"




/////////////////////////////////////////////////////
// UEditorUtilityWidgetBlueprint

UEditorUtilityWidgetBlueprint::UEditorUtilityWidgetBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
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

