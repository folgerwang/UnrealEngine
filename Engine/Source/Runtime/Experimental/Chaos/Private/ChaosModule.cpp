// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if INCLUDE_CHAOS

#include "ChaosModule.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "ChaosLog.h"

DEFINE_LOG_CATEGORY(LogChaosGeneral);
DEFINE_LOG_CATEGORY(LogChaosThread);
DEFINE_LOG_CATEGORY(LogChaosSimulation);
DEFINE_LOG_CATEGORY(LogChaosDebug);

IMPLEMENT_MODULE(FChaosEngineModule, Chaos);

#endif
