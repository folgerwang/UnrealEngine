// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "TimecodeSynchronizer.h"
#include "TimecodeSynchronizerModule.h"

#include "Engine/Engine.h"
#include "FixedFrameRateCustomTimeStep.h"
#include "IMediaModule.h"
#include "ITimeManagementModule.h"
#include "Misc/App.h"

#define LOCTEXT_NAMESPACE "TimecodeSynchronizer"

namespace TimecodeSynchronizerPrivate
{
	struct FTimecodeInputSourceValidator
	{
	private:

		const FTimecodeSynchronizerCachedSyncState& SyncState;

		bool bTimecodeErrors = false;
		int32 FoundOffset = 0;

		FFrameTime Newest;
		FFrameTime Oldest;

		bool bAnySourcesHadRollover = false;
		bool bAllSourcesHadRollover = false;

	public:

		FTimecodeInputSourceValidator(const FTimecodeSynchronizerCachedSyncState& InSyncState, const FTimecodeSynchronizerActiveTimecodedInputSource& InitialInputSource) :
			SyncState(InSyncState)
		{
			ValidateSource(InitialInputSource);
			if (AllSourcesAreValid())
			{
				const FTimecodeSourceState& SynchronizerRelativeState = InitialInputSource.GetSynchronizerRelativeState();
				Newest = SynchronizerRelativeState.NewestAvailableSample;
				Oldest = SynchronizerRelativeState.OldestAvailableSample;
				bAnySourcesHadRollover = (SyncState.RolloverFrame.IsSet() && Newest < Oldest);
				bAllSourcesHadRollover = bAnySourcesHadRollover;
			}
		}

		void UpdateFrameTimes(const FTimecodeSynchronizerActiveTimecodedInputSource& InputSource)
		{
			ValidateSource(InputSource);
			if (AllSourcesAreValid())
			{
				const FTimecodeSourceState& SynchronizerRelativeState = InputSource.GetSynchronizerRelativeState();
				Oldest = FMath::Max(SynchronizerRelativeState.OldestAvailableSample, Oldest);
				Newest = FMath::Min(SynchronizerRelativeState.NewestAvailableSample, Newest);
			}
		}

		const bool AllSourcesAreValid() const
		{
			return !FoundTimecodeErrors() && !FoundFrameRolloverMistmatch();
		}

		const bool FoundFrameRolloverMistmatch() const
		{
			return bAllSourcesHadRollover != bAnySourcesHadRollover;
		}

		const bool FoundTimecodeErrors() const
		{
			return bTimecodeErrors;
		}

		const bool DoAllSourcesContainFrame(const FFrameTime& FrameToCheck) const
		{
			if (FoundTimecodeErrors() || FoundFrameRolloverMistmatch())
			{
				return false;
			}
			else if (!SyncState.RolloverFrame.IsSet() || !bAnySourcesHadRollover)
			{
				return (Oldest <= FrameToCheck) && (FrameToCheck <= Newest);
			}
			else
			{
				return UTimeSynchronizationSource::IsFrameBetweenWithRolloverModulus(FrameToCheck, Oldest, Newest, SyncState.RolloverFrame.GetValue());
			}
		}

		const int32 CalculateOffsetNewest(const FFrameTime& FrameTime) const
		{
			// These cases should never happen, but they may be recoverable, so don't crash.
			ensureAlwaysMsgf(!FoundTimecodeErrors(), TEXT("FTimecodeInputSourceValidator::CalculateOffsetNewest - Called with TimecodeErrors"));
			ensureAlwaysMsgf(!FoundFrameRolloverMistmatch(), TEXT("FTimecodeInputSourceValidater::CalculateOffsetNewest - Called with FrameRolloverMismatch"));

			bool bUnused_DidRollover;
			return UTimeSynchronizationSource::FindDistanceBetweenFramesWithRolloverModulus(FrameTime, Newest, SyncState.RolloverFrame, bUnused_DidRollover);
		}

