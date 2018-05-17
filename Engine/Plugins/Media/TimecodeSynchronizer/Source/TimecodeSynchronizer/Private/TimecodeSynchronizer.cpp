// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "TimecodeSynchronizer.h"
#include "TimecodeSynchronizerModule.h"

#include "Engine/Engine.h"
#include "FixedFrameRateCustomTimeStep.h"
#include "IMediaModule.h"
#include "ITimeManagementModule.h"
#include "Misc/App.h"


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
	, FixedFrameRate(30, 1)
	, PreRollingTimecodeMarginOfErrors(4)
	, PreRollingTimeout(30.f)
	, bUseMasterSynchronizationSource(false)
	, MasterSynchronizationSourceIndex(INDEX_NONE)
	, State(ESynchronizationState::None)
	, CurrentFrameTime(0)
	, CurrentSynchronizedTimecode(FTimecode())
	, StartPreRollingTime(0.0)
	, bRegistered(false)
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
void UTimecodeSynchronizer::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	// Make sure the master source index is valid UseAsMasterSynchronizationSource
	if (bUseMasterSynchronizationSource)
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

FTimecode UTimecodeSynchronizer::GetTimecode() const
{
	return CurrentSynchronizedTimecode;
}

FFrameRate UTimecodeSynchronizer::GetFrameRate() const
{
	return bUseCustomTimeStep ? CustomTimeStep->FixedFrameRate : FixedFrameRate;
}

ETimecodeProviderSynchronizationState UTimecodeSynchronizer::GetSynchronizationState() const
{
	switch(State)
	{
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
	return State == ESynchronizationState::PreRolling_WaitReadiness
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
	UTimecodeProvider* Provider = GEngine->GetTimecodeProvider();
	if (Provider == this || Provider == nullptr)
	{
		bRegistered = GEngine->SetTimecodeProvider(this);
		SetTickEnabled(bRegistered);

		if (!bRegistered)
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("Could not set %s as the Timecode Provider."), *GetName());
		}
	}
	else
	{
		UE_LOG(LogTimecodeSynchronizer, Error, TEXT("There is already a Timecode Provider in place."));
		SwitchState(ESynchronizationState::Error);
	}
}

void UTimecodeSynchronizer::Unregister()
{
	UTimecodeProvider* Provider = GEngine->GetTimecodeProvider();
	if (Provider == this)
	{
		GEngine->SetTimecodeProvider(nullptr);
	}
	bRegistered = false;

	IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");
	if (MediaModule != nullptr)
	{
		MediaModule->GetOnTickPreEngineCompleted().RemoveAll(this);
	}
}

void UTimecodeSynchronizer::SetTickEnabled(bool bEnabled)
{
	IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");
	if (MediaModule == nullptr)
	{
		UE_LOG(LogTimecodeSynchronizer, Error, TEXT("Media module couldn't be loaded"));
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
		Unregister();
		StopInputSources();
		GEngine->SetCustomTimeStep(nullptr);
		return false;
	}
	else
	{
		if (bUseCustomTimeStep && CustomTimeStep)
		{
			const bool success = GEngine->SetCustomTimeStep(CustomTimeStep);
			if (!success)
			{
				UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("CustomTimeStep failed to be set on Engine."));
				return false;
			}
		}

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

					//Stamp source FrameRate for time conversion
					NewSource.FrameRate = InputSource->GetFrameRate();

					if (!NewSource.FrameRate.IsMultipleOf(GetFrameRate()) && !NewSource.FrameRate.IsFactorOf(GetFrameRate()))
					{
						UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("Source %s doesn't have a frame rate common to TimecodeSynchronizer frame rate."), *NewSource.InputSource->GetDisplayName())
					}

					if (bUseMasterSynchronizationSource && Index == MasterSynchronizationSourceIndex)
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

		if (bUseMasterSynchronizationSource && ActiveMasterSynchronizationTimecodedSourceIndex == INDEX_NONE)
		{
			UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("The Master Synchronization Source could not be found."));
		}

		if (ActiveTimecodedInputSources.Num() > 0)
		{
			Register();
		}

		//Engage synchronization procedure only if we've successfully registered as the TimecodeProvider
		if (bRegistered)
		{
			const bool bDoTick = true;
			SwitchState(ESynchronizationState::PreRolling_WaitReadiness, bDoTick);
		}
		else
		{
			//Cleanup CustomTimeStep since we start by setting it
			GEngine->SetCustomTimeStep(nullptr);
			StopInputSources();

			UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("Couldn't start preroll. TimecodeSynchronizer is not registered. (Maybe there is no input sources)"));
		}

		return bRegistered;
	}
}

