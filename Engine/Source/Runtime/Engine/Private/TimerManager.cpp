// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TickTaskManager.cpp: Manager for ticking tasks
=============================================================================*/

#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "Engine/World.h"
#include "UnrealEngine.h"
#include "Misc/TimeGuard.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Algo/Transform.h"

DECLARE_CYCLE_STAT(TEXT("SetTimer"), STAT_SetTimer, STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("SetTimeForNextTick"), STAT_SetTimerForNextTick, STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("ClearTimer"), STAT_ClearTimer, STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("ClearAllTimers"), STAT_ClearAllTimers, STATGROUP_Engine);

CSV_DECLARE_CATEGORY_MODULE_EXTERN(CORE_API, Basic);

/** Track the last assigned handle globally */
uint64 FTimerManager::LastAssignedSerialNumber = 0;

namespace
{
	void DescribeFTimerDataSafely(const FTimerData& Data)
	{
		UE_LOG(LogEngine, Log, TEXT("TimerData %p : bLoop=%s, bRequiresDelegate=%s, Status=%d, Rate=%f, ExpireTime=%f"),
			&Data,
			Data.bLoop ? TEXT("true") : TEXT("false"),
			Data.bRequiresDelegate ? TEXT("true") : TEXT("false"),
			static_cast<int32>(Data.Status),
			Data.Rate,
			Data.ExpireTime
			)
	}
}

struct FTimerHeapOrder
{
	explicit FTimerHeapOrder(const TSparseArray<FTimerData>& InTimers)
		: Timers(InTimers)
		, NumTimers(InTimers.Num())
	{
	}

	bool operator()(FTimerHandle LhsHandle, FTimerHandle RhsHandle) const
	{
		int32 LhsIndex = LhsHandle.GetIndex();
		int32 RhsIndex = RhsHandle.GetIndex();

		const FTimerData& LhsData = Timers[LhsIndex];
		const FTimerData& RhsData = Timers[RhsIndex];

		return LhsData.ExpireTime < RhsData.ExpireTime;
	}

	const TSparseArray<FTimerData>& Timers;
	int32 NumTimers;
};

FTimerManager::FTimerManager()
	: InternalTime(0.0)
	, LastTickedFrame(static_cast<uint64>(-1))
	, OwningGameInstance(nullptr)
{
	if (IsRunningDedicatedServer())
	{
		// Off by default, renable if needed
		//FCoreDelegates::OnHandleSystemError.AddRaw(this, &FTimerManager::OnCrash);
	}
}

FTimerManager::~FTimerManager()
{
	if (IsRunningDedicatedServer())
	{
		FCoreDelegates::OnHandleSystemError.RemoveAll(this);
	}
}

void FTimerManager::OnCrash()
{
	UE_LOG(LogEngine, Warning, TEXT("TimerManager %p on crashing delegate called, dumping extra information"), this);

	UE_LOG(LogEngine, Log, TEXT("------- %d Active Timers (including expired) -------"), ActiveTimerHeap.Num());
	int32 ExpiredActiveTimerCount = 0;
	for (FTimerHandle Handle : ActiveTimerHeap)
	{
		const FTimerData& Timer = GetTimer(Handle);
		if (Timer.Status == ETimerStatus::ActivePendingRemoval)
		{
			++ExpiredActiveTimerCount;
		}
		else
		{
			DescribeFTimerDataSafely(Timer);
		}
	}
	UE_LOG(LogEngine, Log, TEXT("------- %d Expired Active Timers -------"), ExpiredActiveTimerCount);

	UE_LOG(LogEngine, Log, TEXT("------- %d Paused Timers -------"), PausedTimerSet.Num());
	for (FTimerHandle Handle : PausedTimerSet)
	{
		const FTimerData& Timer = GetTimer(Handle);
		DescribeFTimerDataSafely(Timer);
	}

	UE_LOG(LogEngine, Log, TEXT("------- %d Pending Timers -------"), PendingTimerSet.Num());
	for (FTimerHandle Handle : PendingTimerSet)
	{
		const FTimerData& Timer = GetTimer(Handle);
		DescribeFTimerDataSafely(Timer);
	}

	UE_LOG(LogEngine, Log, TEXT("------- %d Total Timers -------"), PendingTimerSet.Num() + PausedTimerSet.Num() + ActiveTimerHeap.Num() - ExpiredActiveTimerCount);

	UE_LOG(LogEngine, Warning, TEXT("TimerManager %p dump ended"), this);
}


