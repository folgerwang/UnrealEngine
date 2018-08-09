// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchSettings.h"
#include "Misc/App.h"
#include "HAL/PlatformProcess.h"

namespace BuildPatchServices
{
	FBuildPatchServicesInitSettings::FBuildPatchServicesInitSettings()
		: ApplicationSettingsDir(FPlatformProcess::ApplicationSettingsDir())
		, ProjectName(FApp::GetProjectName())
		, LocalMachineConfigFileName(TEXT("BuildPatchServicesLocal.ini"))
	{
	}
}
