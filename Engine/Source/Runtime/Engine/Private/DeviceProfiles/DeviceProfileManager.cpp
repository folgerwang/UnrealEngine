// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DeviceProfiles/DeviceProfileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "Misc/CommandLine.h"
#include "UObject/Package.h"
#include "SceneManagement.h"
#include "SystemSettings.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "IDeviceProfileSelectorModule.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"
#if WITH_EDITOR
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "PlatformInfo.h"
#include "PIEPreviewDeviceProfileSelectorModule.h"
#endif
#include "ProfilingDebugging/CsvProfiler.h"

static TAutoConsoleVariable<FString> CVarDeviceProfileOverride(
	TEXT("dp.Override"),
	TEXT(""),
	TEXT("DeviceProfile override - setting this will use the named DP as the active DP. In addition, it will restore any\n")
	TEXT(" previous overrides before setting (does a dp.OverridePop before setting after the first time).\n")
	TEXT(" The commandline -dp option will override this on startup, but not when setting this at runtime\n"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAllowScalabilityGroupsToChangeAtRuntime(
	TEXT("dp.AllowScalabilityGroupsToChangeAtRuntime"),
	0,
	TEXT("If true, device profile scalability bucket cvars will be set with scalability")
	TEXT("priority which allows them to be changed at runtime. Off by default."),
	ECVF_Default);

FString UDeviceProfileManager::DeviceProfileFileName;
TMap<FString, FString> UDeviceProfileManager::DeviceProfileScalabilityCVars;

UDeviceProfileManager* UDeviceProfileManager::DeviceProfileManagerSingleton = nullptr;

UDeviceProfileManager& UDeviceProfileManager::Get(bool bFromPostCDOContruct)
{
	if (DeviceProfileManagerSingleton == nullptr)
	{
		static bool bEntered = false;
		if (bEntered && bFromPostCDOContruct)
		{
			return *(UDeviceProfileManager*)0x3; // we know that the return value is never used, linux hates null here, which would be less weird. 
		}
		bEntered = true;
		DeviceProfileManagerSingleton = NewObject<UDeviceProfileManager>();

		DeviceProfileManagerSingleton->AddToRoot();
		if (!FPlatformProperties::RequiresCookedData())
		{
			DeviceProfileManagerSingleton->LoadProfiles();
		}

		// always start with an active profile, even if we create it on the spot
		UDeviceProfile* ActiveProfile = DeviceProfileManagerSingleton->FindProfile(GetActiveProfileName());
		DeviceProfileManagerSingleton->SetActiveDeviceProfile(ActiveProfile);

		// now we allow the cvar changes to be acknowledged
		CVarDeviceProfileOverride.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Variable)
		{
			UDeviceProfileManager::Get().HandleDeviceProfileOverrideChange();
		}));

		IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("dp.Override.Restore"),
			TEXT("Restores any cvars set by dp.Override to their previous value"),
			FConsoleCommandDelegate::CreateLambda([]()
			{
				UDeviceProfileManager::Get().HandleDeviceProfileOverridePop();
			}),
			ECVF_Default
		);

		InitializeSharedSamplerStates();
	}
	return *DeviceProfileManagerSingleton;
}


