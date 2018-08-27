// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "TimecodeSynchronizer.h"
#include "TimecodeSynchronizerModule.h"

#include "Engine/Engine.h"
#include "FixedFrameRateCustomTimeStep.h"
#include "IMediaModule.h"
#include "ITimeManagementModule.h"
#include "Misc/App.h"

#define LOCTEXT_NAMESPACE "TimecodeSynchronizer"

/**
 * FTimecodeSynchronizerActiveTimecodedInputSource
 */

void FTimecodeSynchronizerActiveTimecodedInputSource::ConvertToLocalFrameRate(const FFrameRate& InLocalFrameRate)
{
	const FFrameTime MaxSampleTime = NextSampleTime + AvailableSampleCount;
	NextSampleLocalTime = FFrameRate::TransformTime(NextSampleTime, FrameRate, InLocalFrameRate);
	MaxSampleLocalTime = FFrameRate::TransformTime(MaxSampleTime, FrameRate, InLocalFrameRate);
}

/**
 * UTimecodeSynchronizer
 */

UTimecodeSynchronizer::UTimecodeSynchronizer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bUseCustomTimeStep(false)
	, CustomTimeStep(nullptr)
	, FixedFrameRate(30, 1)
	, TimecodeProviderType(ETimecodeSynchronizationTimecodeType::SystemTime)
	, TimecodeProvider(nullptr)
	, MasterSynchronizationSourceIndex(INDEX_NONE)
	, PreRollingTimecodeMarginOfErrors(4)
	, PreRollingTimeout(30.f)
	, State(ESynchronizationState::None)
	, CurrentFrameTime(0)
	, StartPreRollingTime(0.0)
	, bRegistered(false)
	, PreviousFixedFrameRate(0.f)
	, bPreviousUseFixedFrameRate(false)
	, ActiveMasterSynchronizationTimecodedSourceIndex(INDEX_NONE)
{
}

void UTimecodeSynchronizer::BeginDestroy()
{
	Super::BeginDestroy();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		Unregister();
	}
}

#if WITH_EDITOR
bool UTimecodeSynchronizer::CanEditChange(const UProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UTimecodeSynchronizer, TimecodeProvider))
	{
		return TimecodeProviderType == ETimecodeSynchronizationTimecodeType::TimecodeProvider;
	}
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UTimecodeSynchronizer, MasterSynchronizationSourceIndex))
	{
		return TimecodeProviderType == ETimecodeSynchronizationTimecodeType::InputSource;
	}

	return true;
}

void UTimecodeSynchronizer::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	// Make sure the master source index is valid
	if (TimecodeProviderType == ETimecodeSynchronizationTimecodeType::InputSource)
	{
		if (!TimeSynchronizationInputSources.IsValidIndex(MasterSynchronizationSourceIndex)
			|| TimeSynchronizationInputSources[MasterSynchronizationSourceIndex] == nullptr
			|| !TimeSynchronizationInputSources[MasterSynchronizationSourceIndex]->bUseForSynchronization)
		{
			MasterSynchronizationSourceIndex = INDEX_NONE;
			UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("The MasterSynchronizationSourceIndex is not valid."));
		}
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
#endif

FFrameTime UTimecodeSynchronizer::ConvertTimecodeToFrameTime(const FTimecode& InTimecode) const
{
	return InTimecode.ToFrameNumber(GetFrameRate());
}

FTimecode UTimecodeSynchronizer::ConvertFrameTimeToTimecode(const FFrameTime& InFFrameTime) const
{
	const bool bIsDropFrame = FTimecode::IsDropFormatTimecodeSupported(GetFrameRate());
	return FTimecode::FromFrameNumber(InFFrameTime.FrameNumber, GetFrameRate(), bIsDropFrame);
}

