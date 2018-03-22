// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Channels/MovieSceneFloatChannel.h"
#include "MovieSceneFwd.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieSceneFrameMigration.h"


uint32 FMovieSceneFloatChannel::GetChannelID()
{
	static uint32 ID = FMovieSceneChannelEntry::RegisterNewID();
	return ID;
}

bool FMovieSceneFloatChannel::SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar)
{
	static const FName RichCurveName("RichCurve");
	if (Tag.Type == NAME_StructProperty && Tag.StructName == RichCurveName)
	{
		FRichCurve RichCurve;
		FRichCurve::StaticStruct()->SerializeItem(Ar, &RichCurve, nullptr);

		if (RichCurve.GetDefaultValue() != MAX_flt)
		{
			bHasDefaultValue = true;
			DefaultValue = RichCurve.GetDefaultValue();
		}

		PreInfinityExtrap = RichCurve.PreInfinityExtrap;
		PostInfinityExtrap = RichCurve.PostInfinityExtrap;

		Times.Reserve(RichCurve.GetNumKeys());
		Values.Reserve(RichCurve.GetNumKeys());

		const FFrameRate LegacyFrameRate = GetLegacyConversionFrameRate();
		const float      Interval        = LegacyFrameRate.AsInterval();

		int32 Index = 0;
		for (auto It = RichCurve.GetKeyIterator(); It; ++It)
		{
			const FRichCurveKey& Key = *It;

			FFrameNumber KeyTime = UpgradeLegacyMovieSceneTime(nullptr, LegacyFrameRate, It->Time);

			FMovieSceneFloatValue NewValue;
			NewValue.Value = Key.Value;
			NewValue.InterpMode  = Key.InterpMode;
			NewValue.TangentMode = Key.TangentMode;
			NewValue.Tangent.ArriveTangent = Key.ArriveTangent * Interval;
			NewValue.Tangent.LeaveTangent  = Key.LeaveTangent  * Interval;
			ConvertInsertAndSort<FMovieSceneFloatValue>(Index++, KeyTime, NewValue, Times, Values);
		}
		return true;
	}

	return false;
}

int32 FMovieSceneFloatChannel::InsertKeyInternal(FFrameNumber InTime)
{
	const int32 InsertIndex = Algo::UpperBound(Times, InTime);

	Times.Insert(InTime, InsertIndex);
	Values.Insert(FMovieSceneFloatValue(), InsertIndex);

	KeyHandles.AllocateHandle(InsertIndex);

	return InsertIndex;
}

int32 FMovieSceneFloatChannel::AddConstantKey(FFrameNumber InTime, float InValue)
{
	const int32 Index = InsertKeyInternal(InTime);

	FMovieSceneFloatValue& Value = Values[Index];
	Value.Value = InValue;
	Value.InterpMode = RCIM_Constant;

	AutoSetTangents();

	return Index;
}

int32 FMovieSceneFloatChannel::AddLinearKey(FFrameNumber InTime, float InValue)
{
	const int32 Index = InsertKeyInternal(InTime);

	FMovieSceneFloatValue& Value = Values[Index];
	Value.Value = InValue;
	Value.InterpMode = RCIM_Linear;

	AutoSetTangents();

	return Index;
}

int32 FMovieSceneFloatChannel::AddCubicKey(FFrameNumber InTime, float InValue, ERichCurveTangentMode TangentMode, const FMovieSceneTangentData& Tangent)
{
	const int32 Index = InsertKeyInternal(InTime);

	FMovieSceneFloatValue& Value = Values[Index];
	Value.Value = InValue;
	Value.InterpMode = RCIM_Cubic;
	Value.TangentMode = TangentMode;
	Value.Tangent = Tangent;

	AutoSetTangents();

	return Index;
}

/** Util to find float value on bezier defined by 4 control points */
float BezierInterp(float P0, float P1, float P2, float P3, float Alpha)
{
	const float P01   = FMath::Lerp(P0,   P1,   Alpha);
	const float P12   = FMath::Lerp(P1,   P2,   Alpha);
	const float P23   = FMath::Lerp(P2,   P3,   Alpha);
	const float P012  = FMath::Lerp(P01,  P12,  Alpha);
	const float P123  = FMath::Lerp(P12,  P23,  Alpha);
	const float P0123 = FMath::Lerp(P012, P123, Alpha);

	return P0123;
}

struct FCycleParams
{
	FFrameTime Time;
	int32 CycleCount;
	float ValueOffset;

	FCycleParams(FFrameTime InTime)
		: Time(InTime)
		, CycleCount(0)
		, ValueOffset(0.f)
	{}

