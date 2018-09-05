// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>

#include "BlackmagicLib.h"

#include "Windows/HideWindowsPlatformTypes.h"
#endif

#include "CoreMinimal.h"
#include "BlackmagicMediaSettings.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBlackmagicMedia, Log, All);

namespace BlackmagicMedia
{
	/** Name of the UseTimecode media option. */
	static const FName UseTimecodeOption("UseTimecode");

	/** Name of the UseStreamBuffer media option. */
	static const FName UseStreamBufferOption("UseStreamBuffer");

	/** Debug feature: to encode the timecode into small square on frame */
	static const FName EncodeTimecodeInTexel("EncodeTimecodeInTexel");

	/** Name of the UseTimecode media option. */
	static const FName CaptureStyleOption("CaptureStyle");

	/** Name of the UseTimecode media option. */
	static const FName MediaModeOption("MediaMode");

	/** Name of the UseTimecode media option. */
	static const FName NumFrameBufferOption("NumFrameBuffer");

	/** Enum for number of audio channels to capture. */
	static const FName AudioChannelOption("AudioChannel");
}