FString FTimerUnifiedDelegate::ToString() const
{
	const UObject* Object = nullptr;
	FName FunctionName = NAME_None;
	bool bDynDelegate = false;

	if (FuncDelegate.IsBound())
	{
#if USE_DELEGATE_TRYGETBOUNDFUNCTIONNAME
		FunctionName = FuncDelegate.TryGetBoundFunctionName();
#endif
	}
	else if (FuncDynDelegate.IsBound())
	{
		Object = FuncDynDelegate.GetUObject();
		FunctionName = FuncDynDelegate.GetFunctionName();
		bDynDelegate = true;
	}
	else
	{
		static FName NotBoundName(TEXT("NotBound!"));
		FunctionName = NotBoundName;
	}

	return FString::Printf(TEXT("%s,%s,%s"), bDynDelegate ? TEXT("DELEGATE") : TEXT("DYN DELEGATE"), Object == nullptr ? TEXT("NO OBJ") : *Object->GetPathName(), *FunctionName.ToString());
}

// ---------------------------------
// Private members
// ---------------------------------

FTimerData& FTimerManager::GetTimer(FTimerHandle const& InHandle)
{
	int32 Index = InHandle.GetIndex();
	checkSlow(Index >= 0 && Index < Timers.GetMaxIndex() && Timers.IsAllocated(Index) && Timers[Index].Handle == InHandle);
	FTimerData& Timer = Timers[Index];
	return Timer;
}

FTimerData* FTimerManager::FindTimer(FTimerHandle const& InHandle)
{
	if (!InHandle.IsValid())
	{
		return nullptr;
	}

	int32 Index = InHandle.GetIndex();
	if (Index < 0 || Index >= Timers.GetMaxIndex() || !Timers.IsAllocated(Index))
	{
		return nullptr;
	}

	FTimerData& Timer = Timers[Index];

	if (Timer.Handle != InHandle || Timer.Status == ETimerStatus::ActivePendingRemoval)
	{
		return nullptr;
	}

	return &Timer;
}

/** Finds a handle to a dynamic timer bound to a particular pointer and function name. */
FTimerHandle FTimerManager::K2_FindDynamicTimerHandle(FTimerDynamicDelegate InDynamicDelegate) const
{
	FTimerHandle Result;

	if (const void* Obj = InDynamicDelegate.GetUObject())
	{
		if (const TSet<FTimerHandle>* TimersForObject = ObjectToTimers.Find(Obj))
		{
			for (FTimerHandle Handle : *TimersForObject)
			{
				const FTimerData& Data = GetTimer(Handle);
				if (Data.Status != ETimerStatus::ActivePendingRemoval && Data.TimerDelegate.FuncDynDelegate == InDynamicDelegate)
				{
					Result = Handle;
					break;
				}
			}
		}
	}

	return Result;
}

void FTimerManager::InternalSetTimer(FTimerHandle& InOutHandle, FTimerUnifiedDelegate const& InDelegate, float InRate, bool InbLoop, float InFirstDelay)
{
	SCOPE_CYCLE_COUNTER(STAT_SetTimer);

	// not currently threadsafe
	check(IsInGameThread());

	if (FindTimer(InOutHandle))
	{
		// if the timer is already set, just clear it and we'll re-add it, since 
		// there's no data to maintain.
		InternalClearTimer(InOutHandle);
	}

	if (InRate > 0.f)
	{
		// set up the new timer
		FTimerData NewTimerData;
		NewTimerData.TimerDelegate = InDelegate;

		NewTimerData.Rate = InRate;
		NewTimerData.bLoop = InbLoop;
		NewTimerData.bRequiresDelegate = NewTimerData.TimerDelegate.IsBound();

		// Set level collection
		const UWorld* const OwningWorld = OwningGameInstance ? OwningGameInstance->GetWorld() : nullptr;
		if (OwningWorld && OwningWorld->GetActiveLevelCollection())
		{
			NewTimerData.LevelCollection = OwningWorld->GetActiveLevelCollection()->GetType();
		}

		const float FirstDelay = (InFirstDelay >= 0.f) ? InFirstDelay : InRate;

		FTimerHandle NewTimerHandle;
		if (HasBeenTickedThisFrame())
		{
			NewTimerData.ExpireTime = InternalTime + FirstDelay;
			NewTimerData.Status = ETimerStatus::Active;
			NewTimerHandle = AddTimer(MoveTemp(NewTimerData));
			ActiveTimerHeap.HeapPush(NewTimerHandle, FTimerHeapOrder(Timers));
		}
		else
		{
			// Store time remaining in ExpireTime while pending
			NewTimerData.ExpireTime = FirstDelay;
			NewTimerData.Status = ETimerStatus::Pending;
			NewTimerHandle = AddTimer(MoveTemp(NewTimerData));
			PendingTimerSet.Add(NewTimerHandle);
		}

		InOutHandle = NewTimerHandle;
	}
}

