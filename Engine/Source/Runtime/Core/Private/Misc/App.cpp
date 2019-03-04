// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Misc/App.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Runtime/Launch/Resources/Version.h"
#include "HAL/LowLevelMemTracker.h"
#include "BuildSettings.h"
#include "UObject/DevObjectVersion.h"
#include "Misc/EngineVersion.h"
#include "Misc/NetworkVersion.h"

DEFINE_LOG_CATEGORY_STATIC(LogApp, Log, All);

/* FApp static initialization
 *****************************************************************************/

#if UE_BUILD_DEVELOPMENT
bool FApp::bIsDebugGame = false;
#endif

FGuid FApp::InstanceId = FGuid::NewGuid();
FGuid FApp::SessionId = FGuid::NewGuid();
FString FApp::SessionName = FString();
FString FApp::SessionOwner = FString();
FString FApp::GraphicsRHI = FString();
TArray<FString> FApp::SessionUsers = TArray<FString>();
bool FApp::Standalone = true;
bool FApp::bIsBenchmarking = false;
bool FApp::bUseFixedSeed = false;
bool FApp::bUseFixedTimeStep = false;
double FApp::FixedDeltaTime = 1 / 30.0;
double FApp::CurrentTime = 0.0;
double FApp::LastTime = 0.0;
double FApp::DeltaTime = 1 / 30.0;
double FApp::IdleTime = 0.0;
double FApp::IdleTimeOvershoot = 0.0;
FTimecode FApp::Timecode = FTimecode();
FFrameRate FApp::TimecodeFrameRate = FFrameRate(60,1);
float FApp::VolumeMultiplier = 1.0f;
float FApp::UnfocusedVolumeMultiplier = 0.0f;
bool FApp::bUseVRFocus = false;
bool FApp::bHasVRFocus = false;


/* FApp static interface
 *****************************************************************************/

FString FApp::GetBranchName()
{
	return FString(BuildSettings::GetBranchName());
}

const TCHAR* FApp::GetBuildVersion()
{
	return BuildSettings::GetBuildVersion();
}

int32 FApp::GetEngineIsPromotedBuild()
{
	return BuildSettings::IsPromotedBuild()? 1 : 0;
}


FString FApp::GetEpicProductIdentifier()
{
	return FString(TEXT(EPIC_PRODUCT_IDENTIFIER));
}

EBuildConfigurations::Type FApp::GetBuildConfiguration()
{
#if UE_BUILD_DEBUG
	return EBuildConfigurations::Debug;

#elif UE_BUILD_DEVELOPMENT
	return bIsDebugGame ? EBuildConfigurations::DebugGame : EBuildConfigurations::Development;

#elif UE_BUILD_SHIPPING
	return EBuildConfigurations::Shipping;

#elif UE_BUILD_TEST
	return EBuildConfigurations::Test;

#else
	return EBuildConfigurations::Unknown;
#endif
}

#if UE_BUILD_DEVELOPMENT
void FApp::SetDebugGame(bool bInIsDebugGame)
{
	bIsDebugGame = bInIsDebugGame;
}
#endif

FString FApp::GetBuildDate()
{
	return FString(BuildSettings::GetBuildDate());
}

FString FApp::GetGraphicsRHI()
{
	return GraphicsRHI;
}

void FApp::SetGraphicsRHI(FString RHIString)
{
	GraphicsRHI = RHIString;
}

void FApp::InitializeSession()
{
	// parse session details on command line
	FString InstanceIdString;

	if (FParse::Value(FCommandLine::Get(), TEXT("-InstanceId="), InstanceIdString))
	{
		if (!FGuid::Parse(InstanceIdString, InstanceId))
		{
			UE_LOG(LogInit, Warning, TEXT("Invalid InstanceId on command line: %s"), *InstanceIdString);
		}
	}

	if (!InstanceId.IsValid())
	{
		InstanceId = FGuid::NewGuid();
	}

	FString SessionIdString;

	if (FParse::Value(FCommandLine::Get(), TEXT("-SessionId="), SessionIdString))
	{
		if (FGuid::Parse(SessionIdString, SessionId))
		{
			Standalone = false;
		}
		else
		{
			UE_LOG(LogInit, Warning, TEXT("Invalid SessionId on command line: %s"), *SessionIdString);
		}
	}

	FParse::Value(FCommandLine::Get(), TEXT("-SessionName="), SessionName);

	if (!FParse::Value(FCommandLine::Get(), TEXT("-SessionOwner="), SessionOwner))
	{
		SessionOwner = FPlatformProcess::UserName(false);
	}
}


