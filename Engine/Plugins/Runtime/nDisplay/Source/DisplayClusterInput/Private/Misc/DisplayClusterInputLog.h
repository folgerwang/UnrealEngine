// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/Public/Logging/LogMacros.h"

// Plugin-wide log categories
#if UE_BUILD_SHIPPING
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterInputModule,   Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterInputBP,       Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterInputAnalog,   Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterInputButton,   Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterInputKeyboard, Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterInputTracker,  Warning, Warning);
#else
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterInputModule,   Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterInputBP,       Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterInputAnalog,   Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterInputButton,   Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterInputKeyboard, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterInputTracker,  Log, All);
#endif


//@todo: Linux@GCC will probably require other macro
#if UE_BUILD_SHIPPING
	#define DISPLAY_CLUSTER_FUNC_TRACE(cat) ;
#else
	#if PLATFORM_WINDOWS
			#define DISPLAY_CLUSTER_FUNC_TRACE(cat)  UE_LOG(cat, VeryVerbose, TEXT(">> %s"), TEXT(__FUNCTION__))
	#else
		#define DISPLAY_CLUSTER_FUNC_TRACE(cat) ;
	#endif // PLATFORM_WINDOWS
#endif // UE_BUILD_SHIPPING
