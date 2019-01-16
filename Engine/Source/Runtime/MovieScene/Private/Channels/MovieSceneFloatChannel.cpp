// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Channels/MovieSceneFloatChannel.h"
#include "MovieSceneFwd.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieSceneFrameMigration.h"
#include "UObject/SequencerObjectVersion.h"
#include "HAL/ConsoleManager.h"

static TAutoConsoleVariable<int32> CVarSequencerLinearCubicInterpolation(
	TEXT("Sequencer.LinearCubicInterpolation"),
	1,
	TEXT("If 1 Linear Keys Act As Cubic Interpolation with Linear Tangents, if 0 Linear Key Forces Linear Interpolation to Next Key."),
	ECVF_Default);

bool FMovieSceneTangentData::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FSequencerObjectVersion::GUID);
	if (Ar.CustomVer(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::SerializeFloatChannel)
	{
		return false;
	}

	// Serialization is handled manually to avoid the extra size overhead of UProperty tagging.
	// Otherwise with many keys in a FMovieSceneTangentData the size can become quite large.
	Ar << ArriveTangent;
	Ar << LeaveTangent;
	Ar << TangentWeightMode;
	Ar << ArriveTangentWeight;
	Ar << LeaveTangentWeight;

	return true;
}

bool FMovieSceneTangentData::operator==(const FMovieSceneTangentData& TangentData) const
{
	return (ArriveTangent == TangentData.ArriveTangent) && (LeaveTangent == TangentData.LeaveTangent) && (TangentWeightMode == TangentData.TangentWeightMode) && (ArriveTangentWeight == TangentData.ArriveTangentWeight) && (LeaveTangentWeight == TangentData.LeaveTangentWeight);
}

bool FMovieSceneTangentData::operator!=(const FMovieSceneTangentData& Other) const
{
	return !(*this == Other);
}

bool FMovieSceneFloatValue::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FSequencerObjectVersion::GUID);
	if (Ar.CustomVer(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::SerializeFloatChannel)
	{
		return false;
	}

	// Serialization is handled manually to avoid the extra size overhead of UProperty tagging.
	// Otherwise with many keys in a FMovieSceneFloatValue the size can become quite large.
	Ar << Value;
	Ar << InterpMode;
	Ar << TangentMode;
	Ar << Tangent;

	return true;
}

bool FMovieSceneFloatValue::operator==(const FMovieSceneFloatValue& FloatValue) const
{
	return (Value == FloatValue.Value) && (InterpMode == FloatValue.InterpMode) && (TangentMode == FloatValue.TangentMode) && (Tangent == FloatValue.Tangent);
}

bool FMovieSceneFloatValue::operator!=(const FMovieSceneFloatValue& Other) const
{
	return !(*this == Other);
}