void FTimerManager::InternalSetTimerForNextTick(FTimerUnifiedDelegate const& InDelegate)
{
	SCOPE_CYCLE_COUNTER(STAT_SetTimerForNextTick);

	// not currently threadsafe
	check(IsInGameThread());

	FTimerData NewTimerData;
	NewTimerData.Rate = 0.f;
	NewTimerData.bLoop = false;
	NewTimerData.bRequiresDelegate = true;
	NewTimerData.TimerDelegate = InDelegate;
	NewTimerData.ExpireTime = InternalTime;
	NewTimerData.Status = ETimerStatus::Active;

	// Set level collection
	const UWorld* const OwningWorld = OwningGameInstance ? OwningGameInstance->GetWorld() : nullptr;
	if (OwningWorld && OwningWorld->GetActiveLevelCollection())
	{
		NewTimerData.LevelCollection = OwningWorld->GetActiveLevelCollection()->GetType();
	}

	FTimerHandle NewTimerHandle = AddTimer(MoveTemp(NewTimerData));
	ActiveTimerHeap.HeapPush(NewTimerHandle, FTimerHeapOrder(Timers));
}

void FTimerManager::InternalClearTimer(FTimerHandle const& InHandle)
{
	SCOPE_CYCLE_COUNTER(STAT_ClearTimer);

	// not currently threadsafe
	check(IsInGameThread());

	FTimerData& Data = GetTimer(InHandle);
	switch (Data.Status)
	{
		case ETimerStatus::Pending:
			{
				int32 NumRemoved = PendingTimerSet.Remove(InHandle);
				check(NumRemoved == 1);
				RemoveTimer(InHandle);
			}
			break;

		case ETimerStatus::Active:
			Data.Status = ETimerStatus::ActivePendingRemoval;
			break;

		case ETimerStatus::ActivePendingRemoval:
			// Already removed
			break;

		case ETimerStatus::Paused:
			{
				int32 NumRemoved = PausedTimerSet.Remove(InHandle);
				check(NumRemoved == 1);
				RemoveTimer(InHandle);
			}
			break;

		case ETimerStatus::Executing:
			check(CurrentlyExecutingTimer == InHandle);

			// Edge case. We're currently handling this timer when it got cleared.  Clear it to prevent it firing again
			// in case it was scheduled to fire multiple times.
			CurrentlyExecutingTimer.Invalidate();
			RemoveTimer(InHandle);
			break;

		default:
			check(false);
	}
}

void FTimerManager::InternalClearAllTimers(void const* Object)
{
	SCOPE_CYCLE_COUNTER(STAT_ClearAllTimers);

	if (!Object)
	{
		return;
	}

	TSet<FTimerHandle>* TimersToRemove = ObjectToTimers.Find(Object);
	if (!TimersToRemove)
	{
		return;
	}

	TSet<FTimerHandle> LocalTimersToRemove = *TimersToRemove;
	for (FTimerHandle TimerToRemove : LocalTimersToRemove)
	{
		InternalClearTimer(TimerToRemove);
	}

	ObjectToTimers.Remove(Object);
}

float FTimerManager::InternalGetTimerRemaining(FTimerData const* const TimerData) const
{
	if (TimerData)
	{
		switch (TimerData->Status)
		{
			case ETimerStatus::Active:
				return TimerData->ExpireTime - InternalTime;

			case ETimerStatus::Executing:
				return 0.0f;

			default:
				// ExpireTime is time remaining for paused timers
				return TimerData->ExpireTime;
		}
	}

	return -1.f;
}

float FTimerManager::InternalGetTimerElapsed(FTimerData const* const TimerData) const
{
	if (TimerData)
	{
		switch (TimerData->Status)
		{
			case ETimerStatus::Active:
			case ETimerStatus::Executing:
				return (TimerData->Rate - (TimerData->ExpireTime - InternalTime));

			default:
				// ExpireTime is time remaining for paused timers
				return (TimerData->Rate - TimerData->ExpireTime);
		}
	}

	return -1.f;
}

