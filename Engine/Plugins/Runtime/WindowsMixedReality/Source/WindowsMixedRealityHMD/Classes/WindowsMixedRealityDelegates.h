// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "IHeadMountedDisplay.h"

#include "WindowsMixedRealityDelegates.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWindowsMRUserPresenceChanged, EHMDWornState::Type, currentUserPresence);

UCLASS()
class AWindowsMixedRealityDelegates : public AActor
{
private:
	static AWindowsMixedRealityDelegates* instance;

public:
	GENERATED_BODY()

	AWindowsMixedRealityDelegates(const FObjectInitializer& ObjectInitializer)
	{
		instance = this;
	}

	static AWindowsMixedRealityDelegates* GetInstance()
	{
		return instance;
	}

	UPROPERTY(BlueprintAssignable, Category = "WindowsMixedRealityHMD")
	FWindowsMRUserPresenceChanged OnUserPresenceChanged;
};
