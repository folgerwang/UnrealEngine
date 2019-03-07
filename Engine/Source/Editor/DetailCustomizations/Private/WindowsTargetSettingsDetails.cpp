// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "WindowsTargetSettingsDetails.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Layout/Margin.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "EditorDirectories.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "SExternalImageReference.h"
#include "UnrealEngine.h"

#if WITH_ENGINE
#include "AudioDevice.h"
#endif 

#define LOCTEXT_NAMESPACE "WindowsTargetSettingsDetails"

namespace WindowsTargetSettingsDetailsConstants
{
	/** The filename for the game splash screen */
	const FString GameSplashFileName(TEXT("Splash/Splash.bmp"));

	/** The filename for the editor splash screen */
	const FString EditorSplashFileName(TEXT("Splash/EdSplash.bmp"));

	/** ToolTip used when an option is not available to binary users. */
	const FText DisabledTip = LOCTEXT("GitHubSourceRequiredToolTip", "This requires GitHub source.");
}

static FText GetFriendlyNameFromWindowsRHIName(const FString& InRHIName)
{
	FText FriendlyRHIName;
	if (InRHIName == TEXT("PCD3D_SM5"))
	{
		FriendlyRHIName = LOCTEXT("DirectX11", "DirectX 11 & 12 (SM5)");
	}
	else if (InRHIName == TEXT("PCD3D_SM4"))
	{
		FriendlyRHIName = LOCTEXT("DirectX10", "DirectX 10 (SM4)");
	}
	else if (InRHIName == TEXT("GLSL_430"))
	{
		FriendlyRHIName = LOCTEXT("OpenGL4", "OpenGL 4 (SM5, Experimental)");
	}
	else if (InRHIName == TEXT("SF_VULKAN_SM5"))
	{
		FriendlyRHIName = LOCTEXT("VulkanSM5", "Vulkan (SM5, Experimental)");
	}
	else if (InRHIName == TEXT("GLSL_SWITCH"))
	{
		FriendlyRHIName = LOCTEXT("Switch", "Switch (Deferred)");
	}
	else if (InRHIName == TEXT("GLSL_SWITCH_FORWARD"))
	{
		FriendlyRHIName = LOCTEXT("SwitchForward", "Switch (Forward)");
	}
	else if (InRHIName == TEXT("GLSL_150_ES2") || InRHIName == TEXT("GLSL_150_ES31") || InRHIName == TEXT("GLSL_150")
		|| InRHIName == TEXT("SF_VULKAN_ES31_ANDROID") || InRHIName == TEXT("SF_VULKAN_ES31")
		|| InRHIName == TEXT("SF_VULKAN_SM4") || InRHIName == TEXT("PCD3D_ES2") || InRHIName == TEXT("PCD3D_ES31"))
	{
		// Explicitly remove these formats as they are obsolete/not quite supported; users can still target them by adding them as +TargetedRHIs in the TargetPlatform ini.
		FriendlyRHIName = FText::GetEmpty();
	}
	else
	{
		UE_LOG(LogEngine, Warning, TEXT("Unknown Windows target RHI %s"), *InRHIName);
		FriendlyRHIName = LOCTEXT("UnknownRHI", "UnknownRHI");
	}

	return FriendlyRHIName;
}


TSharedRef<IDetailCustomization> FWindowsTargetSettingsDetails::MakeInstance()
{
	return MakeShareable(new FWindowsTargetSettingsDetails);
}

namespace EWindowsImageScope
{
	enum Type
	{
		Engine,
		GameOverride
	};
}

/* Helper function used to generate filenames for splash screens */
static FString GetWindowsSplashFilename(EWindowsImageScope::Type Scope, bool bIsEditorSplash)
{
	FString Filename;

	if (Scope == EWindowsImageScope::Engine)
	{
		Filename = FPaths::EngineContentDir();
	}
	else
	{
		Filename = FPaths::ProjectContentDir();
	}

	if(bIsEditorSplash)
	{
		Filename /= WindowsTargetSettingsDetailsConstants::EditorSplashFileName;
	}
	else
	{
		Filename /= WindowsTargetSettingsDetailsConstants::GameSplashFileName;
	}

	Filename = FPaths::ConvertRelativePathToFull(Filename);

	return Filename;
}