float FTimerManager::InternalGetTimerRate(FTimerData const* const TimerData) const
{
	if (TimerData)
	{
		return TimerData->Rate;
	}
	return -1.f;
}

void FTimerManager::PauseTimer(FTimerHandle InHandle)
{
	// not currently threadsafe
	check(IsInGameThread());

	FTimerData* TimerToPause = FindTimer(InHandle);
	if (!TimerToPause || TimerToPause->Status == ETimerStatus::Paused)
	{
		return;
	}

	ETimerStatus PreviousStatus = TimerToPause->Status;

	// Remove from previous TArray
	switch( PreviousStatus )
	{
		case ETimerStatus::ActivePendingRemoval:
			break;

		case ETimerStatus::Active:
			{
				int32 IndexIndex = ActiveTimerHeap.Find(InHandle);
				check(IndexIndex != INDEX_NONE);
				ActiveTimerHeap.HeapRemoveAt(IndexIndex, FTimerHeapOrder(Timers), /*bAllowShrinking=*/ false);
			}
			break;

		case ETimerStatus::Pending:
			{
				int32 NumRemoved = PendingTimerSet.Remove(InHandle);
				check(NumRemoved == 1);
			}
			break;

		case ETimerStatus::Executing:
			check(CurrentlyExecutingTimer == InHandle);

			CurrentlyExecutingTimer.Invalidate();
			break;

		default:
			check(false);
	}

	// Don't pause the timer if it's currently executing and isn't going to loop
	if( PreviousStatus == ETimerStatus::Executing && !TimerToPause->bLoop )
	{
		RemoveTimer(InHandle);
	}
	else
	{
		// Add to Paused list
		PausedTimerSet.Add(InHandle);

		// Set new status
		TimerToPause->Status = ETimerStatus::Paused;

		// Store time remaining in ExpireTime while paused. Don't do this if the timer is in the pending list.
		if (PreviousStatus != ETimerStatus::Pending)
		{
			TimerToPause->ExpireTime -= InternalTime;
		}
	}
}

void FTimerManager::UnPauseTimer(FTimerHandle InHandle)
{
	// not currently threadsafe
	check(IsInGameThread());

	FTimerData* TimerToUnPause = FindTimer(InHandle);
	if (!TimerToUnPause || TimerToUnPause->Status != ETimerStatus::Paused)
	{
		return;
	}

	// Move it out of paused list and into proper TArray
	if( HasBeenTickedThisFrame() )
	{
		// Convert from time remaining back to a valid ExpireTime
		TimerToUnPause->ExpireTime += InternalTime;
		TimerToUnPause->Status = ETimerStatus::Active;
		ActiveTimerHeap.HeapPush(InHandle, FTimerHeapOrder(Timers));
	}
	else
	{
		TimerToUnPause->Status = ETimerStatus::Pending;
		PendingTimerSet.Add(InHandle);
	}

	// remove from paused list
	PausedTimerSet.Remove(InHandle);
}

// ---------------------------------
// Public members
// ---------------------------------

DECLARE_DWORD_COUNTER_STAT(TEXT("TimerManager Heap Size"),STAT_NumHeapEntries,STATGROUP_Game);