	FORCEINLINE void ComputePreValueOffset(float FirstValue, float LastValue)
	{
		ValueOffset = (FirstValue-LastValue) * CycleCount;
	}
	FORCEINLINE void ComputePostValueOffset(float FirstValue, float LastValue)
	{
		ValueOffset = (LastValue-FirstValue) * CycleCount;
	}
	FORCEINLINE void Oscillate(int32 MinFrame, int32 MaxFrame)
	{
		if (CycleCount % 2 == 1)
		{
			Time = MinFrame + (FFrameTime(MaxFrame) - Time);
		}
	}
};

FCycleParams CycleTime(FFrameNumber MinFrame, FFrameNumber MaxFrame, FFrameTime InTime)
{
	FCycleParams Params(InTime);
	
	const int32 Duration = MaxFrame.Value - MinFrame.Value;
	if (Duration == 0)
	{
		Params.Time = MaxFrame;
		Params.CycleCount = 0;
	}
	else if (InTime < MinFrame)
	{
		const int32 CycleCount = ((MaxFrame - InTime) / Duration).FloorToFrame().Value;

		Params.Time = InTime + FFrameTime(Duration)*CycleCount;
		Params.CycleCount = CycleCount;
	}
	else if (InTime > MaxFrame)
	{
		const int32 CycleCount = ((InTime - MinFrame) / Duration).FloorToFrame().Value;

		Params.Time = InTime - FFrameTime(Duration)*CycleCount;
		Params.CycleCount = CycleCount;
	}

	return Params;
}

bool FMovieSceneFloatChannel::EvaluateExtrapolation(FFrameTime InTime, float& OutValue) const
{
	// If the time is outside of the curve, deal with extrapolation
	if (InTime < Times[0])
	{
		if (PreInfinityExtrap == RCCE_None)
		{
			return false;
		}

		if (PreInfinityExtrap == RCCE_Constant)
		{
			OutValue = Values[0].Value;
			return true;
		}

		if (PreInfinityExtrap == RCCE_Linear)
		{
			const FMovieSceneFloatValue FirstValue = Values[0];

			if (FirstValue.InterpMode == RCIM_Constant)
			{
				OutValue = FirstValue.Value;
			}
			else if(FirstValue.InterpMode == RCIM_Cubic)
			{
				FFrameTime Delta = FFrameTime(Times[0]) - InTime;
				OutValue = FirstValue.Value - Delta.AsDecimal() * FirstValue.Tangent.ArriveTangent;
			}
			else if(FirstValue.InterpMode == RCIM_Linear)
			{
				const int32 InterpStartFrame = Times[1].Value;
				const int32 DeltaFrame       = InterpStartFrame - Times[0].Value;
				if (DeltaFrame == 0)
				{
					OutValue = FirstValue.Value;
				}
				else
				{
					OutValue = FMath::Lerp(Values[1].Value, FirstValue.Value, (InterpStartFrame - InTime.AsDecimal())/DeltaFrame);
				}
			}
			return true;
		}
	}
	else if (InTime > Times.Last())
	{
		if (PostInfinityExtrap == RCCE_None)
		{
			return false;
		}

		if (PostInfinityExtrap == RCCE_Constant)
		{
			OutValue = Values.Last().Value;
			return true;
		}

		if (PostInfinityExtrap == RCCE_Linear)
		{
			const FMovieSceneFloatValue LastValue = Values.Last();

			if (LastValue.InterpMode == RCIM_Constant)
			{
				OutValue = LastValue.Value;
			}
			else if(LastValue.InterpMode == RCIM_Cubic)
			{
				FFrameTime Delta = InTime - Times.Last();
				OutValue = LastValue.Value + Delta.AsDecimal() * LastValue.Tangent.LeaveTangent;
			}
			else if(LastValue.InterpMode == RCIM_Linear)
			{
				const int32 NumKeys          = Times.Num();
				const int32 InterpStartFrame = Times[NumKeys-2].Value;
				const int32 DeltaFrame       = Times.Last().Value-InterpStartFrame;

				if (DeltaFrame == 0)
				{
					OutValue = LastValue.Value;
				}
				else
				{
					OutValue = FMath::Lerp(Values[NumKeys-2].Value, LastValue.Value, (InTime.AsDecimal() - InterpStartFrame)/DeltaFrame);
				}
			}
			return true;
		}
	}

	return false;
}

