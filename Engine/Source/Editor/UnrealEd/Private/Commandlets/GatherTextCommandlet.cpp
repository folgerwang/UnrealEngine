// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Commandlets/GatherTextCommandlet.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "SourceControlHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogGatherTextCommandlet, Log, All);

/**
 *	UGatherTextCommandlet
 */
UGatherTextCommandlet::UGatherTextCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const FString UGatherTextCommandlet::UsageText
	(
	TEXT("GatherTextCommandlet usage...\r\n")
	TEXT("    <GameName> GatherTextCommandlet -Config=<path to config ini file>\r\n")
	TEXT("    \r\n")
	TEXT("    <path to config ini file> Full path to the .ini config file that defines what gather steps the commandlet will run.\r\n")
	);


int32 UGatherTextCommandlet::Main( const FString& Params )
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);
	
	// Build up the complete list of config files to process
	TArray<FString> GatherTextConfigPaths;
	if (const FString* ConfigParamPtr = ParamVals.Find(TEXT("config")))
	{
		ConfigParamPtr->ParseIntoArray(GatherTextConfigPaths, TEXT(";"));

		FString ProjectBasePath;
		if (!FPaths::ProjectDir().IsEmpty())
		{
			ProjectBasePath = FPaths::ProjectDir();
		}
		else
		{
			ProjectBasePath = FPaths::EngineDir();
		}

		for (FString& GatherTextConfigPath : GatherTextConfigPaths)
		{
			if (FPaths::IsRelative(GatherTextConfigPath))
			{
				GatherTextConfigPath = FPaths::Combine(*ProjectBasePath, *GatherTextConfigPath);
			}
		}
	}

	if (GatherTextConfigPaths.Num() == 0)
	{
		UE_LOG(LogGatherTextCommandlet, Error, TEXT("-config not specified.\n%s"), *UsageText);
		return -1;
	}

	const bool bEnableSourceControl = Switches.Contains(TEXT("EnableSCC"));
	const bool bDisableSubmit = Switches.Contains(TEXT("DisableSCCSubmit"));

	TSharedPtr<FLocalizationSCC> CommandletSourceControlInfo;
	if (bEnableSourceControl)
	{
		CommandletSourceControlInfo = MakeShareable(new FLocalizationSCC());

		FText SCCErrorStr;
		if (!CommandletSourceControlInfo->IsReady(SCCErrorStr))
		{
			UE_LOG(LogGatherTextCommandlet, Error, TEXT("Source Control error: %s"), *SCCErrorStr.ToString());
			return -1;
		}
	}

	for (const FString& GatherTextConfigPath : GatherTextConfigPaths)
	{
		const int32 Result = ProcessGatherConfig(GatherTextConfigPath, CommandletSourceControlInfo, Tokens, Switches, ParamVals);
		if (Result != 0)
		{
			return Result;
		}
	}

	if (CommandletSourceControlInfo.IsValid() && !bDisableSubmit)
	{
		FText SCCErrorStr;
		if (CommandletSourceControlInfo->CheckinFiles(GetChangelistDescription(GatherTextConfigPaths), SCCErrorStr))
		{
			UE_LOG(LogGatherTextCommandlet, Log, TEXT("Submitted Localization files."));
		}
		else
		{
			UE_LOG(LogGatherTextCommandlet, Error, TEXT("%s"), *SCCErrorStr.ToString());
			if (!CommandletSourceControlInfo->CleanUp(SCCErrorStr))
			{
				UE_LOG(LogGatherTextCommandlet, Error, TEXT("%s"), *SCCErrorStr.ToString());
			}
			return -1;
		}
	}

	return 0;
}

