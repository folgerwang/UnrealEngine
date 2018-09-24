// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Containers/Array.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"
#include "Toolkits/AssetEditorToolkit.h"

#define LOCTEXT_NAMESPACE "FVirtualTexturingEditorModule"

class FVirtualTexturingEditorModule
	: public IModuleInterface
{

public:

	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

protected:

};


IMPLEMENT_MODULE(FVirtualTexturingEditorModule, VirtualTexturingEditor);

#undef LOCTEXT_NAMESPACE