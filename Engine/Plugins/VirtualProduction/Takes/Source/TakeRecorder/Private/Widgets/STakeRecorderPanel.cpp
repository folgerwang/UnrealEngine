// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Widgets/STakeRecorderPanel.h"
#include "Widgets/TakeRecorderWidgetConstants.h"
#include "Widgets/SLevelSequenceTakeEditor.h"
#include "Recorder/TakeRecorder.h"
#include "ScopedSequencerPanel.h"
#include "ITakeRecorderModule.h"
#include "TakesCoreBlueprintLibrary.h"
#include "TakePreset.h"
#include "TakeMetaData.h"
#include "TakeRecorderSources.h"
#include "TakeRecorderStyle.h"
#include "TakeRecorderSettings.h"
#include "Widgets/STakeRecorderCockpit.h"
#include "LevelSequence.h"

// Core includes
#include "Modules/ModuleManager.h"
#include "Algo/Sort.h"
#include "Misc/FileHelper.h"

// AssetRegistry includes
#include "AssetRegistryModule.h"
#include "IAssetRegistry.h"
#include "AssetData.h"

// ContentBrowser includes
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

// AssetTools includes
#include "AssetToolsModule.h"

// Slate includes
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateIconFinder.h"

// EditorStyle includes
#include "EditorStyleSet.h"
#include "EditorFontGlyphs.h"

// UnrealEd includes
#include "ScopedTransaction.h"
#include "FileHelpers.h"
#include "Misc/MessageDialog.h"

// Sequencer includes
#include "ISequencer.h"
#include "SequencerSettings.h"

#include "LevelEditor.h"

#define LOCTEXT_NAMESPACE "STakeRecorderPanel"

STakeRecorderPanel::~STakeRecorderPanel()
{
	UTakeRecorder::OnRecordingInitialized().Remove(OnRecordingInitializedHandle);
}