bool FMovieSceneFloatChannel::Evaluate(FFrameTime InTime, float& OutValue) const
{
	const int32 NumKeys = Times.Num();

	// No keys means default value, or nothing
	if (NumKeys == 0)
	{
		if (bHasDefaultValue)
		{
			OutValue = DefaultValue;
			return true;
		}
		return false;
	}

	// For single keys, we can only ever return that value
	if (NumKeys == 1)
	{
		OutValue = Values[0].Value;
		return true;
	}

	// Evaluate with extrapolation if we're outside the bounds of the curve
	if (EvaluateExtrapolation(InTime, OutValue))
	{
		return true;
	}

	const FFrameNumber MinFrame = Times[0];
	const FFrameNumber MaxFrame = Times.Last();

	// Compute the cycled time
	FCycleParams Params = CycleTime(MinFrame, MaxFrame, InTime);

	// Deal with offset cycles and oscillation
	if (InTime < FFrameTime(MinFrame))
	{
		switch (PreInfinityExtrap)
		{
		case RCCE_CycleWithOffset: Params.ComputePreValueOffset(Values[0].Value, Values[NumKeys-1].Value); break;
		case RCCE_Oscillate:       Params.Oscillate(MinFrame.Value, MaxFrame.Value);                       break;
		}
	}
	else if (InTime > FFrameTime(MaxFrame))
	{
		switch (PostInfinityExtrap)
		{
		case RCCE_CycleWithOffset: Params.ComputePostValueOffset(Values[0].Value, Values[NumKeys-1].Value); break;
		case RCCE_Oscillate:       Params.Oscillate(MinFrame.Value, MaxFrame.Value);                        break;
		}
	}

	if (!ensureMsgf(Params.Time.FrameNumber >= MinFrame && Params.Time.FrameNumber <= MaxFrame, TEXT("Invalid time computed for float channel evaluation")))
	{
		return false;
	}

	// Evaluate the curve data
	float Interp = 0.f;
	int32 Index1 = INDEX_NONE, Index2 = INDEX_NONE;
	MovieScene::EvaluateTime(Times, Params.Time, Index1, Index2, Interp);

	if (Index1 == INDEX_NONE)
	{
		OutValue = Params.ValueOffset + Values[Index2].Value;
	}
	else if (Index2 == INDEX_NONE)
	{
		OutValue = Params.ValueOffset + Values[Index1].Value;
	}
	else
	{
		FMovieSceneFloatValue Key1 = Values[Index1];
		FMovieSceneFloatValue Key2 = Values[Index2];

		switch (Key1.InterpMode)
		{
		case RCIM_Cubic:
		{
			const float OneThird = 1.0f / 3.0f;
			const int32 Diff = Times[Index2].Value - Times[Index1].Value;
			const float P0 = Key1.Value;
			const float P1 = P0 + (Key1.Tangent.LeaveTangent * Diff * OneThird);
			const float P3 = Key2.Value;
			const float P2 = P3 - (Key2.Tangent.ArriveTangent * Diff * OneThird);

			OutValue = Params.ValueOffset + BezierInterp(P0, P1, P2, P3, Interp);
			break;
		}

		case RCIM_Linear:
			OutValue = Params.ValueOffset + FMath::Lerp(Key1.Value, Key2.Value, Interp);
			break;

		default:
			OutValue = Params.ValueOffset + Key1.Value;
			break;
		}
	}

	return true;
}

void FMovieSceneFloatChannel::AutoSetTangents(float Tension)
{
	if (Values.Num() < 2)
	{
		return;
	}

	{
		FMovieSceneFloatValue& FirstValue = Values[0];
		if (FirstValue.InterpMode == RCIM_Cubic && FirstValue.TangentMode == RCTM_Auto)
		{
			FirstValue.Tangent.LeaveTangent = FirstValue.Tangent.ArriveTangent = 0.0f;
		}
	}

	{
		FMovieSceneFloatValue& LastValue = Values.Last();
		if (LastValue.InterpMode == RCIM_Cubic && LastValue.TangentMode == RCTM_Auto)
		{
			LastValue.Tangent.LeaveTangent = LastValue.Tangent.ArriveTangent = 0.0f;
		}
	}

	for (int32 Index = 1; Index < Values.Num() - 1; ++Index)
	{
		FMovieSceneFloatValue  PrevKey = Values[Index-1];
		FMovieSceneFloatValue& ThisKey = Values[Index  ];

		if (ThisKey.InterpMode == RCIM_Cubic && ThisKey.TangentMode == RCTM_Auto)
		{
			FMovieSceneFloatValue& NextKey = Values[Index+1];

			const float PrevToNextTimeDiff = FMath::Max<double>(KINDA_SMALL_NUMBER, Times[Index+1].Value - Times[Index-1].Value);

			float NewTangent = 0.f;
			AutoCalcTangent(PrevKey.Value, ThisKey.Value, NextKey.Value, Tension, NewTangent);
			NewTangent /= PrevToNextTimeDiff;

			// In 'auto' mode, arrive and leave tangents are always the same
			ThisKey.Tangent.LeaveTangent = ThisKey.Tangent.ArriveTangent = NewTangent;
		}
		else if (PrevKey.InterpMode == RCIM_Constant || ThisKey.InterpMode == RCIM_Constant)
		{
			if (PrevKey.InterpMode != RCIM_Cubic)
			{
				ThisKey.Tangent.ArriveTangent = 0.f;
			}

			ThisKey.Tangent.LeaveTangent  = 0.0f;
		}
	}
}