int32 UGatherTextCommandlet::ProcessGatherConfig(const FString& GatherTextConfigPath, const TSharedPtr<FLocalizationSCC>& CommandletSourceControlInfo, const TArray<FString>& Tokens, const TArray<FString>& Switches, const TMap<FString, FString>& ParamVals)
{
	GConfig->LoadFile(*GatherTextConfigPath);

	if (!GConfig->FindConfigFile(*GatherTextConfigPath))
	{
		UE_LOG(LogGatherTextCommandlet, Error, TEXT("Loading Config File \"%s\" failed."), *GatherTextConfigPath);
		return -1; 
	}

	UE_LOG(LogGatherTextCommandlet, Display, TEXT("Beginning GatherText Commandlet for '%s'"), *GatherTextConfigPath);

	// Read in the platform split mode to use
	ELocTextPlatformSplitMode PlatformSplitMode = ELocTextPlatformSplitMode::None;
	{
		FString PlatformSplitModeString;
		if (GetStringFromConfig(TEXT("CommonSettings"), TEXT("PlatformSplitMode"), PlatformSplitModeString, GatherTextConfigPath))
		{
			UEnum* PlatformSplitModeEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("ELocTextPlatformSplitMode"));
			const int64 PlatformSplitModeInt = PlatformSplitModeEnum->GetValueByName(*PlatformSplitModeString);
			if (PlatformSplitModeInt != INDEX_NONE)
			{
				PlatformSplitMode = (ELocTextPlatformSplitMode)PlatformSplitModeInt;
			}
		}
	}

	// Basic helper that can be used only to gather a new manifest for writing
	TSharedRef<FLocTextHelper> CommandletGatherManifestHelper = MakeShareable(new FLocTextHelper(MakeShareable(new FLocFileSCCNotifies(CommandletSourceControlInfo)), PlatformSplitMode));
	CommandletGatherManifestHelper->LoadManifest(ELocTextHelperLoadFlags::Create);

	const FString GatherTextStepPrefix = TEXT("GatherTextStep");

	// Read the list of steps from the config file (they all have the format GatherTextStep{N})
	TArray<FString> StepNames;
	GConfig->GetSectionNames(GatherTextConfigPath, StepNames);
	StepNames.RemoveAllSwap([&GatherTextStepPrefix](const FString& InStepName)
	{
		return !InStepName.StartsWith(GatherTextStepPrefix);
	});

	// Make sure the steps are sorted in ascending order (by numerical suffix)
	StepNames.Sort([&GatherTextStepPrefix](const FString& InStepNameOne, const FString& InStepNameTwo)
	{
		const FString NumericalSuffixOneStr = InStepNameOne.RightChop(GatherTextStepPrefix.Len());
		const int32 NumericalSuffixOne = FCString::Atoi(*NumericalSuffixOneStr);

		const FString NumericalSuffixTwoStr = InStepNameTwo.RightChop(GatherTextStepPrefix.Len());
		const int32 NumericalSuffixTwo = FCString::Atoi(*NumericalSuffixTwoStr);

		return NumericalSuffixOne < NumericalSuffixTwo;
	});

	// Execute each step defined in the config file.
	for (const FString& StepName : StepNames)
	{
		FString CommandletClassName = GConfig->GetStr( *StepName, TEXT("CommandletClass"), GatherTextConfigPath ) + TEXT("Commandlet");

		UClass* CommandletClass = FindObject<UClass>(ANY_PACKAGE,*CommandletClassName,false);
		if (!CommandletClass)
		{
			UE_LOG(LogGatherTextCommandlet, Error, TEXT("The commandlet name %s in section %s is invalid."), *CommandletClassName, *StepName);
			continue;
		}

		UGatherTextCommandletBase* Commandlet = NewObject<UGatherTextCommandletBase>(GetTransientPackage(), CommandletClass);
		check(Commandlet);
		Commandlet->AddToRoot();
		Commandlet->Initialize( CommandletGatherManifestHelper, CommandletSourceControlInfo );

		// Execute the commandlet.
		double CommandletExecutionStartTime = FPlatformTime::Seconds();

		UE_LOG(LogGatherTextCommandlet, Display, TEXT("Executing %s: %s"), *StepName, *CommandletClassName);
		
		FString GeneratedCmdLine = FString::Printf(TEXT("-Config=\"%s\" -Section=%s"), *GatherTextConfigPath , *StepName);

		// Add all the command params with the exception of config
		for(auto ParamIter = ParamVals.CreateConstIterator(); ParamIter; ++ParamIter)
		{
			const FString& Key = ParamIter.Key();
			const FString& Val = ParamIter.Value();
			if(Key != TEXT("config"))
			{
				GeneratedCmdLine += FString::Printf(TEXT(" -%s=%s"), *Key , *Val);
			}	
		}

		// Add all the command switches
		for(auto SwitchIter = Switches.CreateConstIterator(); SwitchIter; ++SwitchIter)
		{
			const FString& Switch = *SwitchIter;
			GeneratedCmdLine += FString::Printf(TEXT(" -%s"), *Switch);
		}

		if( 0 != Commandlet->Main( GeneratedCmdLine ) )
		{
			UE_LOG(LogGatherTextCommandlet, Error, TEXT("%s-%s reported an error."), *StepName, *CommandletClassName);
			if( CommandletSourceControlInfo.IsValid() )
			{
				FText SCCErrorStr;
				if( !CommandletSourceControlInfo->CleanUp( SCCErrorStr ) )
				{
					UE_LOG(LogGatherTextCommandlet, Error, TEXT("%s"), *SCCErrorStr.ToString());
				}
			}
			return -1;
		}

		UE_LOG(LogGatherTextCommandlet, Display, TEXT("Completed %s: %s in %f seconds"), *StepName, *CommandletClassName, FPlatformTime::Seconds() - CommandletExecutionStartTime);
	}

	// Clean-up any stale per-platform data
	{
		FString DestinationPath;
		if (GetPathFromConfig(TEXT("CommonSettings"), TEXT("DestinationPath"), DestinationPath, GatherTextConfigPath))
		{
			IFileManager& FileManager = IFileManager::Get();

			auto RemoveDirectory = [&FileManager](const TCHAR* InDirectory)
			{
				FileManager.IterateDirectoryRecursively(InDirectory, [&FileManager](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
				{
					if (!bIsDirectory)
					{
						if (!USourceControlHelpers::IsAvailable() || !USourceControlHelpers::MarkFileForDelete(FilenameOrDirectory))
						{
							FileManager.Delete(FilenameOrDirectory, false, true);
						}
					}
					return true;
				});
				FileManager.DeleteDirectory(InDirectory, false, true);
			};

			const FString PlatformLocalizationPath = DestinationPath / FPaths::GetPlatformLocalizationFolderName();
			if (CommandletGatherManifestHelper->ShouldSplitPlatformData())
			{
				// Remove any stale platform sub-folders
				FileManager.IterateDirectory(*PlatformLocalizationPath, [&](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
				{
					if (bIsDirectory)
					{
						const FString SplitPlatformName = FPaths::GetCleanFilename(FilenameOrDirectory);
						if (!CommandletGatherManifestHelper->GetPlatformsToSplit().Contains(SplitPlatformName))
						{
							RemoveDirectory(FilenameOrDirectory);
						}
					}
					return true;
				});
			}
			else
			{
				// Remove the entire Platforms folder
				RemoveDirectory(*PlatformLocalizationPath);
			}
		}
		else
		{
			UE_LOG(LogGatherTextCommandlet, Warning, TEXT("No destination path specified in the 'CommonSettings' section. Cannot check for stale per-platform data!"));
		}
	}

	return 0;
}

FText UGatherTextCommandlet::GetChangelistDescription(const TArray<FString>& GatherTextConfigPaths)
{
	FString ProjectName = FApp::GetProjectName();
	if (ProjectName.IsEmpty())
	{
		ProjectName = TEXT("Engine");
	}

	FString ChangeDescriptionString = FString::Printf(TEXT("[Localization Update] %s\n\n"), *ProjectName);

	ChangeDescriptionString += TEXT("Targets:\n");
	for (const FString& GatherTextConfigPath : GatherTextConfigPaths)
	{
		const FString TargetName = FPaths::GetBaseFilename(GatherTextConfigPath, true);
		ChangeDescriptionString += FString::Printf(TEXT("  %s\n"), *TargetName);
	}

	return FText::FromString(MoveTemp(ChangeDescriptionString));
}