void UDeviceProfileManager::InitializeCVarsForActiveDeviceProfile(bool bPushSettings)
{
	FString ActiveProfileName = DeviceProfileManagerSingleton ? DeviceProfileManagerSingleton->ActiveDeviceProfile->GetName() : GetActiveProfileName();

	UE_LOG(LogInit, Log, TEXT("Applying CVar settings loaded from the selected device profile: [%s]"), *ActiveProfileName);

	// Load the device profile config
	FConfigCacheIni::LoadGlobalIniFile(DeviceProfileFileName, TEXT("DeviceProfiles"));

	TArray< FString > AvailableProfiles;
	GConfig->GetSectionNames( DeviceProfileFileName, AvailableProfiles );

	// Look up the ini for this tree as we are far too early to use the UObject system
	AvailableProfiles.Remove( TEXT( "DeviceProfiles" ) );

	// Next we need to create a hierarchy of CVars from the Selected Device Profile, to it's eldest parent
	TMap<FString, FString> CVarsAlreadySetList;
	DeviceProfileScalabilityCVars.Empty();
	
	// even if we aren't pushing new values, we should clear any old pushed values, as they are no longer valid after we run this loop
	if (DeviceProfileManagerSingleton)
	{
		DeviceProfileManagerSingleton->PushedSettings.Empty();
	}

#if !UE_BUILD_SHIPPING
#if PLATFORM_ANDROID
	// allow ConfigRules to override cvars first
	TMap<FString, FString> ConfigRules = FAndroidMisc::GetConfigRulesTMap();
	for (const TPair<FString, FString>& Pair : ConfigRules)
	{
		FString Key = Pair.Key;
		if (Key.StartsWith("cvar_"))
		{
			FString CVarKey = Key.RightChop(5);
			FString CVarValue = Pair.Value;

			UE_LOG(LogInit, Log, TEXT("Setting ConfigRules Device Profile CVar: [[%s:%s]]"), *CVarKey, *CVarValue);

			// set it and remember it
			OnSetCVarFromIniEntry(*DeviceProfileFileName, *CVarKey, *CVarValue, ECVF_SetByDeviceProfile);
			CVarsAlreadySetList.Add(CVarKey, CVarValue);
		}
	}
#endif
#endif

#if !UE_BUILD_SHIPPING
	// pre-apply any -dpcvars= items, so that they override anything in the DPs
	FString DPCVarString;
	if (FParse::Value(FCommandLine::Get(), TEXT("DPCVars="), DPCVarString, false) || FParse::Value(FCommandLine::Get(), TEXT("DPCVar="), DPCVarString, false))
	{
		// look over a list of cvars
		TArray<FString> DPCVars;
		DPCVarString.ParseIntoArray(DPCVars, TEXT(","), true);
		for (FString& DPCVar : DPCVars)
		{
			// split up each Key=Value pair
			FString CVarKey, CVarValue;
			if (DPCVar.Split(TEXT("="), &CVarKey, &CVarValue))
			{
				UE_LOG(LogInit, Log, TEXT("Setting CommandLine Device Profile CVar: [[%s:%s]]"), *CVarKey, *CVarValue);

				// set it and remember it (no thanks, Ron Popeil)
				OnSetCVarFromIniEntry(*DeviceProfileFileName, *CVarKey, *CVarValue, ECVF_SetByDeviceProfile);
				CVarsAlreadySetList.Add(CVarKey, CVarValue);
			}
		}
	}
#endif
	
	// Preload a cvar we rely on
	if (GConfig)
	{
		if (FConfigSection* Section = GConfig->GetSectionPrivate(TEXT("ConsoleVariables"), false, true, *GEngineIni))
		{
			static FName AllowScalabilityAtRuntimeName = TEXT("dp.AllowScalabilityGroupsToChangeAtRuntime");
			if (const FConfigValue* Value = Section->Find(AllowScalabilityAtRuntimeName))
			{
				const FString& KeyString = AllowScalabilityAtRuntimeName.ToString();
				const FString& ValueString = Value->GetValue();
				OnSetCVarFromIniEntry(*GEngineIni, *KeyString, *ValueString, ECVF_SetBySystemSettingsIni);
			}
		}
	}

	// For each device profile, starting with the selected and working our way up the BaseProfileName tree,
	// Find all CVars and set them 
	FString BaseDeviceProfileName = ActiveProfileName;
	bool bReachedEndOfTree = BaseDeviceProfileName.IsEmpty();
	while( bReachedEndOfTree == false ) 
	{
		FString CurrentSectionName = FString::Printf( TEXT("%s %s"), *BaseDeviceProfileName, *UDeviceProfile::StaticClass()->GetName() );
		
		// Check the profile was available.
		bool bProfileExists = AvailableProfiles.Contains( CurrentSectionName );
		if( bProfileExists )
		{
			// put this up in some shared code somewhere in FGenericPlatformMemory
			const TCHAR* BucketNames[] = {
				TEXT("_Largest"),
				TEXT("_Larger"),
				TEXT("_Default"),
				TEXT("_Smaller"),
				TEXT("_Smallest"),
                TEXT("_Tiniest"),
			};

			for (int Pass = 0; Pass < 2; Pass++)
			{
				// apply the current memory bucket CVars in Pass 0, regular CVars in pass 1 (anything set in Pass 0 won't be set in pass 1)
				FString ArrayName = TEXT("CVars");
				if (Pass == 0)
				{
					ArrayName += BucketNames[(int32)FPlatformMemory::GetMemorySizeBucket()];
				}

				TArray< FString > CurrentProfilesCVars;
				GConfig->GetArray(*CurrentSectionName, *ArrayName, CurrentProfilesCVars, DeviceProfileFileName);

				// Iterate over the profile and make sure we do not have duplicate CVars
				{
					TMap< FString, FString > ValidCVars;
					for (TArray< FString >::TConstIterator CVarIt(CurrentProfilesCVars); CVarIt; ++CVarIt)
					{
						FString CVarKey, CVarValue;
						if ((*CVarIt).Split(TEXT("="), &CVarKey, &CVarValue))
						{
							if (ValidCVars.Find(CVarKey))
							{
								ValidCVars.Remove(CVarKey);
							}

							ValidCVars.Add(CVarKey, CVarValue);
						}
					}

					// Empty the current list, and replace with the processed CVars. This removes duplicates
					CurrentProfilesCVars.Empty();

					for (TMap< FString, FString >::TConstIterator ProcessedCVarIt(ValidCVars); ProcessedCVarIt; ++ProcessedCVarIt)
					{
						CurrentProfilesCVars.Add(FString::Printf(TEXT("%s=%s"), *ProcessedCVarIt.Key(), *ProcessedCVarIt.Value()));
					}

				}

				// Iterate over this profiles cvars and set them if they haven't been already.
				for (TArray< FString >::TConstIterator CVarIt(CurrentProfilesCVars); CVarIt; ++CVarIt)
				{
					FString CVarKey, CVarValue;
					if ((*CVarIt).Split(TEXT("="), &CVarKey, &CVarValue))
					{
						if (!CVarsAlreadySetList.Find(CVarKey))
						{
							IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarKey);
							if (CVar)
							{
								if (DeviceProfileManagerSingleton && bPushSettings)
								{
									// remember the previous value
									FString OldValue = CVar->GetString();
									DeviceProfileManagerSingleton->PushedSettings.Add(CVarKey, OldValue);

									// indicate we are pushing, not setting
									UE_LOG(LogInit, Log, TEXT("Pushing Device Profile CVar: [[%s:%s -> %s]]"), *CVarKey, *OldValue, *CVarValue);
								}
							}
							else
							{
								UE_LOG(LogInit, Warning, TEXT("Creating unregistered Device Profile CVar: [[%s:%s]]"), *CVarKey, *CVarValue);
							}

							// General scalability bucket cvars are set as a suggested default but can be overridden by game settings.
							bool bIsScalabilityBucket = false;
							if (CVarAllowScalabilityGroupsToChangeAtRuntime.GetValueOnGameThread() > 0)
							{
								// Cache any scalability related cvars so we can conveniently reapply them later as a way to reset the device defaults
								if (CVarKey.StartsWith(TEXT("sg.")))
								{
									DeviceProfileScalabilityCVars.Add(*CVarKey, *CVarValue);
									bIsScalabilityBucket = true;
								}
							}

							uint32 CVarPriority = bIsScalabilityBucket ? ECVF_SetByScalability : ECVF_SetByDeviceProfile;
							OnSetCVarFromIniEntry(*DeviceProfileFileName, *CVarKey, *CVarValue, CVarPriority);
							CVarsAlreadySetList.Add(CVarKey, CVarValue);
						}
					}
				}
			}

			// Get the next device profile name, to look for CVars in, along the tree
			FString NextBaseDeviceProfileName;
			if( GConfig->GetString( *CurrentSectionName, TEXT("BaseProfileName"), NextBaseDeviceProfileName, DeviceProfileFileName ) )
			{
				BaseDeviceProfileName = NextBaseDeviceProfileName;
				UE_LOG(LogInit, Log, TEXT("Going up to parent DeviceProfile [%s]"), *BaseDeviceProfileName);
			}
			else
			{
				BaseDeviceProfileName.Empty();
			}
		}
		
		// Check if we have inevitably reached the end of the device profile tree.
		bReachedEndOfTree = !bProfileExists || BaseDeviceProfileName.IsEmpty();
	}
}

