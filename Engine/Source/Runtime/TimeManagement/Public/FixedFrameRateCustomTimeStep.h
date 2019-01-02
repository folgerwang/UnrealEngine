// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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

protected:
	/** The fixed FrameRate */
	UE_DEPRECATED(4.21, "The FixedFrameRateCustomTimeStep.FixedFrameRate is replaced by the function GetFixedFrameRate and will be removed from the codebase in a future release. Please use the function GetFixedFrameRate().")
	UPROPERTY()
	FFrameRate FixedFrameRate;

public:
	/** Get The fixed FrameRate */
	virtual FFrameRate GetFixedFrameRate() const PURE_VIRTUAL(UFixedFrameRateCustomTimeStep::GetFixedFrameRate, return GetFixedFrameRate_PureVirtual(););

protected:
	/** Default behavior of the engine. Used FixedFrameRate */
	void WaitForFixedFrameRate() const;

private:
	FFrameRate GetFixedFrameRate_PureVirtual() const;
};
