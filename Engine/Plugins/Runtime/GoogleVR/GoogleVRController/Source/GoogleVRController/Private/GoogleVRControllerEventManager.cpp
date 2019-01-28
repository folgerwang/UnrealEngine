// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "GoogleVRControllerEventManager.h"
#include "GoogleVRController.h"
#include "GoogleVRControllerPrivate.h"

static UGoogleVRControllerEventManager* Singleton = nullptr;

UGoogleVRControllerEventManager::UGoogleVRControllerEventManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UGoogleVRControllerEventManager* UGoogleVRControllerEventManager::GetInstance()
{
	if (!Singleton)
	{
		Singleton = NewObject<UGoogleVRControllerEventManager>();
		Singleton->AddToRoot();
	}
	return Singleton;
}