void FTimerManager::Tick(float DeltaTime)
{
	CSV_SCOPED_TIMING_STAT(Basic, UWorld_Tick_TimerManagerTick);

#if DO_TIMEGUARD && 0
	TArray<FTimerUnifiedDelegate> RunTimerDelegates;
	FTimerNameDelegate NameFunction = FTimerNameDelegate::CreateLambda([&] {
			FString ActiveDelegates;
			for ( const FTimerUnifiedDelegate& Descriptor : RunTimerDelegates )
			{
				ActiveDelegates += FString::Printf(TEXT("Delegate %s, "), *Descriptor.ToString() );
			}
			return FString::Printf(TEXT("UWorld::Tick - TimerManager, %s"), *ActiveDelegates);
		});


	// no delegate should take longer then 5ms to run 
	SCOPE_TIME_GUARD_DELEGATE_MS(NameFunction, 5);
#endif	

	// @todo, might need to handle long-running case
	// (e.g. every X seconds, renormalize to InternalTime = 0)

	INC_DWORD_STAT_BY(STAT_NumHeapEntries, ActiveTimerHeap.Num());

	if (HasBeenTickedThisFrame())
	{
		return;
	}

	InternalTime += DeltaTime;

	UWorld* const OwningWorld = OwningGameInstance ? OwningGameInstance->GetWorld() : nullptr;
	UWorld* const LevelCollectionWorld = OwningWorld;

	while (ActiveTimerHeap.Num() > 0)
	{
		FTimerHandle TopHandle = ActiveTimerHeap.HeapTop();

		// Test for expired timers
		int32 TopIndex = TopHandle.GetIndex();
		FTimerData* Top = &Timers[TopIndex];

		if (Top->Status == ETimerStatus::ActivePendingRemoval)
		{
			ActiveTimerHeap.HeapPop(TopHandle, FTimerHeapOrder(Timers), /*bAllowShrinking=*/ false);
			continue;
		}

		if (InternalTime > Top->ExpireTime)
		{
			// Timer has expired! Fire the delegate, then handle potential looping.

			// Set the relevant level context for this timer
			const int32 LevelCollectionIndex = OwningWorld ? OwningWorld->FindCollectionIndexByType(Top->LevelCollection) : INDEX_NONE;
			
			FScopedLevelCollectionContextSwitch LevelContext(LevelCollectionIndex, LevelCollectionWorld);

			// Remove it from the heap and store it while we're executing
			ActiveTimerHeap.HeapPop(CurrentlyExecutingTimer, FTimerHeapOrder(Timers), /*bAllowShrinking=*/ false);
			Top->Status = ETimerStatus::Executing;

			// Determine how many times the timer may have elapsed (e.g. for large DeltaTime on a short looping timer)
			int32 const CallCount = Top->bLoop ? 
				FMath::TruncToInt( (InternalTime - Top->ExpireTime) / Top->Rate ) + 1
				: 1;

			// Now call the function
			for (int32 CallIdx=0; CallIdx<CallCount; ++CallIdx)
			{ 
#if DO_TIMEGUARD && 0
				FTimerNameDelegate NameFunction = FTimerNameDelegate::CreateLambda([&] { 
						return FString::Printf(TEXT("FTimerManager slowtick from delegate %s "), *Top->TimerDelegate.ToString());
					});
				// no delegate should take longer then 2ms to run 
				SCOPE_TIME_GUARD_DELEGATE_MS(NameFunction, 2);
#endif
#if DO_TIMEGUARD && 0
				RunTimerDelegates.Add(Top->TimerDelegate);
#endif


				Top->TimerDelegate.Execute();

				// Update Top pointer, in case it has been invalidated by the Execute call
				Top = FindTimer(CurrentlyExecutingTimer);
				if (!Top || Top->Status != ETimerStatus::Executing)
				{
					break;
				}
			}

			// test to ensure it didn't get cleared during execution
			if (Top)
			{
				// if timer requires a delegate, make sure it's still validly bound (i.e. the delegate's object didn't get deleted or something)
				if (Top->bLoop && (!Top->bRequiresDelegate || Top->TimerDelegate.IsBound()))
				{
					// Put this timer back on the heap
					Top->ExpireTime += CallCount * Top->Rate;
					Top->Status = ETimerStatus::Active;
					ActiveTimerHeap.HeapPush(CurrentlyExecutingTimer, FTimerHeapOrder(Timers));
				}
				else
				{
					RemoveTimer(CurrentlyExecutingTimer);
				}

				CurrentlyExecutingTimer.Invalidate();
			}
		}
		else
		{
			// no need to go further down the heap, we can be finished
			break;
		}
	}

	// Timer has been ticked.
	LastTickedFrame = GFrameCounter;

	// If we have any Pending Timers, add them to the Active Queue.
	if( PendingTimerSet.Num() > 0 )
	{
		for (FTimerHandle Handle : PendingTimerSet)
		{
			FTimerData& TimerToActivate = GetTimer(Handle);

			// Convert from time remaining back to a valid ExpireTime
			TimerToActivate.ExpireTime += InternalTime;
			TimerToActivate.Status = ETimerStatus::Active;
			ActiveTimerHeap.HeapPush( Handle, FTimerHeapOrder(Timers) );
		}
		PendingTimerSet.Reset();
	}
}

TStatId FTimerManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FTimerManager, STATGROUP_Tickables);
}

