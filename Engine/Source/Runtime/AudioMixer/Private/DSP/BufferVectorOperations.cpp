// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DSP/BufferVectorOperations.h"

#define AUDIO_USE_SIMD 1

namespace Audio
{
	void BufferMultiplyByConstant(const AlignedFloatBuffer& InFloatBuffer, float InValue, AlignedFloatBuffer& OutFloatBuffer)
	{
		check(InFloatBuffer.Num() >= 4);

		// Prepare output buffer
		OutFloatBuffer.Reset();
		OutFloatBuffer.AddUninitialized(InFloatBuffer.Num());

		check(InFloatBuffer.Num() == OutFloatBuffer.Num());

		const int32 NumSamples = InFloatBuffer.Num();

		// Get ptrs to audio buffers to avoid bounds check in non-shipping builds
		const float* InBufferPtr = InFloatBuffer.GetData();
		float* OutBufferPtr = OutFloatBuffer.GetData();

#if !AUDIO_USE_SIMD
		for (int32 i = 0; i < NumSamples; ++i)
		{
			OutBufferPtr[i] = InValue * InBufferPtr[i];
		}
#else

		// Can only SIMD on multiple of 4 buffers, we'll do normal multiples on last bit
		const int32 NumSamplesRemaining = NumSamples % 4;
		const int32 NumSamplesToSimd = NumSamples - NumSamplesRemaining;

		// Load the single value we want to multiply all values by into a vector register
		const VectorRegister MultiplyValue = VectorLoadFloat1(&InValue);
		for (int32 i = 0; i < NumSamplesToSimd; i += 4)
		{
			// Load the next 4 samples of the input buffer into a register
			VectorRegister InputBufferRegister = VectorLoadAligned(&InBufferPtr[i]);

			// Perform the multiply
			VectorRegister Temp = VectorMultiply(InputBufferRegister, MultiplyValue);

			// Store results into the output buffer
			VectorStoreAligned(Temp, &OutBufferPtr[i]);
		}

		// Perform remaining non-simd values left over
		for (int32 i = 0; i < NumSamplesRemaining; ++i)
		{
			OutBufferPtr[NumSamplesToSimd + i] = InValue * InBufferPtr[NumSamplesToSimd + i];
		}
#endif
	}

	void FadeBufferFast(AlignedFloatBuffer& OutFloatBuffer, const float StartValue, const float EndValue)
	{
		FadeBufferFast(OutFloatBuffer.GetData(), OutFloatBuffer.Num(), StartValue, EndValue);
	}

	void FadeBufferFast(float* OutFloatBuffer, int32 NumSamples, const float StartValue, const float EndValue)
	{
		checkf(IsAligned<float*>(OutFloatBuffer, 4), TEXT("Memory must be aligned to use vector operations."));
		checkf(NumSamples % 4 == 0, TEXT("Please use a buffer size that is a multiple of 4."));

#if !AUDIO_USE_SIMD
		float Gain = StartValue;
		if (FMath::IsNearlyEqual(StartValue, EndValue))
		{
			// No need to do anything if start and end values are both 0.0
			if (StartValue == 0.0f)
			{
				FMemory::Memset(OutFloatBuffer, 0, sizeof(float)*NumSamples);
			}
			else
			{
				// Only need to do a buffer multiply if start and end values are the same
				for (int32 i = 0; i < NumSamples; ++i)
				{
					OutFloatBuffer[i] = OutFloatBuffer[i] * Gain;
				}
			}
		}
		else
		{
			// Do a fade from start to end
			const float DeltaValue = ((EndValue - StartValue) / NumSamples);
			for (int32 i = 0; i < NumSamples; ++i)
			{
				OutFloatBuffer[i] = OutFloatBuffer[i] * Gain;
				Gain += DeltaValue;
			}
		}
#else
		const int32 NumIterations = NumSamples / 4;

		if (FMath::IsNearlyEqual(StartValue, EndValue))
		{
			// No need to do anything if start and end values are both 0.0
			if (StartValue == 0.0f)
			{
				FMemory::Memset(OutFloatBuffer, 0, sizeof(float)*NumSamples);
			}
			else
			{
				// Only need to do a buffer multiply if start and end values are the same
				const float DeltaValue = ((EndValue - StartValue) / NumIterations);

				VectorRegister Gain = VectorLoadFloat1(&StartValue);

				for (int32 i = 0; i < NumSamples; i += 4)
				{
					VectorRegister Output = VectorLoadAligned(&OutFloatBuffer[i]);
					Output = VectorMultiply(Output, Gain);
					VectorStoreAligned(Output, &OutFloatBuffer[i]);
				}
			}
		}
		else
		{
			const float DeltaValue = ((EndValue - StartValue) / NumIterations);

			VectorRegister Gain = VectorLoadFloat1(&StartValue);
			VectorRegister Delta = VectorLoadFloat1(&DeltaValue);

			for (int32 i = 0; i < NumSamples; i += 4)
			{
				VectorRegister Output = VectorLoadAligned(&OutFloatBuffer[i]);
				Output = VectorMultiply(Output, Gain);
				Gain = VectorAdd(Gain, Delta);
				VectorStoreAligned(Output, &OutFloatBuffer[i]);
			}
		}
#endif
	}

