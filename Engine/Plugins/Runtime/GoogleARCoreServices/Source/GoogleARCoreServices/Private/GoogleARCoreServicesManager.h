// Copyright 2018 Google Inc.

#pragma once

#include "CoreMinimal.h"
#include "ARSystem.h"

#include "GoogleARCoreServicesTypes.h"
#include "GoogleARCoreCloudARPinManager.h"

class FGoogleARCoreServicesManager
{

public:
	FGoogleARCoreServicesManager();

	~FGoogleARCoreServicesManager();

	bool ConfigGoogleARCoreServices(FGoogleARCoreServicesConfig& ServiceConfig);

	UCloudARPin* CreateAndHostCloudARPin(UARPin* ARPinToHost, EARPinCloudTaskResult& OutTaskResult);

	UCloudARPin* ResolveAncCreateCloudARPin(FString CloudId, EARPinCloudTaskResult& OutTaskResult);

	void RemoveCloudARPin(UCloudARPin* PinToRemove);

	TArray<UCloudARPin*> GetAllCloudARPin();

private:
	bool InitARSystem();

	EARPinCloudTaskResult CheckCloudTaskError();

	void OnARSessionStarted();
	void OnWorldTickStart(ELevelTick TickType, float DeltaTime);

	bool bHasValidARSystem;
	bool bCloudARPinEnabled;
	FGoogleARCoreServicesConfig CurrentServicesConfig;

	TSharedPtr<FARSystemBase, ESPMode::ThreadSafe> ARSystem;
	TUniquePtr<FGoogleARCoreCloudARPinManager> CloudARPinManager;
};

