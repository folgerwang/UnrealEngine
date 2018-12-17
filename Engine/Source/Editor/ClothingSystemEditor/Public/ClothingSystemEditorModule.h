// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "SimulationEditorExtenderNv.h"
#include "ClothingAssetFactoryInterface.h"

class UClothingAssetFactoryBase;

class FClothingSystemEditorModule : public IModuleInterface, public IClothingAssetFactoryProvider
{

public:

	FClothingSystemEditorModule();

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual UClothingAssetFactoryBase* GetFactory() override;

private:

#if WITH_NVCLOTH
	FSimulationEditorExtenderNv NvEditorExtender;
#endif

};