	void MixInBufferFast(const AlignedFloatBuffer& InFloatBuffer, AlignedFloatBuffer& BufferToSumTo, const float Gain)
	{
		MixInBufferFast(InFloatBuffer.GetData(), BufferToSumTo.GetData(), InFloatBuffer.Num(), Gain);
	}

	void MixInBufferFast(const float* InFloatBuffer, float* BufferToSumTo, int32 NumSamples, const float Gain)
	{
		checkf(IsAligned<const float*>(InFloatBuffer, 4), TEXT("Memory must be aligned to use vector operations."));
		checkf(IsAligned<float*>(BufferToSumTo, 4), TEXT("Memory must be aligned to use vector operations."));
		checkf(NumSamples % 4 == 0, TEXT("Please use a buffer size that is a multiple of 4."));

#if !AUDIO_USE_SIMD
		for (int32 i = 0; i < NumSamples; ++i)
		{
			BufferToSumTo[i] += InFloatBuffer[i] * Gain;
		}
#else
		VectorRegister GainVector = VectorLoadFloat1(&Gain);

		for (int32 i = 0; i < NumSamples; i += 4)
		{
			VectorRegister Output = VectorLoadAligned(&BufferToSumTo[i]);
			VectorRegister Input  = VectorLoadAligned(&InFloatBuffer[i]);
			Output = VectorMultiplyAdd(Input, GainVector, Output);
			VectorStoreAligned(Output, &BufferToSumTo[i]);
		}
#endif
	}

	void MixInBufferFast(const float* InFloatBuffer, float* BufferToSumTo, int32 NumSamples)
	{
		checkf(IsAligned<const float*>(InFloatBuffer, 4), TEXT("Memory must be aligned to use vector operations."));
		checkf(IsAligned<float*>(BufferToSumTo, 4), TEXT("Memory must be aligned to use vector operations."));
		checkf(NumSamples % 4 == 0, TEXT("Please use a buffer size that is a multiple of 4."));

#if !AUDIO_USE_SIMD
		for (int32 i = 0; i < NumSamples; ++i)
		{
			BufferToSumTo[i] += InFloatBuffer[i];
		}
#else
		for (int32 i = 0; i < NumSamples; i += 4)
		{
			VectorRegister Output = VectorLoadAligned(&BufferToSumTo[i]);
			VectorRegister Input = VectorLoadAligned(&InFloatBuffer[i]);
			Output = VectorAdd(Input, Output);
			VectorStoreAligned(Output, &BufferToSumTo[i]);
		}
#endif
	}

