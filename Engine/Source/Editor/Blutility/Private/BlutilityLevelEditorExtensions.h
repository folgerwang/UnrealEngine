// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Integrate Blutility actions associated with level editor functionality (e.g. Actor editing)
class FBlutilityLevelEditorExtensions
{
public:
	static void InstallHooks();
	static void RemoveHooks();
};
