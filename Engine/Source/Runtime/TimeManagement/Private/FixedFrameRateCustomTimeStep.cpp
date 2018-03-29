// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "FixedFrameRateCustomTimeStep.h"


UFixedFrameRateCustomTimeStep::UFixedFrameRateCustomTimeStep(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FixedFrameRate(30, 1)
{
}


