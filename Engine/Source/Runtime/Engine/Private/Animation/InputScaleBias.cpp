// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Animation/InputScaleBias.h"

/////////////////////////////////////////////////////
// FInputScaleBias

float FInputScaleBias::ApplyTo(float Value) const
{
	return FMath::Clamp<float>( Value * Scale + Bias, 0.0f, 1.0f );
}

/////////////////////////////////////////////////////
// FInputScaleBiasClamp

float FInputScaleBiasClamp::ApplyTo(float Value) const
{
	const float UnClampedResult = Value * Scale + Bias;
	return bClampResult ? FMath::Clamp<float>(UnClampedResult, ClampMin, ClampMax) : UnClampedResult;
}