static void TestProfileForCircularReferences(const FString& ProfileName, const FString& ParentName, const FConfigFile &PlatformConfigFile)
{
	TArray<FString> ProfileDependancies;
	ProfileDependancies.Add(ProfileName);
	FString CurrentParent = ParentName;
	while (!CurrentParent.IsEmpty())
	{
		if (ProfileDependancies.FindByPredicate([CurrentParent](const FString& InName) { return InName.Equals(CurrentParent); }))
		{
			UE_LOG(LogInit, Fatal, TEXT("Device Profile %s has a circular dependency on %s"), *ProfileName, *CurrentParent);
		}
		else
		{
			ProfileDependancies.Add(CurrentParent);
			const FString SectionName = FString::Printf(TEXT("%s %s"), *CurrentParent, *UDeviceProfile::StaticClass()->GetName());
			CurrentParent.Reset();
			PlatformConfigFile.GetString(*SectionName, TEXT("BaseProfileName"), CurrentParent);
		}
	}
}

UDeviceProfile* UDeviceProfileManager::CreateProfile(const FString& ProfileName, const FString& ProfileType, const FString& InSpecifyParentName, const TCHAR* ConfigPlatform)
{
	UDeviceProfile* DeviceProfile = FindObject<UDeviceProfile>( GetTransientPackage(), *ProfileName );
	if (DeviceProfile == NULL)
	{
		// use ConfigPlatform ini hierarchy to look in for the parent profile
		// @todo config: we could likely cache local ini files to speed this up,
		// along with the ones we load in LoadConfig
		// NOTE: This happens at runtime, so maybe only do this if !RequiresCookedData()?
		FConfigFile PlatformConfigFile;
		FConfigCacheIni::LoadLocalIniFile(PlatformConfigFile, TEXT("DeviceProfiles"), true, ConfigPlatform);

		// Build Parent objects first. Important for setup
		FString ParentName = InSpecifyParentName;
		if (ParentName.Len() == 0)
		{
			const FString SectionName = FString::Printf(TEXT("%s %s"), *ProfileName, *UDeviceProfile::StaticClass()->GetName());
			PlatformConfigFile.GetString(*SectionName, TEXT("BaseProfileName"), ParentName);
		}

		UObject* ParentObject = nullptr;
		// Recursively build the parent tree
		if (ParentName.Len() > 0 && ParentName != ProfileName)
		{
			ParentObject = FindObject<UDeviceProfile>(GetTransientPackage(), *ParentName);
			if (ParentObject == nullptr)
			{
				TestProfileForCircularReferences(ProfileName, ParentName, PlatformConfigFile);
				ParentObject = CreateProfile(ParentName, ProfileType, TEXT(""), ConfigPlatform);
			}
		}

		// Create the profile after it's parents have been created.
		DeviceProfile = NewObject<UDeviceProfile>(GetTransientPackage(), *ProfileName);
		if (ConfigPlatform != nullptr)
		{
			// if the config needs to come from a platform, set it now, then reload the config
			DeviceProfile->ConfigPlatform = ConfigPlatform;
			DeviceProfile->LoadConfig();
			DeviceProfile->ValidateProfile();
		}

		// if the config didn't specify a DeviceType, use the passed in one
		if (DeviceProfile->DeviceType.IsEmpty())
		{
			DeviceProfile->DeviceType = ProfileType;
		}

		// final fixups
		DeviceProfile->BaseProfileName = DeviceProfile->BaseProfileName.Len() > 0 ? DeviceProfile->BaseProfileName : ParentName;
		DeviceProfile->Parent = ParentObject;
		// the DP manager can be marked as Disregard for GC, so what it points to needs to be in the Root set
		DeviceProfile->AddToRoot();

		// Add the new profile to the accessible device profile list
		Profiles.Add( DeviceProfile );

		// Inform any listeners that the device list has changed
		ManagerUpdatedDelegate.Broadcast(); 
	}

	return DeviceProfile;
}


