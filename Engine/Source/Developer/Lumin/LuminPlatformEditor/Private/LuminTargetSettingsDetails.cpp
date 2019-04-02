// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LuminTargetSettingsDetails.h"

#include "SExternalImageReference.h"
#include "EditorDirectories.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "UObject/UnrealType.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "SPlatformSetupMessage.h"
#include "DetailCategoryBuilder.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "SourceControlHelpers.h"
#include "LuminRuntimeSettings.h"
#include "Logging/LogMacros.h"
#include "PropertyPathHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogLuminTargetSettingsDetail, Log, All);

#define LOCTEXT_NAMESPACE "LuminTargetSettingsDetails"

TSharedRef<IDetailCustomization> FLuminTargetSettingsDetails::MakeInstance()
{
	return MakeShareable(new FLuminTargetSettingsDetails);
}

FLuminTargetSettingsDetails::FLuminTargetSettingsDetails()
	: DefaultIconModelPath(FPaths::EngineDir() / TEXT("Build/Lumin/Resources/Model"))
	, DefaultIconPortalPath(FPaths::EngineDir() / TEXT("Build/Lumin/Resources/Portal"))
	, GameLuminPath(FPaths::ProjectDir() / TEXT("Build/Lumin"))
	, GameProjectSetupPath(GameLuminPath / TEXT("IconSetup.txt"))
	, SavedLayoutBuilder(nullptr)
{
}

void FLuminTargetSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	SavedLayoutBuilder = &DetailBuilder;

	IconModelPathProp = DetailBuilder.GetProperty("IconModelPath.Path");
	IconPortalPathProp = DetailBuilder.GetProperty("IconPortalPath.Path");
	CertificateProp = DetailBuilder.GetProperty("Certificate.FilePath");

	IconModelPathAttribute.Bind(TAttribute<FString>::FGetter::CreateRaw(this, &FLuminTargetSettingsDetails::IconModelPathGetter));
	IconPortalPathAttribute.Bind(TAttribute<FString>::FGetter::CreateRaw(this, &FLuminTargetSettingsDetails::IconPortalPathGetter));
	CertificateAttribute.Bind(TAttribute<FString>::FGetter::CreateRaw(this, &FLuminTargetSettingsDetails::CertificateGetter));

	BuildAudioSection(DetailBuilder);
	BuildAppTileSection(DetailBuilder);
}

FString FLuminTargetSettingsDetails::IconModelPathGetter()
{
	FString Result;
	if (IconModelPathProp->GetValue(Result) == FPropertyAccess::Success && !Result.IsEmpty())
	{
		if (SetupForPlatformAttribute.Get())
		{
			// If we did the setup, the values are game/project dir relative. So we root them at that
			// for display. Otherwise we show the default values which will show engine exec relative
			// paths to the platform build resource locations.
			Result = FPaths::ProjectDir() / Result;
		}
		else
		{
			// Otherwise they are in the engine tree. But they could be outdate paths.
			// In which case we use the hard-wired defaults.
			if (FPaths::DirectoryExists(FPaths::EngineDir() / Result))
			{
				Result = FPaths::EngineDir() / Result;
			}
			else
			{
				Result = DefaultIconModelPath;
			}
		}
	}
	return Result;
}

FString FLuminTargetSettingsDetails::IconPortalPathGetter()
{
	FString Result;
	if (IconPortalPathProp->GetValue(Result) == FPropertyAccess::Success && !Result.IsEmpty())
	{
		if (SetupForPlatformAttribute.Get())
		{
			// If we did the setup, the values are game/project dir relative. So we root them at that
			// for display. Otherwise we show the default values which will show engine exec relative
			// paths to the platform build resource locations.
			Result = FPaths::ProjectDir() / Result;
		}
		else
		{
			// Otherwise they are in the engine tree. But they could be outdate paths.
			// In which case we use the hard-wired defaults.
			if (FPaths::DirectoryExists(FPaths::EngineDir() / Result))
			{
				Result = FPaths::EngineDir() / Result;
			}
			else
			{
				Result = DefaultIconPortalPath;
			}
		}
	}
	return Result;
}

FString FLuminTargetSettingsDetails::CertificateGetter()
{
	FString Result;
	if (CertificateProp->GetValue(Result) == FPropertyAccess::Success && !Result.IsEmpty())
	{
		Result = FPaths::ProjectDir() / Result;
	}
	return Result;
}

