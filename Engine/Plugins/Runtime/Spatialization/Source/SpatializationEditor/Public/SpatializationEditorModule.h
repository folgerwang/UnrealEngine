// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "ITDSpatializer.h"

class FSpatializationEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;
};
