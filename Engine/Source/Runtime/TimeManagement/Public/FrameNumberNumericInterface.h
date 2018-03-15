// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FrameNumberDisplayFormat.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "FrameRate.h"
#include "FrameNumberTimeEvaluator.h"
#include "Timecode.h"


DECLARE_DELEGATE_RetVal(EFrameNumberDisplayFormats, FOnGetDisplayFormat)
DECLARE_DELEGATE_RetVal(uint8, FOnGetZeroPad)
DECLARE_DELEGATE_RetVal(FFrameRate, FOnGetFrameRate)

/**
* A large portion of the Sequencer UI is built around using SpinBox and NumericBox because the framerate
* used to be stored in (float) seconds. This creates a convenient UI as it allows the users to type in
* numbers (for frames or seconds), or to drag the mouse to change the time. When Sequencer was converted
* to using integer based frames and frame rates (expressed as numerator/denominator) the spinbox-based
* UI became an issue as SpinBox and NumericBox internally convert to a double to perform various calculations.
* This is an issue if your Spinbox type (ie: FQualifiedFrameTime) is not implicitly convertible to double,
* which the new Sequencer types are not (due to accidental type coercion when using outside the UI).
*
* To solve this, the Spinboxes will still use double as their type, but double now means frame number and not
* time. A double can store the entire range of int32 (which backs FFrameNumber) without precision loss, and
* we can execute callbacks to find out what framerate the sequence is running at. This allows us to display
* in Timecode, Time or Frames and convert back and forth to double for the UI, and from double into the backing
* FFrameNumber for the sequence.
*/
struct FFrameNumberInterface : public INumericTypeInterface<double>
{
	FFrameNumberInterface(FOnGetDisplayFormat InGetDisplayFormat, FOnGetZeroPad InOnGetZeroPadFrameNumber, FOnGetFrameRate InGetFrameResolution, FOnGetFrameRate InGetPlayRate)
		: GetDisplayFormat(InGetDisplayFormat)
		, GetFrameResolution(InGetFrameResolution)
		, GetPlayRate(InGetPlayRate)
		, GetZeroPadFrames(InOnGetZeroPadFrameNumber)
	{
		check(InGetDisplayFormat.IsBound());
		check(InGetFrameResolution.IsBound());
		check(InGetPlayRate.IsBound());
	}

private:
	FOnGetDisplayFormat GetDisplayFormat;
	FOnGetFrameRate GetFrameResolution;
	FOnGetFrameRate GetPlayRate;
	FOnGetZeroPad GetZeroPadFrames;

	/** Check whether the typed character is valid */
	virtual bool IsCharacterValid(TCHAR InChar) const override
	{
		auto IsValidLocalizedCharacter = [InChar]() -> bool
		{
			const FDecimalNumberFormattingRules& NumberFormattingRules = ExpressionParser::GetLocalizedNumberFormattingRules();
			return InChar == NumberFormattingRules.GroupingSeparatorCharacter
				|| InChar == NumberFormattingRules.DecimalSeparatorCharacter
				|| Algo::Find(NumberFormattingRules.DigitCharacters, InChar) != 0;
		};

		static const FString ValidChars = TEXT("1234567890()-+=\\/.,*^%%hrmsf[]:; ");
		return InChar != 0 && (ValidChars.GetCharArray().Contains(InChar) || IsValidLocalizedCharacter());
	}

