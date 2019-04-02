// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterBuildConfig.h"


namespace DisplayClusterConstants
{
	namespace net
	{
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
