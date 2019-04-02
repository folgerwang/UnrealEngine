// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DSP/AudioFFT.h"
#include "HAL/IConsoleManager.h"
#include "DSP/BufferVectorOperations.h"

#define IFFT_PRESERVE_COMPLEX_COMPONENT 0

static int32 FFTMethodCVar = 0;
TAutoConsoleVariable<int32> CVarFFTMethod(
	TEXT("au.dsp.FFTMethod"),
	FFTMethodCVar,
	TEXT("Determines whether we use an iterative FFT method or the DFT.\n")
	TEXT("0: Use Iterative FFT, 1:: Use DFT"),
	ECVF_Default);

namespace Audio
{

	void GenerateHammingWindow(float* WindowBuffer, int32 NumFrames, int32 NumChannels, bool bIsPeriodic)
	{
		const int32 N = bIsPeriodic ? NumFrames : NumFrames - 1;
		const float PhaseDelta = 2.0f * PI / N;
		float Phase = 0.0f;

		for (int32 FrameIndex = 0; FrameIndex < NumFrames; FrameIndex++)
		{
			const float Value = 0.54 -  0.46f * (1 - FMath::Cos(Phase));
			Phase += PhaseDelta;

			for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
			{
				WindowBuffer[FrameIndex * NumChannels + ChannelIndex] = Value;
			}
		}
	}

	void GenerateHannWindow(float* WindowBuffer, int32 NumFrames, int32 NumChannels, bool bIsPeriodic)
	{
		const int32 N = bIsPeriodic ? NumFrames : NumFrames - 1;
		const float PhaseDelta = 2.0f * PI / N;
		float Phase = 0.0f;

		for (int32 FrameIndex = 0; FrameIndex < NumFrames; FrameIndex++)
		{
			const float Value = 0.5f * (1 - FMath::Cos(Phase));
			Phase += PhaseDelta;

			for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
			{
				WindowBuffer[FrameIndex * NumChannels + ChannelIndex] = Value;
			}
		}
	}

	void GenerateBlackmanWindow(float* WindowBuffer, int32 NumFrames, int32 NumChannels, bool bIsPeriodic)
	{
		const int32 N = bIsPeriodic ? NumFrames : NumFrames - 1;
		const int32 Midpoint = (N % 2) ? (N + 1) / 2 : N / 2;

		const float PhaseDelta = 2.0f * PI / (N - 1);
		float Phase = 0.0f;

		// Generate the first half of the window:
		for (int32 FrameIndex = 0; FrameIndex <= Midpoint; FrameIndex++)
		{
			const float Value = 0.42f - 0.5 * FMath::Cos(Phase) + 0.08 * FMath::Cos(2 * Phase);
			Phase += PhaseDelta;

			for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
			{
				WindowBuffer[FrameIndex * NumChannels + ChannelIndex] = Value;
			}
		}

		// Flip first half for the second half of the window:
		for (int32 FrameIndex = Midpoint + 1; FrameIndex < NumFrames; FrameIndex++)
		{
			const float Value = WindowBuffer[Midpoint - (FrameIndex - Midpoint)];
			for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
			{
				WindowBuffer[FrameIndex * NumChannels + ChannelIndex] = Value;
			}
		}
	}

	uint32 GetCOLAHopSizeForWindow(EWindowType InType, uint32 WindowLength)
	{
		switch (InType)
		{
		case EWindowType::Hann:
		case EWindowType::Hamming:
			return FMath::FloorToInt((0.5f) * WindowLength);
			break;
		case EWindowType::Blackman:
			// Optimal overlap for any Blackman window is derived in this paper:
			// http://edoc.mpg.de/395068
			return FMath::FloorToInt((0.339f) * WindowLength);
			break;
		case EWindowType::None:
		default:
			return WindowLength;
			break;
		}
	}

	namespace FFTIntrinsics
	{
		// Fast bit reversal helper function. Can be used if N is a power of 2. Not well exercised.
		uint32 FastBitReversal(uint32 X, uint32 N)
		{
			// Num bits:
			uint32 NBit = N; // FMath::Log2(FFTSize);

			uint32 Mask = ~0;

			while ((NBit >>= 1) > 0)
			{
				Mask ^= (Mask << NBit);
				X = ((X >> (32u - N + NBit)) & Mask) | ((X << NBit) & ~Mask);
			}

			return X;
		}

		// Slow bit reversal helper function. performs bit reversal on an index, bit by bit. N is the number of bits (Log2(FFTSize))
		uint32 SlowBitReversal(uint32 X, uint32 N)
		{
			int32 ReversedX = X;
			int32 Count = N - 1;

			X >>= 1;
			while (X > 0)
			{
				ReversedX = (ReversedX << 1) | (X & 1);
				Count--;
				X >>= 1;
			}

			return ((ReversedX << Count) & ((1 << N) - 1));
		}