FTimecode UTimecodeSynchronizer::GetTimecode() const
{
	if(TimecodeProviderType == ETimecodeSynchronizationTimecodeType::InputSource)
	{
		if(ActiveTimecodedInputSources.IsValidIndex(ActiveMasterSynchronizationTimecodedSourceIndex))
		{
			FTimecodeSynchronizerActiveTimecodedInputSource TimecodedInputSource = ActiveTimecodedInputSources[ActiveMasterSynchronizationTimecodedSourceIndex];
			FFrameTime NextSampleTime = TimecodedInputSource.InputSource->GetNextSampleTime();
			if (NextSampleTime != 0)
			{
				TimecodedInputSource.NextSampleTime = NextSampleTime;
				TimecodedInputSource.AvailableSampleCount = TimecodedInputSource.InputSource->GetAvailableSampleCount();
				TimecodedInputSource.ConvertToLocalFrameRate(GetFrameRate());
			}
			return ConvertFrameTimeToTimecode(TimecodedInputSource.MaxSampleLocalTime);
		}
	}
	else if(TimecodeProviderType == ETimecodeSynchronizationTimecodeType::TimecodeProvider)
	{
		if (RegisteredTimecodeProvider)
		{
			return RegisteredTimecodeProvider->GetTimecode();
		}
	}

	FTimecode Result = UTimecodeProvider::GetSystemTimeTimecode(GetFrameRate());
	return Result;
}

FFrameRate UTimecodeSynchronizer::GetFrameRate() const
{
	return bUseCustomTimeStep && CustomTimeStep ? CustomTimeStep->GetFixedFrameRate() : FixedFrameRate;
}

ETimecodeProviderSynchronizationState UTimecodeSynchronizer::GetSynchronizationState() const
{
	switch(State)
	{
		case ESynchronizationState::PreRolling_WaitGenlockTimecodeProvider:
		case ESynchronizationState::PreRolling_WaitReadiness:
		case ESynchronizationState::PreRolling_Synchronizing:
		case ESynchronizationState::PreRolling_Buffering:
			return ETimecodeProviderSynchronizationState::Synchronizing;
		case ESynchronizationState::Synchronized:
		case ESynchronizationState::Rolling:
			return ETimecodeProviderSynchronizationState::Synchronized;
		case ESynchronizationState::Error:
			return ETimecodeProviderSynchronizationState::Error;
	}
	return ETimecodeProviderSynchronizationState::Closed;
}

bool UTimecodeSynchronizer::IsSynchronizing() const
{
	return State == ESynchronizationState::PreRolling_WaitGenlockTimecodeProvider
		|| State == ESynchronizationState::PreRolling_WaitReadiness
		|| State == ESynchronizationState::PreRolling_Synchronizing
		|| State == ESynchronizationState::PreRolling_Buffering;
}

bool UTimecodeSynchronizer::IsSynchronized() const
{
	return State == ESynchronizationState::Synchronized
		|| State == ESynchronizationState::Rolling;
}

void UTimecodeSynchronizer::Register()
{
	// Set CustomTimeStep
	bRegistered = false;

	if (bUseCustomTimeStep)
	{
		if (GEngine->GetCustomTimeStep())
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("Genlock source is already in place."));
			SwitchState(ESynchronizationState::Error);
		}

		if (!CustomTimeStep)
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("The Genlock source is not set."));
			SwitchState(ESynchronizationState::Error);
			return;
		}

		if (!GEngine->SetCustomTimeStep(CustomTimeStep))
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("The Genlock source failed to be set on Engine."));
			SwitchState(ESynchronizationState::Error);
			return;
		}
		RegisteredCustomTimeStep = CustomTimeStep;
	}
	else
	{
		PreviousFixedFrameRate = GEngine->FixedFrameRate;
		bPreviousUseFixedFrameRate = GEngine->bUseFixedFrameRate;
		GEngine->FixedFrameRate = FixedFrameRate.AsDecimal();
		GEngine->bUseFixedFrameRate = true;
	}

	// Set TimecodeProvider
	if (GEngine->GetTimecodeProvider())
	{
		UE_LOG(LogTimecodeSynchronizer, Error, TEXT("A Timecode Provider is already in place."));
		SwitchState(ESynchronizationState::Error);
	}
	else if (TimecodeProviderType == ETimecodeSynchronizationTimecodeType::TimecodeProvider)
	{
		if (!TimecodeProvider)
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("TimecodeProvider is not set."));
			SwitchState(ESynchronizationState::Error);
			return;
		}

		if (!GEngine->SetTimecodeProvider(TimecodeProvider))
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("TimecodeProvider failed to be set on Engine."));
			SwitchState(ESynchronizationState::Error);
			return;
		}
		RegisteredTimecodeProvider = TimecodeProvider;
	}
	else
	{
		if (!GEngine->SetTimecodeProvider(this))
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("TimecodeSynchronizer failed to be set as the TimecodeProvider for the Engine."));
			SwitchState(ESynchronizationState::Error);
			return;
		}
		RegisteredTimecodeProvider = this;
	}

	bRegistered = true;
	SetTickEnabled(bRegistered);
}

