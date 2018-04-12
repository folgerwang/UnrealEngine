// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "ControlRigInterface.generated.h"

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class ENGINE_API UControlRigInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/** A producer of animated values */
class ENGINE_API IControlRigInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	/**
	 * Perform any per tick setup work
	 */
	virtual void PreEvaluate_GameThread() = 0;

	/**
	 * Perform any work that this control rig needs to do per-tick
	 */
	virtual void Evaluate_AnyThread() = 0;

	/**
	 * Perform any per tick finalization work
	 */
	virtual void PostEvaluate_GameThread() = 0;
};
