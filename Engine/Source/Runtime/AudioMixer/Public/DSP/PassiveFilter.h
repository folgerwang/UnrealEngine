// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/AudioFFT.h"
#include "Async/ParallelFor.h"

namespace Audio
{
	struct PassiveFilterParams
	{
		enum class EClass
		{
			Butterworth,
			Chebyshev
		};

		enum class EType
		{
			Lowpass,
			Highpass
		};

		EClass Class;
		EType Type;

		int32 Order;
		float NormalizedCutoffFrequency;
		float UnitGain;

		bool bRemoveDC;

		PassiveFilterParams()
			: Class(EClass::Butterworth)
			, Type(EType::Lowpass)
			, Order(4)
			, NormalizedCutoffFrequency(0.8f)
			, UnitGain(1.0f)
			, bRemoveDC(false)
		{
		}
	};

	static float EvaluateChebyshevPolynomial(float FrequencyRatio, int32 Order)
	{
		if (FMath::IsNearlyEqual(FrequencyRatio, 1.0f))
		{
			return 1.0f;
		}

		switch (Order)
		{
		case 0:
		{
			return 1.0f;
		}
		case 1:
		{
			return FrequencyRatio;
		}
		case 2:
		{
			return 2.0f * FrequencyRatio * FrequencyRatio - 1.0f;
		}
		default:
		{
			// Rather than recursing,
			// we perform an iterative chebyshev polynomial evaluation,
			// since we are likely deep in the stack already:
			float Temp0 = 2.0f * FrequencyRatio * FrequencyRatio - 1.0f;
			float Temp1 = FrequencyRatio;
			
			// Start at an order of 2:
			float Result = Temp0;

			for (int32 CurrentOrder = 3; CurrentOrder <= Order; CurrentOrder++)
			{
				Result = (2.0f * FrequencyRatio * Temp0) - Temp1;
				Temp1 = Temp0;
				Temp0 = Result;
			}

			return Result;
		}
			break;
		}
	}

	static float GetGainForFrequency(float NormalizedFreq, const PassiveFilterParams& InParams)
	{
		const float FrequencyRatio = (InParams.Type == PassiveFilterParams::EType::Lowpass) ? (NormalizedFreq / InParams.NormalizedCutoffFrequency) : (InParams.NormalizedCutoffFrequency / NormalizedFreq);

		switch (InParams.Class)
		{
		case PassiveFilterParams::EClass::Chebyshev:
		{
			const float ChebyshevPolynomial = EvaluateChebyshevPolynomial(FrequencyRatio, InParams.Order);
			return InParams.UnitGain / (FMath::Sqrt(1.0f + ChebyshevPolynomial * ChebyshevPolynomial));
			break;
		}
		default:
		case PassiveFilterParams::EClass::Butterworth:
		{
			const float Denominator = FMath::Sqrt((1 + FMath::Pow(FrequencyRatio, InParams.Order * 2.0f)));
			return InParams.UnitGain / Denominator;
			break;
		}
		}
	}

	/**
	* This can be called on any TArrayView<float> whose length is a power of 2.
	*/
	static void Filter(TArrayView<float>& Signal, const PassiveFilterParams& InParams)
	{
		if (!FMath::IsPowerOfTwo(Signal.Num()))
		{
			UE_LOG(LogAudio, Error, TEXT("Error in Filter: if using TArrayView<float>, Signal's length should be power of 2."));
			return;
		}

		AlignedFloatBuffer TempReal;
		AlignedFloatBuffer TempImag;
		TempReal.AddUninitialized(Signal.Num());
		TempImag.AddUninitialized(Signal.Num());

		// Perform FFT on data:
		FFTTimeDomainData TimeData;
		TimeData.Buffer = Signal.GetData();
		TimeData.NumSamples = Signal.Num();

		FFTFreqDomainData FreqData;
		FreqData.OutReal = TempReal.GetData();
		FreqData.OutImag = TempImag.GetData();

		PerformFFT(TimeData, FreqData);

		const int32 NumSamples = Signal.Num();
		const float NumBins = NumSamples / 2;

		if (InParams.bRemoveDC)
		{
			FreqData.OutReal[0] = 0.0f;
			FreqData.OutImag[0] = 0.0f;
		}

		// Apply filter in parallel:
		ParallelFor(NumBins, [&](int32 Index)
		{
			float NormalizedFreq = ((float)Index) / NumBins;
			float Gain = GetGainForFrequency(NormalizedFreq, InParams);

			FreqData.OutReal[Index] *= Gain;
			FreqData.OutImag[Index] *= Gain;

			FreqData.OutReal[NumSamples - Index] *= Gain;
			FreqData.OutImag[NumSamples - Index] *= Gain;
		});

		// Inverse FFT back into the signal:
		PerformIFFT(FreqData, TimeData);
	}

	/**
	* Static function for applying a filter to any time series.
	*/
	static void Filter(TArray<float>& Signal, const PassiveFilterParams& InParams)
	{
		if (FMath::IsPowerOfTwo(Signal.Num()))
		{
			TArrayView<float> SignalView(Signal);
			Filter(SignalView, InParams);
		}
		else
		{
			const int32 OriginalLength = Signal.Num();
			const int32 NumZerosRequired = FMath::RoundUpToPowerOfTwo(OriginalLength) - OriginalLength;
			Signal.AddZeroed(NumZerosRequired);
			TArrayView<float> SignalView(Signal);
			Filter(SignalView, InParams);
			Signal.SetNum(OriginalLength);
		}
	}
}
