// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ControlRigDeveloper.h"
#include "Modules/ModuleManager.h"

class FControlRigDeveloperModule : public IControlRigDeveloperModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FControlRigDeveloperModule, ControlRigDeveloper)