void UDeviceProfileManager::DeleteProfile( UDeviceProfile* Profile )
{
	Profiles.Remove( Profile );
}


UDeviceProfile* UDeviceProfileManager::FindProfile( const FString& ProfileName, bool bCreateProfileOnFail )
{
	UDeviceProfile* FoundProfile = nullptr;

	for( int32 Idx = 0; Idx < Profiles.Num(); Idx++ )
	{
		UDeviceProfile* CurrentDevice = CastChecked<UDeviceProfile>( Profiles[Idx] );
		if( CurrentDevice->GetName() == ProfileName )
		{
			FoundProfile = CurrentDevice;
			break;
		}
	}

	if ( bCreateProfileOnFail && FoundProfile == nullptr )
	{
		FoundProfile = CreateProfile(ProfileName, FPlatformProperties::PlatformName());
	}
	return FoundProfile;
}

const FString UDeviceProfileManager::GetDeviceProfileIniName() const
{
	return DeviceProfileFileName;
}


FOnDeviceProfileManagerUpdated& UDeviceProfileManager::OnManagerUpdated()
{
	return ManagerUpdatedDelegate;
}


void UDeviceProfileManager::LoadProfiles()
{
	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		TMap<FString, FString> DeviceProfileToPlatformConfigMap;
		TArray<FString> ConfidentialPlatforms = FDataDrivenPlatformInfoRegistry::GetConfidentialPlatforms();
		
		checkf(ConfidentialPlatforms.Contains(FString(FPlatformProperties::IniPlatformName())) == false,
			TEXT("UDeviceProfileManager::LoadProfiles is called from a confidential platform (%s). Confidential platforms are not expected to be editor/non-cooked builds."), 
			ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName()));

		// go over all the platforms we find, starting with the current platform
		for (int32 PlatformIndex = 0; PlatformIndex <= ConfidentialPlatforms.Num(); PlatformIndex++)
		{
			// which platform's set of ini files should we load from?
			FString ConfigLoadPlatform = PlatformIndex == 0 ? FString(FPlatformProperties::IniPlatformName()) : ConfidentialPlatforms[PlatformIndex - 1];

			// load the DP.ini files (from current platform and then by the extra confidential platforms)
			FConfigFile PlatformConfigFile;
			FConfigCacheIni::LoadLocalIniFile(PlatformConfigFile, TEXT("DeviceProfiles"), true, *ConfigLoadPlatform);

			// load all of the DeviceProfiles
			TArray<FString> ProfileDescriptions;
			PlatformConfigFile.GetArray(TEXT("DeviceProfiles"), TEXT("DeviceProfileNameAndTypes"), ProfileDescriptions);


			// add them to our collection of profiles by platform
			for (const FString& Desc : ProfileDescriptions)
			{
				if (!DeviceProfileToPlatformConfigMap.Contains(Desc))
				{
					DeviceProfileToPlatformConfigMap.Add(Desc, ConfigLoadPlatform);
				}
			}
		}

		// now that we have gathered all the unique DPs, load them from the proper platform hierarchy
		for (auto It = DeviceProfileToPlatformConfigMap.CreateIterator(); It; ++It)
		{
			// the value of the map is in the format Name,DeviceType (DeviceType is usually platform)
			FString Name, DeviceType;
			It.Key().Split(TEXT(","), &Name, &DeviceType);

			if (FindObject<UDeviceProfile>(GetTransientPackage(), *Name) == NULL)
			{
				// set the config platform if it's not the current platform
				if (It.Value() != FPlatformProperties::IniPlatformName())
				{
					CreateProfile(Name, DeviceType, TEXT(""), *It.Value());
				}
				else
				{
					CreateProfile(Name, DeviceType);
				}
			}
		}

