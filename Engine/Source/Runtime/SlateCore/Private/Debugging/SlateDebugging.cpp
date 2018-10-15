// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Debugging/SlateDebugging.h"
#include "SlateGlobals.h"

#if WITH_SLATE_DEBUGGING

FSlateDebugging::FBeginWindow FSlateDebugging::BeginWindow;

FSlateDebugging::FEndWindow FSlateDebugging::EndWindow;

FSlateDebugging::FBeginWidgetPaint FSlateDebugging::BeginWidgetPaint;

FSlateDebugging::FEndWidgetPaint FSlateDebugging::EndWidgetPaint;

FSlateDebugging::FDrawElement FSlateDebugging::ElementAdded;

#endif