/* Helper function used to generate filenames for icons */
static FString GetWindowsIconFilename(EWindowsImageScope::Type Scope)
{
	const FString& PlatformName = FModuleManager::GetModuleChecked<ITargetPlatformModule>("WindowsTargetPlatform").GetTargetPlatforms()[0]->PlatformName();

	if (Scope == EWindowsImageScope::Engine)
	{
		FString Filename = FPaths::EngineDir() / FString(TEXT("Build/Windows/Resources/Default.ico"));
		return FPaths::ConvertRelativePathToFull(Filename);
	}
	else
	{
		FString Filename = FPaths::ProjectDir() / TEXT("Build/Windows/Application.ico");
		if(!FPaths::FileExists(Filename))
		{
			FString LegacyFilename = FPaths::GameSourceDir() / FString(FApp::GetProjectName()) / FString(TEXT("Resources")) / PlatformName / FString(FApp::GetProjectName()) + TEXT(".ico");
			if(FPaths::FileExists(LegacyFilename))
			{
				Filename = LegacyFilename;
			}
		}
		return FPaths::ConvertRelativePathToFull(Filename);
	}
}

void FWindowsTargetSettingsDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	// Setup the supported/targeted RHI property view
	ITargetPlatform* TargetPlatform = FModuleManager::GetModuleChecked<ITargetPlatformModule>("WindowsTargetPlatform").GetTargetPlatforms()[0];
	TargetShaderFormatsDetails = MakeShareable(new FShaderFormatsPropertyDetails(&DetailBuilder));
	TargetShaderFormatsDetails->CreateTargetShaderFormatsPropertyView(TargetPlatform, GetFriendlyNameFromWindowsRHIName);

	TSharedRef<IPropertyHandle> MinOSProperty = DetailBuilder.GetProperty("MinimumOSVersion");
	IDetailCategoryBuilder& OSInfoCategory = DetailBuilder.EditCategory(TEXT("OS Info"));
	
	// Setup edit condition and tool tip of Min OS property. Determined by whether the engine is installed or not.
	bool bIsMinOSSelectionAvailable = FApp::IsEngineInstalled() == false;
	IDetailPropertyRow& MinOSRow = OSInfoCategory.AddProperty(MinOSProperty);
	MinOSRow.IsEnabled(bIsMinOSSelectionAvailable);
	MinOSRow.ToolTip(bIsMinOSSelectionAvailable ? MinOSProperty->GetToolTipText() : WindowsTargetSettingsDetailsConstants::DisabledTip);

	// Next add the splash image customization
	const FText EditorSplashDesc(LOCTEXT("EditorSplashLabel", "Editor Splash"));
	IDetailCategoryBuilder& SplashCategoryBuilder = DetailBuilder.EditCategory(TEXT("Splash"));
	FDetailWidgetRow& EditorSplashWidgetRow = SplashCategoryBuilder.AddCustomRow(EditorSplashDesc);

	const FString EditorSplash_TargetImagePath = GetWindowsSplashFilename(EWindowsImageScope::GameOverride, true);
	const FString EditorSplash_DefaultImagePath = GetWindowsSplashFilename(EWindowsImageScope::Engine, true);

	TArray<FString> ImageExtensions;
	ImageExtensions.Add(TEXT("png"));
	ImageExtensions.Add(TEXT("jpg"));
	ImageExtensions.Add(TEXT("bmp"));

	EditorSplashWidgetRow
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(EditorSplashDesc)
			.Font(DetailBuilder.GetDetailFont())
		]
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SExternalImageReference, EditorSplash_DefaultImagePath, EditorSplash_TargetImagePath)
			.FileDescription(EditorSplashDesc)
			.OnGetPickerPath(FOnGetPickerPath::CreateSP(this, &FWindowsTargetSettingsDetails::GetPickerPath))
			.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateSP(this, &FWindowsTargetSettingsDetails::HandlePostExternalIconCopy))
			.DeleteTargetWhenDefaultChosen(true)
			.FileExtensions(ImageExtensions)
			.DeletePreviousTargetWhenExtensionChanges(true)
		]
	];

	const FText GameSplashDesc(LOCTEXT("GameSplashLabel", "Game Splash"));
	FDetailWidgetRow& GameSplashWidgetRow = SplashCategoryBuilder.AddCustomRow(GameSplashDesc);
	const FString GameSplash_TargetImagePath = GetWindowsSplashFilename(EWindowsImageScope::GameOverride, false);
	const FString GameSplash_DefaultImagePath = GetWindowsSplashFilename(EWindowsImageScope::Engine, false);

	GameSplashWidgetRow
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(GameSplashDesc)
			.Font(DetailBuilder.GetDetailFont())
		]
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SExternalImageReference, GameSplash_DefaultImagePath, GameSplash_TargetImagePath)
			.FileDescription(GameSplashDesc)
			.OnGetPickerPath(FOnGetPickerPath::CreateSP(this, &FWindowsTargetSettingsDetails::GetPickerPath))
			.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateSP(this, &FWindowsTargetSettingsDetails::HandlePostExternalIconCopy))
			.DeleteTargetWhenDefaultChosen(true)
			.FileExtensions(ImageExtensions)
			.DeletePreviousTargetWhenExtensionChanges(true)
		]
	];

	IDetailCategoryBuilder& IconsCategoryBuilder = DetailBuilder.EditCategory(TEXT("Icon"));	
	FDetailWidgetRow& GameIconWidgetRow = IconsCategoryBuilder.AddCustomRow(LOCTEXT("GameIconLabel", "Game Icon"));
	GameIconWidgetRow
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GameIconLabel", "Game Icon"))
			.Font(DetailBuilder.GetDetailFont())
		]
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SExternalImageReference, GetWindowsIconFilename(EWindowsImageScope::Engine), GetWindowsIconFilename(EWindowsImageScope::GameOverride))
			.FileDescription(GameSplashDesc)
			.OnPreExternalImageCopy(FOnPreExternalImageCopy::CreateSP(this, &FWindowsTargetSettingsDetails::HandlePreExternalIconCopy))
			.OnGetPickerPath(FOnGetPickerPath::CreateSP(this, &FWindowsTargetSettingsDetails::GetPickerPath))
			.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateSP(this, &FWindowsTargetSettingsDetails::HandlePostExternalIconCopy))
		]
	];

	TSharedPtr<IPropertyHandle> AudioDevicePropertyHandle = DetailBuilder.GetProperty("AudioDevice");
	IDetailCategoryBuilder& AudioDeviceCategory = DetailBuilder.EditCategory("Audio");
	IDetailPropertyRow& AudioDevicePropertyRow = AudioDeviceCategory.AddProperty(AudioDevicePropertyHandle);

	AudioDevicePropertyRow.CustomWidget()
	.NameContent()
	[
		AudioDevicePropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SEditableTextBox)
			.ForegroundColor(this, &FWindowsTargetSettingsDetails::HandleAudioDeviceBoxForegroundColor, AudioDevicePropertyHandle)
			.OnTextChanged(this, &FWindowsTargetSettingsDetails::HandleAudioDeviceTextBoxTextChanged, AudioDevicePropertyHandle)
			.OnTextCommitted(this, &FWindowsTargetSettingsDetails::HandleAudioDeviceTextBoxTextComitted, AudioDevicePropertyHandle)
			.Text(this, &FWindowsTargetSettingsDetails::HandleAudioDeviceTextBoxText, AudioDevicePropertyHandle)
			.ToolTipText(AudioDevicePropertyHandle->GetToolTipText())
		]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SComboButton)
				.ButtonContent()
				[
					SNullWidget::NullWidget
				]
				.ContentPadding(FMargin(6.0f, 1.0f))
				.MenuContent()
				[
					MakeAudioDeviceMenu(AudioDevicePropertyHandle)
				]
				.ToolTipText(LOCTEXT("AudioDevicesButtonToolTip", "Pick from the list of available audio devices"))
			]
	];
	AudioPluginWidgetManager.BuildAudioCategory(DetailBuilder, EAudioPlatform::Windows);
}

