// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "Chaos/DebugDrawQueue.h"
#include "HAL/IConsoleManager.h"

#if CHAOS_DEBUG_DRAW
using namespace Chaos;

int32 FDebugDrawQueue::EnableDebugDrawing = 0;
FAutoConsoleVariableRef CVarEnableDebugDrawingChaos(TEXT("p.ChaosDebugDrawing"), FDebugDrawQueue::EnableDebugDrawing, TEXT("Whether to debug draw low level physics solver information"));
#endif
