// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

// Central location to define all real time signal in use

// Used in UnixPlatformProcess.cpp for forking
#define WAIT_AND_FORK_QUEUE_SIGNAL SIGRTMIN + 1
#define WAIT_AND_FORK_RESPONSE_SIGNAL SIGRTMIN + 2

// Used in UnixSignalGameHitchHeartBeat.cpp for a hitch signal handler
#define HEART_BEAT_SIGNAL SIGRTMIN + 3
