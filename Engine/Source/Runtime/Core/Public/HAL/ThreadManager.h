// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Map.h"
#include "HAL/CriticalSection.h"

/**
 * Manages runnables and runnable threads.
 */
class CORE_API FThreadManager
{
	/** List of thread objects to be ticked. */
	TMap<uint32, class FRunnableThread*> Threads;
	/** Critical section for ThreadList */
	FCriticalSection ThreadsCritical;

public:

	/**
	* Used internally to add a new thread object.
	*
	* @param Thread thread object.
	* @see RemoveThread
	*/
	void AddThread(uint32 ThreadId, class FRunnableThread* Thread);

	/**
	* Used internally to remove thread object.
	*
	* @param Thread thread object to be removed.
	* @see AddThread
	*/
	void RemoveThread(class FRunnableThread* Thread);

	/** Ticks all fake threads and their runnable objects. */
	void Tick();

	/** Returns the name of a thread given its TLS id */
	const FString& GetThreadName(uint32 ThreadId);

#if PLATFORM_WINDOWS
	struct FThreadStackBackTrace
	{
		uint32 ThreadId;
		FString ThreadName;
		TArray<uint64, TInlineAllocator<100>> ProgramCounters;
	};

	void GetAllThreadStackBackTraces(TArray<FThreadStackBackTrace>& StackTraces);
#endif

	/**
	 * Access to the singleton object.
	 *
	 * @return Thread manager object.
	 */
	static FThreadManager& Get();
};