void FMovieSceneFloatChannel::PopulateCurvePoints(double StartTimeSeconds, double EndTimeSeconds, double TimeThreshold, float ValueThreshold, FFrameRate FrameResolution, TArray<TTuple<double, double>>& InOutPoints) const
{
	const FFrameNumber StartFrame = (StartTimeSeconds * FrameResolution).FloorToFrame();
	const FFrameNumber EndFrame   = (EndTimeSeconds   * FrameResolution).CeilToFrame();

	const int32 StartingIndex = Algo::UpperBound(Times, StartFrame);
	const int32 EndingIndex   = Algo::LowerBound(Times, EndFrame);

	// Add the lower bound of the visible space
	float EvaluatedValue = 0.f;
	if (Evaluate(StartFrame, EvaluatedValue))
	{
		InOutPoints.Add(MakeTuple(StartFrame / FrameResolution, double(EvaluatedValue)));
	}

	// Add all keys in-between
	for (int32 KeyIndex = StartingIndex; KeyIndex < EndingIndex; ++KeyIndex)
	{
		InOutPoints.Add(MakeTuple(Times[KeyIndex] / FrameResolution, double(Values[KeyIndex].Value)));
	}

	// Add the upper bound of the visible space
	if (Evaluate(EndFrame, EvaluatedValue))
	{
		InOutPoints.Add(MakeTuple(EndFrame / FrameResolution, double(EvaluatedValue)));
	}

	int32 OldSize = InOutPoints.Num();
	do
	{
		OldSize = InOutPoints.Num();
		RefineCurvePoints(FrameResolution, TimeThreshold, ValueThreshold, InOutPoints);
	}
	while(OldSize != InOutPoints.Num());
}

void FMovieSceneFloatChannel::RefineCurvePoints(FFrameRate FrameResolution, double TimeThreshold, float ValueThreshold, TArray<TTuple<double, double>>& InOutPoints) const
{
	const float InterpTimes[] = { 0.25f, 0.5f, 0.6f };

	for (int32 Index = 0; Index < InOutPoints.Num() - 1; ++Index)
	{
		TTuple<double, double> Lower = InOutPoints[Index];
		TTuple<double, double> Upper = InOutPoints[Index + 1];

		if ((Upper.Get<0>() - Lower.Get<0>()) >= TimeThreshold)
		{
			bool bSegmentIsLinear = true;

			TTuple<double, double> Evaluated[ARRAY_COUNT(InterpTimes)];

			for (int32 InterpIndex = 0; InterpIndex < ARRAY_COUNT(InterpTimes); ++InterpIndex)
			{
				double& EvalTime  = Evaluated[InterpIndex].Get<0>();

				EvalTime = FMath::Lerp(Lower.Get<0>(), Upper.Get<0>(), InterpTimes[InterpIndex]);

				float Value = 0.f;
				Evaluate(EvalTime * FrameResolution, Value);

				const float LinearValue = FMath::Lerp(Lower.Get<1>(), Upper.Get<1>(), InterpTimes[InterpIndex]);
				if (bSegmentIsLinear)
				{
					bSegmentIsLinear = FMath::IsNearlyEqual(Value, LinearValue, ValueThreshold);
				}

				Evaluated[InterpIndex].Get<1>() = Value;
			}

			if (!bSegmentIsLinear)
			{
				// Add the point
				InOutPoints.Insert(Evaluated, ARRAY_COUNT(Evaluated), Index+1);
				--Index;
			}
		}
	}
}

