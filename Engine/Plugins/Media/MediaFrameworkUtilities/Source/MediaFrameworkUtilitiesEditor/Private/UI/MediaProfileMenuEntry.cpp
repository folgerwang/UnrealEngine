// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
			ToolbarBuilder.AddComboButton(
				FUIAction(),
				FOnGetContent::CreateRaw(this, &FMediaProfileMenuEntryImpl::GenerateMenuContent),
				LOCTEXT("MediaProfile_Label", "Media Profile"),
				LOCTEXT("MediaProfile_ToolTip", "List of Media Profile available to the user for editing or creation."),
				FSlateIcon(FMediaFrameworkUtilitiesEditorStyle::GetStyleSetName(), TEXT("ToolbarIcon.MediaProfile"))
			);
		}
		ToolbarBuilder.EndSection();
	}

	TSharedRef<SWidget> GenerateMenuContent()
	{
		const bool bShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

		MenuBuilder.BeginSection("Profile", LOCTEXT("MediaProfileSection", "Media Profile"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("CreateMenuLabel", "New Empty Media Profile"),
				LOCTEXT("CreateMenuTooltip", "Create a new Media Profile asset."),
				FSlateIcon(FMediaFrameworkUtilitiesEditorStyle::GetStyleSetName(), TEXT("ClassIcon.MediaProfile")),
				FUIAction(
					FExecuteAction::CreateRaw(this, &FMediaProfileMenuEntryImpl::CreateNewProfile)
				)
			);

			MenuBuilder.AddMenuSeparator();

			UMediaProfile* Profile = GetCurrentProfile();
			const bool bIsProfileValid = Profile != nullptr;

			MenuBuilder.AddSubMenu(
				bIsProfileValid ? FText::Format(LOCTEXT("EditMenuLabel", "Open '{0}'"), FText::FromName(Profile->GetFName()))  : LOCTEXT("SelectMenuLabel", "Select Profile"),
				LOCTEXT("SelectMenuTooltip", "Select the current profile for this editor."),
				FNewMenuDelegate::CreateRaw(this, &FMediaProfileMenuEntryImpl::AddObjectSubMenu),
				FUIAction(
					bIsProfileValid ? FExecuteAction::CreateRaw(this, &FMediaProfileMenuEntryImpl::OpenCurrentProfile) : FExecuteAction(),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([bIsProfileValid]{ return bIsProfileValid; })
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
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
				CurrentAssetData,
				CurrentMediaProfile != nullptr,
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
