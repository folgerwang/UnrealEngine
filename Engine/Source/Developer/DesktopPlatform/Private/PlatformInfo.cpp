// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PlatformInfo.h"
#include "DesktopPlatformPrivate.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "PlatformInfo"

namespace PlatformInfo
{
	TArray<FName> AllPlatformGroupNames;
	TArray<FName> AllVanillaPlatformNames;

namespace
{

FPlatformInfo BuildPlatformInfo(const FName& InPlatformInfoName, const FName& InTargetPlatformName, const FText& InDisplayName, const EPlatformType InPlatformType, const EPlatformFlags::Flags InPlatformFlags, const FPlatformIconPaths& InIconPaths, const FString& InUATCommandLine, const FString& InAutoSDKPath, EPlatformSDKStatus InStatus, const FString& InTutorial, bool InEnabled, FString InBinaryFolderName, FString InIniPlatformName, bool InUsesHostCompiler, bool InUATClosesAfterLaunch, bool InIsConfidential, const FName& InUBTTargetId, const FName& InPlatformGroupName)
{
	FPlatformInfo PlatformInfo;

	PlatformInfo.PlatformInfoName = InPlatformInfoName;
	PlatformInfo.TargetPlatformName = InTargetPlatformName;

	// See if this name also contains a flavor
	const FString InPlatformInfoNameString = InPlatformInfoName.ToString();
	{
		int32 UnderscoreLoc;
		if(InPlatformInfoNameString.FindChar(TEXT('_'), UnderscoreLoc))
		{
			PlatformInfo.VanillaPlatformName = *InPlatformInfoNameString.Mid(0, UnderscoreLoc);
			PlatformInfo.PlatformFlavor = *InPlatformInfoNameString.Mid(UnderscoreLoc + 1);
		}
		else
		{
			PlatformInfo.VanillaPlatformName = InPlatformInfoName;
		}
	}

	if (PlatformInfo.VanillaPlatformName != NAME_None)
	{
		PlatformInfo::AllVanillaPlatformNames.AddUnique(PlatformInfo.VanillaPlatformName);
	}

	PlatformInfo.DisplayName = InDisplayName;
	PlatformInfo.PlatformType = InPlatformType;
	PlatformInfo.PlatformFlags = InPlatformFlags;
	PlatformInfo.IconPaths = InIconPaths;
	PlatformInfo.UATCommandLine = InUATCommandLine;
	PlatformInfo.AutoSDKPath = InAutoSDKPath;
	PlatformInfo.BinaryFolderName = InBinaryFolderName;
	PlatformInfo.IniPlatformName = InIniPlatformName;
	PlatformInfo.UBTTargetId = InUBTTargetId;
	PlatformInfo.PlatformGroupName = InPlatformGroupName;

	if (InPlatformGroupName != NAME_None)
	{
		PlatformInfo::AllPlatformGroupNames.AddUnique(InPlatformGroupName);
	}

	// Generate the icon style names for FEditorStyle
	PlatformInfo.IconPaths.NormalStyleName = *FString::Printf(TEXT("Launcher.Platform_%s"), *InPlatformInfoNameString);
	PlatformInfo.IconPaths.LargeStyleName  = *FString::Printf(TEXT("Launcher.Platform_%s.Large"), *InPlatformInfoNameString);
	PlatformInfo.IconPaths.XLargeStyleName = *FString::Printf(TEXT("Launcher.Platform_%s.XLarge"), *InPlatformInfoNameString);

	// SDK data
	PlatformInfo.SDKStatus = InStatus;
	PlatformInfo.SDKTutorial = InTutorial;

	// Distribution data
	PlatformInfo.bEnabledForUse = InEnabled;
	PlatformInfo.bUsesHostCompiler = InUsesHostCompiler;
	PlatformInfo.bUATClosesAfterLaunch = InUATClosesAfterLaunch;
	PlatformInfo.bIsConfidential = InIsConfidential;
	return PlatformInfo;
}

#if PLATFORM_WINDOWS
static const bool IsAvailableOnWindows = true;
static const bool IsAvailableOnMac = false;
static const bool IsAvailableOnLinux = false;
static const FString IOSTutorial = TEXT("/Engine/Tutorial/Mobile/InstallingiTunesTutorial.InstallingiTunesTutorial");
#elif PLATFORM_MAC
static const bool IsAvailableOnWindows = false;
static const bool IsAvailableOnMac = true;
static const bool IsAvailableOnLinux = false;
static const FString IOSTutorial = TEXT("/Engine/Tutorial/Installation/InstallingXCodeTutorial.InstallingXCodeTutorial");
#elif PLATFORM_LINUX
static const bool IsAvailableOnWindows = false;
static const bool IsAvailableOnMac = false;
static const bool IsAvailableOnLinux = true;
static const FString IOSTutorial = TEXT("/Engine/Tutorial/NotYetImplemented");
#else
static const bool IsAvailableOnWindows = false;
static const bool IsAvailableOnMac = false;
static const bool IsAvailableOnLinux = false;
static const FString IOSTutorial = TEXT("/Engine/Tutorial/NotYetImplemented");
#endif

static const FPlatformInfo AllPlatformInfoArray[] = {
	// PlatformInfoName									TargetPlatformName			DisplayName														PlatformType			PlatformFlags					IconPaths																																		UATCommandLine										AutoSDKPath			SDKStatus						SDKTutorial																								bEnabledForUse										BinaryFolderName	IniPlatformName		FbUsesHostCompiler		bUATClosesAfterLaunch	bIsConfidential	UBTTargetId (match UBT's UnrealTargetPlatform enum)
	BuildPlatformInfo(TEXT("WindowsNoEditor"),			TEXT("WindowsNoEditor"),	LOCTEXT("WindowsNoEditor", "Windows"),							EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Windows/Platform_WindowsNoEditor_24x"), TEXT("Launcher/Windows/Platform_WindowsNoEditor_128x")),	TEXT("-targetplatform=Win64"),						TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingVisualStudioTutorial.InstallingVisualStudioTutorial"),	IsAvailableOnWindows,											TEXT("Win64"),		TEXT("Windows"),	IsAvailableOnWindows,	false,					false,			TEXT("Win64"),		TEXT("Desktop")),
	BuildPlatformInfo(TEXT("WindowsNoEditor_Win32"),	TEXT("WindowsNoEditor"),	LOCTEXT("WindowsNoEditor_Win32", "Windows (32-bit)"),			EPlatformType::Game,	EPlatformFlags::BuildFlavor,	FPlatformIconPaths(TEXT("Launcher/Windows/Platform_WindowsNoEditor_24x"), TEXT("Launcher/Windows/Platform_WindowsNoEditor_128x")),	TEXT("-targetplatform=Win32"),						TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingVisualStudioTutorial.InstallingVisualStudioTutorial"),	IsAvailableOnWindows,											TEXT("Win32"),		TEXT("Windows"),	IsAvailableOnWindows,	false,					false,			TEXT("Win32"),		TEXT("Desktop")),
	BuildPlatformInfo(TEXT("WindowsNoEditor_Win64"),	TEXT("WindowsNoEditor"),	LOCTEXT("WindowsNoEditor_Win64", "Windows (64-bit)"),			EPlatformType::Game,	EPlatformFlags::BuildFlavor,	FPlatformIconPaths(TEXT("Launcher/Windows/Platform_WindowsNoEditor_24x"), TEXT("Launcher/Windows/Platform_WindowsNoEditor_128x")),	TEXT("-targetplatform=Win64"),						TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingVisualStudioTutorial.InstallingVisualStudioTutorial"),	IsAvailableOnWindows,											TEXT("Win64"),		TEXT("Windows"),	IsAvailableOnWindows,	false,					false,			TEXT("Win64"),		TEXT("Desktop")),
	BuildPlatformInfo(TEXT("Windows"),					TEXT("Windows"),			LOCTEXT("WindowsEditor", "Windows (Editor)"),					EPlatformType::Editor,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Windows/Platform_Windows_24x"), TEXT("Launcher/Windows/Platform_Windows_128x")),					TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingVisualStudioTutorial.InstallingVisualStudioTutorial"),	IsAvailableOnWindows,											TEXT("Win64"),		TEXT("Windows"),	IsAvailableOnWindows,	false,					false,			TEXT("Win64"),		TEXT("Desktop")),
	BuildPlatformInfo(TEXT("WindowsClient"),			TEXT("WindowsClient"),		LOCTEXT("WindowsClient", "Windows (Client-only)"),				EPlatformType::Client,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Windows/Platform_Windows_24x"), TEXT("Launcher/Windows/Platform_Windows_128x")),					TEXT("-client"),									TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingVisualStudioTutorial.InstallingVisualStudioTutorial"),	IsAvailableOnWindows,											TEXT("Win64"),		TEXT("Windows"),	IsAvailableOnWindows,	false,					false,			TEXT("Win64"),		TEXT("Desktop")),
	BuildPlatformInfo(TEXT("WindowsServer"),			TEXT("WindowsServer"),		LOCTEXT("WindowsServer", "Windows (Dedicated Server)"),			EPlatformType::Server,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Windows/Platform_WindowsServer_24x"), TEXT("Launcher/Windows/Platform_WindowsServer_128x")),		TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingVisualStudioTutorial.InstallingVisualStudioTutorial"),	IsAvailableOnWindows,											TEXT("Win64"),		TEXT("Windows"),	IsAvailableOnWindows,	false,					false,			TEXT("Win64"),		TEXT("Desktop")),