PRAGMA_DISABLE_OPTIMIZATION
void STakeRecorderPanel::Construct(const FArguments& InArgs)
{
	// If a recording is currently underway, initialize to that now
	if (UTakeRecorder* ActiveRecorder = UTakeRecorder::GetActiveRecorder())
	{
		RecordingLevelSequence = ActiveRecorder->GetSequence();
		OnRecordingFinishedHandle = ActiveRecorder->OnRecordingFinished().AddSP(this, &STakeRecorderPanel::OnRecordingFinished);
		OnRecordingCancelledHandle = ActiveRecorder->OnRecordingCancelled().AddSP(this, &STakeRecorderPanel::OnRecordingCancelled);
	}
	else
	{
		RecordingLevelSequence = nullptr;
	}

	TransientPreset = AllocateTransientPreset();
	LastRecordedLevelSequence = nullptr;

	// Copy the base preset into the transient preset if it was provided.
	// We do this first so that anything that asks for its Level Sequence
	// on construction gets the right one
	if (InArgs._BasePreset)
	{
		TransientPreset->CopyFrom(InArgs._BasePreset);
	}
	else if (InArgs._BaseSequence)
	{
		TransientPreset->CopyFrom(InArgs._BaseSequence);

		ULevelSequence* LevelSequence = TransientPreset->GetLevelSequence();

		UTakeRecorderSources* BaseSources = InArgs._BaseSequence->FindMetaData<UTakeRecorderSources>();
		if (BaseSources && LevelSequence)
		{
			LevelSequence->CopyMetaData(BaseSources);
		}

		if (LevelSequence)
		{
			LevelSequence->GetMovieScene()->SetReadOnly(false);
		}

		UTakeMetaData*  TakeMetaData = LevelSequence ? LevelSequence->FindMetaData<UTakeMetaData>() : nullptr;
		if (TakeMetaData)
		{
			TakeMetaData->Unlock();
			TakeMetaData->SetTimestamp(FDateTime(0));
		}
	}
	else if (InArgs._SequenceToView)
	{
		SuppliedLevelSequence  = InArgs._SequenceToView;
	}

	// Create the child widgets that need to know about our level sequence
	CockpitWidget = SNew(STakeRecorderCockpit)
	.LevelSequence(this, &STakeRecorderPanel::GetLevelSequence);

	LevelSequenceTakeWidget = SNew(SLevelSequenceTakeEditor)
	.LevelSequence(this, &STakeRecorderPanel::GetLevelSequence);

	// Create the sequencer panel, and open it if necessary
	SequencerPanel = MakeShared<FScopedSequencerPanel>(MakeAttributeSP(this, &STakeRecorderPanel::GetLevelSequence));

	// Bind onto the necessary delegates we need
	OnLevelSequenceChangedHandle = TransientPreset->AddOnLevelSequenceChanged(FSimpleDelegate::CreateSP(this, &STakeRecorderPanel::OnLevelSequenceChanged));
	OnRecordingInitializedHandle = UTakeRecorder::OnRecordingInitialized().AddSP(this, &STakeRecorderPanel::OnRecordingInitialized);

	// Setup the preset origin for the meta-data in the cockpit if one was supplied
	if (InArgs._BasePreset)
	{
		CockpitWidget->GetMetaData()->SetPresetOrigin(InArgs._BasePreset);
	}

	// Add the user settings immediately if the user preference tells us to
	UTakeRecorderUserSettings* UserSettings = GetMutableDefault<UTakeRecorderUserSettings>();
	if (UserSettings->bShowUserSettingsOnUI)
	{
		LevelSequenceTakeWidget->AddExternalSettingsObject(UserSettings);
	}

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(FMargin(0.f, 1.0f))
		.AutoHeight()
		[
			MakeToolBar()
		]

		+ SVerticalBox::Slot()
		.Padding(FMargin(0.f, 1.0f))
		.AutoHeight()
		[
			CockpitWidget.ToSharedRef()
		]

		+ SVerticalBox::Slot()
		.Padding(FMargin(0.f, 1.0f, 0.f, 0.f))
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("DetailsView.CategoryTop"))
			.BorderBackgroundColor( FLinearColor( .6,.6,.6, 1.0f ) )
			.IsEnabled_Lambda([this]() { return !CockpitWidget->Reviewing(); })
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.Padding(TakeRecorder::ButtonOffset)
				.VAlign(VAlign_Fill)
				.AutoWidth()
				[
					LevelSequenceTakeWidget->MakeAddSourceButton()
				]

				+ SHorizontalBox::Slot()
				.Padding(TakeRecorder::ButtonOffset)
				.VAlign(VAlign_Fill)
				.AutoWidth()
				[
					SNew(SComboButton)
					.ContentPadding(TakeRecorder::ButtonPadding)
					.ComboButtonStyle(FTakeRecorderStyle::Get(), "ComboButton")
					.OnGetMenuContent(this, &STakeRecorderPanel::OnGeneratePresetsMenu)
					.ForegroundColor(FSlateColor::UseForeground())
					.ButtonContent()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SImage)
							.Image(FSlateIconFinder::FindIconBrushForClass(UTakePreset::StaticClass()))
						]

						+ SHorizontalBox::Slot()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("PresetsToolbarButton", "Presets"))
						]
					]
				]

				+ SHorizontalBox::Slot()
				[
					SNew(SSpacer)
				]

				+ SHorizontalBox::Slot()
				.Padding(TakeRecorder::ButtonOffset)
				.VAlign(VAlign_Fill)
				.AutoWidth()
				[
					SNew(SButton)
					.ContentPadding(TakeRecorder::ButtonPadding)
					.ToolTipText(LOCTEXT("RevertChanges_Text", "Revert all changes made to this take back its original state (either its original preset, or an empty take)."))
					.ForegroundColor(FSlateColor::UseForeground())
					.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
					.OnClicked(this, &STakeRecorderPanel::OnRevertChanges)
					[
						SNew(STextBlock)
						.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.11"))
						.Text(FEditorFontGlyphs::Undo)
					]
				]
			]
		]

		+ SVerticalBox::Slot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				LevelSequenceTakeWidget.ToSharedRef()
			]
		]
	];
}

