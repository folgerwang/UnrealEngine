// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DSP/SpectrumAnalyzer.h"


namespace Audio
{
	FSpectrumAnalyzer::FSpectrumAnalyzer()
		: CurrentSettings(SpectrumAnalyzerSettings::FSettings())
		, bSettingsWereUpdated(false)
		, SampleRate(0.0f)
		, Window(CurrentSettings.WindowType, (int32)CurrentSettings.FFTSize, 1, false)
		, InputQueue(FMath::Max((int32)CurrentSettings.FFTSize * 4, 4096))
		, FrequencyBuffer(CurrentSettings)
		, LockedFrequencyVector(nullptr)
	{
	}

	FSpectrumAnalyzer::FSpectrumAnalyzer(const SpectrumAnalyzerSettings::FSettings& InSettings, float InSampleRate)
		: CurrentSettings(InSettings)
		, bSettingsWereUpdated(false)
		, SampleRate(InSampleRate)
		, Window(InSettings.WindowType, (int32)InSettings.FFTSize, 1, false)
		, InputQueue(FMath::Max((int32)CurrentSettings.FFTSize * 4, 4096))
		, FrequencyBuffer(InSettings)
		, LockedFrequencyVector(nullptr)
	{
		ResetSettings();
	}

	FSpectrumAnalyzer::FSpectrumAnalyzer(float InSampleRate)
		: CurrentSettings(SpectrumAnalyzerSettings::FSettings())
		, bSettingsWereUpdated(false)
		, SampleRate(InSampleRate)
		, Window(CurrentSettings.WindowType, (int32)CurrentSettings.FFTSize, 1, false)
		, InputQueue(FMath::Max((int32)CurrentSettings.FFTSize * 4, 4096))
		, FrequencyBuffer(CurrentSettings)
		, LockedFrequencyVector(nullptr)
	{
		ResetSettings();
	}

	void FSpectrumAnalyzer::Init(float InSampleRate)
	{
		SpectrumAnalyzerSettings::FSettings DefaultSettings = SpectrumAnalyzerSettings::FSettings();
		Init(DefaultSettings, InSampleRate);
	}

	void FSpectrumAnalyzer::Init(const SpectrumAnalyzerSettings::FSettings& InSettings, float InSampleRate)
	{
		CurrentSettings = InSettings;
		bSettingsWereUpdated = false;
		SampleRate = InSampleRate;
		InputQueue.SetCapacity(FMath::Max((int32)CurrentSettings.FFTSize * 4, 4096));
		FrequencyBuffer.Reset(CurrentSettings);
		ResetSettings();
	}

	void FSpectrumAnalyzer::ResetSettings()
	{
		// If the game thread has locked a frequency vector, we can't resize our buffers under it.
		// Thus, wait until it's unlocked.
		if (LockedFrequencyVector != nullptr)
		{
			return;
		}

		Window = FWindow(CurrentSettings.WindowType, (int32)CurrentSettings.FFTSize, 1, false);
		FFTSize = (int32) CurrentSettings.FFTSize;
		HopInSamples = GetCOLAHopSizeForWindow(CurrentSettings.WindowType, (uint32)CurrentSettings.FFTSize);
		
		AnalysisTimeDomainBuffer.Reset();
		AnalysisTimeDomainBuffer.AddZeroed(FFTSize);

		FrequencyBuffer.Reset(CurrentSettings);
		bSettingsWereUpdated = false;
	}

