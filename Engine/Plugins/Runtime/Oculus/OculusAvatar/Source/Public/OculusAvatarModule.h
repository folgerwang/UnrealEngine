// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
 
#include "Modules/ModuleManager.h"
 
class OculusAvatarModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	void StartupModule();
	void ShutdownModule();
};
