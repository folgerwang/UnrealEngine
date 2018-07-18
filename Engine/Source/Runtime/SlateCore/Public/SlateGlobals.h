// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"

#define SLATE_CHECK_UOBJECT_RENDER_RESOURCES !UE_BUILD_SHIPPING

#ifndef SLATE_PARENT_POINTERS
	#define SLATE_PARENT_POINTERS 0
#endif

#ifndef SLATE_CULL_WIDGETS
	#define SLATE_CULL_WIDGETS 1
#endif

/* Globals
 *****************************************************************************/

 // Compile all the RichText and MultiLine editable text?
#define WITH_FANCY_TEXT 1

 // If you want to get really verbose stats out of Slate to get a really in-depth
 // view of what widgets are causing you the greatest problems, set this define to 1.
#ifndef WITH_VERY_VERBOSE_SLATE_STATS
	#define WITH_VERY_VERBOSE_SLATE_STATS 0
#endif

#ifndef SLATE_VERBOSE_NAMED_EVENTS
	#define SLATE_VERBOSE_NAMED_EVENTS !UE_BUILD_SHIPPING
#endif

// HOW TO GET AN IN-DEPTH PERFORMANCE ANALYSIS OF SLATE
//
// Step 1)
//    Set WITH_VERY_VERBOSE_SLATE_STATS to 1.
//
// Step 2)
//    When running the game (outside of the editor), run these commandline options
//    in order and you'll get a large dump of where all the time is going in Slate.
//    
//    stat group enable slateverbose
//    stat group enable slateveryverbose
//    stat dumpave -root=stat_slate -num=120 -ms=0

SLATECORE_API DECLARE_LOG_CATEGORY_EXTERN(LogSlate, Log, All);
SLATECORE_API DECLARE_LOG_CATEGORY_EXTERN(LogSlateStyles, Log, All);

DECLARE_STATS_GROUP(TEXT("Slate Memory"), STATGROUP_SlateMemory, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Slate"), STATGROUP_Slate, STATCAT_Advanced);
DECLARE_STATS_GROUP_VERBOSE(TEXT("SlateVerbose"), STATGROUP_SlateVerbose, STATCAT_Advanced);
DECLARE_STATS_GROUP_MAYBE_COMPILED_OUT(TEXT("SlateVeryVerbose"), STATGROUP_SlateVeryVerbose, STATCAT_Advanced, WITH_VERY_VERBOSE_SLATE_STATS);

/** Whether or not dynamic prepass and layout caching is enabled */
extern SLATECORE_API int32 GSlateLayoutCaching;

/** Whether or not we've enabled fast widget pathing which validates paths to widgets without arranging children. */
extern SLATECORE_API int32 GSlateFastWidgetPath;

/* Forward declarations
*****************************************************************************/
class FActiveTimerHandle;
enum class EActiveTimerReturnType : uint8;
