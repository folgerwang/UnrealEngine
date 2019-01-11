// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "IOpenColorIOModule.h"

#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"


DEFINE_LOG_CATEGORY(LogOpenColorIO);

#define LOCTEXT_NAMESPACE "OpenColorIOModule"

/**
 * Implements the OpenColorIO module.
 */
class FOpenColorIOModule : public IOpenColorIOModule
{
public:

	//~ IModuleInterface interface
	virtual void StartupModule() override
	{
		// Maps virtual shader source directory /Plugin/OpenCVLensDistortion to the plugin's actual Shaders directory.
		FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("OpenColorIO"))->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/OpenColorIO"), PluginShaderDir);
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FOpenColorIOModule, OpenColorIO);

#undef LOCTEXT_NAMESPACE
