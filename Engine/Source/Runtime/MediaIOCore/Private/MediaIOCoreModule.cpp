// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaIOCoreModule.h"

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogMediaIOCore);

/**
 * Implements the MediaIOCore Module module.
 */
class FMediaIOCoreModule
	: public IModuleInterface
{
};

IMPLEMENT_MODULE(FMediaIOCoreModule, MediaIOCore);
