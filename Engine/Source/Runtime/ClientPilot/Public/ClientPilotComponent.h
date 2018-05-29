// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ClientPilotBlackboardManager.h"
#include "ClientPilotComponent.generated.h"

UCLASS()
class CLIENTPILOT_API UClientPilotComponent : public UObject
{
	GENERATED_BODY()
public:
	UClientPilotComponent();
	UClientPilotBlackboard* GetBlackboardInstance();
	virtual void ThinkAndAct();

};