#if WITH_EDITOR
		if (!FPlatformProperties::RequiresCookedData())
		{
			// Register Texture LOD settings with each Target Platform
			ITargetPlatformManagerModule& TargetPlatformManager = GetTargetPlatformManagerRef();
			const TArray<ITargetPlatform*>& TargetPlatforms = TargetPlatformManager.GetTargetPlatforms();
			for (int32 PlatformIndex = 0; PlatformIndex < TargetPlatforms.Num(); ++PlatformIndex)
			{
				ITargetPlatform* Platform = TargetPlatforms[PlatformIndex];

				// Set TextureLODSettings
				const UTextureLODSettings* TextureLODSettingsObj = FindProfile(*Platform->GetPlatformInfo().VanillaPlatformName.ToString());
				Platform->RegisterTextureLODSettings(TextureLODSettingsObj);
			}
		}
#endif

		ManagerUpdatedDelegate.Broadcast();
	}
}


void UDeviceProfileManager::SaveProfiles(bool bSaveToDefaults)
{
	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		if(bSaveToDefaults)
		{
			for (int32 DeviceProfileIndex = 0; DeviceProfileIndex < Profiles.Num(); ++DeviceProfileIndex)
			{
				UDeviceProfile* CurrentProfile = CastChecked<UDeviceProfile>(Profiles[DeviceProfileIndex]);
				CurrentProfile->UpdateDefaultConfigFile();
			}
		}
		else
		{
			for (int32 DeviceProfileIndex = 0; DeviceProfileIndex < Profiles.Num(); ++DeviceProfileIndex)
			{
				UDeviceProfile* CurrentProfile = CastChecked<UDeviceProfile>(Profiles[DeviceProfileIndex]);
				FString DeviceProfileTypeNameCombo = FString::Printf(TEXT("%s,%s"), *CurrentProfile->GetName(), *CurrentProfile->DeviceType);

				CurrentProfile->SaveConfig(CPF_Config, *DeviceProfileFileName);
			}
		}

		ManagerUpdatedDelegate.Broadcast();
	}
}