bool FApp::IsInstalled()
{
	static int32 InstalledState = -1;

	if (InstalledState == -1)
	{
#if UE_BUILD_SHIPPING && PLATFORM_DESKTOP && !UE_SERVER
		bool bIsInstalled = true;
#else
		bool bIsInstalled = false;
#endif

#if PLATFORM_DESKTOP
		FString InstalledProjectBuildFile = FPaths::RootDir() / TEXT("Engine/Build/InstalledProjectBuild.txt");
		FPaths::NormalizeFilename(InstalledProjectBuildFile);
		bIsInstalled |= IFileManager::Get().FileExists(*InstalledProjectBuildFile);
#endif

		// Allow commandline options to disable/enable installed engine behavior
		if (bIsInstalled)
		{
			bIsInstalled = !FParse::Param(FCommandLine::Get(), TEXT("NotInstalled"));
		}
		else
		{
			bIsInstalled = FParse::Param(FCommandLine::Get(), TEXT("Installed"));
		}
		InstalledState = bIsInstalled ? 1 : 0;
	}
	return InstalledState == 1;
}


bool FApp::IsEngineInstalled()
{
	static int32 EngineInstalledState = -1;

	if (EngineInstalledState == -1)
	{
		bool bIsInstalledEngine = IsInstalled();

#if PLATFORM_DESKTOP
		FString InstalledBuildFile = FPaths::RootDir() / TEXT("Engine/Build/InstalledBuild.txt");
		FPaths::NormalizeFilename(InstalledBuildFile);
		bIsInstalledEngine |= IFileManager::Get().FileExists(*InstalledBuildFile);
#endif

		// Allow commandline options to disable/enable installed engine behavior
		if (bIsInstalledEngine)
		{
			bIsInstalledEngine = !FParse::Param(FCommandLine::Get(), TEXT("NotInstalledEngine"));
		}
		else
		{
			bIsInstalledEngine = FParse::Param(FCommandLine::Get(), TEXT("InstalledEngine"));
		}
		EngineInstalledState = bIsInstalledEngine ? 1 : 0;
	}

	return EngineInstalledState == 1;
}

bool FApp::IsEnterpriseInstalled()
{
	static int32 EnterpriseInstalledState = -1;

	if (EnterpriseInstalledState == -1)
	{
		bool bIsInstalledEnterprise = false;

#if PLATFORM_DESKTOP
		FString InstalledBuildFile = FPaths::RootDir() / TEXT("Enterprise/Build/InstalledBuild.txt");
		FPaths::NormalizeFilename(InstalledBuildFile);
		bIsInstalledEnterprise |= IFileManager::Get().FileExists(*InstalledBuildFile);
#endif

		// Allow commandline options to disable/enable installed engine behavior
		if (bIsInstalledEnterprise)
		{
			bIsInstalledEnterprise = !FParse::Param(FCommandLine::Get(), TEXT("NotInstalledEnterprise"));
		}
		else
		{
			bIsInstalledEnterprise = FParse::Param(FCommandLine::Get(), TEXT("InstalledEnterprise"));
		}
		EnterpriseInstalledState = bIsInstalledEnterprise ? 1 : 0;
	}

	return EnterpriseInstalledState == 1;
}

#if PLATFORM_WINDOWS && defined(__clang__)
bool FApp::IsUnattended() // @todo clang: Workaround for missing symbol export
{
	static bool bIsUnattended = FParse::Param(FCommandLine::Get(), TEXT("UNATTENDED"));
	return bIsUnattended || GIsAutomationTesting;
}
#endif

#if HAVE_RUNTIME_THREADING_SWITCHES

#if PLATFORM_LUMIN
#define MIN_CORE_COUNT 2
#else
#define MIN_CORE_COUNT 4
#endif

bool FApp::ShouldUseThreadingForPerformance()
{
	static bool OnlyOneThread = 
		FParse::Param(FCommandLine::Get(), TEXT("ONETHREAD")) ||
		FParse::Param(FCommandLine::Get(), TEXT("noperfthreads")) ||
		IsRunningDedicatedServer() ||
		!FPlatformProcess::SupportsMultithreading() ||
		FPlatformMisc::NumberOfCoresIncludingHyperthreads() < MIN_CORE_COUNT;
	return !OnlyOneThread;
}
#undef MIN_CORE_COUNT

#endif // HAVE_RUNTIME_THREADING_SWITCHES

static bool GUnfocusedVolumeMultiplierInitialised = false;
float FApp::GetUnfocusedVolumeMultiplier()
{
	if (!GUnfocusedVolumeMultiplierInitialised)
	{
		GUnfocusedVolumeMultiplierInitialised = true;
		GConfig->GetFloat(TEXT("Audio"), TEXT("UnfocusedVolumeMultiplier"), UnfocusedVolumeMultiplier, GEngineIni);
	}
	return UnfocusedVolumeMultiplier;
}

