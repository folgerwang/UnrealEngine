// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Plugin-wide log categories
#if UE_BUILD_SHIPPING
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterGameMode,   Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterEngine,     Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterModule,     Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterCluster,    Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterConfig,     Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterGame,       Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterInput,      Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterInputVRPN,  Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterNetwork,    Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterNetworkMsg, Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterRender,     Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterBlueprint,  Warning, Warning);
#else
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterGameMode,   Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterEngine,     Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterModule,     Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterCluster,    Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterConfig,     Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterGame,       Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterInput,      Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterInputVRPN,  Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterNetwork,    Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterNetworkMsg, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterRender,     Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterBlueprint,  Log, All);
#endif


//@todo: Linux@GCC will probably require other macro
#if UE_BUILD_SHIPPING
	#define DISPLAY_CLUSTER_FUNC_TRACE(cat) ;
#else
	#if PLATFORM_WINDOWS
			#define DISPLAY_CLUSTER_FUNC_TRACE(cat)  UE_LOG(cat, VeryVerbose, TEXT(">> %s"), TEXT(__FUNCTION__))
			//#define DISPLAY_CLUSTER_FUNC_TRACE(cat)  UE_LOG(cat, VeryVerbose, TEXT(">> %s::%s::%d"), TEXT(__FILE__), TEXT(__FUNCTION__), __LINE__)
	#else
		#define DISPLAY_CLUSTER_FUNC_TRACE(cat) ;
	#endif // PLATFORM_WINDOWS
#endif // UE_BUILD_SHIPPING
