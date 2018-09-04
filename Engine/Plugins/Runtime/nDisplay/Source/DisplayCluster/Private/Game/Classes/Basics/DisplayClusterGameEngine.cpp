// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterGameEngine.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Cluster/Controller/IPDisplayClusterNodeController.h"
#include "Config/IPDisplayClusterConfigManager.h"
#include "Input/IPDisplayClusterInputManager.h"

#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/DisplayClusterAppExit.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/Parse.h"
#include "DisplayClusterBuildConfig.h"
#include "DisplayClusterGlobals.h"
#include "IPDisplayCluster.h"


void UDisplayClusterGameEngine::Init(class IEngineLoop* InEngineLoop)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterEngine);

	// Detect requested operation mode
	OperationMode = DetectOperationMode();

	// Initialize Display Cluster
	if (!GDisplayCluster->Init(OperationMode))
	{
		FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::KillImmediately, FString("Couldn't initialize DisplayCluster module"));
	}

	FString cfgPath;
	FString nodeId;

	if (OperationMode == EDisplayClusterOperationMode::Cluster)
	{
		// Extract config path from command line
		if (!FParse::Value(FCommandLine::Get(), DisplayClusterStrings::args::Config, cfgPath))
		{
			UE_LOG(LogDisplayClusterEngine, Error, TEXT("No config file specified"));
			FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::KillImmediately, FString("Cluster mode requires config file"));
		}

		// Extract node ID from command line
		if (!FParse::Value(FCommandLine::Get(), DisplayClusterStrings::args::Node, nodeId))
		{
#ifdef DISPLAY_CLUSTER_USE_AUTOMATIC_NODE_ID_RESOLVE
			UE_LOG(LogDisplayClusterEngine, Log, TEXT("Node ID is not specified"));
#else
			UE_LOG(LogDisplayClusterEngine, Warning, TEXT("Node ID is not specified"));
			FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::KillImmediately, FString("Cluster mode requires node ID"));
#endif
		}
	}
	else if (OperationMode == EDisplayClusterOperationMode::Standalone)
	{
#ifdef DISPLAY_CLUSTER_USE_DEBUG_STANDALONE_CONFIG
		// Save config path from command line
		cfgPath = DisplayClusterStrings::misc::DbgStubConfig;
		nodeId  = DisplayClusterStrings::misc::DbgStubNodeId;
#endif
	}

	if (OperationMode == EDisplayClusterOperationMode::Cluster ||
		OperationMode == EDisplayClusterOperationMode::Standalone)
	{
		DisplayClusterHelpers::str::DustCommandLineValue(cfgPath);
		DisplayClusterHelpers::str::DustCommandLineValue(nodeId);

		// Start game session
		if (!GDisplayCluster->StartSession(cfgPath, nodeId))
		{
			FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::KillImmediately, FString("Couldn't start DisplayCluster session"));
		}

		// Initialize internals
		InitializeInternals();
	}

	// Initialize base stuff.
	UGameEngine::Init(InEngineLoop);
}

EDisplayClusterOperationMode UDisplayClusterGameEngine::DetectOperationMode()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterEngine);

	EDisplayClusterOperationMode OpMode = EDisplayClusterOperationMode::Disabled;
	if (FParse::Param(FCommandLine::Get(), DisplayClusterStrings::args::Cluster))
	{
		OpMode = EDisplayClusterOperationMode::Cluster;
	}
	else if (FParse::Param(FCommandLine::Get(), DisplayClusterStrings::args::Standalone))
	{
		OpMode = EDisplayClusterOperationMode::Standalone;
	}

	UE_LOG(LogDisplayClusterEngine, Log, TEXT("Detected operation mode: %s"), *FDisplayClusterTypesConverter::ToString(OpMode));

	return OpMode;
}

bool UDisplayClusterGameEngine::InitializeInternals()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterEngine);

	// Store debug settings locally
	CfgDebug = GDisplayCluster->GetPrivateConfigMgr()->GetConfigDebug();
	
	InputMgr       = GDisplayCluster->GetPrivateInputMgr();
	ClusterMgr     = GDisplayCluster->GetPrivateClusterMgr();
	NodeController = ClusterMgr->GetController();

	FDisplayClusterConfigClusterNode nodeCfg;
	if (GDisplayCluster->GetPrivateConfigMgr()->GetLocalClusterNode(nodeCfg))
	{
		UE_LOG(LogDisplayClusterEngine, Log, TEXT("Configuring sound enabled: %s"), *FDisplayClusterTypesConverter::ToString(nodeCfg.SoundEnabled));
		bUseSound = nodeCfg.SoundEnabled;
	}

	check(ClusterMgr);
	check(InputMgr);
		
	return true;
}

