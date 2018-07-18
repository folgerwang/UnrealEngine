// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "LaunchEngineLoop.h"
#include "UnixCommonStartup.h"

extern int32 GuardedMain( const TCHAR* CmdLine );

/**
 * Workaround function to avoid circular dependencies between Launch and CommonUnixStartup modules.
 *
 * Other platforms call FEngineLoop::AppExit() in their main() (removed by preprocessor if compiled without engine), but on Unix we want to share a common main() in CommonUnixStartup module,
 * so not just the engine but all the programs could share this logic. Unfortunately, AppExit() practice breaks this nice approach since FEngineLoop cannot be moved outside of Launch without
 * making too many changes. Hence CommonUnixMain will call it through this function if WITH_ENGINE is defined.
 *
 * If you change the prototype here, update CommonUnixMain() too!
 */
void LAUNCH_API LaunchUnix_FEngineLoop_AppExit()
{
	return FEngineLoop::AppExit();
}

int main(int argc, char *argv[])
{
	return CommonUnixMain(argc, argv, &GuardedMain);
}