bool FWindowsTargetSettingsDetails::HandlePreExternalIconCopy(const FString& InChosenImage)
{
	return true;
}


FString FWindowsTargetSettingsDetails::GetPickerPath()
{
	return FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN);
}


bool FWindowsTargetSettingsDetails::HandlePostExternalIconCopy(const FString& InChosenImage)
{
	FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_OPEN, FPaths::GetPath(InChosenImage));
	return true;
}

void FWindowsTargetSettingsDetails::HandleAudioDeviceSelected(FString AudioDeviceName, TSharedPtr<IPropertyHandle> PropertyHandle)
{
	PropertyHandle->SetValue(AudioDeviceName);
}

FSlateColor FWindowsTargetSettingsDetails::HandleAudioDeviceBoxForegroundColor(TSharedPtr<IPropertyHandle> PropertyHandle) const
{
	FString Value;

	if (PropertyHandle->GetValue(Value) == FPropertyAccess::Success)
	{
		if (Value.IsEmpty() || IsValidAudioDeviceName(Value))
		{
			static const FName InvertedForegroundName("InvertedForeground");

			// Return a valid slate color for a valid audio device
			return FEditorStyle::GetSlateColor(InvertedForegroundName);
		}
	}

	// Return Red, which means its an invalid audio device
	return FLinearColor::Red;
}

