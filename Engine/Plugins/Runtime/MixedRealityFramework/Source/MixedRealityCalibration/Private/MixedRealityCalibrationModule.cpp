// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IMixedRealityCalibrationModule.h"
#include "Modules/ModuleManager.h" // for IMPLEMENT_MODULE()
#include "HAL/PlatformProcess.h"

class FMixedRealityCalibrationModule : public IMixedRealityCalibrationModule
{
public:
	FMixedRealityCalibrationModule();

public:
	//~ IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

FMixedRealityCalibrationModule::FMixedRealityCalibrationModule()
{}

void FMixedRealityCalibrationModule::StartupModule()
{
}

void FMixedRealityCalibrationModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FMixedRealityCalibrationModule, MixedRealityCalibration);
