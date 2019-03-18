// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LiveCodingModule.h"
#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "LiveCodingLog.h"
#include "External/LC_EntryPoint.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "LiveCodingSettings.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Windows/WindowsHWrapper.h"

IMPLEMENT_MODULE(FLiveCodingModule, LiveCoding)

#define LOCTEXT_NAMESPACE "LiveCodingModule"

bool GIsCompileActive = false;
FString GLiveCodingConsolePath;
FString GLiveCodingConsoleArguments;

FLiveCodingModule::FLiveCodingModule()
	: bEnabledLastTick(false)
	, bEnabledForSession(false)
	, bStarted(false)
	, FullEnginePluginsDir(FPaths::ConvertRelativePathToFull(FPaths::EnginePluginsDir()))
	, FullProjectDir(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()))
	, FullProjectPluginsDir(FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir()))
{
}

void FLiveCodingModule::StartupModule()
{
	Settings = GetMutableDefault<ULiveCodingSettings>();

	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	EnableCommand = ConsoleManager.RegisterConsoleCommand(
		TEXT("LiveCoding"),
		TEXT("Enables live coding support"),
		FConsoleCommandDelegate::CreateRaw(this, &FLiveCodingModule::EnableForSession, true),
		ECVF_Cheat
	);

	ConsolePathVariable = ConsoleManager.RegisterConsoleVariable(
		TEXT("LiveCoding.ConsolePath"),
		FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/Win64/LiveCodingConsole.exe")),
		TEXT("Path to the live coding console application"),
		ECVF_Cheat
	);

	EndFrameDelegateHandle = FCoreDelegates::OnEndFrame.AddRaw(this, &FLiveCodingModule::Tick);

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule != nullptr)
	{
		SettingsSection = SettingsModule->RegisterSettings("Editor", "General", "Live Coding",
			LOCTEXT("LiveCodingSettingsName", "Live Coding"),
			LOCTEXT("LiveCodintSettingsDescription", "Settings for recompiling C++ code while the engine is running."),
			GetMutableDefault<ULiveCodingSettings>()
		);
	}

	extern void Startup(Windows::HINSTANCE hInstance);
	Startup(hInstance);

	if (Settings->bEnabled)
	{
		if(Settings->Startup == ELiveCodingStartupMode::Automatic)
		{
			StartLiveCoding();
			ShowConsole();
		}
		else if(Settings->Startup == ELiveCodingStartupMode::AutomaticButHidden)
		{
			GLiveCodingConsoleArguments = L"-Hidden";
			StartLiveCoding();
		}
	}

	if(FParse::Param(FCommandLine::Get(), TEXT("LiveCoding")))
	{
		StartLiveCoding();
	}

	bEnabledLastTick = Settings->bEnabled;
}

void FLiveCodingModule::ShutdownModule()
{
	extern void Shutdown();
	Shutdown();

	FCoreDelegates::OnEndFrame.Remove(EndFrameDelegateHandle);

	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	ConsoleManager.UnregisterConsoleObject(ConsolePathVariable);
	ConsoleManager.UnregisterConsoleObject(EnableCommand);
}

void FLiveCodingModule::EnableByDefault(bool bEnable)
{
	if(Settings->bEnabled != bEnable)
	{
		Settings->bEnabled = bEnable;
		if(SettingsSection.IsValid())
		{
			SettingsSection->Save();
		}
	}
	EnableForSession(bEnable);
}

bool FLiveCodingModule::IsEnabledByDefault() const
{
	return Settings->bEnabled;
}

void FLiveCodingModule::EnableForSession(bool bEnable)
{
	if (bEnable)
	{
		if(!bStarted)
		{
			StartLiveCoding();
			ShowConsole();
		}
	}
	else 
	{
		if(bStarted)
		{
			UE_LOG(LogLiveCoding, Display, TEXT("Console will be hidden but remain running in the background. Restart to disable completely."));
			LppSetActive(false);
			LppSetVisible(false);
			bEnabledForSession = false;
		}
	}
}

bool FLiveCodingModule::IsEnabledForSession() const
{
	return bEnabledForSession;
}

bool FLiveCodingModule::CanEnableForSession() const
{
#if !IS_MONOLITHIC
	FModuleManager& ModuleManager = FModuleManager::Get();
	if(ModuleManager.HasAnyOverridenModuleFilename())
	{
		return false;
	}
#endif
	return true;
}

bool FLiveCodingModule::HasStarted() const
{
	return bStarted;
}

void FLiveCodingModule::ShowConsole()
{
	if (bStarted)
	{
		LppSetVisible(true);
		LppSetActive(true);
		LppShowConsole();
	}
}

void FLiveCodingModule::Compile()
{
	if(!GIsCompileActive)
	{
		EnableForSession(true);
		if(bStarted)
		{
			LppTriggerRecompile();
			GIsCompileActive = true;
		}
	}
}

bool FLiveCodingModule::IsCompiling() const
{
	return GIsCompileActive;
}

void FLiveCodingModule::Tick()
{
	if (Settings->bEnabled != bEnabledLastTick && Settings->Startup != ELiveCodingStartupMode::Manual)
	{
		EnableForSession(Settings->bEnabled);
		bEnabledLastTick = Settings->bEnabled;
	}
}