TSharedRef<SWidget> STakeRecorderPanel::MakeToolBar()
{

	int ButtonBoxSize = 28;
	return SNew(SBorder)

	.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
	.Padding(FMargin(3.f, 3.f))
	[

		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.Padding(TakeRecorder::ButtonOffset)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(ButtonBoxSize)
			.HeightOverride(ButtonBoxSize)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ToolTipText(LOCTEXT("Add", "Create a New Take"))
				.ForegroundColor(FSlateColor::UseForeground())
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.OnClicked(this, &STakeRecorderPanel::OnNewTake)
				[
					SNew(STextBlock)
					.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.14"))
					.Text(FEditorFontGlyphs::File)
				]
			]
		]

		+ SHorizontalBox::Slot()
		.Padding(TakeRecorder::ButtonOffset)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[

			SNew(SBox)
			.WidthOverride(ButtonBoxSize)
			.HeightOverride(ButtonBoxSize)
			.Visibility_Lambda([this]() { return !CockpitWidget->Reviewing() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ContentPadding(TakeRecorder::ButtonPadding)
				.ToolTipText(LOCTEXT("ReviewLastRecording", "Review the Last Recording"))
				.ForegroundColor(FSlateColor::UseForeground())
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.IsEnabled_Lambda([this]() { return (LastRecordedLevelSequence != nullptr); })
				.OnClicked(this, &STakeRecorderPanel::OnReviewLastRecording)
				[
					SNew(SImage)
					.Image(FTakeRecorderStyle::Get().GetBrush("TakeRecorder.ReviewRecordingButton"))
				]
			]
		]

		+ SHorizontalBox::Slot()
		.Padding(TakeRecorder::ButtonOffset)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(ButtonBoxSize)
			.HeightOverride(ButtonBoxSize)
			.Visibility_Lambda([this]() { return CockpitWidget->Reviewing() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ContentPadding(TakeRecorder::ButtonPadding)
				.ToolTipText(LOCTEXT("Back", "Return Back to the Pending Take"))
				.ForegroundColor(FSlateColor::UseForeground())
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.OnClicked(this, &STakeRecorderPanel::OnBackToPendingTake)
				[
					SNew(STextBlock)
					.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.14"))
					.Text(FEditorFontGlyphs::Arrow_Left)
				]
			]
		]

		+ SHorizontalBox::Slot()
		[
			SNew(SSpacer)
		]

		+ SHorizontalBox::Slot()
		.Padding(TakeRecorder::ButtonOffset)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(ButtonBoxSize)
			.HeightOverride(ButtonBoxSize)
			[
				SNew(SCheckBox)
				.Padding(TakeRecorder::ButtonPadding)
				.ToolTipText(NSLOCTEXT("TakesBrowser", "ToggleTakeBrowser_Tip", "Show/Hide the Takes Browser"))
				.Style(FTakeRecorderStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked(this, &STakeRecorderPanel::GetTakeBrowserCheckState)
				.OnCheckStateChanged(this, &STakeRecorderPanel::ToggleTakeBrowserCheckState)
				[
					SNew(STextBlock)
					.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.14"))
					.Text(FEditorFontGlyphs::Folder_Open)
				]
			]
		]

		+ SHorizontalBox::Slot()
		.Padding(TakeRecorder::ButtonOffset)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(ButtonBoxSize)
			.HeightOverride(ButtonBoxSize)
			[

				SequencerPanel->MakeToggleButton()
			]
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(ButtonBoxSize)
			.HeightOverride(ButtonBoxSize)
			.Visibility_Lambda([this]() { return CockpitWidget->Reviewing() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				CockpitWidget->MakeLockButton()
			]
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Fill)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(ButtonBoxSize)
			.HeightOverride(ButtonBoxSize)
			[

				SNew(SCheckBox)
				.Padding(TakeRecorder::ButtonPadding)
				.ToolTipText(LOCTEXT("ShowUserSettings_Tip", "Show/Hide the general user settings for take recorder"))
				.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
				.ForegroundColor(FSlateColor::UseForeground())
				.IsChecked(this, &STakeRecorderPanel::GetUserSettingsCheckState)
				.OnCheckStateChanged(this, &STakeRecorderPanel::ToggleUserSettings)
				.Visibility_Lambda([this]() { return !CockpitWidget->Reviewing() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(STextBlock)
					.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.14"))
					.Text(FEditorFontGlyphs::Cogs)
				]
			]
		]
	];
}
PRAGMA_ENABLE_OPTIMIZATION

