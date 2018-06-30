// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaFrameworkUtilitiesModule.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogMediaFrameworkUtilities);

/**
 * Implements the MediaFrameworkUtilitiesModule module.
 */
class FMediaFrameworkUtilitiesModule : public IModuleInterface
{
};

IMPLEMENT_MODULE(FMediaFrameworkUtilitiesModule, MediaFrameworkUtilities);

