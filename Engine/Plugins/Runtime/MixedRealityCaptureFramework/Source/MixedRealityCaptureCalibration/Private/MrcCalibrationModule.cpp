// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IMrcCalibrationModule.h"
#include "Modules/ModuleManager.h" // for IMPLEMENT_MODULE()
#include "HAL/PlatformProcess.h"

class FMrcCalibrationModule : public IMrcCalibrationModule
{
public:
	FMrcCalibrationModule();

public:
	//~ IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

FMrcCalibrationModule::FMrcCalibrationModule()
{}

void FMrcCalibrationModule::StartupModule()
{
}

void FMrcCalibrationModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FMrcCalibrationModule, MixedRealityCaptureCalibration);