		const int32 CalculateOffsetOldest(const FFrameTime& FrameTime) const
		{
			// These cases should never happen, but they may be recoverable, so don't crash.
			ensureAlwaysMsgf(!FoundTimecodeErrors(), TEXT("FTimecodeInputSourceValidator::CalculateOffsetOldest - Called with TimecodeErrors"));
			ensureAlwaysMsgf(!FoundFrameRolloverMistmatch(), TEXT("FTimecodeInputSourceValidater::CalculateOffsetOldest - Called with FrameRolloverMismatch"));

			bool bUnused_DidRollover;

			// Because we switched order of inputs, we need to flip the output as well.
			return -UTimeSynchronizationSource::FindDistanceBetweenFramesWithRolloverModulus(Oldest, FrameTime, SyncState.RolloverFrame, bUnused_DidRollover);
		}

	private:

		void ValidateSource(const FTimecodeSynchronizerActiveTimecodedInputSource& InputSource)
		{
			const FTimecodeSourceState& SynchronizerRelativeState = InputSource.GetSynchronizerRelativeState();
			const FFrameTime& OldestSample = SynchronizerRelativeState.OldestAvailableSample;
			const FFrameTime& NewestSample = SynchronizerRelativeState.NewestAvailableSample;

			const bool bUseRollover = SyncState.RolloverFrame.IsSet();
			const bool bSourceBufferHasRolledOver = (bUseRollover && OldestSample > NewestSample);

			if (!bUseRollover)
			{
				// If we're not using rollover, but Oldest time is later than the Newest time, then the source is
				// reporting incorrect values.
				if (OldestSample > NewestSample)
				{
					UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("Source %s reported out of order frame times (Oldest = %d | Newest = %d)"),
						*InputSource.GetDisplayName(), OldestSample.GetFrame().Value, NewestSample.GetFrame().Value);

					bTimecodeErrors = true;
				}
			}
			else
			{
				const FFrameTime& RolloverFrame = SyncState.RolloverFrame.GetValue();

				// If we're using rollover, and either source has reported a value beyond where we expect to rollover,
				// then the source is reporting incorrect values.
				if ((OldestSample >= RolloverFrame) || (NewestSample >= RolloverFrame))
				{
					UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("Source %s reported frames that go beyond expected rollover point (Oldest = %d | Newest = %d | Rollover = %d"),
						*InputSource.GetDisplayName(), OldestSample.GetFrame().Value, NewestSample.GetFrame().Value, RolloverFrame.GetFrame().Value);

					bTimecodeErrors = true;
				}

				if (bSourceBufferHasRolledOver)
				{
					// See CalculateOffset for the justification

					// Since we think a rollover has occurred, then we'd expect the frame values to be relatively
					// far apart.
					const int32 Offset = (OldestSample - NewestSample).GetFrame().Value;
					if (FMath::Abs<int32>(Offset) < (RolloverFrame.GetFrame().Value / 2))
					{
						UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("Source %s reported out of order frame times (Oldest = %d | Newest = %d)"),
							*InputSource.GetDisplayName(), OldestSample.GetFrame().Value, NewestSample.GetFrame().Value);

						bTimecodeErrors = true;
					}
				}
			}

			bAllSourcesHadRollover &= bSourceBufferHasRolledOver;
			bAnySourcesHadRollover |= bSourceBufferHasRolledOver;
		}
	};

}

/**
 * UTimecodeSynchronizer
 */