bool FMovieSceneFloatChannel::SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	static const FName RichCurveName("RichCurve");
	if (Tag.Type == NAME_StructProperty && Tag.StructName == RichCurveName)
	{
		FRichCurve RichCurve;
		FRichCurve::StaticStruct()->SerializeItem(Slot, &RichCurve, nullptr);

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


static float EvalForTwoKeys(const FMovieSceneFloatValue& Key1, FFrameNumber Key1Time,
							const FMovieSceneFloatValue& Key2, FFrameNumber Key2Time,
							FFrameNumber InTime,
							FFrameRate DisplayRate)
{
	double DecimalRate = DisplayRate.AsDecimal();

	float Diff = (float)(Key2Time - Key1Time).Value;
	Diff /= DecimalRate;
	const int CheckBothLinear = CVarSequencerLinearCubicInterpolation->GetInt();

	if (Diff > 0 && Key1.InterpMode != RCIM_Constant)
	{
		const float Alpha = ((float)(InTime - Key1Time).Value / DecimalRate) / Diff;
		const float P0 = Key1.Value;
		const float P3 = Key2.Value;

		if (Key1.InterpMode == RCIM_Linear && (!CheckBothLinear || Key2.InterpMode != RCIM_Cubic))
		{
			return FMath::Lerp(P0, P3, Alpha);
		}
		else
		{
			float LeaveTangent = Key1.Tangent.LeaveTangent * DecimalRate;
			float ArriveTangent = Key2.Tangent.ArriveTangent * DecimalRate;

			const float OneThird = 1.0f / 3.0f;
			const float P1 = P0 + (LeaveTangent * Diff*OneThird);
			const float P2 = P3 - (ArriveTangent * Diff*OneThird);

			return BezierInterp(P0, P1, P2, P3, Alpha);
		}
	}
	else
	{
		return Key1.Value;
	}
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


/* Solve Cubic Euqation using Cardano's forumla
* Adopted from Graphic Gems 1
* https://github.com/erich666/GraphicsGems/blob/master/gems/Roots3And4.c
*  Solve cubic of form
*
* @param Coeff Coefficient parameters of form  Coeff[0] + Coeff[1]*x + Coeff[2]*x^2 + Coeff[3]*x^3 + Coeff[4]*x^4 = 0
* @param Solution Up to 3 real solutions. We don't include imaginary solutions, would need a complex number objecct
* @return Returns the number of real solutions returned in the Solution array.
*/
static int SolveCubic(double Coeff[4], double Solution[3])
{
	auto cbrt = [](double x) -> double
	{
		return ((x) > 0.0 ? pow((x), 1.0 / 3.0) : ((x) < 0.0 ? -pow((double)-(x), 1.0 / 3.0) : 0.0));
	};
	int     NumSolutions = 0;

	/* normal form: x^3 + Ax^2 + Bx + C = 0 */

	double A = Coeff[2] / Coeff[3];
	double B = Coeff[1] / Coeff[3];
	double C = Coeff[0] / Coeff[3];

	/*  substitute x = y - A/3 to eliminate quadric term:
	x^3 +px + q = 0 */

	double SqOfA = A * A;
	double P = 1.0 / 3 * (-1.0 / 3 * SqOfA + B);
	double Q = 1.0 / 2 * (2.0 / 27 * A * SqOfA - 1.0 / 3 * A * B + C);

	/* use Cardano's formula */

	double CubeOfP = P * P * P;
	double D = Q * Q + CubeOfP;

	if (FMath::IsNearlyZero(D))
	{
		if (FMath::IsNearlyZero(Q)) /* one triple solution */
		{
			Solution[0] = 0;
			NumSolutions = 1;
		}
		else /* one single and one double solution */
		{
			double u = cbrt(-Q);
			Solution[0] = 2 * u;
			Solution[1] = -u;
			NumSolutions = 2;
		}
	}
	else if (D < 0) /* Casus irreducibilis: three real solutions */
	{
		double phi = 1.0 / 3 * acos(-Q / sqrt(-CubeOfP));
		double t = 2 * sqrt(-P);

		Solution[0] = t * cos(phi);
		Solution[1] = -t * cos(phi + PI / 3);
		Solution[2] = -t * cos(phi - PI / 3);
		NumSolutions = 3;
	}
	else /* one real solution */
	{
		double sqrt_D = sqrt(D);
		double u = cbrt(sqrt_D - Q);
		double v = -cbrt(sqrt_D + Q);

		Solution[0] = u + v;
		NumSolutions = 1;
	}

	/* resubstitute */

	double Sub = 1.0 / 3 * A;

	for (int i = 0; i < NumSolutions; ++i)
		Solution[i] -= Sub;

	return NumSolutions;
}

/*
*   Convert the control values for a polynomial defined in the Bezier
*		basis to a polynomial defined in the power basis (t^3 t^2 t 1).
*/
static void BezierToPower(	double A1, double B1, double C1, double D1,
	double *A2, double *B2, double *C2, double *D2)
{
	double A = B1 - A1;
	double B = C1 - B1;
	double C = D1 - C1;
	double D = B - A;
	*A2 = C- B - D;
	*B2 = 3.0 * D;
	*C2 = 3.0 * A;
	*D2 = A1;
}

bool FMovieSceneFloatChannel::Evaluate(FFrameTime InTime,  float& OutValue) const
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
	const int CheckBothLinear = CVarSequencerLinearCubicInterpolation->GetInt();

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
		TEnumAsByte<ERichCurveInterpMode> InterpMode = Key1.InterpMode;
	    if(InterpMode == RCIM_Linear && (CheckBothLinear  && Key2.InterpMode == RCIM_Cubic))
		{
			InterpMode = RCIM_Cubic;
		}
		
		switch (InterpMode)
		{
		case RCIM_Cubic:
		{
			const float OneThird = 1.0f / 3.0f;
			if ((Key1.Tangent.TangentWeightMode == RCTWM_WeightedNone || Key1.Tangent.TangentWeightMode == RCTWM_WeightedArrive)
				&& (Key2.Tangent.TangentWeightMode == RCTWM_WeightedNone || Key2.Tangent.TangentWeightMode == RCTWM_WeightedLeave))
			{
				const int32 Diff = Times[Index2].Value - Times[Index1].Value;
				const float P0 = Key1.Value;
				const float P1 = P0 + (Key1.Tangent.LeaveTangent * Diff * OneThird);
				const float P3 = Key2.Value;
				const float P2 = P3 - (Key2.Tangent.ArriveTangent * Diff * OneThird);

				OutValue = Params.ValueOffset + BezierInterp(P0, P1, P2, P3, Interp);
				break;
			}
			else //its weighted
			{
				const float TimeInterval = TickResolution.AsInterval();
				const float ToSeconds = 1.0f / TimeInterval;

				const double Time1 = TickResolution.AsSeconds(Times[Index1].Value);
				const double Time2 = TickResolution.AsSeconds(Times[Index2].Value);
				const float X = Time2 - Time1;
				float CosAngle, SinAngle;
				float Angle = FMath::Atan(Key1.Tangent.LeaveTangent * ToSeconds);
				FMath::SinCos(&SinAngle, &CosAngle, Angle);
				float LeaveWeight;
				if (Key1.Tangent.TangentWeightMode == RCTWM_WeightedNone || Key1.Tangent.TangentWeightMode == RCTWM_WeightedArrive)
				{
					const float LeaveTangentNormalized = Key1.Tangent.LeaveTangent / (TimeInterval);
					const float Y = LeaveTangentNormalized * X;
					LeaveWeight = FMath::Sqrt(X*X + Y * Y) * OneThird;
				}
				else
				{
					LeaveWeight = Key1.Tangent.LeaveTangentWeight;
				}
				const float Key1TanX = CosAngle * LeaveWeight + Time1;
				const float Key1TanY = SinAngle * LeaveWeight + Key1.Value;

				Angle = FMath::Atan(Key2.Tangent.ArriveTangent * ToSeconds);
				FMath::SinCos(&SinAngle, &CosAngle, Angle);
				float ArriveWeight;
				if (Key2.Tangent.TangentWeightMode == RCTWM_WeightedNone || Key2.Tangent.TangentWeightMode == RCTWM_WeightedLeave)
				{
					const float ArriveTangentNormalized = Key2.Tangent.ArriveTangent / (TimeInterval);
					const float Y = ArriveTangentNormalized * X;
					ArriveWeight = FMath::Sqrt(X*X + Y * Y) * OneThird;
				}
				else
				{
					ArriveWeight =  Key2.Tangent.ArriveTangentWeight;
				}
				const float Key2TanX = -CosAngle * ArriveWeight + Time2;
				const float Key2TanY = -SinAngle * ArriveWeight + Key2.Value;

				//Normalize the Time Range
				const float RangeX = Time2 - Time1;

				const float Dx1 = Key1TanX - Time1;
				const float Dx2 = Key2TanX - Time1;

				// Normalize values
				const float NormalizedX1 = Dx1 / RangeX;
				const float NormalizedX2 = Dx2 / RangeX;
				
				double Coeff[4];
				double Results[3];

				//Convert Bezier to Power basis, also float to double for precision for root finding.
				BezierToPower(
					0.0, NormalizedX1, NormalizedX2, 1.0,
					&(Coeff[3]), &(Coeff[2]), &(Coeff[1]), &(Coeff[0])
				);

				Coeff[0] = Coeff[0] - Interp;
				
				int NumResults = SolveCubic(Coeff, Results);
				float NewInterp = Interp;
				if (NumResults == 1)
				{
					NewInterp = Results[0];
				}
				else
				{
					NewInterp = TNumericLimits<float>::Lowest(); //just need to be out of range
					for (double Result : Results)
					{
						if ((Result >= 0.0f) && (Result <= 1.0f))
						{
							if (NewInterp < 0.0f || Result > NewInterp)
							{
								NewInterp = Result;
							}
						}
					}
				}
				//now use NewInterp and adjusted tangents plugged into the Y (Value) part of the graph.
				const float P0 = Key1.Value;
				const float P1 = Key1TanY;
				const float P3 = Key2.Value;
				const float P2 = Key2TanY;

				OutValue = Params.ValueOffset + BezierInterp(P0, P1, P2, P3,  NewInterp);
			}
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
		if (FirstValue.InterpMode == RCIM_Linear)
		{
			FirstValue.Tangent.TangentWeightMode = RCTWM_WeightedNone;
			FMovieSceneFloatValue& NextKey = Values[1];
			const float NextTimeDiff = FMath::Max<double>(KINDA_SMALL_NUMBER, Times[1].Value - Times[0].Value);
			float NewTangent = (NextKey.Value - FirstValue.Value) / NextTimeDiff;
			FirstValue.Tangent.LeaveTangent = NewTangent;
		}
		else if (FirstValue.InterpMode == RCIM_Cubic && FirstValue.TangentMode == RCTM_Auto)
		{
			FirstValue.Tangent.LeaveTangent = FirstValue.Tangent.ArriveTangent = 0.0f;
			FirstValue.Tangent.TangentWeightMode = RCTWM_WeightedNone; 
		}
	}

	{
		FMovieSceneFloatValue& LastValue = Values.Last();
		if (LastValue.InterpMode == RCIM_Linear)
		{
			LastValue.Tangent.TangentWeightMode = RCTWM_WeightedNone;
			int32 Index = Values.Num() - 1;
			FMovieSceneFloatValue& PrevKey = Values[Index-1];
			const float PrevTimeDiff = FMath::Max<double>(KINDA_SMALL_NUMBER, Times[Index].Value - Times[Index - 1].Value);
			float NewTangent = (LastValue.Value - PrevKey.Value) / PrevTimeDiff;
			LastValue.Tangent.ArriveTangent = NewTangent;
		}
		else if (LastValue.InterpMode == RCIM_Cubic && LastValue.TangentMode == RCTM_Auto)
		{
			LastValue.Tangent.LeaveTangent = LastValue.Tangent.ArriveTangent = 0.0f;
			LastValue.Tangent.TangentWeightMode = RCTWM_WeightedNone;
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
			ThisKey.Tangent.TangentWeightMode = RCTWM_WeightedNone;
		}
		else if (ThisKey.InterpMode == RCIM_Linear)
		{
			ThisKey.Tangent.TangentWeightMode = RCTWM_WeightedNone; 
			FMovieSceneFloatValue& NextKey = Values[Index + 1];

			const float PrevTimeDiff = FMath::Max<double>(KINDA_SMALL_NUMBER, Times[Index].Value - Times[Index - 1].Value);
			float NewTangent  = (ThisKey.Value - PrevKey.Value) / PrevTimeDiff;
			ThisKey.Tangent.ArriveTangent = NewTangent;
			
			const float NextTimeDiff = FMath::Max<double>(KINDA_SMALL_NUMBER, Times[Index + 1].Value - Times[Index].Value);
			NewTangent = (NextKey.Value - ThisKey.Value) / NextTimeDiff;
			ThisKey.Tangent.LeaveTangent = NewTangent;
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

void FMovieSceneFloatChannel::PopulateCurvePoints(double StartTimeSeconds, double EndTimeSeconds, double TimeThreshold, float ValueThreshold, FFrameRate InTickResolution, TArray<TTuple<double, double>>& InOutPoints) const
{
	const FFrameNumber StartFrame = (StartTimeSeconds * InTickResolution).FloorToFrame();
	const FFrameNumber EndFrame   = (EndTimeSeconds   * InTickResolution).CeilToFrame();

	const int32 StartingIndex = Algo::UpperBound(Times, StartFrame);
	const int32 EndingIndex   = Algo::LowerBound(Times, EndFrame);

	// Add the lower bound of the visible space
	float EvaluatedValue = 0.f;
	if (Evaluate(StartFrame, EvaluatedValue))
	{
		InOutPoints.Add(MakeTuple(StartFrame / InTickResolution, double(EvaluatedValue)));
	}

	// Add all keys in-between
	for (int32 KeyIndex = StartingIndex; KeyIndex < EndingIndex; ++KeyIndex)
	{
		InOutPoints.Add(MakeTuple(Times[KeyIndex] / InTickResolution, double(Values[KeyIndex].Value)));
	}

	// Add the upper bound of the visible space
	if (Evaluate(EndFrame, EvaluatedValue))
	{
		InOutPoints.Add(MakeTuple(EndFrame / InTickResolution, double(EvaluatedValue)));
	}

	int32 OldSize = InOutPoints.Num();
	do
	{
		OldSize = InOutPoints.Num();
		RefineCurvePoints(InTickResolution, TimeThreshold, ValueThreshold, InOutPoints);
	}
	while(OldSize != InOutPoints.Num());
}

void FMovieSceneFloatChannel::RefineCurvePoints(FFrameRate InTickResolution, double TimeThreshold, float ValueThreshold, TArray<TTuple<double, double>>& InOutPoints) const
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
				Evaluate(EvalTime * InTickResolution, Value);

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

void FMovieSceneFloatChannel::GetKeys(const TRange<FFrameNumber>& WithinRange, TArray<FFrameNumber>* OutKeyTimes, TArray<FKeyHandle>* OutKeyHandles)
{
	GetData().GetKeys(WithinRange, OutKeyTimes, OutKeyHandles);
}

void FMovieSceneFloatChannel::GetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<FFrameNumber> OutKeyTimes)
{
	GetData().GetKeyTimes(InHandles, OutKeyTimes);
}

void FMovieSceneFloatChannel::SetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<const FFrameNumber> InKeyTimes)
{
	GetData().SetKeyTimes(InHandles, InKeyTimes);
}

void FMovieSceneFloatChannel::DuplicateKeys(TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyHandle> OutNewHandles)
{
	GetData().DuplicateKeys(InHandles, OutNewHandles);
}

void FMovieSceneFloatChannel::DeleteKeys(TArrayView<const FKeyHandle> InHandles)
{
	GetData().DeleteKeys(InHandles);
}

void FMovieSceneFloatChannel::ChangeFrameResolution(FFrameRate SourceRate, FFrameRate DestinationRate)
{
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

TRange<FFrameNumber> FMovieSceneFloatChannel::ComputeEffectiveRange() const
{
	return GetData().GetTotalRange();
}

int32 FMovieSceneFloatChannel::GetNumKeys() const
{
	return Times.Num();
}

void FMovieSceneFloatChannel::Reset()
{
	Times.Reset();
	Values.Reset();
	KeyHandles.Reset();
	bHasDefaultValue = false;
}

void FMovieSceneFloatChannel::PostEditChange()
{
	AutoSetTangents();
}

void FMovieSceneFloatChannel::Offset(FFrameNumber DeltaPosition)
{
	GetData().Offset(DeltaPosition);
}

void FMovieSceneFloatChannel::Optimize(const FKeyDataOptimizationParams& Params)
{
	TMovieSceneChannelData<FMovieSceneFloatValue> ChannelData = GetData();
	TArray<FFrameNumber> OutKeyTimes;
	TArray<FKeyHandle> OutKeyHandles;

	GetKeys(Params.Range, &OutKeyTimes, &OutKeyHandles);

	if (OutKeyHandles.Num() > 2)
	{
		int32 MostRecentKeepKeyIndex = 0;
		TArray<FKeyHandle> KeysToRemove;

		for (int32 TestIndex = 1; TestIndex < OutKeyHandles.Num() - 1; ++TestIndex)
		{
			int32 Index = ChannelData.GetIndex(OutKeyHandles[TestIndex]);
			int32 NextIndex = ChannelData.GetIndex(OutKeyHandles[TestIndex+1]);

			const float KeyValue = ChannelData.GetValues()[Index].Value;
			const float ValueWithoutKey = EvalForTwoKeys(
				ChannelData.GetValues()[MostRecentKeepKeyIndex], ChannelData.GetTimes()[MostRecentKeepKeyIndex].Value,
				ChannelData.GetValues()[NextIndex], ChannelData.GetTimes()[NextIndex].Value,
				ChannelData.GetTimes()[Index].Value,
				Params.DisplayRate);
				
			if (FMath::Abs(ValueWithoutKey - KeyValue) > Params.Tolerance) // Is this key needed
			{
				MostRecentKeepKeyIndex = Index;
			}
			else
			{
				KeysToRemove.Add(OutKeyHandles[TestIndex]);
			}
		}

		ChannelData.DeleteKeys(KeysToRemove);

		if (Params.bAutoSetInterpolation)
		{
			AutoSetTangents();
		}
	}
}

void FMovieSceneFloatChannel::ClearDefault()
{
	bHasDefaultValue = false;
}

FKeyHandle AddKeyToChannel(FMovieSceneFloatChannel* Channel, FFrameNumber InFrameNumber, float InValue, EMovieSceneKeyInterpolation Interpolation)
{
	TMovieSceneChannelData<FMovieSceneFloatValue> ChannelData = Channel->GetData();
	int32 ExistingIndex = ChannelData.FindKey(InFrameNumber);
	if (ExistingIndex != INDEX_NONE)
	{
		FMovieSceneFloatValue& Value = ChannelData.GetValues()[ExistingIndex]; //-V758
		Value.Value = InValue;
	}
	else switch (Interpolation)
	{
		case EMovieSceneKeyInterpolation::Auto:     ExistingIndex = Channel->AddCubicKey(InFrameNumber, InValue, RCTM_Auto);  break;
		case EMovieSceneKeyInterpolation::User:     ExistingIndex = Channel->AddCubicKey(InFrameNumber, InValue, RCTM_User);  break;
		case EMovieSceneKeyInterpolation::Break:    ExistingIndex = Channel->AddCubicKey(InFrameNumber, InValue, RCTM_Break); break;
		case EMovieSceneKeyInterpolation::Linear:   ExistingIndex = Channel->AddLinearKey(InFrameNumber, InValue);            break;
		case EMovieSceneKeyInterpolation::Constant: ExistingIndex = Channel->AddConstantKey(InFrameNumber, InValue);          break;
	}

	return Channel->GetData().GetHandle(ExistingIndex);
}

void Dilate(FMovieSceneFloatChannel* InChannel, FFrameNumber Origin, float DilationFactor)
{
	TArrayView<FFrameNumber> Times = InChannel->GetData().GetTimes();
	for (FFrameNumber& Time : Times)
	{
		Time = Origin + FFrameNumber(FMath::FloorToInt((Time - Origin).Value * DilationFactor));
	}
	InChannel->AutoSetTangents();
}

void FMovieSceneFloatChannel::AddKeys(const TArray<FFrameNumber>& InTimes, const TArray<FMovieSceneFloatValue>& InValues)
{
	check(InTimes.Num() == InValues.Num());
	int32 Index = Times.Num();
	Times.Append(InTimes);
	Values.Append(InValues);
	for (; Index < Times.Num(); ++Index)
	{
		KeyHandles.AllocateHandle(Index);
	}
}

bool FMovieSceneFloatChannel::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FSequencerObjectVersion::GUID);
	return false;
}

void FMovieSceneFloatChannel::PostSerialize(const FArchive& Ar)
{
	if (Ar.CustomVer(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::ModifyLinearKeysForOldInterp)
	{
		bool bNeedAutoSetAlso = false;
		//we need to possibly modify cuvic tangents if we get a set of linear..cubic tangents so it works like it used to
		if (Values.Num() >= 2)
		{
			for (int32 Index = 1; Index < Values.Num(); ++Index)
			{
				FMovieSceneFloatValue  PrevKey = Values[Index - 1];
				FMovieSceneFloatValue& ThisKey = Values[Index];

				if (ThisKey.InterpMode == RCIM_Cubic && PrevKey.InterpMode == RCIM_Linear)
				{
					ThisKey.Tangent.TangentWeightMode = RCTWM_WeightedNone;
					ThisKey.TangentMode = RCTM_Break;
					//leave next tangent will be set up if auto or user, just need to modify prev.
					const float PrevTimeDiff = FMath::Max<double>(KINDA_SMALL_NUMBER, Times[Index].Value - Times[Index - 1].Value);
					float NewTangent = (ThisKey.Value - PrevKey.Value) / PrevTimeDiff;
					ThisKey.Tangent.ArriveTangent = NewTangent;
					bNeedAutoSetAlso = true;
				}
			}
		}
		if (bNeedAutoSetAlso)
		{
			AutoSetTangents();
		}
	}
}