		// Alternate method for SlowBitReversal. Faster when N >= 7.
		uint32 SlowBitReversal2(uint32 X, uint32 N)
		{
			X = ReverseBits(X);
			return X >> (32 - N);
		}

		void ComplexMultiply(const float AReal, const float AImag, const float BReal, const float BImag, float& OutReal, float& OutImag)
		{
			OutReal = AReal * BReal - AImag * BImag;
			OutImag = AReal * BImag + AImag * BReal;
		}

		// Separates InBuffer (assumed to be mono here)
		void SeperateInPlace(float* InBuffer, uint32 NumSamples)
		{
			for (uint32 Index = 0; Index < NumSamples; Index++)
			{
				const uint32 NumBits = FMath::Log2(NumSamples);
				uint32 SwappedIndex = SlowBitReversal(Index, NumBits);
				if (Index < SwappedIndex)
				{
					Swap(InBuffer[Index], InBuffer[SwappedIndex]);
				}
			}
		}

		void SeparateIntoCopy(float* InBuffer, float* OutBuffer, uint32 NumSamples)
		{
			for (uint32 Index = 0; Index < NumSamples; Index++)
			{
				const uint32 NumBits = FMath::Log2(NumSamples);
				const uint32 ReversedIndex = SlowBitReversal2(Index, NumBits);
				OutBuffer[ReversedIndex] = InBuffer[Index];
			}
		}

		void ComputeButterfliesInPlace(float* OutReal, float* OutImag, uint32 NumSamples)
		{
			const uint32 LogNumSamples = FMath::Log2(NumSamples);

			for (uint32 S = 1; S <= LogNumSamples; S++)
			{
				const uint32 M = (1u << S);
				const uint32 M2 = M >> 1;

				// Initialize sinusoid.
				float OmegaReal = 1.0f;
				float OmegaImag = 0.0f;

				// Initialize W of M:
				float OmegaMReal = FMath::Cos(PI / M2);
				float OmegaMImag = FMath::Sin(PI / M2);

				for (uint32 j = 0; j < M2; j++)
				{
					for (uint32 k = j; k < NumSamples; k += M)
					{
						// Compute twiddle factor:
						float TwiddleReal, TwiddleImag;
						const uint32 TwiddleIndex = k + M2;

						ComplexMultiply(OmegaReal, OmegaImag, OutReal[TwiddleIndex], OutImag[TwiddleIndex], TwiddleReal, TwiddleImag);

						// Swap even and odd indices:

						float TempReal = OutReal[k];
						float TempImag = OutImag[k];

						OutReal[k] = TempReal + TwiddleReal;
						OutImag[k] = TempImag + TwiddleImag;

						OutReal[TwiddleIndex] = TempReal - TwiddleReal;
						OutImag[TwiddleIndex] = TempImag - TwiddleImag;
					}

					// Increment phase of W:
					ComplexMultiply(OmegaReal, OmegaImag, OmegaMReal, OmegaMImag, OmegaReal, OmegaImag);
				}
			}
		}

		void ComputeButterfliesInPlace2(float* OutReal, float* OutImag, int32 NumSamples)
		{
			for (int32 BitPosition = 2; BitPosition <= NumSamples; BitPosition <<= 1)
			{
				for (int32 I = 0; I < NumSamples; I += BitPosition)
				{
					for (int32 K = 0; K < (BitPosition / 2); K++)
					{
						int32 EvenIndex = I + K;
						int32 OddIndex = I + K + (BitPosition / 2);

						float EvenReal = OutReal[EvenIndex];
						float EvenImag = OutImag[EvenIndex];

						float OddReal = OutReal[OddIndex];
						float OddImag = OutImag[OddIndex];

						float Phase = -2.0f * PI * K / ((float)BitPosition);
						float TwiddleReal = FMath::Cos(Phase);
						float TwiddleImag = FMath::Sin(Phase);

						ComplexMultiply(TwiddleReal, TwiddleImag, OddReal, OddImag, TwiddleReal, TwiddleImag);

						// Swap even and odd indices:

						OutReal[EvenIndex] = EvenReal + TwiddleReal;
						OutImag[EvenIndex] = EvenImag + TwiddleImag;

						OutReal[OddIndex] = EvenReal - TwiddleReal;
						OutImag[OddIndex] = EvenImag - TwiddleImag;
					}
				}
			}
		}

