// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "UnrealPak.h"
#include "RequiredProgramMainCPPInclude.h"
#include "PakFileUtilities.h"
#include "IPlatformFilePak.h"

IMPLEMENT_APPLICATION(UnrealPak, "UnrealPak");

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	// start up the main loop
	GEngineLoop.PreInit(ArgC, ArgV);

	double StartTime = FPlatformTime::Seconds();

	int32 Result = ExecuteUnrealPak(FCommandLine::Get())? 0 : 1;

	UE_LOG(LogPakFile, Display, TEXT("Unreal pak executed in %f seconds"), FPlatformTime::Seconds() - StartTime );

	GLog->Flush();

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();

	return Result;
}
