// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ClientPilotBlackboardManager.h"

UClientPilotBlackboardManager* UClientPilotBlackboardManager::ObjectInstance = nullptr;

UClientPilotBlackboardManager* UClientPilotBlackboardManager::GetInstance()
{
#if !UE_BUILD_SHIPPING
	if (ObjectInstance == nullptr)
	{
		ObjectInstance = NewObject<UClientPilotBlackboardManager>();
		ObjectInstance->AddToRoot();
	}
#endif
	return ObjectInstance;
}