	void FSpectrumAnalyzer::PerformInterpolation(const FSpectrumAnalyzerFrequencyVector* InFrequencies, SpectrumAnalyzerSettings::EPeakInterpolationMethod InMethod, const float InFreq, float& OutReal, float& OutImag)
	{
		const int32 VectorLength = InFrequencies->RealFrequencies.Num();
		const float NyquistPosition = VectorLength / 2;
		
		const float Nyquist = SampleRate / 2;

		// Fractional position in the frequency vector in terms of indices.
		// float Position = NyquistPosition + (InFreq / Nyquist);
		const float NormalizedFreq = (InFreq / Nyquist);
		float Position = InFreq >= 0 ? (NormalizedFreq * VectorLength / 2) : VectorLength - (NormalizedFreq * VectorLength / 2);
		check(Position >= 0.0f && Position <= VectorLength - 1);

		switch (InMethod)
		{
		case Audio::SpectrumAnalyzerSettings::EPeakInterpolationMethod::NearestNeighbor:
		{
			int32 Index = FMath::RoundToInt(Position);
			OutReal = InFrequencies->RealFrequencies[Index];
			OutImag = InFrequencies->ImagFrequencies[Index];
			break;
		}
			
		case Audio::SpectrumAnalyzerSettings::EPeakInterpolationMethod::Linear:
		{
			const int32 LowerIndex = FMath::FloorToInt(Position);
			const int32 UpperIndex = FMath::CeilToInt(Position);
			const float PositionFraction = Position - LowerIndex;

			const float y1Real = InFrequencies->RealFrequencies[LowerIndex];
			const float y2Real = InFrequencies->RealFrequencies[UpperIndex];
			OutReal = FMath::Lerp<float>(y1Real, y1Real, PositionFraction);

			const float y1Imag = InFrequencies->ImagFrequencies[LowerIndex];
			const float y2Imag = InFrequencies->ImagFrequencies[UpperIndex];
			OutImag = FMath::Lerp<float>(y1Imag, y2Imag, PositionFraction);
			break;
		}	
		case Audio::SpectrumAnalyzerSettings::EPeakInterpolationMethod::Quadratic:
		{
			const int32 MidIndex = FMath::RoundToInt(Position);
			const int32 LowerIndex = FMath::Max(0, MidIndex - 1);
			const int32 UpperIndex = FMath::Min(VectorLength, MidIndex + 1);
			const float PositionFraction = Position - LowerIndex;

			const float y1Real = InFrequencies->RealFrequencies[LowerIndex];
			const float y2Real = InFrequencies->RealFrequencies[MidIndex];
			const float y3Real = InFrequencies->RealFrequencies[UpperIndex];

			const float InterpReal = (y3Real - y1Real) / (2 * (2 * y2Real - y1Real - y3Real));
			
			OutReal = InterpReal;

			const float y1Imag = InFrequencies->ImagFrequencies[LowerIndex];
			const float y2Imag = InFrequencies->ImagFrequencies[MidIndex];
			const float y3Imag = InFrequencies->ImagFrequencies[UpperIndex];
			const float InterpImag = (y3Imag - y1Imag) / (2 * (2 * y2Imag - y1Imag - y3Imag));

			OutImag = InterpImag;
			break;
		}
			
		default:
			break;
		}
	}

	void FSpectrumAnalyzer::SetSettings(const SpectrumAnalyzerSettings::FSettings& InSettings)
	{
		CurrentSettings = InSettings;
		bSettingsWereUpdated = true;
	}

	void FSpectrumAnalyzer::GetSettings(SpectrumAnalyzerSettings::FSettings& OutSettings)
	{
		OutSettings = CurrentSettings;
	}

	float FSpectrumAnalyzer::GetMagnitudeForFrequency(float InFrequency)
	{
		const FSpectrumAnalyzerFrequencyVector* OutVector;
		bool bShouldUnlockBuffer = true;

		if (LockedFrequencyVector)
		{
			OutVector = LockedFrequencyVector;
			bShouldUnlockBuffer = false;
		}
		else
		{
			OutVector = FrequencyBuffer.LockMostRecentBuffer();
		}

		// Perform work.
		if (OutVector)
		{
			float OutMagnitude = 0.0f;

			float InterpolatedReal, InterpolatedImag;
			PerformInterpolation(OutVector, CurrentSettings.InterpolationMethod, InFrequency, InterpolatedReal, InterpolatedImag);

			OutMagnitude = FMath::Sqrt((InterpolatedReal * InterpolatedReal) + (InterpolatedImag * InterpolatedImag));

			if (bShouldUnlockBuffer)
			{
				FrequencyBuffer.UnlockBuffer();
			}

			return OutMagnitude;
		}

		// If we got here, something went wrong, so just output zero.
		return 0.0f;
	}

	float FSpectrumAnalyzer::GetPhaseForFrequency(float InFrequency)
	{
		const FSpectrumAnalyzerFrequencyVector* OutVector;
		bool bShouldUnlockBuffer = true;

		if (LockedFrequencyVector)
		{
			OutVector = LockedFrequencyVector;
			bShouldUnlockBuffer = false;
		}
		else
		{
			OutVector = FrequencyBuffer.LockMostRecentBuffer();
		}

		// Perform work.
		if (OutVector)
		{
			float OutPhase = 0.0f;

			float InterpolatedReal, InterpolatedImag;
			PerformInterpolation(OutVector, CurrentSettings.InterpolationMethod, InFrequency, InterpolatedReal, InterpolatedImag);

			OutPhase = FMath::Atan2(InterpolatedImag, InterpolatedReal);

			if (bShouldUnlockBuffer)
			{
				FrequencyBuffer.UnlockBuffer();
			}

			return OutPhase;
		}

		// If we got here, something went wrong, so just output zero.
		return 0.0f;
	}

	void FSpectrumAnalyzer::LockOutputBuffer()
	{
		if (LockedFrequencyVector != nullptr)
		{
			FrequencyBuffer.UnlockBuffer();
		}

		LockedFrequencyVector = FrequencyBuffer.LockMostRecentBuffer();
	}

	void FSpectrumAnalyzer::UnlockOutputBuffer()
	{
		if (LockedFrequencyVector != nullptr)
		{
			FrequencyBuffer.UnlockBuffer();
			LockedFrequencyVector = nullptr;
		}
	}

