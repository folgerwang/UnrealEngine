// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "EngineCustomTimeStep.generated.h"

UCLASS(abstract)
class ENGINE_API UEngineCustomTimeStep : public UObject
{
	GENERATED_BODY()

public:
	virtual bool Initialize(class UEngine* InEngine) PURE_VIRTUAL(UEngineCustomTimeStep::Initialize, return false;);
	virtual void Shutdown(class UEngine* InEngine) PURE_VIRTUAL(UEngineCustomTimeStep::Shutdown, );

	virtual void UpdateTimeStep(class UEngine* InEngine) PURE_VIRTUAL(UEngineCustomTimeStep::UpdateTimeStep, );
};

