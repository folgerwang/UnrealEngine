// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "UI/TimecodeSynchronizerEditorLevelToolbar.h"

#include "TimecodeSynchronizerFactory.h"
#include "TimecodeSynchronizerProjectSettings.h"
#include "UI/TimecodeSynchronizerEditorCommand.h"
#include "UI/TimecodeSynchronizerEditorStyle.h"

#include "AssetToolsModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "LevelEditor.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "Toolkits/AssetEditorManager.h"
#include "Toolkits/AssetEditorToolkit.h"


#define LOCTEXT_NAMESPACE "TimecodeSynchronizerEditor"

//////////////////////////////////////////////////////////////////////////
// FTimecodeSynchronizerEditorLevelToolbar

FTimecodeSynchronizerEditorLevelToolbar::FTimecodeSynchronizerEditorLevelToolbar()
{
	CurrentTimecodeSynchronizer = GetDefault<UTimecodeSynchronizerEditorSettings>()->UserTimecodeSynchronizer.Get();
	if (CurrentTimecodeSynchronizer == nullptr)
	{
		CurrentTimecodeSynchronizer = GetDefault<UTimecodeSynchronizerProjectSettings>()->DefaultTimecodeSynchronizer.Get();
	}

	ExtendLevelEditorToolbar();
}

FTimecodeSynchronizerEditorLevelToolbar::~FTimecodeSynchronizerEditorLevelToolbar()
{
	if (UObjectInitialized() && LevelToolbarExtender.IsValid() && !GIsRequestingExit)
	{
		// Add a TimecodeSynchronizer toolbar section after the settings section of the level editor
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		if (LevelEditorModule.GetToolBarExtensibilityManager().IsValid())
		{
			LevelEditorModule.GetToolBarExtensibilityManager()->RemoveExtender(LevelToolbarExtender);
		}
	}
}

void FTimecodeSynchronizerEditorLevelToolbar::ExtendLevelEditorToolbar()
{
	check(!LevelToolbarExtender.IsValid());

	// Create Toolbar extension
	LevelToolbarExtender = MakeShareable(new FExtender);
	LevelToolbarExtender->AddToolBarExtension(
		"Settings",
		EExtensionHook::After,
		FTimecodeSynchronizerEditorCommand::Get().CommandActionList,
		FToolBarExtensionDelegate::CreateRaw(this, &FTimecodeSynchronizerEditorLevelToolbar::FillToolbar)
		);

	// Add a TimecodeSynchronizer toolbar section after the settings section of the level editor
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(LevelToolbarExtender);
}

void FTimecodeSynchronizerEditorLevelToolbar::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("TimecodeSynchronizer");
	{
		// Add a button to open a TimecodeSynchronizer Editor
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateRaw(this, &FTimecodeSynchronizerEditorLevelToolbar::OpenCurrentTimecodeSynchronizer),
				FCanExecuteAction::CreateLambda([this]() { return CurrentTimecodeSynchronizer.IsValid(); }),
				FIsActionChecked::CreateLambda([this]() { return CurrentTimecodeSynchronizer.IsValid(); })
			),
			NAME_None,
			MakeAttributeRaw(this, &FTimecodeSynchronizerEditorLevelToolbar::GetMenuLabel),
			LOCTEXT("ToolbarButtonTooltip", "Edit the selected Timecode Provider."),
			FSlateIcon(FTimecodeSynchronizerEditorStyle::GetStyleSetName(), TEXT("Console"))
		);

		ToolbarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateRaw(this, &FTimecodeSynchronizerEditorLevelToolbar::GenerateMenuContent),
			LOCTEXT("TimecodeSynchronizerComboLabel", "Timecode Synchronizer Options"),
			LOCTEXT("TimecodeSynchronizerComboToolTip", "Timecode Synchronizer options menu"),
			FSlateIcon(),
			true
		);
	}
	ToolbarBuilder.EndSection();
}

