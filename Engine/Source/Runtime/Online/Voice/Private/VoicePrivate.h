// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS

#include "Windows/WindowsHWrapper.h"

#include "Windows/AllowWindowsPlatformTypes.h"

THIRD_PARTY_INCLUDES_START
#include <Audiopolicy.h>
#include <Mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <audiodefs.h>
#include <dsound.h>
THIRD_PARTY_INCLUDES_END

#include "Windows/HideWindowsPlatformTypes.h"

#endif // PLATFORM_WINDOWS

#define ANDROIDVOICE_SUPPORTED_PLATFORMS (PLATFORM_ANDROID && (PLATFORM_ANDROID_ARM || PLATFORM_ANDROID_ARM64 || PLATFORM_ANDROID_X64) && !PLATFORM_LUMIN)
#define PLATFORM_SUPPORTS_VOICE_CAPTURE (PLATFORM_WINDOWS || PLATFORM_MAC || ANDROIDVOICE_SUPPORTED_PLATFORMS || (PLATFORM_UNIX && VOICE_MODULE_WITH_CAPTURE))
#define PLATFORM_SUPPORTS_OPUS_CODEC (!PLATFORM_HTML5 && !PLATFORM_TVOS)

// Module includes
