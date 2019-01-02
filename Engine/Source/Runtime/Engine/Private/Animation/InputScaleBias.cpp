// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Animation/InputScaleBias.h"

#define LOCTEXT_NAMESPACE "FInputScaleBias"

/////////////////////////////////////////////////////
// FInputScaleBias

float FInputScaleBias::ApplyTo(float Value) const
{
	return FMath::Clamp<float>( Value * Scale + Bias, 0.0f, 1.0f );
}

FText FInputScaleBias::GetFriendlyName(FText InFriendlyName) const
{
	FText OutFriendlyName = InFriendlyName;

	if (Scale != 1.f)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PinFriendlyName"), OutFriendlyName);
		Args.Add(TEXT("Scale"), FText::AsNumber(Scale));

		if (Scale == -1.f)
		{
			OutFriendlyName = FText::Format(LOCTEXT("FInputScaleBias_Scale", "- {PinFriendlyName}"), Args);
		}
		else
		{
			OutFriendlyName = FText::Format(LOCTEXT("FInputScaleBias_ScaleMul", "{Scale} * {PinFriendlyName}"), Args);
		}
	}

	if (Bias != 0.f)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PinFriendlyName"), OutFriendlyName);
		Args.Add(TEXT("Bias"), FText::AsNumber(Bias));

		// '-' Sign already included in Scale above.
		if (Scale < 0.f)
		{
			OutFriendlyName = FText::Format(LOCTEXT("FInputScaleBias_Bias", "{Bias} {PinFriendlyName}"), Args);
		}
		else
		{
			OutFriendlyName = FText::Format(LOCTEXT("FInputScaleBias_BiasPlus", "{Bias} + {PinFriendlyName}"), Args);
		}
	}

	return OutFriendlyName;
}

/////////////////////////////////////////////////////
// FInputScaleBiasClamp

float FInputScaleBiasClamp::ApplyTo(float Value, float InDeltaTime) const
{
	float Result = Value;

	if (bMapRange)
	{
		Result = FMath::GetMappedRangeValueUnclamped(InRange.ToVector2D(), OutRange.ToVector2D(), Result);
	}

	Result = Result * Scale + Bias;

	if (bClampResult)
	{
		Result = FMath::Clamp<float>(Result, ClampMin, ClampMax);
	}

	if (bInterpResult)
	{
		if (bInitialized)
		{
			const float InterpSpeed = (Result >= InterpolatedResult) ? InterpSpeedIncreasing : InterpSpeedDecreasing;
			Result = FMath::FInterpTo(InterpolatedResult, Result, InDeltaTime, InterpSpeed);
		}

		InterpolatedResult = Result;
	}

	bInitialized = true;
	return Result;
}

FText FInputScaleBiasClamp::GetFriendlyName(FText InFriendlyName) const
{
	FText OutFriendlyName = InFriendlyName;

	// MapRange
	if (bMapRange)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PinFriendlyName"), OutFriendlyName);
		Args.Add(TEXT("InRangeMin"), InRange.Min);
		Args.Add(TEXT("InRangeMax"), InRange.Max);
		Args.Add(TEXT("OutRangeMin"), OutRange.Min);
		Args.Add(TEXT("OutRangeMax"), OutRange.Max);
		OutFriendlyName = FText::Format(LOCTEXT("FInputScaleBias_MapRange", "MapRange({PinFriendlyName}, In({InRangeMin}:{InRangeMax}), Out({OutRangeMin}:{OutRangeMax}))"), Args);
	}

	if (Scale != 1.f)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PinFriendlyName"), OutFriendlyName);
		Args.Add(TEXT("Scale"), FText::AsNumber(Scale));

		if (Scale == -1.f)
		{
			OutFriendlyName = FText::Format(LOCTEXT("FInputScaleBias_Scale", "- {PinFriendlyName}"), Args);
		}
		else 
		{
			OutFriendlyName = FText::Format(LOCTEXT("FInputScaleBias_ScaleMul", "{Scale} * {PinFriendlyName}"), Args);
		}
	}

	if (Bias != 0.f)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PinFriendlyName"), OutFriendlyName);
		Args.Add(TEXT("Bias"), FText::AsNumber(Bias));

		// '-' Sign already included in Scale above.
		if (Scale < 0.f)
		{
			OutFriendlyName = FText::Format(LOCTEXT("FInputScaleBias_Bias", "{Bias} {PinFriendlyName}"), Args);
		}
		else
		{
			OutFriendlyName = FText::Format(LOCTEXT("FInputScaleBias_BiasPlus", "{Bias} + {PinFriendlyName}"), Args);
		}
	}

	// Clamp
	if (bClampResult)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PinFriendlyName"), OutFriendlyName);
		Args.Add(TEXT("ClampMin"), ClampMin);
		Args.Add(TEXT("ClampMax"), ClampMax);
		OutFriendlyName = FText::Format(LOCTEXT("FInputScaleBias_Clamp", "Clamp({PinFriendlyName}, {ClampMin}, {ClampMax})"), Args);
	}

	// Interp
	if (bInterpResult)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PinFriendlyName"), OutFriendlyName);
		Args.Add(TEXT("InterpSpeedIncreasing"), InterpSpeedIncreasing);
		Args.Add(TEXT("InterpSpeedDecreasing"), InterpSpeedDecreasing);
		OutFriendlyName = FText::Format(LOCTEXT("FInputScaleBias_Interp", "FInterp({PinFriendlyName}, ({InterpSpeedIncreasing}:{InterpSpeedDecreasing}))"), Args);
	}

	return OutFriendlyName;
}


/////////////////////////////////////////////////////
// FInputAlphaBool

float FInputAlphaBoolBlend::ApplyTo(bool bEnabled, float InDeltaTime)
{
	const float TargetValue = bEnabled ? 1.f : 0.f;

	if (!bInitialized)
	{
		if (CustomCurve != AlphaBlend.GetCustomCurve())
		{
			AlphaBlend.SetCustomCurve(CustomCurve);
		}

		if (BlendOption != AlphaBlend.GetBlendOption())
		{
			AlphaBlend.SetBlendOption(BlendOption);
		}

		AlphaBlend.SetDesiredValue(TargetValue);
		AlphaBlend.SetBlendTime(0.f);
		AlphaBlend.Reset();
		bInitialized = true;
	}
	else
	{
		if (AlphaBlend.GetDesiredValue() != TargetValue)
		{
			AlphaBlend.SetDesiredValue(TargetValue);
			AlphaBlend.SetBlendTime(bEnabled ? BlendInTime : BlendOutTime);
		}
	}

	AlphaBlend.Update(InDeltaTime);
	return AlphaBlend.GetBlendedValue();
}

#undef LOCTEXT_NAMESPACE 