void UTimecodeSynchronizer::Unregister()
{
	UTimecodeProvider* Provider = GEngine->GetTimecodeProvider();
	if (Provider == RegisteredTimecodeProvider)
	{
		GEngine->SetTimecodeProvider(nullptr);
	}
	RegisteredTimecodeProvider = nullptr;

	UEngineCustomTimeStep* TimeStep = GEngine->GetCustomTimeStep();
	if (TimeStep == RegisteredCustomTimeStep)
	{
		GEngine->SetCustomTimeStep(nullptr);
	}
	else if (RegisteredCustomTimeStep == nullptr)
	{
		GEngine->FixedFrameRate = PreviousFixedFrameRate;
		GEngine->bUseFixedFrameRate = bPreviousUseFixedFrameRate;
	}
	RegisteredCustomTimeStep = nullptr;

	bRegistered = false;
	SetTickEnabled(bRegistered);
}

void UTimecodeSynchronizer::SetTickEnabled(bool bEnabled)
{
	IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");
	if (MediaModule == nullptr)
	{
		UE_LOG(LogTimecodeSynchronizer, Error, TEXT("The 'Media' module couldn't be loaded"));
		SwitchState(ESynchronizationState::Error);
		return;
	}

	MediaModule->GetOnTickPreEngineCompleted().RemoveAll(this);
	if (bEnabled)
	{
		MediaModule->GetOnTickPreEngineCompleted().AddUObject(this, &UTimecodeSynchronizer::Tick);
	}
}

void UTimecodeSynchronizer::Tick()
{
	Tick_Switch();

	if (IsSynchronizing() && bUsePreRollingTimeout)
	{
		const double TimeSinceStarted = FApp::GetCurrentTime() - StartPreRollingTime;
		if (TimeSinceStarted > PreRollingTimeout)
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("PreRoll Timeout."));
			SwitchState(ESynchronizationState::Error);
		}
	}
}

bool UTimecodeSynchronizer::StartPreRoll()
{
	if (IsSynchronizing() || IsSynchronized())
	{
		UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("Already synchronizing or synchronized."));
		return false;
	}
	else
	{
		StopInputSources();

		ActiveMasterSynchronizationTimecodedSourceIndex = INDEX_NONE;
	
		// Go through all sources and select usable ones
		for (int32 Index = 0; Index < TimeSynchronizationInputSources.Num(); ++Index)
		{
			UTimeSynchronizationSource* InputSource = TimeSynchronizationInputSources[Index];
			if (InputSource)
			{
				if (InputSource->bUseForSynchronization && InputSource->Open())
				{
					const int32 NewItemIndex = ActiveTimecodedInputSources.AddDefaulted();
					FTimecodeSynchronizerActiveTimecodedInputSource& NewSource = ActiveTimecodedInputSources[NewItemIndex];
					NewSource.InputSource = InputSource;

					if (TimecodeProviderType == ETimecodeSynchronizationTimecodeType::InputSource && Index == MasterSynchronizationSourceIndex)
					{
						ActiveMasterSynchronizationTimecodedSourceIndex = NewItemIndex;
					}
				}
				else if (!InputSource->bUseForSynchronization && InputSource->Open())
				{
					const int32 NewItemIndex = ActiveSynchronizedSources.AddDefaulted();
					FTimecodeSynchronizerActiveTimecodedInputSource& NewSource = ActiveSynchronizedSources[NewItemIndex];
					NewSource.InputSource = InputSource;

					//Stamp source FrameRate for time conversion
					NewSource.FrameRate = InputSource->GetFrameRate();
					NewSource.bCanBeSynchronized = false;
				}
			}
		}

		if (TimecodeProviderType == ETimecodeSynchronizationTimecodeType::InputSource && ActiveMasterSynchronizationTimecodedSourceIndex == INDEX_NONE)
		{
			UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("The Master Synchronization Source could not be found."));
		}

		if (ActiveTimecodedInputSources.Num() > 0)
		{
			Register();
		}

		//Engage synchronization procedure only if we've successfully
		if (bRegistered)
		{
			const bool bDoTick = true;
			SwitchState(ESynchronizationState::PreRolling_WaitGenlockTimecodeProvider, bDoTick);
		}
		else
		{
			StopInputSources();

			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("Couldn't start preroll. TimecodeSynchronizer is not registered. (Maybe there is no input sources)"));
			SwitchState(ESynchronizationState::Error);
		}

		return bRegistered;
	}
}

