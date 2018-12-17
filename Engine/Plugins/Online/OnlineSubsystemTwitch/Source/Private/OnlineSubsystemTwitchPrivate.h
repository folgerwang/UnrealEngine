// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "OnlineSubsystemTwitch.h"
#include "OnlineSubsystemTwitchModule.h"

/** pre-pended to all Twitch logging */
#undef ONLINE_LOG_PREFIX
#define ONLINE_LOG_PREFIX TEXT("TWITCH: ")


// from OnlineSubsystemTypes.h
TEMP_UNIQUENETIDSTRING_SUBCLASS(FUniqueNetIdTwitch, TWITCH_SUBSYSTEM);