FText FWindowsTargetSettingsDetails::HandleAudioDeviceTextBoxText(TSharedPtr<IPropertyHandle> PropertyHandle) const
{
	FString Value;

	if (PropertyHandle->GetValue(Value) == FPropertyAccess::Success)
	{
		FString WindowsAudioDeviceName;
		GConfig->GetString(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("AudioDevice"), WindowsAudioDeviceName, GEngineIni);
		return FText::FromString(WindowsAudioDeviceName);
	}

	return FText::GetEmpty();
}

void FWindowsTargetSettingsDetails::HandleAudioDeviceTextBoxTextChanged(const FText& InText, TSharedPtr<IPropertyHandle> PropertyHandle)
{
	PropertyHandle->SetValue(InText.ToString());
}

void FWindowsTargetSettingsDetails::HandleAudioDeviceTextBoxTextComitted(const FText& InText, ETextCommit::Type CommitType, TSharedPtr<IPropertyHandle> PropertyHandle)
{
	FString Value;

	// Clear the property if its not valid
	if ((PropertyHandle->GetValue(Value) != FPropertyAccess::Success) || !IsValidAudioDeviceName(Value))
	{
		PropertyHandle->SetValue(FString());
	}
}

bool FWindowsTargetSettingsDetails::IsValidAudioDeviceName(const FString& InDeviceName) const
{
	bool bIsValid = false;

#if WITH_ENGINE
	FAudioDevice* AudioDevice = GEngine->GetMainAudioDevice();
	if (AudioDevice)
	{
		TArray<FString> DeviceNames;
		AudioDevice->GetAudioDeviceList(DeviceNames);

		for (FString& DeviceName : DeviceNames)
		{
			if (InDeviceName == DeviceName)
			{
				bIsValid = true;
				break;
			}
		}
	}
#endif

	return bIsValid;
}

TSharedRef<SWidget> FWindowsTargetSettingsDetails::MakeAudioDeviceMenu(const TSharedPtr<IPropertyHandle>& PropertyHandle)
{
	FMenuBuilder MenuBuilder(true, nullptr);

#if WITH_ENGINE
	FAudioDevice* AudioDevice = GEngine->GetMainAudioDevice();
	if (AudioDevice)
	{
		TArray<FString> AudioDeviceNames;
		AudioDevice->GetAudioDeviceList(AudioDeviceNames);

		// Construct the custom menu widget from the list of device names
		MenuBuilder.BeginSection(NAME_None, LOCTEXT("AudioDevicesSectionHeader", "Audio Devices"));
		{
			for (int32 i = 0; i < AudioDeviceNames.Num(); i++)
			{
				FUIAction Action(FExecuteAction::CreateRaw(this, &FWindowsTargetSettingsDetails::HandleAudioDeviceSelected, AudioDeviceNames[i], PropertyHandle));
				MenuBuilder.AddMenuEntry(
					FText::FromString(AudioDeviceNames[i]),
					FText::FromString(TEXT("")),
					FSlateIcon(),
					Action
					);
			}
		}
		MenuBuilder.EndSection();
	}
#endif

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
