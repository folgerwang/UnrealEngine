// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	LinuxPlatformMisc.h: Linux platform misc functions
==============================================================================================*/

#pragma once

#include "Unix/UnixPlatformMisc.h"

/**
 * Linux implementation of the misc OS functions
 */
struct CORE_API FLinuxPlatformMisc : public FUnixPlatformMisc
{
	static void GetValidTargetPlatforms(class TArray<class FString>& TargetPlatformNames);
};

typedef FLinuxPlatformMisc FPlatformMisc;
