// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemModule.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemImpl.h"
#include "OnlineDelegates.h"

IMPLEMENT_MODULE( FOnlineSubsystemModule, OnlineSubsystem );

/**
 * OnlineDelegates.h static declarations
 */
FOnlineSubsystemDelegates::FOnOnlineSubsystemCreated FOnlineSubsystemDelegates::OnOnlineSubsystemCreated;

/** Helper function to turn the friendly subsystem name into the module name */
static inline FName GetOnlineModuleName(const FString& SubsystemName)
{
	FString ModuleBase(TEXT("OnlineSubsystem"));

	FName ModuleName;
	if (!SubsystemName.StartsWith(ModuleBase, ESearchCase::CaseSensitive))
	{
		ModuleName = FName(*(ModuleBase + SubsystemName));
	}
	else
	{
		ModuleName = FName(*SubsystemName);
	}

	return ModuleName;
}

/**
 * Helper function that loads a given platform service module if it isn't already loaded
 *
 * @param SubsystemName Name of the requested platform service to load
 * @return The module interface of the requested platform service, NULL if the service doesn't exist
 */
static IModuleInterface* LoadSubsystemModule(const FString& SubsystemName)
{
	const bool bAttemptLoadModule = IOnlineSubsystem::IsEnabled(FName(*SubsystemName));
	if (bAttemptLoadModule)
	{
		const FName ModuleName = GetOnlineModuleName(SubsystemName);
		FModuleManager& ModuleManager = FModuleManager::Get();

		if (!ModuleManager.IsModuleLoaded(ModuleName))
		{
			// Attempt to load the module
			ModuleManager.LoadModule(ModuleName);
		}

		return ModuleManager.GetModule(ModuleName);
	}

	return nullptr;
}

void FOnlineSubsystemModule::StartupModule()
{
	// These should not be LoadModuleChecked because these modules might not exist
	// Load dependent modules to ensure they will still exist during ShutdownModule.
	// We will always load these modules at the cost of extra modules loaded for the few OSS (like Null) that don't use it.
	if (FModuleManager::Get().ModuleExists(TEXT("HTTP")))
	{
		FModuleManager::Get().LoadModule(TEXT("HTTP"));
	}
	if (FModuleManager::Get().ModuleExists(TEXT("XMPP")))
	{
		FModuleManager::Get().LoadModule(TEXT("XMPP"));
	}

	LoadDefaultSubsystem();
	
	// Also load the console/platform specific OSS which might not necessarily be the default OSS instance

	FString InterfaceString;
	GConfig->GetString(TEXT("OnlineSubsystem"), TEXT("NativePlatformService"), InterfaceString, GEngineIni);
	NativePlatformService = FName(*InterfaceString);

	IOnlineSubsystem::GetByPlatform();
}

void FOnlineSubsystemModule::PreUnloadCallback()
{
	PreUnloadOnlineSubsystem();
}

void FOnlineSubsystemModule::ShutdownModule()
{
	ShutdownOnlineSubsystem();
}

bool FOnlineSubsystemModule::TryLoadSubsystemAndSetDefault(FName ModuleName)
{
	// A module loaded with its factory method set for creation and a default instance of the online subsystem is required
	if ((LoadSubsystemModule(ModuleName.ToString()) &&
		OnlineFactories.Contains(ModuleName) &&
		GetOnlineSubsystem(ModuleName) != NULL))
	{
		DefaultPlatformService = ModuleName;
		return true;
	}

	return false;
}

void FOnlineSubsystemModule::LoadDefaultSubsystem()
{
	FString InterfaceString;
	GConfig->GetString(TEXT("OnlineSubsystem"), TEXT("DefaultPlatformService"), InterfaceString, GEngineIni);

	bool bHasLoadedModule = false;
	if (InterfaceString.Len() > 0)
	{
		bHasLoadedModule = TryLoadSubsystemAndSetDefault(FName(*InterfaceString));
	}

	// if default fails, attempt to load Null
	if (!bHasLoadedModule)
	{
		bHasLoadedModule = TryLoadSubsystemAndSetDefault(NULL_SUBSYSTEM);
	}

	if (!bHasLoadedModule)
	{
		UE_LOG_ONLINE(Log, TEXT("Failed to load any Online Subsystem Modules"));
	}
}

