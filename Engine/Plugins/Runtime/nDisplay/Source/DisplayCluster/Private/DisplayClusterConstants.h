// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterBuildConfig.h"


namespace DisplayClusterConstants
{
	namespace net
	{
		static constexpr int32  ClientConnectTriesAmount    = 10;   // times
		static constexpr float  ClientConnectRetryDelay     = 1.0f; // sec
		static constexpr uint32 BarrierGameStartWaitTimeout = 20000; // ms
		static constexpr uint32 BarrierWaitTimeout          = 5000; // ms
		static constexpr int32  SocketBufferSize            = INT16_MAX; // bytes
		static constexpr int32  MessageBufferSize           = INT16_MAX; // bytes
	};

	namespace misc
	{
#ifdef DISPLAY_CLUSTER_USE_DEBUG_STANDALONE_CONFIG
		static constexpr int32 DebugAutoWinX = 0;
		static constexpr int32 DebugAutoWinY = 0;
		static constexpr int32 DebugAutoResX = 1920;
		static constexpr int32 DebugAutoResY = 1080;
#endif
	}
};
