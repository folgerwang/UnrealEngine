// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/IConsoleManager.h"
#include "Stats/Stats.h"

DECLARE_STATS_GROUP(TEXT("Chaos"), STATGROUP_Chaos, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("ChaosWide"), STATGROUP_ChaosWide, STATCAT_Advanced);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Physics Advance"), STAT_PhysicsAdvance, STATGROUP_Chaos, CHAOS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Solver Advance"), STAT_SolverAdvance, STATGROUP_Chaos, CHAOS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Handle Solver Commands"), STAT_HandleSolverCommands, STATGROUP_Chaos, CHAOS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Integrate Solver"), STAT_IntegrateSolver, STATGROUP_Chaos, CHAOS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Sync Physics Proxies"), STAT_SyncProxies, STATGROUP_Chaos, CHAOS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Handle Physics Commands"), STAT_PhysCommands, STATGROUP_Chaos, CHAOS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Handle Task Commands"), STAT_TaskCommands, STATGROUP_Chaos, CHAOS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Kinematic Particle Update"), STAT_KinematicUpdate, STATGROUP_Chaos, CHAOS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Begin Frame"), STAT_BeginFrame, STATGROUP_Chaos, CHAOS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("End Frame"), STAT_EndFrame, STATGROUP_Chaos, CHAOS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Geometry Collection Raycast"), STAT_GCRaycast, STATGROUP_Chaos, CHAOS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Geometry Collection UpdateBounds"), STAT_GCUpdateBounds, STATGROUP_Chaos, CHAOS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Physics Lock Waits"), STAT_LockWaits, STATGROUP_Chaos, CHAOS_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Geometry Collection Begin Frame"), STAT_GeomBeginFrame, STATGROUP_Chaos, CHAOS_API);