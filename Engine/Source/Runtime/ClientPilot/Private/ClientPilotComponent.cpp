// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ClientPilotComponent.h"
#include "Modules/ModuleManager.h"


UClientPilotComponent::UClientPilotComponent()
{
}

UClientPilotBlackboard* UClientPilotComponent::GetBlackboardInstance()
{
	return UClientPilotBlackboardManager::GetInstance() ? UClientPilotBlackboardManager::GetInstance()->PilotBlackboard : nullptr;
}

void UClientPilotComponent::ThinkAndAct() 
{
}

IMPLEMENT_MODULE(FDefaultModuleImpl, ClientPilot);