TSharedRef<SWidget> FTimecodeSynchronizerEditorLevelToolbar::GenerateMenuContent()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	MenuBuilder.BeginSection("TimecodeSynchronizer", LOCTEXT("TimecodeSynchronizerSection", "Timecode Synchronizer"));
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("SelectMenuLabel", "Select Synchronizer"),
			LOCTEXT("SelectMenuTooltip", "Select the current timecode synchronizer for this editor."),
			FNewMenuDelegate::CreateRaw(this, &FTimecodeSynchronizerEditorLevelToolbar::AddObjectSubMenu)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("CreateMenuLabel", "Create New Timecode Synchronizer"),
			LOCTEXT("CreateMenuTooltip", "Create a new Timecode Synchronizer asset."),
			FSlateIcon(FTimecodeSynchronizerEditorStyle::GetStyleSetName(), TEXT("Console")),
			FUIAction(
				FExecuteAction::CreateRaw(this, &FTimecodeSynchronizerEditorLevelToolbar::CreateNewTimecodeSynchronizer)
			)
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FTimecodeSynchronizerEditorLevelToolbar::AddObjectSubMenu(FMenuBuilder& MenuBuilder)
{
	FAssetData CurrentAssetData = CurrentTimecodeSynchronizer.IsValid() ? FAssetData(CurrentTimecodeSynchronizer.Get()) : FAssetData();

	TArray<const UClass*> ClassFilters;
	ClassFilters.Add(UTimecodeSynchronizer::StaticClass());

	MenuBuilder.AddWidget(
		PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
			FAssetData(),
			true,
			false,
			ClassFilters,
			TArray<UFactory*>(),
			FOnShouldFilterAsset::CreateLambda([CurrentAssetData](const FAssetData& InAssetData) { return InAssetData == CurrentAssetData; }),
			FOnAssetSelected::CreateRaw(this, &FTimecodeSynchronizerEditorLevelToolbar::NewTimecodeSynchronizerSelected),
			FSimpleDelegate()
		),
		FText::GetEmpty(),
		true,
		false
	);
}

FText FTimecodeSynchronizerEditorLevelToolbar::GetMenuLabel()
{
	return CurrentTimecodeSynchronizer.IsValid() ? FText::FromName(CurrentTimecodeSynchronizer.Get()->GetFName()) : LOCTEXT("NoTimecodeProvider", "[No asset selected]");
}

void FTimecodeSynchronizerEditorLevelToolbar::OpenCurrentTimecodeSynchronizer()
{
	FAssetEditorManager::Get().OpenEditorForAsset(CurrentTimecodeSynchronizer.Get());
}

void FTimecodeSynchronizerEditorLevelToolbar::CreateNewTimecodeSynchronizer()
{
	UTimecodeSynchronizerFactory* FactoryInstance = DuplicateObject<UTimecodeSynchronizerFactory>(GetDefault<UTimecodeSynchronizerFactory>(), GetTransientPackage());
	FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
	UTimecodeSynchronizer* NewAsset = Cast<UTimecodeSynchronizer>(FAssetToolsModule::GetModule().Get().CreateAssetWithDialog(FactoryInstance->GetSupportedClass(), FactoryInstance));
	if (NewAsset != nullptr)
	{
		GetMutableDefault<UTimecodeSynchronizerEditorSettings>()->UserTimecodeSynchronizer = NewAsset;
		GetMutableDefault<UTimecodeSynchronizerEditorSettings>()->SaveConfig();
		CurrentTimecodeSynchronizer = NewAsset;
		FAssetEditorManager::Get().OpenEditorForAsset(NewAsset);
	}
}

void FTimecodeSynchronizerEditorLevelToolbar::NewTimecodeSynchronizerSelected(const FAssetData& AssetData)
{
	FSlateApplication::Get().DismissAllMenus();

	GWarn->BeginSlowTask(LOCTEXT("TImecodeSynchronizerLoadPackage", "Loading Timecode Synchronizer"), true, false);
	UTimecodeSynchronizer* Asset = Cast<UTimecodeSynchronizer>(AssetData.GetAsset());
	GWarn->EndSlowTask();

	GetMutableDefault<UTimecodeSynchronizerEditorSettings>()->UserTimecodeSynchronizer = Asset;
	GetMutableDefault<UTimecodeSynchronizerEditorSettings>()->SaveConfig();
	CurrentTimecodeSynchronizer = Asset;
}

#undef LOCTEXT_NAMESPACE
