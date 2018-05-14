// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "UnrealPak.h"
#include "RequiredProgramMainCPPInclude.h"
#include "PakFileUtilities.h"

IMPLEMENT_APPLICATION(UnrealPak, "UnrealPak");

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	// start up the main loop
	GEngineLoop.PreInit(ArgC, ArgV);

	int32 Result = ExecuteUnrealPak(ArgC, ArgV);

	GLog->Flush();

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();

	return Result;
}
