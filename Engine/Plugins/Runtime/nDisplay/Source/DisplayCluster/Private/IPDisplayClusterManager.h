// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterOperationMode.h"


class ADisplayClusterGameMode;
class ADisplayClusterSettings;


/**
 * Private manager interface
 */
struct IPDisplayClusterManager
{
	virtual ~IPDisplayClusterManager() = 0
	{ }

	// Called at start to initialize internals
	virtual bool Init(EDisplayClusterOperationMode OperationMode)
	{ return true; }

	// Called before application/Editor exit to release internals
	virtual void Release()
	{ }

	// Called on each session start before first level start (before the first tick)
	virtual bool StartSession(const FString& configPath, const FString& nodeId)
	{ return true; }

	// Called on each session end at early step before exit (before UGameEngine::Preexit)
	virtual void EndSession()
	{ }

	// Called each time a new game level starts
	virtual bool StartScene(UWorld* pWorld)
	{ return true; }

	// Called when current level is going to be closed (i.e. when loading new map)
	virtual void EndScene()
	{ }

	// Called every frame before world Tick
	virtual void PreTick(float DeltaSeconds)
	{ }
};