ULevelSequence* STakeRecorderPanel::GetLevelSequence() const
{
	if (SuppliedLevelSequence)
	{
		return SuppliedLevelSequence;
	}
	else if (RecordingLevelSequence)
	{
		return RecordingLevelSequence;
	}
	else
	{
		return TransientPreset->GetLevelSequence();
	}
}

UTakeMetaData* STakeRecorderPanel::GetTakeMetaData() const
{
	return CockpitWidget->GetMetaData();
}

void STakeRecorderPanel::NewTake()
{
	if (CockpitWidget->Reviewing())
	{
		LastRecordedLevelSequence = SuppliedLevelSequence;
	}

	SuppliedLevelSequence = nullptr;

	FScopedTransaction Transaction(LOCTEXT("NewTake_Transaction", "New Take"));

	TransientPreset->Modify();
	TransientPreset->CreateLevelSequence();
}

UTakePreset* STakeRecorderPanel::AllocateTransientPreset()
{
	static const TCHAR* PackageName = TEXT("/Temp/TakeRecorder/PendingTake");

	UTakePreset* ExistingPreset = FindObject<UTakePreset>(nullptr, TEXT("/Temp/TakeRecorder/PendingTake.PendingTake"));
	if (ExistingPreset)
	{
		return ExistingPreset;
	}

	UTakePreset* TemplatePreset = GetDefault<UTakeRecorderUserSettings>()->LastOpenedPreset.Get();

	static FName DesiredName = "PendingTake";

	UPackage* NewPackage = CreatePackage(nullptr, PackageName);
	NewPackage->SetFlags(RF_Transient);
	NewPackage->AddToRoot();

	UTakePreset* NewPreset = nullptr;

	if (TemplatePreset)
	{
		NewPreset = DuplicateObject<UTakePreset>(TemplatePreset, NewPackage, DesiredName);
		NewPreset->SetFlags(RF_Transient | RF_Transactional | RF_Standalone);
	}
	else
	{
		NewPreset = NewObject<UTakePreset>(NewPackage, DesiredName, RF_Transient | RF_Transactional | RF_Standalone);
	}

	NewPreset->GetOrCreateLevelSequence();

	return NewPreset;
}


void STakeRecorderPanel::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(TransientPreset);
	Collector.AddReferencedObject(SuppliedLevelSequence);
	Collector.AddReferencedObject(RecordingLevelSequence);
}


TSharedRef<SWidget> STakeRecorderPanel::OnGeneratePresetsMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SaveAsPreset_Text", "Save As Preset"),
		LOCTEXT("SaveAsPreset_Tip", "Save the current setup as a new preset that can be imported at a later date"),
		FSlateIcon(FEditorStyle::Get().GetStyleSetName(), "AssetEditor.SaveAsset.Greyscale"),
		FUIAction(
			FExecuteAction::CreateSP(this, &STakeRecorderPanel::OnSaveAsPreset)
		)
	);

	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.SelectionMode = ESelectionMode::Single;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;
		AssetPickerConfig.bFocusSearchBoxWhenOpened = true;
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.bShowBottomToolbar = true;
		AssetPickerConfig.bAutohideSearchBar = false;
		AssetPickerConfig.bAllowDragging = false;
		AssetPickerConfig.bCanShowClasses = false;
		AssetPickerConfig.bShowPathInColumnView = true;
		AssetPickerConfig.bShowTypeInColumnView = false;
		AssetPickerConfig.bSortByPathInColumnView = false;

		AssetPickerConfig.AssetShowWarningText = LOCTEXT("NoPresets_Warning", "No Presets Found");
		AssetPickerConfig.Filter.ClassNames.Add(UTakePreset::StaticClass()->GetFName());
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &STakeRecorderPanel::OnImportPreset);
	}

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ImportPreset_MenuSection", "Import Preset"));
	{
		TSharedRef<SWidget> PresetPicker = SNew(SBox)
			.MinDesiredWidth(400.f)
			.MinDesiredHeight(400.f)
			[
				ContentBrowser.CreateAssetPicker(AssetPickerConfig)
			];

		MenuBuilder.AddWidget(PresetPicker, FText(), true, false);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}