/**
* Overrides the device profile. The original profile can be restored with RestoreDefaultDeviceProfile
*/
void UDeviceProfileManager::SetOverrideDeviceProfile(UDeviceProfile* DeviceProfile)
{
	// pop any pushed settings
	HandleDeviceProfileOverridePop();

	// activate new one!
	DeviceProfileManagerSingleton->SetActiveDeviceProfile(DeviceProfile);
	InitializeCVarsForActiveDeviceProfile(true);

	// broadcast cvar sinks now that we are done
	IConsoleManager::Get().CallAllConsoleVariableSinks();
}

/**
* Restore the device profile to the default for this device
*/
void UDeviceProfileManager::RestoreDefaultDeviceProfile()
{
	// restore pushed settings
	for (TMap<FString, FString>::TIterator It(PushedSettings); It; ++It)
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*It.Key());
		if (CVar)
		{
			// restore it!
			CVar->Set(*It.Value(), ECVF_SetByDeviceProfile);
			UE_LOG(LogInit, Log, TEXT("Popping Device Profile CVar: [[%s:%s]]"), *It.Key(), *It.Value());
		}
	}
}



void UDeviceProfileManager::HandleDeviceProfileOverrideChange()
{
	FString CVarValue = CVarDeviceProfileOverride.GetValueOnGameThread();
	// only handle when the value is different
	if (CVarValue.Len() > 0 && CVarValue != GetActiveProfile()->GetName())
	{
		// find the profile (note that if the name is bad, this will create one with that name)
		UDeviceProfile* NewActiveProfile = FindProfile(CVarValue);
		SetOverrideDeviceProfile(NewActiveProfile);
	}
}

void UDeviceProfileManager::HandleDeviceProfileOverridePop()
{
	RestoreDefaultDeviceProfile();
}

