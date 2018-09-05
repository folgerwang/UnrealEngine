// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCProxyCommon.h"

/**
 * Portable semaphore, based on a mutex and condition variable
 * Read https://en.cppreference.com/w/cpp/thread/condition_variable if you are
 * unfamiliar with condition_variable
 */
class FSemaphore
{
public:
	FSemaphore(unsigned int Count = 0)
		: Count(Count)
	{
	}

	/**
	 * Increases the counter
	 */
	void Notify()
	{
		std::unique_lock<std::mutex> Lk(Mtx);
		Count++;
		Cv.notify_one();
	}

	/**
	 * Blocks until the counter is >0
	 */
	void Wait()
	{
		std::unique_lock<std::mutex> Lk(Mtx);
		Cv.wait(Lk, [this]() { return Count > 0; });
		Count--;
	}

	/**
	 * Similar to "Wait", but doesn't block.
	 * If the semaphore is not ready (aka: counter==0), it will just return false
	 * without blocking
	 */
	bool TryWait()
	{
		std::unique_lock<std::mutex> Lk(Mtx);
		if (Count)
		{
			Count--;
			return true;
		}
		else
		{
			return false;
		}
	}

	/**
	 * Waits for the semaphore to be set, until the specific time is reached.
	 * @return
	 *	Returns true if the semaphore was set before we reached the specified time point.
	 *	Returns false if we reached the time point before the semaphore was set.
	 */
	template <class Clock, class Duration>
	bool WaitUntil(const std::chrono::time_point<Clock, Duration>& point)
	{
		std::unique_lock<std::mutex> lock(Mtx);
		if (!Cv.wait_until(lock, point, [this]() { return Count > 0; }))
		{
			return false;
		}
		Count--;
		return true;
	}

private:
	std::mutex Mtx;
	std::condition_variable Cv;
	unsigned int Count;
};