void FLuminTargetSettingsDetails::CopySetupFilesIntoProject()
{
	// Start out with the hard-wired default paths.
	FString SourceModelPath = DefaultIconModelPath;
	FString SourcePortalPath = DefaultIconPortalPath;
	// Override with soft-wired defaults from the engine config. These are going to be engine
	// root relative as we are copying from the engine tree to the project tree.
	// But only override if those soft-wired engine paths exists. If they don't it's likely
	// some old invalid value. This prevents someone from using obsolete data.
	FString PropSourceModelPath;
	FString PropSourcePortalPath;
	if (IconModelPathProp->GetValue(PropSourceModelPath) == FPropertyAccess::Success && !PropSourceModelPath.IsEmpty() && FPaths::DirectoryExists(FPaths::EngineDir() / PropSourceModelPath))
	{
		SourceModelPath = FPaths::EngineDir() / PropSourceModelPath;
	}
	if (IconPortalPathProp->GetValue(PropSourcePortalPath) == FPropertyAccess::Success && !PropSourcePortalPath.IsEmpty() && FPaths::DirectoryExists(FPaths::EngineDir() / PropSourcePortalPath))
	{
		SourcePortalPath = FPaths::EngineDir() / PropSourcePortalPath;
	}
	const FString TargetModelPath = GameLuminPath / TEXT("Model");
	const FString TargetPortalPath = GameLuminPath / TEXT("Portal");
	bool DidModelCopy = CopyDir(*SourceModelPath, *TargetModelPath);
	bool DidPortalCopy = CopyDir(*SourcePortalPath, *TargetPortalPath);
	if (DidModelCopy && DidPortalCopy)
	{
		// Touch the setup file to indicate we did the copies.
		delete IPlatformFile::GetPlatformPhysical().OpenWrite(*GameProjectSetupPath);
		// And set the icon path config vars to the project directory now that we have it.
		// This makes it so that the packaging will use these instead of the engine
		// files directly. The values for both are fixed to the project root relative locations.
		if (IconModelPathProp->SetValue(FString("Build/Lumin/Model")) != FPropertyAccess::Success ||
			IconPortalPathProp->SetValue(FString("Build/Lumin/Portal")) != FPropertyAccess::Success)
		{
			UE_LOG(LogLuminTargetSettingsDetail, Error, TEXT("Failed to update icon or portal "));
		}
	}
}

void FLuminTargetSettingsDetails::BuildAudioSection(IDetailLayoutBuilder& DetailBuilder)
{
	AudioPluginManager.BuildAudioCategory(DetailBuilder, EAudioPlatform::Lumin);
}

void FLuminTargetSettingsDetails::BuildAppTileSection(IDetailLayoutBuilder& DetailBuilder)
{
	////////// UI for icons..

	IDetailCategoryBuilder& AppTitleCategory = DetailBuilder.EditCategory(TEXT("Magic Leap App Tile"));
	DetailBuilder.HideProperty("IconModelPath");
	DetailBuilder.HideProperty("IconPortalPath");
	TSharedRef<SPlatformSetupMessage> PlatformSetupMessage = SNew(SPlatformSetupMessage, GameProjectSetupPath)
		.PlatformName(LOCTEXT("LuminPlatformName", "Magic Leap"))
		.OnSetupClicked(FSimpleDelegate::CreateLambda([this] { this->CopySetupFilesIntoProject(); }));
	SetupForPlatformAttribute = PlatformSetupMessage->GetReadyToGoAttribute();
	AppTitleCategory.AddCustomRow(LOCTEXT("Warning", "Warning"), false)
		.WholeRowWidget
		[
			PlatformSetupMessage
		];
	AppTitleCategory.AddCustomRow(LOCTEXT("BuildFolderLabel", "Build Folder"), false)
		.IsEnabled(SetupForPlatformAttribute)
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 1, 0, 1))
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BuildFolderLabel", "Build Folder"))
				.Font(DetailBuilder.GetDetailFont())
			]
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("OpenBuildFolderButton", "Open Build Folder"))
				.ToolTipText(LOCTEXT("OpenManifestFolderButton_Tooltip", "Opens the folder containing the build files in Explorer or Finder (it's recommended you check these in to source control to share with your team)"))
				.OnClicked(this, &FLuminTargetSettingsDetails::OpenBuildFolder)
			]
		];
	BuildPathPicker(DetailBuilder, AppTitleCategory, IconModelPathAttribute,
		LOCTEXT("IconModelLabel", "Icon Model"),
		LOCTEXT("PickIconModelButton_Tooltip", "Select the icon model to use for the application. The files will be copied to the project build folder."),
		FOnChoosePath::CreateRaw(this, &FLuminTargetSettingsDetails::OnPickDirectory),
		FOnPickPath::CreateRaw(this, &FLuminTargetSettingsDetails::OnPickIconModelPath));
	BuildPathPicker(DetailBuilder, AppTitleCategory, IconPortalPathAttribute,
		LOCTEXT("IconPortalLabel", "Icon Portal"),
		LOCTEXT("PickIconPortalButton_Tooltip", "Select the icon portal to use for the application. The files will be copied to the project build folder."),
		FOnChoosePath::CreateRaw(this, &FLuminTargetSettingsDetails::OnPickDirectory),
		FOnPickPath::CreateRaw(this, &FLuminTargetSettingsDetails::OnPickIconPortalPath));

	////////// UI for signing cert..

	IDetailCategoryBuilder& DistributionSigningCategory = DetailBuilder.EditCategory(TEXT("Distribution Signing"));
	DetailBuilder.HideProperty("Certificate");
	BuildPathPicker(DetailBuilder, DistributionSigningCategory, CertificateAttribute,
		LOCTEXT("CertificateFilePathLabel", "Certificate File Path"),
		LOCTEXT("PickCertificateButton_Tooltip", "Select the certificate to use for signing a distribution package. The file will be copied to the project build folder."),
		FOnChoosePath::CreateLambda([this](TAttribute<FString> FilePath, const FOnPickPath& OnPick, TSharedPtr<SButton> PickButton)->FReply
		{
			const FString FilterText = LOCTEXT("CertificateFile", "Certificate File").ToString();
			return this->OnPickFile(FilePath, OnPick, PickButton,
				LOCTEXT("PickCertificateFileDialogTitle", "Choose a certificate").ToString(),
				FString::Printf(TEXT("%s (*.cert)|*.cert"), *FilterText));
		}),
		FOnPickPath::CreateRaw(this, &FLuminTargetSettingsDetails::OnPickCertificate));
}

