// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#if WITH_LIBWEBSOCKETS
	#if PLATFORM_SWITCH
		#include "Lws/Switch/LwsSwitchWebSocketsManager.h"
	#else
		#include "Lws/LwsWebSocketsManager.h"
	#endif //PLATFORM_SWITCH

#elif PLATFORM_XBOXONE
	#include "XboxOne/XboxOneWebSocketsManager.h"
#else
	#error "Web sockets not implemented on this platform yet"
#endif // WITH_LIBWEBSOCKETS

#if WITH_LIBWEBSOCKETS
	#if PLATFORM_SWITCH
		typedef FLwsSwitchWebSocketsManager FPlatformWebSocketsManager;
	#else
		typedef FLwsWebSocketsManager FPlatformWebSocketsManager;
	#endif // PLATFORM_SWITCH

#elif PLATFORM_XBOXONE
	typedef FXboxOneWebSocketsManager FPlatformWebSocketsManager;
#endif //WITH_LIBWEBSOCKETS
