// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GauntletTestController.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"


UGauntletTestController::UGauntletTestController(const FObjectInitializer& ObjectInitializer)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		ParentModule = &FModuleManager::Get().GetModuleChecked<FGauntletModule>(TEXT("Gauntlet"));
	}
}

UGauntletTestController::~UGauntletTestController()
{

}

FName UGauntletTestController::GetCurrentState() const
{
	check(ParentModule);
	return ParentModule->GetCurrentState();
}

double UGauntletTestController::GetTimeInCurrentState() const
{
	check(ParentModule);
	return ParentModule->GetTimeInCurrentState();
}

FString UGauntletTestController::GetCurrentMap() const
{
	return GWorld->GetName();
}

/**
* Returns the gauntlet module running this test
*/
FGauntletModule* UGauntletTestController::GetGauntlet()
{
	return ParentModule;
}

UWorld* UGauntletTestController::GetWorld() const
{
	UGameInstance* GameInstance = GEngine->GameViewport->GetGameInstance();

	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

APlayerController* UGauntletTestController::GetFirstPlayerController() const
{
	return GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;	
}

void UGauntletTestController::EndTest(int32 ExitCode /*= 0*/)
{
	UE_LOG(LogGauntlet, Display, TEXT("**** TEST COMPLETE. EXIT CODE: %d ****"), ExitCode);
	// we flush logs because we don't (currently...) want to treat shutdown errors as failures
	GLog->PanicFlushThreadedLogs();
	FPlatformMisc::RequestExit(1);
}