FReply FLuminTargetSettingsDetails::OnPickIconModelPath(const FString& DirPath)
{
	FString ProjectModelPath = GameLuminPath / TEXT("Model");
	if (ProjectModelPath != DirPath)
	{
		// Copy the contents of the selected path to the project build path.
		CopyDir(DirPath, ProjectModelPath);
	}
	return FReply::Handled();
}

FReply FLuminTargetSettingsDetails::OnPickIconPortalPath(const FString& DirPath)
{
	FString ProjectPortalPath = GameLuminPath / TEXT("Portal");
	if (ProjectPortalPath != DirPath)
	{
		// Copy the contents of the selected path to the project build path.
		CopyDir(DirPath, ProjectPortalPath);
	}
	return FReply::Handled();
}

bool FLuminTargetSettingsDetails::CopyDir(FString SourceDir, FString TargetDir)
{
	FPaths::NormalizeDirectoryName(SourceDir);
	FPaths::NormalizeDirectoryName(TargetDir);
	if (!IPlatformFile::GetPlatformPhysical().DirectoryExists(*SourceDir))
	{
		return false;
	}
	// The source control utilities only deal with single files at a time. Hence need to collect
	// the files we are copying and copy each one in turn.
	TArray<FString> FilesToCopy;
	int FilesCopiedCount = 0;
	IPlatformFile::GetPlatformPhysical().FindFilesRecursively(FilesToCopy, *SourceDir, nullptr);
	FText Description = FText::FromString(FPaths::GetBaseFilename(TargetDir));
	for (FString& FileToCopy : FilesToCopy)
	{
		FString NewFile = FileToCopy.Replace(*SourceDir, *TargetDir);
		if (IPlatformFile::GetPlatformPhysical().FileExists(*FileToCopy))
		{
			FString ToCopySubDir = FPaths::GetPath(FileToCopy);
			if (!IPlatformFile::GetPlatformPhysical().DirectoryExists(*ToCopySubDir))
			{
				IPlatformFile::GetPlatformPhysical().CreateDirectoryTree(*ToCopySubDir);
			}
			FText ErrorMessage;
			if (SourceControlHelpers::CopyFileUnderSourceControl(NewFile, FileToCopy, Description, ErrorMessage))
			{
				FilesCopiedCount += 1;
			}
			else
			{
				FNotificationInfo Info(ErrorMessage);
				Info.ExpireDuration = 3.0f;
				FSlateNotificationManager::Get().AddNotification(Info);
			}
		}
	}
	return true;
}

