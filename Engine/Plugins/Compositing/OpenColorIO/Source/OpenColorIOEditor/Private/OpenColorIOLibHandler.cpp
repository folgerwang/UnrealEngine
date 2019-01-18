// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOLibHandler.h"

#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "IOpenColorIOEditorModule.h"
#include "Logging/LogMacros.h"
#include "Misc/Paths.h"


 //~ Static initialization
 //--------------------------------------------------------------------
void* FOpenColorIOLibHandler::LibHandle = nullptr;


//~ Initialization functions implementation
//--------------------------------------------------------------------
bool FOpenColorIOLibHandler::Initialize()
{
#if WITH_EDITOR
	check(LibHandle == nullptr);

	const FString PluginDir = IPluginManager::Get().FindPlugin(TEXT("OpenColorIO"))->GetBaseDir();
	const FString OCIOBinPath = FPaths::Combine(PluginDir, TEXT(PREPROCESSOR_TO_STRING(OCIO_PLATFORM_PATH)));
	const FString DLLPath = FPaths::Combine(OCIOBinPath, TEXT(PREPROCESSOR_TO_STRING(OCIO_DLL_NAME)));

	FPlatformProcess::PushDllDirectory(*OCIOBinPath);

	if (!FPaths::FileExists(DLLPath))
	{
		UE_LOG(LogOpenColorIOEditor, Error, TEXT("Failed to find the OpenColorIO dll. Plug-in will not be functional."));
		return false;
	}
	
	LibHandle = FPlatformProcess::GetDllHandle(*DLLPath);
	FPlatformProcess::PopDllDirectory(*OCIOBinPath);

	if (LibHandle == nullptr)
	{
		UE_LOG(LogOpenColorIOEditor, Error, TEXT("Failed to load required library %s. Plug-in will not be functional."), *DLLPath);
		return false;
	}

#else
	return false;
#endif // WITH_EDITOR
	return true;
}

bool FOpenColorIOLibHandler::IsInitialized()
{
	return (LibHandle != nullptr);
}

void FOpenColorIOLibHandler::Shutdown()
{
#if WITH_EDITOR
	if (LibHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(LibHandle);
		LibHandle = nullptr;
	}
#endif // WITH_EDITOR
}



