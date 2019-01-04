// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ClientPilotBlackboard.generated.h"

UCLASS()
class CLIENTPILOT_API UClientPilotBlackboard : public UObject
{
GENERATED_BODY()
public:
	virtual void InitializeFromProfile(FString CategoryToUse);

	void AddOrUpdateValue(FString KeyName, float Value);
	void AddOrUpdateValue(FString KeyName, int Value);
	void AddOrUpdateValue(FString KeyName, FString Value);
	void AddOrUpdateValue(FString KeyName, FVector Value);

	FString GetStringValue(FString KeyName);
	int GetIntValue(FString KeyName);
	float GetFloatValue(FString KeyName);
	FVector GetVectorValue(FString KeyName);

	void RemoveKey(FString Key) 
	{ 
		Blackboard.Remove(Key);
	}
	void ResetBlackboard()
	{
		Blackboard.Empty();
	}

protected:
	TMap<FString, FString> Blackboard;
};