void UTimecodeSynchronizer::StopInputSources()
{
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

	SetCurrentFrameTime(FFrameTime(0));
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
		case ESynchronizationState::PreRolling_WaitReadiness:
		{
			StartPreRollingTime = FApp::GetCurrentTime();
			SynchronizationEvent.Broadcast(ETimecodeSynchronizationEvent::SynchronizationStarted);
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

void UTimecodeSynchronizer::TickPreRolling_WaitReadiness()
{
	bool bAllSourceAreReady = true;
	for (FTimecodeSynchronizerActiveTimecodedInputSource& TimecodedInputSource : ActiveTimecodedInputSources)
	{
		check(TimecodedInputSource.InputSource);

		bool bIsReady = TimecodedInputSource.InputSource->IsReady();
		if (bIsReady != TimecodedInputSource.bIsReady)
		{
			if (bIsReady)
			{
				TimecodedInputSource.AvailableSampleCount = TimecodedInputSource.InputSource->GetAvailableSampleCount();
				bIsReady = bIsReady && TimecodedInputSource.AvailableSampleCount > 0;
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
	// Fetch each sources samples time and early exit if a source isn`t ready
	for (FTimecodeSynchronizerActiveTimecodedInputSource& TimecodedInputSource : ActiveTimecodedInputSources)
	{
		check(TimecodedInputSource.InputSource);

		const bool bIsReady = TimecodedInputSource.InputSource->IsReady();
		TimecodedInputSource.NextSampleTime = TimecodedInputSource.InputSource->GetNextSampleTime();
		TimecodedInputSource.AvailableSampleCount = TimecodedInputSource.InputSource->GetAvailableSampleCount();
		TimecodedInputSource.ConvertToLocalFrameRate(GetFrameRate());

		if (!bIsReady)
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("Source '%s' stopped while synchronizing."), *TimecodedInputSource.InputSource->GetDisplayName());
			SwitchState(ESynchronizationState::Error);
			return;
		}
	}

	// Find the synchronization time that matches for all active sources. 
	// If a master source is selected, it forces the selected Timecode is simply fetched from it.
	bool bFoundTimecode = false;
	FFrameTime NewSynchronizedTime;
	if (ActiveTimecodedInputSources.IsValidIndex(ActiveMasterSynchronizationTimecodedSourceIndex))
	{
		const FTimecodeSynchronizerActiveTimecodedInputSource& TimecodedInputSource = ActiveTimecodedInputSources[ActiveMasterSynchronizationTimecodedSourceIndex];
		if (TimecodedInputSource.AvailableSampleCount > 0)
		{
			NewSynchronizedTime = TimecodedInputSource.NextSampleLocalTime;
			bFoundTimecode = true;
		}
	}
	else
	{
		// Loop to find the next sample not yet processed by all sources
		check(ActiveTimecodedInputSources.Num() > 0);
		NewSynchronizedTime = ActiveTimecodedInputSources[0].NextSampleLocalTime;
		for (FTimecodeSynchronizerActiveTimecodedInputSource& TimecodedInputSource : ActiveTimecodedInputSources)
		{
			if (TimecodedInputSource.AvailableSampleCount > 0)
			{
				NewSynchronizedTime = FMath::Max(NewSynchronizedTime, TimecodedInputSource.NextSampleLocalTime);
				bFoundTimecode = true;
			}
		}
	}

	if (bFoundTimecode == false)
	{
		UE_LOG(LogTimecodeSynchronizer, Error, TEXT("No initial Timecode was found."));
		SwitchState(ESynchronizationState::Error);
		return;
	}

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
		SetCurrentFrameTime(NewSynchronizedTime);

		const bool bDoTick = true;
		SwitchState(ESynchronizationState::PreRolling_Buffering, bDoTick);
	}
}

void UTimecodeSynchronizer::TickPreRolling_Buffering()
{
	// Wait for all the NumberOfExtraBufferedFrame
	bool bAllBuffered = true;
	for (FTimecodeSynchronizerActiveTimecodedInputSource& TimecodedInputSource : ActiveTimecodedInputSources)
	{
		check(TimecodedInputSource.InputSource);
		if (TimecodedInputSource.InputSource->NumberOfExtraBufferedFrame > 0)
		{
			bool bIsReady = TimecodedInputSource.InputSource->IsReady();
			TimecodedInputSource.NextSampleTime = TimecodedInputSource.InputSource->GetNextSampleTime();
			TimecodedInputSource.AvailableSampleCount = TimecodedInputSource.InputSource->GetAvailableSampleCount();
			TimecodedInputSource.ConvertToLocalFrameRate(GetFrameRate());

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
			StartSources();

			UE_LOG(LogTimecodeSynchronizer, Log, TEXT("TimecodeProvider synchronized at %s"), *CurrentSynchronizedTimecode.ToString());
			
			const bool bDoTick = false;
			SwitchState(ESynchronizationState::Synchronized, bDoTick);
		}
	}
}

void UTimecodeSynchronizer::TickSynchronized()
{
	FFrameTime NewFrameTime = CurrentFrameTime + GetFrameRate().AsFrameTime(FApp::GetDeltaTime());
	
	if (ActiveTimecodedInputSources.IsValidIndex(ActiveMasterSynchronizationTimecodedSourceIndex))
	{
		FTimecodeSynchronizerActiveTimecodedInputSource& TimecodedInputSource = ActiveTimecodedInputSources[ActiveMasterSynchronizationTimecodedSourceIndex];
		TimecodedInputSource.NextSampleTime = TimecodedInputSource.InputSource->GetNextSampleTime();
		TimecodedInputSource.AvailableSampleCount = TimecodedInputSource.InputSource->GetAvailableSampleCount();
		TimecodedInputSource.ConvertToLocalFrameRate(GetFrameRate());

		if (NewFrameTime > TimecodedInputSource.MaxSampleLocalTime)
		{
			UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("Current Timecode went beyond the master source maximum Timecode. Consider adding more buffer."));
			NewFrameTime = TimecodedInputSource.MaxSampleLocalTime;
		}
		else if (NewFrameTime < TimecodedInputSource.NextSampleLocalTime)
		{
			UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("Current Timecode went below the master source maximum Timecode. Is FrameRate too slow?"));
			NewFrameTime = TimecodedInputSource.NextSampleLocalTime;
		};
	}

	SetCurrentFrameTime(NewFrameTime);

	// Test if all sources have the frame
	for (FTimecodeSynchronizerActiveTimecodedInputSource& TimecodedInputSource : ActiveTimecodedInputSources)
	{
		TimecodedInputSource.NextSampleTime = TimecodedInputSource.InputSource->GetNextSampleTime();
		TimecodedInputSource.AvailableSampleCount = TimecodedInputSource.InputSource->GetAvailableSampleCount();
		TimecodedInputSource.ConvertToLocalFrameRate(GetFrameRate());
		
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
	Unregister();
	StopInputSources();
	GEngine->SetCustomTimeStep(nullptr);
	SynchronizationEvent.Broadcast(ETimecodeSynchronizationEvent::SynchronizationFailed);
}

void UTimecodeSynchronizer::TickError()
{

}

void UTimecodeSynchronizer::SetCurrentFrameTime(const FFrameTime& InNewTime)
{
	CurrentFrameTime = InNewTime;
	const bool bIsDropFrame = FTimecode::IsDropFormatTimecodeSupported(GetFrameRate());
	CurrentSynchronizedTimecode = FTimecode::FromFrameNumber(CurrentFrameTime.FrameNumber, GetFrameRate(), bIsDropFrame);
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



