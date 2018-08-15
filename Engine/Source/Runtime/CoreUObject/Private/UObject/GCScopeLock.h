// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Garbage Collection scope lock. 
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeLock.h"
#include "HAL/ThreadSafeBool.h"
#include "HAL/Event.h"

/** Locks all UObject hash tables when performing GC */
class FGCScopeLock
{
	/** Previous value of the GetGarbageCollectingFlag() */
	bool bPreviousGabageCollectingFlagValue;
public:

	static FThreadSafeBool& GetGarbageCollectingFlag();

	/** 
	 * We're storing the value of GetGarbageCollectingFlag in the constructor, it's safe as only 
	 * one thread is ever going to be setting it and calling this code - the game thread.
	 **/
	FORCEINLINE FGCScopeLock()
		: bPreviousGabageCollectingFlagValue(GetGarbageCollectingFlag())
	{		
		void LockUObjectHashTables();
		LockUObjectHashTables();		
		GetGarbageCollectingFlag() = true;
	}
	FORCEINLINE ~FGCScopeLock()
	{		
		GetGarbageCollectingFlag() = bPreviousGabageCollectingFlagValue;
		void UnlockUObjectHashTables();
		UnlockUObjectHashTables();		
	}
};


/**
* Garbage Collection synchronization objects
* Will not lock other threads if GC is not running.
* Has the ability to only lock for GC if no other locks are present.
*/
class FGCCSyncObject
{
	/** Non zero if any of the non-game threads is blocking GC */
	FThreadSafeCounter AsyncCounter;
	/** Non zero if GC is running */
	FThreadSafeCounter GCCounter;
	/** Non zero if GC wants to run but is blocked by some other thread \
	    This flag is not automatically enforced on the async threads, instead
			threads have to manually implement support for it. */
	FThreadSafeCounter GCWantsToRunCounter;
	/** Critical section for thread safe operations */
	FCriticalSection Critical;
	/** Event used to block non-game threads when GC is running */
	FEvent* GCUnlockedEvent;

	/** One and only global instance of FGCCSyncObject */
	static TUniquePtr<FGCCSyncObject> Singleton;

public:

	FGCCSyncObject();
	~FGCCSyncObject();

	/** Creates the singleton object */
	static void Create();

	/** Gets the singleton object */
	static FGCCSyncObject& Get()
	{
		check(Singleton.IsValid());
		return *Singleton.Get();
	}

	/** Lock on non-game thread. Will block if GC is running. */
	void LockAsync()
	{
		if (!IsInGameThread())
		{
			// Wait until GC is done if it was running when entering this function
			bool bLocked = false;
			do
			{
				if (GCCounter.GetValue() > 0)
				{
					GCUnlockedEvent->Wait();
				}
				{
					FScopeLock CriticalLock(&Critical);
					if (GCCounter.GetValue() == 0)
					{
						AsyncCounter.Increment();
						bLocked = true;
					}
				}
			} while (!bLocked);
		}
	}
	/** Release lock from non-game thread */
	void UnlockAsync()
	{
		if (!IsInGameThread())
		{
			AsyncCounter.Decrement();
		}
	}
	/** Lock for GC. Will block if any other thread has locked. */
	void GCLock()
	{
		// Signal other threads that GC wants to run
		SetGCIsWaiting();

		// Wait until all other threads are done if they're currently holding the lock
		bool bLocked = false;
		do
		{
			FPlatformProcess::ConditionalSleep([&]()
			{
				return AsyncCounter.GetValue() == 0;
			});
			{
				FScopeLock CriticalLock(&Critical);
				if (AsyncCounter.GetValue() == 0)
				{
					GCUnlockedEvent->Reset();
					int32 GCCounterValue = GCCounter.Increment();
					check(GCCounterValue == 1); // GCLock doesn't support recursive locks
					// At this point GC can run so remove the signal that it's waiting
					FPlatformMisc::MemoryBarrier();
					ResetGCIsWaiting();
					bLocked = true;
				}
			}
		} while (!bLocked);
	}
	/** Checks if any async thread has a lock */
	bool IsAsyncLocked() const
	{
		return AsyncCounter.GetValue() != 0;
	}
	/** Checks if GC has a lock */
	bool IsGCLocked() const
	{
		return GCCounter.GetValue() != 0;
	}
	/** Lock for GC. Will not block and return false if any other thread has already locked. */
	bool TryGCLock()
	{
		bool bSuccess = false;
		FScopeLock CriticalLock(&Critical);
		// If any other thread is currently locking we just exit
		if (AsyncCounter.GetValue() == 0)
		{
			GCUnlockedEvent->Reset();
			int32 GCCounterValue = GCCounter.Increment();
			check(GCCounterValue == 1); // GCLock doesn't support recursive locks
			bSuccess = true;
		}
		return bSuccess;
	}
	/** Unlock GC */
	void GCUnlock()
	{
		GCUnlockedEvent->Trigger();
		GCCounter.Decrement();
	}

	/** Manually mark GC state as 'waiting to run' */
	void SetGCIsWaiting()
	{
		GCWantsToRunCounter.Increment();
	}

	/** Manually reset GC 'waiting to run' state*/
	void ResetGCIsWaiting()
	{
		GCWantsToRunCounter.Set(0);
	}

	/** True if GC wants to run on the game thread but is maybe blocked by some other thread */
	bool IsGCWaiting() const
	{
		return GCWantsToRunCounter.GetValue() > 0;
	}
};
