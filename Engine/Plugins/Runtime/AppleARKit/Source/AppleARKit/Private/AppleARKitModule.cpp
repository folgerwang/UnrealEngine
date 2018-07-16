// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AppleARKitModule.h"
#include "AppleARKitSystem.h"
#include "Modules/ModuleManager.h"
#include "Features/IModularFeature.h"
#include "Features/IModularFeatures.h"
#include "CoreGlobals.h"
#include "Misc/CoreDelegates.h"


TWeakPtr<class FAppleARKitSystem, ESPMode::ThreadSafe> FAppleARKitARKitSystemPtr;

TSharedPtr<class IXRTrackingSystem, ESPMode::ThreadSafe> FAppleARKitModule::CreateTrackingSystem()
{
#if PLATFORM_IOS
	auto NewARKitSystem = AppleARKitSupport::CreateAppleARKitSystem();
	FAppleARKitARKitSystemPtr = NewARKitSystem;
    return NewARKitSystem;
#else
	return TSharedPtr<class IXRTrackingSystem, ESPMode::ThreadSafe>();
#endif
}

TSharedPtr<class FAppleARKitSystem, ESPMode::ThreadSafe> FAppleARKitModule::GetARKitSystem()
{
    return FAppleARKitARKitSystemPtr.Pin();
}

FString FAppleARKitModule::GetModuleKeyName() const
{
    static const FString ModuleKeyName(TEXT("AppleARKit"));
    return ModuleKeyName;
}

void FAppleARKitModule::StartupModule()
{
	ensureMsgf(FModuleManager::Get().LoadModule("AugmentedReality"), TEXT("ARKit depends on the AugmentedReality module."));
	IHeadMountedDisplayModule::StartupModule();

	FCoreDelegates::OnPreExit.AddRaw(this, &FAppleARKitModule::PreExit);
}

void FAppleARKitModule::PreExit()
{
	if (FAppleARKitARKitSystemPtr.IsValid())
	{
		FAppleARKitARKitSystemPtr.Pin()->Shutdown();
	}
	FAppleARKitARKitSystemPtr.Reset();
}

void FAppleARKitModule::ShutdownModule()
{
	IHeadMountedDisplayModule::ShutdownModule();
}


IMPLEMENT_MODULE(FAppleARKitModule, AppleARKit);

DEFINE_LOG_CATEGORY(LogAppleARKit);