void UTimecodeSynchronizer::StopInputSources()
{
	Unregister();
	for (FTimecodeSynchronizerActiveTimecodedInputSource& TimecodedInputSource : ActiveTimecodedInputSources)
	{
		if (TimecodedInputSource.InputSource)
		{
			TimecodedInputSource.InputSource->Close();
		}
	}

	for (FTimecodeSynchronizerActiveTimecodedInputSource& SynchronizedInputSource : ActiveSynchronizedSources)
	{
		if (SynchronizedInputSource.InputSource)
		{
			SynchronizedInputSource.InputSource->Close();
		}
	}

	CurrentFrameTime = FFrameTime(0);
	ActiveTimecodedInputSources.Reset();
	ActiveSynchronizedSources.Reset();
	SwitchState(ESynchronizationState::None);
	ActiveMasterSynchronizationTimecodedSourceIndex = INDEX_NONE;
}

void UTimecodeSynchronizer::SwitchState(const ESynchronizationState NewState, const bool bDoTick)
{
	if (NewState != State)
	{
		State = NewState;

		//Do State entering procedure and tick if required
		switch (NewState)
		{
		case ESynchronizationState::None:
		{
			break;
		}
		case ESynchronizationState::PreRolling_WaitGenlockTimecodeProvider:
		{
			StartPreRollingTime = FApp::GetCurrentTime();
			SynchronizationEvent.Broadcast(ETimecodeSynchronizationEvent::SynchronizationStarted);
			if (bDoTick)
			{
				TickPreRolling_WaitGenlockTimecodeProvider();
			}
			break;
		}
		case ESynchronizationState::PreRolling_WaitReadiness:
		{
			if (bDoTick)
			{
				TickPreRolling_WaitReadiness();
			}
			break;
		}
		case ESynchronizationState::PreRolling_Synchronizing:
		{
			if (bDoTick)
			{
				TickPreRolling_Synchronizing();
			}
			break;
		}
		case ESynchronizationState::PreRolling_Buffering:
		{
			if (bDoTick)
			{
				TickPreRolling_Buffering();
			}
			break;
		}
		case ESynchronizationState::Synchronized:
		{
			bSourceStarted = false;
			SynchronizationEvent.Broadcast(ETimecodeSynchronizationEvent::SynchronizationSucceeded);
			if (bDoTick)
			{
				TickSynchronized();
			}
			break;
		}
		case ESynchronizationState::Error:
		{
			EnterStateError();
			if (bDoTick)
			{
				TickError();
			}
			break;
		}
		default:
		{
			SetTickEnabled(false);
			break;
		}
		};
	}
}

void UTimecodeSynchronizer::Tick_Switch()
{
	switch (State)
	{
	case ESynchronizationState::PreRolling_WaitGenlockTimecodeProvider:
		TickPreRolling_WaitGenlockTimecodeProvider();
		break;
	case ESynchronizationState::PreRolling_WaitReadiness:
		TickPreRolling_WaitReadiness();
		break;
	case ESynchronizationState::PreRolling_Synchronizing:
		TickPreRolling_Synchronizing();
		break;
	case ESynchronizationState::PreRolling_Buffering:
		TickPreRolling_Buffering();
		break;
	case ESynchronizationState::Synchronized:
		TickSynchronized();
		break;
	default:
		SetTickEnabled(false);
		break;
	}
}

bool UTimecodeSynchronizer::Tick_TestGenlock()
{
	if (bUseCustomTimeStep)
	{
		if (RegisteredCustomTimeStep == nullptr)
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("The registered Genlock source is invalid."));
			SwitchState(ESynchronizationState::Error);
			return false;
		}

		if (GEngine->GetCustomTimeStep() != RegisteredCustomTimeStep)
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("The registered Genlock source is not the Engine CustomTimeStep."));
			SwitchState(ESynchronizationState::Error);
			return false;
		}

		const ECustomTimeStepSynchronizationState SynchronizationState = RegisteredCustomTimeStep->GetSynchronizationState();

		if (SynchronizationState != ECustomTimeStepSynchronizationState::Synchronized && SynchronizationState != ECustomTimeStepSynchronizationState::Synchronizing)
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("The Genlock source stopped while synchronizing."));
			SwitchState(ESynchronizationState::Error);
			return false;
		}

		return SynchronizationState == ECustomTimeStepSynchronizationState::Synchronized;
	}
	return true;
}

