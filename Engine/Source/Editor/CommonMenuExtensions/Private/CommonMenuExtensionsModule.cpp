// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CommonMenuExtensionsModule.h"
#include "BufferVisualizationMenuCommands.h"
#include "ShowFlagMenuCommands.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FCommonMenuExtensionsModule, CommonMenuExtensions);

void FCommonMenuExtensionsModule::StartupModule()
{
	FBufferVisualizationMenuCommands::Register();
	FShowFlagMenuCommands::Register();
}

void FCommonMenuExtensionsModule::ShutdownModule()
{
	FShowFlagMenuCommands::Unregister();
	FBufferVisualizationMenuCommands::Unregister();
}