void STakeRecorderPanel::OnImportPreset(const FAssetData& InPreset)
{
	FSlateApplication::Get().DismissAllMenus();

	SuppliedLevelSequence = nullptr;

	UTakePreset* Take = CastChecked<UTakePreset>(InPreset.GetAsset());
	if (Take)
	{
		FScopedTransaction Transaction(LOCTEXT("ImportPreset_Transaction", "Import Take Preset"));

		TransientPreset->Modify();
		TransientPreset->CopyFrom(Take);

		CockpitWidget->GetMetaData()->SetPresetOrigin(Take);
	}
	else
	{
		// @todo: notification?
	}
}


static bool OpenSaveDialog(const FString& InDefaultPath, const FString& InNewNameSuggestion, FString& OutPackageName)
{
	FSaveAssetDialogConfig SaveAssetDialogConfig;
	{
		SaveAssetDialogConfig.DefaultPath = InDefaultPath;
		SaveAssetDialogConfig.DefaultAssetName = InNewNameSuggestion;
		SaveAssetDialogConfig.AssetClassNames.Add(UTakePreset::StaticClass()->GetFName());
		SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
		SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveTakePresetDialogTitle", "Save Take Preset");
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

	if (!SaveObjectPath.IsEmpty())
	{
		OutPackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
		return true;
	}

	return false;
}


bool STakeRecorderPanel::GetSavePresetPackageName(FString& OutName)
{
	UTakeRecorderUserSettings* ConfigSettings = GetMutableDefault<UTakeRecorderUserSettings>();

	FDateTime Today = FDateTime::Now();

	TMap<FString, FStringFormatArg> FormatArgs;
	FormatArgs.Add(TEXT("date"), Today.ToString());

	// determine default package path
	const FString DefaultSaveDirectory = FString::Format(*ConfigSettings->PresetSaveDir.Path, FormatArgs);

	FString DialogStartPath;
	FPackageName::TryConvertFilenameToLongPackageName(DefaultSaveDirectory, DialogStartPath);
	if (DialogStartPath.IsEmpty())
	{
		DialogStartPath = TEXT("/Game");
	}

	// determine default asset name
	FString DefaultName = LOCTEXT("NewTakePreset", "NewTakePreset").ToString();

	FString UniquePackageName;
	FString UniqueAssetName;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(DialogStartPath / DefaultName, TEXT(""), UniquePackageName, UniqueAssetName);

	FString DialogStartName = FPaths::GetCleanFilename(UniqueAssetName);

	FString UserPackageName;
	FString NewPackageName;

	// get destination for asset
	bool bFilenameValid = false;
	while (!bFilenameValid)
	{
		if (!OpenSaveDialog(DialogStartPath, DialogStartName, UserPackageName))
		{
			return false;
		}

		NewPackageName = FString::Format(*UserPackageName, FormatArgs);

		FText OutError;
		bFilenameValid = FFileHelper::IsFilenameValidForSaving(NewPackageName, OutError);
	}

	ConfigSettings->PresetSaveDir.Path = FPackageName::GetLongPackagePath(UserPackageName);
	ConfigSettings->SaveConfig();
	OutName = MoveTemp(NewPackageName);
	return true;
}


void STakeRecorderPanel::OnSaveAsPreset()
{
	FString PackageName;
	if (!GetSavePresetPackageName(PackageName))
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("SaveAsPreset", "Save As Preset"));

	// Saving into a new package
	const FString NewAssetName = FPackageName::GetLongPackageAssetName(PackageName);
	UPackage*     NewPackage   = CreatePackage(nullptr, *PackageName);
	UTakePreset*  NewPreset    = NewObject<UTakePreset>(NewPackage, *NewAssetName, RF_Public | RF_Standalone | RF_Transactional);

	if (NewPreset)
	{
		NewPreset->CopyFrom(TransientPreset);
		if (ULevelSequence* LevelSequence = NewPreset->GetLevelSequence())
		{
			// Ensure no take meta data is saved with this preset
			LevelSequence->RemoveMetaData<UTakeMetaData>();
		}

		NewPreset->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(NewPreset);

		FEditorFileUtils::PromptForCheckoutAndSave({ NewPackage }, false, false);

		CockpitWidget->GetMetaData()->SetPresetOrigin(NewPreset);
	}
}

