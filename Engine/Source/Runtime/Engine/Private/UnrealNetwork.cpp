// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Net/UnrealNetwork.h"

FPreReplayScrub FNetworkReplayDelegates::OnPreScrub;
FOnWriteGameSpecificDemoHeader FNetworkReplayDelegates::OnWriteGameSpecificDemoHeader;
FOnProcessGameSpecificDemoHeader FNetworkReplayDelegates::OnProcessGameSpecificDemoHeader;

// ----------------------------------------------------------------
