// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Integrate Blutility actions associated with existing engine types (e.g., Texture2D) into the content browser
class FBlutilityContentBrowserExtensions
{
public:
	static void InstallHooks();
	static void RemoveHooks();
};