bool FLiveCodingModule::StartLiveCoding()
{
	if(!bStarted)
	{
		// Make sure there aren't any hot reload modules already active
		if (!CanEnableForSession())
		{
			UE_LOG(LogLiveCoding, Error, TEXT("Unable to start live coding session. Some modules have already been hot reloaded."));
			return false;
		}

		// Setup the console path
		GLiveCodingConsolePath = ConsolePathVariable->GetString();
		if (!FPaths::FileExists(GLiveCodingConsolePath))
		{
			UE_LOG(LogLiveCoding, Error, TEXT("Unable to start live coding session. Missing executable '%s'. Use the LiveCoding.ConsolePath console variable to modify."), *GLiveCodingConsolePath);
			return false;
		}

		UE_LOG(LogLiveCoding, Display, TEXT("Starting LiveCoding"));

		// Enable external build system
		LppUseExternalBuildSystem();

		// Enable the server
		FString ProcessGroup = FString::Printf(TEXT("UE4_%s_0x%08x"), FApp::GetProjectName(), GetTypeHash(FPaths::ProjectDir()));
		LppRegisterProcessGroup(TCHAR_TO_ANSI(*ProcessGroup));

		// Build the command line
		FString Arguments;
		Arguments += FString::Printf(TEXT("%s"), FPlatformMisc::GetUBTPlatform());
		Arguments += FString::Printf(TEXT(" %s"), EBuildConfigurations::ToString(FApp::GetBuildConfiguration()));
		Arguments += FString::Printf(TEXT(" -TargetType=%s"), FPlatformMisc::GetUBTTarget());
		if(FPaths::IsProjectFilePathSet())
		{
			Arguments += FString::Printf(TEXT(" -Project=\"%s\""), *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()));
		}
		LppSetBuildArguments(*Arguments);

		// Configure all the current modules
		UpdateModules();

		// Register a delegate to listen for new modules loaded from this point onwards
		ModulesChangedDelegateHandle = FModuleManager::Get().OnModulesChanged().AddRaw(this, &FLiveCodingModule::OnModulesChanged);

		// Mark it as started
		bStarted = true;
		bEnabledForSession = true;
	}
	return true;
}

void FLiveCodingModule::UpdateModules()
{
#if IS_MONOLITHIC
	wchar_t FullFilePath[WINDOWS_MAX_PATH];
	verify(GetModuleFileName(hInstance, FullFilePath, ARRAY_COUNT(FullFilePath)));
	LppEnableModule(FullFilePath);
#else
	TArray<FModuleStatus> ModuleStatuses;
	FModuleManager::Get().QueryModules(ModuleStatuses);

	extern void BeginCommandBatch();
	BeginCommandBatch();

	for (const FModuleStatus& ModuleStatus : ModuleStatuses)
	{
		if (ModuleStatus.bIsLoaded)
		{
			FString FullFilePath = FPaths::ConvertRelativePathToFull(ModuleStatus.FilePath);
			ConfigureModule(FName(*ModuleStatus.Name), FullFilePath);
		}
	}

	extern void EndCommandBatch();
	EndCommandBatch();
#endif
}

void FLiveCodingModule::OnModulesChanged(FName ModuleName, EModuleChangeReason Reason)
{
#if !IS_MONOLITHIC
	if (Reason == EModuleChangeReason::ModuleLoaded)
	{
		FModuleStatus Status;
		if (FModuleManager::Get().QueryModule(ModuleName, Status))
		{
			FString FullFilePath = FPaths::ConvertRelativePathToFull(Status.FilePath);
			ConfigureModule(ModuleName, FullFilePath);
		}
	}
#endif
}

void FLiveCodingModule::ConfigureModule(const FName& Name, const FString& FullFilePath)
{
#if !IS_MONOLITHIC
	if (!ConfiguredModules.Contains(Name))
	{
		if (ShouldPreloadModule(Name, FullFilePath))
		{
			LppEnableModule(*FullFilePath);
		}
		else
		{
			LppEnableLazyLoadedModule(*FullFilePath);
		}
		ConfiguredModules.Add(Name);
	}
#endif
}

bool FLiveCodingModule::ShouldPreloadModule(const FName& Name, const FString& FullFilePath) const
{
	if (Settings->PreloadNamedModules.Contains(Name))
	{
		return true;
	}

	if (FullFilePath.StartsWith(FullProjectDir))
	{
		if (Settings->bPreloadProjectModules == Settings->bPreloadProjectPluginModules)
		{
			return Settings->bPreloadProjectModules;
		}

		if(FullFilePath.StartsWith(FullProjectPluginsDir))
		{
			return Settings->bPreloadProjectPluginModules;
		}
		else
		{
			return Settings->bPreloadProjectModules;
		}
	}
	else
	{
		if (FApp::IsEngineInstalled())
		{
			return false;
		}

		if (Settings->bPreloadEngineModules == Settings->bPreloadEnginePluginModules)
		{
			return Settings->bPreloadEngineModules;
		}

		if(FullFilePath.StartsWith(FullEnginePluginsDir))
		{
			return Settings->bPreloadEnginePluginModules;
		}
		else
		{
			return Settings->bPreloadEngineModules;
		}
	}
}

#undef LOCTEXT_NAMESPACE
