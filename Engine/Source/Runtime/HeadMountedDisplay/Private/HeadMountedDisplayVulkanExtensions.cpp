// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IHeadMountedDisplayVulkanExtensions.h"
#include "GeneralProjectSettings.h"
#include "Misc/CommandLine.h"

bool IHeadMountedDisplayVulkanExtensions::ShouldDisableVulkanVSync() const
{
	return FParse::Param(FCommandLine::Get(), TEXT("vr")) || GetDefault<UGeneralProjectSettings>()->bStartInVR;
}