void FOnlineSubsystemModule::ReloadDefaultSubsystem()
{
	DestroyOnlineSubsystem(DefaultPlatformService);
	LoadDefaultSubsystem();
}

void FOnlineSubsystemModule::PreUnloadOnlineSubsystem()
{
	// Shutdown all online subsystem instances
	for (TMap<FName, IOnlineSubsystemPtr>::TIterator It(OnlineSubsystems); It; ++It)
	{
		It.Value()->PreUnload();
	}
}

void FOnlineSubsystemModule::ShutdownOnlineSubsystem()
{
	FModuleManager& ModuleManager = FModuleManager::Get();

	// Shutdown all online subsystem instances
	for (TMap<FName, IOnlineSubsystemPtr>::TIterator It(OnlineSubsystems); It; ++It)
	{
		It.Value()->Shutdown();
	}
	OnlineSubsystems.Empty();

	// Unload all the supporting factories
	for (TMap<FName, IOnlineFactory*>::TIterator It(OnlineFactories); It; ++It)
	{
		UE_LOG_ONLINE(Verbose, TEXT("Unloading online subsystem: %s"), *It.Key().ToString());

		// Unloading the module will do proper cleanup
		FName ModuleName = GetOnlineModuleName(It.Key().ToString());

		const bool bIsShutdown = true;
		ModuleManager.UnloadModule(ModuleName, bIsShutdown);
	} 
	//ensure(OnlineFactories.Num() == 0);
}

void FOnlineSubsystemModule::RegisterPlatformService(const FName FactoryName, IOnlineFactory* Factory)
{
	if (!OnlineFactories.Contains(FactoryName))
	{
		OnlineFactories.Add(FactoryName, Factory);
	}
}

void FOnlineSubsystemModule::UnregisterPlatformService(const FName FactoryName)
{
	if (OnlineFactories.Contains(FactoryName))
	{
		OnlineFactories.Remove(FactoryName);
	}
}

void FOnlineSubsystemModule::EnumerateOnlineSubsystems(FEnumerateOnlineSubsystemCb& EnumCb)
{
	for (TPair<FName, IOnlineSubsystemPtr>& OnlineSubsystem : OnlineSubsystems)
	{
		if (OnlineSubsystem.Value.IsValid())
		{
			EnumCb(OnlineSubsystem.Value.Get());
		}
	}
}

FName FOnlineSubsystemModule::ParseOnlineSubsystemName(const FName& FullName, FName& SubsystemName, FName& InstanceName) const
{
#if !(UE_GAME || UE_SERVER)
	SubsystemName = DefaultPlatformService;
	InstanceName = FOnlineSubsystemImpl::DefaultInstanceName;

	if (!FullName.IsNone())
	{
		FString FullNameStr = FullName.ToString();

		int32 DelimIdx = INDEX_NONE;
		static const TCHAR InstanceDelim = ':';
		if (FullNameStr.FindChar(InstanceDelim, DelimIdx))
		{
			if (DelimIdx > 0)
			{
				SubsystemName = FName(*FullNameStr.Left(DelimIdx));
			}

			if ((DelimIdx + 1) < FullNameStr.Len())
			{
				InstanceName = FName(*FullNameStr.RightChop(DelimIdx + 1));
			}
		}
		else
		{
			SubsystemName = FName(*FullNameStr);
		}
	}

	return FName(*FString::Printf(TEXT("%s:%s"), *SubsystemName.ToString(), *InstanceName.ToString()));
#else	
	
	SubsystemName = FullName.IsNone() ? DefaultPlatformService : FullName;
	InstanceName = FOnlineSubsystemImpl::DefaultInstanceName;

#if !UE_BUILD_SHIPPING
	int32 DelimIdx = INDEX_NONE;
	static const TCHAR InstanceDelim = ':';
	ensure(!FullName.ToString().FindChar(InstanceDelim, DelimIdx) && DelimIdx == INDEX_NONE);
#endif
	return SubsystemName;
#endif // !(UE_GAME || UE_SERVER)
}