void FApp::SetUnfocusedVolumeMultiplier(float InVolumeMultiplier)
{
	UnfocusedVolumeMultiplier = InVolumeMultiplier;
	GConfig->SetFloat(TEXT("Audio"), TEXT("UnfocusedVolumeMultiplier"), UnfocusedVolumeMultiplier, GEngineIni);
	GUnfocusedVolumeMultiplierInitialised = true;
}

void FApp::SetUseVRFocus(bool bInUseVRFocus)
{
	UE_CLOG(bUseVRFocus != bInUseVRFocus, LogApp, Verbose, TEXT("UseVRFocus has changed to %d"), int(bInUseVRFocus));
	bUseVRFocus = bInUseVRFocus;
}

void FApp::SetHasVRFocus(bool bInHasVRFocus)
{
	UE_CLOG(bHasVRFocus != bInHasVRFocus, LogApp, Verbose, TEXT("HasVRFocus has changed to %d"), int(bInHasVRFocus));
	bHasVRFocus = bInHasVRFocus;
}

void FApp::PrintStartupLogMessages()
{
	UE_LOG(LogInit, Log, TEXT("Build: %s"), FApp::GetBuildVersion());
	UE_LOG(LogInit, Log, TEXT("Engine Version: %s"), *FEngineVersion::Current().ToString());
	UE_LOG(LogInit, Log, TEXT("Compatible Engine Version: %s"), *FEngineVersion::CompatibleWith().ToString());
	UE_LOG(LogInit, Log, TEXT("Net CL: %u"), FNetworkVersion::GetNetworkCompatibleChangelist());
	FString OSLabel, OSVersion;
	FPlatformMisc::GetOSVersions(OSLabel, OSVersion);
	UE_LOG(LogInit, Log, TEXT("OS: %s (%s), CPU: %s, GPU: %s"), *OSLabel, *OSVersion, *FPlatformMisc::GetCPUBrand(), *FPlatformMisc::GetPrimaryGPUBrand());

#if PLATFORM_64BITS
	UE_LOG(LogInit, Log, TEXT("Compiled (64-bit): %s %s"), ANSI_TO_TCHAR(__DATE__), ANSI_TO_TCHAR(__TIME__));
#else
	UE_LOG(LogInit, Log, TEXT("Compiled (32-bit): %s %s"), ANSI_TO_TCHAR(__DATE__), ANSI_TO_TCHAR(__TIME__));
#endif

	// Print compiler version info
#if defined(__clang__)
	UE_LOG(LogInit, Log, TEXT("Compiled with Clang: %s"), ANSI_TO_TCHAR(__clang_version__));
#elif defined(__INTEL_COMPILER)
	UE_LOG(LogInit, Log, TEXT("Compiled with ICL: %d"), __INTEL_COMPILER);
#elif defined( _MSC_VER )
#ifndef __INTELLISENSE__	// Intellisense compiler doesn't support _MSC_FULL_VER
	{
		const FString VisualCPPVersion(FString::Printf(TEXT("%d"), _MSC_FULL_VER));
		const FString VisualCPPRevisionNumber(FString::Printf(TEXT("%02d"), _MSC_BUILD));
		UE_LOG(LogInit, Log, TEXT("Compiled with Visual C++: %s.%s.%s.%s"),
			*VisualCPPVersion.Mid(0, 2), // Major version
			*VisualCPPVersion.Mid(2, 2), // Minor version
			*VisualCPPVersion.Mid(4),	// Build version
			*VisualCPPRevisionNumber	// Revision number
		);
	}
#endif
#else
	UE_LOG(LogInit, Log, TEXT("Compiled with unrecognized C++ compiler"));
#endif

	UE_LOG(LogInit, Log, TEXT("Build Configuration: %s"), EBuildConfigurations::ToString(FApp::GetBuildConfiguration()));
	UE_LOG(LogInit, Log, TEXT("Branch Name: %s"), *FApp::GetBranchName());
	FString FilteredString = FCommandLine::IsCommandLineLoggingFiltered() ? TEXT("Filtered ") : TEXT("");
	UE_LOG(LogInit, Log, TEXT("%sCommand Line: %s"), *FilteredString, FCommandLine::GetForLogging());
	UE_LOG(LogInit, Log, TEXT("Base Directory: %s"), FPlatformProcess::BaseDir());
	//UE_LOG(LogInit, Log, TEXT("Character set: %s"), sizeof(TCHAR)==1 ? TEXT("ANSI") : TEXT("Unicode") );
	UE_LOG(LogInit, Log, TEXT("Installed Engine Build: %d"), FApp::IsEngineInstalled() ? 1 : 0);

	FDevVersionRegistration::DumpVersionsToLog();
}