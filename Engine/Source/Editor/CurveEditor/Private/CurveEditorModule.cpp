// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "CurveEditorCommands.h"


class FCurveEditorModule : public IModuleInterface
{
public:
	FCurveEditorModule()
	{
		check(false);
	}

	virtual void StartupModule() override
	{
		FCurveEditorCommands::Register();
	}

	virtual void ShutdownModule() override
	{
		FCurveEditorCommands::Unregister();
	}
};


IMPLEMENT_MODULE(FCurveEditorModule, CurveEditor)