void FTimerManager::ListTimers() const
{
	TArray<const FTimerData*> ValidActiveTimers;
	ValidActiveTimers.Reserve(ActiveTimerHeap.Num());
	for (FTimerHandle Handle : ActiveTimerHeap)
	{
		if (const FTimerData* Data = FindTimer(Handle))
		{
			ValidActiveTimers.Add(Data);
		}
	}

	UE_LOG(LogEngine, Log, TEXT("------- %d Active Timers -------"), ValidActiveTimers.Num());
	for (const FTimerData* Data : ValidActiveTimers)
	{
		FString TimerString = Data->TimerDelegate.ToString();
		UE_LOG(LogEngine, Log, TEXT("%s"), *TimerString);
	}

	UE_LOG(LogEngine, Log, TEXT("------- %d Paused Timers -------"), PausedTimerSet.Num());
	for (FTimerHandle Handle : PausedTimerSet)
	{
		const FTimerData& Data = GetTimer(Handle);
		FString TimerString = Data.TimerDelegate.ToString();
		UE_LOG(LogEngine, Log, TEXT("%s"), *TimerString);
	}

	UE_LOG(LogEngine, Log, TEXT("------- %d Pending Timers -------"), PendingTimerSet.Num());
	for (FTimerHandle Handle : PendingTimerSet)
	{
		const FTimerData& Data = GetTimer(Handle);
		FString TimerString = Data.TimerDelegate.ToString();
		UE_LOG(LogEngine, Log, TEXT("%s"), *TimerString);
	}

	UE_LOG(LogEngine, Log, TEXT("------- %d Total Timers -------"), PendingTimerSet.Num() + PausedTimerSet.Num() + ValidActiveTimers.Num());
}

FTimerHandle FTimerManager::AddTimer(FTimerData&& TimerData)
{
	const void* TimerIndicesByObjectKey = TimerData.TimerDelegate.GetBoundObject();
	TimerData.TimerIndicesByObjectKey = TimerIndicesByObjectKey;

	int32 NewIndex = Timers.Add(MoveTemp(TimerData));

	FTimerHandle Result = GenerateHandle(NewIndex);
	Timers[NewIndex].Handle = Result;

	if (TimerIndicesByObjectKey)
	{
		ObjectToTimers.FindOrAdd(TimerIndicesByObjectKey).Add(Result);
	}

	return Result;
}

void FTimerManager::RemoveTimer(FTimerHandle Handle)
{
	const FTimerData& Data = GetTimer(Handle);

	// Remove TimerIndicesByObject entry if necessary
	if (const void* TimerIndicesByObjectKey = Data.TimerIndicesByObjectKey)
	{
		TSet<FTimerHandle>* TimersForObject = ObjectToTimers.Find(TimerIndicesByObjectKey);
		checkf(TimersForObject, TEXT("Removed timer was bound to an object which is not tracked by TimerIndicesByObject!"));

		int32 NumRemoved = TimersForObject->Remove(Handle);
		checkf(NumRemoved == 1, TEXT("Removed timer was bound to an object which is not tracked by TimerIndicesByObject!"));

		if (TimersForObject->Num() == 0)
		{
			ObjectToTimers.Remove(TimerIndicesByObjectKey);
		}
	}

	Timers.RemoveAt(Handle.GetIndex());
}

FTimerHandle FTimerManager::GenerateHandle(int32 Index)
{
	uint64 NewSerialNumber = ++LastAssignedSerialNumber;
	if (!ensureMsgf(NewSerialNumber != FTimerHandle::MaxSerialNumber, TEXT("Timer serial number has wrapped around!")))
	{
		NewSerialNumber = (uint64)1;
	}

	FTimerHandle Test;
	Test.SetIndexAndSerialNumber(FTimerHandle::MaxIndex - 1, FTimerHandle::MaxSerialNumber - 1);
	check(Test.GetIndex() == FTimerHandle::MaxIndex - 1 && Test.GetSerialNumber() == FTimerHandle::MaxSerialNumber - 1);

	FTimerHandle Result;
	Result.SetIndexAndSerialNumber(Index, NewSerialNumber);
	check(Result.GetIndex() == Index && Result.GetSerialNumber() == NewSerialNumber);
	return Result;
}


// Handler for ListTimers console command
static void OnListTimers(UWorld* World)
{
	if(World != nullptr)
	{
		World->GetTimerManager().ListTimers();
	}
}

// Register ListTimers console command, needs a World context
FAutoConsoleCommandWithWorld ListTimersConsoleCommand(
	TEXT("ListTimers"),
	TEXT(""),
	FConsoleCommandWithWorldDelegate::CreateStatic(OnListTimers)
	);