FReply STakeRecorderPanel::OnBackToPendingTake()
{
	if (CockpitWidget->Reviewing())
	{
		LastRecordedLevelSequence = SuppliedLevelSequence;
	}

	SuppliedLevelSequence = nullptr;
	
	TransientPreset = AllocateTransientPreset();
	RefreshPanel();

	return FReply::Handled();
}

FReply STakeRecorderPanel::OnNewTake()
{
	FText WarningMessage (LOCTEXT("Warning_NewTake", "Are you sure you want to create a new empty take setup? Your current changes will be discarded."));
	if (EAppReturnType::No == FMessageDialog::Open(EAppMsgType::YesNo, WarningMessage))
	{
		return FReply::Handled();
	}

	NewTake();
	return FReply::Handled();
}

FReply STakeRecorderPanel::OnReviewLastRecording()
{
	if (LastRecordedLevelSequence)
	{
		SuppliedLevelSequence = LastRecordedLevelSequence;
		LastRecordedLevelSequence = nullptr;
		RefreshPanel();
	}

	return FReply::Handled();
}

FReply STakeRecorderPanel::OnRevertChanges()
{
	FText WarningMessage(LOCTEXT("Warning_RevertChanges", "Are you sure you want to revert changes? Your current changes will be discarded."));
	if (EAppReturnType::No == FMessageDialog::Open(EAppMsgType::YesNo, WarningMessage))
	{
		return FReply::Handled();
	}

	UTakePreset* PresetOrigin = CockpitWidget->GetMetaData()->GetPresetOrigin();

	FScopedTransaction Transaction(LOCTEXT("RevertChanges_Transaction", "Revert Changes"));

	TransientPreset->Modify();
	TransientPreset->CopyFrom(PresetOrigin);

	return FReply::Handled();
}


void STakeRecorderPanel::RefreshPanel()
{
	// Re-open the sequencer panel for the new level sequence if it should be
	if (GetDefault<UTakeRecorderUserSettings>()->bIsSequenceOpen)
	{
		SequencerPanel->Open();
	}
}


ECheckBoxState STakeRecorderPanel::GetUserSettingsCheckState() const
{
	return GetDefault<UTakeRecorderUserSettings>()->bShowUserSettingsOnUI ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}


void STakeRecorderPanel::ToggleUserSettings(ECheckBoxState CheckState)
{
	UTakeRecorderUserSettings* UserSettings = GetMutableDefault<UTakeRecorderUserSettings>();

	if (LevelSequenceTakeWidget->RemoveExternalSettingsObject(UserSettings))
	{
		UserSettings->bShowUserSettingsOnUI = false;
	}
	else
	{
		LevelSequenceTakeWidget->AddExternalSettingsObject(UserSettings);
		UserSettings->bShowUserSettingsOnUI = true;
	}

	UserSettings->SaveConfig();
}


void STakeRecorderPanel::OnLevelSequenceChanged()
{
	RefreshPanel();
}