FKeyHandle AddKeyToChannel(FMovieSceneFloatChannel* Channel, FFrameNumber InFrameNumber, float InValue, EMovieSceneKeyInterpolation Interpolation)
{
	auto ChannelInterface = Channel->GetInterface();
	int32 ExistingIndex = ChannelInterface.FindKey(InFrameNumber);
	if (ExistingIndex != INDEX_NONE)
	{
		FMovieSceneFloatValue& Value = ChannelInterface.GetValues()[ExistingIndex]; //-V758
		Value.Value = InValue;
		switch (Interpolation)
		{
		case EMovieSceneKeyInterpolation::User:
			Value.InterpMode = RCIM_Cubic;
			Value.TangentMode = RCTM_User;
			break;

		case EMovieSceneKeyInterpolation::Break:
			Value.InterpMode = RCIM_Cubic;
			Value.TangentMode = RCTM_Break;
			break;

		case EMovieSceneKeyInterpolation::Linear:
			Value.InterpMode = RCIM_Linear;
			Value.TangentMode = RCTM_Auto;
			break;

		case EMovieSceneKeyInterpolation::Constant:
			Value.InterpMode = RCIM_Constant;
			Value.TangentMode = RCTM_Auto;
			break;

		default:
			Value.InterpMode = RCIM_Cubic;
			Value.TangentMode = RCTM_Auto;
			break;
		}
	}
	else switch (Interpolation)
	{
		case EMovieSceneKeyInterpolation::Auto:     ExistingIndex = Channel->AddCubicKey(InFrameNumber, InValue, RCTM_Auto);  break;
		case EMovieSceneKeyInterpolation::User:     ExistingIndex = Channel->AddCubicKey(InFrameNumber, InValue, RCTM_User);  break;
		case EMovieSceneKeyInterpolation::Break:    ExistingIndex = Channel->AddCubicKey(InFrameNumber, InValue, RCTM_Break); break;
		case EMovieSceneKeyInterpolation::Linear:   ExistingIndex = Channel->AddLinearKey(InFrameNumber, InValue);            break;
		case EMovieSceneKeyInterpolation::Constant: ExistingIndex = Channel->AddConstantKey(InFrameNumber, InValue);          break;
	}

	return Channel->GetInterface().GetHandle(ExistingIndex);
}

void Optimize(FMovieSceneFloatChannel* InChannel, const FKeyDataOptimizationParams& Params)
{
	auto ChannelInterface = InChannel->GetInterface();
	if (ChannelInterface.GetTimes().Num() > 1)
	{
		int32 StartIndex = 0;
		int32 EndIndex = 0;

		{
			TArrayView<const FFrameNumber> Times = ChannelInterface.GetTimes();
			StartIndex = Params.Range.GetLowerBound().IsClosed() ? Algo::LowerBound(Times, Params.Range.GetLowerBoundValue()) : 0;
			EndIndex   = Params.Range.GetUpperBound().IsClosed() ? Algo::UpperBound(Times, Params.Range.GetUpperBoundValue()) : Times.Num();
		}

		for (int32 Index = StartIndex; Index < EndIndex; ++Index)
		{
			// Reget times and values as they may be reallocated
			FFrameNumber Time                   = ChannelInterface.GetTimes()[Index];
			FMovieSceneFloatValue OriginalValue = ChannelInterface.GetValues()[Index];

			// If the channel evaluates the same with this key removed, we can leave it out
			ChannelInterface.RemoveKey(Index);
			if (ValueExistsAtTime(InChannel, Time, OriginalValue.Value, Params.Tolerance))
			{
				Index--;
			}
			else
			{
				ChannelInterface.AddKey(Time, OriginalValue);
			}
		}

		if (Params.bAutoSetInterpolation)
		{
			InChannel->AutoSetTangents();
		}
	}
}

void Dilate(FMovieSceneFloatChannel* InChannel, FFrameNumber Origin, float DilationFactor)
{
	TArrayView<FFrameNumber> Times = InChannel->GetInterface().GetTimes();
	for (FFrameNumber& Time : Times)
	{
		Time = Origin + FFrameNumber(FMath::FloorToInt((Time - Origin).Value * DilationFactor));
	}
	InChannel->AutoSetTangents();
}

void ChangeFrameResolution(FMovieSceneFloatChannel* InChannel, FFrameRate SourceRate, FFrameRate DestinationRate)
{
	TArrayView<FFrameNumber>          Times  = InChannel->GetInterface().GetTimes();
	TArrayView<FMovieSceneFloatValue> Values = InChannel->GetInterface().GetValues();

	check(Times.Num() == Values.Num());

	float IntervalFactor = DestinationRate.AsInterval() / SourceRate.AsInterval();
	for (int32 Index = 0; Index < Times.Num(); ++Index)
	{
		Times[Index] = ConvertFrameTime(Times[Index], SourceRate, DestinationRate).RoundToFrame();

		FMovieSceneFloatValue& Value = Values[Index];
		Value.Tangent.ArriveTangent *= IntervalFactor;
		Value.Tangent.LeaveTangent  *= IntervalFactor;
	}
}