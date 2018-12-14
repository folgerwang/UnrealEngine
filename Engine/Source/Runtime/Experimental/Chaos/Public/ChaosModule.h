// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if INCLUDE_CHAOS

#pragma once

#include "Modules/ModuleInterface.h"

class CHAOS_API FChaosEngineModule : public IModuleInterface
{
public:

	virtual void StartupModule() override {};
	virtual void ShutdownModule() override {};
};

#endif