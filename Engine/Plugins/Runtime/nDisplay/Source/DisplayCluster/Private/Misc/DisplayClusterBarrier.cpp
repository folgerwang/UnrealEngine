// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterBarrier.h"

#include "DisplayClusterLog.h"
#include "Engine/EngineTypes.h"

#include <chrono>


FDisplayClusterBarrier::FDisplayClusterBarrier(uint32 threadsAmount, const FString& name, uint32 timeout) :
	Name(name),
	ThreadsAmount(threadsAmount),
	ThreadsLeft(threadsAmount),
	IterationCounter(0),
	Timeout(timeout)
{
	UE_LOG(LogDisplayClusterNetwork, Log, TEXT("Initialized barrier %s with timeout %u for threads count: %u"), *Name, Timeout, ThreadsAmount);
}

FDisplayClusterBarrier::FDisplayClusterBarrier(uint32 threadsAmount, uint32 timeout) :
	FDisplayClusterBarrier(threadsAmount, FString("noname_barrier"), timeout)
{
}


FDisplayClusterBarrier::~FDisplayClusterBarrier()
{
	// Free currently blocked threads
	Deactivate();
}

FDisplayClusterBarrier::WaitResult FDisplayClusterBarrier::Wait(double* pThreadWaitTime /*= nullptr*/, double* pBarrierWaitTime /*= nullptr*/)
{
	if (bEnabled == false)
	{
		UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("%s barrier is not active"), *Name);
		return WaitResult::NotActive;
	}

	const double threadWaitTimeStart = FPlatformTime::Seconds();

	{
		std::unique_lock<std::mutex> lock{ Mutex };

		size_t curIter = IterationCounter;

		if (ThreadsLeft == ThreadsAmount)
		{
			WaitTimeStart = FPlatformTime::Seconds();
			UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s barrier start time: %lf"), *Name, WaitTimeStart);
		}

		// Check if all threads are in front of the barrier
		if (--ThreadsLeft == 0)
		{
			UE_LOG(LogDisplayClusterNetwork, Verbose, TEXT("%s barrier trigger!"), *Name);
			++IterationCounter;
			ThreadsLeft = ThreadsAmount;

			WaitTimeFinish = FPlatformTime::Seconds();
			UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s barrier finish time: %lf"), *Name, WaitTimeFinish);

			WaitTimeOverall = WaitTimeFinish - WaitTimeStart;
			UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s barrier overall wait time: %lf"), *Name, WaitTimeOverall);

			// This is the last node. Unblock the barrier.
			CondVar.notify_all();
		}
		else
		{
			UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s barrier waiting, %u threads left"), *Name, ThreadsLeft);
			// Not all of threads have came here. Wait.
			if (!CondVar.wait_for(lock, std::chrono::milliseconds(Timeout), [this, curIter] { return curIter != IterationCounter || bEnabled == false; }))
			{
				//@todo: no timeout result if barrier has been disabled
				UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("%s barrier waiting timeout"), *Name);
				return WaitResult::Timeout;
			}
		}
	}

	const double threadWaitTimeFinish = FPlatformTime::Seconds();

	if (pBarrierWaitTime)
		*pBarrierWaitTime = WaitTimeOverall;

	if (pThreadWaitTime)
		*pThreadWaitTime = threadWaitTimeFinish - threadWaitTimeStart;

	// Go ahead
	return WaitResult::Ok;
}

void FDisplayClusterBarrier::Activate()
{
	std::unique_lock<std::mutex> lock{ Mutex };

	IterationCounter = 0;
	ThreadsLeft = ThreadsAmount;
	bEnabled = true;
	CondVar.notify_all();
}

void FDisplayClusterBarrier::Deactivate()
{
	std::unique_lock<std::mutex> lock{ Mutex };

	bEnabled = false;
	CondVar.notify_all();
}