UTimecodeSynchronizer::UTimecodeSynchronizer()
	: bUseCustomTimeStep(false)
	, CustomTimeStep(nullptr)
	, FixedFrameRate(30, 1)
	, TimecodeProviderType(ETimecodeSynchronizationTimecodeType::TimecodeProvider)
	, TimecodeProvider(nullptr)
	, MasterSynchronizationSourceIndex(INDEX_NONE)
	, PreRollingTimecodeMarginOfErrors(4)
	, PreRollingTimeout(30.f)
	, State(ESynchronizationState::None)
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

	const FName PropertyName = InProperty->GetFName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTimecodeSynchronizer, TimecodeProvider))
	{
		return TimecodeProviderType == ETimecodeSynchronizationTimecodeType::TimecodeProvider;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UTimecodeSynchronizer, MasterSynchronizationSourceIndex))
	{
		return TimecodeProviderType == ETimecodeSynchronizationTimecodeType::InputSource;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UTimecodeSynchronizer, FrameOffset))
	{
		return SyncMode == ETimecodeSynchronizationSyncMode::UserDefinedOffset;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UTimecodeSynchronizer, AutoFrameOffset))
	{
		return (SyncMode == ETimecodeSynchronizationSyncMode::Auto) ||
			(SyncMode == ETimecodeSynchronizationSyncMode::AutoOldest);
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

FTimecode UTimecodeSynchronizer::GetTimecode() const
{
	FTimecode Timecode;
	if (IsSynchronized())
	{
		Timecode = UTimeSynchronizationSource::ConvertFrameTimeToTimecode(CurrentSystemFrameTime.GetValue(), CachedSyncState.FrameRate);
	}
	else if (IsSynchronizing())
	{
		Timecode = UTimeSynchronizationSource::ConvertFrameTimeToTimecode(CurrentProviderFrameTime, CachedSyncState.FrameRate);
	}
	else
	{
		Timecode = UTimeSynchronizationSource::ConvertFrameTimeToTimecode(GetProviderFrameTime(), GetFrameRate());
	}

	return Timecode;
}

FFrameTime UTimecodeSynchronizer::GetProviderFrameTime() const
{
	FFrameTime ProviderFrameTime;

	if (TimecodeProviderType == ETimecodeSynchronizationTimecodeType::InputSource)
	{
		if (SynchronizedSources.IsValidIndex(ActiveMasterSynchronizationTimecodedSourceIndex))
		{
			const FTimecodeSynchronizerActiveTimecodedInputSource& TimecodedInputSource = SynchronizedSources[ActiveMasterSynchronizationTimecodedSourceIndex];

			if (GFrameCounter != LastUpdatedSources)
			{
				const_cast<FTimecodeSynchronizerActiveTimecodedInputSource&>(TimecodedInputSource).UpdateSourceState(GetFrameRate());
			}

			if (TimecodedInputSource.IsReady())
			{
				ProviderFrameTime = TimecodedInputSource.GetSynchronizerRelativeState().NewestAvailableSample;
			}
			else
			{
				UE_LOG(LogTimecodeSynchronizer, Log, TEXT("Unable to get frame time - Specified source was not ready."));
			}
		}
		else
		{
			UE_LOG(LogTimecodeSynchronizer, Log, TEXT("Unable to get frame time - Invalid source specified."));
		}
	}
	else
	{
		// In the case where we aren't registered, or we've registered ourselves, we'll use the engine default provider.
		const bool bIsProviderValid = (RegisteredTimecodeProvider != nullptr && RegisteredTimecodeProvider != this);
		const UTimecodeProvider* Provider = bIsProviderValid ? RegisteredTimecodeProvider : GEngine->GetDefaultTimecodeProvider();

		ProviderFrameTime = FFrameTime(Provider->GetTimecode().ToFrameNumber(GetFrameRate()));
	}

	return ProviderFrameTime;
}

FFrameRate UTimecodeSynchronizer::GetFrameRate() const
{
	return bUseCustomTimeStep && CustomTimeStep ? CustomTimeStep->GetFixedFrameRate() : FixedFrameRate;
}

ETimecodeProviderSynchronizationState UTimecodeSynchronizer::GetSynchronizationState() const
{
	switch (State)
	{
	case ESynchronizationState::Initializing:
	case ESynchronizationState::PreRolling_WaitGenlockTimecodeProvider:
	case ESynchronizationState::PreRolling_WaitReadiness:
	case ESynchronizationState::PreRolling_Synchronizing:
		return ETimecodeProviderSynchronizationState::Synchronizing;
	case ESynchronizationState::Synchronized:
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
		|| State == ESynchronizationState::Initializing;
}

bool UTimecodeSynchronizer::IsSynchronized() const
{
	return State == ESynchronizationState::Synchronized;
}

bool UTimecodeSynchronizer::IsError() const
{
	return State == ESynchronizationState::Error;
}

void UTimecodeSynchronizer::Register()
{
	if (!bRegistered)
	{
		bRegistered = true;

		if (bUseCustomTimeStep)
		{
			if (GEngine->GetCustomTimeStep())
			{
				UE_LOG(LogTimecodeSynchronizer, Error, TEXT("Genlock source is already in place."));
				SwitchState(ESynchronizationState::Error);
				return;
			}
			else if (!CustomTimeStep)
			{
				UE_LOG(LogTimecodeSynchronizer, Error, TEXT("The Genlock source is not set."));
				SwitchState(ESynchronizationState::Error);
				return;
			}
			else if (!GEngine->SetCustomTimeStep(CustomTimeStep))
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
		if (GEngine->GetTimecodeProvider() != GEngine->GetDefaultTimecodeProvider())
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("A Timecode Provider is already in place."));
			SwitchState(ESynchronizationState::Error);
			return;
		}
		else if (TimecodeProviderType == ETimecodeSynchronizationTimecodeType::TimecodeProvider && TimecodeProvider)
		{
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

		SetTickEnabled(true);
	}
}

void UTimecodeSynchronizer::Unregister()
{
	if (bRegistered)
	{
		bRegistered = false;

		const UTimecodeProvider* Provider = GEngine->GetTimecodeProvider();
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

		SetTickEnabled(false);
	}
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
	UpdateSourceStates();
	CurrentProviderFrameTime = GetProviderFrameTime();

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

bool UTimecodeSynchronizer::StartSynchronization()
{
	if (IsSynchronizing() || IsSynchronized())
	{
		UE_LOG(LogTimecodeSynchronizer, Log, TEXT("Already synchronizing or synchronized."));
		return true;
	}
	else
	{
		if (!ensure(SynchronizedSources.Num() == 0) || !ensure(NonSynchronizedSources.Num() == 0) || !ensure(ActiveMasterSynchronizationTimecodedSourceIndex))
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("StartSynchronization called without properly closing sources"));
			CloseSources();
		}

		SwitchState(ESynchronizationState::Initializing);
		OpenSources();

		if (SynchronizedSources.Num() == 0)
		{
			UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("No sources available to synchronize."));
			SwitchState(ESynchronizationState::Error);
		}
		else if (TimecodeProviderType == ETimecodeSynchronizationTimecodeType::InputSource && ActiveMasterSynchronizationTimecodedSourceIndex == INDEX_NONE)
		{
			UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("The Master Synchronization Source could not be found."));
			SwitchState(ESynchronizationState::Error);
		}
		else
		{
			Register();

			if (bRegistered)
			{
				SwitchState(ESynchronizationState::PreRolling_WaitGenlockTimecodeProvider);
			}
		}

		return bRegistered;
	}
}

