// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "UI/MediaProfileMenuEntry.h"

#include "AssetEditor/MediaProfileCommands.h"
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
		TSharedPtr<FUICommandList> Actions = MakeShareable(new FUICommandList);

		// Action to edit the current selected media profile
		Actions->MapAction(FMediaProfileCommands::Get().Edit,
			FExecuteAction::CreateLambda([this]()
			{
				if (UMediaProfile* MediaProfile = IMediaProfileManager::Get().GetCurrentMediaProfile())
				{
					FAssetEditorManager::Get().OpenEditorForAsset(MediaProfile);
				}
			}),
			FCanExecuteAction::CreateLambda([] { return IMediaProfileManager::Get().GetCurrentMediaProfile() != nullptr; }),
			FIsActionChecked::CreateLambda([]{ return IMediaProfileManager::Get().GetCurrentMediaProfile() != nullptr; })
		);

		ToolBarExtender = MakeShareable(new FExtender);
		ToolBarExtender->AddToolBarExtension("Settings", EExtensionHook::After, Actions, FToolBarExtensionDelegate::CreateRaw(this, &FMediaProfileMenuEntryImpl::FillToolbar));

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
			auto TooltipLambda = [this]()
			{
				UMediaProfile* MediaProfile = IMediaProfileManager::Get().GetCurrentMediaProfile();
				if (MediaProfile == nullptr)
				{
					return LOCTEXT("EmptyMediaProfile_ToolTip", "Select a Media Profile to edit it.");
				}
				return FText::Format(LOCTEXT("MediaProfile_ToolTip", "Edit '{0}'")
					, FText::FromName(MediaProfile->GetFName()));
			};

			// Add a button to edit the current media profile
			ToolbarBuilder.AddToolBarButton(
				FMediaProfileCommands::Get().Edit,
				NAME_None,
				LOCTEXT("MediaProfile_Label", "Media Profile"),
				MakeAttributeLambda(TooltipLambda),
				FSlateIcon(FMediaFrameworkUtilitiesEditorStyle::GetStyleSetName(), TEXT("ToolbarIcon.MediaProfile"))
			);

			// Add a simple drop-down menu (no label, no icon for the drop-down button itself) that list the media profile available
			ToolbarBuilder.AddComboButton(
				FUIAction(),
				FOnGetContent::CreateRaw(this, &FMediaProfileMenuEntryImpl::GenerateMenuContent),
				FText::GetEmpty(),
				LOCTEXT("MediaProfileButton_ToolTip", "List of Media Profile available to the user for editing or creation."),
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

		MenuBuilder.BeginSection("Profile", LOCTEXT("NewMediaProfileSection", "New"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("CreateMenuLabel", "New Empty Media Profile"),
				LOCTEXT("CreateMenuTooltip", "Create a new Media Profile asset."),
				FSlateIcon(FMediaFrameworkUtilitiesEditorStyle::GetStyleSetName(), TEXT("ClassIcon.MediaProfile")),
				FUIAction(
					FExecuteAction::CreateRaw(this, &FMediaProfileMenuEntryImpl::CreateNewProfile)
				)
			);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("Profile", LOCTEXT("MediaProfileSection", "Media Profile"));
		{
			UMediaProfile* Profile = GetCurrentProfile();
			const bool bIsProfileValid = Profile != nullptr;

			MenuBuilder.AddSubMenu(
				bIsProfileValid ? FText::FromName(Profile->GetFName()) : LOCTEXT("SelectMenuLabel", "Select a Media Profile"),
				LOCTEXT("SelectMenuTooltip", "Select the current profile for this editor."),
				FNewMenuDelegate::CreateRaw(this, &FMediaProfileMenuEntryImpl::AddObjectSubMenu),
				FUIAction(),
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