void UDisplayClusterGameEngine::PreExit()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterEngine);

	if (OperationMode == EDisplayClusterOperationMode::Cluster ||
		OperationMode == EDisplayClusterOperationMode::Standalone)
	{
		// Close current DisplayCluster session
		GDisplayCluster->EndSession();
	}

	// Release the engine
	UGameEngine::PreExit();
}

bool UDisplayClusterGameEngine::LoadMap(FWorldContext& WorldContext, FURL URL, class UPendingNetGame* Pending, FString& Error)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterEngine);

	// Perform map loading
	if (!Super::LoadMap(WorldContext, URL, Pending, Error))
	{
		return false;
	}

	if (OperationMode == EDisplayClusterOperationMode::Cluster ||
		OperationMode == EDisplayClusterOperationMode::Standalone)
	{
		// Game start barrier
		NodeController->WaitForGameStart();
	}

	return true;
}

void UDisplayClusterGameEngine::Tick(float DeltaSeconds, bool bIdleMode)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterEngine);

	if (OperationMode == EDisplayClusterOperationMode::Cluster ||
		OperationMode == EDisplayClusterOperationMode::Standalone)
	{
		FTimecode Timecode;
		FFrameRate FrameRate;

		// Update input device state (master only)
		InputMgr->Update();

		// Update delta time. Cluster slaves will get this value from the master few steps later
		ClusterMgr->SetDeltaTime(DeltaSeconds);

		// Sync cluster objects
		ClusterMgr->SyncObjects();

		//////////////////////////////////////////////////////////////////////////////////////////////
		// Frame start barrier
		NodeController->WaitForFrameStart();
		UE_LOG(LogDisplayClusterEngine, Verbose, TEXT("Sync frame start"));

		// Get DisplayCluster time delta
		NodeController->GetDeltaTime(DeltaSeconds);
		NodeController->GetTimecode(Timecode, FrameRate);
		UE_LOG(LogDisplayClusterEngine, Verbose, TEXT("DisplayCluster delta time (seconds): %f"), DeltaSeconds);
		UE_LOG(LogDisplayClusterEngine, Verbose, TEXT("DisplayCluster Timecode: %s | %s"), *Timecode.ToString(), *FrameRate.ToPrettyText().ToString());

		// Update delta time in the application
		FApp::SetDeltaTime(DeltaSeconds);
		FApp::SetTimecodeAndFrameRate(Timecode, FrameRate);

		// Update input state in the cluster
		ClusterMgr->SyncInput();

		// Perform PreTick for DisplayCluster module
		UE_LOG(LogDisplayClusterEngine, Verbose, TEXT("Perform PreTick()"));
		GDisplayCluster->PreTick(DeltaSeconds);

		// Perform Tick() calls for scene actors
		UE_LOG(LogDisplayClusterEngine, Verbose, TEXT("Perform Tick()"));
		Super::Tick(DeltaSeconds, bIdleMode);

		if (CfgDebug.LagSimulateEnabled)
		{
			const float lag = CfgDebug.LagMaxTime;
			UE_LOG(LogDisplayClusterEngine, Log, TEXT("Simulating lag: %f seconds"), lag);
#if 1
			FPlatformProcess::Sleep(FMath::RandRange(0.f, lag));
#else
			FPlatformProcess::Sleep(lag);
#endif
		}

#if 0
		//////////////////////////////////////////////////////////////////////////////////////////////
		// Tick end barrier
		NodeController->WaitForTickEnd();
#endif

		//////////////////////////////////////////////////////////////////////////////////////////////
		// Frame end barrier
		NodeController->WaitForFrameEnd();
		UE_LOG(LogDisplayClusterEngine, Verbose, TEXT("Sync frame end"));
	}
	else
	{
		Super::Tick(DeltaSeconds, bIdleMode);
	}
}

