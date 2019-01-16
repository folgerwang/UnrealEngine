// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
	CurrentTimecodeSynchronizer = GetDefault<UTimecodeSynchronizerEditorSettings>()->UserTimecodeSynchronizer.LoadSynchronous();
	if (CurrentTimecodeSynchronizer == nullptr)
	{
		CurrentTimecodeSynchronizer = GetDefault<UTimecodeSynchronizerProjectSettings>()->DefaultTimecodeSynchronizer.LoadSynchronous();
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
		ToolbarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateRaw(this, &FTimecodeSynchronizerEditorLevelToolbar::GenerateMenuContent),
			LOCTEXT("TimecodeSynch_Label", "Timecode Synchronizer"),
			LOCTEXT("TimecodeSynch_ToolTip", "List of imecode Synchronizer available to the user for editing or creation."),
			FSlateIcon(FTimecodeSynchronizerEditorStyle::GetStyleSetName(), TEXT("Console"))
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
		MenuBuilder.AddMenuEntry(
			LOCTEXT("CreateMenuLabel", "New Empty Timecode Synchronizer"),
			LOCTEXT("CreateMenuTooltip", "Create a new Timecode Synchronizer asset."),
			FSlateIcon(FTimecodeSynchronizerEditorStyle::GetStyleSetName(), TEXT("Console")),
			FUIAction(
				FExecuteAction::CreateRaw(this, &FTimecodeSynchronizerEditorLevelToolbar::CreateNewTimecodeSynchronizer)
			)
		);

		MenuBuilder.AddMenuSeparator();

		const bool bIsTimecodeSynchronizerValid = CurrentTimecodeSynchronizer.IsValid();

		MenuBuilder.AddSubMenu(
			bIsTimecodeSynchronizerValid ? FText::Format(LOCTEXT("EditMenuLabel", "Open '{0}'"), FText::FromName(CurrentTimecodeSynchronizer->GetFName())) : LOCTEXT("SelectMenuLabel", "Select Profile"),
			LOCTEXT("SelectMenuTooltip", "Select the current timecode synchronizer for this editor."),
			FNewMenuDelegate::CreateRaw(this, &FTimecodeSynchronizerEditorLevelToolbar::AddObjectSubMenu),
			FUIAction(
				FExecuteAction::CreateRaw(this, &FTimecodeSynchronizerEditorLevelToolbar::OpenCurrentTimecodeSynchronizer),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([bIsTimecodeSynchronizerValid] { return bIsTimecodeSynchronizerValid; })
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
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
			CurrentAssetData,
			CurrentTimecodeSynchronizer.IsValid(),
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
