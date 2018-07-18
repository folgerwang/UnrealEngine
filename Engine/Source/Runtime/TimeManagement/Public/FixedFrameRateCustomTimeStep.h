// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineCustomTimeStep.h"

#include "Misc/FrameRate.h"

#include "FixedFrameRateCustomTimeStep.generated.h"



/**
 * Class to control the Engine TimeStep via a FixedFrameRate
 */
UCLASS(Abstract)
class TIMEMANAGEMENT_API UFixedFrameRateCustomTimeStep : public UEngineCustomTimeStep
{
	GENERATED_UCLASS_BODY()

public:
	/** The fixed FrameRate */
	UPROPERTY(EditAnywhere, Category=Time)
	FFrameRate FixedFrameRate;

protected:
	/** Default behaviour of the engine. Used FixedFrameRate */
	void WaitForFixedFrameRate() const;
};
