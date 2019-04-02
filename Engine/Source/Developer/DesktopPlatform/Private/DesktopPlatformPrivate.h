// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS
#include "Windows/DesktopPlatformWindows.h"
#elif PLATFORM_MAC
#include "Mac/DesktopPlatformMac.h"
#elif PLATFORM_LINUX
#include "Linux/DesktopPlatformLinux.h"
#else
#include "DesktopPlatformStub.h"
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogDesktopPlatform, Log, All);
