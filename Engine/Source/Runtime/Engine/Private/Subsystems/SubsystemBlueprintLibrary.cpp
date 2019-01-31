// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Subsystems/SubsystemBlueprintLibrary.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"

/*static*/  UEngineSubsystem* USubsystemBlueprintLibrary::GetEngineSubsystem(TSubclassOf<UEngineSubsystem> Class)
{
	return GEngine->GetEngineSubsystemBase(Class);
}

/*static*/ UGameInstanceSubsystem* USubsystemBlueprintLibrary::GetGameInstanceSubsystem(UObject* ContextObject, TSubclassOf<UGameInstanceSubsystem> Class)
{
	if (const UWorld* World = ThisClass::GetWorldFrom(ContextObject))
	{
		if (const UGameInstance* GameInstance = GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystemBase(Class);
		}
	}
	return nullptr;
}

/*static*/ ULocalPlayerSubsystem* USubsystemBlueprintLibrary::GetLocalPlayerSubsystem(UObject* ContextObject, TSubclassOf<ULocalPlayerSubsystem> Class)
{
	const ULocalPlayer* LocalPlayer = nullptr;

	if (const UUserWidget* UserWidget = Cast<UUserWidget>(ContextObject))
	{
		LocalPlayer = UserWidget->GetOwningLocalPlayer();
	}
	else if (APlayerController* PlayerController = Cast<APlayerController>(ContextObject))
	{
		LocalPlayer = Cast<ULocalPlayer>(PlayerController->Player);
	}
	else
	{
		LocalPlayer = Cast<ULocalPlayer>(ContextObject);
	}

	if (LocalPlayer != nullptr)
	{
		return LocalPlayer->GetSubsystemBase(Class);
	}

	return nullptr;
}

/*static*/ ULocalPlayerSubsystem* USubsystemBlueprintLibrary::GetLocalPlayerSubSystemFromPlayerController(APlayerController* PlayerController, TSubclassOf<ULocalPlayerSubsystem> Class)
{
	if (PlayerController)
	{
		if (const ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(PlayerController->Player))
		{
			return LocalPlayer->GetSubsystemBase(Class);
		}
	}
	return nullptr;
}

/*static*/ UWorld* USubsystemBlueprintLibrary::GetWorldFrom(UObject* ContextObject)
{
	if (ContextObject)
	{
		return GEngine->GetWorldFromContextObject(ContextObject, EGetWorldErrorMode::ReturnNull);
	}
	return nullptr;
}