void UTimecodeSynchronizer::StopSynchronization()
{
	if (IsSynchronizing() || IsSynchronized() || IsError())
	{
		Unregister();
		CloseSources();

		LastUpdatedSources = 0;
		CurrentSystemFrameTime.Reset();
		CurrentProviderFrameTime = FFrameTime(0);
		StartPreRollingTime = 0.f;

		SwitchState(ESynchronizationState::None);
	}
}

void UTimecodeSynchronizer::SwitchState(const ESynchronizationState NewState)
{
	if (NewState != State)
	{
		State = NewState;

		// Do any setup that needs to happen to "enter" the state.
		switch (NewState)
		{
		case ESynchronizationState::Initializing:
			CachedSyncState.FrameRate = GetFrameRate();
			CachedSyncState.SyncMode = SyncMode;
			CachedSyncState.FrameOffset = FrameOffset;

			// System time inherently has rollover.
			if (bWithRollover)
			{
				// In most cases, rollover occurs on 24 periods.
				// TODO: Make this configurable
				CachedSyncState.RolloverFrame = FTimecode(24, 0, 0, 0, false).ToFrameNumber(CachedSyncState.FrameRate);
			}
			else
			{
				CachedSyncState.RolloverFrame.Reset();
			}

			break;

		case ESynchronizationState::PreRolling_WaitGenlockTimecodeProvider:
			StartPreRollingTime = FApp::GetCurrentTime();
			SynchronizationEvent.Broadcast(ETimecodeSynchronizationEvent::SynchronizationStarted);
			break;

		case ESynchronizationState::Synchronized:
			StartSources();
			SynchronizationEvent.Broadcast(ETimecodeSynchronizationEvent::SynchronizationSucceeded);
			break;

		case ESynchronizationState::Error:
			StopSynchronization();
			SynchronizationEvent.Broadcast(ETimecodeSynchronizationEvent::SynchronizationFailed);
			break;

		default:
			break;
		};

		Tick_Switch();
	}
}

