// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include <functional>
#if !COMPILE_WITHOUT_UNREAL_SUPPORT
#include "CoreTypes.h"
#include "Logging/MessageLog.h"
#include "Misc/CoreMiscDefines.h"

DECLARE_LOG_CATEGORY_EXTERN(LogApeiron, Verbose, All);
#else
#include <stdint.h>
#define PI 3.14159
#define check(condition)

typedef int32_t int32;
#endif