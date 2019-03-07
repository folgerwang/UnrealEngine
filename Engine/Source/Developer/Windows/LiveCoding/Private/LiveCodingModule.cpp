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
#include "Windows/WindowsHWrapper.h"

IMPLEMENT_MODULE(FLiveCodingModule, LiveCoding)

#define LOCTEXT_NAMESPACE "LiveCodingModule"

FString GLiveCodingConsolePath;
FString GLiveCodingConsoleArguments;

FLiveCodingModule::FLiveCodingModule()
	: bEnabled(false)
	, bShouldStart(false)
	, bStarted(false)
{
#if WITH_EDITOR
	GConfig->GetBool(TEXT("LiveCoding"), TEXT("Enabled"), bEnabled, GEditorPerProjectIni);
#endif
}

void FLiveCodingModule::StartupModule()
{
	if(FParse::Param(FCommandLine::Get(), TEXT("LiveCoding")))
	{
		bEnabled = true;
	}

	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	EnableCommand = ConsoleManager.RegisterConsoleCommand(
		TEXT("LiveCoding"),
		TEXT("Enables live coding support"),
		FConsoleCommandDelegate::CreateRaw(this, &FLiveCodingModule::Enable, true),
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
		SettingsModule->RegisterSettings("Project", "Project", "Live Coding",
			LOCTEXT("LiveCodingSettingsName", "Live Coding"),
			LOCTEXT("LiveCodintSettingsDescription", "Settings for recompiling C++ code while the engine is running."),
			GetMutableDefault<ULiveCodingSettings>()
		);
	}

	const ULiveCodingSettings* Settings = GetDefault<ULiveCodingSettings>();
	if (bEnabled && Settings->StartupMode == ELiveCodingStartupMode::Automatic)
	{
		bShouldStart = true;
		if(!Settings->bShowConsole)
		{
			GLiveCodingConsoleArguments = L"-Hidden";
		}
	}

	extern void Startup(Windows::HINSTANCE hInstance);
	Startup(hInstance);
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

void FLiveCodingModule::Enable(bool bInEnabled)
{
	if (bEnabled != bInEnabled)
	{
		bEnabled = bInEnabled;
#if WITH_EDITOR
		GConfig->SetBool(TEXT("LiveCoding"), TEXT("Enabled"), bEnabled, GEditorPerProjectIni);
#endif

		if (bEnabled)
		{
			ShowConsole();
		}
		else if(bStarted)
		{
			UE_LOG(LogLiveCoding, Display, TEXT("Console will be hidden but remain running in the background. Restart to disable completely."));
			LppSetActive(false);
			LppSetVisible(false);
		}
	}
}

bool FLiveCodingModule::IsEnabled() const
{
	return bEnabled;
}

void FLiveCodingModule::ShowConsole()
{
	if (bStarted)
	{
		LppSetVisible(bEnabled);
		LppSetActive(bEnabled);
		LppShowConsole();
	}
	else
	{
		bShouldStart = true;
	}
}

void FLiveCodingModule::TriggerRecompile()
{
	if (!bStarted)
	{
		bShouldStart = true;
		Tick();
	}
	if(bStarted)
	{
		LppTriggerRecompile();
	}
}

void FLiveCodingModule::Tick()
{
	if (bShouldStart && !bStarted)
	{
		if(StartLiveCoding())
		{
			bStarted = true;
		}
		else
		{
			bShouldStart = false;
		}
	}
}

bool FLiveCodingModule::StartLiveCoding()
{
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
	return true;
}

void FLiveCodingModule::UpdateModules()
{
#if IS_MONOLITHIC
	wchar_t FullFilePath[WINDOWS_MAX_PATH];
	verify(GetModuleFileName(hInstance, FullFilePath, ARRAY_COUNT(FullFilePath)));
	EnableModule(FullFilePath);
#else
	TArray<FModuleStatus> ModuleStatuses;
	FModuleManager::Get().QueryModules(ModuleStatuses);

	for (const FModuleStatus& ModuleStatus : ModuleStatuses)
	{
		if (ModuleStatus.bIsLoaded)
		{
			FString FullFilePath = FPaths::ConvertRelativePathToFull(ModuleStatus.FilePath);
			ConfigureModule(FName(*ModuleStatus.Name), ModuleStatus.bIsGameModule, FullFilePath);
		}
	}
#endif
}

void FLiveCodingModule::EnableModule(const FString& FullFilePath)
{
	if (!EnabledModules.Contains(FullFilePath))
	{
		LppEnableModule(*FullFilePath);
		EnabledModules.Add(FullFilePath);
	}
}

void FLiveCodingModule::DisableModule(const FString& FullFilePath)
{
	if(EnabledModules.Contains(FullFilePath))
	{
		LppDisableModule(*FullFilePath);
		EnabledModules.Remove(FullFilePath);
	}
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
			ConfigureModule(ModuleName, Status.bIsGameModule, FullFilePath);
		}
	}
#endif
}

void FLiveCodingModule::ConfigureModule(const FName& Name, bool bIsProjectModule, const FString& FullFilePath)
{
#if !IS_MONOLITHIC
	if (ShouldEnableModule(Name, bIsProjectModule, FullFilePath))
	{
		EnableModule(FullFilePath);
	}
	else
	{
		DisableModule(FullFilePath);
	}
#endif
}

bool FLiveCodingModule::ShouldEnableModule(const FName& Name, bool bIsProjectModule, const FString& FullFilePath) const
{
	const ULiveCodingSettings* Settings = GetDefault<ULiveCodingSettings>();
	if (Settings->ExcludeSpecificModules.Contains(Name))
	{
		return false;
	}
	if (Settings->IncludeSpecificModules.Contains(Name))
	{
		return true;
	}

	if (bIsProjectModule)
	{
		if (Settings->bIncludeProjectModules == Settings->bIncludeProjectPluginModules)
		{
			return Settings->bIncludeProjectModules;
		}

		FString FullProjectPluginsDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir());
		if(FullFilePath.StartsWith(FullProjectPluginsDir))
		{
			return Settings->bIncludeProjectPluginModules;
		}
		else
		{
			return Settings->bIncludeProjectModules;
		}
	}
	else
	{
		if (FApp::IsEngineInstalled())
		{
			return false;
		}

		if (Settings->bIncludeEngineModules == Settings->bIncludeEnginePluginModules)
		{
			return Settings->bIncludeEngineModules;
		}

		FString FullEnginePluginsDir = FPaths::ConvertRelativePathToFull(FPaths::EnginePluginsDir());
		if(FullFilePath.StartsWith(FullEnginePluginsDir))
		{
			return Settings->bIncludeEnginePluginModules;
		}
		else
		{
			return Settings->bIncludeEngineModules;
		}
	}
}

#undef LOCTEXT_NAMESPACE