const FString UDeviceProfileManager::GetActiveProfileName()
{
	FString ActiveProfileName = FPlatformProperties::PlatformName();

	// look for a commandline override (never even calls into the selector plugin)
	FString OverrideProfileName;
	if (FParse::Value(FCommandLine::Get(), TEXT("DeviceProfile="), OverrideProfileName) || FParse::Value(FCommandLine::Get(), TEXT("DP="), OverrideProfileName))
	{
		return OverrideProfileName;
	}

	// look for cvar override
	OverrideProfileName = CVarDeviceProfileOverride.GetValueOnGameThread();
	if (OverrideProfileName.Len() > 0)
	{
		return OverrideProfileName;
	}


	FString DeviceProfileSelectionModule;
	if (GConfig->GetString(TEXT("DeviceProfileManager"), TEXT("DeviceProfileSelectionModule"), DeviceProfileSelectionModule, GEngineIni))
	{
		if (IDeviceProfileSelectorModule* DPSelectorModule = FModuleManager::LoadModulePtr<IDeviceProfileSelectorModule>(*DeviceProfileSelectionModule))
		{
			ActiveProfileName = DPSelectorModule->GetRuntimeDeviceProfileName();
		}
	}

#if WITH_EDITOR
	if (FPIEPreviewDeviceModule::IsRequestingPreviewDevice())
	{
		IDeviceProfileSelectorModule* PIEPreviewDeviceProfileSelectorModule = FModuleManager::LoadModulePtr<IDeviceProfileSelectorModule>("PIEPreviewDeviceProfileSelector");
		if (PIEPreviewDeviceProfileSelectorModule)
		{
			FString PIEProfileName = PIEPreviewDeviceProfileSelectorModule->GetRuntimeDeviceProfileName();
			if (!PIEProfileName.IsEmpty())
			{
				ActiveProfileName = PIEProfileName;
			}
		}
	}
#endif
	return ActiveProfileName;
}

bool UDeviceProfileManager::GetScalabilityCVar(const FString& CVarName, int32& OutValue)
{
	if (const FString* CVarValue = DeviceProfileScalabilityCVars.Find(CVarName))
	{
		TTypeFromString<int32>::FromString(OutValue, **CVarValue);
		return true;
	}

	return false;
}

bool UDeviceProfileManager::GetScalabilityCVar(const FString& CVarName, float& OutValue)
{
	if (const FString* CVarValue = DeviceProfileScalabilityCVars.Find(CVarName))
	{
		TTypeFromString<float>::FromString(OutValue, **CVarValue);
		return true;
	}

	return false;
}

void UDeviceProfileManager::SetActiveDeviceProfile( UDeviceProfile* DeviceProfile )
{
	ActiveDeviceProfile = DeviceProfile;

#if CSV_PROFILER
	FCsvProfiler::Get()->SetDeviceProfileName(ActiveDeviceProfile->GetName());
#endif
}


UDeviceProfile* UDeviceProfileManager::GetActiveProfile() const
{
	return ActiveDeviceProfile;
}


void UDeviceProfileManager::GetAllPossibleParentProfiles(const UDeviceProfile* ChildProfile, OUT TArray<UDeviceProfile*>& PossibleParentProfiles) const
{
	for(auto& NextProfile : Profiles)
	{
		UDeviceProfile* ParentProfile = CastChecked<UDeviceProfile>(NextProfile);
		if (ParentProfile->DeviceType == ChildProfile->DeviceType && ParentProfile != ChildProfile)
		{
			bool bIsValidPossibleParent = true;

			UDeviceProfile* CurrentAncestor = ParentProfile;
			do
			{
				if(CurrentAncestor->BaseProfileName == ChildProfile->GetName())
				{
					bIsValidPossibleParent = false;
					break;
				}
				else
				{
					CurrentAncestor = CurrentAncestor->Parent != nullptr ? CastChecked<UDeviceProfile>(CurrentAncestor->Parent) : NULL;
				}
			} while(CurrentAncestor && bIsValidPossibleParent);

			if(bIsValidPossibleParent)
			{
				PossibleParentProfiles.Add(ParentProfile);
			}
		}
	}
}
