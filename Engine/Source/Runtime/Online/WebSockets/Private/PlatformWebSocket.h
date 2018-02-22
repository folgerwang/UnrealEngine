// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#if WITH_LIBWEBSOCKETS
#include "Lws/LwsWebSocketsManager.h"
typedef FLwsWebSocketsManager FPlatformWebSocketsManager;
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneWebSocketsManager.h"
typedef FXboxOneWebSocketsManager FPlatformWebSocketsManager;
#else
#error "Web sockets not implemented on this platform yet"
#endif