	BuildPlatformInfo(TEXT("MacNoEditor"),				TEXT("MacNoEditor"),		LOCTEXT("MacNoEditor", "Mac"),									EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Mac/Platform_Mac_24x"), TEXT("Launcher/Mac/Platform_Mac_128x")),									TEXT("-targetplatform=Mac"),						TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingXCodeTutorial.InstallingXCodeTutorial"),					IsAvailableOnMac,												TEXT("Mac"),		TEXT("Mac"),		IsAvailableOnMac,		true,					false,			TEXT("Mac"),		TEXT("Desktop")),
	BuildPlatformInfo(TEXT("Mac"),						TEXT("Mac"),				LOCTEXT("MacEditor", "Mac (Editor)"),							EPlatformType::Editor,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Mac/Platform_Mac_24x"), TEXT("Launcher/Mac/Platform_Mac_128x")),									TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingXCodeTutorial.InstallingXCodeTutorial"),					IsAvailableOnMac,												TEXT("Mac"),		TEXT("Mac"),		IsAvailableOnMac,		true,					false,			TEXT("Mac"),		TEXT("Desktop")),
	BuildPlatformInfo(TEXT("MacClient"),				TEXT("MacClient"),			LOCTEXT("MacClient", "Mac (Client-only)"),						EPlatformType::Client,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Mac/Platform_Mac_24x"), TEXT("Launcher/Mac/Platform_Mac_128x")),									TEXT("-client"),									TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingXCodeTutorial.InstallingXCodeTutorial"),					IsAvailableOnMac,												TEXT("Mac"),		TEXT("Mac"),		IsAvailableOnMac,		true,					false,			TEXT("Mac"),		TEXT("Desktop")),
	BuildPlatformInfo(TEXT("MacServer"),				TEXT("MacServer"),			LOCTEXT("MacServer", "Mac (Dedicated Server)"),					EPlatformType::Server,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Mac/Platform_Mac_24x"), TEXT("Launcher/Mac/Platform_Mac_128x")),									TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingXCodeTutorial.InstallingXCodeTutorial"),					IsAvailableOnMac,												TEXT("Mac"),		TEXT("Mac"),		IsAvailableOnMac,		true,					false,			TEXT("Mac"),		TEXT("Desktop")),

	BuildPlatformInfo(TEXT("LinuxNoEditor"),			TEXT("LinuxNoEditor"),		LOCTEXT("LinuxNoEditor", "Linux"),								EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Linux/Platform_Linux_24x"), TEXT("Launcher/Linux/Platform_Linux_128x")),							TEXT("-targetplatform=Linux"),						TEXT("Linux_x64"),	EPlatformSDKStatus::Unknown,	TEXT("/Platforms/Linux/GettingStarted"),																IsAvailableOnLinux || IsAvailableOnWindows,						TEXT("Linux"),		TEXT("Linux"),		IsAvailableOnLinux,		true,					false,			TEXT("Linux"),		TEXT("Desktop")),
	BuildPlatformInfo(TEXT("Linux"),					TEXT("Linux"),				LOCTEXT("LinuxEditor", "Linux (Editor)"),						EPlatformType::Editor,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Linux/Platform_Linux_24x"), TEXT("Launcher/Linux/Platform_Linux_128x")),							TEXT(""),											TEXT("Linux_x64"),	EPlatformSDKStatus::Unknown,	TEXT("/Platforms/Linux/GettingStarted"),																IsAvailableOnLinux,												TEXT("Linux"),		TEXT("Linux"),		IsAvailableOnLinux,		true,					false,			TEXT("Linux"),		TEXT("Desktop")),
	BuildPlatformInfo(TEXT("LinuxClient"),				TEXT("LinuxClient"),		LOCTEXT("LinuxClient", "Linux (Client-only)"),					EPlatformType::Client,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Linux/Platform_Linux_24x"), TEXT("Launcher/Linux/Platform_Linux_128x")),							TEXT("-client"),									TEXT("Linux_x64"),	EPlatformSDKStatus::Unknown,	TEXT("/Platforms/Linux/GettingStarted"),																IsAvailableOnLinux || IsAvailableOnWindows,						TEXT("Linux"),		TEXT("Linux"),		IsAvailableOnLinux,		true,					false,			TEXT("Linux"),		TEXT("Desktop")),
	BuildPlatformInfo(TEXT("LinuxServer"),				TEXT("LinuxServer"),		LOCTEXT("LinuxServer", "Linux (Dedicated Server)"),				EPlatformType::Server,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Linux/Platform_Linux_24x"), TEXT("Launcher/Linux/Platform_Linux_128x")),							TEXT(""),											TEXT("Linux_x64"),	EPlatformSDKStatus::Unknown,	TEXT("/Platforms/Linux/GettingStarted"),																IsAvailableOnLinux || IsAvailableOnWindows,						TEXT("Linux"),		TEXT("Linux"),		IsAvailableOnLinux,		true,					false,			TEXT("Linux"),		TEXT("Desktop")),

	BuildPlatformInfo(TEXT("IOS"),						TEXT("IOS"),				LOCTEXT("IOS", "iOS"),											EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/iOS/Platform_iOS_24x"), TEXT("Launcher/iOS/Platform_iOS_128x")),									TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	IOSTutorial,																							IsAvailableOnWindows || IsAvailableOnMac,						TEXT("IOS"),		TEXT("IOS"),		false,					true,					false,			TEXT("IOS"),		TEXT("Mobile")),
	BuildPlatformInfo(TEXT("IOSClient"),				TEXT("IOSClient"),			LOCTEXT("IOSClient", "iOSClient"),								EPlatformType::Client,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/iOS/Platform_iOS_24x"), TEXT("Launcher/iOS/Platform_iOS_128x")),									TEXT("-client"),									TEXT(""),			EPlatformSDKStatus::Unknown,	IOSTutorial,																							IsAvailableOnWindows || IsAvailableOnMac,						TEXT("IOS"),		TEXT("IOS"),		false,					true,					false,			TEXT("IOS"),		TEXT("Mobile")),

	BuildPlatformInfo(TEXT("Android"),					TEXT("Android"),			LOCTEXT("Android", "Android"),									EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_24x"), TEXT("Launcher/Android/Platform_Android_128x")),					TEXT(""),											TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),						IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android"),	TEXT("Mobile")),
	BuildPlatformInfo(TEXT("Android_Multi"),			TEXT("Android_Multi"),		LOCTEXT("Android_Multi", "Android (Multi)"),					EPlatformType::Game,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_24x"), TEXT("Launcher/Android/Platform_Android_128x")),					TEXT("-targetplatform=Android -cookflavor=Multi"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),						IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android"),	TEXT("Mobile")),
	BuildPlatformInfo(TEXT("Android_ATC"),				TEXT("Android_ATC"),		LOCTEXT("Android_ATC", "Android (ATC)"),						EPlatformType::Game,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_ATC_24x"), TEXT("Launcher/Android/Platform_Android_128x")),				TEXT("-targetplatform=Android -cookflavor=ATC"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),						IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android"),	TEXT("Mobile")),
	BuildPlatformInfo(TEXT("Android_DXT"),				TEXT("Android_DXT"),		LOCTEXT("Android_DXT", "Android (DXT)"),						EPlatformType::Game,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_DXT_24x"), TEXT("Launcher/Android/Platform_Android_128x")),				TEXT("-targetplatform=Android -cookflavor=DXT"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),						IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android"),	TEXT("Mobile")),
	BuildPlatformInfo(TEXT("Android_ETC1"),				TEXT("Android_ETC1"),		LOCTEXT("Android_ETC1", "Android (ETC1)"),						EPlatformType::Game,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_ETC1_24x"), TEXT("Launcher/Android/Platform_Android_128x")),				TEXT("-targetplatform=Android -cookflavor=ETC1"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),						IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android"),	TEXT("Mobile")),
	BuildPlatformInfo(TEXT("Android_ETC1a"),			TEXT("Android_ETC1a"),		LOCTEXT("Android_ETC1a", "Android (ETC1a)"),					EPlatformType::Game,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_ETC1_24x"), TEXT("Launcher/Android/Platform_Android_128x")),				TEXT("-targetplatform=Android -cookflavor=ETC1a"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),						IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android"),	TEXT("Mobile")),
	BuildPlatformInfo(TEXT("Android_ETC2"),				TEXT("Android_ETC2"),		LOCTEXT("Android_ETC2", "Android (ETC2)"),						EPlatformType::Game,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_ETC2_24x"), TEXT("Launcher/Android/Platform_Android_128x")),				TEXT("-targetplatform=Android -cookflavor=ETC2"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),						IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android"),	TEXT("Mobile")),
	BuildPlatformInfo(TEXT("Android_PVRTC"),			TEXT("Android_PVRTC"),		LOCTEXT("Android_PVRTC", "Android (PVRTC)"),					EPlatformType::Game,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_PVRTC_24x"), TEXT("Launcher/Android/Platform_Android_128x")),			TEXT("-targetplatform=Android -cookflavor=PVRTC"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),						IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android"),	TEXT("Mobile")),
	BuildPlatformInfo(TEXT("Android_ASTC"),				TEXT("Android_ASTC"),		LOCTEXT("Android_ASTC", "Android (ASTC)"),						EPlatformType::Game,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_ASTC_24x"), TEXT("Launcher/Android/Platform_Android_128x")),				TEXT("-targetplatform=Android -cookflavor=ASTC"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),						IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android"),	TEXT("Mobile")),

	BuildPlatformInfo(TEXT("AndroidClient"),			TEXT("AndroidClient"),		LOCTEXT("AndroidClient", "Android (Client-only)"),				EPlatformType::Client,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_24x"), TEXT("Launcher/Android/Platform_Android_128x")),					TEXT("-client"),									TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),						IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android"),	TEXT("Mobile")),
	BuildPlatformInfo(TEXT("Android_MultiClient"),		TEXT("Android_MultiClient"),LOCTEXT("Android_MultiClient", "Android (Multi) (Client-only)"),EPlatformType::Client,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_24x"), TEXT("Launcher/Android/Platform_Android_128x")),					TEXT("-client -targetplatform=Android -cookflavor=Multi"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),				IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android"),	TEXT("Mobile")),
	BuildPlatformInfo(TEXT("Android_ATCClient"),		TEXT("Android_ATCClient"),	LOCTEXT("Android_ATCClient", "Android (ATC) (Client-only)"),	EPlatformType::Client,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_ATC_24x"), TEXT("Launcher/Android/Platform_Android_128x")),				TEXT("-client -targetplatform=Android -cookflavor=ATC"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),				IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android"),	TEXT("Mobile")),
	BuildPlatformInfo(TEXT("Android_DXTClient"),		TEXT("Android_DXTClient"),	LOCTEXT("Android_DXTClient", "Android (DXT) (Client-only)"),	EPlatformType::Client,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_DXT_24x"), TEXT("Launcher/Android/Platform_Android_128x")),				TEXT("-client -targetplatform=Android -cookflavor=DXT"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),				IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android"),	TEXT("Mobile")),
	BuildPlatformInfo(TEXT("Android_ETC1Client"),		TEXT("Android_ETC1Client"),	LOCTEXT("Android_ETC1Client", "Android (ETC1) (Client-only)"),	EPlatformType::Client,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_ETC1_24x"), TEXT("Launcher/Android/Platform_Android_128x")),				TEXT("-client -targetplatform=Android -cookflavor=ETC1"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),				IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android"),	TEXT("Mobile")),
	BuildPlatformInfo(TEXT("Android_ETC1aClient"),		TEXT("Android_ETC1aClient"),LOCTEXT("Android_ETC1aClient", "Android (ETC1a) (Client-only)"),EPlatformType::Client,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_ETC1_24x"), TEXT("Launcher/Android/Platform_Android_128x")),				TEXT("-client -targetplatform=Android -cookflavor=ETC1a"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),				IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android"),	TEXT("Mobile")),
	BuildPlatformInfo(TEXT("Android_ETC2Client"),		TEXT("Android_ETC2Client"),	LOCTEXT("Android_ETC2Client", "Android (ETC2) (Client-only)"),	EPlatformType::Client,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_ETC2_24x"), TEXT("Launcher/Android/Platform_Android_128x")),				TEXT("-client -targetplatform=Android -cookflavor=ETC2"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),				IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android"),	TEXT("Mobile")),
	BuildPlatformInfo(TEXT("Android_PVRTCClient"),		TEXT("Android_PVRTCClient"),LOCTEXT("Android_PVRTCClient", "Android (PVRTC) (Client-only)"),EPlatformType::Client,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_PVRTC_24x"), TEXT("Launcher/Android/Platform_Android_128x")),			TEXT("-client -targetplatform=Android -cookflavor=PVRTC"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),				IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android"),	TEXT("Mobile")),
	BuildPlatformInfo(TEXT("Android_ASTCClient"),		TEXT("Android_ASTCClient"),	LOCTEXT("Android_ASTCClient", "Android (ASTC) (Client-only)"),	EPlatformType::Client,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_ASTC_24x"), TEXT("Launcher/Android/Platform_Android_128x")),				TEXT("-client -targetplatform=Android -cookflavor=ASTC"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),				IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android"),	TEXT("Mobile")),

	BuildPlatformInfo(TEXT("HTML5"),					TEXT("HTML5"),				LOCTEXT("HTML5", "HTML5"),										EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/HTML5/Platform_HTML5_24x"), TEXT("Launcher/HTML5/Platform_HTML5_128x")),							TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Platforms/HTML5/GettingStarted"),																IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("HTML5"),		TEXT("HTML5"),		false,					true,					false,			TEXT("HTML5"),		TEXT("Mobile")),

	BuildPlatformInfo(TEXT("PS4"),						TEXT("PS4"),				LOCTEXT("PS4", "PlayStation 4"),								EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/PS4/Platform_PS4_24x"), TEXT("Launcher/PS4/Platform_PS4_128x")),									TEXT(""),											TEXT("PS4"),		EPlatformSDKStatus::Unknown,	TEXT("/Platforms/PS4/GettingStarted"),																	IsAvailableOnWindows,											TEXT("PS4"),		TEXT("PS4"),		false,					false,					true,			TEXT("PS4"),		TEXT("Console")),

	BuildPlatformInfo(TEXT("XboxOne"),					TEXT("XboxOne"),			LOCTEXT("XboxOne", "Xbox One"),									EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/XboxOne/Platform_XboxOne_24x"), TEXT("Launcher/XboxOne/Platform_XboxOne_128x")),					TEXT(""),											TEXT("XboxOne"),	EPlatformSDKStatus::Unknown,	TEXT("/Platforms/XboxOne/GettingStarted"),																IsAvailableOnWindows,											TEXT("XboxOne"),	TEXT("XboxOne"),	false,					true,					true,			TEXT("XboxOne"),	TEXT("Console")),

	BuildPlatformInfo(TEXT("AllDesktop"),				TEXT("AllDesktop"),			LOCTEXT("DesktopTargetPlatDisplay", "Desktop (Win+Mac+Linux)"),	EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Desktop/Platform_Desktop_24x"), TEXT("Launcher/Desktop/Platform_Desktop_128x")),					TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT(""),																								IsAvailableOnWindows /* see note below */,						TEXT(""),			TEXT(""),			false,					true,					false,			TEXT("AllDesktop"),	TEXT("Desktop")),

	BuildPlatformInfo(TEXT("TVOS"),						TEXT("TVOS"),				LOCTEXT("TVOSTargetPlatDisplay", "tvOS"),						EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/TVOS/Platform_TVOS_24x"), TEXT("Launcher/TVOS/Platform_TVOS_128x")),								TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT(""),																								IsAvailableOnWindows || IsAvailableOnMac,						TEXT("TVOS"),		TEXT("TVOS"),		false,					true,					false,			TEXT("TVOS"),		TEXT("Mobile")),
	BuildPlatformInfo(TEXT("TVOSClient"),				TEXT("TVOSClient"),			LOCTEXT("TVOSTargetPlatDisplayClient", "tvOSClient"),			EPlatformType::Client,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/TVOS/Platform_TVOS_24x"), TEXT("Launcher/TVOS/Platform_TVOS_128x")),								TEXT("-client"),									TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT(""),																								IsAvailableOnWindows || IsAvailableOnMac,						TEXT("TVOS"),		TEXT("TVOS"),		false,					true,					false,			TEXT("TVOS"),		TEXT("Mobile")),
	BuildPlatformInfo(TEXT("Switch"),					TEXT("Switch"),				LOCTEXT("Switch", "Switch"),									EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Switch/Platform_Switch_24x"), TEXT("Launcher/Switch/Platform_Switch_128x")),						TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT(""),																								IsAvailableOnWindows,											TEXT("Switch"),		TEXT("Switch"),		false,					true,					true,			TEXT("Switch"),		TEXT("Console")),
	BuildPlatformInfo(TEXT("Lumin"),					TEXT("Lumin"),				LOCTEXT("Lumin", "Lumin"),										EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/LuminTarget/Platform_Lumin_24x"), TEXT("Launcher/Lumin/Platform_Lumin_128x")),					TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpLuminTutorial.SettingUpLuminTutorial"),							IsAvailableOnWindows || IsAvailableOnMac,						TEXT("Lumin"),		TEXT("Lumin"),		false,					true,					false,			TEXT("Lumin"),		TEXT("Mobile")),
	BuildPlatformInfo(TEXT("LuminClient"),				TEXT("LuminClient"),		LOCTEXT("LuminClient", "Lumin (Client-only)"),					EPlatformType::Client,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/LuminTarget/Platform_Lumin_24x"), TEXT("Launcher/Lumin/Platform_Lumin_128x")),					TEXT("-client"),									TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpLuminTutorial.SettingUpLuminTutorial"),							IsAvailableOnWindows || IsAvailableOnMac,						TEXT("Lumin"),		TEXT("Lumin"),		false,					true,					false,			TEXT("Lumin"),		TEXT("Mobile")),

	BuildPlatformInfo(TEXT("Quail"),					TEXT("Quail"),				LOCTEXT("Quail", "Quail"),										EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Quail/Platform_Quail_24x"), TEXT("Launcher/Quail/Platform_Quail_128x")),							TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT(""),																								IsAvailableOnWindows,											TEXT("Quail"),		TEXT("Quail"),		false,					false,					true,			TEXT("Quail"),		TEXT("")),
	BuildPlatformInfo(TEXT("QuailClient"),				TEXT("QuailClient"),		LOCTEXT("QuailClient", "Quail (Client-only)"),					EPlatformType::Client,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Quail/Platform_Quail_24x"), TEXT("Launcher/Quail/Platform_Quail_128x")),							TEXT("-client -targetplatform=Quail"),				TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT(""),																								IsAvailableOnWindows,											TEXT("Quail"),		TEXT("Quail"),		false,					false,					true,			TEXT("Quail"),		TEXT("")),

	// Note: For "AllDesktop" bEnabledForUse value, see SProjectTargetPlatformSettings::Construct !!!! IsAvailableOnWindows || IsAvailableOnMac || IsAvailableOnLinux
};

} // anonymous namespace

const FPlatformInfo* FindPlatformInfo(const FName& InPlatformName)
{
	for(const FPlatformInfo& PlatformInfo : AllPlatformInfoArray)
	{
		if(PlatformInfo.PlatformInfoName == InPlatformName)
		{
			return &PlatformInfo;
		}
	}
	return nullptr;
}

const FPlatformInfo* FindVanillaPlatformInfo(const FName& InPlatformName)
{
	const FPlatformInfo* const FoundInfo = FindPlatformInfo(InPlatformName);
	return (FoundInfo) ? (FoundInfo->IsVanilla()) ? FoundInfo : FindPlatformInfo(FoundInfo->VanillaPlatformName) : nullptr;
}

const FPlatformInfo* GetPlatformInfoArray(int32& OutNumPlatforms)
{
	OutNumPlatforms = ARRAY_COUNT(AllPlatformInfoArray);
	return AllPlatformInfoArray;
}

void UpdatePlatformSDKStatus(FString InPlatformName, EPlatformSDKStatus InStatus)
{
	for(const FPlatformInfo& PlatformInfo : AllPlatformInfoArray)
	{
		if(PlatformInfo.VanillaPlatformName == FName(*InPlatformName))
		{
			const_cast<FPlatformInfo&>(PlatformInfo).SDKStatus = InStatus;
		}
	}
}

void UpdatePlatformDisplayName(FString InPlatformName, FText InDisplayName)
{
	for (const FPlatformInfo& PlatformInfo : AllPlatformInfoArray)
	{
		if (PlatformInfo.TargetPlatformName == FName(*InPlatformName))
		{
			const_cast<FPlatformInfo&>(PlatformInfo).DisplayName = InDisplayName;
		}
	}
}

FPlatformEnumerator EnumeratePlatformInfoArray(bool bAccessiblePlatformsOnly)
{
	if (bAccessiblePlatformsOnly)
	{
		static bool bHasSearchedForPlatforms = false;
		static TArray<FPlatformInfo> AccessiblePlatforms;

		if (!bHasSearchedForPlatforms)
		{
			FPlatformEnumerator Enumerator(AllPlatformInfoArray, ARRAY_COUNT(AllPlatformInfoArray));

			const TArray<FString>& ConfidentalPlatforms = FDataDrivenPlatformInfoRegistry::GetConfidentialPlatforms();

			for (const FPlatformInfo& PlatformInfo : Enumerator)
			{
				const FString IniFolderPath = FPaths::RootDir() / TEXT("Engine") / TEXT("Config") / PlatformInfo.IniPlatformName;
				TArray<FString> FoundFiles;
				IFileManager::Get().FindFiles(FoundFiles, *IniFolderPath);
			
				// We check that the configuration directory exists with actual files in it to include the platform
				// P4/Git may have filtered out platforms and we don't want to include filtered platform to keep code from trying to load other files that don't exist.
				if (FoundFiles.Num() > 0)
				{
					if (PlatformInfo.bIsConfidential && ConfidentalPlatforms.Contains(PlatformInfo.IniPlatformName))
					{
						AccessiblePlatforms.Add(PlatformInfo);
					}
					else if (!PlatformInfo.bIsConfidential)
					{
						AccessiblePlatforms.Add(PlatformInfo);
					}
				}
			}

			bHasSearchedForPlatforms = true;
		}

		return FPlatformEnumerator(AccessiblePlatforms.GetData(), AccessiblePlatforms.Num());
	}
	else
	{

		return FPlatformEnumerator(AllPlatformInfoArray, ARRAY_COUNT(AllPlatformInfoArray));
	}
}

TArray<FVanillaPlatformEntry> BuildPlatformHierarchy(const EPlatformFilter InFilter, bool bAccessiblePlatformsOnly)
{
	TArray<FVanillaPlatformEntry> VanillaPlatforms;

	// Build up a tree from the platforms we support (vanilla outers, with a list of flavors)
	// PlatformInfoArray should be ordered in such a way that the vanilla platforms always appear before their flavors
	for (const PlatformInfo::FPlatformInfo& PlatformInfo : EnumeratePlatformInfoArray(bAccessiblePlatformsOnly))
	{
		if(PlatformInfo.IsVanilla())
		{
			VanillaPlatforms.Add(FVanillaPlatformEntry(&PlatformInfo));
		}
		else
		{
			const bool bHasBuildFlavor = !!(PlatformInfo.PlatformFlags & EPlatformFlags::BuildFlavor);
			const bool bHasCookFlavor  = !!(PlatformInfo.PlatformFlags & EPlatformFlags::CookFlavor);
			
			const bool bValidFlavor = 
				InFilter == EPlatformFilter::All || 
				(InFilter == EPlatformFilter::BuildFlavor && bHasBuildFlavor) || 
				(InFilter == EPlatformFilter::CookFlavor && bHasCookFlavor);

			if(bValidFlavor)
			{
				const FName VanillaPlatformName = PlatformInfo.VanillaPlatformName;
				FVanillaPlatformEntry* const VanillaEntry = VanillaPlatforms.FindByPredicate([VanillaPlatformName](const FVanillaPlatformEntry& Item) -> bool
				{
					return Item.PlatformInfo->PlatformInfoName == VanillaPlatformName;
				});
				check(VanillaEntry);
				VanillaEntry->PlatformFlavors.Add(&PlatformInfo);
			}
		}
	}

	return VanillaPlatforms;
}

FVanillaPlatformEntry BuildPlatformHierarchy(const FName& InPlatformName, const EPlatformFilter InFilter, bool bAccessiblePlatformsOnly)
{
	FVanillaPlatformEntry VanillaPlatformEntry;
	const FPlatformInfo* VanillaPlatformInfo = FindVanillaPlatformInfo(InPlatformName);
	
	if (VanillaPlatformInfo)
	{
		VanillaPlatformEntry.PlatformInfo = VanillaPlatformInfo;
		
		for (const PlatformInfo::FPlatformInfo& PlatformInfo : EnumeratePlatformInfoArray(bAccessiblePlatformsOnly))
		{
			if (!PlatformInfo.IsVanilla() && PlatformInfo.VanillaPlatformName == VanillaPlatformInfo->PlatformInfoName)
			{
				const bool bHasBuildFlavor = !!(PlatformInfo.PlatformFlags & EPlatformFlags::BuildFlavor);
				const bool bHasCookFlavor = !!(PlatformInfo.PlatformFlags & EPlatformFlags::CookFlavor);

				const bool bValidFlavor =
					InFilter == EPlatformFilter::All ||
					(InFilter == EPlatformFilter::BuildFlavor && bHasBuildFlavor) ||
					(InFilter == EPlatformFilter::CookFlavor && bHasCookFlavor);

				if (bValidFlavor)
				{
					VanillaPlatformEntry.PlatformFlavors.Add(&PlatformInfo);
				}
			}
		}
	}
	
	return VanillaPlatformEntry;
}

EPlatformType EPlatformTypeFromString(const FString& PlatformTypeName)
{
	if (FCString::Strcmp(*PlatformTypeName, TEXT("Game")) == 0)
	{
		return PlatformInfo::EPlatformType::Game;
	}
	else if (FCString::Strcmp(*PlatformTypeName, TEXT("Editor")) == 0)
	{
		return PlatformInfo::EPlatformType::Editor;
	}
	else if (FCString::Strcmp(*PlatformTypeName, TEXT("Client")) == 0)
	{
		return PlatformInfo::EPlatformType::Client;
	}
	else if (FCString::Strcmp(*PlatformTypeName, TEXT("Server")) == 0)
	{
		return PlatformInfo::EPlatformType::Server;
	}
	else
	{
		UE_LOG(LogDesktopPlatform, Warning, TEXT("Unable to read Platform Type from %s, defaulting to Game"), *PlatformTypeName);
		return PlatformInfo::EPlatformType::Game;
	}
}

const TArray<FName>& GetAllPlatformGroupNames()
{
	return PlatformInfo::AllPlatformGroupNames;
}

const TArray<FName>& GetAllVanillaPlatformNames()
{
	return PlatformInfo::AllVanillaPlatformNames;
}

} // namespace PlatformInfo

FString LexToString(const PlatformInfo::EPlatformType Value)
{
	switch (Value)
	{
	case PlatformInfo::EPlatformType::Game:
		return TEXT("Game");
	case PlatformInfo::EPlatformType::Editor:
		return TEXT("Editor");
	case PlatformInfo::EPlatformType::Client:
		return TEXT("Client");
	case PlatformInfo::EPlatformType::Server:
		return TEXT("Server");
	}

	return TEXT("");
}

#undef LOCTEXT_NAMESPACE
