// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterGameMode.h"

#include "Game/IPDisplayClusterGameManager.h"
#include "Input/IPDisplayClusterInputManager.h"

#include "Misc/DisplayClusterAppExit.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Misc/Paths.h"

#include "DisplayClusterPawn.h"
#include "DisplayClusterSettings.h"

#include "DisplayClusterStrings.h"
#include "DisplayClusterPlayerController.h"
#include "DisplayClusterHUD.h"
#include "DisplayClusterGlobals.h"
#include "IPDisplayCluster.h"


#if WITH_EDITOR
bool ADisplayClusterGameMode::bNeedSessionStart = true;
bool ADisplayClusterGameMode::bSessionStarted = false;
#endif


ADisplayClusterGameMode::ADisplayClusterGameMode() :
	Super()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	if (!bIsDisplayClusterActive)
	{
		return;
	}
	
	DefaultPawnClass = ADisplayClusterPawn::StaticClass();
	PlayerControllerClass = ADisplayClusterPlayerController::StaticClass();
	HUDClass = ADisplayClusterHUD::StaticClass();
}

ADisplayClusterGameMode::~ADisplayClusterGameMode()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);
}

void ADisplayClusterGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	Super::InitGame(MapName, Options, ErrorMessage);

	if (!GDisplayCluster->IsModuleInitialized())
	{
		return;
	}

	UE_LOG(LogDisplayClusterGame, Log, TEXT("%s"), bIsDisplayClusterActive ?
		TEXT("DisplayCluster feature is active for this world.") : 
		TEXT("DisplayCluster feature has been deactivated for this world by game mode."));

	if (!bIsDisplayClusterActive)
	{
		return;
	}

#if WITH_EDITOR
	if (GIsEditor && ADisplayClusterGameMode::bNeedSessionStart)
	{
		// Look for DisplayClusterSettings actor
		TArray<ADisplayClusterSettings*> Settings;
		DisplayClusterHelpers::game::FindAllActors(GetWorld(), Settings);

		FString NodeId;
		FString ConfigPath;

		// Extract user settings
		if (Settings.Num() > 0)
		{
			NodeId     = Settings[0]->EditorNodeId;
			ConfigPath = Settings[0]->EditorConfigPath;
		}
		else
		{ 
			UE_LOG(LogDisplayClusterGame, Warning, TEXT("No DisplayCluster settings found. Using defaults."));
			
			NodeId     = DisplayClusterStrings::misc::DbgStubNodeId;
			ConfigPath = DisplayClusterStrings::misc::DbgStubConfig;
		}

		DisplayClusterHelpers::str::DustCommandLineValue(ConfigPath);
		DisplayClusterHelpers::str::DustCommandLineValue(NodeId);

		// Check if config path is relative. In this case we have to build an absolute path from a project directory.
		if (FPaths::IsRelative(ConfigPath))
		{
			UE_LOG(LogDisplayClusterGame, Log, TEXT("Relative path detected. Generating absolute path..."));
			ConfigPath = FPaths::Combine(FPaths::ProjectDir(), ConfigPath);
			ConfigPath = FPaths::ConvertRelativePathToFull(ConfigPath);
			UE_LOG(LogDisplayClusterGame, Log, TEXT("Absolute path: %s"), *ConfigPath);
		}

		ADisplayClusterGameMode::bSessionStarted = GDisplayCluster->StartSession(ConfigPath, NodeId);
		if (!ADisplayClusterGameMode::bSessionStarted)
		{
			UE_LOG(LogDisplayClusterGame, Error, TEXT("Couldn't start DisplayCluster session"));
			FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::NormalSoft, FString("Couldn't start DisplayCluster session"));
		}

		// Subscribe to EndPIE event to close the DisplayCluster session
		EndPIEDelegate = FEditorDelegates::EndPIE.AddUObject(this, &ADisplayClusterGameMode::OnEndPIE);

		// Don't start DisplayCluster session again after LoadLevel
		ADisplayClusterGameMode::bNeedSessionStart = false;
	}
#endif
}

void ADisplayClusterGameMode::StartPlay()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	if (GDisplayCluster->IsModuleInitialized() && bIsDisplayClusterActive)
	{
		IPDisplayClusterGameManager* const pGameMgr = GDisplayCluster->GetPrivateGameMgr();
		if (pGameMgr)
		{
			// Set current DisplayClusterGameMode
			pGameMgr->SetDisplayClusterGameMode(this);

			TArray<ADisplayClusterSettings*> Settings;
			DisplayClusterHelpers::game::FindAllActors(GetWorld(), Settings);

			// Set current DisplayCluster scene settings
			if (Settings.Num())
			{
				UE_LOG(LogDisplayClusterGame, Log, TEXT("Found DisplayCluster settings for this level"));
				pGameMgr->SetDisplayClusterSceneSettings(Settings[0]);
			}
		}
	}

	Super::StartPlay();
}


void ADisplayClusterGameMode::BeginPlay()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	if (GDisplayCluster->IsModuleInitialized() && bIsDisplayClusterActive)
	{
		bGameStarted = GDisplayCluster->StartScene(GetWorld());
		if (!bGameStarted)
		{
			UE_LOG(LogDisplayClusterGame, Error, TEXT("Couldn't start game"));
			GetWorld()->Exec(GetWorld(), TEXT("quit"));
		}
	}

	Super::BeginPlay();
}

void ADisplayClusterGameMode::BeginDestroy()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	if (GDisplayCluster->IsModuleInitialized() && bIsDisplayClusterActive)
	{
		if (bGameStarted)
		{
			GDisplayCluster->EndScene();
		}

		// ...
	}

	Super::BeginDestroy();
}

void ADisplayClusterGameMode::Tick(float DeltaSeconds)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	Super::Tick(DeltaSeconds);

	if (!GDisplayCluster->IsModuleInitialized() || !bIsDisplayClusterActive)
	{
		return;
	}

#if WITH_EDITOR
	IPDisplayClusterInputManager* const pInputMgr = GDisplayCluster->GetPrivateInputMgr();
	if (pInputMgr)
	{
		pInputMgr->Update();
	}

	GDisplayCluster->PreTick(DeltaSeconds);
#endif
}

#if WITH_EDITOR
void ADisplayClusterGameMode::OnEndPIE(const bool bSimulate)
{
	if (GIsEditor)
	{
		FEditorDelegates::EndPIE.Remove(EndPIEDelegate);
		GDisplayCluster->EndSession();

		ADisplayClusterGameMode::bNeedSessionStart = true;
		ADisplayClusterGameMode::bSessionStarted = false;
	}
}
#endif