	virtual FString ToString(const double& Value) const override
	{
		FFrameRate SourceFrameRate = GetFrameResolution.Execute();
		FFrameRate DestinationFrameRate = GetPlayRate.Execute();
		EFrameNumberDisplayFormats Format = GetDisplayFormat.Execute();

		// If they want Drop Frame Timecode format but we're in an unsupported frame rate, we'll override it and say they want non drop frame.
		bool bIsValidRateForDropFrame = FTimecode::IsDropFormatTimecodeSupported(DestinationFrameRate);
		if (Format == EFrameNumberDisplayFormats::DropFrameTimecode && !bIsValidRateForDropFrame)
		{
			Format = EFrameNumberDisplayFormats::NonDropFrameTimecode;
		}

		switch (Format)
		{
		case EFrameNumberDisplayFormats::Frames:
		{
			// Convert from sequence resolution into display rate frames.
			FFrameTime DisplayTime = FFrameRate::TransformTime(FFrameTime::FromDecimal(Value), SourceFrameRate, DestinationFrameRate);
			FString SubframeIndicator = FMath::IsNearlyZero(DisplayTime.GetSubFrame()) ? TEXT("") : TEXT("*");

			return FString::Printf(TEXT("%0*d%s"), GetZeroPadFrames.Execute(), DisplayTime.GetFrame().Value, *SubframeIndicator);
		}
		case EFrameNumberDisplayFormats::Seconds:
		{
			double TimeInSeconds = SourceFrameRate.AsSeconds(FFrameTime::FromDecimal(Value));
			return FString::Printf(TEXT("%.2f s"), TimeInSeconds);
		}
		case EFrameNumberDisplayFormats::NonDropFrameTimecode:
		{
			FFrameTime InternalFrameNumber = FFrameTime::FromDecimal(Value);
			FFrameNumber PlayRateFrameNumber = FFrameRate::TransformTime(InternalFrameNumber, SourceFrameRate, DestinationFrameRate).FloorToFrame();

			FTimecode AsNonDropTimecode = FTimecode::FromFrameNumber(PlayRateFrameNumber, DestinationFrameRate, false);
			return FString::Printf(TEXT("[%s]"), *AsNonDropTimecode.ToString(false));
		}
		case EFrameNumberDisplayFormats::DropFrameTimecode:
		{
			FFrameTime InternalFrameNumber = FFrameTime::FromDecimal(Value);
			FFrameNumber PlayRateFrameNumber = FFrameRate::TransformTime(InternalFrameNumber, SourceFrameRate, DestinationFrameRate).FloorToFrame();

			FTimecode AsDropTimecode = FTimecode::FromFrameNumber(PlayRateFrameNumber, DestinationFrameRate, true);
			return FString::Printf(TEXT("[%s]"), *AsDropTimecode.ToString(false));
		}
		default:
			return FString(TEXT("Unsupported Format"));
		}
	}

	virtual TOptional<double> FromString(const FString& InString, const double& InExistingValue) override
	{
		FFrameRate SourceFrameRate = GetPlayRate.Execute();
		FFrameRate DestinationFrameRate = GetFrameResolution.Execute();
		EFrameNumberDisplayFormats FallbackFormat = GetDisplayFormat.Execute();

		// We allow input in any format (time, frames or timecode) and we just convert it into into the internal sequence resolution.
		// The user's input can be ambiguous though (does "5" mean 5 frames or 5 seconds?) so when we check each possible result we
		// also check to see if they explicitly specified that format, or if the evaluator just happens to be able to parse that.

		// All of these will convert into the frame resolution from the user's input before returning.
		FFrameNumberTimeEvaluator Eval;
		TValueOrError<FFrameTime, FExpressionError> TimecodeResult = Eval.EvaluateTimecode(*InString, SourceFrameRate, DestinationFrameRate);
		bool bWasFrameText;
		TValueOrError<FFrameTime, FExpressionError> FrameResult = Eval.EvaluateFrame(*InString, SourceFrameRate, DestinationFrameRate, /*Out*/ bWasFrameText);
		bool bWasTimeText;
		TValueOrError<FFrameTime, FExpressionError> TimeResult = Eval.EvaluateTime(*InString, DestinationFrameRate, /*Out*/ bWasTimeText);

		if (TimecodeResult.IsValid())
		{
			return TOptional<double>(TimecodeResult.GetValue().GetFrame().Value);
		}
		else if (!bWasFrameText && !bWasTimeText)
		{
			// If the value was ambiguous ("5") and not specifically a frame ("5f") or a time "(5s") then we defer to the current display format.
			if (TimeResult.IsValid() && FallbackFormat == EFrameNumberDisplayFormats::Seconds)
			{
				return TOptional<double>(TimeResult.GetValue().GetFrame().Value);
			}
			else if (FrameResult.IsValid() && FallbackFormat == EFrameNumberDisplayFormats::Frames)
			{
				return TOptional<double>(FrameResult.GetValue().GetFrame().Value);
			}
			else
			{ 
				// We don't support ambiguous conversion to Timecode
				return TOptional<double>();
			}
		}
		else if (TimeResult.IsValid() && !bWasFrameText)
		{
			// Both time and frames can parse "5" successfully, so if we know it's not specifically frame text
			return TOptional<double>(TimeResult.GetValue().GetFrame().Value);
		}
		else if (FrameResult.IsValid() && !bWasTimeText)
		{
			// Both time and frames can parse "5" successfully and we know it's not specifically time
			return TOptional<double>(FrameResult.GetValue().GetFrame().Value);
		}

		// We're not sure what they typed in
		return TOptional<double>();
	}
};