bool UTimecodeSynchronizer::Tick_TestTimecode()
{
	if (TimecodeProviderType == ETimecodeSynchronizationTimecodeType::TimecodeProvider)
	{
		if (RegisteredTimecodeProvider == nullptr)
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("The registered TimecodeProvider is invalid."));
			SwitchState(ESynchronizationState::Error);
			return false;
		}

		if (GEngine->GetTimecodeProvider() != RegisteredTimecodeProvider)
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("The registered TimecodeProvider is not the Engine TimecodeProvider."));
			SwitchState(ESynchronizationState::Error);
			return false;
		}

		const ETimecodeProviderSynchronizationState SynchronizationState = RegisteredTimecodeProvider->GetSynchronizationState();

		if (SynchronizationState != ETimecodeProviderSynchronizationState::Synchronized && SynchronizationState != ETimecodeProviderSynchronizationState::Synchronizing)
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("The TimecodeProvider stopped while synchronizing."));
			SwitchState(ESynchronizationState::Error);
			return false;
		}

		if (SynchronizationState == ETimecodeProviderSynchronizationState::Synchronized)
		{
			if (RegisteredTimecodeProvider->GetFrameRate() != GetFrameRate())
			{
				UE_LOG(LogTimecodeSynchronizer, Error, TEXT("The TimecodeProvider frame rate do not correspond to the specified frame rate."));
				SwitchState(ESynchronizationState::Error);
			}
		}

		return SynchronizationState == ETimecodeProviderSynchronizationState::Synchronized;
	}
	else if (TimecodeProviderType == ETimecodeSynchronizationTimecodeType::InputSource)
	{
		if (!ActiveTimecodedInputSources.IsValidIndex(ActiveMasterSynchronizationTimecodedSourceIndex))
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("The InputSource '%d' that we try to synchronize on is not valid."), ActiveMasterSynchronizationTimecodedSourceIndex);
			SwitchState(ESynchronizationState::Error);
			return false;
		}

		if (ActiveTimecodedInputSources[ActiveMasterSynchronizationTimecodedSourceIndex].InputSource == nullptr)
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("The InputSource '%d' doesn't have an input source."), ActiveMasterSynchronizationTimecodedSourceIndex);
			SwitchState(ESynchronizationState::Error);
			return false;
		}

		return ActiveTimecodedInputSources[ActiveMasterSynchronizationTimecodedSourceIndex].InputSource->IsReady();
	}
	return true;
}

void UTimecodeSynchronizer::TickPreRolling_WaitGenlockTimecodeProvider()
{
	const bool bCustomTimeStepReady = Tick_TestGenlock();
	const bool bTimecodeProvider = Tick_TestTimecode();

	if (bCustomTimeStepReady && bTimecodeProvider)
	{
		const bool bDoTick = true;
		SwitchState(ESynchronizationState::PreRolling_WaitReadiness, bDoTick);
	}
}

void UTimecodeSynchronizer::TickPreRolling_WaitReadiness()
{
	const bool bCustomTimeStepReady = Tick_TestGenlock();
	const bool bTimecodeProvider = Tick_TestTimecode();
	if (!bCustomTimeStepReady || !bTimecodeProvider)
	{
		return;
	}

	bool bAllSourceAreReady = true;
	for (FTimecodeSynchronizerActiveTimecodedInputSource& TimecodedInputSource : ActiveTimecodedInputSources)
	{
		check(TimecodedInputSource.InputSource);

		bool bIsReady = TimecodedInputSource.InputSource->IsReady();
		if (bIsReady != TimecodedInputSource.bIsReady)
		{
			if (bIsReady)
			{
				check(TimecodedInputSource.InputSource);

				TimecodedInputSource.AvailableSampleCount = TimecodedInputSource.InputSource->GetAvailableSampleCount();
				bIsReady = bIsReady && TimecodedInputSource.AvailableSampleCount > 0;

				if (!TimecodedInputSource.bIsReady && bIsReady)
				{
					//Stamp source FrameRate for time conversion
					TimecodedInputSource.FrameRate = TimecodedInputSource.InputSource->GetFrameRate();
					if (!TimecodedInputSource.FrameRate.IsMultipleOf(GetFrameRate()) && !TimecodedInputSource.FrameRate.IsFactorOf(GetFrameRate()))
					{
						UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("Source %s doesn't have a frame rate common to TimecodeSynchronizer frame rate."), *TimecodedInputSource.InputSource->GetDisplayName())
					}
				}

				TimecodedInputSource.bIsReady = bIsReady;
			}
		}

		bAllSourceAreReady = bAllSourceAreReady && bIsReady;
	}

	if (bAllSourceAreReady)
	{
		const bool bDoTick = true;
		SwitchState(ESynchronizationState::PreRolling_Synchronizing, bDoTick);
	}
}

