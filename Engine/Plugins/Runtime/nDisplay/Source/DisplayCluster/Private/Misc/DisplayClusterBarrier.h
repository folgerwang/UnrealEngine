// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <condition_variable>
#include <mutex>


/**
 * Thread barrier
 */
class FDisplayClusterBarrier
{
public:
	explicit FDisplayClusterBarrier(uint32 threadsAmount, uint32 timeout);
	explicit FDisplayClusterBarrier(uint32 threadsAmount, const FString& name, uint32 timeout);
	~FDisplayClusterBarrier();

public:
	enum class WaitResult
	{
		Ok,
		NotActive,
		Timeout
	};

public:
	// Wait until all threads arrive
	WaitResult Wait(double* pThreadWaitTime = nullptr, double* pBarrierWaitTime = nullptr);
	// Enable barrier
	void Activate();
	// Disable barrier (no blocking operation performed anymore)
	void Deactivate();

private:
	// Barrier name for logging
	const FString Name;

	// Barrier state
	bool bEnabled = true;

	// Amount of threads to wait at the barrier
	const uint32 ThreadsAmount;
	// Waiting threads amount
	uint32 ThreadsLeft;
	// Iteration counter (kind of barrier sync transaction)
	size_t IterationCounter;

	std::condition_variable CondVar;
	std::mutex Mutex;

	uint32 Timeout = 0;

	double WaitTimeStart = 0;
	double WaitTimeFinish = 0;
	double WaitTimeOverall = 0;
};