void UTimecodeSynchronizer::Tick_Switch()
{
#define CONDITIONALLY_CALL_TICK(TickFunc) {if (ShouldTick()) {TickFunc();}}

	switch (State)
	{
	case ESynchronizationState::Initializing:
		break;

	case ESynchronizationState::PreRolling_WaitGenlockTimecodeProvider:
		CONDITIONALLY_CALL_TICK(TickPreRolling_WaitGenlockTimecodeProvider);
		break;

	case ESynchronizationState::PreRolling_WaitReadiness:
		CONDITIONALLY_CALL_TICK(TickPreRolling_WaitReadiness);
		break;

	case ESynchronizationState::PreRolling_Synchronizing:
		CONDITIONALLY_CALL_TICK(TickPreRolling_Synchronizing);
		break;

	case ESynchronizationState::Synchronized:
		CONDITIONALLY_CALL_TICK(Tick_Synchronized);
		break;

	default:
		SetTickEnabled(false);
		break;
	}

#undef CONDITIONALLY_CALL_TICK
}

bool UTimecodeSynchronizer::ShouldTick()
{
	return Tick_TestGenlock() && Tick_TestTimecode();
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

		if (RegisteredTimecodeProvider == this)
		{
			return true;
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
		if (!SynchronizedSources.IsValidIndex(ActiveMasterSynchronizationTimecodedSourceIndex))
		{
			UE_LOG(LogTimecodeSynchronizer, Error, TEXT("The InputSource '%d' that we try to synchronize on is not valid."), ActiveMasterSynchronizationTimecodedSourceIndex);
			SwitchState(ESynchronizationState::Error);
			return false;
		}

		return SynchronizedSources[ActiveMasterSynchronizationTimecodedSourceIndex].IsReady();
	}
	return true;
}

void UTimecodeSynchronizer::TickPreRolling_WaitGenlockTimecodeProvider()
{
	SwitchState(ESynchronizationState::PreRolling_WaitReadiness);
}

void UTimecodeSynchronizer::TickPreRolling_WaitReadiness()
{
	bool bAllSourceAreReady = true;

	for (const FTimecodeSynchronizerActiveTimecodedInputSource& InputSource : SynchronizedSources)
	{
		if (InputSource.IsReady())
		{
			const FFrameRate SourceFrameRate = InputSource.GetFrameRate();
			if (!SourceFrameRate.IsMultipleOf(CachedSyncState.FrameRate) && !SourceFrameRate.IsFactorOf(CachedSyncState.FrameRate))
			{
				UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("Source %s doesn't have a frame rate common to TimecodeSynchronizer frame rate."), *InputSource.GetDisplayName())
			}
		}
		else
		{
			bAllSourceAreReady = false;
		}
	}

	if (bAllSourceAreReady)
	{
		SwitchState(ESynchronizationState::PreRolling_Synchronizing);
	}
}

void UTimecodeSynchronizer::TickPreRolling_Synchronizing()
{
	TimecodeSynchronizerPrivate::FTimecodeInputSourceValidator Validator(CachedSyncState, SynchronizedSources[0]);
	for (int32 i = 1; i < SynchronizedSources.Num(); ++i)
	{
		Validator.UpdateFrameTimes(SynchronizedSources[i]);
	}

	if (Validator.AllSourcesAreValid())
	{
		switch (CachedSyncState.SyncMode)
		{
		case ETimecodeSynchronizationSyncMode::Auto:
			ActualFrameOffset = Validator.CalculateOffsetNewest(CurrentProviderFrameTime) - AutoFrameOffset;
			break;

		case ETimecodeSynchronizationSyncMode::AutoOldest:
			ActualFrameOffset = Validator.CalculateOffsetOldest(CurrentProviderFrameTime) + AutoFrameOffset;
			break;

		default:
			ActualFrameOffset = CachedSyncState.FrameOffset;
			break;
		}

		if (Validator.DoAllSourcesContainFrame(CalculateSyncTime()))
		{
			SwitchState(ESynchronizationState::Synchronized);
		}
	}
}