void UTimecodeSynchronizer::TickPreRolling_Synchronizing()
{
	const bool bCustomTimeStepReady = Tick_TestGenlock();
	const bool bTimecodeProvider = Tick_TestTimecode();
	if (!bCustomTimeStepReady || !bTimecodeProvider)
	{
		return;
	}

	// Fetch each sources samples time and early exit if a source isn`t ready
	for (FTimecodeSynchronizerActiveTimecodedInputSource& TimecodedInputSource : ActiveTimecodedInputSources)
	{
		check(TimecodedInputSource.InputSource);

		FFrameTime NextSampleTime = TimecodedInputSource.InputSource->GetNextSampleTime();
		if (NextSampleTime != 0)
		{
			TimecodedInputSource.NextSampleTime = NextSampleTime;
			TimecodedInputSource.AvailableSampleCount = TimecodedInputSource.InputSource->GetAvailableSampleCount();
			TimecodedInputSource.ConvertToLocalFrameRate(GetFrameRate());
		}

		const bool bIsReady = TimecodedInputSource.InputSource->IsReady();
		if (!bIsReady)
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("Source '%s' stopped while synchronizing."), *TimecodedInputSource.InputSource->GetDisplayName());
			SwitchState(ESynchronizationState::Error);
			return;
		}
	}

	FFrameTime NewSynchronizedTime = ConvertTimecodeToFrameTime(GetTimecode());

	// Check if all inputs have that valid FrameTime
	bool bDoContains = true;
	for (FTimecodeSynchronizerActiveTimecodedInputSource& TimecodedInputSource : ActiveTimecodedInputSources)
	{
		TimecodedInputSource.bCanBeSynchronized = TimecodedInputSource.NextSampleLocalTime <= NewSynchronizedTime && NewSynchronizedTime <= TimecodedInputSource.MaxSampleLocalTime;
		if (!TimecodedInputSource.bCanBeSynchronized)
		{
			bDoContains = false;
			break;
		}

		if (bUsePreRollingTimecodeMarginOfErrors)
		{
			const FFrameTime Difference = TimecodedInputSource.MaxSampleLocalTime - NewSynchronizedTime;
			if (Difference.FrameNumber.Value > PreRollingTimecodeMarginOfErrors)
			{
				UE_LOG(LogTimecodeSynchronizer, Error, TEXT("PreRollingTimecodeMarginOfErrors '%s'."), *TimecodedInputSource.InputSource->GetDisplayName());
				SwitchState(ESynchronizationState::Error);
				return;
			}
		}
	}

	if (bDoContains)
	{
		CurrentFrameTime = NewSynchronizedTime;

		const bool bDoTick = true;
		SwitchState(ESynchronizationState::PreRolling_Buffering, bDoTick);
	}
}

