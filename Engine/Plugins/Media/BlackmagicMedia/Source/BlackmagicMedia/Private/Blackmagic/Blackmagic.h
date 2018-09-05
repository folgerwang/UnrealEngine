// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

class FBlackmagic
{
public:
	static bool Initialize();
	static bool IsInitialized();
	static void Shutdown();

private:
	static void LogInfo(const TCHAR* InFormat, ...);
	static void LogWarning(const TCHAR* InFormat, ...);
	static void LogError(const TCHAR* InFormat, ...);

private:
	static void* LibHandle;
};
