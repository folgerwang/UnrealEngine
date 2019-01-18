// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "UnrealMultiUserServer.h"
#include "RequiredProgramMainCPPInclude.h"

#include "IConcertModule.h"
#include "IConcertServer.h"
#include "ConcertSettings.h"

#define IDEAL_FRAMERATE 60;

IMPLEMENT_APPLICATION(UnrealMultiUserServer, "UnrealMultiUserServer");

DEFINE_LOG_CATEGORY(LogMultiUserServer);

/**
 * Application entry point
 *
 * @param	ArgC	Command-line argument count
 * @param	ArgV	Argument strings
 */
INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	// start up the main loop, adding some extra command line arguments:
	//	-Messaging enables MessageBus transports
	//	-stdout prevents double display logging
	int32 Result = GEngineLoop.PreInit(ArgC, ArgV, TEXT(" -Messaging"));
	//TODO: handle Result error? 

	GLogConsole->Show(true);

	// TODO: need config? trim engine loop?
	check(GConfig && GConfig->IsReadyForUse());

	FModuleManager::Get().StartProcessingNewlyLoadedObjects();

	// Load internal Concert plugins in the pre-default phase
	IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PreDefault);

	// Load Concert Sync plugins in default phase
	IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::Default);

	// Install graceful termination handler, this handles graceful CTRL+C shutdown, 
	// but not CTRL+CLOSE, which will potentially still exit process before the main thread exits.
	// Double CTRL+C signal will also cause process to terminate.
	FPlatformMisc::SetGracefulTerminationHandler();

	// Get the server settings
	UConcertServerConfig* ServerConfig = IConcertModule::Get().ParseServerSettings(FCommandLine::Get());


	// Setup Concert to run in server mode.
	IConcertServerPtr ConcertServer = IConcertModule::Get().GetServerInstance();
	ConcertServer->Configure(ServerConfig);
	ConcertServer->Startup();

	// if we have a default session, set it up properly
	if (!ServerConfig->DefaultSessionName.IsEmpty())
	{
		if (!ConcertServer->GetSession(FName(*ServerConfig->DefaultSessionName)).IsValid())
		{
			FConcertSessionInfo SessionInfo = ConcertServer->CreateSessionInfo();
			SessionInfo.SessionName = ServerConfig->DefaultSessionName;
			SessionInfo.Settings = ServerConfig->DefaultSessionSettings;
			ConcertServer->CreateSession(SessionInfo);
		}
	}

	UE_LOG(LogMultiUserServer, Display, TEXT("Multi-User Editing Server Initialized"));

	double DeltaTime = 0.0;
	double LastTime = FPlatformTime::Seconds();
	const float IdealFrameTime = 1.0f / IDEAL_FRAMERATE;

	while (!GIsRequestingExit)
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);

		// Pump & Tick objects
		FTicker::GetCoreTicker().Tick(DeltaTime);

		GFrameCounter++;
		FStats::AdvanceFrame(false);
		GLog->FlushThreadedLogs();

		// Run garbage collection for the uobjects for the rest of the frame or at least to 2 ms
		IncrementalPurgeGarbage(true, FMath::Max<float>(0.002f, IdealFrameTime - (FPlatformTime::Seconds() - LastTime)));

		// Throttle main thread main fps by sleeping if we still have time
		FPlatformProcess::Sleep(FMath::Max<float>(0.0f, IdealFrameTime - (FPlatformTime::Seconds() - LastTime)));

		double CurrentTime = FPlatformTime::Seconds();
		DeltaTime = CurrentTime - LastTime;
		LastTime = CurrentTime;
	}

	ConcertServer->Shutdown();

	UE_LOG(LogMultiUserServer, Display, TEXT("Multi-User Editing Server Shutdown"));

	// Allow the game thread to finish processing any latent tasks.
	// They will be relying on what we are going to shutdown...
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);

	FEngineLoop::AppPreExit();

	// Unloading Modules isn't handled by AppExit
	FModuleManager::Get().UnloadModulesAtShutdown();
	// Nor is it handling stats, if any
#if STATS
	FThreadStats::StopThread();
#endif

	FEngineLoop::AppExit();
	return Result;
}