		void PerformIterativeFFT(const FFTTimeDomainData& InputParams, FFTFreqDomainData& OutputParams)
		{
			// Separate even and odd elements into real buffer:
			SeparateIntoCopy(InputParams.Buffer, OutputParams.OutReal, InputParams.NumSamples);

			//Zero out imaginary buffer since the input signal is not complex:
			FMemory::Memzero(OutputParams.OutImag, InputParams.NumSamples * sizeof(float));

			// Iterate over and compute butterflies.
			ComputeButterfliesInPlace(OutputParams.OutReal, OutputParams.OutImag, InputParams.NumSamples);
		}

		void PerformIterativeIFFT(FFTFreqDomainData& InputParams, FFTTimeDomainData& OutputParams)
		{
			SeperateInPlace(InputParams.OutReal, OutputParams.NumSamples);
			SeperateInPlace(InputParams.OutImag, OutputParams.NumSamples);

			// IFFT can be done by performing a forward FFT on the complex conjugate of a frequency domain signal:
			MultiplyBufferByConstantInPlace(InputParams.OutImag, OutputParams.NumSamples, -1.0f);

			// Iterate over and compute butterflies.
			ComputeButterfliesInPlace(InputParams.OutReal, InputParams.OutImag, OutputParams.NumSamples);

#if IFFT_PRESERVE_COMPLEX_COMPONENT
			for (int32 Index = 0; Index < OutputParams.NumSamples; Index++)
			{
				const float Real = InputParams.OutReal[Index];
				const float Imag = InputParams.OutImag[Index];
				OutputParams.Buffer[Index] = FMath::Sqrt(Real * Real - Imag * Imag);
			}
#else
			FMemory::Memcpy(OutputParams.Buffer, InputParams.OutReal, OutputParams.NumSamples * sizeof(float));
			
			// Personal note: This is a very important step in an inverse FFT.
			Audio::MultiplyBufferByConstantInPlace(OutputParams.Buffer, OutputParams.NumSamples, 1.0f / OutputParams.NumSamples);
#endif
		}

		void PerformDFT(const FFTTimeDomainData& InputParams, FFTFreqDomainData& OutputParams)
		{
			const float* InputBuffer = InputParams.Buffer;
			float* OutReal = OutputParams.OutImag;
			float* OutImag = OutputParams.OutImag;

			float N = InputParams.NumSamples;

			for (int32 FreqIndex = 0; FreqIndex < InputParams.NumSamples; FreqIndex++)
			{
				float RealSum = 0.0f;
				float ImagSum = 0.0f;

				for (int32 TimeIndex = 0; TimeIndex < InputParams.NumSamples; TimeIndex++)
				{
					const float Exponent = FreqIndex * TimeIndex * PI * 2 / N;
					RealSum += InputBuffer[TimeIndex] * FMath::Cos(Exponent);
					ImagSum -= InputBuffer[TimeIndex] * FMath::Sin(Exponent);
				}

				OutReal[FreqIndex] = RealSum;
				OutImag[FreqIndex] = ImagSum;
			}
		}

		void PerformIDFT(const FFTFreqDomainData& InputParams, FFTTimeDomainData& OutputParams)
		{
			float* OutputBuffer = OutputParams.Buffer;
			float* InReal = InputParams.OutImag;
			float* InImag = InputParams.OutImag;

			float N = OutputParams.NumSamples;

			for (int32 TimeIndex = 0; TimeIndex < OutputParams.NumSamples; TimeIndex++)
			{
				float RealSum = 0.0f;
				float ImagSum = 0.0f;

				for (int32 FreqIndex = 0; FreqIndex < OutputParams.NumSamples; FreqIndex++)
				{
					const float Exponent = TimeIndex * FreqIndex * PI * 2 / N;
					RealSum += InReal[FreqIndex] * FMath::Cos(Exponent) - InImag[FreqIndex] * FMath::Sin(Exponent);
				}

				OutputBuffer[TimeIndex] = RealSum;
			}
		}
	}

	void PerformFFT(const FFTTimeDomainData& InputParams, FFTFreqDomainData& OutputParams)
	{
		int32 FFTMethod = CVarFFTMethod.GetValueOnAnyThread();
		if (FFTMethod)
		{
			FFTIntrinsics::PerformDFT(InputParams, OutputParams);
		}
		else
		{
			FFTIntrinsics::PerformIterativeFFT(InputParams, OutputParams);
		}
	}

	void PerformIFFT(FFTFreqDomainData& InputParams, FFTTimeDomainData& OutputParams)
	{
		int32 FFTMethod = CVarFFTMethod.GetValueOnAnyThread();
		if (FFTMethod)
		{
			FFTIntrinsics::PerformIDFT(InputParams, OutputParams);
		}
		else
		{
			FFTIntrinsics::PerformIterativeIFFT(InputParams, OutputParams);
		}
	}
}
