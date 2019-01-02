// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/UniquePtr.h"

struct FBlueprintDebugger
{
	// Initializes the global state of the debugger (commands, tab spawners, etc):
	FBlueprintDebugger();

	// Destructor declaration purely so that we can pimpl:
	~FBlueprintDebugger();

private:
	TUniquePtr< struct FBlueprintDebuggerImpl > Impl;

	// prevent copying:
	FBlueprintDebugger(const FBlueprintDebugger&);
	FBlueprintDebugger(FBlueprintDebugger&&);
	FBlueprintDebugger& operator=(FBlueprintDebugger const&);
	FBlueprintDebugger& operator=(FBlueprintDebugger&&);
};

