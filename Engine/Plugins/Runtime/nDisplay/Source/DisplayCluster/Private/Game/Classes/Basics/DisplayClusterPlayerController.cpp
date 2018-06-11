// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterPlayerController.h"
#include "Misc/DisplayClusterAppExit.h"


void ADisplayClusterPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (WasInputKeyJustPressed(EKeys::Escape))
	{
		FDisplayClusterAppExit::ExitApplication(FDisplayClusterAppExit::ExitType::NormalSoft, FString("Exit on ESC requested"));
	}
}

void ADisplayClusterPlayerController::BeginPlay()
{
	Super::BeginPlay();

#if 0
	//@todo: temporary solution. we need generic DisplayCluster access to statistics
	//@note: next line causes crash
	FDisplayClusterConfigDebug cfgDebug = FGDisplayCluster->GetPrivateConfigMgr()->GetConfigDebug();
	if (cfgDebug.DrawStats)
	{
		UE_LOG(LogDisplayClusterGame, Log, TEXT("Activating onscreen stats"));
		ConsoleCommand(FString("stat fps"),  true);
		ConsoleCommand(FString("stat unit"), true);
	}
#endif
}

