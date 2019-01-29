// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LuminARModule.h"
#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"
#include "Features/IModularFeature.h"

#include "LuminARTrackingSystem.h"
#include "LuminARDevice.h"
#include "Templates/SharedPointer.h"


#define LOCTEXT_NAMESPACE "LuminAR"

class FLuminARModule : public ILuminARModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	//create for mutual connection (regardless of construction order)
	virtual TSharedPtr<IARSystemSupport, ESPMode::ThreadSafe> CreateARImplementation() override;
	//Now connect (regardless of connection order)
	virtual void ConnectARImplementationToXRSystem(FXRTrackingSystemBase* InXRTrackingSystem) override;
	//Now initialize fully connected systems
	virtual void InitializeARImplementation() override;

private:
	TSharedPtr<FLuminARImplementation, ESPMode::ThreadSafe> LuminARImplementation;
};

IMPLEMENT_MODULE(FLuminARModule, MagicLeapAR)


void FLuminARModule::StartupModule()
{
	ensureMsgf(FModuleManager::Get().LoadModule("AugmentedReality"), TEXT("Lumin AR depends on the AugmentedReality module.") );

	FModuleManager::LoadModulePtr<IModuleInterface>("AugmentedReality");
}

void FLuminARModule::ShutdownModule()
{
	// Complete LuminAR teardown.
	FLuminARDevice::GetInstance()->OnModuleUnloaded();
}


//create for mutual connection (regardless of construction order)
TSharedPtr<IARSystemSupport, ESPMode::ThreadSafe> FLuminARModule::CreateARImplementation()
{
#if PLATFORM_LUMIN
	LuminARImplementation = MakeShareable(new FLuminARImplementation());
#endif
	return LuminARImplementation;
}

void FLuminARModule::ConnectARImplementationToXRSystem(FXRTrackingSystemBase* InXRTrackingSystem)
{
	ensure(InXRTrackingSystem);

	FLuminARDevice::GetInstance()->SetLuminARImplementation(LuminARImplementation);
	FLuminARDevice::GetInstance()->SetARSystem(InXRTrackingSystem->GetARCompositionComponent());
	InXRTrackingSystem->GetARCompositionComponent()->InitializeARSystem();
}

void FLuminARModule::InitializeARImplementation()
{
	// Complete LuminAR setup.
	FLuminARDevice::GetInstance()->OnModuleLoaded();
}

#undef LOCTEXT_NAMESPACE
