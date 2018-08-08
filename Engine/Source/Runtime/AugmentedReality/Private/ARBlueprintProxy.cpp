// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ARBlueprintProxy.h"
#include "ARSystem.h"

TSharedPtr<FARSystemBase, ESPMode::ThreadSafe> UARBaseAsyncTaskBlueprintProxy::RegisteredARSystem = nullptr;

UARBaseAsyncTaskBlueprintProxy::UARBaseAsyncTaskBlueprintProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bShouldTick(true)
{
}

void UARBaseAsyncTaskBlueprintProxy::Tick(float DeltaTime)
{
	if (!AsyncTask.IsValid())
	{
		bShouldTick = false;
		ReportFailure();
		return;
	}

	if (AsyncTask->IsDone())
	{
		bShouldTick = false;
		// Fire the right delegate
		if (!AsyncTask->HadError())
		{
			ReportSuccess();
		}
		else
		{
			ReportFailure();
		}
	}
}

void UARBaseAsyncTaskBlueprintProxy::RegisterAsARSystem(const TSharedPtr<FARSystemBase, ESPMode::ThreadSafe>& NewARSystem)
{
	RegisteredARSystem = NewARSystem;
}


const TSharedPtr<FARSystemBase, ESPMode::ThreadSafe>& UARBaseAsyncTaskBlueprintProxy::GetARSystem()
{
	return RegisteredARSystem;
}

UARSaveWorldAsyncTaskBlueprintProxy* UARSaveWorldAsyncTaskBlueprintProxy::ARSaveWorld(UObject* WorldContextObject)
{
	UARSaveWorldAsyncTaskBlueprintProxy* Proxy = NewObject<UARSaveWorldAsyncTaskBlueprintProxy>();
	Proxy->RegisterWithGameInstance(WorldContextObject);
	return Proxy;
}

void UARSaveWorldAsyncTaskBlueprintProxy::Activate()
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		SaveWorldTask = ARSystem->SaveWorld();
		AsyncTask = SaveWorldTask;
	}
	else
	{
		AsyncTask = MakeShared<FARErrorSaveWorldAsyncTask, ESPMode::ThreadSafe>(TEXT("ARSaveWorld - requires a valid, running session"));
	}
}

void UARSaveWorldAsyncTaskBlueprintProxy::ReportSuccess()
{
	OnSuccess.Broadcast(SaveWorldTask->GetSavedWorldData());
}

void UARSaveWorldAsyncTaskBlueprintProxy::ReportFailure()
{
	OnFailed.Broadcast(TArray<uint8>());
}

UARGetCandidateObjectAsyncTaskBlueprintProxy* UARGetCandidateObjectAsyncTaskBlueprintProxy::ARGetCandidateObject(UObject* WorldContextObject, FVector Location, FVector Extent)
{
	UARGetCandidateObjectAsyncTaskBlueprintProxy* Proxy = NewObject<UARGetCandidateObjectAsyncTaskBlueprintProxy>();
	Proxy->RegisterWithGameInstance(WorldContextObject);
	Proxy->Extent = Extent;
	Proxy->Location = Location;

	return Proxy;
}

void UARGetCandidateObjectAsyncTaskBlueprintProxy::Activate()
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		CandidateObjectTask = ARSystem->GetCandidateObject(Location, Extent);
		AsyncTask = CandidateObjectTask;
	}
	else
	{
		AsyncTask = MakeShared<FARErrorGetCandidateObjectAsyncTask, ESPMode::ThreadSafe>(TEXT("ARGetCandidateObject - requires a valid, running session"));
	}
}

void UARGetCandidateObjectAsyncTaskBlueprintProxy::ReportSuccess()
{
	OnSuccess.Broadcast(CandidateObjectTask->GetCandidateObject());
}

void UARGetCandidateObjectAsyncTaskBlueprintProxy::ReportFailure()
{
	OnFailed.Broadcast(nullptr);
}