void UTimecodeSynchronizer::Tick_Synchronized()
{
	// Sanity check to make sure all sources still have valid frames.
	CurrentSystemFrameTime = CalculateSyncTime();
	const FFrameTime& UseFrameTime = CurrentSystemFrameTime.GetValue();

	if (CachedSyncState.RolloverFrame.IsSet())
	{
		for (const FTimecodeSynchronizerActiveTimecodedInputSource& InputSource : SynchronizedSources)
		{
			const FTimecodeSourceState& SynchronizerRelativeState = InputSource.GetSynchronizerRelativeState();
			if (!UTimeSynchronizationSource::IsFrameBetweenWithRolloverModulus(UseFrameTime, SynchronizerRelativeState.OldestAvailableSample, SynchronizerRelativeState.NewestAvailableSample, CachedSyncState.RolloverFrame.GetValue()))
			{
				UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("Source '%s' doesn't have the timecode ready."), *InputSource.GetDisplayName());
			}
		}
	}
	else
	{
		for (const FTimecodeSynchronizerActiveTimecodedInputSource& InputSource : SynchronizedSources)
		{
			const FTimecodeSourceState& SynchronizerRelativeState = InputSource.GetSynchronizerRelativeState();
			if (SynchronizerRelativeState.OldestAvailableSample > UseFrameTime || UseFrameTime > SynchronizerRelativeState.NewestAvailableSample)
			{
				UE_LOG(LogTimecodeSynchronizer, Warning, TEXT("Source '%s' doesn't have the timecode ready."), *InputSource.GetDisplayName());
			}
		}
	}
}

const bool FTimecodeSynchronizerActiveTimecodedInputSource::UpdateSourceState(const FFrameRate& SynchronizerFrameRate)
{
	check(InputSource);

	bIsReady = InputSource->IsReady();

	if (bIsReady)
	{
		FrameRate = InputSource->GetFrameRate();

		InputSourceState.NewestAvailableSample = InputSource->GetNewestSampleTime();
		InputSourceState.OldestAvailableSample = InputSource->GetOldestSampleTime();

		if (FrameRate != SynchronizerFrameRate)
		{
			SynchronizerRelativeState.NewestAvailableSample = FFrameRate::TransformTime(InputSourceState.NewestAvailableSample, FrameRate, SynchronizerFrameRate);
			SynchronizerRelativeState.OldestAvailableSample = FFrameRate::TransformTime(InputSourceState.OldestAvailableSample, FrameRate, SynchronizerFrameRate);
		}
		else
		{
			SynchronizerRelativeState = InputSourceState;
		}
	}

	return bIsReady;
}

void UTimecodeSynchronizer::StartSources()
{
	FTimeSynchronizationStartData StartData;
	CurrentSystemFrameTime = StartData.StartFrame = CalculateSyncTime();

	FApp::SetTimecodeAndFrameRate(GetTimecode(), GetFrameRate());

	for (UTimeSynchronizationSource* InputSource : TimeSynchronizationInputSources)
	{
		if (InputSource != nullptr)
		{
			InputSource->Start(StartData);
		}
	}
}

