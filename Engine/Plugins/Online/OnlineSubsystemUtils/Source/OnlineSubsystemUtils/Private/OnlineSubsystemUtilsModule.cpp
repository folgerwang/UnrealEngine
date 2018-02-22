// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsModule.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineDelegates.h"
#include "OnlinePIESettings.h"

IMPLEMENT_MODULE(FOnlineSubsystemUtilsModule, OnlineSubsystemUtils);

/**
 * Concrete implementation of IOnlineSubsystemUtils interface 
 */
class FOnlineSubsystemUtils : public IOnlineSubsystemUtils
{
public:

	FOnlineSubsystemUtils() 
		: bShouldTryOnlinePIE(true)
	{
	}

	virtual ~FOnlineSubsystemUtils()
	{
		FOnlineSubsystemDelegates::OnOnlineSubsystemCreated.Remove(OnOnlineSubsystemCreatedDelegateHandle);
	}

	virtual FName GetOnlineIdentifier(const FWorldContext& WorldContext, const FName Subsystem = NAME_None) const override
	{
#if WITH_EDITOR
		if (WorldContext.WorldType == EWorldType::PIE)
		{
			return FName(*FString::Printf(TEXT("%s:%s"), !Subsystem.IsNone() ? *Subsystem.ToString() : TEXT(""), *WorldContext.ContextHandle.ToString()));
		}
#endif

		return Subsystem;
	}

	virtual FName GetOnlineIdentifier(UWorld* World, const FName Subsystem = NAME_None) const override
	{
#if WITH_EDITOR
		if (const FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(World))
		{
			return GetOnlineIdentifier(*WorldContext, Subsystem);
		}
#endif
		return Subsystem;
	}

	virtual void OnOnlineSubsystemCreated(IOnlineSubsystem* NewSubsystem)
	{
		if (OnExternalUIChangeDelegate.IsBound())
		{
			IOnlineExternalUIPtr ExternalUI = NewSubsystem->GetExternalUIInterface();
			if (ExternalUI.IsValid())
			{
				ExternalUI->AddOnExternalUIChangeDelegate_Handle(OnExternalUIChangeDelegate);
			}
		}
	}

	virtual void SetEngineExternalUIBinding(const FOnExternalUIChangeDelegate& InOnExternalUIChangeDelegate) override
	{
		OnExternalUIChangeDelegate = InOnExternalUIChangeDelegate;

		FOnlineSubsystemModule::FEnumerateOnlineSubsystemCb ExtDelegateCb = [this](IOnlineSubsystem* Subsystem)
		{
			IOnlineExternalUIPtr ExternalUI = Subsystem->GetExternalUIInterface();
			if (ExternalUI.IsValid())
			{
				FDelegateHandle* ExistingDelegateHandle = ExternalUIDelegateHandles.Find(Subsystem->GetSubsystemName());
				if (ExistingDelegateHandle && ExistingDelegateHandle->IsValid())
				{
					ExternalUI->ClearOnExternalUIChangeDelegate_Handle(*ExistingDelegateHandle);
				}

				FDelegateHandle DelegateHandle = ExternalUI->AddOnExternalUIChangeDelegate_Handle(OnExternalUIChangeDelegate);
				ExternalUIDelegateHandles.Add(Subsystem->GetSubsystemName(), DelegateHandle);
			}
		};
		
		FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
		OSS.EnumerateOnlineSubsystems(ExtDelegateCb);
	}

#if WITH_EDITOR
	virtual bool SupportsOnlinePIE() const override
	{
		check(UObjectInitialized());
		const UOnlinePIESettings* OnlinePIESettings = GetDefault<UOnlinePIESettings>();
		if (OnlinePIESettings->bOnlinePIEEnabled && GetNumPIELogins() > 0)
		{
			// If we can't get the identity interface then things are either not configured right or disabled
			IOnlineIdentityPtr IdentityInt = Online::GetIdentityInterface();
			return IdentityInt.IsValid();
		}

		return false;
	}

	virtual void SetShouldTryOnlinePIE(bool bShouldTry) override
	{
		if (bShouldTryOnlinePIE != bShouldTry)
		{
			bShouldTryOnlinePIE = bShouldTry;

			// This will swap it back to the null subsystem if needed
			IOnlineSubsystem::ReloadDefaultSubsystem();
		}
	}

	virtual bool IsOnlinePIEEnabled() const override
	{
		check(UObjectInitialized());
		const UOnlinePIESettings* OnlinePIESettings = GetDefault<UOnlinePIESettings>();
		return bShouldTryOnlinePIE && OnlinePIESettings->bOnlinePIEEnabled;
	}

	virtual int32 GetNumPIELogins() const override
	{
		check(UObjectInitialized());

		int32 NumValidLogins = 0;
		const UOnlinePIESettings* OnlinePIESettings = GetDefault<UOnlinePIESettings>();
		for (const FPIELoginSettingsInternal& Login : OnlinePIESettings->Logins)
		{
			if (Login.IsValid())
			{
				NumValidLogins++;
			}
		}
	
		return NumValidLogins;
	}

	virtual void GetPIELogins(TArray<FOnlineAccountCredentials>& Logins) override
	{
		check(UObjectInitialized());
		const UOnlinePIESettings* OnlinePIESettings = GetDefault<UOnlinePIESettings>();
		if (OnlinePIESettings->Logins.Num() > 0)
		{
			FOnlineAccountCredentials TempLogin;

			Logins.Empty(OnlinePIESettings->Logins.Num());
			for (const FPIELoginSettingsInternal& Login : OnlinePIESettings->Logins)
			{
				if (Login.IsValid())
				{
					new (Logins)FOnlineAccountCredentials(Login.Type, Login.Id, Login.Token);
				}
			}
		}
	}

#endif // WITH_EDITOR

private:

	void Init()
	{
		FOnlineSubsystemDelegates::OnOnlineSubsystemCreated.AddRaw(this, &FOnlineSubsystemUtils::OnOnlineSubsystemCreated);
	}

	/** If false it will not try to do online PIE at all */
	bool bShouldTryOnlinePIE;

	/** Delegate set by the engine for notification of external UI operations */
	FOnExternalUIChangeDelegate OnExternalUIChangeDelegate;
	TMap<FName, FDelegateHandle> ExternalUIDelegateHandles;

	/** Delegate binding when new online subsystems are created */
	FDelegateHandle OnOnlineSubsystemCreatedDelegateHandle;

	friend FOnlineSubsystemUtilsModule;
};

void FOnlineSubsystemUtilsModule::StartupModule()
{
	TUniquePtr<FOnlineSubsystemUtils> TempUtils = MakeUnique<FOnlineSubsystemUtils>();
	TempUtils->Init();
	SubsystemUtils = MoveTemp(TempUtils);
}

void FOnlineSubsystemUtilsModule::ShutdownModule()
{
	SubsystemUtils.Reset();
}
