// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "UI/MediaProfileMenuEntry.h"

#include "AssetToolsModule.h"
#include "Factories/MediaProfileFactoryNew.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "IAssetTools.h"
#include "LevelEditor.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "Profile/IMediaProfileManager.h"
#include "Profile/MediaProfile.h"
#include "Profile/MediaProfileSettings.h"
#include "PropertyCustomizationHelpers.h"
#include "Toolkits/AssetEditorManager.h"
#include "UI/MediaFrameworkUtilitiesEditorStyle.h"


#define LOCTEXT_NAMESPACE "MediaProfileEditor"

struct FMediaProfileMenuEntryImpl
{
	FMediaProfileMenuEntryImpl()
	{
		ToolBarExtender = MakeShareable(new FExtender);
		ToolBarExtender->AddToolBarExtension("Settings", EExtensionHook::After, nullptr, FToolBarExtensionDelegate::CreateRaw(this, &FMediaProfileMenuEntryImpl::FillToolbar));

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolBarExtender);
	}

	~FMediaProfileMenuEntryImpl()
	{
		if (!GIsRequestingExit && ToolBarExtender.IsValid())
		{
			FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor");
			if (LevelEditorModule)
			{
				LevelEditorModule->GetToolBarExtensibilityManager()->RemoveExtender(ToolBarExtender);
			}
		}
	}

	FText GetMenuLabel()
	{
		UMediaProfile* Profile = GetCurrentProfile();
		return Profile ? FText::FromName(Profile->GetFName()) : LOCTEXT("NoMediaProfile", "[No profile selected]");
	}

	UMediaProfile* GetCurrentProfile()
	{
		return IMediaProfileManager::Get().GetCurrentMediaProfile();
	}

	void OpenCurrentProfile()
	{
		FAssetEditorManager::Get().OpenEditorForAsset(GetCurrentProfile());
	}

	void CreateNewProfile()
	{
		UMediaProfileFactoryNew* FactoryInstance = DuplicateObject<UMediaProfileFactoryNew>(GetDefault<UMediaProfileFactoryNew>(), GetTransientPackage());
		FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
		UMediaProfile* NewAsset = Cast<UMediaProfile>(FAssetToolsModule::GetModule().Get().CreateAssetWithDialog(FactoryInstance->GetSupportedClass(), FactoryInstance));
		if (NewAsset != nullptr)
		{
			GetMutableDefault<UMediaProfileEditorSettings>()->SetUserMediaProfile(NewAsset);
			IMediaProfileManager::Get().SetCurrentMediaProfile(NewAsset);
			FAssetEditorManager::Get().OpenEditorForAsset(NewAsset);
		}
	}

	void NewProfileSelected(const FAssetData& AssetData)
	{
		FSlateApplication::Get().DismissAllMenus();

		GWarn->BeginSlowTask(LOCTEXT("MediaProfileLoadPackage", "Loading Media Profile"), true, false);
		UMediaProfile* Asset = Cast<UMediaProfile>(AssetData.GetAsset());
		GWarn->EndSlowTask();

		GetMutableDefault<UMediaProfileEditorSettings>()->SetUserMediaProfile(Asset);
		IMediaProfileManager::Get().SetCurrentMediaProfile(Asset);
	}

	void FillToolbar(FToolBarBuilder& ToolbarBuilder)
	{
		ToolbarBuilder.BeginSection("Media Profile");
		{
			// Add a button to edit the current Profile
			ToolbarBuilder.AddToolBarButton(
				FUIAction(
					FExecuteAction::CreateRaw(this, &FMediaProfileMenuEntryImpl::OpenCurrentProfile),
					FCanExecuteAction::CreateLambda([this]() { return GetCurrentProfile() != nullptr; }),
					FIsActionChecked::CreateLambda([this]() { return GetCurrentProfile() != nullptr; })
				),
				NAME_None,
				MakeAttributeRaw(this, &FMediaProfileMenuEntryImpl::GetMenuLabel),
				LOCTEXT("SelectedMenuTooltip", "Edit the selected Media Profile."),
				FSlateIcon(FMediaFrameworkUtilitiesEditorStyle::GetStyleSetName(), TEXT("ToolbarIcon.MediaProfile"))
			);

			ToolbarBuilder.AddComboButton(
				FUIAction(),
				FOnGetContent::CreateRaw(this, &FMediaProfileMenuEntryImpl::GenerateMenuContent),
				LOCTEXT("MediaProfileComboLabel", "Media Profile Options"),
				LOCTEXT("MediaProfileComboToolTip", "Media Profile options menu"),
				FSlateIcon(),
				true
			);
		}
		ToolbarBuilder.EndSection();
	}

	TSharedRef<SWidget> GenerateMenuContent()
	{
		const bool bShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

		MenuBuilder.BeginSection("Profile", LOCTEXT("MediaProfileSection", "Profile"));
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("SelectMenuLabel", "Select Profile"),
				LOCTEXT("SelectMenuTooltip", "Select the current profile for this editor."),
				FNewMenuDelegate::CreateRaw(this, &FMediaProfileMenuEntryImpl::AddObjectSubMenu)
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("CreateMenuLabel", "Create New Media Profile"),
				LOCTEXT("CreateMenuTooltip", "Create a new Media Profile asset."),
				FSlateIcon(FMediaFrameworkUtilitiesEditorStyle::GetStyleSetName(), TEXT("ClassIcon.MediaProfile")),
				FUIAction(
					FExecuteAction::CreateRaw(this, &FMediaProfileMenuEntryImpl::CreateNewProfile)
				)
			);
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	void AddObjectSubMenu(FMenuBuilder& MenuBuilder)
	{
		UMediaProfile* CurrentMediaProfile = GetCurrentProfile();
		FAssetData CurrentAssetData = CurrentMediaProfile ? FAssetData(CurrentMediaProfile) : FAssetData();

		TArray<const UClass*> ClassFilters;
		ClassFilters.Add(UMediaProfile::StaticClass());

		MenuBuilder.AddWidget(
			PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
				FAssetData(),
				true,
				false,
				ClassFilters,
				TArray<UFactory*>(),
				FOnShouldFilterAsset::CreateLambda([CurrentAssetData](const FAssetData& InAssetData){ return InAssetData == CurrentAssetData; }),
				FOnAssetSelected::CreateRaw(this, &FMediaProfileMenuEntryImpl::NewProfileSelected),
				FSimpleDelegate()
			),
			FText::GetEmpty(),
			true,
			false
		);
	}

private:

	TSharedPtr<FExtender> ToolBarExtender;

public:

	static TUniquePtr<FMediaProfileMenuEntryImpl> Implementation;
};

TUniquePtr<FMediaProfileMenuEntryImpl> FMediaProfileMenuEntryImpl::Implementation;

void FMediaProfileMenuEntry::Register()
{
	if (!IsRunningCommandlet() && GetDefault<UMediaProfileEditorSettings>()->bDisplayInToolbar)
	{
		FMediaProfileMenuEntryImpl::Implementation = MakeUnique<FMediaProfileMenuEntryImpl>();
	}
}

void FMediaProfileMenuEntry::Unregister()
{
	FMediaProfileMenuEntryImpl::Implementation.Reset();
}

#undef LOCTEXT_NAMESPACE