	void SumBuffers(const AlignedFloatBuffer& InFloatBuffer1, const AlignedFloatBuffer& InFloatBuffer2, AlignedFloatBuffer& OutputBuffer)
	{
		SumBuffers(InFloatBuffer1.GetData(), InFloatBuffer2.GetData(), OutputBuffer.GetData(), OutputBuffer.Num());
	}

	void SumBuffers(const float* InFloatBuffer1, const float* InFloatBuffer2, float* OutputBuffer, int32 NumSamples)
	{
		checkf(IsAligned<const float*>(InFloatBuffer1, 4), TEXT("Memory must be aligned to use vector operations."));
		checkf(IsAligned<const float*>(InFloatBuffer2, 4), TEXT("Memory must be aligned to use vector operations."));
		checkf(IsAligned<float*>(OutputBuffer, 4), TEXT("Memory must be aligned to use vector operations."));
		checkf(NumSamples % 4 == 0, TEXT("Please use a buffer size that is a multiple of 4."));

#if !AUDIO_USE_SIMD
		for (int32 i = 0; i < NumSamples; ++i)
		{
			OutputBuffer[i] = InFloatBuffer1[i] + InFloatBuffer2[i];
		}
#else
		for (int32 i = 0; i < NumSamples; i += 4)
		{
			VectorRegister Input1 = VectorLoadAligned(&InFloatBuffer1[i]);
			VectorRegister Input2 = VectorLoadAligned(&InFloatBuffer2[i]);

			VectorRegister Output = VectorAdd(Input1, Input2);
			VectorStoreAligned(Output, &OutputBuffer[i]);
		}
#endif
	}

	float GetMagnitude(const AlignedFloatBuffer& Buffer)
	{
		return GetMagnitude(Buffer.GetData(), Buffer.Num());
	}

	float GetMagnitude(const float* Buffer, int32 NumSamples)
	{
		checkf(NumSamples % 4 == 0, TEXT("Please use a buffer size that is a multiple of 4."));

#if !AUDIO_USE_SIMD
		float Sum = 0.0f;
		for (int32 i = 0; i < NumSamples; ++i)
		{
			Sum += Buffer[i] * Buffer[i];
		}
		return FMath::Sqrt(Sum);
#else
		VectorRegister Sum = VectorZero();

		const float Exponent = 2.0f;
		VectorRegister ExponentVector = VectorLoadFloat1(&Exponent);

		for (int32 i = 0; i < NumSamples; i += 4)
		{
			VectorRegister Input = VectorPow(VectorLoadAligned(&Buffer[i]), ExponentVector);
			Sum = VectorAdd(Sum, Input);
		}

		float PartionedSums[4];
		VectorStoreAligned(Sum, PartionedSums);

		return FMath::Sqrt(PartionedSums[0] + PartionedSums[1] + PartionedSums[2] + PartionedSums[3]);
#endif
	}

	float GetAverageAmplitude(const AlignedFloatBuffer& Buffer)
	{
		checkf(Buffer.Num() % 4 == 0, TEXT("Please use a buffer size that is a multiple of 4."));

		return GetAverageAmplitude(Buffer.GetData(), Buffer.Num());
	}

	float GetAverageAmplitude(const float* Buffer, int32 NumSamples)
	{
		checkf(NumSamples % 4 == 0, TEXT("Please use a buffer size that is a multiple of 4."));

#if !AUDIO_USE_SIMD
		float Sum = 0.0f;
		for (int32 i = 0; i < NumSamples; ++i)
		{
			Sum += Buffer[i];
		}
		return Sum / NumSamples;
#else
		VectorRegister Sum = VectorZero();

		for (int32 i = 0; i < NumSamples; i += 4)
		{
			VectorRegister Input = VectorAbs(VectorLoadAligned(&Buffer[i]));
			Sum = VectorAdd(Sum, Input);
		}

		float PartionedSums[4];
		VectorStore(Sum, PartionedSums);
		
		return (PartionedSums[0] + PartionedSums[1] + PartionedSums[2] + PartionedSums[3]) / NumSamples;
#endif
	}
}
