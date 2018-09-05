// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "HAL/IConsoleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(PixelStreaming, Log, VeryVerbose);
DECLARE_LOG_CATEGORY_EXTERN(PixelStreamingInput, Log, VeryVerbose);
DECLARE_LOG_CATEGORY_EXTERN(PixelStreamingNet, Log, VeryVerbose);
DECLARE_LOG_CATEGORY_EXTERN(PixelStreamingCapture, Log, VeryVerbose);

extern TAutoConsoleVariable<int32> CVarEncoderAverageBitRate;
