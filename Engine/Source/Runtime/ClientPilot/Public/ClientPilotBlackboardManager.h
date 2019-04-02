// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ClientPilotBlackboard.h"
#include "ClientPilotBlackboardManager.generated.h"


UCLASS()
class CLIENTPILOT_API UClientPilotBlackboardManager : public UObject
{
GENERATED_BODY()
protected:
	static UClientPilotBlackboardManager * ObjectInstance;

public:
	static UClientPilotBlackboardManager * GetInstance();
	UPROPERTY()
	UClientPilotBlackboard* PilotBlackboard;
};