void UTimecodeSynchronizer::TickPreRolling_Buffering()
{
	const bool bCustomTimeStepReady = Tick_TestGenlock();
	const bool bTimecodeProvider = Tick_TestTimecode();
	if (!bCustomTimeStepReady || !bTimecodeProvider)
	{
		return;
	}

	// Wait for all the NumberOfExtraBufferedFrame
	bool bAllBuffered = true;
	for (FTimecodeSynchronizerActiveTimecodedInputSource& TimecodedInputSource : ActiveTimecodedInputSources)
	{
		check(TimecodedInputSource.InputSource);
		if (TimecodedInputSource.InputSource->NumberOfExtraBufferedFrame > 0)
		{
			FFrameTime NextSampleTime = TimecodedInputSource.InputSource->GetNextSampleTime();
			if (NextSampleTime != 0)
			{
				TimecodedInputSource.NextSampleTime = NextSampleTime;
				TimecodedInputSource.AvailableSampleCount = TimecodedInputSource.InputSource->GetAvailableSampleCount();
				TimecodedInputSource.ConvertToLocalFrameRate(GetFrameRate());
			}

			bool bIsReady = TimecodedInputSource.InputSource->IsReady();
			if (bIsReady && TimecodedInputSource.AvailableSampleCount > 0)
			{
				//Count buffered frame from the selected start time and not from this source next sample time
				const FFrameTime NextSampleDelta = TimecodedInputSource.NextSampleLocalTime - CurrentFrameTime;
				const int32 FrameCountAfterStartTime = TimecodedInputSource.AvailableSampleCount - NextSampleDelta.AsDecimal();
				if (FrameCountAfterStartTime < TimecodedInputSource.InputSource->NumberOfExtraBufferedFrame)
				{
					bAllBuffered = false;
					break;
				}
			}
			else
			{
				UE_LOG(LogTimecodeSynchronizer, Error, TEXT("Source '%s' stopped while buffering."), *TimecodedInputSource.InputSource->GetDisplayName());
				SwitchState(ESynchronizationState::Error);
				break;
			}
		}
	}
	
	if (bAllBuffered)
	{
		const bool bCanProceed = AreSourcesReady();
		if (bCanProceed)
		{			
			const bool bDoTick = false;
			SwitchState(ESynchronizationState::Synchronized, bDoTick);
		}
	}
}

void UTimecodeSynchronizer::TickSynchronized()
{
	const bool bCustomTimeStepReady = Tick_TestGenlock();
	const bool bTimecodeProvider = Tick_TestTimecode();
	if (!bCustomTimeStepReady || !bTimecodeProvider)
	{
		return;
	}

	if (!bSourceStarted)
	{
		StartSources();
		bSourceStarted = true;
		UE_LOG(LogTimecodeSynchronizer, Log, TEXT("TimecodeProvider synchronized at %s"), *FApp::GetTimecode().ToString());
	}

	CurrentFrameTime = ConvertTimecodeToFrameTime(FApp::GetTimecode());

	// Test if all sources have the frame
	for (FTimecodeSynchronizerActiveTimecodedInputSource& TimecodedInputSource : ActiveTimecodedInputSources)
	{
		FFrameTime NextSampleTime = TimecodedInputSource.InputSource->GetNextSampleTime();
		if (NextSampleTime != 0)
		{
			TimecodedInputSource.NextSampleTime = NextSampleTime;
			TimecodedInputSource.AvailableSampleCount = TimecodedInputSource.InputSource->GetAvailableSampleCount();
			TimecodedInputSource.ConvertToLocalFrameRate(GetFrameRate());
		}
		
		const bool bIsReady = TimecodedInputSource.InputSource->IsReady();
		if (bIsReady)
		{
			const bool bDoContains = TimecodedInputSource.NextSampleLocalTime <= CurrentFrameTime && CurrentFrameTime <= TimecodedInputSource.MaxSampleLocalTime;
			if (!bDoContains)
			{
				UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("Source '%s' doesn't have the timecode ready."), *TimecodedInputSource.InputSource->GetDisplayName());
			}
		}
		else
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("Source '%s' stopped when all sources were synchronized."), *TimecodedInputSource.InputSource->GetDisplayName());
			SwitchState(ESynchronizationState::Error);
		}
	}
}

void UTimecodeSynchronizer::EnterStateError()
{
	StopInputSources();
	SynchronizationEvent.Broadcast(ETimecodeSynchronizationEvent::SynchronizationFailed);
}

void UTimecodeSynchronizer::TickError()
{

}

bool UTimecodeSynchronizer::AreSourcesReady() const
{
	for (const FTimecodeSynchronizerActiveTimecodedInputSource& InputSource : ActiveTimecodedInputSources)
	{
		if (!InputSource.InputSource->IsReady())
		{
			return false;
		}
	}

	for (const FTimecodeSynchronizerActiveTimecodedInputSource& InputSource : ActiveSynchronizedSources)
	{
		if (!InputSource.InputSource->IsReady())
		{
			return false;
		}
	}

	return true;
}

void UTimecodeSynchronizer::StartSources()
{
	for (FTimecodeSynchronizerActiveTimecodedInputSource& InputSource : ActiveTimecodedInputSources)
	{
		InputSource.InputSource->Start();
	}

	for (FTimecodeSynchronizerActiveTimecodedInputSource& InputSource : ActiveSynchronizedSources)
	{
		InputSource.InputSource->Start();
	}
}

#undef LOCTEXT_NAMESPACE