void UTimecodeSynchronizer::OpenSources()
{
	FTimeSynchronizationOpenData OpenData;
	OpenData.RolloverFrame = CachedSyncState.RolloverFrame;
	OpenData.SynchronizationFrameRate = CachedSyncState.FrameRate;
	for (int32 Index = 0; Index < TimeSynchronizationInputSources.Num(); ++Index)
	{
		if (UTimeSynchronizationSource* InputSource = TimeSynchronizationInputSources[Index])
		{
			if (InputSource->Open(OpenData))
			{
				if (InputSource->bUseForSynchronization)
				{
					FTimecodeSynchronizerActiveTimecodedInputSource& NewSource = SynchronizedSources.Emplace_GetRef(InputSource);
					if (TimecodeProviderType == ETimecodeSynchronizationTimecodeType::InputSource && Index == MasterSynchronizationSourceIndex)
					{
						ActiveMasterSynchronizationTimecodedSourceIndex = SynchronizedSources.Num() - 1;
					}
				}
				else
				{
					NonSynchronizedSources.Emplace(InputSource);
				}
			}
		}
	}
}

void UTimecodeSynchronizer::CloseSources()
{
	for (UTimeSynchronizationSource* InputSource : TimeSynchronizationInputSources)
	{
		if (InputSource != nullptr)
		{
			InputSource->Close();
		}
	}

	SynchronizedSources.Reset();
	NonSynchronizedSources.Reset();
	ActiveMasterSynchronizationTimecodedSourceIndex = INDEX_NONE;
}

void UTimecodeSynchronizer::UpdateSourceStates()
{
	// Update all of our source states.
	if (GFrameCounter != LastUpdatedSources)
	{
		LastUpdatedSources = GFrameCounter;

		// If we're in the process of synchronizing, or have already achieved synchronization,
		// we don't expect sources to become unready. If they do, that's an error.
		// This is only relevant to 
		const bool bTreatUnreadyAsError = (State > ESynchronizationState::PreRolling_WaitReadiness);
		TArray<const FTimecodeSynchronizerActiveTimecodedInputSource*> UnreadySources;
		TArray<const FTimecodeSynchronizerActiveTimecodedInputSource*> InvalidSources;

		const FFrameRate FrameRate = GetFrameRate();
		for (FTimecodeSynchronizerActiveTimecodedInputSource& InputSource : SynchronizedSources)
		{
			InputSource.UpdateSourceState(FrameRate);
			if (!InputSource.IsInputSourceValid())
			{
				InvalidSources.Add(&InputSource);
			}
			else if (!InputSource.IsReady())
			{
				UnreadySources.Add(&InputSource);
			}
		}

		// Don't track readiness for these sources, they are not actively being used.
		for (FTimecodeSynchronizerActiveTimecodedInputSource& InputSource : NonSynchronizedSources)
		{
			InputSource.UpdateSourceState(FrameRate);
			if (!InputSource.IsInputSourceValid())
			{
				InvalidSources.Add(&InputSource);
			}
		}

		const FString StateString = SynchronizationStateToString(State);
		if (InvalidSources.Num() > 0)
		{
			for (const FTimecodeSynchronizerActiveTimecodedInputSource* UnreadySource : UnreadySources)
			{
				UE_LOG(LogTimecodeSynchronizer, Error, TEXT("Invalid source found unready during State '%s'"), *StateString);
			}
		}

		// Process our unready sources.
		// This is done here to keep the loops above fairly clean.
		if (bTreatUnreadyAsError && UnreadySources.Num() > 0)
		{
			for (const FTimecodeSynchronizerActiveTimecodedInputSource* UnreadySource : UnreadySources)
			{
				UE_LOG(LogTimecodeSynchronizer, Error, TEXT("Source '%s' became unready during State '%s'"), *(UnreadySource->GetDisplayName()), *StateString);
			}
		}

		if (InvalidSources.Num() > 0 || (bTreatUnreadyAsError && UnreadySources.Num() > 0))
		{
			SwitchState(ESynchronizationState::Error);
		}
	}
}

FFrameTime UTimecodeSynchronizer::CalculateSyncTime()
{
	if (CachedSyncState.RolloverFrame.IsSet())
	{
		return UTimeSynchronizationSource::AddOffsetWithRolloverModulus(CurrentProviderFrameTime, ActualFrameOffset, CachedSyncState.RolloverFrame.GetValue());
	}
	else
	{
		return CurrentProviderFrameTime + ActualFrameOffset;
	}
}

#undef LOCTEXT_NAMESPACE