IOnlineSubsystem* FOnlineSubsystemModule::GetOnlineSubsystem(const FName InSubsystemName)
{
	FName SubsystemName, InstanceName;
	FName KeyName = ParseOnlineSubsystemName(InSubsystemName, SubsystemName, InstanceName);

	IOnlineSubsystemPtr* OnlineSubsystem = nullptr;
	if (!SubsystemName.IsNone())
	{
		OnlineSubsystem = OnlineSubsystems.Find(KeyName);
		if (OnlineSubsystem == nullptr)
		{
			if (IOnlineSubsystem::IsEnabled(SubsystemName))
			{
				IOnlineFactory** OSSFactory = OnlineFactories.Find(SubsystemName);
				if (OSSFactory == nullptr)
				{
					// Attempt to load the requested factory
					IModuleInterface* NewModule = LoadSubsystemModule(SubsystemName.ToString());
					if (NewModule)
					{
						// If the module loaded successfully this should be non-NULL
						OSSFactory = OnlineFactories.Find(SubsystemName);
					}
				}

				if (OSSFactory != nullptr)
				{
					IOnlineSubsystemPtr NewSubsystemInstance = (*OSSFactory)->CreateSubsystem(InstanceName);
					if (NewSubsystemInstance.IsValid())
					{
						OnlineSubsystems.Add(KeyName, NewSubsystemInstance);
						OnlineSubsystem = OnlineSubsystems.Find(KeyName);
						FOnlineSubsystemDelegates::OnOnlineSubsystemCreated.Broadcast(NewSubsystemInstance.Get());
					}
					else
					{
						bool* bNotedPreviously = OnlineSubsystemFailureNotes.Find(KeyName);
						if (!bNotedPreviously || !(*bNotedPreviously))
						{
							UE_LOG_ONLINE(Log, TEXT("Unable to create OnlineSubsystem module %s"), *SubsystemName.ToString());
							OnlineSubsystemFailureNotes.Add(KeyName, true);
						}
					}
				}
			}
		}
	}

	return (OnlineSubsystem == nullptr) ? nullptr : (*OnlineSubsystem).Get();
}

IOnlineSubsystem* FOnlineSubsystemModule::GetNativeSubsystem(bool bAutoLoad)
{
	if (!NativePlatformService.IsNone())
	{
		if (bAutoLoad || IOnlineSubsystem::IsLoaded(NativePlatformService))
		{
			return IOnlineSubsystem::Get(NativePlatformService);
		}
	}
	return nullptr;
}

void FOnlineSubsystemModule::DestroyOnlineSubsystem(const FName InSubsystemName)
{
	FName SubsystemName, InstanceName;
	FName KeyName = ParseOnlineSubsystemName(InSubsystemName, SubsystemName, InstanceName);

	if (!SubsystemName.IsNone())
	{
		IOnlineSubsystemPtr OnlineSubsystem;
		OnlineSubsystems.RemoveAndCopyValue(KeyName, OnlineSubsystem);
		if (OnlineSubsystem.IsValid())
		{
			OnlineSubsystem->Shutdown();
			OnlineSubsystemFailureNotes.Remove(KeyName);
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("OnlineSubsystem instance %s not found, unable to destroy."), *KeyName.ToString());
		}
	}
}

bool FOnlineSubsystemModule::DoesInstanceExist(const FName InSubsystemName) const
{
	bool bIsLoaded = false;

	FName SubsystemName, InstanceName;
	FName KeyName = ParseOnlineSubsystemName(InSubsystemName, SubsystemName, InstanceName);
	if (!SubsystemName.IsNone())
	{
		const IOnlineSubsystemPtr* OnlineSubsystem = OnlineSubsystems.Find(KeyName);
		return OnlineSubsystem && OnlineSubsystem->IsValid() ? true : false;
	}

	return false;
}

bool FOnlineSubsystemModule::IsOnlineSubsystemLoaded(const FName InSubsystemName) const
{
	bool bIsLoaded = false;

	FName SubsystemName, InstanceName;
	ParseOnlineSubsystemName(InSubsystemName, SubsystemName, InstanceName);

	if (!SubsystemName.IsNone())
	{
		if (FModuleManager::Get().IsModuleLoaded(GetOnlineModuleName(SubsystemName.ToString())))
		{
			bIsLoaded = true;
		}
	}
	return bIsLoaded;
}