void FLuminTargetSettingsDetails::BuildPathPicker(
	IDetailLayoutBuilder& DetailBuilder, IDetailCategoryBuilder& Category,
	TAttribute<FString> Path, const FText& Label, const FText& Tooltip,
	const FOnChoosePath& OnChoose, const FOnPickPath& OnPick)
{
	TSharedPtr<SButton> PickButton = nullptr;
	TSharedPtr<SWidget> PickWidget = nullptr;
	PickWidget = SAssignNew(PickButton, SButton)
		.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
		.ToolTipText(Tooltip)
		.ContentPadding(2.0f)
		.ForegroundColor(FSlateColor::UseForeground())
		.IsFocusable(false)
		[
			SNew(SImage)
			.Image(FEditorStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
	;
	PickButton->SetOnClicked(FOnClicked::CreateLambda([OnChoose, OnPick, Path, PickButton]()->FReply
	{
		return OnChoose.Execute(Path.Get(), OnPick, PickButton);
	}));

	Category.AddCustomRow(Label, false)
		.IsEnabled(SetupForPlatformAttribute)
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 1, 0, 1))
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(DetailBuilder.GetDetailFont())
			]
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(TAttribute<FText>::Create(
					TAttribute<FText>::FGetter::CreateLambda([Path]()->FText { return FText::FromString(Path.Get()); })))
				.Font(DetailBuilder.GetDetailFont())
				.Margin(2.0f)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				PickWidget.ToSharedRef()
			]
		];
}

FReply FLuminTargetSettingsDetails::OnPickDirectory(TAttribute<FString> DirPath, const FLuminTargetSettingsDetails::FOnPickPath& OnPick, TSharedPtr<SButton> PickButton)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(PickButton.ToSharedRef());
		void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;
		FString StartDirectory = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_IMPORT);
		FString Directory;
		if (DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, LOCTEXT("FolderDialogTitle", "Choose a directory").ToString(), StartDirectory, Directory))
		{
			FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_IMPORT, Directory);
			return OnPick.Execute(Directory);
		}
	}
	return FReply::Handled();
}

FReply FLuminTargetSettingsDetails::OnPickFile(
	TAttribute<FString> FilePath, const FLuminTargetSettingsDetails::FOnPickPath& OnPick,
	TSharedPtr<SButton> PickButton, const FString & Title, const FString & Filter)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(PickButton.ToSharedRef());
		void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;
		FString StartDirectory = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_IMPORT);
		TArray<FString> OutFiles;
		if (DesktopPlatform->OpenFileDialog(
			ParentWindowHandle,
			Title,
			FilePath.Get(),
			TEXT(""),
			Filter,
			EFileDialogFlags::None,
			OutFiles))
		{
			return OnPick.Execute(OutFiles[0]);
		}
	}
	return FReply::Handled();
}

FReply FLuminTargetSettingsDetails::OpenBuildFolder()
{
	const FString BuildFolder = FPaths::ConvertRelativePathToFull(GameLuminPath);
	FPlatformProcess::ExploreFolder(*BuildFolder);

	return FReply::Handled();
}

FReply FLuminTargetSettingsDetails::OnPickCertificate(const FString& SourceCertificateFile)
{
	FString TargetCertificateFile = GameLuminPath / FPaths::GetCleanFilename(SourceCertificateFile);
	if (!IPlatformFile::GetPlatformPhysical().FileExists(*SourceCertificateFile))
	{
		// Sanity check for chosen file. Do nothing if it doesn't exist.
		return FReply::Handled();
	}
	// We only ask for the certificate file.. But we also need the accompanying private key file.
	FString SourceKeyFile = FPaths::Combine(FPaths::GetPath(SourceCertificateFile), FPaths::GetBaseFilename(SourceCertificateFile) + TEXT(".privkey"));
	FString TargetKeyFile = GameLuminPath / FPaths::GetCleanFilename(SourceKeyFile);
	if (!IPlatformFile::GetPlatformPhysical().FileExists(*SourceKeyFile))
	{
		// We really need the key file.
		FNotificationInfo Info(FText(LOCTEXT("LuminMissingPrivKeyFile", "Could not find private key file.")));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return FReply::Handled();
	}
	{
		FText Description = FText::FromString(FPaths::GetBaseFilename(TargetCertificateFile));
		FText ErrorMessage;
		if (!SourceControlHelpers::CopyFileUnderSourceControl(TargetCertificateFile, SourceCertificateFile, Description, ErrorMessage))
		{
			FNotificationInfo Info(ErrorMessage);
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}
	{
		FText Description = FText::FromString(FPaths::GetBaseFilename(TargetKeyFile));
		FText ErrorMessage;
		if (!SourceControlHelpers::CopyFileUnderSourceControl(TargetKeyFile, SourceKeyFile, Description, ErrorMessage))
		{
			FNotificationInfo Info(ErrorMessage);
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
			return FReply::Handled();
		}
	}
	CertificateProp->SetValue(TargetCertificateFile.Replace(*FPaths::ProjectDir(), TEXT("")));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
