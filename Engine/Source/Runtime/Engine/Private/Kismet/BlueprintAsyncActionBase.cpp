// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

//////////////////////////////////////////////////////////////////////////
// UBlueprintAsyncActionBase

UBlueprintAsyncActionBase::UBlueprintAsyncActionBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		SetFlags(RF_StrongRefOnFrame);
	}
}

void UBlueprintAsyncActionBase::Activate()
{
}

void UBlueprintAsyncActionBase::RegisterWithGameInstance(UObject* WorldContextObject)
{
	UWorld* FoundWorld = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	if (FoundWorld && FoundWorld->GetGameInstance())
	{
		RegisterWithGameInstance(FoundWorld->GetGameInstance());
	}
}

void UBlueprintAsyncActionBase::RegisterWithGameInstance(UGameInstance* GameInstance)
{
	if (GameInstance)
	{
		UGameInstance* OldGameInstance = RegisteredWithGameInstance.Get();
		if (OldGameInstance)
		{
			OldGameInstance->UnregisterReferencedObject(this);
		}

		GameInstance->RegisterReferencedObject(this);
		RegisteredWithGameInstance = GameInstance;
	}
}

void UBlueprintAsyncActionBase::SetReadyToDestroy()
{
	ClearFlags(RF_StrongRefOnFrame);

	UGameInstance* OldGameInstance = RegisteredWithGameInstance.Get();
	if (OldGameInstance)
	{
		OldGameInstance->UnregisterReferencedObject(this);
	}
}
