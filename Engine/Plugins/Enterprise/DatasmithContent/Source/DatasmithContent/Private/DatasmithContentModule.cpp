// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DatasmithContentModule.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

DEFINE_LOG_CATEGORY(LogDatasmithContent)

/**
 * DatasmithContent module implementation (private)
 */
class FDatasmithContentModule : public IDatasmithContentModule
{
public:
	virtual void StartupModule() override
	{
		// Create temporary directory which will be used by UDatasmithStaticMeshCADImportData to store transient data
		TempDir = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("DatasmithContentTemp"));
		IFileManager::Get().MakeDirectory(*TempDir);

		// Maps virtual shader source directory /Plugin/DatasmithContent to the plugin's actual Shaders directory.
		FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("DatasmithContent"))->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/DatasmithContent"), PluginShaderDir);
	}

	virtual void ShutdownModule() override
	{
		// Clean up all transient files created during the process
		IFileManager::Get().DeleteDirectory(*TempDir, false, true);
	}

	const FString& GetTempDir() const override
	{
		return TempDir;
	}

private:
	FString TempDir;
};

IMPLEMENT_MODULE(FDatasmithContentModule, DatasmithContent);
