// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinuxPlatformMisc.cpp: Linux implementations of misc platform functions
=============================================================================*/

#include "Linux/LinuxPlatformMisc.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"

void FLinuxPlatformMisc::GetValidTargetPlatforms(class TArray<class FString>& TargetPlatformNames)
{
	TargetPlatformNames.Add(TEXT("Linux"));
}