	bool FSpectrumAnalyzer::PushAudio(const TSampleBuffer<float>& InBuffer)
	{
		check(InBuffer.GetNumChannels() == 1);
		return PushAudio(InBuffer.GetData(), InBuffer.GetNumSamples());
	}

	bool FSpectrumAnalyzer::PushAudio(const float* InBuffer, int32 NumSamples)
	{
		return InputQueue.Push(InBuffer, NumSamples) > 0;
	}

	bool FSpectrumAnalyzer::PerformAnalysisIfPossible()
	{
		// If settings were updated, perform resizing and parameter updates here:
		if (bSettingsWereUpdated)
		{
			ResetSettings();
		}

		FSpectrumAnalyzerFrequencyVector* OutputVector = FrequencyBuffer.StartWorkOnBuffer();

		// If we have enough audio pushed to the spectrum analyzer and we have an available buffer to work in,
		// we can start analyzing.
		if (InputQueue.Num() >= ((uint32)FFTSize) && OutputVector)
		{
			float* TimeDomainBuffer = AnalysisTimeDomainBuffer.GetData();

			// Perform pop/peek here based on FFT size and hop amount.
			const int32 PeekAmount = FFTSize - HopInSamples;
			InputQueue.Pop(TimeDomainBuffer, HopInSamples);
			InputQueue.Peek(TimeDomainBuffer + HopInSamples, PeekAmount);

			// apply window if necessary.
			Window.ApplyToBuffer(TimeDomainBuffer);

			// Perform FFT.
			FFTInputParams InputParams;
			InputParams.InBuffer = TimeDomainBuffer;
			InputParams.NumSamples = FFTSize;

			FFTOutputParams OutputParams;
			OutputParams.OutReal = OutputVector->RealFrequencies.GetData();
			OutputParams.OutImag = OutputVector->ImagFrequencies.GetData();


			PerformFFT(InputParams, OutputParams);

			// We're done, so unlock this vector.
			FrequencyBuffer.StopWorkOnBuffer();

			return true;
		}
		else
		{
			return false;
		}
	}

	static const int32 SpectrumAnalyzerBufferSize = 3;

	FSpectrumAnalyzerBuffer::FSpectrumAnalyzerBuffer()
		: OutputIndex(0)
		, InputIndex(0)
	{
	}

	FSpectrumAnalyzerBuffer::FSpectrumAnalyzerBuffer(const SpectrumAnalyzerSettings::FSettings& InSettings)
	{
		Reset(InSettings);
	}

	void FSpectrumAnalyzerBuffer::Reset(const SpectrumAnalyzerSettings::FSettings& InSettings)
	{
		FScopeLock ScopeLock(&BufferIndicesCriticalSection);

		static_assert(SpectrumAnalyzerBufferSize > 2, "Please ensure that SpectrumAnalyzerBufferSize is greater than 2.");
		
		FrequencyVectors.Reset();

		for (int32 Index = 0; Index < SpectrumAnalyzerBufferSize; Index++)
		{
			FrequencyVectors.Emplace((int32)InSettings.FFTSize);
		}

		InputIndex = 1;
		OutputIndex = 0;
	}

	void FSpectrumAnalyzerBuffer::IncrementInputIndex()
	{
		FScopeLock ScopeLock(&BufferIndicesCriticalSection);

		InputIndex = (InputIndex + 1) % SpectrumAnalyzerBufferSize;
		if (InputIndex == OutputIndex)
		{
			InputIndex = (InputIndex + 1) % SpectrumAnalyzerBufferSize;
		}

		check(InputIndex != OutputIndex);
	}

	void FSpectrumAnalyzerBuffer::IncrementOutputIndex()
	{
		FScopeLock ScopeLock(&BufferIndicesCriticalSection);

		OutputIndex = (OutputIndex + 1) % SpectrumAnalyzerBufferSize;
		if (InputIndex == OutputIndex)
		{
			OutputIndex = (OutputIndex + 1) % SpectrumAnalyzerBufferSize;
		}

		check(InputIndex != OutputIndex);
	}

	FSpectrumAnalyzerFrequencyVector* FSpectrumAnalyzerBuffer::StartWorkOnBuffer()
	{
		return &(FrequencyVectors[InputIndex]);
	}

	void FSpectrumAnalyzerBuffer::StopWorkOnBuffer()
	{
		IncrementInputIndex();
	}

	const FSpectrumAnalyzerFrequencyVector* FSpectrumAnalyzerBuffer::LockMostRecentBuffer()
	{
		return &(FrequencyVectors[OutputIndex]);
	}

	void FSpectrumAnalyzerBuffer::UnlockBuffer()
	{
		IncrementOutputIndex();
	}

	FSpectrumAnalyzerFrequencyVector::FSpectrumAnalyzerFrequencyVector(int32 InFFTSize)
	{
		RealFrequencies.Reset();
		RealFrequencies.AddZeroed(InFFTSize);

		ImagFrequencies.Reset();
		ImagFrequencies.AddZeroed(InFFTSize);
	}
}