void STakeRecorderPanel::OnRecordingInitialized(UTakeRecorder* Recorder)
{
	RecordingLevelSequence = Recorder->GetSequence();
	RefreshPanel();

	OnRecordingFinishedHandle = Recorder->OnRecordingFinished().AddSP(this, &STakeRecorderPanel::OnRecordingFinished);
	OnRecordingCancelledHandle = Recorder->OnRecordingCancelled().AddSP(this, &STakeRecorderPanel::OnRecordingCancelled);
}

void STakeRecorderPanel::OnRecordingFinished(UTakeRecorder* Recorder)
{
	LastRecordedLevelSequence = RecordingLevelSequence;
	OnRecordingCancelled(Recorder);

	// Update the preset take number at the end of recording
	ULevelSequence* LevelSequence = TransientPreset->GetLevelSequence();
	UTakeMetaData*  TakeMetaData = LevelSequence ? LevelSequence->FindMetaData<UTakeMetaData>() : nullptr;

	if (TakeMetaData)
	{
		int32 NextTakeNumber = UTakesCoreBlueprintLibrary::ComputeNextTakeNumber(TakeMetaData->GetSlate());
		TakeMetaData->SetTakeNumber(NextTakeNumber);
	}
}

void STakeRecorderPanel::OnRecordingCancelled(UTakeRecorder* Recorder)
{
	RecordingLevelSequence = nullptr;
	RefreshPanel();

	Recorder->OnRecordingFinished().Remove(OnRecordingFinishedHandle);
	Recorder->OnRecordingCancelled().Remove(OnRecordingCancelledHandle);
}

ECheckBoxState STakeRecorderPanel::GetTakeBrowserCheckState() const
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<SDockTab> TakesBrowserTab = LevelEditorModule.GetLevelEditorTabManager()->FindExistingLiveTab(ITakeRecorderModule::TakesBrowserTabName);
	if (TakesBrowserTab.IsValid())
	{
		return TakesBrowserTab->IsForeground() ? ECheckBoxState::Checked : ECheckBoxState::Undetermined;
	}
	return ECheckBoxState::Unchecked;
}

void STakeRecorderPanel::ToggleTakeBrowserCheckState(ECheckBoxState CheckState)
{
	// If it is up, but not visible, then bring it forward
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<SDockTab> TakesBrowserTab = LevelEditorModule.GetLevelEditorTabManager()->FindExistingLiveTab(ITakeRecorderModule::TakesBrowserTabName);
	if (TakesBrowserTab.IsValid())
	{
		if (!TakesBrowserTab->IsForeground())
		{
			TakesBrowserTab->ActivateInParent(ETabActivationCause::SetDirectly);
			TakesBrowserTab->FlashTab();
		}
		else
		{
			TakesBrowserTab->RequestCloseTab();
		}
	}
	else 
	{
		TakesBrowserTab = LevelEditorModule.GetLevelEditorTabManager()->InvokeTab(ITakeRecorderModule::TakesBrowserTabName);

		bool bAllowLockedBrowser =  true;
		bool bFocusContentBrowser = false;

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		FString TakesPath = GetTakeMetaData()->GenerateAssetPath(GetDefault<UTakeRecorderProjectSettings>()->Settings.GetTakeAssetPath());
		TakesPath = FPaths::GetPath(*TakesPath);

		while(!TakesPath.IsEmpty())
		{
			if (AssetRegistry.HasAssets(FName(*TakesPath), true))
			{
				break;
			}
			TakesPath = FPaths::GetPath(TakesPath);
		}

		TArray<FString> TakesFolder;
		TakesFolder.Push(TakesPath);
		if (AssetRegistry.HasAssets(FName(*TakesPath), true) )
		{
			FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().SyncBrowserToFolders(TakesFolder, bAllowLockedBrowser, bFocusContentBrowser, ITakeRecorderModule::TakesBrowserInstanceName );
		}

		TakesBrowserTab->FlashTab();
	}
}

#undef LOCTEXT_NAMESPACE